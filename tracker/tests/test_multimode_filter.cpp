#include "doctest.h"
#include "synth/multimode_filter.h"

#include <cmath>

TEST_CASE("filter cutoff control is exponential and round-trips") {
  CHECK(filterCutoffFromControl(0) == doctest::Approx(20.0f));
  CHECK(filterCutoffFromControl(255) == doctest::Approx(20000.0f));
  CHECK(filterCutoffFromControl(128) < 1000.0f);
  for (int value = 0; value <= 255; ++value) {
    CHECK(std::abs((int)filterControlFromCutoff(filterCutoffFromControl(value)) - value) <= 1);
  }
}
