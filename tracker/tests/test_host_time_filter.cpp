#include "doctest.h"
#include "link/host_time_filter.h"

#include <cstdint>

// T29 verification: the HostTimeFilter's sliding-window least-squares
// regression must recover the linear mapping between sample counters and
// host times, even with small jitter, and must refuse to estimate until
// it has at least two distinct points.

namespace {

// Feed pairs following hostTime = base + sampleCounter * microsPerSample
// (+ jitter), one callback-sized step at a time, like the audio thread
// would.
void feedLinear(HostTimeFilter& filter, uint64_t base, double microsPerSample,
                int count, uint64_t stepSamples, int jitterMicros) {
  uint64_t counter = 0;
  for (int i = 0; i < count; ++i) {
    // Deterministic pseudo-jitter: alternate +/- around the ideal line.
    const int jitter = (jitterMicros && (i & 1)) ? jitterMicros : (jitterMicros ? -jitterMicros : 0);
    const uint64_t host = base + (uint64_t)((double)counter * microsPerSample) + (uint64_t)jitter;
    filter.add(counter, host);
    counter += stepSamples;
  }
}

}  // namespace

TEST_SUITE("Host time filter") {

TEST_CASE("Refuses to estimate before two pairs") {
  HostTimeFilter filter;
  uint64_t out = 0;
  CHECK_FALSE(filter.estimate(0, &out));
  filter.add(0, 1000000);
  CHECK_FALSE(filter.estimate(0, &out));
  // Two identical x values: no slope information (sxx == 0).
  filter.add(0, 1000100);
  CHECK_FALSE(filter.estimate(0, &out));
}

TEST_CASE("Recovers exact linear mapping without jitter") {
  HostTimeFilter filter;
  // 48 kHz: 48000 samples per second -> 1000000/48000 micros per sample.
  const double microsPerSample = 1000000.0 / 48000.0;
  feedLinear(filter, 5000000, microsPerSample, 8, 512, 0);
  uint64_t out = 0;
  REQUIRE(filter.estimate(0, &out));
  CHECK(out == doctest::Approx(5000000).epsilon(0.001));
  // One buffer (512 samples) later: base + 512 * microsPerSample.
  REQUIRE(filter.estimate(512, &out));
  CHECK(out == doctest::Approx(5000000.0 + 512.0 * microsPerSample).epsilon(0.001));
  // Half a second in: 24000 samples.
  REQUIRE(filter.estimate(24000, &out));
  CHECK(out == doctest::Approx(5000000.0 + 24000.0 * microsPerSample).epsilon(0.001));
}

TEST_CASE("Tolerates small jitter within the window") {
  HostTimeFilter filter;
  const double microsPerSample = 1000000.0 / 44100.0;
  feedLinear(filter, 10000000, microsPerSample, 32, 256, 40);
  uint64_t out = 0;
  REQUIRE(filter.estimate(0, &out));
  // 40 micros of jitter on a ~23 micros/sample slope: allow 1% slack.
  CHECK(out == doctest::Approx(10000000).epsilon(0.01));
  REQUIRE(filter.estimate(44100, &out));
  CHECK(out == doctest::Approx(10000000.0 + 44100.0 * microsPerSample).epsilon(0.01));
}

TEST_CASE("Sliding window forgets old pairs") {
  HostTimeFilter filter;
  const double microsPerSample = 1000000.0 / 48000.0;
  // Fill the 64-pair window with a 48 kHz slope.
  feedLinear(filter, 0, microsPerSample, HostTimeFilter::kWindow, 512, 0);
  // Then drift the slope (simulating a rate change without an explicit
  // reset): new pairs follow 2x the micros per sample.
  uint64_t counter = (uint64_t)(HostTimeFilter::kWindow) * 512;
  uint64_t host = (uint64_t)((double)counter * microsPerSample);
  for (int i = 0; i < HostTimeFilter::kWindow; ++i) {
    host += (uint64_t)(512.0 * microsPerSample * 2.0);
    counter += 512;
    filter.add(counter, host);
  }
  // The window is now entirely new pairs: the estimate must follow the
  // new slope, not a blend with the old one.
  uint64_t out = 0;
  REQUIRE(filter.estimate(counter, &out));
  CHECK(out == doctest::Approx((double)host).epsilon(0.001));
}

TEST_CASE("Reset clears the window") {
  HostTimeFilter filter;
  feedLinear(filter, 0, 1000000.0 / 48000.0, 8, 512, 0);
  filter.reset();
  uint64_t out = 0;
  CHECK_FALSE(filter.estimate(0, &out));
}

}
