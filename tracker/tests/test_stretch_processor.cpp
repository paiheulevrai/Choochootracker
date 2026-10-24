#include "doctest.h"
#include "../../chipnomad_lib/synth/stretch_processor.h"
#include "../../chipnomad_lib/project_instruments.h"

#include <cmath>
#include <cstring>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
  stretch.configure(&sample, 0, 50.0f, 0.0f, 0, 255, sample.loopMode);
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
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255, sample.loopMode);
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
  stretch.configure(&sample, 1, 50.0f, 0.0f, 0, 255, sample.loopMode);
  stretch.noteOn();

  std::vector<float> scratch(8192 * 2);
  RenderResult result = renderAll(stretch, scratch.data(), 256);
  CHECK(result.finite);
  CHECK(result.deactivated);
  // Target 0.48 s = 23040 frames.
  CHECK(result.frames > 20000);
  CHECK(result.frames < 27000);
}

TEST_CASE("StretchProcessor speed mode follows speed, not clock") {
  auto data = makeSine(440.0, 48000, 1.0);
  InstrumentSample sample = makeSample(data, 48000);
  float scratch[256 * 2];

  auto renderSpeed = [&](uint16_t speedPercent, float tickRateHz) {
    StretchProcessor stretch;
    stretch.init(48000.0, false);
    stretch.configure(&sample, 0, speedPercent, tickRateHz, 0.0f, 0, 255, sample.loopMode);
    stretch.noteOn();
    return renderAll(stretch, scratch, 256).frames;
  };

  size_t slow = renderSpeed(50, 50.0f);
  size_t normal = renderSpeed(100, 50.0f);
  size_t fast = renderSpeed(200, 50.0f);
  size_t normalAtAnotherClock = renderSpeed(100, 100.0f);

  CHECK(slow > normal * 17 / 10);
  CHECK(fast < normal * 7 / 10);
  CHECK(std::abs((long long)normal - (long long)normalAtAnotherClock) < 512);
}

TEST_CASE("StretchProcessor follows a tempo change mid-note") {
  auto data = makeSine(440.0, 48000, 1.0);
  InstrumentSample sample = makeSample(data, 48000);
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255, sample.loopMode);
  stretch.noteOn();

  std::vector<float> scratch(8192 * 2);
  // Render half a bar, then double the tempo (half the target duration).
  for (int i = 0; i < 180; ++i) stretch.process(scratch.data(), 256);
  stretch.configure(&sample, 3, 100.0f, 0.0f, 0, 255, sample.loopMode);
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
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255, sample.loopMode);
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
  sample.loopMode = 3;
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255, sample.loopMode);
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

TEST_CASE("StretchProcessor ping-pong reverses across per-tick reconfigures") {
  // Regression: the engine reconfigures every tick. The old code reset the
  // runtime direction from the marker order on each configure, so the
  // ping-pong flip at the window end was undone and the playhead re-seeked
  // to the start - the note looped forward forever and never reversed.
  auto data = makeSine(440.0, 48000, 0.25);
  InstrumentSample sample = makeSample(data, 48000);
  sample.loopMode = 3;
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  // Speed mode 200% consumes the 0.25 s window quickly, so several direction
  // flips happen within the iteration budget.
  stretch.configure(&sample, 0, 200, 50.0f, 0.0f, 0, 255, sample.loopMode);
  stretch.noteOn();

  std::vector<float> scratch(8192 * 2);
  double maxSeen = 0.0;
  bool descended = false;
  bool roseAgain = false;
  double minAfterDescend = 1e30;
  for (int i = 0; i < 600; ++i) {
    REQUIRE(stretch.active());
    // Simulate the engine's per-tick configure call.
    stretch.configure(&sample, 0, 200, 50.0f, 0.0f, 0, 255, sample.loopMode);
    stretch.process(scratch.data(), 256);
    for (size_t j = 0; j < 256 * 2; ++j) CHECK(std::isfinite(scratch[j]));
    double position = stretch.sourcePosition();
    if (position > maxSeen) maxSeen = position;
    if (maxSeen > 0.8 * (double)sample.frameCount && position < maxSeen - 16.0) {
      descended = true;
      if (position < minAfterDescend) minAfterDescend = position;
      if (minAfterDescend < 0.5 * (double)sample.frameCount &&
          position > minAfterDescend + 16.0) roseAgain = true;
    }
  }
  // The playhead must have reached the window end, come back down (reverse
  // pass) and risen again (forward pass) - a full ping-pong cycle.
  CHECK(maxSeen > 0.8 * (double)sample.frameCount);
  CHECK(descended);
  CHECK(roseAgain);
}

TEST_CASE("StretchProcessor uses the passed loop mode, not the instrument field") {
  // SPL 02/03 override the instrument's loop setting; the engine passes the
  // effective mode to configure(). The instrument field says one-shot here,
  // but the passed ping-pong mode must keep the note alive.
  auto data = makeSine(440.0, 48000, 0.25);
  InstrumentSample sample = makeSample(data, 48000);
  sample.loopMode = 0;
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255, 2);
  stretch.noteOn();

  std::vector<float> scratch(8192 * 2);
  // A one-shot 1-bar stretch ends after ~92160 frames (360 chunks); 400
  // chunks must still be active when ping-pong is in effect.
  for (int i = 0; i < 400; ++i) {
    REQUIRE(stretch.active());
    stretch.process(scratch.data(), 256);
    for (size_t j = 0; j < 256 * 2; ++j) CHECK(std::isfinite(scratch[j]));
  }
}

TEST_CASE("StretchProcessor reverse window plays and drains") {
  auto data = makeSine(440.0, 48000, 0.5);
  InstrumentSample sample = makeSample(data, 48000);
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  // Start > End: reverse playback of the whole sample.
  stretch.configure(&sample, 3, 50.0f, 0.0f, 255, 0, sample.loopMode);
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
  stretch.configure(&sample, 3, 50.0f, 12.0f, 0, 255, sample.loopMode);
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
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255, sample.loopMode);
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
  stretch.configure(nullptr, 3, 50.0f, 0.0f, 0, 255, 0);
  stretch.noteOn();
  CHECK_FALSE(stretch.active());
}

TEST_CASE("StretchProcessor per-tick reconfigure preserves the running note") {
  auto data = makeSine(440.0, 48000, 0.25);
  InstrumentSample sample = makeSample(data, 48000);
  sample.loopMode = 1;
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255, sample.loopMode);
  stretch.noteOn();

  std::vector<float> scratch(8192 * 2);
  stretch.process(scratch.data(), 256);
  // Simulate the per-tick configure calls the engine makes.
  for (int i = 0; i < 50; ++i) {
    stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255, sample.loopMode);
    CHECK(stretch.active());
    stretch.process(scratch.data(), 256);
  }
}

TEST_CASE("StretchProcessor loop wraps at the loop start marker") {
  // loopStart 0x40 maps to frame 12046 of 48000 (sampleMarkerToStartFrame:
  // 64*47999/255). With loop mode 1 the stretched source cursor must wrap
  // back to the marker frame, not the window start (0).
  auto data = makeSine(440.0, 48000, 1.0);
  InstrumentSample sample = makeSample(data, 48000);
  sample.loopMode = 1;
  sample.loopStart = 0x40;
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255, sample.loopMode, sample.loopStart);
  stretch.noteOn();

  std::vector<float> scratch(8192 * 2);
  // Stretch mode 3 = 1 bar = 96 ticks at 50 Hz = 1.92 s of output from a
  // 1 s source: the cursor reaches the window end (48000) well before the
  // note ends. After the first wrap the position must stay above the
  // marker frame (12046) - the old code folded all the way down to 0.
  // The FIRST pass now starts at the region start (the priming seek is
  // skipped when it would land past the marker), so the lower bound only
  // applies once the cursor has wrapped.
  bool wrapped = false;
  double minSeen = 1e18;
  for (int i = 0; i < 400; ++i) {
    if (!stretch.active()) break;
    stretch.process(scratch.data(), 256);
    const double pos = stretch.sourcePosition();
    if (pos >= 47000.0) wrapped = true;
    if (wrapped) minSeen = pos < minSeen ? pos : minSeen;
    if (wrapped) CHECK(pos >= 12046.0 - 1.0);
  }
  CHECK(wrapped);
  CHECK(minSeen < 48000.0);
}

TEST_CASE("StretchProcessor ping-pong bounces at the loop start marker") {
  auto data = makeSine(440.0, 48000, 1.0);
  InstrumentSample sample = makeSample(data, 48000);
  sample.loopMode = 3;
  sample.loopStart = 0x40;
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255, sample.loopMode, sample.loopStart);
  stretch.noteOn();

  std::vector<float> scratch(8192 * 2);
  // After the cursor has reached the top and reversed, the bounce lower
  // bound is the marker frame (12046), never the window start (0).
  // The FIRST pass now starts at the region start (the priming seek is
  // skipped when it would land past the marker), so the lower bound only
  // applies once the cursor has reversed.
  bool reversed = false;
  double minSeen = 1e18;
  for (int i = 0; i < 400; ++i) {
    if (!stretch.active()) break;
    stretch.process(scratch.data(), 256);
    const double pos = stretch.sourcePosition();
    if (pos >= 47000.0) reversed = true;
    if (reversed) minSeen = pos < minSeen ? pos : minSeen;
    if (reversed) CHECK(pos >= 12046.0 - 1.0);
  }
  CHECK(reversed);
  CHECK(minSeen < 48000.0);
}

TEST_CASE("StretchProcessor short sample starts the first pass at the region start") {
  // Regression: with a clamped priming seek (window smaller than the seek
  // length) the playhead parked at the window edge and the wrap branches
  // folded it straight to the loop start marker - the first pass never
  // played from the region start. The seek must be skipped instead so the
  // playhead starts at the region edge.
  const uint32_t frameCount = 256;
  std::vector<int16_t> data(frameCount);
  for (uint32_t i = 0; i < frameCount; ++i) data[i] = (int16_t)(i * 100);
  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  sample.sampleRate = 44100;
  sample.frameCount = frameCount;
  sample.channels = 1;
  sample.data = data.data();
  sample.start = 0;
  sample.end = 255;
  sample.speedPercent = 100;
  sample.loopMode = 1;
  sample.loopStart = 0x40;  // frame 64 of 256

  StretchProcessor stretch;
  stretch.init(44100.0, false);
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255, sample.loopMode, sample.loopStart);
  stretch.noteOn();
  // The playhead must sit at the region start, not at the window edge (255)
  // and not at the loop marker (64).
  CHECK(stretch.sourcePosition() == 0.0);

  std::vector<float> scratch(8192 * 2);
  // The first pass sweeps the whole window before any wrap: the position
  // must climb past the loop marker (64) and stay below the window end
  // while the first pass is still running.
  bool climbedPastMarker = false;
  for (int i = 0; i < 40; ++i) {
    if (!stretch.active()) break;
    stretch.process(scratch.data(), 256);
    const double pos = stretch.sourcePosition();
    if (pos > 64.0) climbedPastMarker = true;
    CHECK(pos >= 0.0);
    CHECK(pos < 256.0);
  }
  CHECK(climbedPastMarker);
}

TEST_CASE("StretchProcessor seek landing past the loop marker is skipped") {
  // A seek that fits the window but lands at/past the loop start marker
  // would make playback begin at the marker instead of the region marker.
  // The seek must be skipped and the playhead must start at the region edge.
  auto data = makeSine(440.0, 48000, 1.0);
  InstrumentSample sample = makeSample(data, 48000);
  sample.loopMode = 1;
  // Marker 0x1E -> frame 5646 of 48000; the unclamped seek at speed 100%
  // (output rate 48000) is 5760 frames, so the feed would land at 5760,
  // past the marker.
  sample.loopStart = 0x1E;
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(&sample, 0, 100, 50.0f, 0.0f, 0, 255, sample.loopMode, sample.loopStart);
  stretch.noteOn();
  CHECK(stretch.sourcePosition() == 0.0);

  std::vector<float> scratch(8192 * 2);
  // The first pass must climb through the whole region before wrapping.
  // The playhead may touch the window end (48000) exactly at the wrap.
  bool reachedEnd = false;
  for (int i = 0; i < 400; ++i) {
    if (!stretch.active()) break;
    stretch.process(scratch.data(), 256);
    const double pos = stretch.sourcePosition();
    if (pos >= 47000.0) reachedEnd = true;
    CHECK(pos >= 0.0);
    CHECK(pos <= 48000.0);
  }
  CHECK(reachedEnd);
}

TEST_CASE("StretchProcessor unclamped seek keeps the primed start") {
  // When the window can supply the full seek and the feed lands before the
  // loop marker, the seek stays: the playhead starts seekLength*step into
  // the region (the seeked material is rendered as the output's beginning,
  // so the first pass still starts at the region marker audibly).
  auto data = makeSine(440.0, 48000, 1.0);
  InstrumentSample sample = makeSample(data, 48000);
  sample.loopMode = 1;
  sample.loopStart = 0xC0;  // frame 36127 of 48000, far past the seek landing
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(&sample, 0, 100, 50.0f, 0.0f, 0, 255, sample.loopMode, sample.loopStart);
  stretch.noteOn();
  // Speed 100% at 48k/48k: seek 5760 frames, step 1.0 -> feedPos 5760.
  CHECK(stretch.sourcePosition() > 4000.0);
  CHECK(stretch.sourcePosition() < 6000.0);
}

TEST_CASE("StretchProcessor modes 2/4 map to loop/ping-pong with xfade") {
  // Modes 2 (Loop X) and 4 (Ping X) map to the plain wrap modes 1/3 with
  // the crossfade enabled: the note sustains and the output stays finite.
  auto data = makeSine(440.0, 48000, 0.25);
  for (uint8_t mode : {2, 4}) {
    InstrumentSample sample = makeSample(data, 48000);
    sample.loopMode = mode;
    StretchProcessor stretch;
    stretch.init(48000.0, false);
    stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255, sample.loopMode);
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
}

TEST_CASE("StretchProcessor mode 2 descending hard-wraps like mode 1") {
  // Legacy Start > End windows reverse the playback direction. A descending
  // forward loop (mode 1 or 2) must hard-wrap: the phase-aligned incoming
  // pass would read past the sample data. Only ping-pong (modes 3/4) arms
  // a fade on the way down.
  auto data = makeSine(440.0, 48000, 0.25);
  InstrumentSample sample = makeSample(data, 48000);
  sample.loopMode = 2;
  sample.start = 200; // descending window: start marker above end marker
  sample.end = 40;
  StretchProcessor stretch;
  stretch.init(48000.0, false);
  stretch.configure(&sample, 3, 50.0f, 0.0f, 0, 255, sample.loopMode);
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
