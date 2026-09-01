#include "doctest.h"
#include "synth/plaits_voice.h"

#include <cmath>
#include <vector>

TEST_CASE("Plaits renders all engines with finite output") {
  PlaitsVoice* voice = new PlaitsVoice();
  voice->init(96000.0f);
  std::vector<float> output(4096);
  for (int engine = 0; engine < 24; ++engine) {
    voice->configure(engine, 16384, 16384, 16384, 0, 0, 128, 128, 60.0f, 1.0f);
    voice->setEnvelope(0.0f, 0.0f, 1.0f, 0.0f);
    voice->noteOn();
    voice->render(output.data(), output.size());
    double energy = 0.0;
    for (float sample : output) {
      REQUIRE(std::isfinite(sample));
      energy += std::fabs(sample);
    }
    CHECK(energy > 0.0);
  }
  delete voice;
}

TEST_CASE("Trace 6-OP FM output through the tracker wrapper") {
  for (int engine = 2; engine <= 4; ++engine) {
    PlaitsVoice voice;
    std::vector<float> output(96000);
    voice.init(96000.0f);
    voice.configure(engine, 16384, 16384, 16384, 0, 0, 255, 255, 60.0f, 1.0f);
    voice.noteOn();
    voice.render(output.data(), output.size());
    double firstBlock = 0.0, first100ms = 0.0, tail = 0.0;
    for (size_t i = 0; i < output.size(); ++i) {
      const double sample = std::abs(output[i]);
      if (i < 4096) firstBlock += sample;
      if (i < 9600) first100ms += sample;
      if (i >= 9600) tail += sample;
    }
    CAPTURE(engine);
    CAPTURE(firstBlock);
    CAPTURE(first100ms);
    CAPTURE(tail);
    CHECK(firstBlock > 0.01);
    CHECK(first100ms > 0.01);
    CHECK(tail > 10.0);
  }
}

TEST_CASE("Plaits envelope routings produce finite audio") {
  PlaitsVoice voice;
  std::vector<float> output(4096);
  voice.init(96000.0f);
  voice.setEnvelope(0.0f, 0.0f, 1.0f, 0.0f);
  for (int mode = 0; mode < 3; ++mode) {
    voice.configure(8, 16384, 16384, 16384, 0, mode, 128, 128, 60.0f, 1.0f);
    voice.noteOn();
    voice.render(output.data(), output.size());
    double energy = 0.0;
    for (float sample : output) {
      REQUIRE(std::isfinite(sample));
      energy += std::fabs(sample);
    }
    CHECK(energy > 0.0);
  }
}

TEST_CASE("Plaits legacy LEVEL mode uses the click-free VCA envelope") {
  PlaitsVoice voice;
  std::vector<float> output(512);
  voice.init(96000.0f);
  voice.configure(8, 16384, 16384, 16384, 0, 1, 128, 128, 60.0f, 1.0f);
  voice.setEnvelope(0.0f, 0.0f, 1.0f, 0.001f);
  voice.noteOn();
  voice.render(output.data(), 128);
  voice.noteOff();
  voice.render(output.data(), output.size());

  CHECK_FALSE(voice.active());
  for (size_t i = 128; i < output.size(); ++i) {
    CHECK(output[i] == doctest::Approx(0.0f));
  }
}

TEST_CASE("Plaits retriggers after the previous internal envelope decays") {
  PlaitsVoice voice;
  std::vector<float> output(96000);
  voice.init(96000.0f);
  voice.configure(21, 16384, 16384, 16384, 0, 2, 0, 255, 60.0f, 1.0f);
  voice.setEnvelope(0.0f, 0.0f, 1.0f, 0.0f);

  voice.noteOn();
  voice.render(output.data(), output.size());
  voice.noteOn();
  voice.render(output.data(), 4096);

  double energy = 0.0;
  for (size_t i = 0; i < 4096; ++i) energy += std::fabs(output[i]);
  CHECK(energy > 1.0);
}

TEST_CASE("Plaits physical engines decay in TRIG mode") {
  for (int engine = 19; engine <= 20; ++engine) {
    PlaitsVoice voice;
    std::vector<float> output(96000 * 3);
    voice.init(96000.0f);
    voice.configure(engine, 16384, 16384, 16384, 0, 0, 0, 255, 60.0f, 1.0f);
    voice.noteOn();
    voice.render(output.data(), output.size());

    double head = 0.0, tail = 0.0;
    for (size_t i = 0; i < output.size(); ++i) {
      if (i < 9600) head += std::fabs(output[i]);
      if (i >= output.size() - 9600) tail += std::fabs(output[i]);
    }
    CAPTURE(engine);
    CHECK(head > 0.01);
    CHECK(tail < head * 0.1);
  }
}

TEST_CASE("Plaits FM closes after note off in TRIG mode") {
  auto tailEnergy = [](bool release) {
    PlaitsVoice voice;
    std::vector<float> output(96000);
    voice.init(96000.0f);
    voice.configure(2, 16384, 16384, 16384, 0, 0, 255, 255, 60.0f, 1.0f);
    voice.noteOn();
    voice.render(output.data(), 4096);
    if (release) voice.noteOff();
    voice.render(output.data(), output.size());
    double energy = 0.0;
    for (size_t i = output.size() - 4096; i < output.size(); ++i)
      energy += std::fabs(output[i]);
    return energy;
  };
  CHECK(tailEnergy(true) < tailEnergy(false) * 0.1);
}

TEST_CASE("Plaits VCA keeps the internal level open") {
  PlaitsVoice voice;
  std::vector<float> output(96000);
  voice.init(96000.0f);
  voice.configure(17, 16384, 16384, 16384, 0, 2, 128, 255, 60.0f, 1.0f);
  voice.setEnvelope(0.0f, 0.0f, 1.0f, 0.0f);
  voice.noteOn();
  voice.render(output.data(), output.size());

  double tailEnergy = 0.0;
  for (size_t i = output.size() - 4096; i < output.size(); ++i) {
    tailEnergy += std::fabs(output[i]);
  }
  CHECK(tailEnergy > 1.0);
}

TEST_CASE("Plaits VCA retrigger continues from the current envelope level") {
  PlaitsVoice voice;
  std::vector<float> output(4096);
  voice.init(96000.0f);
  voice.configure(17, 16384, 16384, 16384, 0, 2, 128, 255, 60.0f, 1.0f);
  voice.setEnvelope(0.0f, 0.0f, 1.0f, 0.0f);
  voice.noteOn();
  voice.render(output.data(), output.size());

  voice.setEnvelope(1.0f, 0.0f, 1.0f, 0.0f);
  voice.noteOn();
  voice.render(output.data(), 128);

  double energy = 0.0;
  for (size_t i = 0; i < 128; ++i) energy += std::fabs(output[i]);
  CHECK(energy > 1.0);
}

static double crossingFrequency(const std::vector<float>& output,
                                size_t start, double sampleRate) {
  double mean = 0.0;
  for (size_t i = start; i < output.size(); ++i) mean += output[i];
  mean /= output.size() - start;
  int crossings = 0;
  for (size_t i = start + 1; i < output.size(); ++i) {
    if (output[i - 1] < mean && output[i] >= mean) ++crossings;
  }
  return crossings * sampleRate / (output.size() - start);
}

TEST_CASE("Plaits pitch rises by one octave for twelve semitones") {
  PlaitsVoice voice;
  std::vector<float> low(48000), high(48000);
  voice.init(96000.0f);
  voice.setEnvelope(0.0f, 0.0f, 1.0f, 0.0f);
  voice.configure(8, 8192, 8192, 8192, 0, 0, 128, 128, 36.0f, 1.0f);
  voice.noteOn();
  voice.render(low.data(), low.size());
  voice.configure(8, 8192, 8192, 8192, 0, 0, 128, 128, 48.0f, 1.0f);
  voice.noteOn();
  voice.render(high.data(), high.size());
  double ratio = crossingFrequency(high, 4096, 96000.0) /
                 crossingFrequency(low, 4096, 96000.0);
  CHECK(ratio == doctest::Approx(2.0).epsilon(0.2));
}
