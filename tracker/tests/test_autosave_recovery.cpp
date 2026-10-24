#include "doctest.h"
#include "common.h"
#include "app.h"
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
  std::string settings = std::filesystem::path(autosave).parent_path().string() + "/settings.txt";

  AutosaveFixture() {
    for (const std::string& path : {autosave, settings})
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
    for (const std::string& path : {autosave, kept, kept2, settings}) std::filesystem::remove(path);
    for (const std::string& path : {autosave, settings})
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
  CHECK(autosaveLoadFailed == 1);
  CHECK(autosaveKept == 1);
  CHECK(readAll(kept) == unreadable);
  quitApp();  // Quitting autosaves the project that is open now
  CHECK(readAll(kept) == unreadable);
  Project reloaded;
  projectInit(&reloaded);
  CHECK(projectLoad(&reloaded, autosave.c_str()) == 0);
  projectFree(&reloaded);
}

TEST_CASE_FIXTURE(AutosaveFixture, "A second unreadable autosave does not replace the first kept copy") {
  writeAll(kept, "first unreadable autosave\n");
  writeAll(autosave, unreadable);
  appSetup();
  CHECK(autosaveKept == 1);
  CHECK(readAll(kept) == "first unreadable autosave\n");
  CHECK(readAll(kept2) == unreadable);
}

TEST_CASE_FIXTURE(AutosaveFixture, "A readable autosave loads normally and nothing is set aside") {
  appSetup();
  quitApp();  // Writes a valid autosave
  appCleanup();
  appSetup();
  CHECK(autosaveLoadFailed == 0);
  CHECK(autosaveKept == 0);
  CHECK_FALSE(std::filesystem::exists(kept));
}
