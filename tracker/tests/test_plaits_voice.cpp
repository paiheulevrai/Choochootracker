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
