#include "export.h"
#include "playback.h"
#include <string.h>

///////////////////////////////////////////////////////////////////////////////
// ExporterSelectionWAV
//
// Bounces a selected region (song rows / chain rows / phrase rows) to WAV.
// Reuses ExporterWAV's file handling and rendering loop unchanged; the
// constructor only reconfigures the private playback state:
//   1. Stop the song playback started by the base Exporter ctor.
//   2. Mute unselected tracks via trackEnabled[] (mixer-only gate).
//   3. Set a stopRange so tracks stop (notes killed) at the region end.
//   4. Start every selected track at the region start (song level: tracks
//      whose first chain is later in the region wait through empty rows).
///////////////////////////////////////////////////////////////////////////////

ExporterSelectionWAV::ExporterSelectionWAV(const char* path, Project* project, const ExportSelection& selection,
                                           int sampleRate, int bitDepth, float mixVolume)
  : ExporterWAV(path, project, 0, sampleRate, bitDepth, mixVolume, false) {
  PlaybackState* playback = &chipnomadState->playbackState;

  // Reset the song playback queued by the base Exporter constructor
  playbackStop(playback);

  // Mute unselected tracks (mixer gate only; the sequencer still advances)
  for (int t = 0; t < PROJECT_MAX_TRACKS; t++) {
    playback->trackEnabled[t] = (selection.trackMask & (1u << t)) ? 1 : 0;
  }

  // Stop boundary: tracks stop (notes killed) when the region ends
  StopRange stopRange = {};
  stopRange.enabled = 1;
  stopRange.level = selection.level;
  stopRange.startSongRow = selection.startSongRow;
  stopRange.startChainRow = selection.startChainRow;
  stopRange.startPhraseRow = selection.startPhraseRow;
  stopRange.endSongRow = selection.endSongRow;
  stopRange.endChainRow = selection.endChainRow;
  stopRange.endPhraseRow = selection.endPhraseRow;
  playbackSetStopRange(playback, stopRange);

  // Start every selected track at the region start
  if (selection.level == 0) {
    // Song level: queue every selected track at the region start. Tracks
    // whose first chain is later in the region wait silently through empty
    // song rows (stopRange level 0 lets song-mode tracks advance past empty
    // rows) and join when their song row arrives, so the bounce contains
    // every selected track's chains.
    playback->scaleRoot = playback->p->scaleRoot;
    playback->scalePreset = playback->p->scalePreset;
    for (int t = 0; t < PROJECT_MAX_TRACKS; t++) {
      if (selection.trackMask & (1u << t)) {
        PlaybackTrackState* track = &playback->tracks[t];
        track->queue.mode = PlaybackMode::song;
        track->queue.songRow = selection.startSongRow;
        track->queue.chainRow = 0;
        track->queue.phraseRow = 0;
        track->queue.loop = 0;
      }
    }
  } else if (selection.level == 1) {
    // Chain level: start every selected track at the selected chain row
    for (int t = 0; t < PROJECT_MAX_TRACKS; t++) {
      if ((selection.trackMask & (1u << t)) && project->song[selection.startSongRow][t] != EMPTY_VALUE_16) {
        playbackStartChain(playback, t, selection.startSongRow, selection.startChainRow, 0);
      }
    }
  } else {
    // Phrase level: start every selected track at the selected phrase row
    for (int t = 0; t < PROJECT_MAX_TRACKS; t++) {
      if (selection.trackMask & (1u << t)) {
        playbackStartPhrase(playback, t, selection.startSongRow, selection.startChainRow, 0,
                            selection.startPhraseRow);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// exportSelectionLengthRows
//
// Predicts the rendered length of a selection bounce in phrase rows
// (16th notes). Mirrors the stopRange semantics in playback.cpp:
//   - phrase level: the selected row span
//   - chain level: consecutive non-empty chain rows from the start row
//     (the engine stops a track at the first empty chain row)
//   - song level: per selected track, walk the song rows in the region;
//     an empty song row costs 16 rows (the track waits silently), a chain
//     contributes its consecutive non-empty rows from row 0 (a chain whose
//     row 0 is empty contributes nothing - the track dies there). The
//     result is the longest track timeline.
///////////////////////////////////////////////////////////////////////////////

static int chainConsecutiveRows(const Project* project, uint16_t chainIdx, int startRow, int maxRows) {
  if (chainIdx == EMPTY_VALUE_16) return 0;
  int rows = 0;
  for (int r = startRow; r < maxRows; r++) {
    if (project->chains[chainIdx].rows[r].phrase == EMPTY_VALUE_16) break;
    rows++;
  }
  return rows;
}

int exportSelectionLengthRows(const Project* project, const ExportSelection& selection) {
  if (project == NULL) return 0;

  if (selection.level == 2) {
    // Phrase level: the selected row span
    int rows = selection.endPhraseRow - selection.startPhraseRow + 1;
    return rows > 0 ? rows : 0;
  }

  if (selection.level == 1) {
    // Chain level: consecutive non-empty chain rows from the start row,
    // capped at the selected chain region (stopRange stops the track at
    // endChainRow even if the chain continues)
    int longest = 0;
    int maxChainRows = selection.endChainRow - selection.startChainRow + 1;
    if (maxChainRows < 0) maxChainRows = 0;
    for (int t = 0; t < PROJECT_MAX_TRACKS; t++) {
      if (!(selection.trackMask & (1u << t))) continue;
      if (selection.startSongRow < 0 || selection.startSongRow >= PROJECT_MAX_LENGTH) continue;
      uint16_t chainIdx = project->song[selection.startSongRow][t];
      int rows = chainConsecutiveRows(project, chainIdx, selection.startChainRow, 16);
      if (rows > maxChainRows) rows = maxChainRows;
      rows *= 16;
      if (rows > longest) longest = rows;
    }
    return longest;
  }

  // Song level: longest track timeline across the selected song rows
  int longest = 0;
  for (int t = 0; t < PROJECT_MAX_TRACKS; t++) {
    if (!(selection.trackMask & (1u << t))) continue;
    int total = 0;
    for (int songRow = selection.startSongRow;
         songRow <= selection.endSongRow && songRow < PROJECT_MAX_LENGTH; songRow++) {
      uint16_t chainIdx = project->song[songRow][t];
      if (chainIdx == EMPTY_VALUE_16) {
        // The track waits silently through an empty row only when more
        // selected rows follow; on the last selected row it is stopped
        // immediately by the readPhraseRow safeguard (songRow == endSongRow)
        if (songRow < selection.endSongRow) total += 16;
        else break;
      } else {
        int chainRows = chainConsecutiveRows(project, chainIdx, 0, 16);
        if (chainRows == 0) break; // Track dies on a chain with an empty row 0
        total += chainRows * 16;
      }
    }
    if (total > longest) longest = total;
  }
  return longest;
}
