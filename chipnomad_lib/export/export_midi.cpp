#include "export_midi.h"
#include "../midi/smf_file.h"
#include "../project_constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIDI_PPQ 480
#define MIDI_ROWS_PER_BEAT 4
#define MIDI_TICKS_PER_ROW (MIDI_PPQ / MIDI_ROWS_PER_BEAT)
#define MIDI_DEFAULT_GROOVE_TICKS 6

// PitchTable index -> MIDI note. The app's own 12TET table generators
// (tracker/src/pitch_table_utils.cpp: calculatePitchTableAY/
// calculateLinearPitchTable12TET) lay out entry N as MIDI note 12+N; custom
// or microtonal tables are approximated with the same rule.
static uint8_t noteIndexToMidi(int noteIndex) {
  int midi = 12 + noteIndex;
  if (midi < 0) midi = 0;
  if (midi > 127) midi = 127;
  return (uint8_t)midi;
}

static uint8_t volumeToVelocity(uint16_t volume) {
  if (volume == EMPTY_VALUE_16) return PHRASE_VOLUME_MAX;
  return (uint8_t)(volume > PHRASE_VOLUME_MAX ? PHRASE_VOLUME_MAX : volume);
}

// Tracks one tracker track's position while walking the arrangement,
// mirroring the structural (non-FX/non-live-queue) rule playback.cpp's
// moveToNextPhraseRow() uses for PlaybackMode::song: a chain row's phrase
// plays for a full 16-row phrase before the next chain row is even looked
// at; a chain row whose phrase is empty ends the chain immediately (any
// further chain rows are never visited, not even as silence) and moves to
// the next song row; a song row with no chain for this track ends the
// track for good - there is no scanning ahead for a later non-empty row.
// songRow == -1 means the track has stopped.
struct TrackCursor {
  int songRow;
  int chainRow;
  int phraseRow;
};

// Starting position for "Play Song" from row 0, matching playbackStartSong():
// a track only starts if row 0 has a chain for it AND that chain's row 0 has
// a phrase - otherwise it stays silent for the entire song.
static void trackCursorInit(Project* p, int trackIdx, TrackCursor* c) {
  c->songRow = -1;
  uint16_t chain = p->song[0][trackIdx];
  if (chain != EMPTY_VALUE_16 && p->chains[chain].rows[0].phrase != EMPTY_VALUE_16) {
    c->songRow = 0;
    c->chainRow = 0;
    c->phraseRow = 0;
  }
}

static void trackCursorAdvance(Project* p, int trackIdx, TrackCursor* c) {
  if (c->songRow < 0) return;
  c->phraseRow++;
  if (c->phraseRow < 16) return;
  c->phraseRow = 0;

  uint16_t chain = p->song[c->songRow][trackIdx];
  int nextChainRow = c->chainRow + 1;
  if (chain != EMPTY_VALUE_16 && nextChainRow < 16 && p->chains[chain].rows[nextChainRow].phrase != EMPTY_VALUE_16) {
    c->chainRow = nextChainRow;
    return;
  }

  int nextSongRow = c->songRow + 1;
  if (nextSongRow >= PROJECT_MAX_LENGTH || p->song[nextSongRow][trackIdx] == EMPTY_VALUE_16) {
    c->songRow = -1;
    return;
  }
  c->songRow = nextSongRow;
  c->chainRow = 0;
}

// Resolves the phrase row (if any) currently active for a track's cursor,
// and its chain transpose. Returns NULL once the track has stopped.
static PhraseRow* resolveCursorRow(Project* p, int trackIdx, const TrackCursor* c, int8_t* outTranspose) {
  if (c->songRow < 0) return NULL;
  uint16_t chainIdx = p->song[c->songRow][trackIdx];
  ChainRow* cr = &p->chains[chainIdx].rows[c->chainRow];
  *outTranspose = cr->transpose;
  return &p->phrases[cr->phrase].rows[c->phraseRow];
}

// Total number of absolute rows any track is still playing for, i.e. one
// past the last row where at least one track's cursor was active. 0 if no
// track ever starts (song empty from row 0's perspective).
static int computeSongLength(Project* p, int trackCount) {
  TrackCursor cursors[PROJECT_MAX_TRACKS];
  for (int t = 0; t < trackCount; t++) trackCursorInit(p, t, &cursors[t]);

  int absoluteRow = 0;
  for (;;) {
    int anyActive = 0;
    for (int t = 0; t < trackCount; t++) if (cursors[t].songRow >= 0) anyActive = 1;
    if (!anyActive) break;
    for (int t = 0; t < trackCount; t++) trackCursorAdvance(p, t, &cursors[t]);
    absoluteRow++;
  }
  return absoluteRow;
}

// Advances a groove cursor by one row, mirroring the skip/wrap rule in
// playback.cpp (empty groove slots wrap back to the start of the table).
struct GrooveCursor {
  uint8_t grooveIdx;
  uint8_t grooveRow;
};

static uint8_t grooveCursorNext(Project* p, GrooveCursor* g) {
  Groove* groove = &p->grooves[g->grooveIdx];
  if (groove->speed[g->grooveRow] == EMPTY_VALUE_8) g->grooveRow = 0;
  uint8_t ticks = groove->speed[g->grooveRow];
  if (ticks == EMPTY_VALUE_8) ticks = MIDI_DEFAULT_GROOVE_TICKS;
  g->grooveRow++;
  if (g->grooveRow >= 16 || groove->speed[g->grooveRow] == EMPTY_VALUE_8) g->grooveRow = 0;
  return ticks;
}

// Scans a phrase row's fx slots for a global groove change (fxGGR).
static int phraseRowGlobalGroove(PhraseRow* row) {
  for (int i = 0; i < 3; i++) {
    if (row->fx[i][0] == fxGGR) return row->fx[i][1] & (PROJECT_MAX_GROOVES - 1);
  }
  return -1;
}

// Builds the tempo map (conductor track): walks every track's cursor at
// every absolute row looking for fxGGR, and emits a tempo meta-event each
// time the effective row duration changes.
static void writeConductorTrack(Project* p, SmfTrackWriter* conductor, int totalRows, int trackCount) {
  TrackCursor cursors[PROJECT_MAX_TRACKS];
  for (int t = 0; t < trackCount; t++) trackCursorInit(p, t, &cursors[t]);

  GrooveCursor groove = { 0, 0 };
  uint32_t lastEventTick = 0;
  int haveLast = 0;
  int lastTicksPerRow = -1;

  for (int absoluteRow = 0; absoluteRow < totalRows; absoluteRow++) {
    for (int t = 0; t < trackCount; t++) {
      int8_t transpose;
      PhraseRow* row = resolveCursorRow(p, t, &cursors[t], &transpose);
      if (!row) continue;
      int newGroove = phraseRowGlobalGroove(row);
      if (newGroove >= 0) {
        groove.grooveIdx = (uint8_t)newGroove;
        groove.grooveRow = 0;
      }
    }

    uint8_t grooveTicks = grooveCursorNext(p, &groove);
    if (grooveTicks != lastTicksPerRow) {
      lastTicksPerRow = grooveTicks;
      double rowSeconds = (double)grooveTicks / (p->tickRate > 0 ? p->tickRate : 60.0);
      double quarterSeconds = rowSeconds * MIDI_ROWS_PER_BEAT;
      uint32_t microsecondsPerQuarter = (uint32_t)(quarterSeconds * 1000000.0);
      uint32_t tick = (uint32_t)absoluteRow * MIDI_TICKS_PER_ROW;
      smfTrackTempo(conductor, haveLast ? tick - lastEventTick : tick, microsecondsPerQuarter);
      lastEventTick = tick;
      haveLast = 1;
    }

    for (int t = 0; t < trackCount; t++) trackCursorAdvance(p, t, &cursors[t]);
  }

  smfTrackEnd(conductor, (uint32_t)totalRows * MIDI_TICKS_PER_ROW - lastEventTick);
}

static void writeNoteTrack(Project* p, SmfTrackWriter* track, int trackIndex, int totalRows) {
  uint8_t channel = trackIndex & 0x0f;
  smfTrackProgramChange(track, 0, channel, 0);

  TrackCursor cursor;
  trackCursorInit(p, trackIndex, &cursor);

  // Track name = first instrument used on this track, for reference only.
  char trackName[64];
  snprintf(trackName, sizeof(trackName), "Track %d", trackIndex + 1);
  {
    TrackCursor scan = cursor;
    for (int absoluteRow = 0; absoluteRow < totalRows; absoluteRow++) {
      int8_t transpose;
      PhraseRow* row = resolveCursorRow(p, trackIndex, &scan, &transpose);
      if (row && row->instrument != EMPTY_VALUE_8 && row->instrument < PROJECT_MAX_INSTRUMENTS) {
        snprintf(trackName, sizeof(trackName), "Track %d - %s", trackIndex + 1, p->instruments[row->instrument].name);
        break;
      }
      trackCursorAdvance(p, trackIndex, &scan);
    }
  }
  smfTrackText(track, 0, 0x03, trackName);

  uint32_t lastEventTick = 0;
  int currentNote = -1;

  for (int absoluteRow = 0; absoluteRow < totalRows; absoluteRow++) {
    int8_t transpose = 0;
    PhraseRow* row = resolveCursorRow(p, trackIndex, &cursor, &transpose);
    uint32_t tick = (uint32_t)absoluteRow * MIDI_TICKS_PER_ROW;

    if (row) {
      if (row->note == NOTE_OFF) {
        if (currentNote >= 0) {
          smfTrackNoteOff(track, tick - lastEventTick, channel, (uint8_t)currentNote, 0);
          lastEventTick = tick;
          currentNote = -1;
        }
      } else if (row->note != EMPTY_VALUE_8) {
        if (currentNote >= 0) {
          smfTrackNoteOff(track, tick - lastEventTick, channel, (uint8_t)currentNote, 0);
          lastEventTick = tick;
        }
        int effectiveNote = (int)row->note + transpose;
        uint8_t midiNote = noteIndexToMidi(effectiveNote);
        uint8_t velocity = volumeToVelocity(row->volume);
        smfTrackNoteOn(track, tick - lastEventTick, channel, midiNote, velocity);
        lastEventTick = tick;
        currentNote = midiNote;
      }
    }

    trackCursorAdvance(p, trackIndex, &cursor);
  }

  uint32_t endTick = (uint32_t)totalRows * MIDI_TICKS_PER_ROW;
  if (currentNote >= 0) {
    smfTrackNoteOff(track, endTick - lastEventTick, channel, (uint8_t)currentNote, 0);
    lastEventTick = endTick;
  }
  smfTrackEnd(track, endTick - lastEventTick);
}

int projectExportMidi(Project* project, const char* path) {
  int trackCount = project->tracksCount;
  if (trackCount < 1) trackCount = 1;
  if (trackCount > PROJECT_MAX_TRACKS) trackCount = PROJECT_MAX_TRACKS;

  int totalRows = computeSongLength(project, trackCount);
  if (totalRows <= 0) {
    snprintf(smfFileError, sizeof(smfFileError), "Song is empty, nothing to export");
    return 1;
  }

  int midiTrackCount = trackCount + 1; // + conductor track
  SmfTrackWriter* tracks = (SmfTrackWriter*)malloc(sizeof(SmfTrackWriter) * midiTrackCount);
  for (int i = 0; i < midiTrackCount; i++) smfTrackInit(&tracks[i]);

  writeConductorTrack(project, &tracks[0], totalRows, trackCount);
  for (int t = 0; t < trackCount; t++) {
    writeNoteTrack(project, &tracks[t + 1], t, totalRows);
  }

  int result = smfWriteFile(path, MIDI_PPQ, tracks, midiTrackCount);

  for (int i = 0; i < midiTrackCount; i++) smfTrackFree(&tracks[i]);
  free(tracks);

  return result;
}
