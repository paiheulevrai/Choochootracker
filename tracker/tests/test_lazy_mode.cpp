// Phase 3 (LAZY slice mode) engine-level tests. The UI layer
// (screen_sample_settings.cpp) is not part of the test build; what cannot be
// automated here (marker rendering, key handling, the tap-PLAY/EDIT-drop
// interaction) stays on the manual checklist at the bottom of this file.
//
// The user-facing interaction model (user directive, supersedes plan §6):
//   tap PLAY  -> toggles a full-sample playback that keeps running after the
//                key is released (guarded in app.cpp via
//                sampleLazyPlaybackActive)
//   EDIT click -> drops a slice at the playback marker's position
#include "doctest.h"
#include "../../chipnomad_lib/project_instruments.h"
#include "../../chipnomad_lib/synth/sample_voice.h"
#include "../../src/project_utils.h"

#include <cstring>
#include <cstdlib>
#include <vector>

TEST_SUITE("lazy_mode") {

// --- Sentinel semantics (LAZY acts as sliced since chromatic mapping) ----

TEST_CASE("LAZY sentinel acts as sliced for playback") {
  CHECK(sampleActsAsSliced(nullptr) == 0);
  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.slice = sampleEncodeSlice(sliceModeLazy, 1);
  CHECK(sampleActsAsSliced(&sample) == 1);
  sample.slice = sampleEncodeSlice(sliceModeLazy, 64);
  CHECK(sampleActsAsSliced(&sample) == 1);
  sample.slice = sampleEncodeSlice(sliceModeEqual, 4);
  CHECK(sampleActsAsSliced(&sample) == 1);
  sample.slice = sampleEncodeSlice(sliceModeAuto, 4);
  CHECK(sampleActsAsSliced(&sample) == 1);
  sample.slice = 0;
  CHECK(sampleActsAsSliced(&sample) == 0);
}

TEST_CASE("LAZY sentinel round-trips through encode/decode") {
  for (uint8_t count = 1; count <= 64; ++count) {
    const uint8_t sentinel = sampleEncodeSlice(sliceModeLazy, count);
    CHECK(sentinel == (uint8_t)(128 + count));
    CHECK(sampleDecodeSliceMode(sentinel) == sliceModeLazy);
    CHECK(sampleDecodeSliceCount(sentinel) == count);
  }
  // Reserved range loads as off
  CHECK(sampleDecodeSliceMode(200) == sliceModeOff);
  CHECK(sampleDecodeSliceCount(200) == 0);
}

// --- sampleSliceInitLazy --------------------------------------------------

TEST_CASE("sampleSliceInitLazy clears bounds and sets a single whole-loop slice") {
  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.frameCount = 1000;
  sample.sliceBounds[0] = 100;
  sample.sliceBounds[1] = 500;
  sample.slice = sampleEncodeSlice(sliceModeEqual, 2);

  const uint8_t count = sampleSliceInitLazy(&sample);
  CHECK(count == 1);
  CHECK(sample.slice == sampleEncodeSlice(sliceModeLazy, 1));
  CHECK(sample.sliceBounds[0] == 0);
  CHECK(sample.sliceBounds[1] == 0);
  CHECK(sampleSliceBoundCount(&sample) == 1);
}

// --- sampleSliceInsertAtFrameGapped ---------------------------------------

TEST_CASE("gapped insert places the bound sorted and returns its index") {
  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.frameCount = 10000;
  sample.sampleRate = 1000; // minGap = 50 frames
  sample.start = 0;
  sample.end = 255; // full-sample loop region [0, frameCount)
  sample.slice = sampleEncodeSlice(sliceModeLazy, 1);

  // First drop at frame 5000
  int index = sampleSliceInsertAtFrameGapped(&sample, 5000, 50);
  CHECK(index == 1);
  CHECK(sample.sliceBounds[1] == 5000);
  CHECK(sampleDecodeSliceCount(sample.slice) == 2);

  // Drop before it (frame 1000): sorted insert at index 1, old bound shifts
  index = sampleSliceInsertAtFrameGapped(&sample, 1000, 50);
  CHECK(index == 1);
  CHECK(sample.sliceBounds[1] == 1000);
  CHECK(sample.sliceBounds[2] == 5000);
  CHECK(sampleDecodeSliceCount(sample.slice) == 3);

  // Drop between them (frame 3000): index 2
  index = sampleSliceInsertAtFrameGapped(&sample, 3000, 50);
  CHECK(index == 2);
  CHECK(sample.sliceBounds[1] == 1000);
  CHECK(sample.sliceBounds[2] == 3000);
  CHECK(sample.sliceBounds[3] == 5000);
}

TEST_CASE("gapped insert rejects frames too close to existing bounds") {
  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.frameCount = 10000;
  sample.sampleRate = 1000;
  sample.start = 0;
  sample.end = 255;
  sample.slice = sampleEncodeSlice(sliceModeLazy, 1);
  REQUIRE(sampleSliceInsertAtFrameGapped(&sample, 5000, 50) == 1);

  // 49 frames away: rejected (gap must be >= minGap)
  CHECK(sampleSliceInsertAtFrameGapped(&sample, 5049, 50) == -1);
  CHECK(sampleSliceInsertAtFrameGapped(&sample, 4951, 50) == -1);
  // Exactly 50 frames away: accepted
  CHECK(sampleSliceInsertAtFrameGapped(&sample, 5050, 50) >= 0);
  CHECK(sampleSliceInsertAtFrameGapped(&sample, 4950, 50) >= 0);
  // Duplicate bound: rejected
  CHECK(sampleSliceInsertAtFrameGapped(&sample, 5000, 50) == -1);
}

TEST_CASE("gapped insert rejects frames too close to the loop start") {
  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.frameCount = 10000;
  sample.sampleRate = 1000;
  sample.start = 0;
  sample.end = 255;
  sample.slice = sampleEncodeSlice(sliceModeLazy, 1);

  // Loop start is frame 0; minGap 50 rejects frames 0..49
  CHECK(sampleSliceInsertAtFrameGapped(&sample, 0, 50) == -1);
  CHECK(sampleSliceInsertAtFrameGapped(&sample, 49, 50) == -1);
  CHECK(sampleSliceInsertAtFrameGapped(&sample, 50, 50) >= 0);
}

TEST_CASE("gapped insert respects a non-zero playback start marker") {
  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.frameCount = 10000;
  sample.sampleRate = 1000;
  sample.start = 128; // loop starts at frame 5019 (128*9999/255)
  sample.end = 255;
  sample.slice = sampleEncodeSlice(sliceModeLazy, 1);

  const uint32_t loopStart = sampleMarkerToStartFrame(sample.frameCount, sample.start);
  CHECK(loopStart == 5019);
  // Just inside the loop start: rejected
  CHECK(sampleSliceInsertAtFrameGapped(&sample, loopStart + 49, 50) == -1);
  // Far enough in: accepted
  CHECK(sampleSliceInsertAtFrameGapped(&sample, loopStart + 50, 50) >= 0);
  // Before the loop start: rejected
  CHECK(sampleSliceInsertAtFrameGapped(&sample, 100, 50) == -1);
}

TEST_CASE("gapped insert rejects frames at or past the loop end") {
  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.frameCount = 10000;
  sample.sampleRate = 1000;
  sample.start = 0;
  sample.end = 255;
  sample.slice = sampleEncodeSlice(sliceModeLazy, 1);

  CHECK(sampleSliceInsertAtFrameGapped(&sample, 10000, 50) == -1);
  // The last valid frame is fine (the last slice just gets tiny)
  CHECK(sampleSliceInsertAtFrameGapped(&sample, 9999, 1) >= 0);
}

TEST_CASE("gapped insert rejects the cap and non-sliced samples") {
  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.frameCount = 100000;
  sample.sampleRate = 1000;
  sample.start = 0;
  sample.end = 255;
  sample.slice = sampleEncodeSlice(sliceModeLazy, 1);

  // Fill to the cap: 1 initial + 63 drops
  for (int i = 0; i < 63; ++i) {
    const int index = sampleSliceInsertAtFrameGapped(&sample, (uint32_t)(1000 + i * 1000), 10);
    REQUIRE(index >= 0);
  }
  CHECK(sampleDecodeSliceCount(sample.slice) == 64);
  // One more: rejected
  CHECK(sampleSliceInsertAtFrameGapped(&sample, 99000, 10) == -1);

  // Not sliced: rejected
  InstrumentSample plain;
  std::memset(&plain, 0, sizeof(plain));
  plain.frameCount = 10000;
  CHECK(sampleSliceInsertAtFrameGapped(&plain, 5000, 10) == -1);
}

// --- Engine: LAZY full-sample playback + playbackFrame() ------------------

namespace {

// Ramp sample: data[i] = i, so the playback position can be read back from
// the rendered output (or checked via the voice cursor directly).
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

  void startPreview(uint8_t instrument) {
    // Root note = zero pitch offset (same as the instrument screen preview)
    chipnomadQueuePlaybackPreviewNote(state, 0, state->project.pitchTable.octaveSize * 4, instrument);
  }

  void render(int frames) {
    std::vector<float> buffer((size_t)frames * 2);
    chipnomadRender(state, buffer.data(), frames);
  }
};

}  // namespace

TEST_CASE("LAZY preview maps notes to slices like EQUAL") {
  EngineSample sample;
  sample.init(44100); // 1 second
  // A mid-sample bound that distinguishes the slices
  sample.s.slice = sampleEncodeSlice(sliceModeLazy, 2);
  sample.s.sliceBounds[0] = 0;
  sample.s.sliceBounds[1] = 44100 / 2;

  LazyEngine engine(sample.s, 0);
  // Note 0 (C-0) selects slice 0: LAZY slices map chromatically now.
  chipnomadQueuePlaybackPreviewNote(engine.state, 0, 0, 0);
  engine.render(882);

  const PlaybackStatus* status = chipnomadGetPlaybackStatus(engine.state);
  CHECK(status->tracks[0].mode == PlaybackMode::phraseRow);
  CHECK(status->isPlaying == 1);
  CHECK(status->tracks[0].note.instrument == 0);

  SampleVoice* voice = engine.state->sampleVoices[0][0];
  REQUIRE(voice->active());
  // Slice 0 starts at bound 0, so the cursor sits at the loop start.
  const double pos = voice->playbackFrame();
  CHECK(pos < 1000.0);
  CHECK(pos >= 0.0);
}

TEST_CASE("LAZY full-sample preview (sliceBypass) ignores slice bounds") {
  EngineSample sample;
  sample.init(44100); // 1 second
  sample.s.slice = sampleEncodeSlice(sliceModeLazy, 2);
  sample.s.sliceBounds[0] = 0;
  sample.s.sliceBounds[1] = 44100 / 2;

  LazyEngine engine(sample.s, 0);
  // Note 1 would select slice 1 (bound 22050); the kStartPhraseRowFull
  // bypass must ignore slice mapping and play the whole region instead.
  PhraseRow row;
  std::memset(&row, 0, sizeof(row));
  row.note = engine.state->project.pitchTable.octaveSize * 4 + 1;
  row.instrument = 0;
  row.volume = PHRASE_VOLUME_MAX;
  chipnomadQueuePlaybackStartPhraseRowFull(engine.state, 0, &row);
  engine.render(882);

  const PlaybackStatus* status = chipnomadGetPlaybackStatus(engine.state);
  CHECK(status->tracks[0].mode == PlaybackMode::phraseRow);
  CHECK(status->isPlaying == 1);

  SampleVoice* voice = engine.state->sampleVoices[0][0];
  REQUIRE(voice->active());
  // The bypassed voice starts at the loop start (frame 0), NOT at the
  // slice bound.
  const double pos = voice->playbackFrame();
  CHECK(pos < 1000.0);
  CHECK(pos >= 0.0);
}

TEST_CASE("LAZY full-sample preview plays at original pitch regardless of note") {
  // Regression: the preview row used the first sequencer note mapped to the
  // instrument (e.g. C-0 = 0), and with sliceBypass the engine applied the
  // full note-to-root delta (root = C-4), stretching the sample 4 octaves
  // down. The bypass must ignore the note entirely: the sample plays at its
  // original pitch and speed.
  EngineSample sample;
  sample.init(44100); // 1 second
  sample.s.slice = sampleEncodeSlice(sliceModeLazy, 2);
  sample.s.sliceBounds[0] = 0;
  sample.s.sliceBounds[1] = 44100 / 2;

  // C-0 (note 0) is 4 octaves below the root: without the fix the voice
  // would advance at 1/16 speed and stay near frame 0.
  LazyEngine engine(sample.s, 0);
  PhraseRow row;
  std::memset(&row, 0, sizeof(row));
  row.note = 0;
  row.instrument = 0;
  row.volume = PHRASE_VOLUME_MAX;
  chipnomadQueuePlaybackStartPhraseRowFull(engine.state, 0, &row);
  engine.render(882);

  const PlaybackStatus* status = chipnomadGetPlaybackStatus(engine.state);
  CHECK(status->tracks[0].mode == PlaybackMode::phraseRow);
  CHECK(status->isPlaying == 1);

  SampleVoice* voice = engine.state->sampleVoices[0][0];
  REQUIRE(voice->active());
  // Original speed: one tick (882 frames) advances the cursor by ~882
  // frames. At the buggy 1/16 speed it would sit below ~60.
  const double pos = voice->playbackFrame();
  CHECK(pos > 700.0);
  CHECK(pos < 1100.0);
}

TEST_CASE("notes past the slice count wrap around") {
  // Regression: notes >= sliceCount used to clamp to the last slice, so a
  // phrase playing D-5..A-5 over a 4-slice chop kept retriggering the last
  // slice. The mapping now wraps (note % count).
  EngineSample sample;
  sample.init(44100);
  sample.s.slice = sampleEncodeSlice(sliceModeEqual, 4);
  sample.s.sliceBounds[0] = 0;
  sample.s.sliceBounds[1] = 11025;
  sample.s.sliceBounds[2] = 22050;
  sample.s.sliceBounds[3] = 33075;

  LazyEngine engine(sample.s, 0);
  // Retrigger notes 4..7 back to back: each wraps to slice note % 4. (No
  // StopPreview between notes: the stop render exits before rendering any
  // frames, leaving the next render to burn the pending tick as silence and
  // delaying the next preview command by one render.)
  for (int note = 4; note < 8; ++note) {
    chipnomadQueuePlaybackPreviewNote(engine.state, 0, note, 0);
    engine.render(882);
    SampleVoice* voice = engine.state->sampleVoices[0][0];
    REQUIRE(voice->active());
    const double pos = voice->playbackFrame();
    const double bound = 11025.0 * (note % 4);
    // One tick (882 frames at step 1.0) has played past the slice bound.
    CHECK(pos >= bound);
    CHECK(pos < bound + 2000.0);
  }

  // High keyboard notes (D-5 = 62) also wrap: 62 % 4 = 2
  const int octaveSize = engine.state->project.pitchTable.octaveSize;
  chipnomadQueuePlaybackPreviewNote(engine.state, 0, octaveSize * 5 + 2, 0);
  engine.render(882);
  SampleVoice* voice = engine.state->sampleVoices[0][0];
  REQUIRE(voice->active());
  const double pos = voice->playbackFrame();
  CHECK(pos >= 22050.0);
  CHECK(pos < 24000.0);
}

TEST_CASE("EQUAL preview starts at the selected slice bound") {
  EngineSample sample;
  sample.init(44100);
  sample.s.slice = sampleEncodeSlice(sliceModeEqual, 2);
  sample.s.sliceBounds[0] = 0;
  sample.s.sliceBounds[1] = 44100 / 2;

  LazyEngine engine(sample.s, 0);
  // Note 1 selects slice 1 (pitch index = slice index)
  chipnomadQueuePlaybackPreviewNote(engine.state, 0,
                                    engine.state->project.pitchTable.octaveSize * 4 + 1, 0);
  engine.render(882);

  SampleVoice* voice = engine.state->sampleVoices[0][0];
  REQUIRE(voice->active());
  // EQUAL playback starts at the bound (frame 22050), not the loop start.
  // The first tick renders 882 frames at step 1.0, so the cursor has
  // already advanced past the bound when we read it.
  const double pos = voice->playbackFrame();
  CHECK(pos >= 22050.0);
  CHECK(pos < 24000.0);
}

TEST_CASE("stop preview resets the phrase row and the voice") {
  EngineSample sample;
  sample.init(44100);
  sample.s.slice = sampleEncodeSlice(sliceModeLazy, 1);

  LazyEngine engine(sample.s, 0);
  engine.startPreview(0);
  engine.render(882);
  SampleVoice* voice = engine.state->sampleVoices[0][0];
  REQUIRE(voice->active());

  chipnomadQueuePlaybackStopPreview(engine.state, 0);
  engine.render(882);

  const PlaybackStatus* status = chipnomadGetPlaybackStatus(engine.state);
  CHECK(status->tracks[0].mode != PlaybackMode::phraseRow);
  // The voice is killed on the next update pass
  CHECK_FALSE(voice->active());
}

TEST_CASE("playbackFrame advances during playback") {
  EngineSample sample;
  sample.init(44100);
  sample.s.slice = sampleEncodeSlice(sliceModeLazy, 1);

  LazyEngine engine(sample.s, 0);
  engine.startPreview(0);
  engine.render(882);
  SampleVoice* voice = engine.state->sampleVoices[0][0];
  REQUIRE(voice->active());
  const double early = voice->playbackFrame();
  CHECK(early < 2000.0);

  // Render ~0.25 s more; the cursor must have moved forward
  engine.render(11025);
  const double later = voice->playbackFrame();
  CHECK(later > early);
  CHECK(later > 10000.0);
}

TEST_CASE("granular path reports an advancing playbackFrame") {
  // speedPercent != 100 with speedAlgorithm 0 -> granular path, where the
  // plain position_ cursor is NOT advanced. playbackFrame() must still
  // report the interpolated grain cursor.
  EngineSample sample;
  sample.init(44100);
  sample.s.slice = sampleEncodeSlice(sliceModeLazy, 1);
  sample.s.speedPercent = 200; // double speed

  LazyEngine engine(sample.s, 0);
  engine.startPreview(0);
  engine.render(882);
  SampleVoice* voice = engine.state->sampleVoices[0][0];
  REQUIRE(voice->active());
  const double early = voice->playbackFrame();
  CHECK(early < 4000.0);

  engine.render(11025);
  const double later = voice->playbackFrame();
  CHECK(later > early);
  // Double speed: ~0.5 s of source consumed in 0.25 s
  CHECK(later > 20000.0);
}

TEST_CASE("LAZY preview survives the PLAY release (app-level guard)") {
  // This exercises the app.cpp guard indirectly: the preview must still be
  // running after the "key release" event (keys == 0) that would normally
  // auto-stop a phrase row. The guard itself is UI code; here we verify the
  // engine contract it relies on: a phrase row keeps playing until an
  // explicit stop command arrives.
  EngineSample sample;
  sample.init(44100);
  sample.s.slice = sampleEncodeSlice(sliceModeLazy, 1);

  LazyEngine engine(sample.s, 0);
  engine.startPreview(0);
  engine.render(882);
  CHECK(chipnomadGetPlaybackStatus(engine.state)->tracks[0].mode == PlaybackMode::phraseRow);

  // Simulate several UI ticks with no keys held (the PLAY release moment)
  for (int i = 0; i < 10; ++i) engine.render(882);
  CHECK(chipnomadGetPlaybackStatus(engine.state)->tracks[0].mode == PlaybackMode::phraseRow);
  CHECK(engine.state->sampleVoices[0][0]->active());
}

// --- Manual checklist (UI-only, screen_sample_settings.cpp) ---------------
//
// The following behaviors live in the screen layer and cannot be exercised
// in the test build (screen_sample_settings.cpp is not linked into tests):
//
// 1. Slice row, Mode cell: cycle to LAZY -> single whole-loop slice; with
//    existing EQUAL/AUTO chops they are stashed (no dialog), and cycling
//    back to EQUAL/AUTO restores them verbatim.
// 2. Tap PLAY on the sample screen in LAZY mode: full-sample playback
//    starts and KEEPS RUNNING after the key is released (app.cpp guard:
//    sampleLazyPlaybackActive suppresses the phrase-row auto-stop).
// 3. While the LAZY playback runs, a vertical marker line tracks the
//    playback position across the waveform preview (textValue color).
// 4. Every EDIT click during the playback drops a slice at the marker;
//    the Number cell count grows, the new slice becomes current.
// 5. EDIT clicks within 50 ms of an existing bound (or the loop start)
//    show "Too close to slice" and change nothing.
// 6. EDIT clicks while NOT playing show "Not playing".
// 7. Tap PLAY again: playback stops, the marker disappears, the Frame cell
//    un-dims.
// 8. The Frame cell dims while the playback-drop is armed (drawField dim
//    condition includes sampleLazyPlaybackActive for col 2).
// 9. Leaving the screen stops the preview and resets the flag (setup()
//    clears sampleLazyPlaybackActive).
// 10. PLAY while the song is playing falls through and stops the song
//     (no preview starts).
// 11. Slice markers drawn during LAZY playback reflect the dropped bounds
//     immediately (settingsRepaintSlice after each drop).
// 12. SHIFT + PLAY in LAZY starts phrase playback like on every other
//     screen (the PLAY intercept is an exact key match, so SHIFT+PLAY
//     falls through to the app-level handler).
// 13. EDIT tap on the Number or Frame cell previews the current slice in
//     every slice mode; releasing the key stops it (app.cpp auto-stop),
//     and the preview does not arm the LAZY playback-drop.

}  // TEST_SUITE("lazy_mode")
