#include "doctest.h"
#include "../../chipnomad_lib/project_instruments.h"
#include "../../chipnomad_lib/synth/sample_voice.h"
#include "../../chipnomad_lib/synth/sample_transient.h"
#include "../../chipnomad_lib/synth/sample_ops.h"

#include <cmath>
#include <cstring>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Synthetic material for spectral-flux detection tests: a sine carrier
// whose envelope pulses at known frames. Each pulse attacks sharply (the
// onset we want detected) and fades out over its final `fade` frames so
// the pulse END is not itself a broadband transient (an abrupt stop
// clicks, and the detector would rightly flag it).
static void makeOnsetSample(InstrumentSample* sample, std::vector<int16_t>& buffer,
                            uint32_t frameCount, const std::vector<uint32_t>& onsets,
                            const std::vector<uint32_t>& ends,
                            uint16_t amplitude = 24000, uint32_t fade = 480) {
  buffer.assign(frameCount, 0);
  for (uint32_t i = 0; i < frameCount; ++i) {
    float level = 0.0f;
    for (size_t k = 0; k < onsets.size(); ++k) {
      if (i >= onsets[k] && i < ends[k]) {
        level = 1.0f;
        const uint32_t fadeStart = ends[k] > fade ? ends[k] - fade : onsets[k];
        if (i >= fadeStart && ends[k] > fadeStart) {
          level = 1.0f - (float)(i - fadeStart) / (float)(ends[k] - fadeStart);
        }
      }
    }
    buffer[i] = (int16_t)(std::sin((double)i * 0.05) * amplitude * level);
  }
  std::memset(sample, 0, sizeof(*sample));
  sample->sampleRate = 48000;
  sample->frameCount = frameCount;
  sample->channels = 1;
  sample->data = buffer.data();
  sample->end = 255;
}

TEST_CASE("Transient detection finds synthetic onsets within tolerance") {
  const uint32_t frameCount = 48000; // 1 second at 48 kHz
  // Four separated pulses: each onset is followed by silence before the
  // next one, so every pulse is an independent transient.
  const std::vector<uint32_t> onsets = {4800, 16800, 28800, 40800};
  const std::vector<uint32_t> ends = {9600, 21600, 33600, 45600};
  InstrumentSample sample;
  std::vector<int16_t> buffer;
  makeOnsetSample(&sample, buffer, frameCount, onsets, ends);

  uint32_t frames[PROJECT_SAMPLE_MAX_SLICES];
  uint8_t count = 0;
  const int result = sampleDetectTransients(&sample, 0, frameCount, 8, 50, frames, &count);
  REQUIRE(result == 0);
  CHECK(count == 4);
  for (int i = 0; i < 4; ++i) {
    CHECK(frames[i] >= onsets[i] - 512);
    CHECK(frames[i] <= onsets[i] + 512);
  }
}

TEST_CASE("Silence yields zero onsets") {
  InstrumentSample sample;
  std::vector<int16_t> buffer;
  makeOnsetSample(&sample, buffer, 48000, {}, {});
  uint32_t frames[PROJECT_SAMPLE_MAX_SLICES];
  uint8_t count = 99;
  const int result = sampleDetectTransients(&sample, 0, 48000, 8, 99, frames, &count);
  REQUIRE(result == 0);
  CHECK(count == 0);
}

TEST_CASE("Single transient yields exactly one onset") {
  const uint32_t frameCount = 48000;
  InstrumentSample sample;
  std::vector<int16_t> buffer;
  makeOnsetSample(&sample, buffer, frameCount, {12000}, {24000});
  uint32_t frames[PROJECT_SAMPLE_MAX_SLICES];
  uint8_t count = 0;
  const int result = sampleDetectTransients(&sample, 0, frameCount, 8, 50, frames, &count);
  REQUIRE(result == 0);
  REQUIRE(count == 1);
  CHECK(frames[0] >= 12000 - 512);
  CHECK(frames[0] <= 12000 + 512);
}

TEST_CASE("Requested count caps at the strongest peaks") {
  const uint32_t frameCount = 48000;
  const std::vector<uint32_t> onsets = {4800, 16800, 28800, 40800};
  const std::vector<uint32_t> ends = {9600, 21600, 33600, 45600};
  InstrumentSample sample;
  std::vector<int16_t> buffer;
  makeOnsetSample(&sample, buffer, frameCount, onsets, ends);
  uint32_t frames[PROJECT_SAMPLE_MAX_SLICES];
  uint8_t count = 0;
  const int result = sampleDetectTransients(&sample, 0, frameCount, 2, 50, frames, &count);
  REQUIRE(result == 0);
  CHECK(count == 2);
  // Ascending order regardless of which peaks won.
  CHECK(frames[0] < frames[1]);
}

TEST_CASE("Higher sensitivity finds at least as many onsets") {
  const uint32_t frameCount = 48000;
  const std::vector<uint32_t> onsets = {4800, 16800, 28800, 40800};
  const std::vector<uint32_t> ends = {9600, 21600, 33600, 45600};
  InstrumentSample sample;
  std::vector<int16_t> buffer;
  makeOnsetSample(&sample, buffer, frameCount, onsets, ends);
  uint32_t framesLow[PROJECT_SAMPLE_MAX_SLICES];
  uint32_t framesHigh[PROJECT_SAMPLE_MAX_SLICES];
  uint8_t countLow = 0;
  uint8_t countHigh = 0;
  REQUIRE(sampleDetectTransients(&sample, 0, frameCount, 16, 5, framesLow, &countLow) == 0);
  REQUIRE(sampleDetectTransients(&sample, 0, frameCount, 16, 95, framesHigh, &countHigh) == 0);
  CHECK(countHigh >= countLow);
}

TEST_CASE("Detection respects the loop region") {
  const uint32_t frameCount = 48000;
  // Onsets at 4800 (inside region) and 28800 (outside region).
  const std::vector<uint32_t> onsets = {4800, 28800};
  const std::vector<uint32_t> ends = {9600, 33600};
  InstrumentSample sample;
  std::vector<int16_t> buffer;
  makeOnsetSample(&sample, buffer, frameCount, onsets, ends);
  uint32_t frames[PROJECT_SAMPLE_MAX_SLICES];
  uint8_t count = 0;
  const int result = sampleDetectTransients(&sample, 9600, 19200, 8, 50, frames, &count);
  REQUIRE(result == 0);
  CHECK(count == 0);
}

TEST_CASE("Stereo input matches the mono sum") {
  const uint32_t frameCount = 48000;
  const std::vector<uint32_t> onsets = {4800, 16800, 28800, 40800};
  const std::vector<uint32_t> ends = {9600, 21600, 33600, 45600};
  InstrumentSample mono;
  std::vector<int16_t> monoBuffer;
  makeOnsetSample(&mono, monoBuffer, frameCount, onsets, ends);

  std::vector<int16_t> stereoBuffer(frameCount * 2);
  for (uint32_t i = 0; i < frameCount; ++i) {
    stereoBuffer[i * 2] = monoBuffer[i];
    stereoBuffer[i * 2 + 1] = monoBuffer[i];
  }
  InstrumentSample stereo;
  std::memset(&stereo, 0, sizeof(stereo));
  stereo.sampleRate = 48000;
  stereo.frameCount = frameCount;
  stereo.channels = 2;
  stereo.data = stereoBuffer.data();
  stereo.end = 255;

  uint32_t framesMono[PROJECT_SAMPLE_MAX_SLICES];
  uint32_t framesStereo[PROJECT_SAMPLE_MAX_SLICES];
  uint8_t countMono = 0;
  uint8_t countStereo = 0;
  REQUIRE(sampleDetectTransients(&mono, 0, frameCount, 8, 50, framesMono, &countMono) == 0);
  REQUIRE(sampleDetectTransients(&stereo, 0, frameCount, 8, 50, framesStereo, &countStereo) == 0);
  CHECK(countStereo == countMono);
  for (int i = 0; i < countMono; ++i) {
    CHECK(framesStereo[i] == framesMono[i]);
  }
}

TEST_CASE("Detection edge cases: empty, tiny and invalid inputs") {
  InstrumentSample sample;
  std::vector<int16_t> buffer;
  makeOnsetSample(&sample, buffer, 48000, {12000}, {24000});
  uint32_t frames[PROJECT_SAMPLE_MAX_SLICES];
  uint8_t count = 7;

  // Empty region.
  CHECK(sampleDetectTransients(&sample, 100, 100, 8, 50, frames, &count) != 0);
  CHECK(count == 0);
  // Inverted region is swapped, not rejected.
  count = 0;
  CHECK(sampleDetectTransients(&sample, 19200, 9600, 8, 50, frames, &count) == 0);
  // Region shorter than 64 frames reports zero onsets.
  count = 7;
  CHECK(sampleDetectTransients(&sample, 0, 32, 8, 50, frames, &count) == 0);
  CHECK(count == 0);
  // No data.
  InstrumentSample empty = sample;
  empty.data = NULL;
  count = 7;
  CHECK(sampleDetectTransients(&empty, 0, 48000, 8, 50, frames, &count) != 0);
  CHECK(count == 0);
  // Zero count requested.
  count = 7;
  CHECK(sampleDetectTransients(&sample, 0, 48000, 0, 50, frames, &count) == 0);
  CHECK(count == 0);
}

TEST_CASE("sampleSliceInitAuto stores detected bounds and sentinel") {
  const uint32_t frameCount = 48000;
  const std::vector<uint32_t> onsets = {4800, 16800, 28800, 40800};
  const std::vector<uint32_t> ends = {9600, 21600, 33600, 45600};
  InstrumentSample sample;
  std::vector<int16_t> buffer;
  makeOnsetSample(&sample, buffer, frameCount, onsets, ends);
  sample.autoSensitivity = 50;

  const uint8_t count = sampleSliceInitAuto(&sample, 8);
  CHECK(count == 4);
  CHECK(sampleDecodeSliceMode(sample.slice) == sliceModeAuto);
  CHECK(sampleDecodeSliceCount(sample.slice) == 4);
  for (int i = 0; i < 4; ++i) {
    CHECK(sample.sliceBounds[i] >= onsets[i] - 512);
    CHECK(sample.sliceBounds[i] <= onsets[i] + 512);
  }
}

TEST_CASE("sampleSliceInitAuto falls back to even division on silence") {
  InstrumentSample sample;
  std::vector<int16_t> buffer;
  makeOnsetSample(&sample, buffer, 48000, {}, {});
  sample.autoSensitivity = 50;

  const uint8_t count = sampleSliceInitAuto(&sample, 4);
  CHECK(count == 4);
  CHECK(sampleDecodeSliceMode(sample.slice) == sliceModeAuto);
  CHECK(sampleDecodeSliceCount(sample.slice) == 4);
  // Even division of the whole sample.
  CHECK(sample.sliceBounds[0] == 0);
  CHECK(sample.sliceBounds[1] == 12000);
  CHECK(sample.sliceBounds[2] == 24000);
  CHECK(sample.sliceBounds[3] == 36000);
}

TEST_CASE("sampleSliceInitAuto caps count at 16-frame minimum slices") {
  // 640 frames: cap = 640 / 16 = 40 slices max.
  InstrumentSample sample;
  std::vector<int16_t> buffer;
  makeOnsetSample(&sample, buffer, 640, {100, 300, 500}, {200, 400, 600});
  sample.autoSensitivity = 99;

  const uint8_t count = sampleSliceInitAuto(&sample, 64);
  CHECK(count <= 40);
  CHECK(sampleDecodeSliceCount(sample.slice) == count);
}

TEST_CASE("Undo restores slice state after detection") {
  const uint32_t frameCount = 48000;
  const std::vector<uint32_t> onsets = {4800, 16800, 28800, 40800};
  const std::vector<uint32_t> ends = {9600, 21600, 33600, 45600};
  // The undo API swaps heap buffers, so the sample data must be malloc'd
  // (a vector-owned buffer would be freed twice by the swap + FreeUndo).
  int16_t* buffer = (int16_t*)malloc((size_t)frameCount * sizeof(int16_t));
  REQUIRE(buffer != NULL);
  for (uint32_t i = 0; i < frameCount; ++i) {
    float level = 0.0f;
    for (size_t k = 0; k < onsets.size(); ++k) {
      if (i >= onsets[k] && i < ends[k]) {
        level = 1.0f;
        const uint32_t fadeStart = ends[k] > 480 ? ends[k] - 480 : onsets[k];
        if (i >= fadeStart && ends[k] > fadeStart) {
          level = 1.0f - (float)(i - fadeStart) / (float)(ends[k] - fadeStart);
        }
      }
    }
    buffer[i] = (int16_t)(std::sin((double)i * 0.05) * 24000.0 * level);
  }
  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.sampleRate = 48000;
  sample.frameCount = frameCount;
  sample.channels = 1;
  sample.data = buffer;
  sample.end = 255;
  sample.autoSensitivity = 50;

  SampleUndo undo;
  std::memset(&undo, 0, sizeof(undo));
  REQUIRE(sampleOpPrepareUndo(&sample, &undo) == sampleOpOk);
  const uint8_t before = sample.slice;
  const uint8_t detected = sampleSliceInitAuto(&sample, 8);
  REQUIRE(detected == 4);
  CHECK(sample.slice != before);
  REQUIRE(sampleOpApplyUndo(&sample, &undo) == sampleOpOk);
  // The pre-detection sentinel (off) and bounds come back.
  CHECK(sample.slice == before);
  CHECK(sample.sliceBounds[0] == 0);
  CHECK(sample.autoSensitivity == 50);
  sampleOpFreeUndo(&undo); // frees the pre-undo buffer
  free(sample.data);       // the buffer the undo handed back to the sample
}
