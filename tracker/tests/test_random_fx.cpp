// Engine tests for the random FX: RNS (random slice, sample FX) and RNN
// (random note, sequencer FX). Uses the same fixture patterns as
// test_sample_fx.cpp (LazyEngine) and test_playback.cpp (PlaybackFixture).
#include "doctest.h"
#include "chipnomad_lib.h"
#include "playback_internal.h"
#include "../../src/project_utils.h"
#include "../../src/pitch_table_utils.h"
#include "../../chipnomad_lib/project_instruments.h"
#include "../../chipnomad_lib/synth/sample_voice.h"

#include <cstring>
#include <vector>
TEST_SUITE("random_fx") {

// --- RNS: engine-level tests through the real sample voice ------------------

namespace {

// Same ramp-sample fixture as test_sample_fx.cpp: data[i] = i % 32768, so the
// playback position can be read back from the voice cursor. One tick = 882
// frames (44100 Hz / 50 Hz).
struct EngineSample {
  InstrumentSample s;
  std::vector<int16_t> data;

  void init(uint32_t frames) {
    std::memset(&s, 0, sizeof(s));
    s.frameCount = frames;
    s.channels = 1;
    s.sampleRate = 44100;
    s.start = 0;
    s.end = 255;
    s.speedPercent = 100;
    s.pitch = 0;
    s.filterEnabled = 1;
    s.filterCharacter = 2;
    s.filterMode = 0;
    s.filterSlope24dB = 0;
    s.filterCutoffHz = FILTER_CUTOFF_MAX_HZ;
    s.filterResonance = 0;
    s.attack = 0;
    s.decay = 0;
    s.sustain = 255;
    s.release = 0;
    s.envelopeShape = 0x80;
    data.resize(frames);
    for (uint32_t i = 0; i < frames; ++i) data[i] = (int16_t)(i % 32768);
    s.data = data.data();
  }
};

struct LazyEngine {
  ChipNomadState* state = chipnomadCreate();

  LazyEngine(const InstrumentSample& sample, uint8_t instrument) {
    projectInitAY(&state->project);
    state->project.trackVolume[0] = 100;
    state->project.trackVolume[1] = 0;
    state->project.trackVolume[2] = 0;
    state->project.trackVolume[3] = 0;
    setSampleInstrument(instrument, sample);
    // The sample data buffer is owned by the fixture (std::vector), not by
    // the project: without this, chipnomadDestroy -> projectFree frees the
    // shared data pointer and the vector destructor frees it again.
    state->ownsProjectResources = 0;
    chipnomadInitChips(state, 44100, nullptr);
  }
  ~LazyEngine() { chipnomadDestroy(state); }

  void setSampleInstrument(uint8_t instrument, const InstrumentSample& sample) {
    Instrument& inst = state->project.instruments[instrument];
    getInstrumentFunctions(InstrumentType::Sample).init(&inst);
    inst.volume = 255;
    inst.chip.sample = sample; // copies the struct; data pointer shared
  }

  void render(int frames) {
    std::vector<float> buffer((size_t)frames * 2);
    chipnomadRender(state, buffer.data(), frames);
  }

  SampleVoice* voice() { return state->sampleVoices[0][0]; }
};

static PhraseRow makeRow(uint8_t note) {
  PhraseRow row;
  std::memset(&row, 0, sizeof(PhraseRow));
  row.note = note;
  row.instrument = 0;
  row.volume = PHRASE_VOLUME_MAX;
  return row;
}

}  // namespace

// utilsRandom() is a process-global LCG: its state depends on every draw
// consumed anywhere in the test binary before the current test case, so
// exact draw values are not predictable here. Every assertion below
// therefore holds for ANY draw: range bounds, kept-state equality, and
// disjoint-range tricks that prove a re-pick happened no matter what was
// drawn.
TEST_CASE("RNS mode 0 picks a random slice instead of the note mapping") {
  EngineSample es;
  es.init(8820); // 10 slices of 882 frames
  es.s.slice = sampleEncodeSlice(sliceModeEqual, 10);
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48); // without RNS: slice 8 (48 % 10)
  row.fx[0][0] = fxRNS;
  row.fx[0][1] = 0x00; // mode 0: always random

  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  engine.render(400); // one tick from counter 0: triggers the row

  auto& track = engine.state->playbackState.tracks[0];
  SampleVoice* v = engine.voice();
  REQUIRE(v->active());
  // The pick went through the RNS path and the voice follows it, whatever
  // the draw was.
  CHECK(track.rnsSeeded == 1);
  uint8_t firstSlice = track.rnsSlice;
  CHECK(firstSlice < 10);
  CHECK((uint32_t)v->playbackFrame() >= firstSlice * 882);
  CHECK((uint32_t)v->playbackFrame() < (firstSlice + 1) * 882);

  // Re-trigger with a different value: mode 0 re-picks regardless of the
  // value, and rnsLastValue only changes inside the pick branch, so it
  // proves the second trigger picked again.
  row.fx[0][1] = 0x01;
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  engine.render(1000); // crosses a tick boundary from counter 482
  CHECK(track.rnsLastValue == 0x01);
  CHECK(track.rnsSlice < 10);
  CHECK((uint32_t)v->playbackFrame() >= track.rnsSlice * 882);
  CHECK((uint32_t)v->playbackFrame() < (track.rnsSlice + 1) * 882);
}

TEST_CASE("RNS mode 1 keeps one slice for the whole PLAY") {
  EngineSample es;
  es.init(8820);
  es.s.slice = sampleEncodeSlice(sliceModeEqual, 10);
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxRNS;
  row.fx[0][1] = 0x10; // mode 1: random per PLAY

  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  engine.render(400); // one tick from counter 0: triggers the row
  auto& track = engine.state->playbackState.tracks[0];
  SampleVoice* v = engine.voice();
  REQUIRE(v->active());
  CHECK(track.rnsLastValue == 0x10); // the pick branch ran
  uint8_t firstSlice = track.rnsSlice;
  CHECK(firstSlice < 10);
  CHECK((uint32_t)v->playbackFrame() >= firstSlice * 882);
  CHECK((uint32_t)v->playbackFrame() < (firstSlice + 1) * 882);

  // Further rows within the same PLAY keep the slice. A queued re-trigger
  // would start a NEW PLAY (playbackStartPhraseRow resets the track and
  // reseeds), so rows are fed directly instead. Five repeats make a
  // vacuous pass (every re-pick landing on the same slice) practically
  // impossible.
  for (int i = 0; i < 5; ++i) {
    readPhraseRowDirect(&engine.state->playbackState, 0, &row, 0);
    CHECK(track.rnsSlice == firstSlice);
  }

  // A re-trigger starts a new PLAY and picks fresh. The row value changes
  // to 0x11 (the second digit is unused in RNS), and rnsLastValue only
  // updates inside the pick branch, so the new value proves the re-pick
  // ran whatever the draw was. render(1000) crosses one tick boundary
  // from counter 482.
  row.fx[0][1] = 0x11;
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  engine.render(1000);
  v = engine.voice();
  REQUIRE(v->active());
  CHECK(track.rnsLastValue == 0x11);
  CHECK(track.rnsSlice < 10);
  CHECK((uint32_t)v->playbackFrame() >= track.rnsSlice * 882);
  CHECK((uint32_t)v->playbackFrame() < (track.rnsSlice + 1) * 882);
}

TEST_CASE("RNS mode 2 re-picks when the FX value changes") {
  EngineSample es;
  es.init(8820);
  es.s.slice = sampleEncodeSlice(sliceModeEqual, 10);
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxRNS;
  row.fx[0][1] = 0x20; // mode 2: pick and keep

  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  engine.render(400); // one tick from counter 0: triggers the row
  auto& track = engine.state->playbackState.tracks[0];
  SampleVoice* v = engine.voice();
  REQUIRE(v->active());
  CHECK(track.rnsLastValue == 0x20); // the pick branch ran
  uint8_t firstSlice = track.rnsSlice;
  CHECK(firstSlice < 10);
  CHECK((uint32_t)v->playbackFrame() >= firstSlice * 882);
  CHECK((uint32_t)v->playbackFrame() < (firstSlice + 1) * 882);

  // Rows with the same value keep the pick (fed directly: a queued
  // re-trigger would start a new PLAY and reseed). Five repeats make a
  // vacuous pass practically impossible.
  for (int i = 0; i < 5; ++i) {
    readPhraseRowDirect(&engine.state->playbackState, 0, &row, 0);
    CHECK(track.rnsSlice == firstSlice);
  }

  // A changed value re-picks within the same PLAY. rnsLastValue only
  // updates inside the pick branch, so the new value proves the re-pick
  // ran whatever the draw was.
  row.fx[0][1] = 0x21;
  readPhraseRowDirect(&engine.state->playbackState, 0, &row, 0);
  CHECK(track.rnsLastValue == 0x21);
  CHECK(track.rnsSlice < 10);
}

TEST_CASE("RNS without slicing leaves the note mapping alone") {
  EngineSample es;
  es.init(8820);
  es.s.slice = 0; // slicing off
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxRNS;
  row.fx[0][1] = 0x00;

  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  engine.render(400);

  auto& track = engine.state->playbackState.tracks[0];
  SampleVoice* v = engine.voice();
  REQUIRE(v->active());
  // No slices: the whole sample plays from the start regardless of RNS,
  // and the pick branch never runs at all.
  CHECK(track.rnsSeeded == 0);
  CHECK(v->playbackFrame() < 882);
}

// --- RNN: sequencer-level tests through readPhraseRowDirect -----------------

// Major scale on C: pitch classes {0,2,4,5,7,9,11}. doctest forbids ||
// chains inside CHECK, so the membership test lives in a plain function.
static bool inMajorScale(uint8_t note) {
  switch (note % 12) {
    case 0: case 2: case 4: case 5: case 7: case 9: case 11: return true;
    default: return false;
  }
}

struct RandomFxPlaybackFixture {
  ChipNomadState* state;

  RandomFxPlaybackFixture() {
    state = chipnomadCreate();
    Project* p = &state->project;
    p->tickRate = 50;
    p->chipType = ChipType::AY;
    p->chipsCount = 1;
    p->chipSetup.ay = (ChipSetupAY){ .clock = 1773400, .isYM = 0,
      .stereoMode = StereoModeAY::ABC, .stereoSeparation = 50, .pwmFullRange = 0 };
    p->tracksCount = projectGetTotalTracks(p);
    p->linearPitch = 0;
    calculatePitchTableAY(p);
    chipnomadInitChips(state, 44100, nullptr);
    playbackInit(&state->playbackState, p);
  }
  ~RandomFxPlaybackFixture() { chipnomadDestroy(state); }
};

static PhraseRow makeFxRow(uint8_t note, FXId fx, uint8_t value) {
  PhraseRow row;
  std::memset(&row, 0, sizeof(PhraseRow));
  row.note = note;
  row.instrument = 0;
  row.volume = PHRASE_VOLUME_MAX;
  row.fx[0][0] = fx;
  row.fx[0][1] = value;
  return row;
}

TEST_CASE_FIXTURE(RandomFxPlaybackFixture, "RNN range 0 picks any note of the pitch table") {
  PhraseRow row = makeFxRow(48, fxRNN, 0x00);
  readPhraseRowDirect(&state->playbackState, 0, &row, 0);
  auto& track = state->playbackState.tracks[0];
  CHECK(track.rnnSeeded == 1); // the pick branch ran
  // Full range: the pick spans the whole pitch table, unanchored to the
  // written note. The default chromatic project scale leaves it as is.
  CHECK(track.rnnPickedNote < state->project.pitchTable.length);
  CHECK(track.note.pitchBase == track.rnnPickedNote);
}

TEST_CASE_FIXTURE(RandomFxPlaybackFixture, "RNN range 0 snaps the pick to the project scale") {
  state->playbackState.scalePreset = scaleMajor;
  state->playbackState.scaleRoot = 0;
  PhraseRow row = makeFxRow(48, fxRNN, 0x00);
  readPhraseRowDirect(&state->playbackState, 0, &row, 0);
  auto& track = state->playbackState.tracks[0];
  CHECK(track.rnnSeeded == 1);
  // Quantizer mode snaps the picked note down to the major scale.
  CHECK(track.note.pitchBase <= track.rnnPickedNote);
  CHECK(inMajorScale(track.note.pitchBase));
}

TEST_CASE_FIXTURE(RandomFxPlaybackFixture, "RNN mode 0 re-picks every row") {
  // Draw-independent re-pick proof: the second row sits an octave and a
  // half higher, so a fresh pick from 84 lands in [72, 96] while a kept
  // first pick from 48 stays in [36, 60]. The ranges cannot overlap.
  PhraseRow row = makeFxRow(48, fxRNN, 0x09); // mode 0, +/-12 st
  readPhraseRowDirect(&state->playbackState, 0, &row, 0);
  auto& track = state->playbackState.tracks[0];
  CHECK(track.rnnSeeded == 1);
  uint8_t first = track.note.pitchBase;
  CHECK(first == track.rnnPickedNote);
  CHECK(first >= 48 - 12);
  CHECK(first <= 48 + 12);

  PhraseRow higher = makeFxRow(84, fxRNN, 0x09);
  readPhraseRowDirect(&state->playbackState, 0, &higher, 0);
  uint8_t second = track.note.pitchBase;
  CHECK(second == track.rnnPickedNote);
  CHECK(second >= 84 - 12); // fresh pick from the new note
  CHECK(second <= 84 + 12);
}

TEST_CASE_FIXTURE(RandomFxPlaybackFixture, "RNN mode 1 keeps one note per PLAY") {
  PhraseRow row = makeFxRow(48, fxRNN, 0x19); // mode 1, +/-12 st
  readPhraseRowDirect(&state->playbackState, 0, &row, 0);
  auto& track = state->playbackState.tracks[0];
  uint8_t first = track.note.pitchBase;
  CHECK(first >= 48 - 12);
  CHECK(first <= 48 + 12);

  // A different note with the same value must NOT re-pick: mode 1 holds
  // until the track restarts. A re-pick from 84 would land in [72, 96],
  // which cannot equal the first pick from 48.
  PhraseRow higher = makeFxRow(84, fxRNN, 0x19);
  readPhraseRowDirect(&state->playbackState, 0, &higher, 0);
  CHECK(track.note.pitchBase == first);
}

TEST_CASE_FIXTURE(RandomFxPlaybackFixture, "RNN mode 2 re-picks on value change") {
  PhraseRow row = makeFxRow(48, fxRNN, 0x29); // mode 2, +/-12 st
  readPhraseRowDirect(&state->playbackState, 0, &row, 0);
  auto& track = state->playbackState.tracks[0];
  CHECK(track.rnnLastValue == 0x29); // pick branch ran
  uint8_t first = track.note.pitchBase;
  CHECK(first >= 48 - 12);
  CHECK(first <= 48 + 12);

  // Same value, different note: pick is kept. A re-pick from 84 would
  // land in [72, 96], which cannot equal the first pick from 48.
  PhraseRow higher = makeFxRow(84, fxRNN, 0x29);
  readPhraseRowDirect(&state->playbackState, 0, &higher, 0);
  CHECK(track.note.pitchBase == first);

  // Changed value: new pick from the current row note, +/-6 st -> [78, 90].
  // rnnLastValue only changes inside the pick branch.
  PhraseRow changed = makeFxRow(84, fxRNN, 0x28);
  readPhraseRowDirect(&state->playbackState, 0, &changed, 0);
  CHECK(track.rnnLastValue == 0x28);
  CHECK(track.note.pitchBase >= 84 - 6);
  CHECK(track.note.pitchBase <= 84 + 6);
}

TEST_CASE_FIXTURE(RandomFxPlaybackFixture, "RNN one-sided ranges stay in bounds") {
  auto& track = state->playbackState.tracks[0];
  // Range 3 = +12 st: pick is in [note, note+12].
  PhraseRow up = makeFxRow(48, fxRNN, 0x03);
  readPhraseRowDirect(&state->playbackState, 0, &up, 0);
  CHECK(track.rnnSeeded == 1);
  uint8_t pickedUp = track.note.pitchBase;
  CHECK(track.rnnPickedNote == pickedUp);
  CHECK(pickedUp >= 48);
  CHECK(pickedUp <= 60);

  // Range 6 = -12 st: pick is in [note-12, note].
  PhraseRow down = makeFxRow(48, fxRNN, 0x06);
  readPhraseRowDirect(&state->playbackState, 0, &down, 0);
  uint8_t pickedDown = track.note.pitchBase;
  CHECK(track.rnnPickedNote == pickedDown);
  CHECK(pickedDown >= 36);
  CHECK(pickedDown <= 48);
}

TEST_CASE_FIXTURE(RandomFxPlaybackFixture, "RNN snaps to the scale in Note Lock") {
  // readPhraseRowDirect reads mode/apply/track mask from the project the
  // playback state points at, but the preset and root live in the
  // playback state's own copies, so both must be set.
  Project* p = &state->project;
  p->scaleMode = 1; // Note Lock
  p->scaleApply = 1;
  p->scaleTracksMask = 0x01;
  state->playbackState.scalePreset = scaleMajor;
  state->playbackState.scaleRoot = 0;

  PhraseRow row = makeFxRow(49, fxRNN, 0x09); // +/-12 st, C# is off-scale
  readPhraseRowDirect(&state->playbackState, 0, &row, 0);
  auto& track = state->playbackState.tracks[0];
  CHECK(track.rnnSeeded == 1);
  // The raw pick stays within +/-12 of the written note; the snapped
  // result is in the scale and never above the raw pick (snapping scans
  // downward).
  CHECK(track.rnnPickedNote >= 49 - 12);
  CHECK(track.rnnPickedNote <= 49 + 12);
  CHECK(track.note.pitchBase <= track.rnnPickedNote);
  // Major scale on C: pitch classes {0,2,4,5,7,9,11}.
  CHECK(inMajorScale(track.note.pitchBase));
}

TEST_CASE_FIXTURE(RandomFxPlaybackFixture, "RNN quantizer mode snaps to the scale") {
  // Quantizer mode (scaleMode 0) is the project default; the quantizer
  // reads the preset from the playback state's own copy.
  Project* p = &state->project;
  p->scaleMode = 0; // quantizer
  p->scaleApply = 1;
  p->scaleTracksMask = 0x01;
  state->playbackState.scalePreset = scaleMajor;
  state->playbackState.scaleRoot = 0;

  PhraseRow row = makeFxRow(49, fxRNN, 0x09); // +/-12 st, C# is off-scale
  readPhraseRowDirect(&state->playbackState, 0, &row, 0);
  auto& track = state->playbackState.tracks[0];
  CHECK(track.rnnSeeded == 1);
  CHECK(track.note.pitchBase <= track.rnnPickedNote);
  // Major scale on C: pitch classes {0,2,4,5,7,9,11}.
  CHECK(inMajorScale(track.note.pitchBase));
}

// --- ID stability: FX enum values must never shift (song data compat) -------

TEST_CASE("RNS/RNN keep their FX IDs") {
  CHECK((int)fxRNS == 260);
  CHECK((int)fxRNN == 261);
  CHECK((int)fxLST == 262);
  CHECK((int)fxTotalCount == 263);
}

TEST_CASE("RNS/RNN resolve through name-based serialization") {
  fillFXNames();
  CHECK(std::strcmp(fxNames[fxRNS].name, "RNS") == 0);
  CHECK(std::strcmp(fxNames[fxRNN].name, "RNN") == 0);
}

}  // TEST_SUITE
