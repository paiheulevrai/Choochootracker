#include "doctest.h"
#include "../../chipnomad_lib/project_instruments.h"
#include "../../chipnomad_lib/synth/sample_voice.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

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

TEST_CASE("Sample modulation exposes all sample parameter destinations") {
  CHECK(instrumentModDestinationMax(InstrumentType::Sample) == 33);
  CHECK(std::strcmp(instrumentModDestinationName(InstrumentType::Sample, 3), "Start") == 0);
  CHECK(std::strcmp(instrumentModDestinationName(InstrumentType::Sample, 4), "End") == 0);
  CHECK(std::strcmp(instrumentModDestinationName(InstrumentType::Sample, 5), "Speed") == 0);
  CHECK(std::strcmp(instrumentModDestinationName(InstrumentType::Sample, 6), "Loop") == 0);
  CHECK(std::strcmp(instrumentModDestinationName(InstrumentType::Sample, 7), "Cutoff") == 0);
  CHECK(std::strcmp(instrumentModDestinationName(InstrumentType::Sample, 8), "Reso") == 0);
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

TEST_CASE("sampleNormalizeSlice keeps even counts and rejects others") {
  CHECK(sampleNormalizeSlice(0) == 0);
  CHECK(sampleNormalizeSlice(2) == 2);
  CHECK(sampleNormalizeSlice(4) == 4);
  CHECK(sampleNormalizeSlice(8) == 8);
  CHECK(sampleNormalizeSlice(16) == 16);
  CHECK(sampleNormalizeSlice(32) == 32);
  CHECK(sampleNormalizeSlice(1) == 0);
  CHECK(sampleNormalizeSlice(6) == 0);
  CHECK(sampleNormalizeSlice(3) == 0);
  CHECK(sampleNormalizeSlice(64) == 0);
}

TEST_CASE("sampleSliceFrames splits the whole sample evenly and clamps extras") {
  const uint8_t counts[] = {2, 4, 8, 16, 32};
  for (uint8_t count : counts) {
    uint32_t previousEnd = 0;
    for (uint8_t index = 0; index < count; ++index) {
      uint32_t start = 0;
      uint32_t end = 0;
      sampleSliceFrames(256, count, index, &start, &end);
      CHECK(start == previousEnd);
      CHECK(end == (uint32_t)(index + 1) * 256 / count);
      CHECK(end > start);
      previousEnd = end;
    }
    CHECK(previousEnd == 256);
    uint32_t start = 0;
    uint32_t end = 0;
    sampleSliceFrames(256, count, 255, &start, &end);
    CHECK(start == (uint32_t)(count - 1) * 256 / count);
    CHECK(end == 256);
  }
}

TEST_CASE("SampleVoice Off uses Start/End and sliced notes use even windows") {
  int16_t pcm[32];
  for (int i = 0; i < 32; ++i) pcm[i] = static_cast<int16_t>(i * 1000);

  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.sampleRate = 48000;
  sample.frameCount = 32;
  sample.channels = 1;
  sample.data = pcm;
  sample.start = 64;
  sample.end = 128;
  sample.sustain = 255;
  sample.filterCutoffHz = 20000;

  SampleVoice voice;
  float output[8];
  voice.init(48000.0f);
  voice.configure(&sample, 0.0f, 1.0f, 100.0f, sample.start, sample.end, 0,
                  sample.filterCutoffHz, sample.filterResonance);
  voice.noteOn();
  voice.render(output, 4);
  CHECK(std::fabs(output[0] - pcm[(uint32_t)sample.start * 31 / 255] / 32768.0f) < 0.01f);

  // Slices divide the Start/End loop region (frames [7,16) here), not the
  // whole sample: 9 frames / 4 slices -> windows [7,9) [9,11) [11,13) [13,16).
  voice.configure(&sample, 1200.0f, 1.0f, 100.0f, sample.start, sample.end, 0,
                  sample.filterCutoffHz, sample.filterResonance, -1, -1, -1, -1, -1, 4, 1);
  voice.noteOn();
  voice.render(output, 4);
  CHECK(std::fabs(output[0] - pcm[9] / 32768.0f) < 0.01f);

  voice.configure(&sample, 0.0f, 1.0f, 100.0f, sample.start, sample.end, 0,
                  sample.filterCutoffHz, sample.filterResonance, -1, -1, -1, -1, -1, 4, 0);
  voice.noteOn();
  voice.render(output, 4);
  CHECK(std::fabs(output[0] - pcm[7] / 32768.0f) < 0.01f);

  voice.configure(&sample, 0.0f, 1.0f, 100.0f, sample.start, sample.end, 0,
                  sample.filterCutoffHz, sample.filterResonance, -1, -1, -1, -1, -1, 4, 3);
  voice.noteOn();
  voice.render(output, 4);
  CHECK(std::fabs(output[0] - pcm[13] / 32768.0f) < 0.01f);

  voice.configure(&sample, 0.0f, 1.0f, 100.0f, sample.start, sample.end, 0,
                  sample.filterCutoffHz, sample.filterResonance, -1, -1, -1, -1, -1, 4, 9);
  voice.noteOn();
  voice.render(output, 4);
  CHECK(std::fabs(output[0] - pcm[13] / 32768.0f) < 0.01f);
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

TEST_CASE("SampleVoice stretch mode 0 keeps plain playback behavior") {
  int16_t pcm[64];
  for (int i = 0; i < 64; ++i) pcm[i] = (int16_t)(i * 500);

  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.sampleRate = 48000;
  sample.frameCount = 64;
  sample.channels = 1;
  sample.data = pcm;
  sample.end = 255;
  sample.sustain = 255;

  SampleVoice voice;
  float output[128 * 2];
  voice.init(48000.0f);
  voice.configure(&sample, 0.0f, 1.0f, 100.0f, sample.start, sample.end, 0,
                  sample.filterCutoffHz, sample.filterResonance, -1, -1, -1, -1, -1, 0, 0, 0, 50.0f);
  voice.noteOn();
  voice.render(output, 128);
  // One-shot: 64 source frames at 1:1 rate, so the voice must stop.
  CHECK_FALSE(voice.active());
  CHECK(std::fabs(output[0] - pcm[0] / 32768.0f) < 0.01f);
}

TEST_CASE("SampleVoice stretch renders for the musical division duration") {
  int16_t pcm[48000];
  for (int i = 0; i < 48000; ++i) {
    pcm[i] = (int16_t)(std::sin(2.0 * M_PI * 440.0 * i / 48000.0) * 24000.0);
  }

  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.sampleRate = 48000;
  sample.frameCount = 48000;
  sample.channels = 1;
  sample.data = pcm;
  sample.end = 255;
  sample.sustain = 255;

  SampleVoice voice;
  std::vector<float> output(256 * 2);
  voice.init(48000.0f);
  // 1 bar (96 ticks) at tickRate 50 = 1.92 s target for a 1.0 s source.
  voice.configure(&sample, 0.0f, 1.0f, 100.0f, sample.start, sample.end, 0,
                  sample.filterCutoffHz, sample.filterResonance, -1, -1, -1, -1, -1, 0, 0, 3, 50.0f);
  voice.noteOn();

  size_t frames = 0;
  bool finite = true;
  for (int iteration = 0; iteration < 4096; ++iteration) {
    if (!voice.active()) break;
    voice.render(output.data(), 256);
    for (float value : output) {
      if (!std::isfinite(value)) finite = false;
    }
    frames += 256;
  }
  CHECK(finite);
  // Target 1.92 s = 92160 frames; the priming seek shortens the note
  // (measured ~87000 frames at the voice level).
  CHECK(frames > 80000);
  CHECK(frames < 98000);
}

TEST_CASE("SampleVoice stretch follows tempo change mid-note") {
  int16_t pcm[48000];
  for (int i = 0; i < 48000; ++i) {
    pcm[i] = (int16_t)(std::sin(2.0 * M_PI * 440.0 * i / 48000.0) * 24000.0);
  }

  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.sampleRate = 48000;
  sample.frameCount = 48000;
  sample.channels = 1;
  sample.data = pcm;
  sample.end = 255;
  sample.sustain = 255;

  SampleVoice voice;
  std::vector<float> output(256 * 2);
  voice.init(48000.0f);
  voice.configure(&sample, 0.0f, 1.0f, 100.0f, sample.start, sample.end, 0,
                  sample.filterCutoffHz, sample.filterResonance, -1, -1, -1, -1, -1, 0, 0, 3, 50.0f);
  voice.noteOn();

  for (int i = 0; i < 180; ++i) voice.render(output.data(), 256);
  // Double the tempo: the remaining duration halves.
  voice.configure(&sample, 0.0f, 1.0f, 100.0f, sample.start, sample.end, 0,
                  sample.filterCutoffHz, sample.filterResonance, -1, -1, -1, -1, -1, 0, 0, 3, 100.0f);
  CHECK(voice.active());

  size_t frames = 180 * 256;
  for (int iteration = 0; iteration < 4096; ++iteration) {
    if (!voice.active()) break;
    voice.render(output.data(), 256);
    frames += 256;
  }
  // Well under the 1.92 s (92160 frames) the original tempo would need
  // (measured ~69000 frames).
  CHECK(frames < 75000);
}

TEST_CASE("SampleVoice stretch with slices falls back to plain path") {
  int16_t pcm[64];
  for (int i = 0; i < 64; ++i) pcm[i] = (int16_t)(i * 500);

  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.sampleRate = 48000;
  sample.frameCount = 64;
  sample.channels = 1;
  sample.data = pcm;
  sample.end = 255;
  sample.sustain = 255;

  SampleVoice voice;
  float output[8];
  voice.init(48000.0f);
  // Stretch mode set but slices active: the plain path must win.
  voice.configure(&sample, 0.0f, 1.0f, 100.0f, sample.start, sample.end, 0,
                  sample.filterCutoffHz, sample.filterResonance, -1, -1, -1, -1, -1, 4, 0, 3, 50.0f);
  voice.noteOn();
  voice.render(output, 4);
  CHECK(std::fabs(output[0] - pcm[0] / 32768.0f) < 0.01f);
}

TEST_CASE("SampleVoice stretch kill stops output") {
  int16_t pcm[48000];
  for (int i = 0; i < 48000; ++i) {
    pcm[i] = (int16_t)(std::sin(2.0 * M_PI * 440.0 * i / 48000.0) * 24000.0);
  }

  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.sampleRate = 48000;
  sample.frameCount = 48000;
  sample.channels = 1;
  sample.data = pcm;
  sample.end = 255;
  sample.sustain = 255;

  SampleVoice voice;
  std::vector<float> output(256 * 2);
  voice.init(48000.0f);
  voice.configure(&sample, 0.0f, 1.0f, 100.0f, sample.start, sample.end, 1,
                  sample.filterCutoffHz, sample.filterResonance, -1, -1, -1, -1, -1, 0, 0, 3, 50.0f);
  voice.noteOn();
  voice.render(output.data(), 256);
  CHECK(voice.active());

  voice.kill();
  CHECK_FALSE(voice.active());
  voice.render(output.data(), 256);
  for (float value : output) CHECK(value == 0.0f);
}

TEST_CASE("SampleVoice stretch retrigger restarts the playhead") {
  // Tracks are monophonic: a new note must kill the previous stretched
  // playhead and restart from the top of the source window, so every note
  // keeps its full stretch length.
  int16_t pcm[48000];
  for (int i = 0; i < 48000; ++i) {
    pcm[i] = (int16_t)(std::sin(2.0 * M_PI * 440.0 * i / 48000.0) * 24000.0);
  }

  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.sampleRate = 48000;
  sample.frameCount = 48000;
  sample.channels = 1;
  sample.data = pcm;
  sample.end = 255;
  sample.sustain = 255;

  SampleVoice voice;
  std::vector<float> output(256 * 2);
  voice.init(48000.0f);
  // 1 bar (96 ticks) at tickRate 50 = 1.92 s target for a 1.0 s source.
  voice.configure(&sample, 0.0f, 1.0f, 100.0f, sample.start, sample.end, 0,
                  sample.filterCutoffHz, sample.filterResonance, -1, -1, -1, -1, -1, 0, 0, 3, 50.0f);
  voice.noteOn();

  // Play most of the stretched note (target 92160 frames).
  for (int i = 0; i < 300; ++i) voice.render(output.data(), 256);
  CHECK(voice.active());

  // Retrigger: the playhead must restart from the top of the window, so the
  // note lasts a full stretch length again from this point.
  voice.noteOn();
  CHECK(voice.active());

  size_t frames = 0;
  for (int iteration = 0; iteration < 4096; ++iteration) {
    if (!voice.active()) break;
    voice.render(output.data(), 256);
    frames += 256;
  }
  // The retriggered note must run for roughly its full stretched duration
  // (~87000 frames measured at the voice level), NOT the ~12000 frames that
  // remained of the old playhead if the retrigger were swallowed.
  CHECK(frames > 60000);
}
