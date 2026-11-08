#include "doctest.h"
#include "chipnomad_lib.h"
#include "synth/bass808_voice.h"
#include "synth/audio_math.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <algorithm>
#include <cmath>
#include <vector>

TEST_CASE("Fatty exposes voice-post settings and routes its controls") {
  Instrument instrument{};
  getInstrumentFunctions(InstrumentType::Bass808).init(&instrument);

  CHECK(instrumentVoicePostSettings(&instrument) == &instrument.chip.bass808);
  const FXId expectedFX[] = {fxBWF, fxBSW, fxBSL, fxBDR, fxGLD, fxB80CF, fxB80RS};
  for (int destination = 3; destination <= 9; ++destination) {
    FXId fx = FX_ID_EMPTY;
    int base = 0, range = 0;
    InstrumentMotionValue value{};
    REQUIRE(instrumentMotionDestination(&instrument, destination, &fx, &base, &range, &value));
    CHECK(fx == expectedFX[destination - 3]);
  }

  CHECK(instrumentSetCCDestination(&instrument, 3, 127));
  CHECK(instrument.chip.bass808.waveform == 3);
  CHECK(instrumentSetCCDestination(&instrument, 4, 127));
  CHECK(instrument.chip.bass808.subWaveform == 4);
  CHECK(instrumentSetCCDestination(&instrument, 5, 64));
  CHECK(instrument.chip.bass808.subLevel == instrumentCCDestinationValue(&instrument, 5, 64));
  CHECK(instrumentCCDestinationAvailable(&instrument, midiCCDestinationAttack));
  CHECK(instrumentSetCCDestination(&instrument, midiCCDestinationAttack, 127));
  CHECK(instrument.chip.bass808.attack == 255);
}

TEST_CASE("Fatty zero glide snaps pitch immediately") {
  InstrumentBass808 instrument{};
  instrument.waveform = (uint8_t)Bass808Waveform::saw;
  instrument.filterEnabled = 0;
  instrument.sustain = 255;
  instrument.envelopeMode = 2;
  Bass808Voice snap, glide;
  snap.init(48000);
  glide.init(48000);
  snap.configure(&instrument, 6900, 1.0f, FILTER_CUTOFF_MAX_HZ, 0, 0);
  glide.configure(&instrument, 6900, 1.0f, FILTER_CUTOFF_MAX_HZ, 0, 0);
  snap.noteOn();
  glide.noteOn();
  float snapWarmup[1000], glideWarmup[1000];
  snap.render(snapWarmup, 1000);
  glide.render(glideWarmup, 1000);

  snap.configure(&instrument, 5700, 1.0f, FILTER_CUTOFF_MAX_HZ, 0, 0);
  glide.configure(&instrument, 5700, 1.0f, FILTER_CUTOFF_MAX_HZ, 0, 1);
  float snapSample, glideSample;
  snap.render(&snapSample, 1);
  glide.render(&glideSample, 1);
  const float snapStep = snapSample - snapWarmup[999];
  const float glideStep = glideSample - glideWarmup[999];
  REQUIRE(glideStep > 0.0f);
  CHECK(snapStep < glideStep);
}

TEST_CASE("Fatty VCA retrigger restarts attack and completes ADSR") {
  InstrumentBass808 instrument{};
  instrument.waveform = (uint8_t)Bass808Waveform::sine;
  instrument.filterEnabled = 0;
  instrument.attack = 32;
  instrument.decay = 32;
  instrument.sustain = 128;
  instrument.release = 32;
  instrument.envelopeShape = 128;
  instrument.envelopeMode = 2;

  Bass808Voice voice;
  voice.init(48000);
  voice.configure(&instrument, 6900, 1.0f, FILTER_CUTOFF_MAX_HZ, 0, 0);
  voice.noteOn();
  std::vector<float> output(96000);
  voice.render(output.data(), output.size());
  REQUIRE(voice.envelopeLevel() == doctest::Approx(128.0f / 255));

  voice.noteOn();
  CHECK(voice.envelopeLevel() == 0.0f);
  voice.render(output.data(), 1024);
  CHECK(voice.envelopeLevel() > 0.0f);
  CHECK(voice.envelopeLevel() < 0.5f);
  voice.render(output.data(), output.size());
  CHECK(voice.envelopeLevel() == doctest::Approx(128.0f / 255));
  voice.noteOff();
  voice.render(output.data(), output.size());
  CHECK_FALSE(voice.active());
  CHECK(output.back() == 0.0f);
}

TEST_CASE("Fatty preserves Reip linear VCA timing and per-note filter reset") {
  InstrumentBass808 instrument{};
  instrument.waveform = 2;
  instrument.attack = 32;
  instrument.decay = 32;
  instrument.sustain = 128;
  instrument.release = 32;
  instrument.envelopeShape = 128;
  instrument.envelopeMode = 2;
  instrument.filterEnabled = 1;
  instrument.filterCharacter = 2;
  instrument.filterSlope24dB = 1;
  constexpr int rate = 48000;
  const int stageFrames = (int)(32 / 255.0f * 4.0f * rate);
  Bass808Voice voice;
  voice.init(rate);
  voice.configure(&instrument, 4500, 1, 800, 0x80, 255);
  voice.noteOn();
  std::vector<float> output(stageFrames);
  voice.render(output.data(), stageFrames / 2);
  CHECK(voice.envelopeLevel() == doctest::Approx(0.5f).epsilon(0.001));
  voice.render(output.data(), stageFrames - stageFrames / 2);
  CHECK(voice.envelopeLevel() == 1.0f);
  voice.render(output.data(), stageFrames);
  CHECK(voice.envelopeLevel() == doctest::Approx(128.0f / 255));
  voice.noteOff();
  voice.render(output.data(), stageFrames / 2);
  CHECK(voice.envelopeLevel() == doctest::Approx(64.0f / 255).epsilon(0.001));

  // At a one-period block boundary, the retrigger has the same oscillator
  // phase as a fresh note; only its short output bridge may differ.
  instrument.attack = instrument.decay = 0;
  instrument.sustain = 255;
  instrument.release = 0;
  Bass808Voice retriggered, fresh;
  retriggered.init(rate);
  fresh.init(rate);
  const float cents = 6900 + 1200 * std::log2(375.0f / 440.0f);
  for (auto* v : {&retriggered, &fresh})
    v->configure(&instrument, cents, 1, 800, 0x80, 255);
  retriggered.noteOn();
  std::vector<float> warmup(128 * 100);
  retriggered.render(warmup.data(), warmup.size());
  retriggered.noteOn();
  fresh.noteOn();
  std::vector<float> actual(2048), expected(2048);
  retriggered.render(actual.data(), actual.size());
  fresh.render(expected.data(), expected.size());
  for (size_t n = 1000; n < actual.size(); ++n)
    CHECK(actual[n] == doctest::Approx(expected[n]).epsilon(0.001).scale(1));
}

TEST_CASE("Fatty switching from a long VCA attack to TRIG uses TRIG timing") {
  InstrumentBass808 instrument{};
  instrument.waveform = 1;
  instrument.attack = 255;
  instrument.decay = 1;
  instrument.sustain = 255;
  instrument.envelopeMode = 2;
  instrument.envelopeShape = 128;
  Bass808Voice voice;
  voice.init(48000);
  voice.configure(&instrument, 4500, 1, 20000, 0, 255);
  voice.noteOn();
  std::vector<float> output(4800);
  voice.render(output.data(), output.size());
  REQUIRE(voice.active());
  instrument.envelopeMode = 0;
  voice.configure(&instrument, 4500, 1, 20000, 0, 255);
  voice.render(output.data(), output.size());
  CHECK_FALSE(voice.active());
  CHECK(output.back() == 0.0f);
}

TEST_CASE("Fatty held oscillator glide is independent of tick and render block boundaries") {
  InstrumentBass808 instrument{};
  instrument.waveform = 1;
  instrument.subWaveform = 2;
  instrument.subLevel = instrument.drive = 255;
  instrument.decay = 0x41;
  instrument.sustain = 255;
  instrument.envelopeMode = 0;
  instrument.filterEnabled = 1;
  instrument.filterCharacter = 2;
  instrument.filterSlope24dB = 1;
  for (int shape : {1, 2, 3}) for (int rate : {22050, 44100, 48000, 96000}) {
    CAPTURE(shape);
    CAPTURE(rate);
    instrument.waveform = shape;
    if (shape != 1) {
      instrument.subLevel = 0;
      instrument.drive = 0x7f;
      instrument.decay = 0x40;
      instrument.sustain = 0;
    }
    Bass808Voice continuous, chunked;
    continuous.init((float)rate);
    chunked.init((float)rate);
    for (auto* voice : {&continuous, &chunked}) {
      voice->configure(&instrument, 4600, 1, 20000, 0x20, 255);
      voice->noteOn();
      float warmup[640];
      voice->render(warmup, 640);
      voice->configure(&instrument, 5800, 1, 20000, 0x20, 255);
    }
    std::vector<float> expected(rate * 2), actual(expected.size());
    continuous.render(expected.data(), expected.size());
    for (size_t offset = 0; offset < actual.size();) {
      const size_t count = std::min(size_t(1 + offset % 799), actual.size() - offset);
      chunked.configure(&instrument, 5800, 1, 20000, 0x20, 255);
      chunked.render(actual.data() + offset, count);
      offset += count;
    }
    CHECK(actual == expected);
  }
}

TEST_CASE("Fatty TRI sub and maximum drive glide have continuous mono retriggers") {
  InstrumentBass808 instrument{};
  instrument.waveform = 1;
  instrument.subWaveform = 2;
  instrument.subLevel = instrument.drive = instrument.glide = 255;
  instrument.decay = 0x41;
  instrument.sustain = 255;
  instrument.envelopeMode = 0;
  instrument.envelopeShape = 128;
  instrument.filterEnabled = 1;
  instrument.filterCharacter = 2;
  instrument.filterSlope24dB = 1;
  for (int rate : {22050, 44100, 48000, 96000}) {
    CAPTURE(rate);
    Bass808Voice voice;
    voice.init((float)rate);
    float previous = 0, maxStep = 0, triggerStep = 0;
    std::vector<float> output(257);
    for (int event = 0; event < 40; ++event) {
      voice.configure(&instrument, (float)(4500 + (event % 3) * 700), 1, 20000, 0x20, 255);
      voice.noteOn();
      for (int block = 0; block < 20 + event % 7; ++block) {
        voice.render(output.data(), output.size());
        for (size_t n = 0; n < output.size(); ++n) {
          REQUIRE(std::isfinite(output[n]));
          float step = std::fabs(output[n] - previous);
          if (block == 0 && n == 0) triggerStep = std::fmax(triggerStep, step);
          maxStep = std::fmax(maxStep, step);
          previous = output[n];
        }
      }
    }
    CAPTURE(maxStep);
    CAPTURE(triggerStep);
    CHECK(triggerStep < 0.015f);
    CHECK(maxStep < 0.25f * 22050.0f / rate);
  }
}

TEST_CASE("Fatty preserves Reip raw oscillator and drive output") {
  InstrumentBass808 instrument{};
  instrument.drive = 0x7f;
  instrument.envelopeMode = 2;
  instrument.sustain = 255;
  instrument.envelopeShape = 128;
  constexpr int size = 4096;
  constexpr float pi = 3.14159265358979323846f;
  const auto rawWave = [=](float p, int shape) {
    switch (shape) {
      case 1: return 1.0f - 4.0f * std::fabs(p - 0.5f);
      case 2: return 2.0f * p - 1.0f;
      case 3: return p < 0.5f ? 1.0f : -1.0f;
      default: return std::sin(2.0f * pi * p);
    }
  };
  for (bool sub : {false, true}) for (int shape : {0, 1, 2, 3}) for (int rate : {22050, 44100, 48000, 96000}) {
    CAPTURE(sub);
    CAPTURE(shape);
    CAPTURE(rate);
    instrument.waveform = shape;
    instrument.subWaveform = shape + 1;
    instrument.subLevel = sub ? 255 : 0;
    Bass808Voice voice;
    voice.init((float)rate);
    voice.configure(&instrument, 4500, 1, 20000, 0, 255);
    voice.noteOn();
    std::vector<float> audio(size);
    voice.render(audio.data(), audio.size());
    float mainPhase = 0, subPhase = 0;
    const float increment = 110.0f / rate;
    const float drive = 1.0f + instrument.drive / 255.0f * 5.5f;
    const float normalization = 1.0f / audioTanh(drive);
    for (int n = 0; n < size; ++n) {
      mainPhase += increment;
      mainPhase -= std::floor(mainPhase);
      subPhase += increment * 0.5f;
      subPhase -= std::floor(subPhase);
      const float source = rawWave(mainPhase, shape) +
        (sub ? rawWave(subPhase, shape) : 0.0f);
      const float expected = audioTanh(source * drive) * normalization * 0.7f;
      if (n >= rate * 0.012f)
        CHECK(audio[n] == doctest::Approx(expected).epsilon(0.00001).scale(1));
    }
  }
}

TEST_CASE("Fatty screenshot saw TRIG has continuous mono retriggers") {
  InstrumentBass808 instrument{};
  instrument.waveform = 2;
  instrument.subWaveform = 2;
  instrument.drive = 0x7f;
  instrument.decay = 0x40;
  instrument.envelopeMode = 0;
  instrument.envelopeShape = 128;
  instrument.filterEnabled = 1;
  instrument.filterCharacter = 2;
  instrument.filterSlope24dB = 1;
  for (int rate : {22050, 44100, 48000, 96000}) {
    CAPTURE(rate);
    Bass808Voice voice;
    voice.init((float)rate);
    float previous = 0, triggerStep = 0;
    std::vector<float> output(257);
    for (int event = 0; event < 40; ++event) {
      voice.configure(&instrument, (float)(4500 + (event % 3) * 700), 1, 20000, 0x20, 255);
      voice.noteOn();
      voice.render(output.data(), output.size());
      triggerStep = std::max(triggerStep, std::fabs(output[0] - previous));
      for (int block = 0; block < 20 + event % 7; ++block) {
        for (float sample : output) REQUIRE(std::isfinite(sample));
        previous = output.back();
        voice.render(output.data(), output.size());
      }
      previous = output.back();
    }
    CAPTURE(triggerStep);
    CHECK(triggerStep < 0.015f);
    output.resize(rate * 2);
    voice.render(output.data(), output.size());
    CHECK_FALSE(voice.active());
    CHECK(output.back() == 0.0f);
  }
}

TEST_CASE("Fatty screenshot VCA first note bridges silence across render boundaries") {
  InstrumentBass808 instrument{};
  instrument.waveform = 1;
  instrument.subWaveform = 3;
  instrument.subLevel = 0x70;
  instrument.drive = 0xbf;
  instrument.envelopeMode = 2;
  instrument.attack = 0;
  instrument.decay = 0x39;
  instrument.sustain = 0;
  instrument.release = 0x40;
  instrument.envelopeShape = 0x80;
  instrument.filterEnabled = 1;
  instrument.filterCharacter = 2;
  instrument.filterSlope24dB = 1;
  for (int rate : {22050, 44100, 48000, 96000})
    for (int pitch : {3300, 4500, 5700}) {
      CAPTURE(rate);
      CAPTURE(pitch);
      Bass808Voice continuous, chunked;
      for (auto* voice : {&continuous, &chunked}) {
        voice->init((float)rate);
        voice->configure(&instrument, (float)pitch, 1,
                         (uint16_t)filterCutoffFromControl(0x65), 0, 0x2f);
        voice->noteOn();
      }
      std::vector<float> expected(rate / 4), actual(expected.size());
      continuous.render(expected.data(), expected.size());
      for (size_t offset = 0; offset < actual.size();) {
        const size_t count = std::min(size_t(1 + offset % 257), actual.size() - offset);
        chunked.render(actual.data() + offset, count);
        offset += count;
      }
      CHECK(actual == expected);
      CHECK(std::fabs(actual.front()) < 0.015f);
      for (float sample : actual) REQUIRE(std::isfinite(sample));
    }
}

TEST_CASE("Fatty factory presets and Fatty FX names round trip") {
  fillFXNames();
  auto saved = std::make_unique<Project>();
  auto loaded = std::make_unique<Project>();
  projectInit(saved.get());
  projectInit(loaded.get());

  getInstrumentFunctions(InstrumentType::Bass808).init(&saved->instruments[0]);
  saved->instruments[0].chip.bass808.drive = 93;
  saved->instruments[0].chip.bass808.envelopeMode = 0;
  saved->tables[0].rows[0].fx[0].command = fxB80CF;
  saved->tables[0].rows[0].fx[0].value = 127;
  saved->tables[0].rows[1].fx[0].command = fxB80RS;
  saved->tables[0].rows[1].fx[0].value = 64;

  const char* path = "build/tests/fatty.cni";
  REQUIRE(instrumentSave(saved.get(), path, 0) == 0);
  std::ifstream input(path, std::ios::binary);
  REQUIRE(input.good());
  std::string contents((std::istreambuf_iterator<char>(input)), {});
  input.close();
  REQUIRE(contents.find("- Envelope mode: 0\n") != std::string::npos);
  contents.erase(contents.find("- Envelope mode: 0\n"), strlen("- Envelope mode: 0\n"));
  REQUIRE(instrumentLoadMemory(loaded.get(), reinterpret_cast<const uint8_t*>(contents.data()), contents.size(), 0) == 0);
  std::remove(path);
  CHECK(loaded->instruments[0].type == InstrumentType::Bass808);
  CHECK(loaded->instruments[0].chip.bass808.drive == 93);
  CHECK(loaded->instruments[0].chip.bass808.envelopeMode == 2);
  CHECK(loaded->tables[0].rows[0].fx[0].command == fxB80CF);
  CHECK(loaded->tables[0].rows[1].fx[0].command == fxB80RS);

  const char* presets[] = {
    "packaging/common/instruments/808 Pure.cni",
    "packaging/common/instruments/808 Drive.cni",
    "packaging/common/instruments/808 Glide.cni",
    "packaging/common/instruments/808 Saw Bass.cni"
  };
  for (const char* preset : presets) {
    REQUIRE(instrumentLoad(loaded.get(), preset, 0) == 0);
    CHECK(loaded->instruments[0].type == InstrumentType::Bass808);
    CHECK(loaded->instruments[0].chip.bass808.envelopeMode == 2);
  }

  projectFree(saved.get());
  projectFree(loaded.get());
}
