#include "doctest.h"

#include <algorithm>
#include <cmath>

#include "synth/audio_math.h"

TEST_CASE("audio tanh lookup stays close to libm") {
  float maximumError = 0.0f;
  for (int i = -32768; i <= 32768; ++i) {
    float x = i / 4096.0f;
    maximumError = std::max(maximumError, std::fabs(audioTanh(x) - std::tanh(x)));
  }
  CHECK(maximumError < 0.00003f);
  CHECK(audioTanh(-100.0f) == -1.0f);
  CHECK(audioTanh(100.0f) == 1.0f);
}
