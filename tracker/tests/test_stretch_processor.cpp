#include "doctest.h"
#include "../../chipnomad_lib/synth/stretch_processor.h"
#include "../../chipnomad_lib/project_instruments.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace {

// 1 second of a 440 Hz sine at 48 kHz, mono.
static std::vector<int16_t> makeSine(double freq, uint32_t sampleRate, double seconds) {
  size_t frames = (size_t)(sampleRate * seconds);
  std::vector<int16_t> data(frames);
  for (size_t i = 0; i < frames; ++i) {
    data[i] = (int16_t)(std::sin(2.0 * M_PI * freq * (double)i / sampleRate) * 24000.0);
  }
  return data;
}

static InstrumentSample makeSample(const std::vector<int16_t>& data, uint32_t sampleRate) {
  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.sampleRate = sampleRate;
  sample.frameCount = (uint32_t)data.size();
  sample.channels = 1;
  sample.data = const_cast<int16_t*>(data.data());
  sample.start = 0;
  sample.end = 255;
  sample.loopMode = 0;
  return sample;
}

// Renders until the voice deactivates (or a frame cap), returns total frames
// written and whether any non-finite sample appeared.
struct RenderResult {
  size_t frames = 0;
  bool finite = true;
  bool deactivated = false;
};

static RenderResult renderAll(StretchProcessor& stretch, float* scratch, size_t chunk) {
  RenderResult result;
  for (int iteration = 0; iteration < 4096; ++iteration) {
    if (!stretch.active()) { result.deactivated = true; break; }
    stretch.process(scratch, chunk);
    for (size_t i = 0; i < chunk * 2; ++i) {
      if (!std::isfinite(scratch[i])) result.finite = false;
    }
    result.frames += chunk;
  }
  return result;
}

}  // namespace

TEST_CASE("StretchProcessor mode 0 is inert") {
  auto data = makeSine(440.0, 48000, 1.0);
  InstrumentSample sample = makeSample(data, 48000);
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(&sample, 0, 50.0f, 0.0f, 0, 255);
  CHECK_FALSE(stretch.active());
  stretch.noteOn();
  CHECK_FALSE(stretch.active());
  float output[256 * 2];
  std::memset(output, 0, sizeof(output));
  stretch.process(output, 256);
  for (float value : output) CHECK(value == 0.0f);
}

TEST_CASE("StretchProcessor stretches 1 second to 1 bar at tickRate 50") {
  auto data = makeSine(440.0, 48000, 1.0);
  InstrumentSample sample = makeSample(data, 48000);
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  // 96 ticks at 50 Hz = 1.92 s target for a 1.0 s source.
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255);
  stretch.noteOn();
  CHECK(stretch.active());

  std::vector<float> scratch(8192 * 2);
  RenderResult result = renderAll(stretch, scratch.data(), 256);
  CHECK(result.finite);
  CHECK(result.deactivated);
  // Target 1.92 s = 92160 frames; allow stretcher latency/flush slack.
  CHECK(result.frames > 86000);
  CHECK(result.frames < 98000);
}

TEST_CASE("StretchProcessor stretches 1 second to 1 beat at tickRate 50") {
  auto data = makeSine(440.0, 48000, 1.0);
  InstrumentSample sample = makeSample(data, 48000);
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  // 24 ticks at 50 Hz = 0.48 s target for a 1.0 s source (compression).
  stretch.configure(&sample, 1, 50.0f, 0.0f, 0, 255);
  stretch.noteOn();

  std::vector<float> scratch(8192 * 2);
  RenderResult result = renderAll(stretch, scratch.data(), 256);
  CHECK(result.finite);
  CHECK(result.deactivated);
  // Target 0.48 s = 23040 frames.
  CHECK(result.frames > 20000);
  CHECK(result.frames < 27000);
}

TEST_CASE("StretchProcessor follows a tempo change mid-note") {
  auto data = makeSine(440.0, 48000, 1.0);
  InstrumentSample sample = makeSample(data, 48000);
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255);
  stretch.noteOn();

  std::vector<float> scratch(8192 * 2);
  // Render half a bar, then double the tempo (half the target duration).
  for (int i = 0; i < 180; ++i) stretch.process(scratch.data(), 256);
  stretch.configure(&sample, 3, 100.0f, 0.0f, 0, 255);
  // Reconfiguration must not kill the running note.
  CHECK(stretch.active());
  RenderResult result = renderAll(stretch, scratch.data(), 256);
  CHECK(result.finite);
  // Total should be well under the original 1.92 s target (measured ~69000).
  CHECK(result.frames < 75000);
}

TEST_CASE("StretchProcessor loop mode 1 sustains indefinitely") {
  auto data = makeSine(440.0, 48000, 0.25);
  InstrumentSample sample = makeSample(data, 48000);
  sample.loopMode = 1;
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255);
  stretch.noteOn();

  std::vector<float> scratch(8192 * 2);
  double energy = 0.0;
  for (int i = 0; i < 400; ++i) {
    REQUIRE(stretch.active());
    stretch.process(scratch.data(), 256);
    for (size_t j = 0; j < 256 * 2; ++j) {
      CHECK(std::isfinite(scratch[j]));
      energy += std::fabs(scratch[j]);
    }
  }
  // 400 * 256 = 102400 frames > the 0.25 s source, so it must have looped.
  CHECK(energy > 0.0);
}

TEST_CASE("StretchProcessor ping-pong loop sustains and stays finite") {
  auto data = makeSine(440.0, 48000, 0.25);
  InstrumentSample sample = makeSample(data, 48000);
  sample.loopMode = 2;
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255);
  stretch.noteOn();

  std::vector<float> scratch(8192 * 2);
  double energy = 0.0;
  for (int i = 0; i < 400; ++i) {
    REQUIRE(stretch.active());
    stretch.process(scratch.data(), 256);
    for (size_t j = 0; j < 256 * 2; ++j) {
      CHECK(std::isfinite(scratch[j]));
      energy += std::fabs(scratch[j]);
    }
  }
  CHECK(energy > 0.0);
}

TEST_CASE("StretchProcessor reverse window plays and drains") {
  auto data = makeSine(440.0, 48000, 0.5);
  InstrumentSample sample = makeSample(data, 48000);
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  // Start > End: reverse playback of the whole sample.
  stretch.configure(&sample, 3, 50.0f, 0.0f, 255, 0);
  stretch.noteOn();
  CHECK(stretch.active());

  std::vector<float> scratch(8192 * 2);
  RenderResult result = renderAll(stretch, scratch.data(), 256);
  CHECK(result.finite);
  CHECK(result.deactivated);
  // 0.5 s source stretched to 1.92 s; the priming seek shortens the note
  // (measured 81408 frames).
  CHECK(result.frames > 70000);
}

TEST_CASE("StretchProcessor pitch transpose keeps duration") {
  auto data = makeSine(440.0, 48000, 1.0);
  InstrumentSample sample = makeSample(data, 48000);
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(&sample, 3, 50.0f, 12.0f, 0, 255);
  stretch.noteOn();

  std::vector<float> scratch(8192 * 2);
  RenderResult result = renderAll(stretch, scratch.data(), 256);
  CHECK(result.finite);
  // +12 semitones must not change the stretched duration.
  CHECK(result.frames > 86000);
  CHECK(result.frames < 98000);
}

TEST_CASE("StretchProcessor reset deactivates immediately") {
  auto data = makeSine(440.0, 48000, 0.25);
  InstrumentSample sample = makeSample(data, 48000);
  sample.loopMode = 1;
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255);
  stretch.noteOn();
  CHECK(stretch.active());

  std::vector<float> scratch(8192 * 2);
  stretch.process(scratch.data(), 256);
  stretch.reset();
  CHECK_FALSE(stretch.active());
  CHECK_FALSE(stretch.primed());
  std::memset(scratch.data(), 0, scratch.size() * sizeof(float));
  stretch.process(scratch.data(), 256);
  for (float value : scratch) CHECK(value == 0.0f);
}

TEST_CASE("StretchProcessor configure with null sample is safe") {
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(nullptr, 3, 50.0f, 0.0f, 0, 255);
  stretch.noteOn();
  CHECK_FALSE(stretch.active());
}

TEST_CASE("StretchProcessor per-tick reconfigure preserves the running note") {
  auto data = makeSine(440.0, 48000, 0.25);
  InstrumentSample sample = makeSample(data, 48000);
  sample.loopMode = 1;
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255);
  stretch.noteOn();

  std::vector<float> scratch(8192 * 2);
  stretch.process(scratch.data(), 256);
  // Simulate the per-tick configure calls the engine makes.
  for (int i = 0; i < 50; ++i) {
    stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255);
    CHECK(stretch.active());
    stretch.process(scratch.data(), 256);
  }
}
