#include "doctest.h"
#include "app_ui_mock.h"
#include "common.h"
#include "corelib_file.h"
#include "corelib_gfx.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace {
struct MinimapFixture {
  AppSettings savedSettings = appSettings;
  ChipNomadState* savedState = chipnomadState;
  const AppScreen* savedScreen = currentScreen;
  int* savedSongRow = pSongRow;
  int* savedSongTrack = pSongTrack;
  int* savedChainRow = pChainRow;
  std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)> state{chipnomadCreate(), chipnomadDestroy};
  int songRow = 0;
  int songTrack = 0;

  MinimapFixture() {
    initDefaultAppSettings();
    REQUIRE(state != nullptr);
    chipnomadState = state.get();
    projectInit(&state->project);
    state->project.tracksCount = PROJECT_MAX_TRACKS;
    state->project.song[0][0] = 0;
    state->project.song[0][1] = 1;
    pSongRow = &songRow;
    pSongTrack = &songTrack;
    currentScreen = &screenChain;
    screenChain.init();
    screenChain.setup(0);
    gfxClear();
  }
  ~MinimapFixture() {
    appSettings = savedSettings;
    chipnomadState = savedState;
    currentScreen = savedScreen;
    pSongRow = savedSongRow;
    pSongTrack = savedSongTrack;
    pChainRow = savedChainRow;
  }
};

struct SettingsFileFixture {
  AppSettings savedSettings = appSettings;
  std::filesystem::path path;
  bool existed = false;
  std::string contents;

  SettingsFileFixture() {
    char directory[PATH_LENGTH];
    REQUIRE(fileGetDefaultDirectory(directory, sizeof(directory)) == 0);
    path = std::filesystem::path(directory) / "settings.txt";
    existed = std::filesystem::exists(path);
    if (existed) {
      std::ifstream input(path, std::ios::binary);
      contents.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }
    initDefaultAppSettings();
  }
  ~SettingsFileFixture() {
    if (existed) std::ofstream(path, std::ios::binary) << contents;
    else std::filesystem::remove(path);
    appSettings = savedSettings;
  }
};
}

TEST_SUITE("Chain minimap") {
TEST_CASE_FIXTURE(MinimapFixture, "only OPT double keydown toggles and clears the minimap") {
  CHECK(appSettings.chainMinimap == 0);
  screenChain.draw();
  CHECK(std::string(mockGfxCells[3] + 10, 2) == "  ");
  screenChain.onInput(1, keyOpt, 1);
  screenChain.onInput(0, keyOpt, 2);
  screenChain.onInput(1, keyOpt | keyRight, 2);
  CHECK(appSettings.chainMinimap == 0);

  screenChain.onInput(1, keyOpt, 2);
  CHECK(appSettings.chainMinimap == 1);
  screenChain.draw();
  CHECK(std::string(mockGfxCells[3] + 10, 2) == "00");
  CHECK(mockGfxCells[2][31] == '8');
  screenChain.onInput(0, keyOpt, 2);
  screenChain.onInput(1, keyOpt, 3);
  CHECK(appSettings.chainMinimap == 1);
  screenChain.onInput(1, keyOpt, 2);
  CHECK(appSettings.chainMinimap == 0);
  screenChain.draw();
  CHECK(std::string(mockGfxCells[3] + 10, 23) == std::string(23, ' '));
}

TEST_CASE_FIXTURE(MinimapFixture, "toggle preserves selection and chain navigation") {
  screenChain.fullRedraw();
  REQUIRE(mockScreenData != nullptr);
  mockScreenData->selectMode = 1;
  screenChain.onInput(1, keyOpt, 2);
  CHECK(mockScreenData->selectMode == 1);
  mockScreenData->selectMode = 0;
  screenChain.onInput(1, keyOpt | keyRight, 1);
  CHECK(songTrack == 1);
  CHECK(appSettings.chainMinimap == 1);
  screenChain.setup(0);
  CHECK(appSettings.chainMinimap == 1);
}

TEST_CASE_FIXTURE(SettingsFileFixture, "preference persists both states and defaults safely") {
  for (int enabled : {0, 1}) {
    appSettings.chainMinimap = enabled;
    REQUIRE(settingsSave() == 0);
    appSettings.chainMinimap = !enabled;
    REQUIRE(settingsLoad() == 0);
    CHECK(appSettings.chainMinimap == enabled);
  }
  std::ofstream(path) << "screenWidth: 640\n";
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.chainMinimap == 0);
  for (const char* value : {"-1", "2", "garbage"}) {
    std::ofstream(path) << "chainMinimap: " << value << "\n";
    REQUIRE(settingsLoad() == 0);
    CHECK(appSettings.chainMinimap == 0);
  }
}
}
