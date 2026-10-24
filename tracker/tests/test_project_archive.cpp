#include "doctest.h"
#include "project.h"
#include "project_utils.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

namespace {
using OwnedProject = std::unique_ptr<Project, void(*)(Project*)>;
OwnedProject newProject() {
  OwnedProject p(new Project, [](Project* p) { projectFree(p); delete p; });
  projectInitAY(p.get());
  return p;
}
std::string readFile(const char* path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void fillSample(InstrumentSample& sample, int channels, int seed) {
  std::strcpy(sample.path, "missing-original-archive-test.wav");
  sample.sampleRate = 32000;
  sample.frameCount = 64;
  sample.channels = channels;
  sample.data = static_cast<int16_t*>(std::malloc(64 * channels * sizeof(int16_t)));
  REQUIRE(sample.data != nullptr);
  for (int n = 0; n < 64 * channels; ++n) sample.data[n] = (n % 2 ? -1 : 1) * (seed + n);
}
void checkSample(const InstrumentSample& expected, const InstrumentSample& actual) {
  REQUIRE(actual.data != nullptr);
  CHECK(actual.frameCount == expected.frameCount);
  CHECK(actual.channels == expected.channels);
  CHECK(actual.sampleRate == expected.sampleRate);
  CHECK(std::strcmp(actual.path, expected.path) == 0);
  REQUIRE(actual.frameCount == expected.frameCount);
  REQUIRE(actual.channels == expected.channels);
  CHECK(std::memcmp(actual.data, expected.data, expected.frameCount * expected.channels * sizeof(int16_t)) == 0);
}
}

TEST_CASE("upstream song archives retain native patches and every sample oscillator") {
  fillFXNames();
  auto saved = newProject(), loaded = newProject(), legacy = newProject();
  const InstrumentType types[] = {InstrumentType::OPLL, InstrumentType::VRC7,
    InstrumentType::OPL2, InstrumentType::OPL3, InstrumentType::SegaPSG,
    InstrumentType::GBPulse, InstrumentType::GBNoise, InstrumentType::DX7,
    InstrumentType::GenesisFM, InstrumentType::ArcadeFM};
  for (int n = 0; n < 10; ++n) getInstrumentFunctions(types[n]).init(&saved->instruments[n + 8]);
  saved->instruments[15].chip.dx7.voice[20] = 12;
  saved->instruments[15].chip.dx7.bankId = 900;
  saved->instruments[15].chip.dx7.fineTune = -37;
  REQUIRE(projectSave(saved.get(), "build/tests/archive-legacy.cct") == 0);
  CHECK(readFile("build/tests/archive-legacy.cct").find("# ChooChooTracker Module 11.0") == 0);
  REQUIRE(projectLoad(legacy.get(), "build/tests/archive-legacy.cct") == 0);

  getInstrumentFunctions(InstrumentType::Sample).init(&saved->instruments[0]);
  fillSample(saved->instruments[0].chip.sample, 2, 1000);
  saved->instruments[0].chip.sample.slice = 4;
  saved->instruments[0].chip.sample.speedPercent = 125;
  getInstrumentFunctions(InstrumentType::SCWF).init(&saved->instruments[2]);
  for (int n = 0; n < 2; ++n) fillSample(saved->instruments[2].chip.scwf.oscillator[n], 1, 2000 + n * 100);
  getInstrumentFunctions(InstrumentType::BYOWTBL).init(&saved->instruments[4]);
  for (int n = 0; n < 2; ++n) {
    auto& wavetable = saved->instruments[4].chip.byowtbl;
    fillSample(wavetable.oscillator[n], 1, 3000 + n * 100);
    wavetable.frameSize[n] = 32; wavetable.tableFrames[n] = 2; wavetable.frameIndex[n] = 1;
  }
  // An empty sample slot must not shift the archive's later sample indices.
  getInstrumentFunctions(InstrumentType::Sample).init(&saved->instruments[1]);
  REQUIRE(projectSave(saved.get(), "build/tests/archive-mixed.cct") == 0);
  CHECK(readFile("build/tests/archive-mixed.cct").substr(0, 2) == "PK");
  REQUIRE(projectLoad(loaded.get(), "build/tests/archive-mixed.cct") == 0);
  checkSample(saved->instruments[0].chip.sample, loaded->instruments[0].chip.sample);
  CHECK(loaded->instruments[0].chip.sample.slice == 4);
  CHECK(loaded->instruments[0].chip.sample.speedPercent == 125);
  CHECK(loaded->instruments[1].chip.sample.data == nullptr);
  for (int n = 0; n < 2; ++n) {
    checkSample(saved->instruments[2].chip.scwf.oscillator[n], loaded->instruments[2].chip.scwf.oscillator[n]);
    checkSample(saved->instruments[4].chip.byowtbl.oscillator[n], loaded->instruments[4].chip.byowtbl.oscillator[n]);
    CHECK(loaded->instruments[4].chip.byowtbl.frameSize[n] == 32);
    CHECK(loaded->instruments[4].chip.byowtbl.tableFrames[n] == 2);
    CHECK(loaded->instruments[4].chip.byowtbl.frameIndex[n] == 1);
  }
  for (int n = 8; n < 18; ++n) {
    CAPTURE(n);
    REQUIRE(instrumentSave(saved.get(), "build/tests/archive-before.cni", n) == 0);
    REQUIRE(instrumentSave(loaded.get(), "build/tests/archive-after.cni", n) == 0);
    REQUIRE(instrumentSave(legacy.get(), "build/tests/archive-legacy.cni", n) == 0);
    CHECK(readFile("build/tests/archive-before.cni") == readFile("build/tests/archive-after.cni"));
    CHECK(readFile("build/tests/archive-before.cni") == readFile("build/tests/archive-legacy.cni"));
  }
  REQUIRE(projectSave(loaded.get(), "build/tests/archive-resaved.cct") == 0);
  CHECK(readFile("build/tests/archive-mixed.cct") == readFile("build/tests/archive-resaved.cct"));
  for (const char* name : {"archive-legacy.cct", "archive-mixed.cct", "archive-resaved.cct",
                          "archive-before.cni", "archive-after.cni", "archive-legacy.cni"})
    std::remove((std::string("build/tests/") + name).c_str());
}
