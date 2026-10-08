#include "doctest.h"
#include "../../chipnomad_lib/project_instruments.h"
#include "../../chipnomad_lib/synth/sample_voice.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
  CHECK(instrumentModDestinationMax(InstrumentType::Sample) == 53);
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

TEST_CASE("sampleNormalizeSlice accepts counts 1..64 and rejects 0 and overflow") {
  CHECK(sampleNormalizeSlice(0) == 0);
  CHECK(sampleNormalizeSlice(1) == 1);
  CHECK(sampleNormalizeSlice(2) == 2);
  CHECK(sampleNormalizeSlice(3) == 3);
  CHECK(sampleNormalizeSlice(4) == 4);
  CHECK(sampleNormalizeSlice(6) == 6);
  CHECK(sampleNormalizeSlice(8) == 8);
  CHECK(sampleNormalizeSlice(16) == 16);
  CHECK(sampleNormalizeSlice(32) == 32);
  CHECK(sampleNormalizeSlice(64) == 64);
  CHECK(sampleNormalizeSlice(65) == 0);
  CHECK(sampleNormalizeSlice(255) == 0);
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

TEST_CASE("Sample loader accepts signed PCM24 WAV") {
  const char* path = "build/tests/test_pcm24.wav";
  FILE* file = fopen(path, "wb");
  REQUIRE(file != nullptr);
  fwrite("RIFF", 1, 4, file); writeU32(file, 48);
  fwrite("WAVEfmt ", 1, 8, file); writeU32(file, 16);
  writeU16(file, 1); writeU16(file, 1); writeU32(file, 8000);
  writeU32(file, 24000); writeU16(file, 3); writeU16(file, 24);
  fwrite("data", 1, 4, file); writeU32(file, 12);
  // Little-endian 24-bit frames: -8388608, 8388607, 0, 4194304
  const uint8_t pcm[] = {
    0x00, 0x00, 0x80, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40,
  };
  fwrite(pcm, 1, sizeof(pcm), file);
  fclose(file);

  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  char error[64];
  CHECK(sampleLoadWav16(path, &sample, error, sizeof(error)) == 0);
  REQUIRE(sample.data != nullptr);
  CHECK(sample.frameCount == 4);
  CHECK(sample.data[0] == -32768);
  CHECK(sample.data[1] == 32767);
  CHECK(sample.data[2] == 0);
  CHECK(sample.data[3] == 16384);
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

TEST_CASE("Sample marker-to-frame helpers match the playback mapping") {
  // Empty sample: both helpers return 0
  CHECK(sampleMarkerToStartFrame(0, 128) == 0);
  CHECK(sampleMarkerToEndFrame(0, 128) == 0);

  // Single-frame sample: start maps to 0, end 255 covers the frame
  CHECK(sampleMarkerToStartFrame(1, 0) == 0);
  CHECK(sampleMarkerToStartFrame(1, 255) == 0);
  CHECK(sampleMarkerToEndFrame(1, 255) == 1);
  CHECK(sampleMarkerToEndFrame(1, 0) == 0);

  // Known mapping: 256 frames, start 128 -> frame 128 (start * 255 / 255)
  CHECK(sampleMarkerToStartFrame(256, 128) == 128);
  // end 127 -> frame 128 (half the sample), end 255 -> frameCount
  CHECK(sampleMarkerToEndFrame(256, 127) == 128);
  CHECK(sampleMarkerToEndFrame(256, 255) == 256);

  // Monotonicity across the full marker range on a large sample
  uint32_t frameCount = 100000;
  uint32_t previousStart = 0;
  uint32_t previousEnd = 0;
  for (int marker = 0; marker <= 255; ++marker) {
    uint32_t s = sampleMarkerToStartFrame(frameCount, (uint8_t)marker);
    uint32_t e = sampleMarkerToEndFrame(frameCount, (uint8_t)marker);
    CHECK(s >= previousStart);
    CHECK(e >= previousEnd);
    CHECK(s < frameCount);
    CHECK(e <= frameCount);
    previousStart = s;
    previousEnd = e;
  }
}

// --- Phase 1: generalized EQUAL mode + slice bounds editing --------------

// Fixture: a silent sample whose loop region covers [start, end). The
// helpers never touch sample->data, so playback tests set it separately.
static InstrumentSample makeSliceSample(uint32_t frameCount, uint8_t start, uint8_t end) {
  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.sampleRate = 48000;
  sample.frameCount = frameCount;
  sample.channels = 1;
  sample.start = start;
  sample.end = end;
  return sample;
}

TEST_CASE("sampleSliceInitEven divides the loop region evenly for any count") {
  // Full-sample loop (start 0, end 255) on 256 frames: bounds at i*256/N.
  InstrumentSample sample = makeSliceSample(256, 0, 255);

  uint8_t count = sampleSliceInitEven(&sample, sliceModeEqual, 3);
  CHECK(count == 3);
  CHECK(sample.slice == sampleEncodeSlice(sliceModeEqual, 3));
  CHECK(sample.sliceBounds[0] == 0);
  CHECK(sample.sliceBounds[1] == 85);
  CHECK(sample.sliceBounds[2] == 170);

  count = sampleSliceInitEven(&sample, sliceModeEqual, 5);
  CHECK(count == 5);
  CHECK(sample.sliceBounds[0] == 0);
  CHECK(sample.sliceBounds[1] == 51);
  CHECK(sample.sliceBounds[2] == 102);
  CHECK(sample.sliceBounds[3] == 153);
  CHECK(sample.sliceBounds[4] == 204);

  count = sampleSliceInitEven(&sample, sliceModeEqual, 64);
  CHECK(count == 64);
  CHECK(sample.sliceBounds[63] == 252);
  CHECK(sample.slice == sampleEncodeSlice(sliceModeEqual, 64));

  // Counts clamp into 1..64.
  count = sampleSliceInitEven(&sample, sliceModeEqual, 0);
  CHECK(count == 1);
  count = sampleSliceInitEven(&sample, sliceModeEqual, 200);
  CHECK(count == 64);

  // AUTO uses the same even division as its Phase 1 placeholder.
  count = sampleSliceInitEven(&sample, sliceModeAuto, 4);
  CHECK(count == 4);
  CHECK(sample.slice == sampleEncodeSlice(sliceModeAuto, 4));
  CHECK(sample.sliceBounds[2] == 128);

  // The loop region follows the Start/End markers: markers 64/128 on 256
  // frames map to frames 64 and 128, so N=4 divides [64, 128).
  InstrumentSample windowed = makeSliceSample(256, 64, 128);
  count = sampleSliceInitEven(&windowed, sliceModeEqual, 4);
  CHECK(count == 4);
  CHECK(windowed.sliceBounds[0] == 64);
  CHECK(windowed.sliceBounds[1] == 80);
  CHECK(windowed.sliceBounds[2] == 96);
  CHECK(windowed.sliceBounds[3] == 112);
}

TEST_CASE("sampleSliceInitLazy clears bounds and keeps one whole-loop slice") {
  InstrumentSample sample = makeSliceSample(256, 0, 255);
  sampleSliceInitEven(&sample, sliceModeEqual, 4);
  CHECK(sample.sliceBounds[3] == 192);

  uint8_t count = sampleSliceInitLazy(&sample);
  CHECK(count == 1);
  CHECK(sample.slice == sampleEncodeSlice(sliceModeLazy, 1));
  CHECK(sample.sliceBounds[0] == 0);
  CHECK(sample.sliceBounds[1] == 0);
  CHECK(sample.sliceBounds[3] == 0);
}

TEST_CASE("sampleSliceBoundCount reports the sentinel count per mode") {
  InstrumentSample sample = makeSliceSample(256, 0, 255);
  CHECK(sampleSliceBoundCount(&sample) == 0);

  sampleSliceInitEven(&sample, sliceModeEqual, 7);
  CHECK(sampleSliceBoundCount(&sample) == 7);

  sampleSliceInitEven(&sample, sliceModeAuto, 9);
  CHECK(sampleSliceBoundCount(&sample) == 9);

  sampleSliceInitLazy(&sample);
  CHECK(sampleSliceBoundCount(&sample) == 1);

  sample.slice = 0;
  CHECK(sampleSliceBoundCount(&sample) == 0);
}

TEST_CASE("sampleSliceNudge moves one bound and keeps the mode sticky") {
  InstrumentSample sample = makeSliceSample(256, 0, 255);
  sampleSliceInitEven(&sample, sliceModeEqual, 4);

  // Slice 1 starts at 64; +5 moves only that bound.
  int32_t frame = sampleSliceNudge(&sample, 1, 5);
  CHECK(frame == 69);
  CHECK(sample.sliceBounds[1] == 69);
  CHECK(sample.sliceBounds[0] == 0);
  CHECK(sample.sliceBounds[2] == 128);
  CHECK(sample.sliceBounds[3] == 192);
  CHECK(sample.slice == sampleEncodeSlice(sliceModeEqual, 4));

  // A bound cannot cross its neighbours: clamps to the next bound.
  frame = sampleSliceNudge(&sample, 1, 1000);
  CHECK(frame == 128);
  CHECK(sample.sliceBounds[1] == 128);

  // Clamps to the previous bound (0 for the first slice); a nudge that
  // leaves the bound unchanged reports -1.
  CHECK(sampleSliceNudge(&sample, 0, -50) == -1);
  CHECK(sample.sliceBounds[0] == 0);

  // The last slice's right edge is the loop end.
  frame = sampleSliceNudge(&sample, 3, 1000);
  CHECK(frame == 256);

  // Out-of-range and unsliced inputs fail.
  CHECK(sampleSliceNudge(&sample, 4, 1) == -1);
  sample.slice = 0;
  CHECK(sampleSliceNudge(&sample, 0, 1) == -1);
}

TEST_CASE("sampleSliceSplit inserts the midpoint bound and returns the right half") {
  InstrumentSample sample = makeSliceSample(256, 0, 255);
  sampleSliceInitEven(&sample, sliceModeEqual, 2);

  // Split slice 0 ([0,128)) at 64: count 3, current becomes slice 1.
  int newSlice = sampleSliceSplit(&sample, 0);
  CHECK(newSlice == 1);
  CHECK(sampleSliceBoundCount(&sample) == 3);
  CHECK(sample.sliceBounds[0] == 0);
  CHECK(sample.sliceBounds[1] == 64);
  CHECK(sample.sliceBounds[2] == 128);
  CHECK(sample.slice == sampleEncodeSlice(sliceModeEqual, 3));

  // Splitting the last slice uses the loop end as its right edge.
  newSlice = sampleSliceSplit(&sample, 2);
  CHECK(newSlice == 3);
  CHECK(sampleSliceBoundCount(&sample) == 4);
  CHECK(sample.sliceBounds[3] == 192);

  // A degenerate (empty) slice cannot be split: nudge slice 1's start to
  // the loop end minus one frame, leaving a single-frame slice.
  sampleSliceInitEven(&sample, sliceModeEqual, 2);
  CHECK(sampleSliceNudge(&sample, 1, 127) == 255);
  CHECK(sampleSliceSplit(&sample, 1) == -1);

  // Out-of-range and unsliced inputs fail.
  CHECK(sampleSliceSplit(&sample, 5) == -1);
  sample.slice = 0;
  CHECK(sampleSliceSplit(&sample, 0) == -1);
}

TEST_CASE("sampleSliceDelete removes one bound and keeps the mode sticky") {
  InstrumentSample sample = makeSliceSample(256, 0, 255);
  sampleSliceInitEven(&sample, sliceModeEqual, 3);

  // Delete slice 1: bounds shift down, the caller lands on slice 0.
  int next = sampleSliceDelete(&sample, 1);
  CHECK(next == 0);
  CHECK(sampleSliceBoundCount(&sample) == 2);
  CHECK(sample.sliceBounds[0] == 0);
  CHECK(sample.sliceBounds[1] == 170);
  CHECK(sample.sliceBounds[2] == 0);
  CHECK(sample.slice == sampleEncodeSlice(sliceModeEqual, 2));

  // Deleting the first slice keeps the caller at 0.
  next = sampleSliceDelete(&sample, 0);
  CHECK(next == 0);
  CHECK(sampleSliceBoundCount(&sample) == 1);
  CHECK(sample.sliceBounds[0] == 170);

  // Deleting the last remaining slice turns the mode off but keeps the
  // bounds in memory (toggling back restores them).
  InstrumentSample windowed = makeSliceSample(256, 64, 128);
  sampleSliceInitEven(&windowed, sliceModeEqual, 1);
  CHECK(windowed.sliceBounds[0] == 64);
  next = sampleSliceDelete(&windowed, 0);
  CHECK(next == -1);
  CHECK(windowed.slice == 0);
  CHECK(windowed.sliceBounds[0] == 64);

  // Out-of-range and unsliced inputs fail.
  CHECK(sampleSliceDelete(&sample, 5) == -1);
  sample.slice = 0;
  CHECK(sampleSliceDelete(&sample, 0) == -1);
}

TEST_CASE("Mode switches follow the universal editing model") {
  InstrumentSample sample = makeSliceSample(256, 0, 255);

  // EQUAL -> LAZY clears the bounds.
  sampleSliceInitEven(&sample, sliceModeEqual, 4);
  sampleSliceInitLazy(&sample);
  CHECK(sample.sliceBounds[0] == 0);
  CHECK(sample.sliceBounds[2] == 0);
  CHECK(sampleDecodeSliceMode(sample.slice) == sliceModeLazy);

  // LAZY -> EQUAL re-divides evenly (LAZY chops are discarded).
  uint8_t count = sampleSliceInitEven(&sample, sliceModeEqual, 4);
  CHECK(count == 4);
  CHECK(sample.sliceBounds[1] == 64);
  CHECK(sample.sliceBounds[3] == 192);

  // Editing operations keep the mode sticky: nudge/split/delete re-encode
  // the same mode, only the count changes.
  sampleSliceNudge(&sample, 1, 3);
  CHECK(sampleDecodeSliceMode(sample.slice) == sliceModeEqual);
  sampleSliceSplit(&sample, 0);
  CHECK(sampleDecodeSliceMode(sample.slice) == sliceModeEqual);
  sampleSliceDelete(&sample, 0);
  CHECK(sampleDecodeSliceMode(sample.slice) == sliceModeEqual);

  // Switching to Off keeps the bounds in memory.
  sampleSliceInitEven(&sample, sliceModeEqual, 4);
  CHECK(sample.sliceBounds[1] == 64);
  sample.slice = 0;
  CHECK(sample.sliceBounds[1] == 64);
  // ...and switching back re-divides (the sentinel lost the old bounds'
  // custom edits, so the initializer runs again).
  sampleSliceInitEven(&sample, sliceModeEqual, 4);
  CHECK(sample.sliceBounds[1] == 64);
}

TEST_CASE("sampleSliceInsertAtFrame inserts sorted and rejects duplicates") {
  InstrumentSample sample = makeSliceSample(256, 0, 255);
  sampleSliceInitEven(&sample, sliceModeEqual, 2);

  int at = sampleSliceInsertAtFrame(&sample, 64);
  CHECK(at == 1);
  CHECK(sampleSliceBoundCount(&sample) == 3);
  CHECK(sample.sliceBounds[1] == 64);
  CHECK(sample.sliceBounds[2] == 128);

  // Duplicate bounds are rejected.
  CHECK(sampleSliceInsertAtFrame(&sample, 64) == -1);
  CHECK(sampleSliceInsertAtFrame(&sample, 0) == -1);

  // Frames outside the loop region are rejected.
  CHECK(sampleSliceInsertAtFrame(&sample, 256) == -1);
  CHECK(sampleSliceInsertAtFrame(&sample, 999) == -1);

  // Insert before the first bound shifts it right.
  at = sampleSliceInsertAtFrame(&sample, 32);
  CHECK(at == 1);
  CHECK(sample.sliceBounds[0] == 0);
  CHECK(sample.sliceBounds[1] == 32);
  CHECK(sample.sliceBounds[2] == 64);

  sample.slice = 0;
  CHECK(sampleSliceInsertAtFrame(&sample, 32) == -1);
}

TEST_CASE("sampleSliceStartFrame reads bounds and falls back for legacy samples") {
  InstrumentSample sample = makeSliceSample(256, 0, 255);

  // Legacy sample: sentinel 8, bounds all zero -> even division fallback.
  sample.slice = 8;
  CHECK(sampleSliceStartFrame(&sample, 0, sample.start, sample.end) == 0);
  CHECK(sampleSliceStartFrame(&sample, 3, sample.start, sample.end) == 96);
  CHECK(sampleSliceStartFrame(&sample, 8, sample.start, sample.end) == -1);

  // Populated bounds win over the fallback.
  sampleSliceInitEven(&sample, sliceModeEqual, 4);
  sampleSliceNudge(&sample, 2, 10);
  CHECK(sampleSliceStartFrame(&sample, 2, sample.start, sample.end) == 138);
  CHECK(sampleSliceStartFrame(&sample, 0, sample.start, sample.end) == 0);

  sample.slice = 0;
  CHECK(sampleSliceStartFrame(&sample, 0, sample.start, sample.end) == -1);
}

TEST_CASE("SampleVoice plays stored slice bounds and legacy fallback identically") {
  int16_t pcm[32];
  for (int i = 0; i < 32; ++i) pcm[i] = static_cast<int16_t>(i * 1000);

  InstrumentSample sample = makeSliceSample(32, 0, 255);
  sample.data = pcm;
  sample.sustain = 255;
  sample.filterCutoffHz = 20000;

  SampleVoice voice;
  float output[8];
  voice.init(48000.0f);

  // Stored bounds: slice 1 of an even 4-way division starts at frame 8.
  sampleSliceInitEven(&sample, sliceModeEqual, 4);
  voice.configure(&sample, 0.0f, 1.0f, 100.0f, sample.start, sample.end, 0,
                  sample.filterCutoffHz, sample.filterResonance, -1, -1, -1, -1, -1, 4, 1);
  voice.noteOn();
  voice.render(output, 4);
  CHECK(std::fabs(output[0] - pcm[8] / 32768.0f) < 0.01f);

  // A nudged bound moves the playback window: slice 1 now starts at 11.
  CHECK(sampleSliceNudge(&sample, 1, 3) == 11);
  voice.configure(&sample, 0.0f, 1.0f, 100.0f, sample.start, sample.end, 0,
                  sample.filterCutoffHz, sample.filterResonance, -1, -1, -1, -1, -1, 4, 1);
  voice.noteOn();
  voice.render(output, 4);
  CHECK(std::fabs(output[0] - pcm[11] / 32768.0f) < 0.01f);

  // Legacy fallback: sentinel 8 with empty bounds divides the loop evenly
  // (slice 1 of 8 over 32 frames starts at frame 4).
  InstrumentSample legacy = makeSliceSample(32, 0, 255);
  legacy.data = pcm;
  legacy.sustain = 255;
  legacy.filterCutoffHz = 20000;
  legacy.slice = 8;
  voice.configure(&legacy, 0.0f, 1.0f, 100.0f, legacy.start, legacy.end, 0,
                  legacy.filterCutoffHz, legacy.filterResonance, -1, -1, -1, -1, -1, 8, 1);
  voice.noteOn();
  voice.render(output, 4);
  CHECK(std::fabs(output[0] - pcm[4] / 32768.0f) < 0.01f);

  // LAZY slices map chromatically like EQUAL/AUTO: with bounds present the
  // caller passes the decoded count and the voice plays the selected window.
  InstrumentSample lazy = makeSliceSample(32, 0, 255);
  lazy.data = pcm;
  lazy.sustain = 255;
  lazy.filterCutoffHz = 20000;
  sampleSliceInitLazy(&lazy);
  CHECK(sampleActsAsSliced(&lazy) == 1);
  // Slice 1 of 1 starts at frame 0 (single hand-placed slice covers all).
  voice.configure(&lazy, 0.0f, 1.0f, 100.0f, lazy.start, lazy.end, 0,
                  lazy.filterCutoffHz, lazy.filterResonance, -1, -1, -1, -1, -1, 1, 1);
  voice.noteOn();
  voice.render(output, 4);
  CHECK(std::fabs(output[0] - pcm[0] / 32768.0f) < 0.01f);

  // sliceBypass (kStartPhraseRowFull) still forces the whole region: the
  // caller passes sliceCount 0 and the voice plays the full sample.
  voice.configure(&lazy, 0.0f, 1.0f, 100.0f, lazy.start, lazy.end, 0,
                  lazy.filterCutoffHz, lazy.filterResonance, -1, -1, -1, -1, -1, 0, 0);
  voice.noteOn();
  voice.render(output, 4);
  CHECK(std::fabs(output[0] - pcm[0] / 32768.0f) < 0.01f);
}
