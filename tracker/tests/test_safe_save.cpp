// Safe saving: projects and samples are written beside the original and
// renamed into place, so a failed save keeps the old file.
#include "doctest.h"
#include "../../chipnomad_lib/project.h"
#include "../../chipnomad_lib/project_instruments.h"
#include "../../chipnomad_lib/synth/sample_voice.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#ifdef _WIN32
#include <direct.h>
static int makeDirectory(const char* path) { return _mkdir(path); }
static int removeDirectory(const char* path) { return _rmdir(path); }
#else
#include <sys/stat.h>
#include <unistd.h>
static int makeDirectory(const char* path) { return mkdir(path, 0755); }
static int removeDirectory(const char* path) { return rmdir(path); }
#endif

TEST_SUITE("safe_save") {

namespace {

InstrumentSample constantSample(std::vector<int16_t>& pcm, int16_t value) {
  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));
  pcm.assign(1000, value);
  sample.data = pcm.data();
  sample.frameCount = (uint32_t)pcm.size();
  sample.channels = 1;
  sample.sampleRate = 44100;
  return sample;
}

bool fileExists(const char* path) {
  FILE* file = std::fopen(path, "rb");
  if (file) std::fclose(file);
  return file != nullptr;
}

std::string readFile(const char* path) {
  std::string text;
  FILE* file = std::fopen(path, "rb");
  if (!file) return text;
  char buffer[4096];
  size_t n;
  while ((n = std::fread(buffer, 1, sizeof(buffer), file)) > 0) text.append(buffer, n);
  std::fclose(file);
  return text;
}

}  // namespace

TEST_CASE("Saving a sample over an existing WAV replaces it and leaves no temporary file") {
  std::vector<int16_t> oldPcm, newPcm;
  InstrumentSample oldSample = constantSample(oldPcm, 1000), newSample = constantSample(newPcm, -2000);
  char error[64];
  REQUIRE(sampleSaveWav16(&oldSample, "test_safe_save.wav", error, sizeof(error)) == 0);
  REQUIRE(sampleSaveWav16(&newSample, "test_safe_save.wav", error, sizeof(error)) == 0);
  CHECK_FALSE(fileExists("test_safe_save.wav.tmp"));
  InstrumentSample loaded;
  std::memset(&loaded, 0, sizeof(loaded));
  REQUIRE(sampleLoadWav16("test_safe_save.wav", &loaded, error, sizeof(error)) == 0);
  CHECK(loaded.frameCount == 1000);
  CHECK(loaded.data[0] == -2000);
  free(loaded.data);
  std::remove("test_safe_save.wav");
}

TEST_CASE("A sample save that cannot be written keeps the old WAV") {
  std::vector<int16_t> oldPcm, newPcm;
  InstrumentSample oldSample = constantSample(oldPcm, 1000), newSample = constantSample(newPcm, -2000);
  char error[64];
  REQUIRE(sampleSaveWav16(&oldSample, "test_safe_save.wav", error, sizeof(error)) == 0);
  const std::string before = readFile("test_safe_save.wav");
  REQUIRE(makeDirectory("test_safe_save.wav.tmp") == 0);  // Blocks the temporary file
  CHECK(sampleSaveWav16(&newSample, "test_safe_save.wav", error, sizeof(error)) == 1);
  CHECK(readFile("test_safe_save.wav") == before);
  removeDirectory("test_safe_save.wav.tmp");
  std::remove("test_safe_save.wav");
}

TEST_CASE("A project save that cannot be written keeps the old project") {
  auto project = std::make_unique<Project>();
  projectInit(project.get());
  REQUIRE(projectLoad(project.get(), "packaging/common/projects/gm-midi-demo.cct") == 0);
  REQUIRE(projectSave(project.get(), "test_safe_save.cct") == 0);
  CHECK_FALSE(fileExists("test_safe_save.cct.tmp"));
  const std::string before = readFile("test_safe_save.cct");
  REQUIRE(makeDirectory("test_safe_save.cct.tmp") == 0);  // Blocks the temporary file
  std::strcpy(project->title, "Changed");
  CHECK(projectSave(project.get(), "test_safe_save.cct") == 1);
  CHECK(readFile("test_safe_save.cct") == before);
  removeDirectory("test_safe_save.cct.tmp");
  std::remove("test_safe_save.cct");
  projectFree(project.get());
}

}  // TEST_SUITE
