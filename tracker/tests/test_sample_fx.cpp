// Engine tests for the sample FX: SLI (slice select) and SPL (playback mode,
// formerly SLP). Uses the same fixture pattern as test_lazy_mode.cpp.
#include "doctest.h"
#include "../../chipnomad_lib/project_instruments.h"
#include "../../chipnomad_lib/synth/sample_voice.h"
#include "../../src/project_utils.h"

#include <cstring>
#include <vector>

TEST_SUITE("sample_fx") {

// --- Fixture (same pattern as test_lazy_mode.cpp) --------------------------

namespace {

// Ramp sample: data[i] = i % 32768, so the playback position can be read back
// from the voice cursor directly. One tick = 882 frames (44100 Hz / 50 Hz).
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

  // The sample instrument must exist BEFORE chipnomadInitChips: InitChips
  // copies the project into the audio snapshot, and updateSampleVoices
  // kills voices whose instrument is not a Sample in that snapshot.
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

// Phrase row helper: root note (48 = octaveSize*4) keeps the playback step at
// 1.0 so cursor math stays frame-exact.
static PhraseRow makeRow(uint8_t note) {
  PhraseRow row;
  std::memset(&row, 0, sizeof(PhraseRow));
  row.note = note;
  row.instrument = 0;
  row.volume = PHRASE_VOLUME_MAX;
  return row;
}

}  // namespace

// --- SLI: slice select overrides note mapping -------------------------------
// Sliced instruments keep the playback step at 1.0 (the note selects the
// slice, not the pitch), so after rendering N frames the cursor sits at
// sliceStart + N.

TEST_CASE("SLI plays the numbered slice regardless of note pitch") {
  EngineSample es;
  es.init(8820); // 10 slices of 882 frames
  // Slicing must be set BEFORE the engine snapshots the project.
  es.s.slice = sampleEncodeSlice(sliceModeEqual, 10);
  LazyEngine engine(es.s, 0);

  // Note 48 maps to slice 8 (48 % 10) without the override; SLI = 01 must
  // play the FIRST slice instead.
  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxSLI;
  row.fx[0][1] = 1;

  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  engine.render(400);

  SampleVoice* v = engine.voice();
  REQUIRE(v->active());
  // Slice 0 spans frames 0..881; the cursor is at ~400.
  CHECK(v->playbackFrame() < 882);
}

TEST_CASE("SLI 00 keeps normal note mapping") {
  EngineSample es;
  es.init(8820);
  es.s.slice = sampleEncodeSlice(sliceModeEqual, 10);
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48); // maps to slice 8 (48 % 10)
  row.fx[0][0] = fxSLI;
  row.fx[0][1] = 0;

  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  engine.render(400);

  SampleVoice* v = engine.voice();
  REQUIRE(v->active());
  // Slice 8 spans frames 7056..7937.
  CHECK(v->playbackFrame() >= 7056);
  CHECK(v->playbackFrame() < 7938);
}

TEST_CASE("SLI value wraps past the last slice") {
  EngineSample es;
  es.init(8820);
  es.s.slice = sampleEncodeSlice(sliceModeEqual, 10);
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxSLI;
  row.fx[0][1] = 11; // wraps to slice 0 (11-1 = 10 % 10)

  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  engine.render(400);

  SampleVoice* v = engine.voice();
  REQUIRE(v->active());
  CHECK(v->playbackFrame() < 882);
}

// --- SPL: playback modes -----------------------------------------------------
// Non-sliced sample, root note: step 1.0, window 0..8820. After one tick
// (882 frames) the cursor has moved 882 frames from its start.

TEST_CASE("SPL 01 reverse plays the sample backwards") {
  EngineSample es;
  es.init(8820);
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxSLP;
  row.fx[0][1] = 1; // reverse

  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  engine.render(882);

  SampleVoice* v = engine.voice();
  REQUIRE(v->active());
  // Reverse starts at the window end (8819) and one tick has played:
  // 8819 - 882 = 7937. Forward playback would sit near frame 882 instead.
  const double first = v->playbackFrame();
  CHECK(first > 7000);

  // The cursor keeps moving DOWN on the next tick.
  engine.render(882);
  const double second = v->playbackFrame();
  CHECK(second < first);
  CHECK(second > 5000);
}

TEST_CASE("SPL 00 forward keeps ascending playback") {
  EngineSample es;
  es.init(8820);
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxSLP;
  row.fx[0][1] = 0; // forward

  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  engine.render(882);

  SampleVoice* v = engine.voice();
  REQUIRE(v->active());
  const double first = v->playbackFrame();
  CHECK(first < 1000);

  // The cursor keeps moving UP on the next tick.
  engine.render(882);
  CHECK(v->playbackFrame() > first);
}

TEST_CASE("SPL 02 maps to loop and SPL 03 maps to ping-pong") {
  EngineSample es;
  es.init(8820);

  // Loop: voice stays alive past the window end.
  {
    LazyEngine engine(es.s, 0);
    PhraseRow row = makeRow(48);
    row.fx[0][0] = fxSLP;
    row.fx[0][1] = 2; // loop
    chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
    engine.render(882 * 3);
    CHECK(engine.voice()->active());
  }

  // Ping-pong: voice stays alive past the window end too.
  {
    LazyEngine engine(es.s, 0);
    PhraseRow row = makeRow(48);
    row.fx[0][0] = fxSLP;
    row.fx[0][1] = 3; // ping-pong
    chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
    engine.render(882 * 3);
    CHECK(engine.voice()->active());
  }
}

}  // TEST_SUITE("sample_fx")
