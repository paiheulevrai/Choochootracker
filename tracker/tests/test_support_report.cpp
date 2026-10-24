#include "doctest.h"

#include "common.h"
#include "support_report.h"
#include "version.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

TEST_CASE("support report writes a bounded runtime summary") {
  namespace fs = std::filesystem;

  const fs::path path = "build/tests/support-report-test.txt";
  fs::create_directories(path.parent_path());
  fs::remove(path);

  ChipNomadState* savedState = chipnomadState;
  AppSettings savedSettings = appSettings;
  int savedModified = projectModified;
  char savedError[sizeof(projectFileError)];
  memcpy(savedError, projectFileError, sizeof(savedError));

  ChipNomadState* state = chipnomadCreate();
  REQUIRE(state != nullptr);
  chipnomadState = state;
  initDefaultAppSettings();
  projectModified = 1;
  state->project.tracksCount = 3;
  state->project.chipsCount = 1;
  state->project.tickRate = 60.0f;
  state->project.instruments[2].type = InstrumentType::AY1;
  strncpy(projectFileError, "sample failure", sizeof(projectFileError) - 1);
  projectFileError[sizeof(projectFileError) - 1] = 0;

  CHECK(supportReportWrite(path.string().c_str(), "Settings") == 0);

  std::ifstream input(path, std::ios::binary);
  CHECK(input.good());
  if (input.good()) {
    std::string report((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());

    CHECK(report.find("ChooChooTracker support report\n") == 0);
    CHECK(report.find(std::string("version=") + appVersion + "\n") != std::string::npos);
    CHECK(report.find("platform=") != std::string::npos);
    CHECK(report.find("screen=Settings\n") != std::string::npos);
    CHECK(report.find("audio.sample_rate=48000\n") != std::string::npos);
    CHECK(report.find("runtime.command_overflow=") != std::string::npos);
    CHECK(report.find("runtime.render_buffer_overflow=") != std::string::npos);
    CHECK(report.find("playback.is_playing=") != std::string::npos);
    CHECK(report.find("project.modified=1\n") != std::string::npos);
    CHECK(report.find("project.tracks=3\n") != std::string::npos);
    CHECK(report.find("project.tick_rate=60.000\n") != std::string::npos);
    CHECK(report.find("project.instrument_count=1\n") != std::string::npos);
    CHECK(report.find("02:AY Classic") != std::string::npos);
    CHECK(report.find("last_project_file_error=sample failure\n") != std::string::npos);

    // The support report deliberately avoids project payload and user-authored
    // title/author text.
    CHECK(report.find("project.title=") == std::string::npos);
    CHECK(report.find("project.author=") == std::string::npos);
  }

  chipnomadDestroy(state);
  chipnomadState = savedState;
  appSettings = savedSettings;
  projectModified = savedModified;
  memcpy(projectFileError, savedError, sizeof(savedError));
  fs::remove(path);
}
