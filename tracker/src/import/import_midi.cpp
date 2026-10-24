#include <chipnomad_lib.h>
#include <project_utils.h>
#include <midi/smf_file.h>
#include "import_midi.h"
#include "import_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory>

#define MIDI_ROWS_PER_BEAT 4
#define MIDI_DEFAULT_TEMPO_USEC 500000 // 120 BPM, used when the file has no tempo meta-event
#define MIDI_GROOVE_TICKS 6 // matches the app's own default groove (see project.cpp projectInit)

struct ImportNoteSpan {
  uint32_t startRow;
  uint32_t endRow; // exclusive
  uint8_t midiNote;
  uint8_t volume;
};

enum class RowEventType : uint8_t { none, on, off };

struct RowEvent {
  RowEventType type;
  uint8_t note; // PitchTable index, valid when type == on
  uint8_t volume; // valid when type == on
};

// MIDI note -> PitchTable index. Inverse of the rule used by
// chipnomad_lib/export/export_midi.cpp (and matching how the app's own
// 12TET table generators lay out entry N as MIDI note 12+N).
static uint8_t midiNoteToIndex(Project* p, int midiNote) {
  int index = midiNote - 12;
  if (index < 0) index = 0;
  if (p->pitchTable.length > 0 && index >= p->pitchTable.length) index = p->pitchTable.length - 1;
  return (uint8_t)index;
}

static uint8_t velocityToVolume(uint8_t velocity) {
  int v = (velocity * 15 + 63) / 127;
  if (v < 0) v = 0;
  if (v > 15) v = 15;
  return (uint8_t)v;
}

static int compareByTick(const void* a, const void* b) {
  const SmfNoteEvent* ea = (const SmfNoteEvent*)a;
  const SmfNoteEvent* eb = (const SmfNoteEvent*)b;
  if (ea->absoluteTick < eb->absoluteTick) return -1;
  if (ea->absoluteTick > eb->absoluteTick) return 1;
  // Note-offs before note-ons at the same tick, so a retrigger on the same
  // pitch closes cleanly before reopening.
  if (ea->type != eb->type) return ea->type == SmfEventType::noteOff ? -1 : 1;
  return 0;
}

// Turns one MIDI channel's raw note on/off stream into a monophonic list of
// non-overlapping spans: a new note-on always cuts whatever was playing,
// matching how a single tracker track behaves.
static int buildNoteSpans(SmfNoteEventList* channel, double ticksPerRow, ImportNoteSpan** outSpans) {
  if (channel->count == 0) {
    *outSpans = NULL;
    return 0;
  }

  qsort(channel->events, channel->count, sizeof(SmfNoteEvent), compareByTick);

  ImportNoteSpan* spans = (ImportNoteSpan*)malloc(sizeof(ImportNoteSpan) * channel->count);
  int spanCount = 0;
  int activeNote = -1;
  uint32_t activeStartRow = 0;
  uint8_t activeVolume = 15;

  for (int i = 0; i < channel->count; i++) {
    SmfNoteEvent* e = &channel->events[i];
    uint32_t row = (uint32_t)(e->absoluteTick / ticksPerRow + 0.5);

    if (e->type == SmfEventType::noteOn) {
      if (activeNote >= 0) {
        uint32_t endRow = row > activeStartRow ? row : activeStartRow + 1;
        spans[spanCount++] = { activeStartRow, endRow, (uint8_t)activeNote, activeVolume };
      }
      activeNote = e->note;
      activeStartRow = row;
      activeVolume = velocityToVolume(e->velocity);
    } else if (e->type == SmfEventType::noteOff && activeNote == e->note) {
      uint32_t endRow = row > activeStartRow ? row : activeStartRow + 1;
      spans[spanCount++] = { activeStartRow, endRow, (uint8_t)activeNote, activeVolume };
      activeNote = -1;
    }
  }
  if (activeNote >= 0) {
    spans[spanCount++] = { activeStartRow, activeStartRow + 1, (uint8_t)activeNote, activeVolume };
  }

  *outSpans = spans;
  return spanCount;
}

// Writes one track's worth of spans into project->song/chains/phrases,
// starting from the shared chain/phrase index pools. Returns the number of
// rows actually written (may be less than requested if a pool is exhausted).
static void writeTrackTimeline(Project* project, int trackIdx, RowEvent* timeline, uint32_t rowCount,
                                int* nextChainIdx, int* nextPhraseIdx) {
  uint32_t row = 0;
  int songRow = 0;

  while (row < rowCount && songRow < PROJECT_MAX_LENGTH && *nextChainIdx < PROJECT_MAX_CHAINS &&
         *nextPhraseIdx < PROJECT_MAX_PHRASES) {
    int chainIdx = (*nextChainIdx)++;
    project->song[songRow][trackIdx] = chainIdx;
    chainClear(&project->chains[chainIdx]);

    for (int chainRow = 0; chainRow < 16 && row < rowCount; chainRow++) {
      if (*nextPhraseIdx >= PROJECT_MAX_PHRASES) break;
      int phraseIdx = (*nextPhraseIdx)++;
      phraseClear(&project->phrases[phraseIdx]);
      project->chains[chainIdx].rows[chainRow].phrase = phraseIdx;
      project->chains[chainIdx].rows[chainRow].transpose = 0;

      for (int phraseRowIdx = 0; phraseRowIdx < 16 && row < rowCount; phraseRowIdx++, row++) {
        PhraseRow* dest = &project->phrases[phraseIdx].rows[phraseRowIdx];
        initEmptyPhraseRow(dest);
        if (timeline[row].type == RowEventType::on) {
          dest->note = timeline[row].note;
          dest->instrument = 0;
          dest->volume = timeline[row].volume;
        } else if (timeline[row].type == RowEventType::off) {
          dest->note = NOTE_OFF;
        }
      }
    }

    songRow++;
  }
}

int projectLoadMidi(Project* project, const char* path) {
  if (!project || !path) return 1;

  SmfReadResult smf;
  if (smfReadFile(path, &smf) != 0) {
    snprintf(projectFileError, 40, "%s", smfFileError);
    return 1;
  }

  double ticksPerRow = smf.ppq > 0 ? smf.ppq / (double)MIDI_ROWS_PER_BEAT : 1.0;

  // Collect the MIDI channels that actually contain notes, in channel order.
  int activeChannels[SMF_MAX_CHANNELS];
  int activeChannelCount = 0;
  for (int c = 0; c < SMF_MAX_CHANNELS && activeChannelCount < PROJECT_MAX_TRACKS; c++) {
    if (smf.channels[c].count > 0) activeChannels[activeChannelCount++] = c;
  }

  if (activeChannelCount == 0) {
    snprintf(projectFileError, 40, "No notes found in MIDI file");
    smfFreeReadResult(&smf);
    return 1;
  }

  auto owned = std::make_unique<Project>();
  Project& p = *owned;
  projectInitAY(&p);
  // projectLoadInternal (chipnomad_lib/project_io.cpp) always resets
  // chipsCount/tracksCount to PROJECT_MAX_TRACKS after loading a project, no
  // matter what was saved - so a project with fewer tracks doesn't survive a
  // save/reload round-trip. Keep the default 8 tracks here too and simply
  // leave tracks beyond the imported channel count empty.

  uint32_t initialTempo = smf.tempoChanges.count > 0 ? smf.tempoChanges.changes[0].microsecondsPerQuarter : MIDI_DEFAULT_TEMPO_USEC;
  double rowSeconds = (initialTempo / 1000000.0) / MIDI_ROWS_PER_BEAT;
  p.tickRate = rowSeconds > 0 ? (float)(MIDI_GROOVE_TICKS / rowSeconds) : 50.0f;

  Instrument* inst = &p.instruments[0];
  getInstrumentFunctions(InstrumentType::AY1).init(inst);
  inst->type = InstrumentType::AY1;
  strncpy(inst->name, "MIDI Import", PROJECT_INSTRUMENT_NAME_LENGTH);
  inst->name[PROJECT_INSTRUMENT_NAME_LENGTH] = '\0';

  int nextChainIdx = 0;
  int nextPhraseIdx = 0;

  for (int t = 0; t < activeChannelCount; t++) {
    ImportNoteSpan* spans;
    int spanCount = buildNoteSpans(&smf.channels[activeChannels[t]], ticksPerRow, &spans);

    if (spanCount == 0) continue;

    // +1 so the last span's note-off has a row to land on (every earlier
    // span's off row is either followed by a coinciding note-on, needing no
    // explicit off, or falls strictly before this bound already).
    uint32_t rowCount = spans[spanCount - 1].endRow + 1;
    RowEvent* timeline = (RowEvent*)calloc(rowCount, sizeof(RowEvent));

    for (int i = 0; i < spanCount; i++) {
      timeline[spans[i].startRow].type = RowEventType::on;
      timeline[spans[i].startRow].note = midiNoteToIndex(&p, spans[i].midiNote);
      timeline[spans[i].startRow].volume = spans[i].volume;

      uint32_t offRow = spans[i].endRow;
      int coincidesWithNextOn = (i + 1 < spanCount && spans[i + 1].startRow == offRow);
      if (!coincidesWithNextOn && offRow < rowCount) {
        timeline[offRow].type = RowEventType::off;
      }
    }

    writeTrackTimeline(&p, t, timeline, rowCount, &nextChainIdx, &nextPhraseIdx);

    free(timeline);
    free(spans);
  }

  smfFreeReadResult(&smf);

  projectFree(project);
  *project = p;

  return 0;
}
