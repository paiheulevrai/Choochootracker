#include "doctest.h"
#include "title_night_sky.h"
#include <memory>
#include <vector>

TEST_CASE("Night galaxy evolves, caches frames and closes its orbit exactly") {
  auto sky = std::make_unique<TitleNightSky>();
  REQUIRE(sky->render(0));
  const std::vector<uint32_t> first(std::begin(sky->pixels), std::end(sky->pixels));
  CHECK_FALSE(sky->render(5));
  REQUIRE(sky->render(1800));
  size_t changed = 0;
  for (size_t i = 0; i < first.size(); ++i) changed += first[i] != sky->pixels[i];
  CHECK(changed > 5000);
  REQUIRE(sky->render(14400));
  CHECK(std::equal(first.begin(), first.end(), std::begin(sky->pixels)));
  auto second = std::make_unique<TitleNightSky>();
  REQUIRE(second->render(0));
  CHECK(std::equal(first.begin(), first.end(), std::begin(second->pixels)));
  CHECK(std::all_of(first.begin(), first.end(), [](uint32_t p) { return (p >> 24) == 255; }));
}

TEST_CASE("Night galaxy spreads work over draws without mixing phases") {
  auto expected = std::make_unique<TitleNightSky>();
  REQUIRE(expected->render(6));
  auto sliced = std::make_unique<TitleNightSky>();
  for (int i = 0; i < 5; ++i) CHECK_FALSE(sliced->render(6 + i * 2, 38));
  REQUIRE(sliced->render(16, 38));
  CHECK(std::equal(std::begin(expected->pixels), std::end(expected->pixels), std::begin(sliced->pixels)));
  REQUIRE(expected->render(18));
  CHECK_FALSE(sliced->render(18, 75));
  CHECK_FALSE(sliced->render(20, 75));
  REQUIRE(sliced->render(22, 75));
  CHECK(std::equal(std::begin(expected->pixels), std::end(expected->pixels), std::begin(sliced->pixels)));
}
