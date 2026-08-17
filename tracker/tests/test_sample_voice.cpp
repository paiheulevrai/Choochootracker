#include "doctest.h"
#include "../../chipnomad_lib/project_instruments.h"
#include "../../chipnomad_lib/synth/sample_voice.h"

#include <cmath>
#include <cstdio>
#include <cstring>

static void writeU16(FILE* file, uint16_t value) {
  fputc(value & 0xff, file);
  fputc(value >> 8, file);
}

static void writeU32(FILE* file, uint32_t value) {
  writeU16(file, value & 0xffff);
  writeU16(file, value >> 16);
}

TEST_CASE("SampleVoice renders PCM16 with envelope and every filter mode") {
  int16_t pcm[512];
  for (int i = 0; i < 512; ++i) {
    pcm[i] = static_cast<int16_t>(std::sin(i * 0.1) * 24000.0);
  }

  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.sampleRate = 48000;
  sample.frameCount = 512;
  sample.channels = 1;
  sample.data = pcm;
  sample.end = 255;
  sample.filterEnabled = 1;
  sample.filterCutoffHz = 6000;
  sample.filterResonance = 64;
  sample.sustain = 255;

  for (int mode = 0; mode < 3; ++mode) {
    for (int slope24dB = 0; slope24dB < 2; ++slope24dB) {
      sample.filterMode = mode;
      sample.filterSlope24dB = slope24dB;
      SampleVoice voice;
      float output[256 * 2];
      voice.init(48000.0f);
      voice.configure(&sample, 0.0f, 1.0f, 100.0f, sample.start, sample.end, sample.loopMode,
                      sample.filterCutoffHz, sample.filterResonance);
      voice.noteOn();
      voice.render(output, 256);

      double energy = 0.0;
      for (float value : output) {
        CHECK(std::isfinite(value));
        energy += std::fabs(value);
      }
      CHECK(energy > 0.0);
    }
  }
}

TEST_CASE("SampleVoice start and end delimit playback") {
  int16_t pcm[64];
  for (int i = 0; i < 64; ++i) pcm[i] = 12000;

  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.sampleRate = 48000;
  sample.frameCount = 64;
  sample.channels = 1;
  sample.data = pcm;
  sample.start = 64;
  sample.end = 128;
  sample.loopMode = 1;
  sample.sustain = 255;

  SampleVoice voice;
  float output[128 * 2];
  voice.init(48000.0f);
  voice.configure(&sample, 0.0f, 1.0f, 100.0f, sample.start, sample.end, 0,
                  sample.filterCutoffHz, sample.filterResonance);
  voice.noteOn();
  voice.render(output, 128);
  CHECK_FALSE(voice.active());
}

TEST_CASE("Sample modulation exposes speed and loop destinations") {
  CHECK(instrumentModDestinationMax(InstrumentType::Sample) == 24);
  CHECK(std::strcmp(instrumentModDestinationName(InstrumentType::Sample, 5), "Speed") == 0);
  CHECK(std::strcmp(instrumentModDestinationName(InstrumentType::Sample, 6), "Loop") == 0);
}

TEST_CASE("SampleVoice loops and grain time keeps rendering") {
  int16_t pcm[64];
  for (int i = 0; i < 64; ++i) pcm[i] = i * 400 - 12000;
  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.sampleRate = 48000;
  sample.frameCount = 64;
  sample.channels = 1;
  sample.data = pcm;
  sample.end = 255;
  sample.loopMode = 1;
  sample.sustain = 255;

  SampleVoice voice;
  float output[256 * 2];
  voice.init(48000.0f);
  voice.configure(&sample, 0.0f, 1.0f, 1200.0f, sample.start, sample.end, sample.loopMode,
                  sample.filterCutoffHz, sample.filterResonance);
  voice.noteOn();
  voice.render(output, 256);
  CHECK(voice.active());
  for (float value : output) CHECK(std::isfinite(value));
}

TEST_CASE("Sample loader accepts unsigned PCM8 WAV") {
  const char* path = "build/tests/test_pcm8.wav";
  FILE* file = fopen(path, "wb");
  REQUIRE(file != nullptr);
  fwrite("RIFF", 1, 4, file); writeU32(file, 40);
  fwrite("WAVEfmt ", 1, 8, file); writeU32(file, 16);
  writeU16(file, 1); writeU16(file, 1); writeU32(file, 8000);
  writeU32(file, 8000); writeU16(file, 1); writeU16(file, 8);
  fwrite("data", 1, 4, file); writeU32(file, 4);
  const uint8_t pcm[] = {0, 64, 128, 255};
  fwrite(pcm, 1, sizeof(pcm), file);
  fclose(file);

  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  char error[64];
  CHECK(sampleLoadWav16(path, &sample, error, sizeof(error)) == 0);
  REQUIRE(sample.data != nullptr);
  CHECK(sample.frameCount == 4);
  CHECK(sample.data[0] == -32768);
  CHECK(sample.data[2] == 0);
  CHECK(sample.data[3] == 32512);
  free(sample.data);
  remove(path);
}
