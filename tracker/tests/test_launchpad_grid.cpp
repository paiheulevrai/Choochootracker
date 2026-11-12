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
  int sceneRow = -1;
  CHECK(launchpadGrid::songRowForSceneControl(89, 24, &sceneRow));
  CHECK(sceneRow == 24);
  CHECK(launchpadGrid::songRowForSceneControl(19, 24, &sceneRow));
  CHECK(sceneRow == 31);
  CHECK(launchpadGrid::sceneControlForVisibleRow(0) == 89);
  CHECK(launchpadGrid::sceneControlForVisibleRow(7) == 19);
  CHECK_FALSE(launchpadGrid::songRowForSceneControl(20, 24, &sceneRow));
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

TEST_CASE("Launchpad double presses track the same released control and reject held duplicates") {
  launchpadGrid::PressTracker presses;
  CHECK(presses.press(3, 20) == 1);
  CHECK(presses.press(3, 20) == 0); // duplicate Note On while held
  presses.release(3);
  CHECK(presses.press(3, 20) == 2);
  presses.release(3);
  CHECK(presses.press(4, 20) == 1); // different pad is a new sequence
  presses.release(4);
  presses.tick();
  CHECK(presses.press(4, 20) == 2);
  presses.release(4);
  for (int i = 0; i < 20; ++i) presses.tick();
  CHECK(presses.press(4, 20) == 1); // expired interval
  presses.reset();
  CHECK(presses.press(64, 20) == 1); // scene controls have separate identities
}

TEST_CASE("Launchpad LED state reflects playing, urgent queue, queued stop and screen scope") {
  using launchpadGrid::LedState;
  CHECK(launchpadGrid::ledState(false, false, false, false) == LedState::off);
  CHECK(launchpadGrid::ledState(true, false, false, false) == LedState::populated);
  CHECK(launchpadGrid::ledState(true, true, false, false) == LedState::queued);
  CHECK(launchpadGrid::ledState(true, true, true, false) == LedState::urgent);
  CHECK(launchpadGrid::ledState(false, true, true, false) == LedState::urgent); // queued stop
  CHECK(launchpadGrid::ledState(true, true, true, true) == LedState::playing);
  CHECK(launchpadGrid::colourForState(LedState::urgent) == launchpadGrid::kUrgentAmber);
  CHECK(launchpadGrid::isScreenActive(true, true));
  CHECK_FALSE(launchpadGrid::isScreenActive(false, true));
  CHECK_FALSE(launchpadGrid::isScreenActive(true, false));
}
