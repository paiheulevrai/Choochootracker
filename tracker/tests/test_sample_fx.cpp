// Engine tests for the sample FX: SLI (slice select) and SPL (playback mode,
// formerly SLP). Uses the same fixture pattern as test_lazy_mode.cpp.
#include "doctest.h"
#include "../../chipnomad_lib/playback_internal.h"
#include "../../chipnomad_lib/project_instruments.h"
#include "../../chipnomad_lib/project.h"
#include "../../chipnomad_lib/synth/sample_voice.h"
#include "../../src/project_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

TEST_CASE("long silent looping sample stays silent through RET and phrase wrap") {
  EngineSample es;
  es.init(44100 * 4);
  std::fill(es.data.begin(), es.data.end(), 0);
  es.s.loopMode = 1;
  LazyEngine engine(es.s, 0);
  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxRET;
  row.fx[0][1] = 1;
  for (Project* project : {&engine.state->project, &engine.state->audioProject}) {
    project->song[0][0] = 0;
    project->chains[0].rows[0].phrase = 0;
    project->phrases[0].rows[0] = row;
  }
  chipnomadQueuePlaybackStartPhrase(engine.state, 0, 0, 0, 1);

  std::vector<float> output(44100 * 2 * 2);
  REQUIRE(chipnomadRender(engine.state, output.data(), 44100 * 2) == 44100 * 2);
  float peak = 0.0f;
  for (float value : output) peak = std::max(peak, std::fabs(value));
  CHECK(peak < 1e-5f);
}

TEST_CASE("phrase loop crossfades a long sample at its retrigger") {
  EngineSample es;
  es.init(44100 * 4);
  std::fill(es.data.begin(), es.data.end(), -16000);
  std::fill(es.data.begin(), es.data.begin() + 1000, 16000);
  es.s.loopMode = 1;
  es.s.filterEnabled = 0;
  LazyEngine engine(es.s, 0);
  PhraseRow row = makeRow(48);
  for (Project* project : {&engine.state->project, &engine.state->audioProject}) {
    project->song[0][0] = 0;
    project->chains[0].rows[0].phrase = 0;
    project->phrases[0].rows[0] = row;
  }
  chipnomadQueuePlaybackStartPhrase(engine.state, 0, 0, 0, 1);

  std::vector<float> output(44100 * 2 * 2);
  REQUIRE(chipnomadRender(engine.state, output.data(), 44100 * 2) == 44100 * 2);
  REQUIRE(engine.voice()->active());
  CHECK(engine.voice()->playbackFrame() < 10000);
  float largestStep = 0.0f;
  for (int frame = 84000; frame < 85000; ++frame)
    largestStep = std::max(largestStep, std::fabs(output[frame * 2] - output[(frame - 1) * 2]));
  CHECK(largestStep < 0.03f);
}

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

// --- SPL 02-05 write-through -------------------------------------------------
// SPL 02-05 WRITE THROUGH to the instrument's loopMode on the audio project:
// the instrument view parameter is the central source of truth, so the lane
// FX changes it while the tracker proceeds. 00/01 keep the legacy local
// override and must not clobber the instrument's loop mode.

TEST_CASE("SPL 02-05 write the mapped loop mode into the instrument") {
  EngineSample es;
  es.init(8820);

  // SPL 02 -> loopMode 1 (Loop), 03 -> 3 (Ping), 04 -> 2 (Loop X),
  // 05 -> 4 (Ping X). The write lands on the audio project's instrument.
  const uint8_t splValues[] = {2, 3, 4, 5};
  for (uint8_t spl : splValues) {
    LazyEngine engine(es.s, 0);
    PhraseRow row = makeRow(48);
    row.fx[0][0] = fxSLP;
    row.fx[0][1] = spl;
    chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
    engine.render(882);
    REQUIRE(engine.voice()->active());
    const uint8_t expectedMode[] = {1, 3, 2, 4};
    CHECK(engine.state->audioProject.instruments[0].chip.sample.loopMode == expectedMode[spl - 2]);
  }
}

TEST_CASE("SPL 00 and 01 do not clobber the instrument loop mode") {
  EngineSample es;
  es.init(8820);
  es.s.loopMode = 3; // instrument says ping-pong

  // SPL 00 (forward): legacy local override, instrument field untouched.
  {
    LazyEngine engine(es.s, 0);
    PhraseRow row = makeRow(48);
    row.fx[0][0] = fxSLP;
    row.fx[0][1] = 0;
    chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
    engine.render(882);
    CHECK(engine.state->audioProject.instruments[0].chip.sample.loopMode == 3);
  }

  // SPL 01 (reverse): same - the one-shot preview relies on this.
  {
    LazyEngine engine(es.s, 0);
    PhraseRow row = makeRow(48);
    row.fx[0][0] = fxSLP;
    row.fx[0][1] = 1;
    chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
    engine.render(882);
    CHECK(engine.state->audioProject.instruments[0].chip.sample.loopMode == 3);
  }
}

TEST_CASE("SPL write-through persists across retriggers") {
  // The write-through mutates the audio project's instrument, so a later
  // row without SPL keeps the mode the previous row wrote.
  EngineSample es;
  es.init(8820);
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxSLP;
  row.fx[0][1] = 3; // ping-pong
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  engine.render(882);
  REQUIRE(engine.state->audioProject.instruments[0].chip.sample.loopMode == 3);

  // Retrigger without SPL: the mode written by the previous row persists.
  PhraseRow plain = makeRow(48);
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &plain);
  engine.render(882);
  CHECK(engine.state->audioProject.instruments[0].chip.sample.loopMode == 3);
  CHECK(engine.voice()->active());
}

// --- LOOP START marker -------------------------------------------------------
// loopStart is a 0-255 normalized marker (like start/end). When a loop mode is
// active (instrument loopMode != 0 or SPL >= 02) the voice wraps back to the
// LOOP START frame instead of the Region START frame. With no loop the marker
// is inert and playback stays bit-identical. On the 8820-frame fixture,
// loopStart 0x40 maps to frame 2213 (sampleMarkerToStartFrame).

TEST_CASE("loop start is inert when the loop is off") {
  // Two engines, same sample: one with loopStart 0, one with 0x40, both with
  // loopMode 0. The cursor trajectory must be identical (inert marker).
  EngineSample es;
  es.init(8820);

  std::vector<double> baseline;
  {
    LazyEngine engine(es.s, 0);
    PhraseRow row = makeRow(48);
    chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
    for (int tick = 0; tick < 8; ++tick) {
      engine.render(882);
      baseline.push_back(engine.voice()->playbackFrame());
    }
  }

  es.s.loopStart = 0x40;
  std::vector<double> withMarker;
  {
    LazyEngine engine(es.s, 0);
    PhraseRow row = makeRow(48);
    chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
    for (int tick = 0; tick < 8; ++tick) {
      engine.render(882);
      withMarker.push_back(engine.voice()->playbackFrame());
    }
  }

  REQUIRE(baseline.size() == withMarker.size());
  for (size_t i = 0; i < baseline.size(); ++i) {
    CHECK(baseline[i] == withMarker[i]);
  }
}

TEST_CASE("forward loop wraps to the loop start marker") {
  EngineSample es;
  es.init(8820);
  es.s.loopMode = 1;
  es.s.loopStart = 0x40; // frame 2213 of 8820
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  // One tick = 882 frames at 44100 Hz / 50 Hz. The region is the whole
  // sample, so the first wrap happens during tick 10.
  for (int tick = 0; tick < 9; ++tick) engine.render(882);
  const double before = engine.voice()->playbackFrame();
  CHECK(before > 7000.0); // still in the first pass, near the end
  engine.render(882);
  const double after = engine.voice()->playbackFrame();
  // Wrapped back to the LOOP START frame (2213), not the Region START (0).
  CHECK(after >= 2213.0);
  CHECK(after < 2213.0 + 882.0);
}

TEST_CASE("ping-pong bounces at the loop start marker") {
  EngineSample es;
  es.init(8820);
  es.s.loopMode = 3;
  es.s.loopStart = 0x40;
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  // Play until the cursor has passed the end and reversed: after enough
  // ticks the cursor must bounce at the LOOP START frame, never reaching
  // the region start (0).
  double minSeen = 1e18;
  for (int tick = 0; tick < 40; ++tick) {
    engine.render(882);
    const double frame = engine.voice()->playbackFrame();
    if (tick >= 12) minSeen = frame < minSeen ? frame : minSeen;
  }
  // The loop window is [2213, 8820); the bounce lower bound is 2213.
  CHECK(minSeen >= 2213.0 - 1.0);
  // And the cursor did travel up to the top at least once (sanity).
  CHECK(minSeen < 8820.0);
}

TEST_CASE("region end moved below the loop start clamps it") {
  EngineSample es;
  es.init(8820);
  es.s.loopMode = 1;
  es.s.loopStart = 0x80; // frame 4426
  // Region END at 0x40 puts the region end frame (2239) below the marker
  // frame (4426): the voice must clamp the wrap point into the region.
  es.s.end = 0x40;
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  // The clamped loop window is [0, 2239): the voice must keep looping
  // inside it instead of wrapping to the unclamped marker frame.
  bool wrapped = false;
  for (int tick = 0; tick < 12; ++tick) {
    engine.render(882);
    const double frame = engine.voice()->playbackFrame();
    if (tick > 0 && frame < 2239.0) wrapped = true;
    CHECK(frame < 2239.0);
  }
  CHECK(wrapped);
}

TEST_CASE("SPL 03 ping-pong respects the loop start marker") {
  EngineSample es;
  es.init(8820);
  es.s.loopMode = 0; // instrument loop off; SPL must override
  es.s.loopStart = 0x40;
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxSLP;
  row.fx[0][1] = 3; // ping-pong via SPL
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  double minSeen = 1e18;
  for (int tick = 0; tick < 40; ++tick) {
    engine.render(882);
    const double frame = engine.voice()->playbackFrame();
    if (tick >= 12) minSeen = frame < minSeen ? frame : minSeen;
  }
  // SPL 03 activates the loop, so the bounce lower bound is the marker
  // frame (2213), not the region start (0).
  CHECK(minSeen >= 2213.0 - 1.0);
  CHECK(minSeen < 8820.0);
}

TEST_CASE("sample loop start survives save and load; missing field is zero") {
  Project saved, loaded;
  projectInit(&saved);
  projectInit(&loaded);
  saved.chipsCount = 1;
  saved.tracksCount = 1;
  saved.chipType = ChipType::AY;
  std::strcpy(saved.pitchTable.name, "Test");
  saved.pitchTable.length = 1;
  std::strcpy(saved.pitchTable.noteNames[0], "C-4");
  saved.pitchTable.values[0] = 1000;
  getInstrumentFunctions(InstrumentType::Sample).init(&saved.instruments[0]);
  saved.instruments[0].chip.sample.loopStart = 0x40;
  const char* path = "build/tests/sample_loop_start_io.cct";
  REQUIRE(projectSave(&saved, path) == 0);
  INFO(projectFileError);
  REQUIRE(projectLoad(&loaded, path) == 0);
  CHECK(loaded.instruments[0].type == InstrumentType::Sample);
  CHECK(loaded.instruments[0].chip.sample.loopStart == 0x40);

  // A file without the field loads with loopStart 0 (Region START wrap).
  FILE* in = std::fopen(path, "r");
  REQUIRE(in != nullptr);
  const char* stripped = "build/tests/sample_loop_start_missing.cct";
  FILE* out = std::fopen(stripped, "w");
  REQUIRE(out != nullptr);
  char line[512];
  while (std::fgets(line, sizeof(line), in)) {
    if (std::strncmp(line, "- Sample loop start:", 20) == 0) continue;
    std::fputs(line, out);
  }
  std::fclose(in);
  std::fclose(out);

  Project missing;
  projectInit(&missing);
  REQUIRE(projectLoad(&missing, stripped) == 0);
  CHECK(missing.instruments[0].type == InstrumentType::Sample);
  CHECK(missing.instruments[0].chip.sample.loopStart == 0);
  std::remove(path);
  std::remove(stripped);
}


// --- SPL 04/05: loop X-fade playback modes -----------------------------------
// SPL 04 = forward loop with X-fade, SPL 05 = ping-pong with X-fade. The
// engine maps them to loopMode 1/3 plus the crossfade flag; the fade window
// is 25% of the loop window in source frames. The fixture ramp is the
// worst case: the hard wrap jumps (8819 - 2213) / 32768 = 0.2016 in one
// sample (the wrap target is the loop start marker at frame 2213).

static double xfadeMaxJump(LazyEngine& engine, int ticks) {
  double maxJump = 0.0;
  double prev = 0.0;
  bool first = true;
  for (int tick = 0; tick < ticks; ++tick) {
    std::vector<float> buffer((size_t)882 * 2);
    chipnomadRender(engine.state, buffer.data(), 882);
    for (int i = 0; i < 882; ++i) {
      const double value = buffer[i * 2];
      if (first) { prev = value; first = false; continue; }
      const double jump = std::fabs(value - prev);
      if (jump > maxJump) maxJump = jump;
      prev = value;
    }
  }
  return maxJump;
}

TEST_CASE("SPL 04 crossfades the forward loop wrap") {
  EngineSample es;
  es.init(8820);
  es.s.loopStart = 0x40; // frame 2213
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxSLP;
  row.fx[0][1] = 4; // loop with X-fade
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);

  // The hard wrap (SPL 02) jumps 0.2016 in one sample; the crossfaded
  // render must spread the transition over the 1651-frame window.
  const double fadeJump = xfadeMaxJump(engine, 30);
  CHECK(fadeJump < 0.01);
  CHECK(engine.voice()->active());
}

TEST_CASE("SPL 05 crossfades the ping-pong turns") {
  EngineSample es;
  es.init(8820);
  es.s.loopStart = 0x40;
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxSLP;
  row.fx[0][1] = 5; // ping-pong with X-fade
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);

  // Both turn points (region end and loop start marker) are faded; on the
  // ramp the fold is naturally continuous, so the fade stays small in
  // absolute terms.
  const double fadeJump = xfadeMaxJump(engine, 40);
  CHECK(fadeJump < 0.01);
  CHECK(engine.voice()->active());
}

TEST_CASE("SPL 04 keeps looping where SPL 00 would stop") {
  // SPL 04 must activate the loop like SPL 02 does: the voice stays alive
  // past the window end.
  EngineSample es;
  es.init(8820);
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxSLP;
  row.fx[0][1] = 4;
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  engine.render(882 * 3);
  CHECK(engine.voice()->active());
}

TEST_CASE("SPL mode switch mid-note does not glitch") {
  // Switching SPL 02 -> 03 while the voice plays must not produce a jump
  // larger than the material's own step: the new mode takes over from the
  // current position (the one-shot X-fade between modes is future work;
  // the switch itself must stay click-free on continuous material).
  EngineSample es;
  es.init(8820);
  // Sine material: continuous everywhere, so any wrap glitch is audible
  // in the output as a jump far above the sine's own slope.
  std::vector<int16_t> sine(8820);
  for (uint32_t i = 0; i < 8820; ++i) {
    sine[i] = (int16_t)(std::sin(i * 2.0 * 3.14159265358979 / 441.0) * 20000.0);
  }
  es.data = sine;
  es.s.data = sine.data();
  es.s.loopStart = 0x40;
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxSLP;
  row.fx[0][1] = 2; // loop
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  // Play 12 ticks (into the fade window region), then switch to ping-pong.
  for (int tick = 0; tick < 12; ++tick) engine.render(882);
  PhraseRow switchRow = makeRow(48);
  switchRow.fx[0][0] = fxSLP;
  switchRow.fx[0][1] = 3; // ping-pong
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &switchRow);
  // The voice must survive the switch and keep playing.
  double maxJump = 0.0;
  double prev = 0.0;
  bool first = true;
  for (int tick = 0; tick < 10; ++tick) {
    std::vector<float> buffer((size_t)882 * 2);
    chipnomadRender(engine.state, buffer.data(), 882);
    for (int i = 0; i < 882; ++i) {
      const double value = buffer[i * 2];
      if (first) { prev = value; first = false; continue; }
      const double jump = std::fabs(value - prev);
      if (jump > maxJump) maxJump = jump;
      prev = value;
    }
  }
  CHECK(engine.voice()->active());
  // The sine's own max per-sample slope is 0.6104 * 2*pi/441 = 0.0087
  // (normalized); the observed transition peak is ~0.0101, i.e. within
  // 16% of the material slope - no click. Bound at 2x material slope.
  CHECK(maxJump < 0.02);
}

// --- Instrument loop modes 2/4 (X-fade variants) -----------------------------
// Modes 2 (Loop X) and 4 (Ping X) derive the crossfade from the mode itself:
// the xfade parameter stays 0 (the SPL lane FX path can still force it for
// modes 1/3). The wrap geometry matches the plain modes 1/3.

TEST_CASE("instrument mode 2 crossfades the forward wrap without xfade param") {
  EngineSample es;
  es.init(8820);
  es.s.loopStart = 0x40; // frame 2213
  es.s.loopMode = 2;     // Loop X: xfade derived from the mode
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);

  // Same worst-case ramp as SPL 04: the hard wrap jumps 0.2016 in one
  // sample; the crossfade spreads it over the 1651-frame window.
  const double fadeJump = xfadeMaxJump(engine, 30);
  CHECK(fadeJump < 0.01);
  CHECK(engine.voice()->active());
}

TEST_CASE("instrument mode 4 crossfades the ping-pong turns without xfade param") {
  EngineSample es;
  es.init(8820);
  es.s.loopStart = 0x40;
  es.s.loopMode = 4; // Ping X: xfade derived from the mode
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);

  const double fadeJump = xfadeMaxJump(engine, 40);
  CHECK(fadeJump < 0.01);
  CHECK(engine.voice()->active());
}

TEST_CASE("instrument modes 2 and 4 keep looping where mode 0 stops") {
  EngineSample es;
  es.init(8820);

  for (uint8_t mode : {2, 4}) {
    LazyEngine engine(es.s, 0);
    es.s.loopMode = mode;
    engine.setSampleInstrument(0, es.s);
    PhraseRow row = makeRow(48);
    chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
    engine.render(882 * 3);
    CHECK(engine.voice()->active());
  }
}

TEST_CASE("instrument mode 2 descending hard-wraps like mode 1") {
  // Legacy Start > End windows reverse the playback direction. A descending
  // forward loop (mode 1 or 2) must hard-wrap: the phase-aligned incoming
  // pass would read past the sample data (the wrap target sits at the
  // region end). Only ping-pong (modes 3/4) arms a fade on the way down.
  EngineSample es;
  es.init(8820);
  es.s.start = 200; // descending window: start marker above end marker
  es.s.end = 40;
  es.s.loopStart = 0x40;
  es.s.loopMode = 2;
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);

  // The descending pass must survive the wrap and keep playing (a bad
  // incoming read would produce garbage or kill the voice).
  engine.render(882 * 4);
  CHECK(engine.voice()->active());
  const double frame = engine.voice()->playbackFrame();
  CHECK(frame >= 0.0);
  CHECK(frame < 8820.0);
}

// --- LST: relative loop start FX ---------------------------------------------
// LST shifts the loop start marker relative to its current position: the
// signed value accumulates per row (init) and survives retriggers (no-op
// restart) until the next note trigger. The effective marker is
// clamp(loopStart + acc, 0, 255). On the 8820-frame fixture, marker 0x40
// maps to frame 2213 and one marker unit is (8820-1)/255 = 34.6 frames.

TEST_CASE("LST positive shift moves the loop wrap point") {
  EngineSample es;
  es.init(8820);
  es.s.loopMode = 1;
  es.s.loopStart = 0x40; // frame 2213
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxLST;
  row.fx[0][1] = 0x20; // +32 markers -> 0x60 = frame 3319
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  for (int tick = 0; tick < 9; ++tick) engine.render(882);
  const double before = engine.voice()->playbackFrame();
  CHECK(before > 7000.0); // still in the first pass, near the end
  engine.render(882);
  const double after = engine.voice()->playbackFrame();
  // Wrapped back to the SHIFTED marker frame (3319), not the base 2213.
  CHECK(after >= 3319.0);
  CHECK(after < 3319.0 + 882.0);
}

TEST_CASE("LST negative shift moves the loop wrap point back") {
  EngineSample es;
  es.init(8820);
  es.s.loopMode = 1;
  es.s.loopStart = 0x80; // frame 4426
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxLST;
  row.fx[0][1] = 0xFF; // -1 marker -> 0x7F = frame 4392
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  for (int tick = 0; tick < 9; ++tick) engine.render(882);
  const double before = engine.voice()->playbackFrame();
  CHECK(before > 7000.0); // still in the first pass, near the end
  engine.render(882);
  const double after = engine.voice()->playbackFrame();
  // Wrapped back to the SHIFTED marker frame (4392), not the base 4426.
  CHECK(after >= 4392.0);
  CHECK(after < 4392.0 + 882.0);
}

TEST_CASE("LST accumulates across rows and survives retriggers") {
  EngineSample es;
  es.init(8820);
  es.s.loopMode = 1;
  es.s.loopStart = 0x40; // frame 2213
  LazyEngine engine(es.s, 0);

  // Row 0: LST +0x10. Row 1: LST +0x10 again -> accumulated 0x20.
  PhraseRow row0 = makeRow(48);
  row0.fx[0][0] = fxLST;
  row0.fx[0][1] = 0x10;
  PhraseRow row1 = makeRow(48);
  row1.instrument = EMPTY_VALUE_8;
  row1.fx[0][0] = fxLST;
  row1.fx[0][1] = 0x10;
  PhraseRow retrig = makeRow(48); // no LST: retrigger must keep the offset
  retrig.instrument = EMPTY_VALUE_8;
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row0);
  engine.render(882);
  readPhraseRowDirect(&engine.state->playbackState, 0, &row1, 0);
  engine.render(882);
  // Retrigger without an LST cell: the accumulated offset must persist.
  readPhraseRowDirect(&engine.state->playbackState, 0, &retrig, 0);
  for (int tick = 0; tick < 9; ++tick) engine.render(882);
  const double before = engine.voice()->playbackFrame();
  CHECK(before > 7000.0); // still in the first pass, near the end
  engine.render(882);
  const double after = engine.voice()->playbackFrame();
  // 0x40 + 0x20 = 0x60 markers -> frame 3319.
  CHECK(after >= 3319.0);
  CHECK(after < 3319.0 + 882.0);
}

TEST_CASE("LST clamps the effective marker at 0") {
  EngineSample es;
  es.init(8820);
  es.s.loopMode = 1;
  es.s.loopStart = 0x10; // frame 553
  LazyEngine engine(es.s, 0);

  // A large negative shift clamps at marker 0 -> wrap point = region start.
  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxLST;
  row.fx[0][1] = 0x80; // -128 markers -> clamped to 0
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  for (int tick = 0; tick < 9; ++tick) engine.render(882);
  const double before = engine.voice()->playbackFrame();
  CHECK(before > 7000.0); // still in the first pass, near the end
  engine.render(882);
  const double after = engine.voice()->playbackFrame();
  // Wrapped back to the region start (0), not the base marker frame.
  CHECK(after < 882.0);
}

TEST_CASE("LST accumulates past the marker limits until consumption") {
  EngineSample es;
  es.init(8820);
  es.s.loopMode = 1;
  es.s.loopStart = 0x40;
  LazyEngine engine(es.s, 0);

  PhraseRow row = makeRow(48);
  row.fx[0][0] = fxLST;
  row.fx[0][1] = 0x7F; // +127 -> marker 0xBF
  chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
  engine.render(400);

  row.instrument = EMPTY_VALUE_8;
  row.fx[0][1] = 0x7F; // +127 -> acc 254, consumption clamps the marker at 0xFF
  readPhraseRowDirect(&engine.state->playbackState, 0, &row, 0);
  CHECK(engine.state->playbackState.tracks[0].note.fx[fxLST].acc == 254);

  row.fx[0][1] = 0x81; // -127 -> acc 127, marker lands at 0x80
  readPhraseRowDirect(&engine.state->playbackState, 0, &row, 0);
  CHECK(engine.state->playbackState.tracks[0].note.fx[fxLST].acc == 127);
}

TEST_CASE("LST is inert when the loop is off") {
  // Two engines, same sample: one with LST +0x20, one without any FX, both
  // with loopMode 0. The cursor trajectory must be identical (the shifted
  // marker is ignored in playback without an active loop).
  EngineSample es;
  es.init(8820);

  std::vector<double> baseline;
  {
    LazyEngine engine(es.s, 0);
    PhraseRow row = makeRow(48);
    chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
    for (int tick = 0; tick < 8; ++tick) {
      engine.render(882);
      baseline.push_back(engine.voice()->playbackFrame());
    }
  }

  std::vector<double> withLST;
  {
    LazyEngine engine(es.s, 0);
    PhraseRow row = makeRow(48);
    row.fx[0][0] = fxLST;
    row.fx[0][1] = 0x20;
    chipnomadQueuePlaybackStartPhraseRow(engine.state, 0, &row);
    for (int tick = 0; tick < 8; ++tick) {
      engine.render(882);
      withLST.push_back(engine.voice()->playbackFrame());
    }
  }

  REQUIRE(baseline.size() == withLST.size());
  for (size_t i = 0; i < baseline.size(); ++i) {
    CHECK(baseline[i] == withLST[i]);
  }
}

}  // TEST_SUITE("sample_fx")
