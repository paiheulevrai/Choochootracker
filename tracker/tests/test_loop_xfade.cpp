// Unit tests for the LoopXfade crossfade core: gain table shape, fade
// trajectory continuity, and advance/takeIncoming handover. No engine
// integration here - SampleVoice wiring is covered in test_sample_fx.cpp.
#include "doctest.h"
#include "../../chipnomad_lib/synth/loop_xfade.h"

#include <cmath>

TEST_SUITE("loop_xfade") {

TEST_CASE("gains start at full outgoing and end at full incoming") {
  LoopXfade x;
  x.configure(100);
  float gOut, gIn;
  x.gains(0.0, gOut, gIn);
  CHECK(gOut == doctest::Approx(1.0f));
  CHECK(gIn == doctest::Approx(0.0f));
  x.gains(100.0, gOut, gIn);
  CHECK(gOut == doctest::Approx(0.0f));
  CHECK(gIn == doctest::Approx(1.0f));
}

TEST_CASE("gains are monotonic and power stays near constant") {
  LoopXfade x;
  x.configure(256);
  float prevOut = 2.0f, prevIn = -1.0f;
  for (int i = 0; i <= 256; ++i) {
    float gOut, gIn;
    x.gains((double)i, gOut, gIn);
    CHECK(gOut <= prevOut + 1e-6f);
    CHECK(gIn >= prevIn - 1e-6f);
    // Equal-power: gOut^2 + gIn^2 stays close to 1 across the fade.
    const float power = gOut * gOut + gIn * gIn;
    CHECK(power == doctest::Approx(1.0).epsilon(0.02));
    prevOut = gOut;
    prevIn = gIn;
  }
}

TEST_CASE("fade trajectory is continuous at both edges") {
  // Correlated material: outgoing reads L-F+i, incoming reads i. The mixed
  // output must start exactly at the outgoing level and end exactly at the
  // incoming level - no step at either fade edge. Mid-fade the mix glides
  // across the L-F offset (inherent to any crossfade of offset material);
  // the contract is continuity of the EDGES, not a flat slope.
  const double L = 800.0, F = 200.0;
  LoopXfade x;
  x.configure((uint32_t)F);
  double prev = L - F; // plain-path sample just before the fade starts
  double maxJump = 0.0;
  for (int i = 0; i <= (int)F; ++i) {
    float gOut, gIn;
    double out;
    if (i < (int)F) {
      x.gains((double)i, gOut, gIn);
      out = gOut * (L - F + i) + gIn * (double)i;
    } else {
      out = (double)i; // plain path resumes with the pure incoming read
    }
    const double jump = std::fabs(out - prev);
    if (jump > maxJump) maxJump = jump;
    prev = out;
  }
  // Edge continuity: the first mixed sample continues the outgoing level
  // (gIn ~ 0 there) and the handover sample continues the incoming level
  // (gOut ~ 0 there). The 256-step table leaves a quantization residual
  // proportional to the offset (600 LSB) times the residual gain (~0.006),
  // so the bounds account for it explicitly.
  float gOut, gIn;
  x.gains(1.0, gOut, gIn);
  const double first = gOut * (L - F + 1.0) + gIn;
  CHECK(std::fabs(first - (L - F + 1.0)) < 2.0);
  x.gains((double)(F - 1), gOut, gIn);
  const double last = gOut * (L - 1.0) + gIn * (double)(F - 1);
  CHECK(gOut < 0.01f); // handover is effectively the pure incoming read
  CHECK(std::fabs(last - (double)(F - 1)) < 600.0 * 0.01 + 1.0);
  // The worst single-sample jump across the whole fade is the glide slope
  // (offset * pi/2 / F), far below the 800 LSB hard-wrap step.
  CHECK(maxJump < 800.0 / 32.0);
}

TEST_CASE("fade spreads a hard seam step across the window") {
  // Discontinuous material: outgoing reads 800, incoming reads 0. A hard
  // wrap jumps 800 LSB in one sample; the fade must spread the transition
  // so no single-sample jump exceeds a small fraction of it.
  const double F = 200.0;
  LoopXfade x;
  x.configure((uint32_t)F);
  double prev = 800.0;
  double maxJump = 0.0;
  for (int i = 0; i <= (int)F; ++i) {
    float gOut, gIn;
    double out;
    if (i < (int)F) {
      x.gains((double)i, gOut, gIn);
      out = gOut * 800.0 + gIn * 0.0;
    } else {
      out = 0.0;
    }
    const double jump = std::fabs(out - prev);
    if (jump > maxJump) maxJump = jump;
    prev = out;
  }
  // Equal-power on uncorrelated material peaks at ~1.4x the step spread
  // over the window; per-sample it stays far below the 800 LSB hard cut.
  CHECK(maxJump < 800.0 / 32.0);
}

TEST_CASE("advance completes once and takeIncoming hands over") {
  LoopXfade x;
  x.configure(10);
  x.arm(100.0, 0.0, 1, 1);
  CHECK(x.active());
  int completions = 0;
  double position = -1.0;
  int direction = 0;
  for (int i = 0; i < 20; ++i) {
    if (!x.advance(1.0)) {
      ++completions;
      x.takeIncoming(&position, &direction);
      // advance() moves both pointers with the phase: after 10 steps of 1.0
      // the incoming pointer sits at 0 + 10*1 = 10.
      CHECK(position == doctest::Approx(10.0));
      CHECK(direction == 1);
      // Further advances stay inert.
      CHECK_FALSE(x.advance(1.0));
      break;
    }
  }
  CHECK(completions == 1);
  CHECK_FALSE(x.active());
}

TEST_CASE("disabled engine never arms or advances") {
  LoopXfade x;
  x.configure(0);
  CHECK_FALSE(x.enabled());
  x.arm(0.0, 0.0, 1, 1);
  CHECK_FALSE(x.active());
  CHECK_FALSE(x.advance(1.0));
}

TEST_CASE("reconfigure resets an in-flight fade") {
  LoopXfade x;
  x.configure(10);
  x.arm(0.0, 0.0, 1, 1);
  CHECK(x.active());
  x.configure(0);
  CHECK_FALSE(x.active());
  CHECK_FALSE(x.enabled());
}

}  // TEST_SUITE("loop_xfade")
