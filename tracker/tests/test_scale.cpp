#include "doctest.h"
#include "chipnomad_lib.h"
#include "playback_internal.h"

#include <cstring>

TEST_SUITE("scale") {

TEST_CASE("preset masks include their expected scale notes") {
  CHECK(scalePresetMask(scaleMajor) == 0x0ab5);
  CHECK(scalePresetMask(scaleMinor) == 0x05ad);
  CHECK(scalePresetMask(scaleWholeTone) == 0x0555);
}

TEST_CASE("quantizer rounds down with root and octave wrap") {
  uint16_t major = scalePresetMask(scaleMajor);
  CHECK(scaleQuantizeNote(3, 0, major, 96) == 2);   // D# -> D in C major
  CHECK(scaleQuantizeNote(3, 2, major, 96) == 2);   // D# -> D in D major
  CHECK(scaleQuantizeNote(1, 1, major, 96) == 1);   // Root is always admitted
  CHECK(scaleQuantizeNote(0, 1, 1u << 11, 96) == 0); // clamp below the first matching note
}

TEST_CASE("snap up raises to the next scale note and never leaves the scale") {
  uint16_t major = scalePresetMask(scaleMajor);
  CHECK(scaleSnapNoteUp(3, 0, major, 96) == 4);    // D# -> E in C major
  CHECK(scaleSnapNoteUp(1, 0, major, 96) == 2);    // C# -> D
  CHECK(scaleSnapNoteUp(2, 0, major, 96) == 2);    // In-scale note stays
  CHECK(scaleSnapNoteUp(94, 0, major, 96) == 95);  // A# -> B at the top
  CHECK(scaleSnapNoteUp(85, 0, 1, 96) == 84);      // Nothing above: settle below
  CHECK(scaleSnapNoteUp(96, 0, major, 96) == 96);  // Out of range stays untouched
}

TEST_CASE("SCL changes runtime scale without changing phrase data") {
  Project project;
  projectInit(&project);
  project.pitchTable.octaveSize = 12;
  project.pitchTable.length = 96;
  project.scaleApply = 1;
  project.scalePreset = scaleChromatic;

  PlaybackState state = {};
  playbackInit(&state, &project);
  state.tracks[0].mode = PlaybackMode::phraseRow;

  PhraseRow row = {};
  row.note = 3; // D#
  row.instrument = EMPTY_VALUE_8;
  row.volume = EMPTY_VALUE_16;
  for (int i = 0; i < 3; ++i) row.fx[i][0] = EMPTY_VALUE_8;
  row.fx[0][0] = fxSCL;
  row.fx[0][1] = 0x10; // Major, C

  readPhraseRowDirect(&state, 0, &row, 0);
  CHECK(row.note == 3);
  CHECK(state.scalePreset == scaleMajor);
  CHECK(state.scaleRoot == 0);
  CHECK(state.tracks[0].note.pitchBase == 2);
}

TEST_CASE("sliced sample notes skip scale quantization") {
  Project project;
  projectInit(&project);
  project.pitchTable.octaveSize = 12;
  project.pitchTable.length = 96;
  project.scaleApply = 1;
  project.scalePreset = scaleMajor;
  getInstrumentFunctions(InstrumentType::Sample).init(&project.instruments[0]);
  project.instruments[0].chip.sample.slice = 8;

  PlaybackState state = {};
  playbackInit(&state, &project);
  state.tracks[0].mode = PlaybackMode::phraseRow;

  PhraseRow row = {};
  row.note = 3;
  row.instrument = 0;
  row.volume = EMPTY_VALUE_16;
  for (int i = 0; i < 3; ++i) row.fx[i][0] = EMPTY_VALUE_8;
  readPhraseRowDirect(&state, 0, &row, 0);
  CHECK(state.tracks[0].note.pitchBase == 3);

  project.instruments[0].chip.sample.slice = 0;
  readPhraseRowDirect(&state, 0, &row, 0);
  CHECK(state.tracks[0].note.pitchBase == 2);
}

TEST_CASE("track scale mask leaves excluded tracks chromatic") {
  Project project;
  projectInit(&project);
  project.pitchTable.octaveSize = 12;
  project.pitchTable.length = 96;
  project.scaleApply = 1;
  project.scaleTracksMask = 0x01;
  project.scalePreset = scaleMajor;
  PlaybackState state = {};
  playbackInit(&state, &project);
  state.tracks[1].mode = PlaybackMode::phraseRow;
  PhraseRow row = {};
  row.note = 3; row.instrument = EMPTY_VALUE_8; row.volume = EMPTY_VALUE_16;
  for (int i = 0; i < 3; ++i) row.fx[i][0] = EMPTY_VALUE_8;
  readPhraseRowDirect(&state, 1, &row, 0);
  CHECK(state.tracks[1].note.pitchBase == 3);
}

TEST_CASE("lowest track SCL command wins during a playback frame") {
  Project project;
  projectInit(&project);
  project.pitchTable.octaveSize = 12;
  project.pitchTable.length = 96;
  PlaybackState state = {};
  playbackInit(&state, &project);
  state.scaleFXCommandSeen = 0;
  state.tracks[0].mode = state.tracks[1].mode = PlaybackMode::phraseRow;

  PhraseRow first = {}, second = {};
  for (int i = 0; i < 3; ++i) { first.fx[i][0] = EMPTY_VALUE_8; second.fx[i][0] = EMPTY_VALUE_8; }
  first.fx[0][0] = fxSCL; first.fx[0][1] = 0x10;
  second.fx[0][0] = fxSCL; second.fx[0][1] = 0x2b;
  readPhraseRowDirect(&state, 0, &first, 0);
  readPhraseRowDirect(&state, 1, &second, 0);
  CHECK(state.scalePreset == scaleMajor);
  CHECK(state.scaleRoot == 0);
}

TEST_CASE("note lock mode bypasses playback quantization of plain notes") {
  Project project;
  projectInit(&project);
  project.pitchTable.octaveSize = 12;
  project.pitchTable.length = 96;
  project.scaleApply = 1;
  project.scalePreset = scaleMajor;
  project.scaleMode = 1;
  PlaybackState state = {};
  playbackInit(&state, &project);
  state.tracks[0].mode = PlaybackMode::phraseRow;

  PhraseRow row = {};
  row.note = 3; // D# would be snapped to D by the quantizer
  row.instrument = EMPTY_VALUE_8;
  row.volume = EMPTY_VALUE_8;
  for (int i = 0; i < 3; ++i) row.fx[i][0] = EMPTY_VALUE_8;
  readPhraseRowDirect(&state, 0, &row, 0);
  CHECK(state.tracks[0].note.pitchBase == 3);
}

TEST_CASE("note lock mode still quantizes CRD chord notes") {
  Project project;
  projectInit(&project);
  project.pitchTable.octaveSize = 12;
  project.pitchTable.length = 96;
  project.scaleApply = 1;
  project.scalePreset = scaleMinor;
  project.scaleMode = 1;

  PlaybackState state = {};
  playbackInit(&state, &project);
  state.tracks[0].mode = PlaybackMode::phraseRow;

  PhraseRow row = {};
  row.note = 36;
  row.instrument = EMPTY_VALUE_8;
  row.volume = EMPTY_VALUE_8;
  for (int i = 0; i < 3; ++i) row.fx[i][0] = EMPTY_VALUE_8;
  row.fx[0][0] = fxCRD;
  row.fx[0][1] = 0x01; // Minor, root position
  readPhraseRowDirect(&state, 0, &row, 0);
  CHECK(state.tracks[0].chordVoiceCount == 3);
  CHECK(state.tracks[0].chordPitchBase[0] == 36);
  CHECK(state.tracks[0].chordPitchBase[1] == 39);
  CHECK(state.tracks[0].chordPitchBase[2] == 43);
}

TEST_CASE("playback start seeds runtime scale from project settings") {
  Project project;
  projectInit(&project);
  project.pitchTable.octaveSize = 12;
  project.pitchTable.length = 96;
  project.scaleRoot = 5;
  project.scalePreset = scaleDorian;
  PlaybackState state = {};
  playbackInit(&state, &project);
  CHECK(state.scaleRoot == 5);
  CHECK(state.scalePreset == scaleDorian);
  project.scaleRoot = 9;
  project.scalePreset = scaleMajor;
  playbackStartSong(&state, 0, 0, 1);
  CHECK(state.scaleRoot == 9);
  CHECK(state.scalePreset == scaleMajor);
}

TEST_CASE("SCL command is inert in note lock mode") {
  Project project;
  projectInit(&project);
  project.pitchTable.octaveSize = 12;
  project.pitchTable.length = 96;
  project.scaleApply = 1;
  project.scaleMode = 1;
  project.scalePreset = scaleChromatic;
  project.scaleRoot = 0;

  PlaybackState state = {};
  playbackInit(&state, &project);
  state.tracks[0].mode = PlaybackMode::phraseRow;

  PhraseRow row = {};
  row.note = 3; // D#
  row.instrument = EMPTY_VALUE_8;
  row.volume = EMPTY_VALUE_8;
  for (int i = 0; i < 3; ++i) row.fx[i][0] = EMPTY_VALUE_8;
  row.fx[0][0] = fxSCL;
  row.fx[0][1] = 0x10; // Major, C

  readPhraseRowDirect(&state, 0, &row, 0);
  CHECK(state.scalePreset == scaleChromatic);
  CHECK(state.scaleRoot == 0);
  CHECK(state.tracks[0].note.pitchBase == 3);
}

} // TEST_SUITE
