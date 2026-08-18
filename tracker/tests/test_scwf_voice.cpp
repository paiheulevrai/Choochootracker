#include "doctest.h"
#include "../../chipnomad_lib/project_instruments.h"
#include "../../chipnomad_lib/synth/scwf_voice.h"

#include <cmath>
#include <cstring>
#include <vector>

TEST_CASE("2xSCWF mixes two forward-looping oscillators and keeps the detune scale musical") {
  int16_t a[] = {0, 16000, 0, -16000};
  int16_t b[] = {16000, 0, -16000, 0};
  InstrumentSCWF instrument;
  std::memset(&instrument, 0, sizeof(instrument));
  instrument.oscillator[0].data = a;
  instrument.oscillator[0].frameCount = 4;
  instrument.oscillator[0].channels = 1;
  instrument.oscillator[1].data = b;
  instrument.oscillator[1].frameCount = 4;
  instrument.oscillator[1].channels = 1;
  instrument.mix = 128;
  instrument.sustain = 255;

  SCWFVoice voice;
  float output[128 * 2];
  voice.init(48000.0f);
  voice.configure(&instrument, 6000.0f, 1.0f, 0, instrument.mix, 20000, 0);
  voice.noteOn();
  voice.render(output, 128);
  double energy = 0.0;
  for (float sample : output) { CHECK(std::isfinite(sample)); energy += std::fabs(sample); }
  CHECK(energy > 0.0);
  CHECK(scwfDetuneCents(1) == 1);
  CHECK(scwfDetuneCents(127) == 200);
  CHECK(scwfDetuneCents(128) == 300);
  CHECK(scwfDetuneCents(SCWF_DETUNE_MAX) == 2400);
  CHECK(scwfFrequencyHz(6900) == doctest::Approx(440.0));
}

TEST_CASE("2xSCWF reads a complete waveform once per requested period") {
  int16_t waveform[] = {-16000, 0, 16000, 0};
  InstrumentSCWF instrument;
  std::memset(&instrument, 0, sizeof(instrument));
  instrument.oscillator[0].data = waveform;
  instrument.oscillator[0].frameCount = 4;
  instrument.oscillator[0].channels = 1;
  instrument.sustain = 255;

  SCWFVoice voice;
  std::vector<float> output(48000 * 2);
  voice.init(48000.0f);
  voice.configure(&instrument, 5700.0f, 1.0f, 0, 0, 20000, 0); // A3 = 220 Hz
  voice.noteOn();
  voice.render(output.data(), 48000);

  int risingCrossings = 0;
  for (size_t i = 2; i < output.size(); i += 2)
    if (output[i - 2] < 0.0f && output[i] >= 0.0f) ++risingCrossings;
  CHECK(risingCrossings == doctest::Approx(220).epsilon(0.01));
}

TEST_CASE("BYOWTBL linearly interpolates between independently selected frames") {
  int16_t frames[] = {0, 0, 0, 0, 16000, 16000, 16000, 16000};
  InstrumentSCWF instrument{};
  instrument.oscillator[0].data = frames;
  instrument.oscillator[0].frameCount = 8;
  instrument.oscillator[0].channels = 1;
  instrument.sustain = 255;
  uint16_t frameSize[] = {4, 0};
  uint8_t frameIndex[] = {128, 0};
  SCWFVoice voice;
  float output[2]{};
  voice.init(48000.0f);
  voice.configure(&instrument, 6000.0f, 1.0f, 0, 0, 20000, 0, frameSize, frameIndex);
  voice.noteOn();
  voice.render(output, 1);
  CHECK(output[0] == doctest::Approx(8000.0 / 32768.0).epsilon(0.01));
}
