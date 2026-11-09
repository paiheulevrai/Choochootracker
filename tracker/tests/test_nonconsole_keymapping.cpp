// Compile the production editor without the console-only test/build flag.
#undef TEST_PORTMASTER_INPUT
#define screenKeyMapping screenNonConsoleKeyMapping
#include "../src/screens/screen_keymapping.cpp"
#undef screenKeyMapping
#include "doctest.h"
#include "app_ui_mock.h"

namespace {
struct NonConsoleMappingFixture {
  AppSettings previousSettings = appSettings;
  const AppScreen* previousScreen = currentScreen;
  ScreenData* previousScreenData = mockScreenData;
  void (*previousCallback)(InputCode, int) = inputRawCallback;

  NonConsoleMappingFixture() {
    appSettings.keyMapping = {};
    currentScreen = &screenNonConsoleKeyMapping;
    screenNonConsoleKeyMapping.setup(1);
    screenNonConsoleKeyMapping.fullRedraw();
  }
  ~NonConsoleMappingFixture() {
    appSettings = previousSettings;
    currentScreen = previousScreen;
    mockScreenData = previousScreenData;
    inputRawCallback = previousCallback;
    appResetInputState();
  }
};
}

TEST_CASE_FIXTURE(NonConsoleMappingFixture, "nonconsole mapping has no calibration or startup recovery") {
  CHECK(mockScreenData->rows == 12);
  CHECK(inputRawCallback == nullptr);
  mockScreenData->drawField(0, 11, CellState::focus);
  CHECK(std::string(mockGfxCells[13], 4) == "Done");
}

TEST_CASE_FIXTURE(NonConsoleMappingFixture, "nonconsole mapping applies on release without timed verification") {
  const InputCode key = {InputDeviceType::keyboard, 10005};
  mockScreenData->onEdit(0, 4, CellEditAction::tap);
  REQUIRE(inputRawCallback != nullptr);
  mockScreenData->drawStatic();
  CHECK(std::string(mockGfxCells[16], 16) == "Press any key...");
  inputRawCallback(key, 1);
  CHECK(appSettings.keyMapping.keyEdit[0].deviceType == InputDeviceType::none);
  inputRawCallback(key, 0);
  CHECK(inputRawCallback == nullptr);
  CHECK(appSettings.keyMapping.keyEdit[0].deviceType == InputDeviceType::keyboard);
  CHECK(appSettings.keyMapping.keyEdit[0].code == key.code);
}

TEST_CASE_FIXTURE(NonConsoleMappingFixture, "nonconsole mapping clears bindings immediately") {
  appSettings.keyMapping.keyEdit[1] = {InputDeviceType::gamepad, 1};
  mockScreenData->onEdit(1, 4, CellEditAction::clear);
  CHECK(appSettings.keyMapping.keyEdit[1].deviceType == InputDeviceType::none);
  CHECK(inputRawCallback == nullptr);
}
