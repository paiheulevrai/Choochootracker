#include "doctest.h"
#include "common.h"
#include "app.h"
#include "audio_manager.h"
#include "project_utils.h"
#include "app_ui_mock.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {
const char* unreadable = "not a ChooChooTracker project\n";

std::string readAll(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

void writeAll(const std::string& path, const std::string& text) {
  std::ofstream(path, std::ios::binary) << text;
}

// appSetup() and the exit event read and write the real autosave and
// settings files next to the test executable. Set any existing ones aside
// and put them back afterwards.
struct AutosaveFixture {
  AppSettings savedSettings = appSettings;
  ChipNomadState* savedState = chipnomadState;
  std::string autosave = getAutosavePath();
  std::string kept = autosave + ".unreadable";
  std::string kept2 = autosave + ".unreadable-2";
  std::string previous = autosave + ".previous";
  std::string settings = std::filesystem::path(autosave).parent_path().string() + "/settings.txt";

  AutosaveFixture() {
    for (const std::string& path : {autosave, settings, previous})
      if (std::filesystem::exists(path)) std::filesystem::rename(path, path + ".testbackup");
    std::filesystem::remove(kept);
    std::filesystem::remove(kept2);
    initDefaultAppSettings();
    inputRawCallback = nullptr;
    autosaveLoadFailed = 0;
    autosaveKept = 0;
  }
  ~AutosaveFixture() {
    appCleanup();
    chipnomadState = savedState;
    appSettings = savedSettings;
    autosaveLoadFailed = 0;
    autosaveKept = 0;
    for (const std::string& path : {autosave, kept, kept2, settings, previous}) std::filesystem::remove(path);
    for (const std::string& path : {autosave, settings, previous})
      if (std::filesystem::exists(path + ".testbackup")) std::filesystem::rename(path + ".testbackup", path);
  }
  void quitApp() {
    MainLoopEventData event = {};
    event.type = MainLoopEvent::exit;
    appOnEvent(event);
  }
};
}

TEST_CASE_FIXTURE(AutosaveFixture, "An autosave that fails to load is kept, not overwritten") {
  writeAll(autosave, unreadable);
  appSetup();
  CHECK_FALSE(appHasProject());
  CHECK(readAll(autosave) == unreadable);
  CHECK(appContinueProject() == 1);
  CHECK(autosaveLoadFailed == 1);
  CHECK(autosaveKept == 1);
  CHECK(readAll(kept) == unreadable);
  quitApp();  // No project has been committed, so no placeholder is saved.
  CHECK(readAll(kept) == unreadable);
  CHECK_FALSE(std::filesystem::exists(autosave));
}

TEST_CASE_FIXTURE(AutosaveFixture, "A second unreadable autosave does not replace the first kept copy") {
  writeAll(kept, "first unreadable autosave\n");
  writeAll(autosave, unreadable);
  appSetup();
  CHECK(appContinueProject() == 1);
  CHECK(autosaveKept == 1);
  CHECK(readAll(kept) == "first unreadable autosave\n");
  CHECK(readAll(kept2) == unreadable);
}

TEST_CASE_FIXTURE(AutosaveFixture, "A readable autosave loads normally and nothing is set aside") {
  Project initial;
  projectInitAY(&initial);
  REQUIRE(projectSave(&initial, autosave.c_str()) == 0);
  projectFree(&initial);
  appSetup();
  const std::string original = readAll(autosave);
  quitApp();
  CHECK(readAll(autosave) == original);
  CHECK_FALSE(appHasProject());
  REQUIRE(appContinueProject() == 0);
  CHECK(appHasProject());
  appCleanup();
  appSetup();
  CHECK(autosaveLoadFailed == 0);
  CHECK(autosaveKept == 0);
  CHECK_FALSE(std::filesystem::exists(kept));
}

TEST_CASE_FIXTURE(AutosaveFixture, "Title exit and sleep preserve an unreadable autosave without opening it") {
  writeAll(autosave, unreadable);
  appSetup();
  MainLoopEventData event = {};
  event.type = MainLoopEvent::tick;
  for (int tick = 0; tick < 3601; ++tick) appOnEvent(event);
  event.type = MainLoopEvent::sleep;
  appOnEvent(event);
  quitApp();
  CHECK_FALSE(appHasProject());
  CHECK(autosaveLoadFailed == 0);
  CHECK(readAll(autosave) == unreadable);
  CHECK_FALSE(std::filesystem::exists(kept));
}

TEST_CASE_FIXTURE(AutosaveFixture, "Starting another project preserves an unexamined autosave before overwriting") {
  writeAll(autosave, unreadable);
  appSetup();
  appActivateProject(); // Same activation boundary used by OPEN and NEW.
  quitApp();
  CHECK(readAll(previous) == unreadable);
  CHECK(readAll(autosave) != unreadable);
}

TEST_CASE_FIXTURE(AutosaveFixture, "Background lifecycle saves the project and preserves platform playback policy") {
  appSetup();
  projectInitAY(&chipnomadState->project);
  appActivateProject();
  auto& project = chipnomadState->project;
  project.song[0][0] = 0;
  project.chains[0].rows[0].phrase = 0;
  chipnomadQueueProjectRefresh(chipnomadState);
  chipnomadQueuePlaybackStartSong(chipnomadState, 0, 0, 1);
  float output[1024] = {};
  chipnomadRender(chipnomadState, output, 512);
  REQUIRE(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying);

  struct RestoreAudioManager {
    AudioManager saved = audioManager;
    ~RestoreAudioManager() { audioManager = saved; }
  } restoreAudioManager;
  static int pauseCalls, resumeCalls;
  pauseCalls = resumeCalls = 0;
  audioManager.pause = [] { ++pauseCalls; };
  audioManager.resume = [] { ++resumeCalls; };

  MainLoopEventData event = {};
  event.type = MainLoopEvent::sleep;
  appOnEvent(event);
  chipnomadRender(chipnomadState, output, 512);
  CHECK(std::filesystem::exists(autosave));
#ifdef ANDROID_BUILD
  CHECK(pauseCalls == 0);
  CHECK(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying);
#else
  CHECK(pauseCalls == 1);
  CHECK_FALSE(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying);
#endif

  event.type = MainLoopEvent::wake;
  appOnEvent(event);
#ifdef ANDROID_BUILD
  CHECK(resumeCalls == 0);
#else
  CHECK(resumeCalls == 1);
#endif
}
