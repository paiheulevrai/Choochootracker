#include "doctest.h"
#include "synth/track_tilt.h"

#include <cmath>

static float tiltRms(uint8_t value, float frequency) {
  TrackTilt tilt;
  tilt.init(48000.0f);
  float sum = 0.0f;
  for (int i = 0; i < 96000; ++i) {
    float input = sinf(6.283185307179586f * frequency * i / 48000.0f) * 0.1f;
    float output = tilt.process(input, 0, value, 1000);
    if (i >= 48000) sum += output * output;
  }
  return sqrtf(sum / 48000.0f);
}

TEST_CASE("Track tilt is transparent at its center value") {
  TrackTilt tilt;
  tilt.init(48000.0f);
  CHECK(tilt.process(0.375f, 0, 0x80, 1000) == doctest::Approx(0.375f));
}

TEST_CASE("Track tilt settles back to transparent at its center value") {
  TrackTilt settled, fresh;
  settled.init(48000.0f);
  fresh.init(48000.0f);
  for (int i = 0; i < 48000; ++i) settled.process(0.2f, 0, 0x00, 1000);
  for (int i = 0; i < 48000; ++i) settled.process(0.0f, 0, 0x80, 1000);
  CHECK(settled.process(0.375f, 0, 0x7f, 1000) ==
        doctest::Approx(fresh.process(0.375f, 0, 0x7f, 1000)));
}

TEST_CASE("Track tilt changes low and high frequencies in opposite directions") {
  CHECK(tiltRms(0x00, 100.0f) > tiltRms(0x00, 8000.0f));
  CHECK(tiltRms(0xff, 8000.0f) > tiltRms(0xff, 100.0f));
}

TEST_CASE("Track tilt overdrive remains bounded") {
  TrackTilt tilt;
  tilt.init(48000.0f);
  float output = 0.0f;
  for (int i = 0; i < 48000; ++i) output = tilt.process(2.0f, 0, 0x00, 1000);
  CHECK(std::fabs(output) <= 1.0f);
}
