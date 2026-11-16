#include "doctest.h"
#include "app.h"
#include "app_ui_mock.h"
#include "corelib_file.h"
#include "screen_keymapping.h"
#include "console_input.h"
#include "chipnomad_lib_live_stick.h"
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
    CHECK(inputRawCallback == nullptr);
  }
  void done() {
    REQUIRE(mockScreenData->onEdit(0, 14, CellEditAction::tap) == 1);
    REQUIRE(inputRawCallback != nullptr);
    advance(150);
  }
  void verify(int replacedRow = 4, InputCode changed = replacement) {
    for (int row = 0; row < 8; ++row) tap(row == replacedRow ? changed : keys[row]);
  }
  bool unchanged() { return std::memcmp(&original, &appSettings.keyMapping, sizeof(original)) == 0; }
};
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping menu keeps Done last and calibration immediately above it") {
  REQUIRE(mockScreenData->rows == 15);
  mockScreenData->drawField(0, 13, CellState::focus);
  mockScreenData->drawField(0, 14, CellState::focus);
  CHECK(std::string(mockGfxCells[15], 18) == "Calibrate controls");
  CHECK(std::string(mockGfxCells[16], 4) == "Done");
  mockScreenData->cursorRow = 14;
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
  CHECK(inputRawCallback == nullptr);
  done();
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
  done();
  tap(keys[0]);
  advance(14999);
  CHECK(inputRawCallback != nullptr);
  advance(1);
  CHECK(inputRawCallback == nullptr);
  CHECK(unchanged());
  CHECK(std::string(screenGetActiveMessage()).find("active controls unchanged") != std::string::npos);
  done();
  verify();
  CHECK(appSettings.keyMapping.keyEdit[0].code == replacement.code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping verification needs every core control and all releases") {
  prepareChange();
  done();
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
  CHECK(std::filesystem::exists(settingsPath));
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping capture times out even if a key never releases") {
  edit();
  raw(replacement, true);
  advance(15000);
  CHECK(inputRawCallback == nullptr);
  CHECK(unchanged());
}

TEST_CASE_FIXTURE(KeyMappingFixture, "multiple remaps stay staged until one final verification") {
  const InputCode changes[] = {
    {InputDeviceType::gamepad, 210}, {InputDeviceType::gamepad, 211},
    {InputDeviceType::gamepad, 212}, {InputDeviceType::gamepad, 213}
  };
  for (int row = 0; row < 4; ++row) {
    edit(row);
    tap(changes[row]);
    CHECK(inputRawCallback == nullptr);
    CHECK(unchanged());
  }
  edit(4);
  raw(replacement, true);
  advance(15000);
  CHECK(inputRawCallback == nullptr);
  CHECK(unchanged());

  done();
  for (int row = 0; row < 8; ++row) tap(row < 4 ? changes[row] : keys[row]);
  CHECK(currentScreen == &screenControlsSettings);
  CHECK(appSettings.keyMapping.keyUp[0].code == changes[0].code);
  CHECK(appSettings.keyMapping.keyDown[0].code == changes[1].code);
  CHECK(appSettings.keyMapping.keyLeft[0].code == changes[2].code);
  CHECK(appSettings.keyMapping.keyRight[0].code == changes[3].code);
  appSettings.keyMapping = {};
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.keyMapping.keyRight[0].code == changes[3].code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "final verification tests all newly assigned optional and extra inputs") {
  edit(4, 1);
  tap(replacement);
  edit(4, 2);
  tap(duplicate);
  edit(8);
  tap({InputDeviceType::gamepad, 210});
  edit(9);
  tap({InputDeviceType::gamepad, 211});
  done();
  for (int row = 0; row < 8; ++row) tap(row == 4 ? replacement : keys[row]);
  CHECK(unchanged());
  tap(duplicate);
  tap({InputDeviceType::gamepad, 210});
  CHECK(unchanged());
  tap({InputDeviceType::gamepad, 211});
  CHECK(currentScreen == &screenControlsSettings);
  CHECK(consoleInput::same(appSettings.keyMapping.keyEdit[2], duplicate));
  CHECK(appSettings.keyMapping.keyMotionRecord[0].code == 211);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "old edit button navigates until final verification") {
  prepareChange();
  mockScreenData->cursorRow = 14;
  tap(replacement);
  CHECK(inputRawCallback == nullptr);
  raw(keys[4], true);
  REQUIRE(inputRawCallback != nullptr);
  raw(keys[4], false);
  advance(150);
  verify();
  CHECK(currentScreen == &screenControlsSettings);
  CHECK(appSettings.keyMapping.keyEdit[0].code == replacement.code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "calibration also verifies staged optional bindings before saving") {
  edit(8);
  tap(replacement);
  edit(13);
  for (InputCode key : keys) tap(key);
  advance(150);
  verify(-1);
  CHECK(inputRawCallback != nullptr);
  CHECK(unchanged());
  tap(replacement);
  CHECK(inputRawCallback == nullptr);
  CHECK(unchanged());
  REQUIRE(mockScreenData->onEdit(0, 14, CellEditAction::tap) == 1);
  CHECK(currentScreen == &screenControlsSettings);
  CHECK(appSettings.keyMapping.keyMotionLive[0].code == replacement.code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "final save failure keeps the active mapping and allows retry") {
  prepareChange();
  std::filesystem::create_directory(settingsPath);
  done();
  verify();
  CHECK(unchanged());
  CHECK(currentScreen == &screenKeyMapping);
  CHECK(std::string(screenGetActiveMessage()).find("Could not save") != std::string::npos);
  std::filesystem::remove(settingsPath);
  REQUIRE(mockScreenData->onEdit(0, 14, CellEditAction::tap) == 1);
  CHECK(currentScreen == &screenControlsSettings);
  CHECK(appSettings.keyMapping.keyEdit[0].code == replacement.code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping verification cannot use an old alias instead of the newly captured input") {
  appSettings.keyMapping.keyEdit[1] = keys[4];
  original = appSettings.keyMapping;
  open();
  prepareChange();
  done();
  for (int row = 0; row < 4; ++row) tap(keys[row]);
  tap(keys[4]);
  CHECK(unchanged());
  CHECK(std::string(screenGetActiveMessage()).find("newly assigned") != std::string::npos);
  tap(replacement);
  for (int row = 5; row < 8; ++row) tap(keys[row]);
  CHECK(inputRawCallback == nullptr);
  CHECK(appSettings.keyMapping.keyEdit[0].code == replacement.code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping swaps canonical inputs without a transport-specific slot") {
  const InputCode padA = {InputDeviceType::gamepad, 0};
  const InputCode padB = {InputDeviceType::gamepad, 1};
  appSettings.keyMapping.keyEdit[1] = padA;
  appSettings.keyMapping.keyOpt[1] = padB;
  original = appSettings.keyMapping;
  open();
  edit(5);
  tap(padA);
  CHECK(unchanged());
  done();
  for (int row = 0; row < 8; ++row) {
    if (row == 4) tap(keys[5]);
    else if (row == 5) tap(padA);
    else tap(keys[row]);
  }
  CHECK(inputRawCallback == nullptr);
  CHECK(appSettings.keyMapping.keyEdit[0].code == keys[4].code);
  CHECK(appSettings.keyMapping.keyEdit[1].deviceType == InputDeviceType::keyboard);
  CHECK(appSettings.keyMapping.keyEdit[1].code == keys[5].code);
  CHECK(appSettings.keyMapping.keyOpt[0].code == padA.code);
  CHECK(appSettings.keyMapping.keyOpt[1].deviceType == InputDeviceType::gamepad);
  CHECK(appSettings.keyMapping.keyOpt[1].code == padB.code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping tests a swap even when the previous input uses another transport") {
  appSettings.keyMapping.keyEdit[1] = replacement;
  original = appSettings.keyMapping;
  open();
  edit(5);
  tap(replacement);
  CHECK(inputRawCallback == nullptr);
  CHECK(unchanged());
  done();
  for (int row = 0; row < 8; ++row)
    tap(row == 4 ? keys[5] : row == 5 ? replacement : keys[row]);
  CHECK(inputRawCallback == nullptr);
  CHECK(appSettings.keyMapping.keyOpt[0].code == replacement.code);
}

TEST_CASE("console button aliases merge overlapping sources in either order without delaying taps") {
  int count;
  const auto* aliases = inputConsoleAliases(&count);
  for (int i = 0; i < count; ++i) {
    for (bool keyboardFirst : {false, true}) {
      for (bool keyboardReleasedFirst : {false, true}) {
        consoleInput::Buttons buttons;
        const InputCode key{InputDeviceType::keyboard, aliases[i].keyboard};
        const InputCode pad{InputDeviceType::gamepad, aliases[i].gamepad};
        auto first = keyboardFirst ? key : pad;
        auto second = keyboardFirst ? pad : key;
        CHECK(buttons.update(first, true));
        CHECK(consoleInput::same(first, pad));
        CHECK_FALSE(buttons.update(second, true));
        second = keyboardFirst ? pad : key;
        CHECK_FALSE(buttons.update(second, true));
        first = keyboardReleasedFirst ? key : pad;
        second = keyboardReleasedFirst ? pad : key;
        CHECK_FALSE(buttons.update(first, false));
        CHECK(buttons.update(second, false));
        for (int tap = 0; tap < 2; ++tap) {
          auto input = key;
          CHECK(buttons.update(input, true));
          input = key;
          CHECK(buttons.update(input, false));
        }
      }
    }
  }
}

TEST_CASE("console mapping migration preserves custom inputs and rejects alias conflicts without mutation") {
  KeyMapping mapping{};
  mapping.keyEdit[0] = {InputDeviceType::keyboard, 'x'};
  mapping.keyEdit[1] = {InputDeviceType::gamepad, 0};
  mapping.keyOpt[0] = {InputDeviceType::keyboard, 'z'};
  mapping.keyUndo[0] = keys[4];
  REQUIRE(consoleInput::migrate(mapping));
  CHECK(mapping.keyEdit[0].deviceType == InputDeviceType::gamepad);
  CHECK(mapping.keyEdit[1].deviceType == InputDeviceType::none);
  CHECK(consoleInput::same(mapping.keyUndo[0], keys[4]));
  const KeyMapping normalized = mapping;
  REQUIRE(consoleInput::migrate(mapping));
  CHECK(std::memcmp(&normalized, &mapping, sizeof(mapping)) == 0);
  mapping.keyOpt[1] = {InputDeviceType::keyboard, 'x'};
  const KeyMapping conflicting = mapping;
  CHECK_FALSE(consoleInput::migrate(mapping));
  CHECK(std::memcmp(&conflicting, &mapping, sizeof(mapping)) == 0);
}

TEST_CASE("console source removal releases only buttons whose last source disappeared") {
  consoleInput::Buttons buttons;
  InputCode key{InputDeviceType::keyboard, 'x'};
  InputCode pad{InputDeviceType::gamepad, 0};
  REQUIRE(buttons.update(key, true));
  REQUIRE_FALSE(buttons.update(pad, true));
  int releases = 0;
  buttons.release(InputDeviceType::gamepad, [&](InputCode) { ++releases; });
  CHECK(releases == 0);
  buttons.release(InputDeviceType::keyboard, [&](InputCode input) {
    CHECK(consoleInput::same(input, pad));
    ++releases;
  });
  CHECK(releases == 1);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "console capture and verification share keyboard and pad identities") {
  edit();
  raw({InputDeviceType::gamepad, 0}, true);
  raw({InputDeviceType::keyboard, 'x'}, true);
  raw({InputDeviceType::gamepad, 0}, false);
  advance(150);
  CHECK(unchanged());
  raw({InputDeviceType::keyboard, 'x'}, false);
  advance(150);
  done();
  verify(4, {InputDeviceType::keyboard, 'x'});
  CHECK(inputRawCallback == nullptr);
  CHECK(appSettings.keyMapping.keyEdit[0].deviceType == InputDeviceType::gamepad);
  CHECK(appSettings.keyMapping.keyEdit[0].code == 0);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "duplicate console D-pad navigation does not trigger unmapped recovery") {
  int count;
  const auto up = inputConsoleAliases(&count)[0];
  appSettings.keyMapping.keyUp[0] = {InputDeviceType::gamepad, up.gamepad};
  for (int i = 0; i < 6; ++i) {
    raw({InputDeviceType::keyboard, up.keyboard}, true);
    raw({InputDeviceType::gamepad, up.gamepad}, true);
    raw({InputDeviceType::keyboard, up.keyboard}, false);
    raw({InputDeviceType::gamepad, up.gamepad}, false);
    advance(100);
    CHECK(inputRawCallback == nullptr);
  }
}

TEST_CASE_FIXTURE(KeyMappingFixture, "console Motion remains held until both trigger paths release") {
  appSettings.keyMapping.keyMotionRecord[0] = {InputDeviceType::gamepad, gamepadTriggerLeft};
  raw({InputDeviceType::keyboard, 'w'}, true);
  raw({InputDeviceType::gamepad, gamepadTriggerLeft}, true);
  CHECK(chipnomadMotionMode() == 1);
  raw({InputDeviceType::keyboard, 'w'}, false);
  CHECK(chipnomadMotionMode() == 1);
  MainLoopEventData removed{};
  removed.type = MainLoopEvent::inputSourceReleased;
  removed.data.value = static_cast<int>(InputDeviceType::gamepad);
  appOnEvent(removed);
  CHECK(chipnomadMotionMode() == 0);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "conflicting console migration keeps original settings until calibration succeeds") {
  appSettings.keyMapping.keyEdit[0] = {InputDeviceType::keyboard, 'x'};
  appSettings.keyMapping.keyOpt[0] = {InputDeviceType::gamepad, 0};
  original = appSettings.keyMapping;
  open(2);
  CHECK(inputRawCallback != nullptr);
  CHECK(unchanged());
  advance(15000);
  CHECK(inputRawCallback != nullptr);
  CHECK(unchanged());
  advance(150);
  for (int row = 0; row < 8; ++row) tap(keys[row]);
  advance(150);
  verify(-1);
  CHECK(inputRawCallback == nullptr);
  CHECK(currentScreen == &screenTitle);
  CHECK(appSettings.keyMapping.keyEdit[0].code == keys[4].code);
  CHECK_FALSE(std::filesystem::exists(markerPath));
  appSettings.keyMapping = {};
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.keyMapping.keyEdit[0].code == keys[4].code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping verification rejects a physical press mapped to two different actions") {
  prepareChange();
  done();
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

TEST_CASE_FIXTURE(KeyMappingFixture, "console recovery clears only ambiguous optional aliases after eight-control calibration") {
  appSettings.keyMapping.keyMotionLive[0] = {InputDeviceType::keyboard, 'q'};
  appSettings.keyMapping.keyMotionRecord[0] = {InputDeviceType::gamepad, 9};
  appSettings.keyMapping.keyUndo[0] = {InputDeviceType::keyboard, 'q'};
  appSettings.keyMapping.keyUndo[1] = replacement;
  appSettings.keyMapping.keyRedo[0] = {InputDeviceType::keyboard, 'r'};
  original = appSettings.keyMapping;
  std::ofstream(markerPath).close();
  REQUIRE(screenKeyMappingRecoveryRequested() == 1);
  open(3);
  CHECK(std::string(screenGetActiveMessage()).find("Motion/Undo") != std::string::npos);
  CHECK(unchanged());
  advance(15000);
  CHECK(inputRawCallback != nullptr);
  CHECK(unchanged());
  CHECK(std::filesystem::exists(markerPath));
  advance(150);
  for (int row = 0; row < 8; ++row) tap(keys[row]);
  advance(150);
  verify(-1);
  CHECK(inputRawCallback == nullptr);
  CHECK_FALSE(std::filesystem::exists(markerPath));
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.keyMapping.keyMotionLive[0].deviceType == InputDeviceType::none);
  CHECK(appSettings.keyMapping.keyMotionRecord[0].deviceType == InputDeviceType::none);
  CHECK(appSettings.keyMapping.keyUndo[0].deviceType == InputDeviceType::none);
  CHECK(consoleInput::same(appSettings.keyMapping.keyUndo[1], replacement));
  CHECK(appSettings.keyMapping.keyRedo[0].code == 10);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping capture rejects logical buttons and cannot be bypassed by touch edits") {
  edit();
  tap({InputDeviceType::logical, keyEdit});
  CHECK(unchanged());
  CHECK(std::string(screenGetActiveMessage()).find("physical") != std::string::npos);
  mockScreenData->onEdit(0, 14, CellEditAction::tap);
  CHECK(inputRawCallback != nullptr);
  CHECK_FALSE(std::filesystem::exists(settingsPath));
  tap(replacement);
  done();
  verify();
  CHECK(appSettings.keyMapping.keyEdit[0].code == replacement.code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "console migration retries after sleep and save failure without replacing the original") {
  appSettings.keyMapping.keyEdit[0] = {InputDeviceType::keyboard, 'x'};
  appSettings.keyMapping.keyOpt[0] = {InputDeviceType::gamepad, 0};
  original = appSettings.keyMapping;
  open(2);
  advance(150);
  tap(keys[0]);
  appOnEvent({MainLoopEvent::sleep, {}});
  CHECK(unchanged());
  REQUIRE(settingsLoad() == 0);
  CHECK(unchanged());
  appOnEvent({MainLoopEvent::wake, {}});
  advance(150);
  CHECK(inputRawCallback != nullptr);
  std::filesystem::remove(settingsPath);
  std::filesystem::create_directory(settingsPath);
  advance(150);
  for (InputCode key : keys) tap(key);
  advance(150);
  verify(-1);
  CHECK(unchanged());
  CHECK(currentScreen == &screenKeyMapping);
  CHECK(inputRawCallback != nullptr);
  CHECK(std::string(screenGetActiveMessage()).find("Could not save") != std::string::npos);
  std::filesystem::remove(settingsPath);
  advance(150);
  for (InputCode key : keys) tap(key);
  advance(150);
  verify(-1);
  CHECK(inputRawCallback == nullptr);
  CHECK(currentScreen == &screenTitle);
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.keyMapping.keyEdit[0].code == keys[4].code);
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
  CHECK(inputRawCallback == nullptr);
  CHECK(unchanged());
  done();
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
  done();
  verify(-1);
  CHECK(inputRawCallback != nullptr);
  CHECK(unchanged());
  tap(replacement);
  CHECK(appSettings.keyMapping.keyMotionLive[0].code == replacement.code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "mapping clear and default reset also require verification") {
  appSettings.keyMapping.keyEdit[1] = replacement;
  original = appSettings.keyMapping;
  open();
  mockScreenData->onEdit(1, 4, CellEditAction::clear);
  CHECK(unchanged());
  CHECK(inputRawCallback == nullptr);
  done();
  advance(15000);
  CHECK(unchanged());

  // Unmapped controller taps must reach the reset path, not be discarded
  // by the PortMaster duplicate filter.
  for (int i = 0; i < 5; ++i) tap({InputDeviceType::gamepad, 999});
  CHECK(inputRawCallback == nullptr);
  CHECK(unchanged());
  done();
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
  edit(13);
  tap(keys[1]); // Accidentally assign Down to Up.
  raw(keys[0], true);
  advance(999);
  CHECK(unchanged());
  mockScreenData->drawStatic();
  CHECK(std::string(mockGfxCells[17], 40).find("Press physical Down") != std::string::npos);
  advance(1);
  mockScreenData->drawStatic();
  CHECK(std::string(mockGfxCells[17], 40).find("Release all buttons") != std::string::npos);
  advance(500); // Still held: must not enter capture or restart repeatedly.
  raw(keys[0], false);
  advance(150);
  mockScreenData->drawStatic();
  CHECK(std::string(mockGfxCells[17], 40).find("Press physical Up") != std::string::npos);
  for (InputCode key : keys) tap(key);
  advance(150);
  verify(-1);
  CHECK(inputRawCallback == nullptr);
  CHECK(appSettings.keyMapping.keyUp[0].code == keys[0].code);
  CHECK(appSettings.keyMapping.keyDown[0].code == keys[1].code);
}

TEST_CASE_FIXTURE(KeyMappingFixture, "calibration can restart during verification with duplicate held inputs") {
  edit(13);
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

TEST_CASE_FIXTURE(KeyMappingFixture, "sleep discards staged changes from the open editor") {
  prepareChange();
  appOnEvent({MainLoopEvent::sleep, {}});
  appOnEvent({MainLoopEvent::wake, {}});
  CHECK(unchanged());
  REQUIRE(mockScreenData->onEdit(0, 14, CellEditAction::tap) == 1);
  CHECK(inputRawCallback == nullptr);
  CHECK(currentScreen == &screenControlsSettings);
  CHECK(unchanged());
}
