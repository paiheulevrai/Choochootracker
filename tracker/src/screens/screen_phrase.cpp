#include "screens.h"
#include "common.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "chipnomad_lib.h"
#include "project_utils.h"
#include "copy_paste.h"
#include "help.h"
#include "synth/sample_voice.h"

static int phraseIdx = 0;
static PhraseRow *phraseRows = NULL;
static int isFxEdit = 0;

static uint8_t lastNote = 48;
static uint8_t lastInstrument = 0;

static uint16_t lastVolume = PHRASE_VOLUME_MAX;
static uint8_t lastFX[2] = {0, 0};

// Selection fill/mutate/arp cycle positions (see the fill section below).
static int fillCycleIndex = 0;
static int mutateCycleIndex = 0;
static int arpCycleIndex = 0;
// 0 = the slice-spread phase is still pending in a mixed selection,
// 1 = the random arp phase has taken over (reset with the cycles).
static int arpSlicePhase = 0;

// Selection-mode combo hints (session-only): while a selection is active,
// draw() rotates a hint for the selection combos in the message bar every
// 2 s (120 frames at 60 FPS). The rotation pauses while any other
// message is active and restarts from the first hint when selection mode
// is entered or left.
static int selectionHintPhase = 0;
static int selectionHintActive = 0;

// Velocity randomize (B+UP on a velocity-only selection): the first press
// of a hold captures the selection's volumes so the 4th press can restore
// them; presses 1-3 apply the Minimal (+-20), Medium (+-50) and Random
// (fully random) patterns.
static uint16_t velocityOriginalVolumes[16];
static int velocityCycleIndex = 0;

static int getColumnCount(int row);
static void drawStatic(void);
static void drawField(int col, int row, CellState state);
static void drawRowHeader(int row, CellState state);
static void drawColHeader(int col, CellState state);
static void drawCursor(int col, int row);
static void drawSelection(int col1, int row1, int col2, int row2);
static int onEdit(int col, int row, CellEditAction action);
static LoopRange getLoopRange(void);

static int columnX[] = {3, 7, 10, 13, 16, 19, 22, 25, 28, 31};

static ScreenData screen = {
  .rows = 16,
  .cursorRow = 0,
  .cursorCol = 0,
  .topRow = 0,
  .selectMode = 0,
  .selectStartRow = 0,
  .selectStartCol = 0,
  .selectAnchorRow = 0,
  .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = getColumnCount,
  .drawStatic = drawStatic,
  .drawCursor = drawCursor,
  .drawSelection = drawSelection,
  .drawRowHeader = drawRowHeader,
  .drawColHeader = drawColHeader,
  .drawField = drawField,
  .onEdit = onEdit,
  .onInput = NULL,
  .onRawInput = NULL,
  .isCellValid = NULL,
  .getLoopRange = getLoopRange,
};

static void init(void) {
  lastNote = 48;
  lastInstrument = 0;
  lastVolume = PHRASE_VOLUME_MAX;
  lastFX[0] = 0;
  lastFX[1] = 0;
  screen.cursorRow = 0;
  screen.cursorCol = 0;
  screen.topRow = 0;
  screen.selectMode = 0;
  screen.selectStartRow = 0;
  screen.selectStartCol = 0;
  screen.selectAnchorRow = 0;
  screen.selectAnchorCol = 0;
  isFxEdit = 0;
  fillCycleIndex = 0;
  mutateCycleIndex = 0;
  arpCycleIndex = 0;
  arpSlicePhase = 0;
  velocityCycleIndex = 0;
}

static void setup(int input) {
  phraseIdx = chipnomadState->project.chains[chipnomadState->project.song[*pSongRow][*pSongTrack]].rows[*pChainRow].phrase;
  phraseRows = chipnomadState->project.phrases[phraseIdx].rows;
  screen.selectMode = 0;
  isFxEdit = 0;
  fillCycleIndex = 0;
  mutateCycleIndex = 0;
  arpCycleIndex = 0;
  arpSlicePhase = 0;
  velocityCycleIndex = 0;
}

///////////////////////////////////////////////////////////////////////////////
//
// Drawing functions
//

static int getColumnCount(int row) {
  return 9;
}

static void drawStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrintf(0, 0, "PHRASE %03X%c", phraseIdx, isPhraseUsedElsewhere(&chipnomadState->project, phraseIdx, chipnomadState->project.song[*pSongRow][*pSongTrack], *pChainRow) ? '*' : ' ');
}

static void fullRedraw(void) {
  screenFullRedraw(&screen);
}

static void drawField(int col, int row, CellState state) {
  if (row < screen.topRow || row >= screen.topRow + screenVisibleRows()) return;
  int x = columnX[col];
  int y = 3 + row - screen.topRow;
  if (col == 0) {
    // Note
    uint8_t value = phraseRows[row].note;
    setCellColor(state, value == EMPTY_VALUE_8, 1);
    gfxPrint(x, y, noteName(&chipnomadState->project, value));
  } else if (col == 1 || col == 2) {
    // Instrument and volume
    uint16_t value = (col == 1) ? phraseRows[row].instrument : phraseRows[row].volume;
    setCellColor(state, value == (col == 1 ? EMPTY_VALUE_8 : EMPTY_VALUE_16), 1);
    gfxPrint(x, y, col == 1 ? byteToHexOrEmpty((uint8_t)value) : volumeToHexOrEmpty(value));
  } else if (col == 3 || col == 5 || col == 7) {
    // FX name
    uint8_t fx = phraseRows[row].fx[(col - 3) / 2][0];
    setCellColor(state, fx == EMPTY_VALUE_8, 1);
    gfxPrint(x, y, fxNames[fx].name);
  } else if (col == 4 || col == 6 || col == 8) {
    // FX value
    uint8_t value = phraseRows[row].fx[(col - 4) / 2][1];
    setCellColor(state, 0, phraseRows[row].fx[(col - 3) / 2][0] != EMPTY_VALUE_8);
    gfxPrint(x, y, byteToHex(value));
  }
}

static void drawRowHeader(int row, CellState state) {
  if (row < screen.topRow || row >= screen.topRow + screenVisibleRows()) return;
  const ColorScheme cs = appSettings.colorScheme;
  gfxSetFgColor((state == CellState::focus) ? cs.textDefault : ((row & 3) == 0 ? cs.textValue : cs.textInfo));
  gfxPrintf(1, 3 + row - screen.topRow, "%X", row);
}

static void drawColHeader(int col, CellState state) {
  const ColorScheme cs = appSettings.colorScheme;
  gfxSetFgColor((state == CellState::focus) ? cs.textDefault : cs.textInfo);

  switch (col) {
    case 0:
      gfxPrint(3, 2, "N");
      break;
    case 1:
      gfxPrint(7, 2, "I");
      break;
    case 2:
      gfxPrint(10, 2, "V");
      break;
    case 3:
    case 4:
      gfxPrint(13, 2, "FX1");
      break;
    case 5:
    case 6:
      gfxPrint(19, 2, "FX2");
      break;
    case 7:
    case 8:
      gfxPrint(25, 2, "FX3");
      break;
    default:
      break;
  }
}

static void drawCursor(int col, int row) {
  if (row < screen.topRow || row >= screen.topRow + screenVisibleRows()) return;
  int width = 2;
  if (col == 0 || col == 3 || col == 5 || col == 7) width = 3;
  gfxCursor(col == 0 ? 3 : 4 + col * 3, 3 + row - screen.topRow, width);
}

static void drawSelection(int col1, int row1, int col2, int row2) {
  int x = columnX[col1];
  int w = columnX[col2 + 1] - x - 1;
  int y = 3 + row1 - screen.topRow;
  int h = row2 - row1 + 1;
  if (col2 == 3 || col2 == 5 || col2 == 7) w++;
  gfxRect(x, y, w, h);
}

// Advance the rotating selection-mode hint. Called from draw() while a
// selection is active and no other message is showing; each call displays
// the next hint for 2 s.
static void selectionUpdateHint(void) {
  if (!selectionHintActive) {
    // Selection mode was just entered: restart the rotation.
    selectionHintActive = 1;
    selectionHintPhase = 0;
  }
  static const char* hints[] = {
    "EDIT + DIR = batch note edit",
    "Double-tap EDIT: resample selection",
    "OPT + LEFT = pattern fill",
    "OPT + RIGHT = random fill",
    "OPT + UP = mutate/randomize velocity",
    "OPT + DOWN = random arp/slice spread",
  };
  screenMessage(120, "%s", hints[selectionHintPhase++ % 6]);
}

static void draw(void) {
  if (isFxEdit) return;

  // Rotating combo hints while a selection is active: only when no other
  // message (action feedback, error) is showing.
  if (screen.selectMode == 1) {
    if (!screenGetActiveMessage()[0]) selectionUpdateHint();
  } else {
    selectionHintActive = 0;
  }

  gfxClearRect(0, 3, 1, screenVisibleRows());
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  if (*pChainRow >= screen.topRow && *pChainRow < screen.topRow + screenVisibleRows())
    gfxPrint(0, 3 + *pChainRow - screen.topRow, "<");

  gfxClearRect(2, 3, 1, screenVisibleRows());
  const PlaybackTrackState* track = &chipnomadGetPlaybackStatus(chipnomadState)->tracks[*pSongTrack];
  if (track->mode != PlaybackMode::stopped && track->mode != PlaybackMode::phraseRow && track->songRow != EMPTY_VALUE_16) {
    // Chain row
    if (*pSongRow == track->songRow && track->chainRow >= screen.topRow &&
        track->chainRow < screen.topRow + screenVisibleRows()) {
      gfxSetFgColor(appSettings.colorScheme.playMarkers);
      gfxPrint(0, 3 + track->chainRow - screen.topRow, "<");
    }

    // Phrase row
    int playingPhrase = chipnomadState->project.chains[chipnomadState->project.song[track->songRow][*pSongTrack]].rows[track->chainRow].phrase;
    if (playingPhrase == phraseIdx) {
      int row = track->phraseRow;
      if (row >= screen.topRow && row < screen.topRow + screenVisibleRows()) {
        gfxSetFgColor(appSettings.colorScheme.playMarkers);
        gfxPrint(2, 3 + row - screen.topRow, ">");
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
//
// Note Lock: when the scale engine runs in Note Lock mode, notes typed into
// the phrase are snapped to the nearest note of the project scale (up when
// raising a note, down otherwise). Sliced-sample instruments are exempt:
// their notes select slices chromatically from C-0, not pitches. Non-12-TET
// pitch tables are exempt too, matching the playback quantizer's gate.
//

// 1 when notes entered on the current track must be locked to the scale.
static int noteLockActive(void) {
  Project* p = &chipnomadState->project;
  if (!p->scaleMode || !p->scaleApply) return 0;
  if (p->pitchTable.octaveSize != 12) return 0;
  // The track checkboxes in the Scale screen gate the mode per track.
  if (!(p->scaleTracksMask & (1u << *pSongTrack))) return 0;
  return 1;
}

// 1 when the instrument resolved for this position is a sliced sample (its
// notes are slice indices from C-0, so they must stay chromatic).
static int noteLockSlicedSampleHere(int row) {
  uint8_t instrument = phraseRows[row].instrument;
  if (instrument == EMPTY_VALUE_8)
    instrument = lookupInstrument(&chipnomadState->project, *pSongRow, *pChainRow, row, *pSongTrack);
  if (instrument == EMPTY_VALUE_8) return 0;
  const Instrument* inst = &chipnomadState->project.instruments[instrument];
  if (inst->type != InstrumentType::Sample) return 0;
  return sampleActsAsSliced(&inst->chip.sample);
}

// 1 for edit actions that raise a value (EDIT + RIGHT/UP, including their
// multi-edit forms): those snap to the next scale note above instead of the
// one below.
static int noteLockSnapUp(CellEditAction action) {
  return action == CellEditAction::increase || action == CellEditAction::increaseBig ||
    action == CellEditAction::multiIncrease || action == CellEditAction::multiIncreaseBig;
}

// Snap a note being entered to the project scale: increases snap up to the
// next scale note, every other edit snaps down (the quantizer direction).
// Returns the note unchanged when Note Lock doesn't apply.
static uint8_t noteLockSnap(uint8_t note, int up) {
  if (!noteLockActive()) return note;
  if (noteLockSlicedSampleHere(screen.cursorRow)) return note;
  Project* p = &chipnomadState->project;
  uint16_t mask = p->scalePreset == scaleCustom ? p->scaleCustomMask : scalePresetMask(p->scalePreset);
  return up ? scaleSnapNoteUp(note, p->scaleRoot, mask, p->pitchTable.length)
    : scaleQuantizeNote(note, p->scaleRoot, mask, p->pitchTable.length);
}

///////////////////////////////////////////////////////////////////////////////
//
// Selection fill (note column only): with a selection active, B+LEFT
// cycles fixed rhythm patterns and B+RIGHT applies randomized fills. Every
// press first erases the selection's note cells, then writes the current
// cycle step; the step after the last pattern erases only and the pattern
// cycle restarts, while random fills cycle indefinitely. Releasing B
// resets the pattern cycle. Patterns are aligned to absolute phrase rows,
// so a partial selection only receives the steps that fall inside it.
// Placed notes carry the last used instrument.
//

static const char* const fillPatterns[] = {
  "X-------X-------",
  "----X-------X---",
  "X---X---X---X---",
  "--X---X---X---X-",
  "X-X-X-X-X-X-X-X-",
  "X--XX--XX--XX--X",
  "X---XX--X---XX--",
  "X--XXX--X--XXX--",
};
static const int fillPatternCount = (int)(sizeof(fillPatterns) / sizeof(fillPatterns[0]));

// Snap a filled note to the project scale (Note Lock), scoped to the
// target row - unlike noteLockSnap, which is scoped to the cursor row.
static uint8_t fillSnapNote(int row) {
  if (!noteLockActive()) return lastNote;
  if (noteLockSlicedSampleHere(row)) return lastNote;
  Project* p = &chipnomadState->project;
  uint16_t mask = p->scalePreset == scaleCustom ? p->scaleCustomMask : scalePresetMask(p->scalePreset);
  return scaleQuantizeNote(lastNote, p->scaleRoot, mask, p->pitchTable.length);
}

static void fillEraseNotes(int startRow, int endRow) {
  for (int r = startRow; r <= endRow; r++) phraseRows[r].note = EMPTY_VALUE_8;
}

static void fillApplyPattern(const char* pattern, int startRow, int endRow) {
  for (int r = startRow; r <= endRow; r++) {
    if (pattern[r] == 'X') {
      phraseRows[r].note = fillSnapNote(r);
      phraseRows[r].instrument = lastInstrument;
    }
  }
}

static void fillApplyRandom(int startRow, int endRow) {
  // 3-6 notes are equally likely; 7 and 8 are less probable.
  int roll = utilsRandom() % 10;
  int count = roll < 8 ? 3 + (roll % 4) : 7 + (roll - 8);

  // Partial Fisher-Yates: pick `count` distinct positions out of the
  // phrase's 16 rows, then place notes only where a picked position falls
  // inside the selection.
  int positions[16];
  for (int i = 0; i < 16; i++) positions[i] = i;
  for (int i = 0; i < count; i++) {
    int j = i + utilsRandom() % (16 - i);
    int swap = positions[i];
    positions[i] = positions[j];
    positions[j] = swap;
    int row = positions[i];
    if (row >= startRow && row <= endRow) {
      phraseRows[row].note = fillSnapNote(row);
      phraseRows[row].instrument = lastInstrument;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
//
// Selection mutate (note column only): with a selection active, B+UP
// nudges the notes already placed in the selection. Every press picks
// 1-3 of the placed notes and shifts each by a random semitone delta
// within one octave, weighted toward small shifts. Note Lock snaps the
// results back into the project scale (up for raised notes, down for
// lowered ones); sliced-sample instruments stay chromatic - their notes
// select slices, not pitches. Empty cells and OFF commands are never
// touched, and nothing is erased: the 8th press restores the notes the
// selection held when the cycle started, then the cycle restarts.
// Releasing B resets the cycle.
//

static uint8_t mutateOriginalNotes[16];

// Random semitone delta within one octave, weighted toward small
// shifts: 1-3 semitones is the most likely range, 4-6 next, 7-12 (the
// extremes) the least. Never zero, so a mutation always moves.
static int mutateRandomDelta(void) {
  int roll = utilsRandom() % 10;
  int magnitude;
  if (roll < 4) magnitude = 1 + utilsRandom() % 3;      // 1-3 semitones
  else if (roll < 7) magnitude = 4 + utilsRandom() % 3; // 4-6 semitones
  else magnitude = 7 + utilsRandom() % 6;               // 7-12 semitones
  return (utilsRandom() & 1) ? magnitude : -magnitude;
}

// Shift one placed note by delta, clamped into the pitch table (the
// direction flips at a boundary so the mutation still moves the note),
// then snap it back into the project scale when Note Lock applies,
// following the note lock logic: raised notes snap up, lowered notes
// snap down. Sliced-sample instruments are exempt - their notes select
// slices chromatically, so they randomize without scale snapping.
static void mutateNote(int row, int delta) {
  Project* p = &chipnomadState->project;
  int note = phraseRows[row].note;
  int candidate = note + delta;
  if (candidate < 0 || candidate >= p->pitchTable.length) {
    candidate = note - delta;
    if (candidate < 0) candidate = 0;
    if (candidate >= p->pitchTable.length) candidate = p->pitchTable.length - 1;
  }
  if (noteLockActive() && !noteLockSlicedSampleHere(row)) {
    uint16_t mask = p->scalePreset == scaleCustom ? p->scaleCustomMask : scalePresetMask(p->scalePreset);
    candidate = candidate > note
      ? scaleSnapNoteUp((uint8_t)candidate, p->scaleRoot, mask, p->pitchTable.length)
      : scaleQuantizeNote((uint8_t)candidate, p->scaleRoot, mask, p->pitchTable.length);
  }
  phraseRows[row].note = (uint8_t)candidate;
}

// Pick 1-3 of the placed notes in the selection (uniformly, capped to
// how many the selection holds) and mutate each one.
static void mutateApply(int startRow, int endRow) {
  int placed[16];
  int placedCount = 0;
  for (int r = startRow; r <= endRow; r++) {
    uint8_t note = phraseRows[r].note;
    if (note != EMPTY_VALUE_8 && note != NOTE_OFF) placed[placedCount++] = r;
  }
  if (placedCount == 0) return;
  int count = 1 + utilsRandom() % 3;
  if (count > placedCount) count = placedCount;
  for (int i = 0; i < count; i++) {
    int j = i + utilsRandom() % (placedCount - i);
    int swap = placed[i];
    placed[i] = placed[j];
    placed[j] = swap;
    mutateNote(placed[i], mutateRandomDelta());
  }
}

///////////////////////////////////////////////////////////////////////////////
//
// Selection slice spread (note column only): with a selection active,
// B+DOWN turns the placed notes of every sliced Sampler instrument into
// a chromatic run - the instrument's first note (highest in the
// sequencer) keeps its value and every following note of that
// instrument rises one semitone above the previous one, so a stack of
// identical notes becomes consecutive slice triggers. Several sliced
// instruments spread independently from their own first notes,
// instruments without slicing are left untouched, and the result is
// never snapped to the project scale. This easter egg takes priority
// over the random arp below: a selection holding only sliced
// instruments spreads on every press, while a mixed selection spreads
// on the first press of a hold and then hands its remaining notes to
// the random arp. The last note the spread writes becomes the last
// note used in note input.
//

// Resolve the instrument playing this row: the row's own instrument, or
// the one inherited from the rows above it.
static int sliceSpreadInstrumentAt(int row) {
  uint8_t instrument = phraseRows[row].instrument;
  if (instrument == EMPTY_VALUE_8)
    instrument = lookupInstrument(&chipnomadState->project, *pSongRow, *pChainRow, row, *pSongTrack);
  return instrument;
}

// 1 when the resolved instrument is a sliced sample: its notes select
// slices rather than pitches, so they spread chromatically.
static int sliceSpreadIsSliced(int instrument) {
  if (instrument < 0 || instrument >= PROJECT_MAX_INSTRUMENTS) return 0;
  const Instrument* inst = &chipnomadState->project.instruments[instrument];
  if (inst->type != InstrumentType::Sample) return 0;
  return sampleActsAsSliced(&inst->chip.sample);
}

static void sliceSpreadApply(int startRow, int endRow) {
  Project* p = &chipnomadState->project;
  // Per-instrument run state: the first placed note's value and how many
  // notes of that instrument have been seen so far.
  int runBase[PROJECT_MAX_INSTRUMENTS];
  int runIndex[PROJECT_MAX_INSTRUMENTS];
  for (int i = 0; i < PROJECT_MAX_INSTRUMENTS; i++) {
    runBase[i] = -1;
    runIndex[i] = 0;
  }
  int lastWritten = -1;
  for (int r = startRow; r <= endRow; r++) {
    uint8_t note = phraseRows[r].note;
    if (note == EMPTY_VALUE_8 || note == NOTE_OFF) continue;
    int instrument = sliceSpreadInstrumentAt(r);
    if (!sliceSpreadIsSliced(instrument)) continue;
    int index = runIndex[instrument]++;
    if (index == 0) {
      // The first note of the run stays where it is and anchors it.
      runBase[instrument] = note;
      continue;
    }
    int spread = runBase[instrument] + index;
    if (spread >= p->pitchTable.length) spread = p->pitchTable.length - 1;
    phraseRows[r].note = (uint8_t)spread;
    lastWritten = spread;
  }
  // The last note the spread wrote becomes the last note used in note
  // input, so continuing the run manually picks up the next slice.
  if (lastWritten >= 0) lastNote = (uint8_t)lastWritten;
}

///////////////////////////////////////////////////////////////////////////////
//
// Selection random arp (note column only): with a selection active,
// B+DOWN arpeggiates the notes already placed on non-sliced
// instruments with a random chord from the CRD palette; a selection
// holding sliced-instrument notes runs the slice spread first (see
// above). The chord root is a random note of the project scale (any
// chromatic note when no scale applies to the track) inside the register
// of the selection's original notes, and the chord type is random: 3+
// note chords while the selection holds fewer than 4 notes, any chord
// for larger selections. Every press re-spreads the chord tones across
// the placed rows - first in a randomized order, then lowest to
// highest, highest to lowest, then up-and-down - the 5th press restores
// the original notes, and the next press picks a fresh chord. No notes
// are added or removed, and releasing B resets the whole process.
//

static uint8_t arpOriginalNotes[16];
static int arpRows[16];
static int arpRowCount = 0;
static uint8_t arpChordSlot = 0;
static uint8_t arpChordTones[CHORD_MAX_VOICES];
static int arpChordCount = 0;

// Pick a fresh chord: the root is a random note of the project scale
// inside the register of the selection's original notes (any note when
// no scale is applied to the current track), the type is a random CRD
// palette entry - 3+ note chords while the selection holds fewer than 4
// notes so short arpeggios don't repeat - and the tones are quantized
// into the scale the same way playback quantizes CRD chords.
static void arpChooseChord(void) {
  Project* p = &chipnomadState->project;
  int scaleActive = p->scaleApply && (p->scaleTracksMask & (1u << *pSongTrack));
  uint16_t mask = scaleActive ?
    (p->scalePreset == scaleCustom ? p->scaleCustomMask : scalePresetMask(p->scalePreset)) : 0x0fff;
  uint8_t scaleRoot = scaleActive ? p->scaleRoot : 0;

  int minNote = -1, maxNote = -1;
  for (int i = 0; i < arpRowCount; i++) {
    int note = arpOriginalNotes[arpRows[i]];
    if (minNote < 0 || note < minNote) minNote = note;
    if (maxNote < 0 || note > maxNote) maxNote = note;
  }

  // Random scale note inside the original register.
  int count = 0;
  for (int n = minNote; n <= maxNote; n++) {
    int degree = (n - scaleRoot) % 12;
    if (degree < 0) degree += 12;
    if (mask & (1u << degree)) count++;
  }
  int rootNote;
  if (count > 0) {
    int pick = utilsRandom() % count;
    rootNote = minNote;
    for (;;) {
      int degree = (rootNote - scaleRoot) % 12;
      if (degree < 0) degree += 12;
      if (mask & (1u << degree)) {
        if (pick == 0) break;
        pick--;
      }
      rootNote++;
    }
  } else {
    // No scale note inside the register: settle on the closest one below.
    rootNote = scaleQuantizeNote((uint8_t)minNote, scaleRoot, mask, p->pitchTable.length);
  }

  if (arpRowCount <= 2) {
    arpChordSlot = (uint8_t)(utilsRandom() % 6); // 3-note chords: Major..Sus4
  } else if (arpRowCount == 3) {
    // 3+ note chords: the whole palette except the 2-note Power chord.
    arpChordSlot = (uint8_t)(utilsRandom() % 15);
    if (arpChordSlot >= 6) arpChordSlot++;
  } else {
    arpChordSlot = (uint8_t)(utilsRandom() % 16);
  }
  arpChordCount = chordBuild((uint8_t)rootNote, arpChordSlot, 0, p->pitchTable.length, arpChordTones);
  if (scaleActive) {
    for (int i = 0; i < arpChordCount; i++) {
      arpChordTones[i] = scaleQuantizeNote(arpChordTones[i], p->scaleRoot, mask, p->pitchTable.length);
    }
  }
}

// Spread the chord tones across the placed rows: mode 0 randomizes the
// order, 1 runs lowest to highest, 2 highest to lowest and 3 moves
// up-and-down. Tones repeat cyclically when there are more notes to
// fill than the chord has.
static void arpSpread(int mode) {
  uint8_t order[CHORD_MAX_VOICES];
  for (int i = 0; i < arpChordCount; i++) order[i] = arpChordTones[i];
  if (mode == 0) {
    for (int i = arpChordCount - 1; i > 0; i--) {
      int j = utilsRandom() % (i + 1);
      uint8_t swap = order[i];
      order[i] = order[j];
      order[j] = swap;
    }
  } else if (mode == 2) {
    for (int i = 0; i < arpChordCount / 2; i++) {
      uint8_t swap = order[i];
      order[i] = order[arpChordCount - 1 - i];
      order[arpChordCount - 1 - i] = swap;
    }
  }
  for (int i = 0; i < arpRowCount; i++) {
    int tone;
    if (mode == 3 && arpChordCount > 1) {
      // Up-and-down without repeating the turning note: 0..K-1 then
      // K-2..1, then repeat.
      int period = (arpChordCount - 1) * 2;
      int phase = i % period;
      tone = order[phase < arpChordCount ? phase : period - phase];
    } else {
      tone = order[i % arpChordCount];
    }
    phraseRows[arpRows[i]].note = (uint8_t)tone;
  }
}

// One B+DOWN press on a selection without sliced instruments: advance
// the arp cycle - new chord with a randomized spread, up, down,
// up+down, restore, then a fresh chord.
static void randomArpPress(int startRow, int endRow) {
  static const char* const arpModeNames[] = {
    "RANDOM ARP", "RANDOM ARP UP", "RANDOM ARP DOWN", "RANDOM ARP UP+DOWN"
  };
  if (arpCycleIndex == 0) {
    // New cycle: capture the selection's placed notes on non-sliced
    // instruments and pick a chord.
    arpRowCount = 0;
    for (int r = startRow; r <= endRow; r++) {
      uint8_t note = phraseRows[r].note;
      if (note == EMPTY_VALUE_8 || note == NOTE_OFF) continue;
      if (sliceSpreadIsSliced(sliceSpreadInstrumentAt(r))) continue;
      arpRows[arpRowCount] = r;
      arpOriginalNotes[r] = note;
      arpRowCount++;
    }
    if (arpRowCount == 0) return;
    arpChooseChord();
    arpSpread(0);
  } else if (arpCycleIndex < 4) {
    arpSpread(arpCycleIndex);
  } else {
    // 5th press: back to the original notes; the next press picks a
    // fresh chord.
    for (int i = 0; i < arpRowCount; i++) {
      phraseRows[arpRows[i]].note = arpOriginalNotes[arpRows[i]];
    }
    screenMessage(MESSAGE_TIME, "RESTORED");
    arpCycleIndex = 0;
    return;
  }
  screenMessage(MESSAGE_TIME, "%s (%s)", arpModeNames[arpCycleIndex], chordName(arpChordSlot));
  arpCycleIndex++;
}

// Velocity randomize: cycle through three randomizing patterns and reset
// on the 4th press. Only runs when the selection covers the volume column
// exclusively (single column, col 2) - with any other selection the
// mutate feature handles B+UP instead.
static const char* velocityApplyPattern(int startRow, int endRow, int pattern) {
  // pattern 0 = Minimal (up to +-20), 1 = Medium (up to +-50),
  // 2 = Random (completely random values)
  for (int r = startRow; r <= endRow; r++) {
    uint16_t volume = phraseRows[r].volume;
    if (volume == EMPTY_VALUE_16) continue;
    uint16_t next;
    if (pattern == 2) {
      next = (uint16_t)(utilsRandom() % (PHRASE_VOLUME_MAX + 1));
    } else {
      const int range = pattern == 0 ? 20 : 50;
      // Delta in [-range, +range]; clamp to the 0..127 volume range.
      int delta = (int)(utilsRandom() % (2 * range + 1)) - range;
      int value = (int)volume + delta;
      if (value < 0) value = 0;
      if (value > PHRASE_VOLUME_MAX) value = PHRASE_VOLUME_MAX;
      next = (uint16_t)value;
    }
    phraseRows[r].volume = next;
  }
  static const char* labels[] = {"VELOCITY +-20", "VELOCITY +-50", "VELOCITY RANDOM"};
  return labels[pattern];
}

// Returns 1 when the combo was consumed by the fill/mutate/spread/arp
// features. Key-downs are only consumed while a selection is active and
// includes the note column - outside select mode B+LEFT/RIGHT/UP/DOWN
// keep navigating tracks and phrases. Key-ups reset the cycles whenever
// B is no longer held, in any mode.
static int fillInput(int isKeyDown, int keys) {
  if (isKeyDown) {
    if (screen.selectMode != 1) return 0;
    if (keys != (keyOpt | keyLeft) && keys != (keyOpt | keyRight) &&
      keys != (keyOpt | keyUp) && keys != (keyOpt | keyDown)) return 0;
    int startCol, startRow, endCol, endRow;
    getSelectionBounds(&screen, &startCol, &startRow, &endCol, &endRow);
    // Velocity randomize: B+UP on a velocity-only selection (single
    // column, col 2) cycles the randomizing patterns instead of mutate.
    if (keys == (keyOpt | keyUp) && startCol == endCol && startCol == 2) {
      // The fill supersedes a pending copy-on-Opt-release.
      screenClearOptPressed();
      if (velocityCycleIndex == 0) {
        // First press of a hold: capture the selection's volumes so the
        // 4th press can restore them.
        for (int r = startRow; r <= endRow; r++) {
          velocityOriginalVolumes[r] = phraseRows[r].volume;
        }
      }
      if (velocityCycleIndex < 3) {
        // Presses 1-3 apply the Minimal, Medium and Random patterns.
        const char* label = velocityApplyPattern(startRow, endRow,
                                                 velocityCycleIndex);
        screenMessage(MESSAGE_TIME, "%s", label);
        velocityCycleIndex++;
      } else {
        // 4th press: restore the volumes captured on the first press.
        for (int r = startRow; r <= endRow; r++) {
          phraseRows[r].volume = velocityOriginalVolumes[r];
        }
        screenMessage(MESSAGE_TIME, "RESTORED");
        velocityCycleIndex = 0;
      }
      projectModified = 1;
      fullRedraw();
      return 1;
    }
    if (startCol != 0) return 0; // Features are scoped to the note column
    // The fill supersedes a pending copy-on-Opt-release.
    screenClearOptPressed();

    if (keys == (keyOpt | keyLeft)) {
      if (fillCycleIndex < fillPatternCount) {
        fillEraseNotes(startRow, endRow);
        fillApplyPattern(fillPatterns[fillCycleIndex], startRow, endRow);
        screenMessage(MESSAGE_TIME, "PATTERN FILL");
        fillCycleIndex++;
      } else {
        // After the last pattern: erase only, then start over.
        fillEraseNotes(startRow, endRow);
        fillCycleIndex = 0;
        screenMessage(MESSAGE_TIME, "PATTERN CLEARED");
      }
    } else if (keys == (keyOpt | keyRight)) {
      // Random fills cycle indefinitely while B is held.
      fillEraseNotes(startRow, endRow);
      fillApplyRandom(startRow, endRow);
      screenMessage(MESSAGE_TIME, "RANDOM FILL");
    } else if (keys == (keyOpt | keyUp)) {
      // Mutate: the first press of a hold captures the selection's
      // notes so the 8th press can restore them.
      if (mutateCycleIndex == 0) {
        for (int r = startRow; r <= endRow; r++) mutateOriginalNotes[r] = phraseRows[r].note;
        mutateApply(startRow, endRow);
        screenMessage(MESSAGE_TIME, "MUTATE");
        mutateCycleIndex = 1;
      } else if (mutateCycleIndex < 7) {
        mutateApply(startRow, endRow);
        screenMessage(MESSAGE_TIME, "MUTATE");
        mutateCycleIndex++;
      } else {
        // 8th press: back to the original notes, cycle restarts.
        for (int r = startRow; r <= endRow; r++) phraseRows[r].note = mutateOriginalNotes[r];
        screenMessage(MESSAGE_TIME, "RESTORED");
        mutateCycleIndex = 0;
      }
    } else if (keys == (keyOpt | keyDown)) {
      // B+DOWN is conditional: notes on sliced Sampler instruments get
      // the slice spread easter egg, the remaining notes get the random
      // arp. A selection holding only sliced instruments spreads on
      // every press; a mixed selection spreads on the first press of a
      // hold and hands the other notes to the arp from the second press
      // on. Chord arithmetic assumes 12-TET, same as playback's CRD
      // handling.
      int slicedCount = 0, plainCount = 0;
      for (int r = startRow; r <= endRow; r++) {
        uint8_t note = phraseRows[r].note;
        if (note == EMPTY_VALUE_8 || note == NOTE_OFF) continue;
        if (sliceSpreadIsSliced(sliceSpreadInstrumentAt(r))) slicedCount++;
        else plainCount++;
      }
      if (slicedCount > 0 && plainCount == 0) {
        // Only sliced instruments: every press re-spreads the slices.
        sliceSpreadApply(startRow, endRow);
        screenMessage(MESSAGE_TIME, "SLICE SPREAD");
      } else if (slicedCount > 0) {
        // Mixed: the first press spreads the slices, then the arp
        // takes over the remaining notes.
        if (arpSlicePhase == 0) {
          sliceSpreadApply(startRow, endRow);
          screenMessage(MESSAGE_TIME, "SLICE SPREAD");
          if (chipnomadState->project.pitchTable.octaveSize == 12) arpSlicePhase = 1;
        } else {
          randomArpPress(startRow, endRow);
        }
      } else if (plainCount > 0 && chipnomadState->project.pitchTable.octaveSize == 12) {
        randomArpPress(startRow, endRow);
      }
    }
    projectModified = 1;
    fullRedraw();
    return 1;
  }

  // Key-up: once B is no longer held, reset the cycles so the next hold
  // starts from the beginning again.
  if (!(keys & keyOpt)) {
    fillCycleIndex = 0;
    mutateCycleIndex = 0;
    arpCycleIndex = 0;
    arpSlicePhase = 0;
    velocityCycleIndex = 0;
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
//
// Input handling
//

// Preview a row's note through the audio engine, same as any note edit does.
static void triggerRowPreview(int row) {
  if (chipnomadGetPlaybackStatus(chipnomadState)->isPlaying &&
      chipnomadGetPlaybackStatus(chipnomadState)->tracks[*pSongTrack].mode != PlaybackMode::phraseRow) {
    return;
  }
  PhraseRow* previewSource = &phraseRows[row];
  if (previewSource->note != EMPTY_VALUE_8 && previewSource->note != NOTE_OFF && previewSource->instrument == EMPTY_VALUE_8) {
    PhraseRow previewRow = *previewSource;
    previewRow.instrument = lookupInstrument(&chipnomadState->project, *pSongRow, *pChainRow, row, *pSongTrack);
    chipnomadQueuePlaybackStartPhraseRow(chipnomadState, *pSongTrack, &previewRow);
  } else {
    chipnomadQueuePlaybackStartPhraseRow(chipnomadState, *pSongTrack, previewSource);
  }
}

static int editCell(int col, int row, CellEditAction action) {
  int handled = 0;
  uint16_t maxVolume = PHRASE_VOLUME_MAX;

  if (col == 0) {
    // Note
    if (action == CellEditAction::clear && phraseRows[row].note == EMPTY_VALUE_8) {
      // Insert OFF
      phraseRows[row].note = NOTE_OFF;
      phraseRows[row].instrument = EMPTY_VALUE_8;
      phraseRows[row].volume = EMPTY_VALUE_16;
      handled = 1;
    } else if (action == CellEditAction::clear) {
      // Clear note
      handled = edit8withLimit(action, &phraseRows[row].note, &lastNote, chipnomadState->project.pitchTable.octaveSize, chipnomadState->project.pitchTable.length - 1);
      if (handled) phraseRows[row].note = noteLockSnap(phraseRows[row].note, 0);
      edit8withLimit(action, &phraseRows[row].instrument, &lastInstrument, 16, PROJECT_MAX_INSTRUMENTS - 1);
      edit16withLimit(action, &phraseRows[row].volume, &lastVolume, 16, maxVolume);
    } else if (action == CellEditAction::tap && phraseRows[row].note == EMPTY_VALUE_8) {
      phraseRows[row].note = noteLockSnap(lastNote, 0);
      phraseRows[row].instrument = lastInstrument;
      phraseRows[row].volume = lastVolume;
      handled = 1;
    } else if (phraseRows[row].note != NOTE_OFF) {
      handled = edit8withLimit(action, &phraseRows[row].note, &lastNote, chipnomadState->project.pitchTable.octaveSize, chipnomadState->project.pitchTable.length - 1);
      if (handled) {
        phraseRows[row].note = noteLockSnap(phraseRows[row].note, noteLockSnapUp(action));
        if (phraseRows[row].instrument != EMPTY_VALUE_8) lastInstrument = phraseRows[row].instrument;
        if (phraseRows[row].volume != EMPTY_VALUE_16) lastVolume = phraseRows[row].volume;
      }
    }
    if (handled) {
      drawField(1, row, CellState::normal);
      drawField(2, row, CellState::normal);
    }
  } else if (col == 1) {
    // Instrument
    if (action == CellEditAction::doubleTap) {
      uint8_t nextInstrument = findEmptyInstrument(&chipnomadState->project, 0);
      if (nextInstrument != EMPTY_VALUE_8) {
        phraseRows[row].instrument = nextInstrument;
        handled = 1;
      }
    } else {
      handled = edit8withLimit(action, &phraseRows[row].instrument, &lastInstrument, 16, PROJECT_MAX_INSTRUMENTS - 1);
    }
    uint8_t instrument = phraseRows[row].instrument;
    if (handled && instrument != EMPTY_VALUE_8) {
      screenMessage(0, "%s: %s", byteToHex(instrument), instrumentName(&chipnomadState->project, instrument));
    }
  } else if (col == 2) {
    // Volume
    handled = edit16withLimit(action, &phraseRows[row].volume, &lastVolume, 16, maxVolume);
  } else if (col == 3 || col == 5 || col == 7) {
    // FX
    int fxIdx = (col - 3) / 2;
    // Get instrument number from current phrase row or traverse back
    uint8_t instrumentNum = lookupInstrument(&chipnomadState->project, *pSongRow, *pChainRow, row, *pSongTrack);
    int result = editFX(action, phraseRows[row].fx[fxIdx], lastFX, 0, instrumentNum);
    if (result == 2) {
      drawField(col + 1, row, CellState::normal);
      handled = 1;
    } else if (result == 1) {
      isFxEdit = 1;
      handled = 0;
    }
  } else if (col == 4 || col == 6 || col == 8) {
    // FX value
    int fxIdx = (col - 4) / 2;
    if (phraseRows[row].fx[fxIdx][0] != EMPTY_VALUE_8) {
      uint8_t instrumentNum = lookupInstrument(&chipnomadState->project, *pSongRow, *pChainRow, row, *pSongTrack);
      handled = editFXValue(action, phraseRows[row].fx[fxIdx], lastFX, 0, instrumentNum);
    }
  }

  if (handled) triggerRowPreview(screen.cursorRow);

  return handled;
}

static int onEdit(int col, int row, CellEditAction action) {
  int handled = 0;

  int startCol, startRow, endCol, endRow;
  getSelectionBounds(&screen, &startCol, &startRow, &endCol, &endRow);

  if (action == CellEditAction::switchSelection) {
    return switchPhraseSelectionMode(&screen);
  } else if (action == CellEditAction::multiIncrease || action == CellEditAction::multiDecrease) {
    if (!isSingleColumnSelection(&screen)) return 0;
    handled = applyMultiEdit(startCol, startRow, endCol, endRow, action, editCell);
  } else if (action == CellEditAction::multiIncreaseBig || action == CellEditAction::multiDecreaseBig) {
    // Check if full width selection (all columns)
    if (startCol == 0 && endCol == 8) {
      // Rotation mode
      int direction = (action == CellEditAction::multiIncreaseBig) ? -1 : 1;
      applyPhraseRotation(phraseIdx, startRow, endRow, direction);
      fullRedraw();
      handled = 1;
    } else if (isSingleColumnSelection(&screen)) {
      // Single column: big increase/decrease or FX selection
      if (startCol == 3 || startCol == 5 || startCol == 7) {
        // FX type column: show FX selection
        int fxIdx = (startCol - 3) / 2;
        // Get instrument index from current phrase row or traverse back
        uint8_t instrumentNum = lookupInstrument(&chipnomadState->project, *pSongRow, *pChainRow, screen.cursorRow, *pSongTrack);
        fxEditFullDraw(phraseRows[screen.cursorRow].fx[fxIdx][0], instrumentNum, 0);
        isFxEdit = 1;
      } else {
        // Regular big increase/decrease for note, volume, instrument, FX value
        handled = applyMultiEdit(startCol, startRow, endCol, endRow, action, editCell);
      }
    }
  } else if (action == CellEditAction::copy) {
    copyPhrase(phraseIdx, startCol, startRow, endCol, endRow, 0);
    handled = 1;
  } else if (action == CellEditAction::cut) {
    copyPhrase(phraseIdx, startCol, startRow, endCol, endRow, 1);
    handled = 1;
  } else if (action == CellEditAction::paste) {
    const int rowsPasted = pastePhrase(phraseIdx, col, row);
    if (rowsPasted > 0) {
      // Move cursor below pasted data, or to last row if paste extends to end
      int newRow = row + rowsPasted;
      if (newRow > 15) newRow = 15;
      screen.cursorRow = newRow;
    }
    fullRedraw();
    handled = 1;
  } else if (action == CellEditAction::shallowClone) {
    // Handle instrument column cloning
    if (startCol == 1 && endCol == 1) {
      int distinctCount = cloneInstrumentsInPhrase(phraseIdx, startRow, endRow);
      if (distinctCount == 0) {
        screenMessage(MESSAGE_TIME, "No empty instruments");
      }
      screenMessage(MESSAGE_TIME, "Cloned %d instrument%s", distinctCount, distinctCount == 1 ? "" : "s");
      handled = 1;
    }
  } else {
    handled = editCell(col, row, action);
  }

  if (handled) projectModified = 1;
  return handled;
}

static int inputScreenNavigation(int keys, int tapCount) {
  if (keys == (keyRight | keyShift)) {
    // To Instrument/Phrase screen
    int table = -1;
    if (screen.cursorCol > 2) {
      // If we currently on the table command, go to this table
      int fxIdx = (screen.cursorCol - 3) / 2;
      uint8_t fxType = phraseRows[screen.cursorRow].fx[fxIdx][0];
      uint8_t fxValue = phraseRows[screen.cursorRow].fx[fxIdx][1];
      if ((fxType == fxTBL || fxType == fxTBX) && fxValue != 0xff) {
        table = fxValue;
      }
    }

    if (table >= 0) {
      screenSetup(&screenTable, table | 0x1000);
    } else {
      int instrument = 0;
      for (int row = screen.cursorRow; row >= 0; row--) {
        if (phraseRows[row].instrument != EMPTY_VALUE_8) {
          instrument = phraseRows[row].instrument;
          break;
        }
      }
      screenSetup(&screenInstrument, instrument);
    }
    return 1;
  } else if (keys == (keyLeft | keyShift)) {
    // To Chain screen
    screenSetup(&screenChain, -1);
    return 1;
  } else if (keys == (keyUp | keyShift)) {
    // To Groove screen
    int groove = 0;
    if (screen.cursorCol > 2) {
      // If we currently on the groove command, go to this groove
      int fxIdx = (screen.cursorCol - 3) / 2;
      uint8_t fxType = phraseRows[screen.cursorRow].fx[fxIdx][0];
      if (fxType == fxGRV || fxType == fxGGR) {
        groove = phraseRows[screen.cursorRow].fx[fxIdx][1] & (PROJECT_MAX_GROOVES - 1);
      }
    }
    screenSetup(&screenGroove, groove);
    return 1;
  } else if (keys == (keyLeft | keyOpt)) {
    // Previous track
    if (*pSongTrack == 0) return 1;
    uint16_t chain = chipnomadState->project.song[*pSongRow][*pSongTrack - 1];
    if (chain != EMPTY_VALUE_16 && !chainIsEmpty(&chipnomadState->project, chain)) {
      *pSongTrack -= 1;
      while (chipnomadState->project.chains[chain].rows[*pChainRow].phrase == EMPTY_VALUE_16) {
        *pChainRow -= 1;
        if (*pChainRow == 0) break;
      }
      setup(-1);
      fullRedraw();
    }
    return 1;
  } else if (keys == (keyRight | keyOpt)) {
    // Next track
    if (*pSongTrack == chipnomadState->project.tracksCount - 1) return 1;
    uint16_t chain = chipnomadState->project.song[*pSongRow][*pSongTrack + 1];
    if (chain != EMPTY_VALUE_16 && !chainIsEmpty(&chipnomadState->project, chain)) {
      *pSongTrack += 1;
      while (chipnomadState->project.chains[chain].rows[*pChainRow].phrase == EMPTY_VALUE_16) {
        *pChainRow -= 1;
        if (*pChainRow == 0) break;
      }
      setup(-1);
      fullRedraw();
    }
    return 1;
  } else if ((keys == (keyUp | keyOpt)) || (keys == keyUp && screen.cursorRow == 0)) {
    // Previous phrase in the chain
    if (*pChainRow == 0) return 1;
    if (chipnomadState->project.chains[chipnomadState->project.song[*pSongRow][*pSongTrack]].rows[*pChainRow - 1].phrase != EMPTY_VALUE_16) {
      *pChainRow -= 1;
      if (keys == keyUp) screen.cursorRow = 15;
      setup(-1);
      chipnomadQueuePlaybackQueuePhrase(chipnomadState, *pSongTrack, *pSongRow, *pChainRow);
      fullRedraw();
    }
    return 1;
  } else if (keys == (keyDown | keyOpt) || (keys == keyDown && screen.cursorRow == 15)) {
    // Next phrase in the chain
    if (*pChainRow == 15) return 1;
    if (chipnomadState->project.chains[chipnomadState->project.song[*pSongRow][*pSongTrack]].rows[*pChainRow + 1].phrase != EMPTY_VALUE_16) {
      *pChainRow += 1;
      if (keys == keyDown) screen.cursorRow = 0;
      setup(-1);
      chipnomadQueuePlaybackQueuePhrase(chipnomadState, *pSongTrack, *pSongRow, *pChainRow);
      fullRedraw();
    }
    return 1;
  }
  return 0;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (isFxEdit) {
    int fxIdx = (screen.cursorCol - 3) / 2;
    int result = fxEditInput(keys, tapCount, phraseRows[screen.cursorRow].fx[fxIdx], lastFX);
    if (result) {
      isFxEdit = 0;

      // If in selection mode and on FX type column, fill selection with selected FX
      if (screen.selectMode == 1 && (screen.cursorCol == 3 || screen.cursorCol == 5 || screen.cursorCol == 7)) {
        int startCol, startRow, endCol, endRow;
        getSelectionBounds(&screen, &startCol, &startRow, &endCol, &endRow);

        if (isSingleColumnSelection(&screen)) {
          uint8_t selectedFX = phraseRows[screen.cursorRow].fx[fxIdx][0];
          for (int r = startRow; r <= endRow; r++) {
            selectInstrumentFX(phraseRows[r].fx[fxIdx],selectedFX,
              lookupInstrument(&chipnomadState->project,*pSongRow,*pChainRow,r,*pSongTrack));
          }
        }
      }

      fullRedraw();
    }
    return 1;
  }

  // Selection fill & mutate: B+LEFT cycles rhythm patterns, B+RIGHT
  // applies randomized fills, B+UP mutates the placed notes, B+DOWN
  // spreads sliced instruments into chromatic runs and arpeggiates the
  // remaining notes with a random chord (note column only, selection
  // mode only - outside select mode these combos navigate tracks and
  // phrases). Key-ups reset the cycles whenever B is released.
  if (fillInput(isKeyDown, keys)) return 1;

  if (screen.selectMode == 0 && inputScreenNavigation(keys, tapCount)) return 1;
  return screenInput(&screen, isKeyDown, keys, tapCount);
}

static LoopRange getLoopRange(void) {
  LoopRange range = {0};
  if (screen.selectMode == 1) {
    int startCol, startRow, endCol, endRow;
    getSelectionBounds(&screen, &startCol, &startRow, &endCol, &endRow);
    range.enabled = 1;
    range.level = 2;
    range.startSongRow = *pSongRow;
    range.startChainRow = *pChainRow;
    range.startPhraseRow = startRow;
    range.endSongRow = *pSongRow;
    range.endChainRow = *pChainRow;
    range.endPhraseRow = endRow;
  }
  return range;
}

static ScreenPlaybackLevel getPlaybackLevel(void) {
  return ScreenPlaybackLevel::phrase;
}

///////////////////////////////////////////////////////////////////////////////
//
// Key jazz (desktop only): type notes directly on the QWERTY keyboard,
// like m8c (https://github.com/laamaa/m8c). Toggled with Esc. While active,
// this takes over the note keys entirely (they overlap with Edit/Opt/Motion
// on this screen), so Esc again is needed to get those back.
//

#ifdef DESKTOP_BUILD

static int keyJazzEnabled = 0;
static uint8_t keyJazzBaseNote = 48;

static uint8_t keyJazzClampNote(int note) {
  int maxNote = chipnomadState->project.pitchTable.length - 1;
  if (note < 0) return 0;
  if (note > maxNote) return (uint8_t)maxNote;
  return (uint8_t)note;
}

// The selection's column/row bounds if one is active, else the note+
// instrument+volume "bundle" (columns 0-2) at the cursor row - copy/cut
// treat a single note as those 3 columns together, matching how typing
// and Delete/Backspace already fill/clear them as one unit.
static void keyJazzGetActiveRange(int* startCol, int* startRow, int* endCol, int* endRow) {
  if (screen.selectMode) {
    getSelectionBounds(&screen, startCol, startRow, endCol, endRow);
  } else {
    *startCol = 0;
    *endCol = 2;
    *startRow = *endRow = screen.cursorRow;
  }
}

static void keyJazzClearColumn(int row, int col) {
  if (col == 0) phraseRows[row].note = EMPTY_VALUE_8;
  else if (col == 1) phraseRows[row].instrument = EMPTY_VALUE_8;
  else if (col == 2) phraseRows[row].volume = EMPTY_VALUE_16;
  // FX columns are out of scope for key jazz.
}

static void keyJazzSetColumn(int row, int col, uint16_t value) {
  if (col == 0) phraseRows[row].note = (uint8_t)value;
  else if (col == 1) phraseRows[row].instrument = (uint8_t)value;
  else if (col == 2) phraseRows[row].volume = value;
}

static uint16_t keyJazzGetColumn(int row, int col) {
  if (col == 0) return phraseRows[row].note;
  if (col == 1) return phraseRows[row].instrument;
  if (col == 2) return phraseRows[row].volume;
  return EMPTY_VALUE_8;
}

// Removes one column's value at startRow and shifts the rows below it (in
// that same column only) up to fill the gap, clearing the last row.
// Mirrors what Delete does for a whole row, scoped to a single column.
static void keyJazzShiftColumnUp(int col, int startRow, int count) {
  for (int r = startRow; r <= 15 - count; r++) keyJazzSetColumn(r, col, keyJazzGetColumn(r + count, col));
  for (int r = 16 - count; r <= 15; r++) keyJazzClearColumn(r, col);
}

static void keyJazzClearRow(int row, int includeFx) {
  phraseRows[row].note = EMPTY_VALUE_8;
  phraseRows[row].instrument = EMPTY_VALUE_8;
  phraseRows[row].volume = EMPTY_VALUE_16;
  if (includeFx) {
    for (int i = 0; i < 3; i++) {
      phraseRows[row].fx[i][0] = EMPTY_VALUE_8;
      phraseRows[row].fx[i][1] = 0;
    }
  }
}

int phraseKeyJazzHandleRawKey(InputCode input, int isDown) {
  if (input.deviceType != InputDeviceType::keyboard) return 0;

  if (inputIsKeyJazzToggle(input)) {
    if (isDown && !isFxEdit) {
      keyJazzEnabled = !keyJazzEnabled;
      if (keyJazzEnabled) {
        uint8_t currentNote = phraseRows[screen.cursorRow].note;
        uint16_t octaveSize = chipnomadState->project.pitchTable.octaveSize;
        uint8_t reference = (currentNote != EMPTY_VALUE_8 && currentNote != NOTE_OFF) ? currentNote : lastNote;
        keyJazzBaseNote = octaveSize > 0 ? (reference / octaveSize) * octaveSize : reference;
        screenMessage(MESSAGE_TIME, "KEY JAZZ ON (Esc to exit)");
      } else {
        screen.selectMode = 0;
        screenMessage(MESSAGE_TIME, "KEY JAZZ OFF");
      }
      fullRedraw();
    }
    return 1;
  }

  if (!keyJazzEnabled) return 0;

  if (inputIsShiftKey(input)) return 1; // Swallow: see inputIsShiftKey's doc comment

  int arrowDir = inputArrowKeyDirection(input);
  if (arrowDir != 0) {
    if (inputIsShiftHeld()) {
      if (isDown && !screen.selectMode) {
        screen.selectStartRow = screen.cursorRow;
        screen.selectStartCol = screen.cursorCol;
        screen.selectAnchorRow = screen.cursorRow;
        screen.selectAnchorCol = screen.cursorCol;
        screen.selectMode = 1;
      }
    } else if (screen.selectMode) {
      // A plain arrow (Shift released) collapses the selection, like a
      // regular text editor, instead of silently continuing to extend it.
      if (isDown) {
        screen.selectMode = 0;
        fullRedraw();
      }
    }
    return 0; // Let normal cursor movement happen (and extend/render the selection)
  }

  if (inputIsCtrlHeld()) {
    if (inputIsSaveKey(input)) {
      if (isDown) {
        projectSave(&chipnomadState->project, getAutosavePath());
        screenMessage(MESSAGE_TIME, "KEY JAZZ: project saved");
      }
      return 1;
    }
    if (inputIsCopyKey(input) || inputIsCutKey(input)) {
      if (isDown) {
        int startCol, startRow, endCol, endRow;
        keyJazzGetActiveRange(&startCol, &startRow, &endCol, &endRow);
        int isCut = inputIsCutKey(input);
        copyPhrase(phraseIdx, startCol, startRow, endCol, endRow, isCut);
        int count = endRow - startRow + 1;
        screenMessage(MESSAGE_TIME, "KEY JAZZ: %s %d row%s", isCut ? "cut" : "copied", count, count == 1 ? "" : "s");
        if (isCut) {
          screen.selectMode = 0;
          fullRedraw();
        }
      }
      return 1;
    }
    if (inputIsPasteKey(input)) {
      if (isDown) {
        int rowsPasted = pastePhrase(phraseIdx, screen.cursorCol, screen.cursorRow);
        if (rowsPasted > 0) {
          screenMessage(MESSAGE_TIME, "KEY JAZZ: pasted %d row%s", rowsPasted, rowsPasted == 1 ? "" : "s");
          fullRedraw();
        }
      }
      return 1;
    }
    return 0; // Other Ctrl+key combos: not our concern
  }

  if (inputIsDeleteKey(input)) {
    // Delete removes the whole row(s) (every column, not just the
    // selection's columns) and shifts the rest of the phrase up to fill
    // the gap; the cursor stays on the same row index.
    if (isDown) {
      int startRow, endRow;
      if (screen.selectMode) {
        int startCol, endCol;
        getSelectionBounds(&screen, &startCol, &startRow, &endCol, &endRow);
      } else {
        startRow = endRow = screen.cursorRow;
      }
      int count = endRow - startRow + 1;
      for (int r = startRow; r <= 15 - count; r++) phraseRows[r] = phraseRows[r + count];
      for (int r = 16 - count; r <= 15; r++) keyJazzClearRow(r, 1);
      screen.cursorRow = startRow;
      screen.selectMode = 0;
      fullRedraw();
    }
    return 1;
  }

  if (inputIsBackspaceKey(input)) {
    // Backspace is narrower than Delete: it only touches the current
    // column (or the selection's actual columns), not the whole row. It
    // removes the element(s) at the cursor/selection itself (not the row
    // above) and shifts whatever is below, in that same column, up to
    // fill the gap - the column equivalent of what Delete does per row.
    // Like a text editor, it also steps the cursor back one row as it
    // erases (the reverse of typing a note advancing to the next row).
    if (isDown) {
      int startCol, startRow, endCol, endRow;
      if (screen.selectMode) {
        getSelectionBounds(&screen, &startCol, &startRow, &endCol, &endRow);
      } else {
        startCol = endCol = screen.cursorCol;
        startRow = endRow = screen.cursorRow;
      }
      int count = endRow - startRow + 1;
      for (int c = startCol; c <= endCol; c++) keyJazzShiftColumnUp(c, startRow, count);
      screen.cursorRow = startRow > 0 ? startRow - 1 : 0;
      screen.selectMode = 0;
      fullRedraw();
    }
    return 1;
  }

  if (inputIsInsertKey(input)) {
    if (isDown) {
      int row = screen.cursorRow;
      if (row < 15) applyPhraseRotation(phraseIdx, row, 15, 1);
      keyJazzClearRow(row, 1);
      fullRedraw();
    }
    return 1;
  }

  int octaveDelta = inputKeyJazzOctaveDelta(input);
  if (octaveDelta != 0) {
    if (isDown) {
      uint16_t octaveSize = chipnomadState->project.pitchTable.octaveSize;
      keyJazzBaseNote = keyJazzClampNote(keyJazzBaseNote + octaveDelta * (int)octaveSize);
      screenMessage(MESSAGE_TIME, "KEY JAZZ octave: %s", chipnomadState->project.pitchTable.noteNames[keyJazzBaseNote]);
    }
    return 1;
  }

  int offset = inputKeyJazzNoteOffset(input);
  if (offset < 0) return 0; // Not a note key: let normal input handle it (arrows, Shift, Play...)

  if (isDown) {
    int row = screen.cursorRow;
    phraseRows[row].note = noteLockSnap(keyJazzClampNote(keyJazzBaseNote + offset), 0);
    if (phraseRows[row].instrument == EMPTY_VALUE_8) phraseRows[row].instrument = lastInstrument;
    if (phraseRows[row].volume == EMPTY_VALUE_16) phraseRows[row].volume = lastVolume;
    lastNote = phraseRows[row].note;
    triggerRowPreview(row);
    drawField(0, row, CellState::normal);
    drawField(1, row, CellState::normal);
    drawField(2, row, CellState::normal);
    if (row < 15) {
      screen.cursorRow = row + 1;
      fullRedraw();
    }
  }
  return 1;
}

#endif // DESKTOP_BUILD

const AppScreen screenPhrase = {
  .init = init,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = getPlaybackLevel
};

LoopRange phraseScreenGetLoopRange(void) {
  return getLoopRange();
}
