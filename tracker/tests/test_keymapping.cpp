#include "doctest.h"
#include "app.h"
#include "app_ui_mock.h"
#include "corelib_file.h"
#include "screen_keymapping.h"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

extern uint32_t mockMainLoopTicks;

namespace {
const InputCode keys[] = {
  {InputDeviceType::keyboard, 10001}, {InputDeviceType::keyboard, 10002},
  {InputDeviceType::keyboard, 10003}, {InputDeviceType::keyboard, 10004},
  {InputDeviceType::keyboard, 10005}, {InputDeviceType::keyboard, 10006},
  {InputDeviceType::keyboard, 10007}, {InputDeviceType::keyboard, 10008},
};
const InputCode replacement = {InputDeviceType::gamepad, 201};
const InputCode duplicate = {InputDeviceType::keyboard, 202};

struct KeyMappingFixture {
  AppSettings previousSettings = appSettings;
  ChipNomadState* previousState = chipnomadState;
  const AppScreen* previousScreen = currentScreen;
  uint32_t previousTicks = mockMainLoopTicks;
  std::filesystem::path directory;
  std::filesystem::path settingsPath, markerPath, backupPath, markerBackupPath;
  KeyMapping original;

  KeyMappingFixture() {
    char defaultDirectory[PATH_LENGTH];
    REQUIRE(fileGetDefaultDirectory(defaultDirectory, sizeof(defaultDirectory)) == 0);
    directory = defaultDirectory;
    settingsPath = directory / "settings.txt";
    markerPath = directory / "configure-controls";
    std::string suffix = ".controls-test-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    backupPath = settingsPath.string() + suffix;
    markerBackupPath = markerPath.string() + suffix;
    if (std::filesystem::exists(settingsPath)) std::filesystem::rename(settingsPath, backupPath);
    if (std::filesystem::exists(markerPath)) std::filesystem::rename(markerPath, markerBackupPath);
    initDefaultAppSettings();
    appSettings.keyMapping = {};
    appSettings.keyMapping.keyUp[0] = keys[0];
    appSettings.keyMapping.keyDown[0] = keys[1];
    appSettings.keyMapping.keyLeft[0] = keys[2];
    appSettings.keyMapping.keyRight[0] = keys[3];
    appSettings.keyMapping.keyEdit[0] = keys[4];
    appSettings.keyMapping.keyOpt[0] = keys[5];
    appSettings.keyMapping.keyPlay[0] = keys[6];
    appSettings.keyMapping.keyShift[0] = keys[7];
    mockMainLoopTicks = 100;
    appSetup();
    original = appSettings.keyMapping;
    open();
  }
  ~KeyMappingFixture() {
    screenKeyMappingCancel();
    appCleanup();
    chipnomadState = previousState;
    appSettings = previousSettings;
    currentScreen = previousScreen;
    mockMainLoopTicks = previousTicks;
    std::filesystem::remove(settingsPath);
    std::filesystem::remove(markerPath);
    if (std::filesystem::exists(backupPath)) std::filesystem::rename(backupPath, settingsPath);
    if (std::filesystem::exists(markerBackupPath)) std::filesystem::rename(markerBackupPath, markerPath);
  }
  void open(int recovery = 0) {
    currentScreen = &screenKeyMapping;
    screenKeyMapping.setup(recovery);
    screenKeyMapping.fullRedraw();
    REQUIRE(mockScreenData != nullptr);
  }
  void advance(uint32_t ms) {
    mockMainLoopTicks += ms;
    appOnEvent({MainLoopEvent::tick, {}});
  }
  void raw(InputCode input, bool down) {
    MainLoopEventData event{};
    event.type = down ? MainLoopEvent::keyDown : MainLoopEvent::keyUp;
    event.data.input = input;
    appOnEvent(event);
  }
  void tap(InputCode input) {
    raw(input, true);
    raw(input, false);
    advance(150);
  }
  void edit(int row = 4, int col = 0) {
    REQUIRE(mockScreenData->onEdit(col, row, CellEditAction::tap) == 1);
    REQUIRE(inputRawCallback != nullptr);
    advance(150);
  }
  void prepareChange() {
    edit();
    tap(replacement);
    advance(150);
  }
  void verify(int replacedRow = 4, InputCode changed = replacement) {
    for (int row = 0; row < 8; ++row) tap(row == replacedRow ? changed : keys[row]);
  }
  bool unchanged() { return std::memcmp(&original, &appSettings.keyMapping, sizeof(original)) == 0; }
};
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping menu keeps Done last and calibration immediately above it") {
  REQUIRE(mockScreenData->rows == 13);
  mockScreenData->drawField(0, 11, CellState::focus);
  mockScreenData->drawField(0, 12, CellState::focus);
  CHECK(std::string(mockGfxCells[13], 18) == "Calibrate controls");
  CHECK(std::string(mockGfxCells[14], 4) == "Done");
  mockScreenData->cursorRow = 12;
  screenKeyMapping.onInput(1, keyOpt, 1);
  CHECK(currentScreen == &screenControlsSettings);
  CHECK(std::filesystem::exists(settingsPath));
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping capture waits for both input paths and a quiet release") {
  // Model the Edit event opening capture, with its controller duplicate
  // delivered afterwards. Neither is the user's replacement button.
  mockScreenData->cursorRow = 4;
  raw(keys[4], true);
  // The screen navigation mock does not dispatch edits, so open it explicitly.
  mockScreenData->onEdit(0, 4, CellEditAction::tap);
  raw(keys[4], true);
  raw(replacement, true);
  raw(keys[4], false);
  advance(500);
  CHECK(unchanged());
  raw(replacement, false);
  advance(149);
  raw(duplicate, true);
  raw(duplicate, false);
  advance(149);
  CHECK(unchanged());
  advance(1);
  tap(replacement);
  advance(150);
  verify();
  CHECK(appSettings.keyMapping.keyEdit[0].deviceType == replacement.deviceType);
  CHECK(appSettings.keyMapping.keyEdit[0].code == replacement.code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "unverified mappings never change live settings or saved settings") {
  prepareChange();
  CHECK(unchanged());
  REQUIRE(settingsSave() == 0);
  appSettings.keyMapping = {};
  REQUIRE(settingsLoad() == 0);
  CHECK(unchanged());
  tap(keys[0]);
  advance(14999);
  CHECK(inputRawCallback != nullptr);
  advance(1);
  CHECK(inputRawCallback == nullptr);
  CHECK(unchanged());
  CHECK(std::string(screenGetActiveMessage()).find("timed out") != std::string::npos);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping verification needs every core control and all releases") {
  prepareChange();
  tap(keys[5]); // Not Up: must not advance verification.
  CHECK(unchanged());
  for (int row = 0; row < 7; ++row) {
    raw(row == 4 ? replacement : keys[row], true);
    raw(duplicate, true);
    raw(row == 4 ? replacement : keys[row], false);
    advance(150);
    CHECK(unchanged());
    raw(duplicate, false);
    advance(150);
  }
  raw(keys[7], true);
  advance(150);
  CHECK(unchanged());
  raw(keys[7], false);
  advance(149);
  CHECK(unchanged());
  advance(1);
  CHECK(inputRawCallback == nullptr);
  CHECK(appSettings.keyMapping.keyEdit[0].code == replacement.code);
  CHECK_FALSE(std::filesystem::exists(settingsPath));
  REQUIRE(mockScreenData->onEdit(0, 12, CellEditAction::tap) == 1);
  CHECK(std::filesystem::exists(settingsPath));
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping capture times out even if a key never releases") {
  edit();
  raw(replacement, true);
  advance(15000);
  CHECK(inputRawCallback == nullptr);
  CHECK(unchanged());
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping verification cannot use an old alias instead of the newly captured input") {
  appSettings.keyMapping.keyEdit[1] = keys[4];
  original = appSettings.keyMapping;
  prepareChange();
  for (int row = 0; row < 4; ++row) tap(keys[row]);
  tap(keys[4]);
  CHECK(unchanged());
  CHECK(std::string(screenGetActiveMessage()).find("newly assigned") != std::string::npos);
  tap(replacement);
  for (int row = 5; row < 8; ++row) tap(keys[row]);
  CHECK(inputRawCallback == nullptr);
  CHECK(appSettings.keyMapping.keyEdit[0].code == replacement.code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping a pad into a keyboard column swaps pad bindings without losing Edit") {
  const InputCode padA = {InputDeviceType::gamepad, 0};
  const InputCode padB = {InputDeviceType::gamepad, 1};
  appSettings.keyMapping.keyEdit[1] = padA;
  appSettings.keyMapping.keyOpt[1] = padB;
  original = appSettings.keyMapping;
  edit(5);
  tap(padA);
  advance(150);
  CHECK(unchanged());
  for (int row = 0; row < 8; ++row) {
    if (row == 4) tap(padB);
    else if (row == 5) tap(padA);
    else tap(keys[row]);
  }
  CHECK(inputRawCallback == nullptr);
  CHECK(appSettings.keyMapping.keyEdit[0].code == keys[4].code);
  CHECK(appSettings.keyMapping.keyEdit[1].deviceType == InputDeviceType::gamepad);
  CHECK(appSettings.keyMapping.keyEdit[1].code == padB.code);
  CHECK(appSettings.keyMapping.keyOpt[0].code == padA.code);
  CHECK(appSettings.keyMapping.keyOpt[1].deviceType == InputDeviceType::keyboard);
  CHECK(appSettings.keyMapping.keyOpt[1].code == keys[5].code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping refuses a cross-device swap without an equivalent binding") {
  appSettings.keyMapping.keyEdit[1] = replacement;
  original = appSettings.keyMapping;
  edit(5);
  tap(replacement);
  CHECK(inputRawCallback == nullptr);
  CHECK(unchanged());
  CHECK(std::string(screenGetActiveMessage()).find("Calibrate") != std::string::npos);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping verification rejects a physical press mapped to two different actions") {
  prepareChange();
  raw(keys[0], true);
  raw(keys[1], true); // A second OS path claims Down while testing Up.
  raw(keys[0], false);
  raw(keys[1], false);
  advance(150);
  tap(keys[1]); // Cannot advance: still waiting for a clean Up press.
  CHECK(unchanged());
  verify();
  CHECK(inputRawCallback == nullptr);
  CHECK(appSettings.keyMapping.keyEdit[0].code == replacement.code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping capture rejects logical buttons and cannot be bypassed by touch edits") {
  edit();
  tap({InputDeviceType::logical, keyEdit});
  CHECK(unchanged());
  CHECK(std::string(screenGetActiveMessage()).find("physical") != std::string::npos);
  mockScreenData->onEdit(0, 12, CellEditAction::tap);
  CHECK(inputRawCallback != nullptr);
  CHECK_FALSE(std::filesystem::exists(settingsPath));
  tap(replacement);
  advance(150);
  verify();
  CHECK(appSettings.keyMapping.keyEdit[0].code == replacement.code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping reset counts one raw input rather than its duplicate or repeats") {
  InputCode first = {InputDeviceType::gamepad, 999};
  InputCode second = {InputDeviceType::keyboard, 998};
  for (int i = 0; i < 4; ++i) {
    raw(first, true);
    raw(first, true);
    raw(second, true);
    raw(first, false);
    raw(second, false);
    advance(150);
    CHECK(inputRawCallback == nullptr);
  }
  tap(first);
  CHECK(inputRawCallback != nullptr);
  CHECK(unchanged());
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping timeouts remain correct across the SDL tick counter wrap") {
  mockMainLoopTicks = UINT32_MAX - 100;
  edit();
  raw(replacement, true);
  advance(14999);
  CHECK(inputRawCallback != nullptr);
  advance(1);
  CHECK(inputRawCallback == nullptr);
  CHECK(unchanged());
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping verification includes the changed motion control") {
  edit(8);
  tap(replacement);
  advance(150);
  verify(-1);
  CHECK(inputRawCallback != nullptr);
  CHECK(unchanged());
  tap(replacement);
  CHECK(appSettings.keyMapping.keyMotionLive[0].code == replacement.code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping clear and default reset also require verification") {
  appSettings.keyMapping.keyEdit[1] = replacement;
  original = appSettings.keyMapping;
  mockScreenData->onEdit(1, 4, CellEditAction::clear);
  CHECK(unchanged());
  advance(15000);
  CHECK(unchanged());

  // Unmapped controller taps must reach the reset path, not be discarded
  // by the PortMaster duplicate filter.
  for (int i = 0; i < 5; ++i) tap({InputDeviceType::gamepad, 999});
  REQUIRE(inputRawCallback != nullptr);
  CHECK(unchanged());
  advance(15000);
  CHECK(unchanged());
}

TEST_CASE_FIXTURE(KeyMappingFixture, "startup calibration ignores a broken saved mapping and saves only after testing") {
  std::ofstream(markerPath).close();
  REQUIRE(screenKeyMappingRecoveryRequested() == 1);
  appSettings.keyMapping = {};
  appCleanup();
  appSetup();
  original = appSettings.keyMapping;
  CHECK(currentScreen == &screenKeyMapping);
  open(1); // The screenSetup mock deliberately does not call screen setup.
  advance(150);
  for (InputCode key : keys) tap(key);
  advance(150);
  CHECK(unchanged());
  CHECK(std::filesystem::exists(markerPath));
  verify(-1);
  CHECK(currentScreen == &screenTitle);
  CHECK_FALSE(std::filesystem::exists(markerPath));
  CHECK(std::filesystem::exists(settingsPath));
  CHECK(appSettings.keyMapping.keyEdit[0].code == keys[4].code);
  appSettings.keyMapping = {};
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.keyMapping.keyEdit[0].code == keys[4].code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "cancelled startup calibration preserves the recovery request") {
  std::ofstream(markerPath).close();
  REQUIRE(screenKeyMappingRecoveryRequested() == 1);
  open(1);
  advance(150);
  tap(keys[0]);
  tap(keys[0]); // Reusing Up for Down is rejected.
  CHECK(std::string(screenGetActiveMessage()).find("Already used") != std::string::npos);
  advance(15000);
  CHECK(unchanged());
  CHECK(currentScreen == &screenTitle);
  CHECK(std::filesystem::exists(markerPath));
  CHECK_FALSE(std::filesystem::exists(settingsPath));
}

TEST_CASE_FIXTURE(KeyMappingFixture, "calibration restarts on a one-second hold without capturing the held button") {
  edit(11);
  tap(keys[1]); // Accidentally assign Down to Up.
  raw(keys[0], true);
  advance(999);
  CHECK(unchanged());
  mockScreenData->drawStatic();
  CHECK(std::string(mockGfxCells[16], 40).find("Press physical Down") != std::string::npos);
  advance(1);
  mockScreenData->drawStatic();
  CHECK(std::string(mockGfxCells[16], 40).find("Release all buttons") != std::string::npos);
  advance(500); // Still held: must not enter capture or restart repeatedly.
  raw(keys[0], false);
  advance(150);
  mockScreenData->drawStatic();
  CHECK(std::string(mockGfxCells[16], 40).find("Press physical Up") != std::string::npos);
  for (InputCode key : keys) tap(key);
  advance(150);
  verify(-1);
  CHECK(inputRawCallback == nullptr);
  CHECK(appSettings.keyMapping.keyUp[0].code == keys[0].code);
  CHECK(appSettings.keyMapping.keyDown[0].code == keys[1].code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "calibration can restart during verification with duplicate held inputs") {
  edit(11);
  for (InputCode key : keys) tap(key);
  advance(150);
  raw(keys[0], true);
  raw(duplicate, true);
  advance(1000);
  raw(keys[0], false);
  advance(500);
  CHECK(unchanged());
  raw(duplicate, false);
  advance(150);
  for (InputCode key : keys) tap(key);
  advance(150);
  verify(-1);
  CHECK(inputRawCallback == nullptr);
  CHECK(appSettings.keyMapping.keyEdit[0].code == keys[4].code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "startup calibration reports save failures and retains the old mapping") {
  std::ofstream(markerPath).close();
  REQUIRE(screenKeyMappingRecoveryRequested() == 1);
  std::filesystem::create_directory(settingsPath);
  open(1);
  advance(150);
  for (InputCode key : keys) tap(key);
  advance(150);
  verify(-1);
  CHECK(unchanged());
  CHECK(currentScreen == &screenTitle);
  CHECK(std::filesystem::exists(markerPath));
  CHECK(std::string(screenGetActiveMessage()).find("Could not save") != std::string::npos);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "sleep and exit cannot save an unverified mapping") {
  for (MainLoopEvent event : {MainLoopEvent::sleep, MainLoopEvent::exit}) {
    open();
    prepareChange();
    appOnEvent({event, {}});
    CHECK(inputRawCallback == nullptr);
    CHECK(unchanged());
    appSettings.keyMapping = {};
    REQUIRE(settingsLoad() == 0);
    CHECK(unchanged());
  }
}
