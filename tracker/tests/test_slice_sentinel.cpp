#include "doctest.h"

#include "project.h"
#include "project_instruments.h"
#include "synth/sample_voice.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

TEST_SUITE("slice_sentinel") {

// Sentinel encoding: 0 off, 1..64 EQUAL, 65..128 AUTO, 129..192 LAZY.
TEST_CASE("sampleEncodeSlice and decode round-trip every mode") {
  const uint8_t counts[] = {1, 2, 32, 64};
  for (uint8_t count : counts) {
    const uint8_t equal = sampleEncodeSlice(sliceModeEqual, count);
    CHECK(equal == count);
    CHECK(sampleDecodeSliceMode(equal) == sliceModeEqual);
    CHECK(sampleDecodeSliceCount(equal) == count);

    const uint8_t autoSlice = sampleEncodeSlice(sliceModeAuto, count);
    CHECK(autoSlice == (uint8_t)(64 + count));
    CHECK(sampleDecodeSliceMode(autoSlice) == sliceModeAuto);
    CHECK(sampleDecodeSliceCount(autoSlice) == count);

    const uint8_t lazy = sampleEncodeSlice(sliceModeLazy, count);
    CHECK(lazy == (uint8_t)(128 + count));
    CHECK(sampleDecodeSliceMode(lazy) == sliceModeLazy);
    CHECK(sampleDecodeSliceCount(lazy) == count);
  }
}

TEST_CASE("sampleEncodeSlice clamps counts and off always yields 0") {
  CHECK(sampleEncodeSlice(sliceModeOff, 8) == 0);
  CHECK(sampleEncodeSlice(sliceModeOff, 0) == 0);
  CHECK(sampleEncodeSlice(sliceModeEqual, 0) == 1);
  CHECK(sampleEncodeSlice(sliceModeAuto, 0) == 65);
  CHECK(sampleEncodeSlice(sliceModeLazy, 0) == 129);
  CHECK(sampleEncodeSlice(sliceModeEqual, 200) == 64);
  CHECK(sampleEncodeSlice(sliceModeAuto, 200) == 128);
  CHECK(sampleEncodeSlice(sliceModeLazy, 200) == 192);
}

TEST_CASE("sampleDecodeSliceMode rejects reserved and zero values") {
  CHECK(sampleDecodeSliceMode(0) == sliceModeOff);
  CHECK(sampleDecodeSliceMode(193) == sliceModeOff);
  CHECK(sampleDecodeSliceMode(255) == sliceModeOff);
  CHECK(sampleDecodeSliceCount(193) == 0);
  CHECK(sampleDecodeSliceCount(255) == 0);
}

TEST_CASE("sampleNormalizeSliceEx keeps sentinels and zeroes garbage") {
  CHECK(sampleNormalizeSliceEx(0) == 0);
  CHECK(sampleNormalizeSliceEx(1) == 1);
  CHECK(sampleNormalizeSliceEx(8) == 8);
  CHECK(sampleNormalizeSliceEx(65) == 65);
  CHECK(sampleNormalizeSliceEx(192) == 192);
  CHECK(sampleNormalizeSliceEx(193) == 0);
  CHECK(sampleNormalizeSliceEx(255) == 0);
}

TEST_CASE("sampleActsAsSliced truth table") {
  InstrumentSample sample;
  std::memset(&sample, 0, sizeof(sample));

  sample.slice = 0;
  CHECK(sampleActsAsSliced(&sample) == 0);
  sample.slice = 8;
  CHECK(sampleActsAsSliced(&sample) == 1);
  sample.slice = sampleEncodeSlice(sliceModeAuto, 8);
  CHECK(sampleActsAsSliced(&sample) == 1);
  sample.slice = sampleEncodeSlice(sliceModeLazy, 8);
  CHECK(sampleActsAsSliced(&sample) == 1);
  CHECK(sampleActsAsSliced(NULL) == 0);
}

TEST_CASE("legacy slice values load as EQUAL with no bounds") {
  CHECK(sampleDecodeSliceMode(2) == sliceModeEqual);
  CHECK(sampleDecodeSliceMode(4) == sliceModeEqual);
  CHECK(sampleDecodeSliceMode(8) == sliceModeEqual);
  CHECK(sampleDecodeSliceMode(16) == sliceModeEqual);
  CHECK(sampleDecodeSliceMode(32) == sliceModeEqual);
  CHECK(sampleDecodeSliceCount(8) == 8);
}

TEST_CASE("sample instrument init defaults sensitivity and clears bounds") {
  Project p;
  projectInit(&p);
  getInstrumentFunctions(InstrumentType::Sample).init(&p.instruments[0]);
  const InstrumentSample* sample = &p.instruments[0].chip.sample;
  CHECK(sample->autoSensitivity == 50);
  CHECK(sample->slice == 0);
  for (int i = 0; i < PROJECT_SAMPLE_MAX_SLICES; ++i) CHECK(sample->sliceBounds[i] == 0);
}

TEST_CASE("sample sentinel, sensitivity and bounds survive save and load") {
  Project saved, loaded;
  projectInit(&saved);
  projectInit(&loaded);
  saved.chipsCount = 1;
  saved.tracksCount = 1;
  saved.chipType = ChipType::AY;
  std::strcpy(saved.pitchTable.name, "Test");
  saved.pitchTable.length = 1;
  saved.pitchTable.octaveSize = 12;
  std::strcpy(saved.pitchTable.noteNames[0], "C-4");
  saved.pitchTable.values[0] = 1000;
  getInstrumentFunctions(InstrumentType::Sample).init(&saved.instruments[0]);
  std::strcpy(saved.instruments[0].name, "Sliced");
  InstrumentSample* sample = &saved.instruments[0].chip.sample;
  sample->slice = sampleEncodeSlice(sliceModeAuto, 3);
  sample->autoSensitivity = 77;
  sample->sliceBounds[0] = 0;
  sample->sliceBounds[1] = 1234;
  sample->sliceBounds[2] = 5678;

  const char* path = "build/tests/slice_sentinel_io.cct";
  REQUIRE(projectSave(&saved, path) == 0);
  INFO(projectFileError);
  REQUIRE(projectLoad(&loaded, path) == 0);

  CHECK(loaded.instruments[0].type == InstrumentType::Sample);
  const InstrumentSample* loadedSample = &loaded.instruments[0].chip.sample;
  CHECK(loadedSample->slice == sampleEncodeSlice(sliceModeAuto, 3));
  CHECK(sampleDecodeSliceMode(loadedSample->slice) == sliceModeAuto);
  CHECK(sampleDecodeSliceCount(loadedSample->slice) == 3);
  CHECK(loadedSample->autoSensitivity == 77);
  CHECK(loadedSample->sliceBounds[0] == 0);
  CHECK(loadedSample->sliceBounds[1] == 1234);
  CHECK(loadedSample->sliceBounds[2] == 5678);
  CHECK(loadedSample->sliceBounds[3] == 0);
}

TEST_CASE("legacy slice: 8 file loads as EQUAL 8 with empty bounds") {
  Project saved, loaded;
  projectInit(&saved);
  projectInit(&loaded);
  saved.chipsCount = 1;
  saved.tracksCount = 1;
  saved.chipType = ChipType::AY;
  std::strcpy(saved.pitchTable.name, "Test");
  saved.pitchTable.length = 1;
  saved.pitchTable.octaveSize = 12;
  std::strcpy(saved.pitchTable.noteNames[0], "C-4");
  saved.pitchTable.values[0] = 1000;
  getInstrumentFunctions(InstrumentType::Sample).init(&saved.instruments[0]);
  InstrumentSample* sample = &saved.instruments[0].chip.sample;
  sample->slice = 8;

  const char* path = "build/tests/slice_legacy_io.cct";
  REQUIRE(projectSave(&saved, path) == 0);
  INFO(projectFileError);
  REQUIRE(projectLoad(&loaded, path) == 0);

  const InstrumentSample* loadedSample = &loaded.instruments[0].chip.sample;
  CHECK(loadedSample->slice == 8);
  CHECK(sampleDecodeSliceMode(loadedSample->slice) == sliceModeEqual);
  CHECK(sampleDecodeSliceCount(loadedSample->slice) == 8);
  CHECK(loadedSample->autoSensitivity == 50);
  for (int i = 0; i < PROJECT_SAMPLE_MAX_SLICES; ++i) CHECK(loadedSample->sliceBounds[i] == 0);
}

TEST_CASE("reserved sentinel byte loads as off") {
  Project saved, loaded;
  projectInit(&saved);
  projectInit(&loaded);
  saved.chipsCount = 1;
  saved.tracksCount = 1;
  saved.chipType = ChipType::AY;
  std::strcpy(saved.pitchTable.name, "Test");
  saved.pitchTable.length = 1;
  saved.pitchTable.octaveSize = 12;
  std::strcpy(saved.pitchTable.noteNames[0], "C-4");
  saved.pitchTable.values[0] = 1000;
  getInstrumentFunctions(InstrumentType::Sample).init(&saved.instruments[0]);
  saved.instruments[0].chip.sample.slice = 200;

  const char* path = "build/tests/slice_reserved_io.cct";
  REQUIRE(projectSave(&saved, path) == 0);
  INFO(projectFileError);
  REQUIRE(projectLoad(&loaded, path) == 0);
  CHECK(loaded.instruments[0].chip.sample.slice == 0);
}

TEST_CASE("InstrumentSample slicing fields add about 260 bytes") {
  // autoSensitivity (1 byte) + alignment padding + sliceBounds (4 * 64).
  // The exact sizeof delta depends on tail padding, so bound it: the new
  // fields must occupy between 257 and 264 bytes (1 + up to 3 internal
  // padding + 256 bounds + up to 4 tail padding).
  CHECK(sizeof(((InstrumentSample*)0)->sliceBounds) == 64 * sizeof(uint32_t));
  CHECK(sizeof(InstrumentSample) - offsetof(InstrumentSample, autoSensitivity) <= 264);
  CHECK(sizeof(InstrumentSample) - offsetof(InstrumentSample, autoSensitivity) >= 257);
}

} // TEST_SUITE
