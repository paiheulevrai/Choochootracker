#include "doctest.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "project_utils.h"
#include "synth/master_effects.h"
#include "synth/track_audio_modulation.h"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
#include <string>
#include <cstdlib>

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
    if (!audioModulationModeValid((uint8_t)mode)) continue;
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
        (audioModulationModeValid((uint8_t)mode) &&
         (mode < 2 || ((InstrumentType)type != InstrumentType::none && (InstrumentType)type != InstrumentType::Midi))));
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
  for (int mode : {-1, (int)AudioModulationMode::amplitude, (int)AudioModulationMode::ring,
       (int)AudioModulationMode::wave, (int)AudioModulationMode::diode, (int)AudioModulationMode::fold,
       (int)AudioModulationMode::bitXor, (int)AudioModulationMode::comparator, (int)AudioModulationMode::vocoder}) {
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
TEST_CASE("Track audio cross modes are distinct, causal and callback independent") {
  for (auto mode : {AudioModulationMode::diode, AudioModulationMode::fold,
       AudioModulationMode::bitXor, AudioModulationMode::comparator, AudioModulationMode::vocoder}) {
    CAPTURE((int)mode);
    AudioModFixture dry(false), whole(true), split(true);
    for (auto* fixture : {&dry, &whole, &split}) {
      for (int n = 0; n < 48000; ++n) {
        fixture->source[n] = (int16_t)(std::sin(n * 6.283185307 * 330 / 48000) * 26000);
        fixture->target[n] = (int16_t)(std::sin(n * 6.283185307 * 900 / 48000) * 24000);
      }
      if (fixture != &dry) {
        auto& mod = fixture->state->project.instruments[1].modulation[0];
        mod.p5 = (uint8_t)mode; mod.amount = 127;
        REQUIRE(chipnomadQueueProjectRefresh(fixture->state.get()));
      }
      uint8_t enabled[PROJECT_MAX_TRACKS] = {}; enabled[1] = 1;
      REQUIRE(chipnomadQueueTrackEnabled(fixture->state.get(), enabled));
    }
    std::vector<float> a(8192), b(8192), plain(8192);
    whole.render(a.data(), 4096); dry.render(plain.data(), 4096);
    for (int offset = 0; offset < 4096;) {
      const int frames = std::min(37, 4096 - offset);
      split.render(b.data() + offset * 2, frames);
      offset += frames;
    }
    double difference = 0, outputEnergy = 0;
    for (size_t i = 0; i < a.size(); ++i) {
      CHECK(std::isfinite(a[i]));
      CHECK(std::fabs(a[i]) < 2.0f);
      CHECK(a[i] == doctest::Approx(b[i]).epsilon(.00001));
      difference += std::fabs(a[i] - plain[i]);
      outputEnergy += a[i] * a[i];
    }
    CHECK(difference > 1);
    CHECK(outputEnergy > .001);
    CHECK(whole.state->audioCrossOwners[1] == 1);
    CHECK((whole.state->audioVocoders[1][0] != nullptr) == (mode == AudioModulationMode::vocoder));
  }
}

TEST_CASE("Track audio vocoder leases prepare only required slots and survive coalesced refreshes") {
  AudioModFixture fixture(true, true);
  auto* s = fixture.state.get();
  for (auto& owner : s->audioVocoders) for (auto* vocoder : owner) CHECK(vocoder == nullptr);
  auto& mod = s->project.instruments[1].modulation[0];
  mod.p5 = (uint8_t)AudioModulationMode::vocoder;
  REQUIRE(chipnomadQueueProjectRefresh(s));
  float output[1024];
  fixture.render(output, 512);
  auto* original = s->audioVocoders[0][0];
  REQUIRE(original != nullptr);
  for (auto& owner : s->audioVocoders) {
    CHECK(owner[0] != nullptr);
    for (int m = 1; m < 4; ++m) CHECK(owner[m] == nullptr);
  }
  for (int n = 0; n < 5; ++n) REQUIRE(chipnomadQueueProjectRefresh(s));
  fixture.render(output, 512);
  CHECK(s->audioVocoders[0][0] == original);
  mod.p5 = (uint8_t)AudioModulationMode::ring;
  REQUIRE(chipnomadQueueProjectRefresh(s));
  fixture.render(output, 512);
  fixture.render(output, 512);
  for (auto& owner : s->audioVocoders) for (auto* vocoder : owner) CHECK(vocoder == nullptr);
  REQUIRE(chipnomadQueueProjectRefresh(s));
  fixture.render(output, 512);
}

TEST_CASE("Track audio vocoder transfers a modulator spectrum and releases on silence") {
  auto render = [](float frequency, bool silentCarrier) {
    TrackAudioVocoder vocoder(48000);
    std::vector<float> samples(48000);
    for (int n = 0; n < 48000; ++n) {
      const float source = n < 24000 ? std::sin(n * 6.283185307f * frequency / 48000) * .8f : 0;
      float left = silentCarrier ? 0 : (std::sin(n * 6.283185307f * 220 / 48000) +
        std::sin(n * 6.283185307f * 1800 / 48000)) * .25f;
      float right = left;
      vocoder.process(source, left, right, 1.0f - std::exp(-1.0f / (0.02f * 48000)));
      samples[n] = left;
      REQUIRE(std::isfinite(left));
      CHECK(left == right);
    }
    return samples;
  };
  auto low = render(220, false), high = render(1800, false), silent = render(220, true);
  auto magnitude = [](const std::vector<float>& samples, float hz) {
    double re = 0, im = 0;
    for (int n = 12000; n < 24000; ++n) {
      re += samples[n] * std::cos(n * 6.283185307 * hz / 48000);
      im += samples[n] * std::sin(n * 6.283185307 * hz / 48000);
    }
    return re * re + im * im;
  };
  CHECK(magnitude(low, 220) > magnitude(low, 1800) * 4);
  CHECK(magnitude(high, 1800) > magnitude(high, 220) * 4);
  double late = 0;
  for (int n = 45000; n < 48000; ++n) late += low[n] * low[n];
  CHECK(late < 1e-10);
  for (float sample : silent) CHECK(sample == 0);
}

TEST_CASE("Monk vowels retain formant contrast through a CSAW track vocoder") {
  std::vector<float> sounds[6];
  const uint8_t vowels[] = {0, 64, 128, 191, 255, 128};
  double power[6] = {};
  for (int vowel = 0; vowel < 6; ++vowel) {
    AudioModFixture fixture(false);
    auto& p = fixture.state->project;
    getInstrumentFunctions(InstrumentType::Monk).init(&p.instruments[0]);
    auto& monk = p.instruments[0].chip.monk;
    monk.vowel = vowels[vowel];
    monk.glide = monk.delayMix = monk.vibrato = monk.breath = 0;
    monk.unison = 1;
    getInstrumentFunctions(InstrumentType::Braids).init(&p.instruments[1]);
    auto& braids = p.instruments[1].chip.braids;
    braids.model = 0;
    braids.filterEnabled = 0;
    braids.attack = braids.decay = 0; braids.sustain = 255;
    p.instruments[1].modulation[0] = {ModulationType::Audio, 1, (int8_t)(vowel == 5 ? 0 : 127), 0, 0, 64, 79,
      (uint8_t)AudioModulationMode::vocoder};
    chipnomadInitChips(fixture.state.get(), 48000, nullptr);
    REQUIRE(chipnomadQueuePlaybackPreviewNote(fixture.state.get(), 0, 45, 0));
    REQUIRE(chipnomadQueuePlaybackPreviewNote(fixture.state.get(), 1, 45, 1));
    uint8_t enabled[PROJECT_MAX_TRACKS] = {}; enabled[1] = 1;
    REQUIRE(chipnomadQueueTrackEnabled(fixture.state.get(), enabled));
    float output[1024];
    for (int block = 0; block < 48; ++block) fixture.render(output, 512);
    sounds[vowel].resize(48000);
    fixture.render(sounds[vowel].data(), 24000);
    for (float sample : sounds[vowel]) {
      REQUIRE(std::isfinite(sample));
      power[vowel] += sample * sample;
    }
    if (const char* folder = std::getenv("CCT_VOCODER_CAPTURE")) {
      const std::string path = std::string(folder) + "\\monk-csaw-" + std::to_string(vowel) + ".f32";
      FILE* file = std::fopen(path.c_str(), "wb");
      REQUIRE(file != nullptr);
      REQUIRE(std::fwrite(sounds[vowel].data(), sizeof(float), sounds[vowel].size(), file) == sounds[vowel].size());
      REQUIRE(std::fclose(file) == 0);
    }
  }
  auto bandEnergy = [](const std::vector<float>& sound, int low, int high) {
    double result = 0;
    for (int hz = low; hz <= high; hz += 20) {
      double re = 0, im = 0;
      for (int n = 0; n < 24000; ++n) {
        const double phase = n * 6.283185307179586 * hz / 48000;
        re += sound[n * 2] * std::cos(phase);
        im += sound[n * 2] * std::sin(phase);
      }
      result += re * re + im * im;
    }
    return result;
  };
  double contrast[5];
  for (int vowel = 0; vowel < 5; ++vowel) {
    contrast[vowel] = bandEnergy(sounds[vowel], 300, 700) /
      std::max(1e-20, bandEnergy(sounds[vowel], 1100, 2500));
    std::printf("Monk -> CSAW vowel %d: %.2f dB relative to dry, low/mid ratio %.3f\n",
      vowels[vowel], 10 * std::log10(power[vowel] / power[5]), contrast[vowel]);
    CHECK(power[vowel] > power[5] * 0.5);
    CHECK(power[vowel] < power[5] * 4);
  }
  const auto limits = std::minmax_element(std::begin(contrast), std::end(contrast));
  const double contrastDb = 10 * std::log10(*limits.second / *limits.first);
  std::printf("Monk -> CSAW vowel spectral contrast: %.2f dB\n", contrastDb);
  CHECK(contrastDb > 12);
}

TEST_CASE("Track audio vocoder levels hot sources without losing their spectrum") {
  double energy[3] = {};
  for (int volume = 0; volume < 3; ++volume) {
    TrackAudioVocoder vocoder(48000);
    const float scale = volume == 0 ? .8f : volume == 1 ? 4.0f : 16.0f;
    const float release = vocoder.releaseCoefficient(64);
    for (int n = 0; n < 24000; ++n) {
      const float source = scale * (std::sin(n * 6.283185307f * 600 / 48000) +
        .1f * std::sin(n * 6.283185307f * 1800 / 48000));
      float left = .2f * (std::sin(n * 6.283185307f * 600 / 48000) +
        std::sin(n * 6.283185307f * 1800 / 48000));
      float right = 0;
      vocoder.process(source, left, right, release);
      REQUIRE(std::isfinite(left));
      CHECK(std::fabs(left) <= 1);
      CHECK(right == 0);
      if (n >= 12000) energy[volume] += left * left;
    }
    vocoder.reset();
    float left = 0, right = 0;
    vocoder.process(0, left, right, release);
    CHECK(left == 0);
    CHECK(right == 0);
  }
  REQUIRE(energy[0] > 1);
  CHECK(energy[1] == doctest::Approx(energy[0]).epsilon(.005));
  CHECK(energy[2] == doctest::Approx(energy[0]).epsilon(.005));
}

TEST_CASE("Fold and XOR do not turn quiet carriers or release tails into full-scale output") {
  for (auto mode : {AudioModulationMode::fold, AudioModulationMode::bitXor}) {
    for (float amplitude : {0.0f, .00001f, .001f, .01f, .1f, 1.0f}) {
      double dryEnergy = 0, wetEnergy = 0;
      for (int n = 0; n < 4096; ++n) {
        const float carrier = amplitude * std::sin(n * .13f);
        const float source = std::sin(n * .037f);
        const float wet = trackAudioCrossSample(mode, carrier, source);
        REQUIRE(std::isfinite(wet));
        CHECK(std::fabs(wet) <= std::fabs(carrier) + 1e-7f);
        dryEnergy += carrier * carrier;
        wetEnergy += wet * wet;
      }
      CHECK(wetEnergy <= dryEnergy + 1e-10);
      CHECK(trackAudioCrossSample(mode, 0, 1) == 0);
    }
  }
}

TEST_CASE("Optional user vocoder project loads and plays") {
  const char* path = std::getenv("CCT_VOCODER_PROJECT");
  if (!path || !*path) return;
  std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)> state{chipnomadCreate(), chipnomadDestroy};
  REQUIRE(state != nullptr);
  REQUIRE(projectLoad(&state->project, path) == 0);
  chipnomadInitChips(state.get(), 48000, nullptr);
  REQUIRE(chipnomadReserveRenderBuffers(state.get(), 512) == 0);
  playbackStartSong(&state->playbackState, 0, 0, 1);
  float output[1024];
  double energy = 0;
  for (int block = 0; block < 188; ++block) {
    REQUIRE(chipnomadRender(state.get(), output, 512) == 512);
    for (float sample : output) {
      REQUIRE(std::isfinite(sample));
      energy += sample * sample;
    }
  }
  CHECK(energy > 0.01);
}

TEST_CASE("Track audio zero depth preserves the exact mix including Layers") {
  for (bool layers : {false, true})
  for (auto mode : {AudioModulationMode::diode, AudioModulationMode::fold,
       AudioModulationMode::bitXor, AudioModulationMode::comparator, AudioModulationMode::vocoder}) {
    AudioModFixture dry(false, layers), processed(true, layers);
    auto& mod = processed.state->project.instruments[1].modulation[0];
    mod.amount = 0; mod.p5 = (uint8_t)mode;
    REQUIRE(chipnomadQueueProjectRefresh(processed.state.get()));
    float a[1024], b[1024];
    dry.render(a, 512); processed.render(b, 512);
    for (int n = 0; n < 1024; ++n) CHECK(a[n] == b[n]);
  }
}

TEST_CASE("Track audio reciprocal vocoders and multiple Layer slots remain callback independent") {
  for (bool layers : {false, true}) {
    AudioModFixture whole(true, layers), split(true, layers);
    for (auto* fixture : {&whole, &split}) {
      auto& p = fixture->state->project;
      for (int n = 0; n < 48000; ++n) {
        fixture->source[n] = (int16_t)(std::sin(n * 6.283185307 * 330 / 48000) * 24000);
        fixture->target[n] = (int16_t)(std::sin(n * 6.283185307 * 900 / 48000) * 24000);
      }
      p.instruments[0].modulation[0] = {ModulationType::Audio, 1, 90, 1, 0, 80, 64, (uint8_t)AudioModulationMode::vocoder};
      p.instruments[1].modulation[0] = {ModulationType::Audio, 1, 90, 0, 0, 80, 64, (uint8_t)AudioModulationMode::vocoder};
      p.instruments[1].modulation[1] = {ModulationType::Audio, 1, -60, 0, 0, 0, 64, (uint8_t)AudioModulationMode::diode};
      if (layers)
        p.instruments[2].modulation[0] = {ModulationType::Audio, 1, 90, 0, 0, 80, 64, (uint8_t)AudioModulationMode::fold};
      REQUIRE(chipnomadQueueProjectRefresh(fixture->state.get()));
    }
    std::vector<float> a(2048), b(2048);
    whole.render(a.data(), 1024);
    for (int offset = 0; offset < 1024;) {
      const int frames = std::min(17, 1024 - offset);
      split.render(b.data() + offset * 2, frames);
      offset += frames;
    }
    double outputEnergy = 0;
    for (int n = 0; n < 2048; ++n) {
      CHECK(std::isfinite(a[n]));
      CHECK(a[n] == doctest::Approx(b[n]).epsilon(.00001));
      outputEnergy += a[n] * a[n];
    }
    CHECK(outputEnergy > .000001);
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
