#include "doctest.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "synth/audio_input_voice.h"
#include "audio_input_manager.h"
#include "track_bounce.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

TEST_CASE("Audio in gates stereo with ADSR and filters both channels") {
  Instrument instrument{};
  getInstrumentFunctions(InstrumentType::AudioIn).init(&instrument);
  auto& p = instrument.chip.audioIn;
  p.filterEnabled = 0;
  p.attack = 0;
  p.release = 8;
  AudioInputVoice voice;
  voice.init(48000);
  voice.configure(p, 1);
  float input[512], output[512];
  for (int i = 0; i < 256; ++i) { input[i * 2] = .25f; input[i * 2 + 1] = -.5f; }
  voice.setInput(input);
  voice.render(output, 256);
  for (float sample : output) CHECK(sample == 0);
  voice.noteOn(); voice.render(output, 256);
  for (int i = 0; i < 512; ++i) CHECK(output[i] == input[i]);
  voice.noteOff(); voice.render(output, 256);
  CHECK(output[0] > 0);
  CHECK(output[511] == 0);
  CHECK_FALSE(voice.active());
  p.filterEnabled = 1; p.filterCharacter = 1; p.filterCutoffHz = 100;
  voice.configure(p, 1); voice.noteOn(); voice.render(output, 256);
  CHECK(std::fabs(output[0]) < .01f);
  for (int i = 0; i < 256; ++i) CHECK(output[i * 2 + 1] == doctest::Approx(-2 * output[i * 2]));
  voice.kill(); voice.render(output, 256);
  for (float sample : output) CHECK(sample == 0);
  p.filterEnabled = 0; voice.configure(p, 1); voice.noteOn(); voice.setInput(nullptr);
  voice.render(output, 256);
  for (float sample : output) CHECK(sample == 0);
}

TEST_CASE("Audio in capture is stereo and independent from dry monitoring") {
  AudioInputManager manager;
  AudioInputManagerTestPeer::captureOpenForTest(manager, true);
  AudioInputManagerTestPeer::armSlotForTest(manager, 0, 0);
  manager.setMonitor(1);
  int16_t input[] = {8192, -16384, 4096, -8192};
  manager.onCapture(input, 2, 2);
  float live[8]; REQUIRE(manager.readLiveInput(live, 4));
  CHECK(live[0] == .25f); CHECK(live[1] == -.5f);
  CHECK(live[2] == .125f); CHECK(live[3] == -.25f);
  for (int i = 4; i < 8; ++i) CHECK(live[i] == 0);
  float monitor[4]{}; manager.mixMonitor(monitor, 2);
  CHECK(monitor[0] == .125f); CHECK(monitor[1] == -.25f);
  int16_t mono[] = {8192, -4096}; manager.onCapture(mono, 2, 1);
  REQUIRE(manager.readLiveInput(live, 2));
  CHECK(live[0] == .25f); CHECK(live[1] == .25f);
  CHECK(live[2] == -.125f); CHECK(live[3] == -.125f);
  std::vector<int16_t> backlog(4096 * 2);
  for (int i = 0; i < 4096; ++i) { backlog[i * 2] = i; backlog[i * 2 + 1] = -i; }
  manager.onCapture(backlog.data(), 4096, 2);
  float recent[128]; REQUIRE(manager.readLiveInput(recent, 64));
  CHECK(recent[0] == doctest::Approx(3008 / 32768.0f));
  CHECK(recent[1] == doctest::Approx(-3008 / 32768.0f));
}

TEST_CASE("Audio in retrigger preserves a running stereo filter") {
  for (int character = 1; character <= 6; ++character) {
    CAPTURE(character);
    Instrument instrument{};
    getInstrumentFunctions(InstrumentType::AudioIn).init(&instrument);
    auto& p = instrument.chip.audioIn;
    p.filterEnabled = 1;
    p.filterCharacter = character;
    p.filterCutoffHz = 400;
    p.filterResonance = 80;
    AudioInputVoice reference, retriggered;
    float input[512], expected[512], actual[512];
    for (int i = 0; i < 256; ++i) { input[2 * i] = .25f; input[2 * i + 1] = -.125f; }
    for (auto* voice : {&reference, &retriggered}) {
      voice->init(48000);
      voice->configure(p, 1);
      voice->setInput(input, 256);
      voice->noteOn();
    }
    for (int block = 0; block < 8; ++block) {
      reference.render(expected, 256);
      retriggered.render(actual, 256);
    }
    retriggered.noteOn();
    reference.render(expected, 256);
    retriggered.render(actual, 256);
    float maxDifference = 0;
    for (int i = 0; i < 512; ++i)
      maxDifference = std::fmax(maxDifference, std::fabs(actual[i] - expected[i]));
    CHECK(maxDifference < 1e-6f);
  }
}

TEST_CASE("Audio in envelope settings shape notes and retriggers") {
  Instrument instrument{};
  getInstrumentFunctions(InstrumentType::AudioIn).init(&instrument);
  auto& p = instrument.chip.audioIn;
  p.filterEnabled = 0;
  p.attack = 16;
  p.decay = 16;
  p.sustain = 64;
  p.release = 16;
  p.envelopeShape = 128;
  AudioInputVoice voice;
  float input[4096], output[4096];
  for (float& sample : input) sample = .5f;
  voice.init(48000);
  voice.configure(p, 1);
  voice.setInput(input, 2048);
  voice.noteOn();
  voice.render(output, 256);
  CHECK(output[0] == 0);
  CHECK(output[510] > 0);
  CHECK(output[510] < .25f);
  voice.render(output, 2048);
  CHECK(output[4094] == doctest::Approx(.5f * 64 / 255));

  p.sustain = 128;
  voice.configure(p, 1);
  const float before = voice.envelopeLevel();
  voice.noteOn();
  voice.render(output, 2048);
  CHECK(output[0] == doctest::Approx(.5f * before));
  CHECK(output[4094] == doctest::Approx(.5f * 128 / 255));
  voice.noteOff();
  voice.render(output, 2048);
  CHECK(output[0] > 0);
  CHECK(output[4094] == 0);
  CHECK_FALSE(voice.active());
}

TEST_CASE("Audio in preset preserves post settings and utility category") {
  auto saved = std::make_unique<Project>(), loaded = std::make_unique<Project>();
  fillFXNames(); projectInit(saved.get()); projectInit(loaded.get());
  auto& instrument = saved->instruments[0];
  getInstrumentFunctions(InstrumentType::AudioIn).init(&instrument);
  CHECK(instrument.audioInputSource == InstrumentAudioInputSource::external);
  instrument.chip.audioIn.attack = 47; instrument.chip.audioIn.release = 82;
  instrument.chip.audioIn.filterCutoffHz = 1234;
  instrument.audioInputSource = InstrumentAudioInputSource::track8;
  const char* path = "build/tests/audio_in.cni";
  REQUIRE(instrumentSave(saved.get(), path, 0) == 0);
  REQUIRE(instrumentLoad(loaded.get(), path, 0) == 0);
  std::remove(path);
  CHECK(loaded->instruments[0].type == InstrumentType::AudioIn);
  CHECK(loaded->instruments[0].chip.audioIn.attack == 47);
  CHECK(loaded->instruments[0].chip.audioIn.release == 82);
  CHECK(loaded->instruments[0].chip.audioIn.filterCutoffHz == 1234);
  CHECK(loaded->instruments[0].audioInputSource == InstrumentAudioInputSource::track8);
  CHECK(getInstrumentDefinition(InstrumentType::AudioIn)->category == getInstrumentDefinition(InstrumentType::Midi)->category);
  projectFree(saved.get()); projectFree(loaded.get());
}

TEST_CASE("Audio in engine follows notes, chunk offsets, inserts and offline silence") {
  auto dry = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(chipnomadCreate(), chipnomadDestroy);
  auto wet = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(chipnomadCreate(), chipnomadDestroy);
  for (auto* s : {dry.get(), wet.get()}) {
    auto& p = s->project;
    p.chipsCount = p.tracksCount = 1; p.tickRate = 50; p.linearPitch = 1;
    calculateLinearPitchTable12TET(&p);
    getInstrumentFunctions(InstrumentType::AudioIn).init(&p.instruments[0]);
    p.instruments[0].chip.audioIn.filterEnabled = 0;
    p.instruments[0].chip.audioIn.attack = 0;
    p.instruments[0].volume = 255; p.trackVolume[0] = 100;
    p.trackReverbSend[0] = p.trackDelaySend[0] = 0;
    p.reverbReturn = p.delayReturn = 0;
    if (s == wet.get()) insertSelect(&p.trackInserts[0][0], insertDistortion);
    chipnomadInitChips(s, 48000, nullptr);
    REQUIRE(chipnomadReserveRenderBuffers(s, 2048) == 0);
    chipnomadQueuePlaybackPreviewNote(s, 0, 45, 0);
  }
  float input[4096], a[4096]{}, b[4096]{};
  for (int i = 0; i < 2048; ++i) { input[i * 2] = .2f + .1f * std::sin(i * .03f); input[i * 2 + 1] = -input[i * 2] * .5f; }
  REQUIRE(chipnomadRender(dry.get(), a, 2048, input) == 2048);
  REQUIRE(chipnomadRender(wet.get(), b, 2048, input) == 2048);
  float scale = a[1000] / input[1000];
  CHECK(scale > 0);
  double difference = 0;
  for (int i = 0; i < 4096; ++i) {
    CHECK(a[i] == doctest::Approx(input[i] * scale).epsilon(.001));
    CHECK(std::isfinite(b[i])); difference += std::fabs(a[i] - b[i]);
  }
  CHECK(difference > .01);
  REQUIRE(chipnomadRender(dry.get(), a, 2048) == 2048);
  for (float sample : a) CHECK(sample == 0);
  chipnomadQueuePlaybackStopPreview(dry.get(), 0);
  for (int i = 0; i < 3; ++i) { std::fill(a, a + 4096, 0); chipnomadRender(dry.get(), a, 2048, input); }
  for (float sample : a) CHECK(sample == 0);
}

TEST_CASE("Audio in can listen to a delayed internal track and sampler bounce includes its inserts") {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(chipnomadCreate(), chipnomadDestroy);
  auto& p = state->project;
  p.chipsCount = 0; p.tracksCount = 2; p.tickRate = 50; p.linearPitch = 1;
  calculateLinearPitchTable12TET(&p);
  for (int i = 0; i < 2; ++i) {
    getInstrumentFunctions(InstrumentType::AudioIn).init(&p.instruments[i]);
    p.instruments[i].chip.audioIn.filterEnabled = 0;
    p.instruments[i].volume = 255;
    p.trackVolume[i] = 100;
    p.trackReverbSend[i] = p.trackDelaySend[i] = 0;
  }
  p.instruments[1].audioInputSource = InstrumentAudioInputSource::track1;
  p.reverbReturn = p.delayReturn = 0;
  insertSelect(&p.trackInserts[0][0], insertDistortion);
  insertEdit(&p.trackInserts[0][0], 0, 255);
  chipnomadInitChips(state.get(), 48000, nullptr);
  REQUIRE(chipnomadReserveRenderBuffers(state.get(), 2048) == 0);
  REQUIRE(chipnomadQueuePlaybackPreviewNote(state.get(), 0, 45, 0));
  REQUIRE(chipnomadQueuePlaybackPreviewNote(state.get(), 1, 45, 1));
  float input[4096], first[4096]{}, second[4096]{};
  for (int i = 0; i < 2048; ++i) { input[2 * i] = .25f; input[2 * i + 1] = -.125f; }
  REQUIRE(chipnomadRender(state.get(), first, 256, input) == 256);
  CHECK(state->playbackState.tracks[1].note.instrument == 1);
  CHECK(state->audioProject.instruments[1].audioInputSource == InstrumentAudioInputSource::track1);
  CHECK(state->audioInputRoutedTrackMask == 1);
  CHECK(state->insertActive[0] == 1);
  for (float sample : first) CHECK(std::isfinite(sample));
  double firstEnergy = 0, routedSourceEnergy = 0;
  for (float sample : first) firstEnergy += std::fabs(sample);
  const float* processedSource = state->audioInputTrackPrevious;
  for (int i = 0; i < 256 * 2; ++i) routedSourceEnergy += std::fabs(processedSource[i]);
  CHECK(firstEnergy < 1e-6);
  CHECK(routedSourceEnergy > 0);
  int16_t bounced[4096 * 2]{};
  trackBounceStart(1);
  REQUIRE(chipnomadRender(state.get(), second, 2048, input) == 2048);
  double secondEnergy = 0;
  for (int i = 0; i < 4096; ++i) {
    secondEnergy += std::fabs(second[i]);
  }
  REQUIRE(trackBouncePop(bounced, 2048) == 2048);
  double bouncedEnergy = 0;
  for (int i = 0; i < 2048 * 2; ++i) bouncedEnergy += std::abs((int)bounced[i]);
  CHECK(secondEnergy > 0);
  CHECK(bouncedEnergy > 0);
  CHECK(trackBounceDropped() == 0);
  trackBounceStop();
  state->audioProject.instruments[1].audioInputSource = InstrumentAudioInputSource::track2;
  float cycle[256]{};
  REQUIRE(chipnomadRender(state.get(), cycle, 128, input) == 128);
  for (float sample : cycle) CHECK(std::isfinite(sample));
}

TEST_CASE("Audio Input reroutes internal tracks without inserts away from the mixer") {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(chipnomadCreate(), chipnomadDestroy);
  auto& p = state->project;
  p.chipsCount = 0; p.tracksCount = 2; p.tickRate = 50; p.linearPitch = 1;
  calculateLinearPitchTable12TET(&p);
  for (int i = 0; i < 2; ++i) {
    getInstrumentFunctions(InstrumentType::AudioIn).init(&p.instruments[i]);
    p.instruments[i].chip.audioIn.filterEnabled = 0;
    p.instruments[i].volume = 255;
    p.trackVolume[i] = 100;
    p.trackReverbSend[i] = p.trackDelaySend[i] = 0;
  }
  p.instruments[1].audioInputSource = InstrumentAudioInputSource::track1;
  p.reverbReturn = p.delayReturn = 0;
  chipnomadInitChips(state.get(), 48000, nullptr);
  REQUIRE(chipnomadReserveRenderBuffers(state.get(), 256) == 0);
  REQUIRE(chipnomadQueuePlaybackPreviewNote(state.get(), 0, 45, 0));
  REQUIRE(chipnomadQueuePlaybackPreviewNote(state.get(), 1, 45, 1));
  float input[512], first[512]{}, second[512]{};
  for (int i = 0; i < 256; ++i) { input[2 * i] = .25f; input[2 * i + 1] = -.125f; }
  REQUIRE(chipnomadRender(state.get(), first, 128, input) == 128);
  CHECK(state->audioInputRoutedTrackMask == 1);
  double firstEnergy = 0;
  for (float sample : first) firstEnergy += std::fabs(sample);
  CHECK(firstEnergy == doctest::Approx(0.0));
  for (int i = 0; i < 4; ++i)
    REQUIRE(chipnomadRender(state.get(), second, 128, input) == 128);
  double secondEnergy = 0;
  for (float sample : second) secondEnergy += std::fabs(sample);
  CHECK(secondEnergy > 0);
}

TEST_CASE("Audio in internal routing keeps audio continuous across varying render chunks") {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(chipnomadCreate(), chipnomadDestroy);
  auto& p = state->project;
  p.chipsCount = 0; p.tracksCount = 2; p.tickRate = 50; p.linearPitch = 1;
  calculateLinearPitchTable12TET(&p);
  for (int i = 0; i < 2; ++i) {
    getInstrumentFunctions(InstrumentType::AudioIn).init(&p.instruments[i]);
    p.instruments[i].chip.audioIn.filterEnabled = 0;
    p.instruments[i].chip.audioIn.attack = 0;
    p.instruments[i].volume = 255;
    p.trackVolume[i] = 100;
    p.trackReverbSend[i] = p.trackDelaySend[i] = 0;
  }
  p.instruments[1].audioInputSource = InstrumentAudioInputSource::track1;
  p.reverbReturn = p.delayReturn = 0;
  chipnomadInitChips(state.get(), 48000, nullptr);
  REQUIRE(chipnomadReserveRenderBuffers(state.get(), 512) == 0);
  REQUIRE(chipnomadQueuePlaybackPreviewNote(state.get(), 0, 45, 0));
  REQUIRE(chipnomadQueuePlaybackPreviewNote(state.get(), 1, 45, 1));
  float input[1024], output[1024];
  const int sizes[] = {512, 127, 339, 64, 512, 91};
  int position = 0;
  for (int block = 0; block < 32; ++block) {
    const int frames = sizes[block % 6];
    for (int i = 0; i < frames; ++i) {
      input[2 * i] = .25f + .1f * std::sin((position + i) * .013f);
      input[2 * i + 1] = -.5f * input[2 * i];
    }
    REQUIRE(chipnomadRender(state.get(), output, frames, input) == frames);
    for (int i = 0; i < frames; ++i) {
      if (position + i < AUDIO_INPUT_ROUTE_DELAY_FRAMES) continue;
      const float expected = .25f + .1f * std::sin((position + i - AUDIO_INPUT_ROUTE_DELAY_FRAMES) * .013f);
      CHECK(output[2 * i] == doctest::Approx(expected).epsilon(.0001));
      CHECK(output[2 * i + 1] == doctest::Approx(-.5f * expected).epsilon(.0001));
    }
    position += frames;
  }
}

TEST_CASE("sampler track input captures Audio Input after its inserts") {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(chipnomadCreate(), chipnomadDestroy);
  auto& p = state->project;
  p.chipsCount = 0; p.tracksCount = 1; p.tickRate = 50; p.linearPitch = 1;
  calculateLinearPitchTable12TET(&p);
  getInstrumentFunctions(InstrumentType::AudioIn).init(&p.instruments[0]);
  p.instruments[0].chip.audioIn.filterEnabled = 0;
  p.trackReverbSend[0] = p.trackDelaySend[0] = 0;
  p.reverbReturn = p.delayReturn = 0;
  insertSelect(&p.trackInserts[0][0], insertDistortion);
  chipnomadInitChips(state.get(), 48000, nullptr);
  REQUIRE(chipnomadReserveRenderBuffers(state.get(), 2048) == 0);
  REQUIRE(chipnomadQueuePlaybackPreviewNote(state.get(), 0, 45, 0));
  float input[4096], output[4096]{};
  for (int i = 0; i < 2048; ++i) { input[2 * i] = .2f + .1f * std::sin(i * .03f); input[2 * i + 1] = -input[2 * i] * .5f; }
  trackBounceStart(0);
  REQUIRE(chipnomadRender(state.get(), output, 2048, input) == 2048);
  int16_t bounced[4096 * 2]{};
  REQUIRE(trackBouncePop(bounced, 2048) == 2048);
  for (int i = 0; i < 2048 * 2; ++i)
    CHECK(std::fabs((float)bounced[i] - output[i] * 32767.0f) < 2.0f);
  trackBounceStop();
}
