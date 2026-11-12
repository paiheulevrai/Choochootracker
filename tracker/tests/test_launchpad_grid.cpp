#include "launchpad_grid.h"
#include "doctest.h"

TEST_CASE("Launchpad Programmer Mode coordinates map to tracks and visible song rows") {
  int column = -1, visibleRow = -1;
  CHECK(launchpadGrid::noteFor(0, 0) == 81); // upper-left
  CHECK(launchpadGrid::noteFor(7, 0) == 88); // upper-right
  CHECK(launchpadGrid::noteFor(0, 7) == 11); // lower-left
  CHECK(launchpadGrid::noteFor(7, 7) == 18); // lower-right
  CHECK(launchpadGrid::coordinatesForNote(81, &column, &visibleRow));
  CHECK(column == 0);
  CHECK(visibleRow == 0);
  CHECK(launchpadGrid::coordinatesForNote(18, &column, &visibleRow));
  CHECK(column == 7);
  CHECK(visibleRow == 7);
  CHECK_FALSE(launchpadGrid::coordinatesForNote(19, &column, &visibleRow)); // side button
  CHECK(launchpadGrid::songRowForVisibleRow(24, 0) == 24);
  CHECK(launchpadGrid::songRowForVisibleRow(24, 7) == 31);
  CHECK(launchpadGrid::songRowForVisibleRow(32, 0) == 32); // viewport follows topRow
}

TEST_CASE("Launchpad LED states prioritize playing over queued over populated") {
  CHECK(launchpadGrid::colour(false, false, false) == launchpadGrid::kOff);
  CHECK(launchpadGrid::colour(true, false, false) == launchpadGrid::kBlue);
  CHECK(launchpadGrid::colour(true, true, false) == launchpadGrid::kAmber);
  CHECK(launchpadGrid::colour(true, true, true) == launchpadGrid::kGreen);
  CHECK(launchpadGrid::colour(false, false, true) == launchpadGrid::kGreen);
}

TEST_CASE("Launchpad Song layout maps the four Programmer Mode navigation keys") {
  using launchpadGrid::SongNavigation;
  CHECK(launchpadGrid::songNavigationForEvent(0xb0, 92, 127) == SongNavigation::up);
  CHECK(launchpadGrid::songNavigationForEvent(0xb0, 93, 127) == SongNavigation::down);
  CHECK(launchpadGrid::songNavigationForEvent(0xb0, 94, 127) == SongNavigation::left);
  CHECK(launchpadGrid::songNavigationForEvent(0xb0, 95, 127) == SongNavigation::right);
  CHECK(launchpadGrid::songNavigationForEvent(0xb0, 91, 127) == SongNavigation::none);
  CHECK(launchpadGrid::songNavigationForEvent(0xb0, 92, 0) == SongNavigation::none);
  CHECK(launchpadGrid::songNavigationForEvent(0x90, 92, 127) == SongNavigation::none);
}
