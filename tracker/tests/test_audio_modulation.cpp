#include "doctest.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "project_utils.h"
#include "synth/master_effects.h"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
#include <string>

namespace {
struct AudioModFixture {
  std::vector<int16_t> source = std::vector<int16_t>(48000, 8192);
  std::vector<int16_t> target = std::vector<int16_t>(48000, 12288);
  std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)> state{chipnomadCreate(), chipnomadDestroy};

  AudioModFixture(bool enabled, bool layers = false, bool eq = false) {
    auto& p = state->project;
    p.tracksCount = layers ? 8 : 2;
    p.chipsCount = 1;
    p.linearPitch = 1;
    p.tickRate = 50;
    calculateLinearPitchTable12TET(&p);
    p.reverbReturn = p.delayReturn = 0;
    for (int i = 0; i < 2; ++i) {
      auto& inst = p.instruments[i];
      getInstrumentFunctions(InstrumentType::Sample).init(&inst);
      inst.volume = 255;
      auto& s = inst.chip.sample;
      s.data = i ? target.data() : source.data();
      s.frameCount = 48000; s.channels = 1; s.sampleRate = 48000;
      s.start = 0; s.end = 255; s.speedPercent = 100;
      s.filterEnabled = 0;
      s.attack = s.decay = s.release = 0; s.sustain = 255;
      s.loopMode = 1;
    }
    if (layers) {
      auto& layer = p.instruments[2];
      getInstrumentFunctions(InstrumentType::Layer).init(&layer);
      layer.volume = 255;
      for (auto& entry : layer.chip.layer.entries) {
        entry.instrument = 1; entry.volume = 255; entry.transpose = 128;
      }
    }
    if (eq) insertSelect(&p.trackInserts[1][0], insertParametricEQ);
    if (enabled) {
      auto& mod = p.instruments[1].modulation[0];
      mod = {ModulationType::Audio, 1, -127, 0, 0, 80, 64, 0};
      if (eq) mod.destination = getInstrumentFunctions(InstrumentType::Sample).modDestinationsCount +
        1 + genericModFirstInsert + 2;
    }
    for (int t = 0; t < p.tracksCount; ++t) {
      p.trackVolume[t] = 100;
      p.trackReverbSend[t] = p.trackDelaySend[t] = 0;
    }
    state->ownsProjectResources = 0;
    chipnomadInitChips(state.get(), 48000, nullptr);
    REQUIRE(chipnomadReserveRenderBuffers(state.get(), 512) == 0);
    for (int t = 0; t < p.tracksCount; ++t)
      REQUIRE(chipnomadQueuePlaybackPreviewNote(state.get(), t, 45, t == 0 ? 0 : layers ? 2 : 1));
  }

  void render(float* output, int frames) {
    REQUIRE(chipnomadRender(state.get(), output, frames) == frames);
  }
};
}

TEST_CASE("Audio follower has sample-rate independent attack and release time constants") {
  Modulation mod{ModulationType::Audio, 1, 127, 0, 255, 255, 64, 0};
  PlaybackModState state;
  playbackModInit(&state, &mod);
  for (int i = 0; i < 480; ++i) playbackModNextAudioEnvelope(&state, 48000, 100, 1);
  CHECK(state.audio.level == doctest::Approx(1 - std::exp(-1.0)).epsilon(.0001));
  for (int i = 0; i < 441; ++i) playbackModNextAudioEnvelope(&state, 44100, 100, 0);
  CHECK(state.audio.level == doctest::Approx((1 - std::exp(-1.0)) * std::exp(-1.0)).epsilon(.0001));
  CHECK(state.audio.sampleRate == 44100);
}

TEST_CASE("Audio follower scales sensitivity, signed depth and live offsets") {
  Modulation mod{ModulationType::Audio, 1, 127, 0, 0, 0, 128, 0};
  PlaybackModState state;
  playbackModInit(&state, &mod);
  playbackModNextAudioEnvelope(&state, 48000, 64, .25f);
  CHECK(state.audio.level == .5f);
  CHECK(state.outValue == 16192);
  state.amountOffset = -254;
  playbackModNextAudioEnvelope(&state, 48000, 64, .25f);
  CHECK(state.outValue == -16192);
  state.p4Offset = 127;
  playbackModNextAudioEnvelope(&state, 48000, 64, 1);
  CHECK(state.audio.level == 1);
  CHECK(state.outValue == -32385);
  playbackModNext(&state);
  CHECK(state.outValue == -32385);
  playbackModNoteOff(&state);
  CHECK(state.outValue == -32385);
  playbackModNextAudioEnvelope(&state, 48000, 64, 0);
  CHECK(state.outValue == 0);
  CHECK(modulationIsAdditive(ModulationType::Audio));
}

TEST_CASE("Audio modulation taps an audible internal track without an Audio Input voice") {
  AudioModFixture dry(false), wet(true);
  float dryOutput[1024], wetOutput[1024];
  dry.render(dryOutput, 512);
  wet.render(wetOutput, 512);
  auto* s = wet.state.get();
  CHECK(s->audioModTrackMask == 1);
  CHECK(s->audioInputRoutedTrackMask == 0);
  const auto& mod = s->playbackState.tracks[1].note.modulation[0];
  CHECK(mod.outValue < 0);
  CHECK(wetOutput[1000] < dryOutput[1000]);
  CHECK(wetOutput[1000] > 0);
  for (auto& owner : s->audioInputVoices)
    for (auto* voice : owner) CHECK(voice == nullptr);
  auto& changed = s->project.instruments[1].modulation[0];
  changed.p1 = 1;
  REQUIRE(chipnomadQueueProjectRefresh(s));
  wet.render(wetOutput, 512);
  wet.render(wetOutput, 512);
  CHECK(s->audioModTrackMask == 2);
  changed.destination = 0;
  REQUIRE(chipnomadQueueProjectRefresh(s));
  wet.render(wetOutput, 512);
  wet.render(wetOutput, 512);
  CHECK(s->audioModTrackMask == 0);
}

TEST_CASE("Audio source-only tapping is bit-identical to the unmodulated audible mix") {
  AudioModFixture dry(false), tapped(true);
  tapped.state->project.instruments[1].modulation[0].p4 = 0;
  REQUIRE(chipnomadQueueProjectRefresh(tapped.state.get()));
  float baseline[1024], output[1024];
  dry.render(baseline, 512); tapped.render(output, 512);
  for (int i = 0; i < 1024; ++i) CHECK(output[i] == baseline[i]);
  CHECK(tapped.state->audioModTrackMask == 1);
}

TEST_CASE("Muted Audio modulation sources keep their processed signal but not their return or sends") {
  for (int sourceKind = 0; sourceKind < 4; ++sourceKind) {
    CAPTURE(sourceKind);
    AudioModFixture audible(true), muted(true);
    for (auto* fixture : {&audible, &muted}) {
      auto& p = fixture->state->project;
      if (sourceKind == 1) {
        getInstrumentFunctions(InstrumentType::Monk).init(&p.instruments[0]);
        p.instruments[0].chip.monk.vowel = 208;
        p.instruments[0].chip.monk.character = 160;
      } else if (sourceKind == 2) {
        getInstrumentFunctions(InstrumentType::DigiSophie).init(&p.instruments[0]);
      } else if (sourceKind == 3) {
        p.instruments[3] = p.instruments[0];
        getInstrumentFunctions(InstrumentType::Layer).init(&p.instruments[0]);
        p.instruments[0].chip.layer.entries[0] = {3, 128, 255};
      }
      insertSelect(&p.trackInserts[0][0], insertParametricEQ);
      p.trackDelaySend[0] = p.trackReverbSend[0] = 100;
      chipnomadInitChips(fixture->state.get(), 48000, nullptr);
      REQUIRE(chipnomadQueuePlaybackPreviewNote(fixture->state.get(), 0, 24, 0));
      REQUIRE(chipnomadQueuePlaybackPreviewNote(fixture->state.get(), 1, 36, 1));
    }
    uint8_t enabled[PROJECT_MAX_TRACKS] = {};
    enabled[1] = 1;
    REQUIRE(chipnomadQueueTrackEnabled(muted.state.get(), enabled));
    float a[1024], m[1024];
    for (int n = 0; n < 6; ++n) {
      audible.render(a, 512); muted.render(m, 512);
      CHECK(audible.state->playbackState.tracks[1].note.modulation[0].outValue ==
            muted.state->playbackState.tracks[1].note.modulation[0].outValue);
      const int frames = muted.state->audioInputTrackPreviousFrames;
      REQUIRE(frames == audible.state->audioInputTrackPreviousFrames);
      for (int i = 0; i < frames * 2; ++i)
        CHECK(muted.state->audioInputTrackPrevious[i] == audible.state->audioInputTrackPrevious[i]);
    }
    CHECK_FALSE(muted.state->playbackState.trackEnabled[0]);
    CHECK(muted.state->playbackState.tracks[1].note.modulation[0].outValue < 0);
    CHECK_FALSE(muted.state->masterEffects->tailActive());
    CHECK(audible.state->masterEffects->tailActive());
  }
}

TEST_CASE("Monk pulses drive Plaits FM controls directly without a slow modulation envelope") {
  AudioModFixture fixture(true);
  auto& p = fixture.state->project;
  getInstrumentFunctions(InstrumentType::Monk).init(&p.instruments[0]);
  auto& monk = p.instruments[0].chip.monk;
  monk.vowel = 208; monk.character = 160; monk.breath = monk.glide = monk.vibrato = 0;
  monk.unison = 1; monk.detune = monk.voiceSpread = monk.delayMix = 0;
  monk.level = 255; monk.attack = monk.decay = monk.release = 0; monk.sustain = 255;
  getInstrumentFunctions(InstrumentType::Plaits).init(&p.instruments[1]);
  auto& plaits = p.instruments[1].chip.plaits;
  plaits.engine = 2; plaits.harmonics = 18375; plaits.timbre = plaits.morph = 16384;
  p.instruments[1].modulation[0] = {ModulationType::Audio, 3, 127, 0, 0, 0, 255, 0};
  p.trackVolume[0] = 80;
  chipnomadInitChips(fixture.state.get(), 48000, nullptr);
  REQUIRE(chipnomadQueuePlaybackPreviewNote(fixture.state.get(), 0, 24, 0));
  REQUIRE(chipnomadQueuePlaybackPreviewNote(fixture.state.get(), 1, 36, 1));
  float output[128];
  fixture.render(output, 64);
  float minimum = 1, maximum = 0;
  for (int n = 0; n < 150; ++n) {
    float peak = 0;
    const auto* s = fixture.state.get();
    for (int i = 0; i < s->audioInputTrackPreviousFrames * 2; ++i)
      peak = std::max(peak, std::fabs(s->audioInputTrackPrevious[i]));
    fixture.render(output, 64);
    const auto& track = fixture.state->playbackState.tracks[1];
    const float expected = std::min(1.0f, peak * (255.0f / 64.0f));
    CHECK(track.note.modulation[0].audio.level == doctest::Approx(expected));
    const int base = (plaits.harmonics / 129) * 129;
    const int harmonics = std::min(32767, base + playbackModScaleToRange(track.note.modulation[0].outValue, 32767));
    CHECK(track.liveControls[liveSource1] == (harmonics * 255 + 16383) / 32767);
    minimum = std::min(minimum, expected); maximum = std::max(maximum, expected);
  }
  CHECK(maximum - minimum > .05f);
  std::printf("Monk -> Plaits FM Audio envelope range: %.3f..%.3f (Atk/Rel 00, Sens FF)\n", minimum, maximum);
}

TEST_CASE("Audio follower modulates the parametric EQ insert through the normal destination") {
  AudioModFixture wet(true, false, true);
  float output[1024];
  wet.render(output, 512);
  CHECK(wet.state->insertValues[1][0][2] < 128);
  CHECK(wet.state->audioInputRoutedTrackMask == 0);
  CHECK(wet.state->audioModTrackMask == 1);
}

TEST_CASE("Audio modulation preset keeps the source, signed depth and follower parameters") {
  auto saved = std::make_unique<Project>(), loaded = std::make_unique<Project>();
  projectInit(saved.get()); projectInit(loaded.get());
  getInstrumentFunctions(InstrumentType::Sample).init(&saved->instruments[0]);
  saved->instruments[0].modulation[2] = {ModulationType::Audio, 1, -90, 7, 16, 80, 64, 0};
  const char* file = "build/tests/audio_modulation.cni";
  REQUIRE(instrumentSave(saved.get(), file, 0) == 0);
  REQUIRE(instrumentLoad(loaded.get(), file, 0) == 0);
  std::remove(file);
  const auto& mod = loaded->instruments[0].modulation[2];
  CHECK(mod.type == ModulationType::Audio);
  CHECK(mod.amount == -90);
  CHECK(mod.p1 == 7);
  CHECK(mod.p2 == 16);
  CHECK(mod.p3 == 80);
  CHECK(mod.p4 == 64);
  projectFree(saved.get()); projectFree(loaded.get());
}

TEST_CASE("Audio follower Layer benchmark shares one source across seven three-child tracks") {
  AudioModFixture dry(false, true), wet(true, true);
  float output[1024];
  dry.render(output, 512); wet.render(output, 512);
  const auto measure = [&](AudioModFixture& fixture) {
    auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) fixture.render(output, 512);
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
  };
  double baseline = measure(dry), followed = measure(wet);
  std::printf("Audio follower Layer CPU: %.2f ms baseline, %.2f ms followed, %.2fx; shared source mask=%u\n",
    baseline, followed, followed / baseline, wet.state->audioModTrackMask);
  CHECK(wet.state->audioModTrackMask == 1);
  for (int t = 1; t < 8; ++t)
    for (int lane = 0; lane < INSTRUMENT_LAYER_COUNT; ++lane)
      CHECK(wet.state->playbackState.layerTracks[t][lane].note.modulation[0].outValue < 0);
}

TEST_CASE("Audio WAVE AM and RING retain signed samples with a fixed causal delay") {
  for (auto mode : {AudioModulationMode::wave, AudioModulationMode::amplitude, AudioModulationMode::ring}) {
    AudioModFixture fixture(true);
    std::fill(fixture.source.begin(), fixture.source.end(), -8192);
    auto& mod = fixture.state->project.instruments[1].modulation[0];
    mod.amount = 127; mod.p4 = 64; mod.p5 = (uint8_t)mode;
    REQUIRE(chipnomadQueueProjectRefresh(fixture.state.get()));
    uint8_t enabled[PROJECT_MAX_TRACKS] = {}; enabled[1] = 1;
    REQUIRE(chipnomadQueueTrackEnabled(fixture.state.get(), enabled));
    float output[512];
    fixture.render(output, 256);
    CHECK(fixture.state->audioWaveTrackMask == 1);
    CHECK(output[0] >= 0);
    CHECK(output[128] != output[0]);
    const auto& runtime = fixture.state->playbackState.tracks[1].note.modulation[0];
    if (mode == AudioModulationMode::wave) CHECK(runtime.outValue < 0);
    else CHECK(runtime.outValue == 0);
    if (mode == AudioModulationMode::ring) CHECK(output[400] < 0);
    else CHECK(output[400] > 0);
    CHECK_FALSE(fixture.state->playbackState.trackEnabled[0]);
  }
}

TEST_CASE("Audio reciprocal cross modulation is finite and independent of callback sizes") {
  for (auto mode : {AudioModulationMode::wave, AudioModulationMode::amplitude, AudioModulationMode::ring}) {
    AudioModFixture whole(true), split(true);
    for (auto* fixture : {&whole, &split}) {
      auto& p = fixture->state->project;
      p.instruments[0].modulation[0] = {ModulationType::Audio, 1, 64, 1, 0, 0, 64, (uint8_t)mode};
      p.instruments[1].modulation[0] = {ModulationType::Audio, 1, 64, 0, 0, 0, 64, (uint8_t)mode};
      REQUIRE(chipnomadQueueProjectRefresh(fixture->state.get()));
    }
    std::vector<float> a(1024), b(1024);
    whole.render(a.data(), 512);
    int position = 0;
    for (int frames : {1, 7, 63, 2, 129, 3, 307}) {
      split.render(b.data() + position * 2, frames);
      position += frames;
    }
    REQUIRE(position == 512);
    for (size_t i = 0; i < a.size(); ++i) {
      CHECK(std::isfinite(a[i]));
      CHECK(a[i] == doctest::Approx(b[i]).epsilon(.00001));
    }
    CHECK(whole.state->audioWaveTrackMask == 3);
  }
}

TEST_CASE("Audio raw mode serialization retains old ENV and rejects invalid modes") {
  auto p = std::make_unique<Project>(), loaded = std::make_unique<Project>();
  projectInit(p.get()); projectInit(loaded.get());
  getInstrumentFunctions(InstrumentType::SCWF).init(&p->instruments[0]);
  const char* path = "build/tests/audio_modes.cni";
  for (int mode = 0; mode < (int)AudioModulationMode::totalCount; ++mode) {
    p->instruments[0].modulation[0] = {ModulationType::Audio, 1, -64, 1, 16, 80, 64, (uint8_t)mode};
    REQUIRE(instrumentSave(p.get(), path, 0) == 0);
    REQUIRE(instrumentLoad(loaded.get(), path, 0) == 0);
    CHECK(loaded->instruments[0].modulation[0].p5 == mode);
  }
  p->instruments[0].modulation[0].p5 = 255;
  REQUIRE(instrumentSave(p.get(), path, 0) == 0);
  CHECK(instrumentLoad(loaded.get(), path, 0) != 0);
  CHECK(std::string(projectFileError).find("Audio") != std::string::npos);
  for (int type = 0; type < (int)InstrumentType::totalCount; ++type)
    for (int mode = 0; mode < (int)AudioModulationMode::totalCount; ++mode)
      CHECK(audioModulationModeSupported((InstrumentType)type, (AudioModulationMode)mode) ==
        (mode < 2 || ((InstrumentType)type != InstrumentType::none && (InstrumentType)type != InstrumentType::Midi)));
  for (int removedMode : {4, 5}) {
    p->instruments[0].modulation[0].p5 = (uint8_t)removedMode;
    REQUIRE(instrumentSave(p.get(), path, 0) == 0);
    CHECK(instrumentLoad(loaded.get(), path, 0) != 0);
    CHECK(std::string(projectFileError).find("Unsupported Audio") != std::string::npos);
  }
  std::remove(path);
  projectFree(p.get()); projectFree(loaded.get());
}

TEST_CASE("Audio WAVE pitch reaches SCWF and BYOWTBL voices through the track router") {
  for (auto type : {InstrumentType::SCWF, InstrumentType::BYOWTBL}) {
    AudioModFixture baseline(false), modulated(false), split(false);
    for (auto* fixture : {&baseline, &modulated, &split}) {
      for (int n = 0; n < 48000; ++n) fixture->target[n] = (int16_t)(std::sin(n * 6.283185307 / 64) * 16000);
      auto& instrument = fixture->state->project.instruments[1];
      getInstrumentFunctions(type).init(&instrument);
      auto& p = type == InstrumentType::SCWF ? instrument.chip.scwf : instrument.chip.byowtbl;
      for (auto& osc : p.oscillator) {
        osc.data = fixture->target.data(); osc.frameCount = 64; osc.channels = 1;
      }
      p.attack = p.decay = p.release = 0; p.sustain = 255; p.filterEnabled = 0;
      if (fixture != &baseline)
        instrument.modulation[0] = {ModulationType::Audio, 2, 127, 0, 0, 0, 255, (uint8_t)AudioModulationMode::wave};
      chipnomadInitChips(fixture->state.get(), 48000, nullptr);
      REQUIRE(chipnomadQueuePlaybackPreviewNote(fixture->state.get(), 0, 45, 0));
      REQUIRE(chipnomadQueuePlaybackPreviewNote(fixture->state.get(), 1, 45, 1));
      uint8_t enabled[PROJECT_MAX_TRACKS] = {}; enabled[1] = 1;
      REQUIRE(chipnomadQueueTrackEnabled(fixture->state.get(), enabled));
    }
    float plain[1024], full[1024], partial[1024];
    baseline.render(plain, 512); modulated.render(full, 512);
    split.render(partial, 17); split.render(partial + 34, 495);
    double difference = 0;
    for (int n = 0; n < 1024; ++n) {
      CHECK(std::isfinite(full[n]));
      CHECK(full[n] == doctest::Approx(partial[n]).epsilon(.00001));
      difference += std::abs(full[n] - plain[n]);
    }
    CHECK(difference > 1);
    CHECK(modulated.state->audioWaveTrackMask == 1);
  }
}

TEST_CASE("Raw audio Layer CPU benchmark retains the existing voice pool") {
  for (bool varying : {false, true})
  for (int mode : {-1, (int)AudioModulationMode::amplitude, (int)AudioModulationMode::ring, (int)AudioModulationMode::wave}) {
    AudioModFixture fixture(mode >= 0, true);
    if (varying)
      for (int n = 0; n < 48000; ++n)
        fixture.source[n] = (int16_t)(std::sin(n * 6.283185307 * 440 / 48000) * 8192);
    if (mode >= 0) {
      fixture.state->project.instruments[1].modulation[0].p5 = (uint8_t)mode;
      REQUIRE(chipnomadQueueProjectRefresh(fixture.state.get()));
    }
    float output[1024];
    fixture.render(output, 512);
    const auto start = std::chrono::steady_clock::now();
    for (int n = 0; n < 8; ++n) fixture.render(output, 512);
    const auto milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::printf("Raw Audio 8 tracks x 3 Layer children %s (%s): %.2f ms / 4096 frames (48 kHz)\n",
      mode < 0 ? "OFF" : audioModulationModeName((AudioModulationMode)mode), varying ? "440 Hz" : "constant", milliseconds);
    for (auto& owner : fixture.state->audioInputVoices)
      for (auto* voice : owner) CHECK(voice == nullptr);
    for (float sample : output) CHECK(std::isfinite(sample));
  }
}

TEST_CASE("WAVE holds parameters on an eight-sample callback-independent grid") {
  AudioModFixture fixture(true);
  for (int n = 0; n < 48000; ++n)
    fixture.source[n] = (int16_t)(8192 + 6000 * std::sin(n * 6.283185307 / 32));
  fixture.state->project.instruments[1].modulation[0].p5 = (uint8_t)AudioModulationMode::wave;
  REQUIRE(chipnomadQueueProjectRefresh(fixture.state.get()));
  float output[160];
  fixture.render(output, 65);
  const auto value = fixture.state->playbackState.tracks[1].note.modulation[0].outValue;
  fixture.render(output, 7);
  CHECK(fixture.state->playbackState.tracks[1].note.modulation[0].outValue == value);
  fixture.render(output, 1);
  CHECK(fixture.state->playbackState.tracks[1].note.modulation[0].outValue != value);
}
