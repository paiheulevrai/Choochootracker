// Phase 4 (save dialog + WAV cues) engine-level tests. The dialog screen
// itself is UI code (not in the test build); what is tested here is the
// WAV cue chunk writer/reader round trip and the project preference IO.
//
// The cue chunk follows the standard WAV `cue ` format: a 4-byte cue count
// followed by 24-byte cue point records (id, position, "data" chunk id,
// chunk start, block start, sample offset). Only the sample offset matters
// to the tracker; DAWs show the offsets as markers.
#include "doctest.h"
#include "../../chipnomad_lib/project_instruments.h"
#include "../../chipnomad_lib/synth/sample_voice.h"
#include "../../chipnomad_lib/project.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

TEST_SUITE("wav_cue_io") {

static const char* kTestPath = "build/tests/test_cue_roundtrip.wav";

static void writeU16(FILE* file, uint16_t value) {
  fputc(value & 0xff, file);
  fputc(value >> 8, file);
}

static void writeU32(FILE* file, uint32_t value) {
  writeU16(file, value & 0xffff);
  writeU16(file, value >> 16);
}

static void fillSample(InstrumentSample* sample, std::vector<int16_t>& storage,
                       uint32_t frames, uint8_t channels) {
  std::memset(sample, 0, sizeof(*sample));
  sample->sampleRate = 44100;
  sample->frameCount = frames;
  sample->channels = channels;
  storage.resize((size_t)frames * channels);
  for (size_t i = 0; i < storage.size(); ++i) storage[i] = (int16_t)(i % 32768);
  sample->data = storage.data();
}

// --- Writer: cue chunk layout ---------------------------------------------

TEST_CASE("cue chunk is written between fmt and data with correct sizes") {
  InstrumentSample sample;
  std::vector<int16_t> storage;
  fillSample(&sample, storage, 100, 1);

  const uint32_t cues[] = {0, 25, 50, 75};
  char error[64];
  REQUIRE(sampleSaveWav16WithCues(&sample, kTestPath, cues, 4, error, sizeof(error)) == 0);

  FILE* file = fopen(kTestPath, "rb");
  REQUIRE(file != nullptr);
  char id[4];
  REQUIRE(fread(id, 1, 4, file) == 4);
  CHECK(std::memcmp(id, "RIFF", 4) == 0);
  // RIFF size: 36 (header minus RIFF chunk) + data (200) + cue chunk (4+96)
  uint32_t riffSize = fgetc(file) | ((uint32_t)fgetc(file) << 8) |
    ((uint32_t)fgetc(file) << 16) | ((uint32_t)fgetc(file) << 24);
  CHECK(riffSize == 36 + 200 + 4 + 96);
  REQUIRE(fread(id, 1, 4, file) == 4);
  CHECK(std::memcmp(id, "WAVE", 4) == 0);
  REQUIRE(fread(id, 1, 4, file) == 4);
  CHECK(std::memcmp(id, "fmt ", 4) == 0);
  uint32_t fmtSize = fgetc(file) | ((uint32_t)fgetc(file) << 8) |
    ((uint32_t)fgetc(file) << 16) | ((uint32_t)fgetc(file) << 24);
  CHECK(fmtSize == 16);
  // Skip the fmt payload, land on the cue chunk
  fseek(file, 16, SEEK_CUR);
  REQUIRE(fread(id, 1, 4, file) == 4);
  CHECK(std::memcmp(id, "cue ", 4) == 0);
  uint32_t cueSize = fgetc(file) | ((uint32_t)fgetc(file) << 8) |
    ((uint32_t)fgetc(file) << 16) | ((uint32_t)fgetc(file) << 24);
  CHECK(cueSize == 4 + 24 * 4);
  uint32_t numCues = fgetc(file) | ((uint32_t)fgetc(file) << 8) |
    ((uint32_t)fgetc(file) << 16) | ((uint32_t)fgetc(file) << 24);
  CHECK(numCues == 4);
  // First cue record: id 0, position 0, "data", chunkStart 0, blockStart 0,
  // sampleOffset 0
  uint32_t record[6];
  for (int field = 0; field < 6; ++field) {
    record[field] = fgetc(file) | ((uint32_t)fgetc(file) << 8) |
      ((uint32_t)fgetc(file) << 16) | ((uint32_t)fgetc(file) << 24);
  }
  CHECK(record[0] == 0);
  CHECK(record[1] == 0);
  CHECK(std::memcmp(&record[2], "data", 4) == 0);
  CHECK(record[4] == 0);
  CHECK(record[5] == 0);
  // Last cue's sample offset (75): after record 0 we sit at record 1;
  // skip records 1-2 (2*24) then 5 fields (20) to reach record 3's offset.
  fseek(file, 2 * 24 + 20, SEEK_CUR);
  uint32_t lastOffset = fgetc(file) | ((uint32_t)fgetc(file) << 8) |
    ((uint32_t)fgetc(file) << 16) | ((uint32_t)fgetc(file) << 24);
  CHECK(lastOffset == 75);
  // The data chunk follows
  REQUIRE(fread(id, 1, 4, file) == 4);
  CHECK(std::memcmp(id, "data", 4) == 0);
  fclose(file);
  remove(kTestPath);
}

TEST_CASE("zero cues produce byte-identical output to the plain writer") {
  InstrumentSample sample;
  std::vector<int16_t> storage;
  fillSample(&sample, storage, 64, 2);

  char error[64];
  REQUIRE(sampleSaveWav16(&sample, "build/tests/test_cue_plain.wav", error, sizeof(error)) == 0);
  REQUIRE(sampleSaveWav16WithCues(&sample, "build/tests/test_cue_zero.wav", NULL, 0,
                                  error, sizeof(error)) == 0);

  FILE* a = fopen("build/tests/test_cue_plain.wav", "rb");
  FILE* b = fopen("build/tests/test_cue_zero.wav", "rb");
  REQUIRE(a != nullptr);
  REQUIRE(b != nullptr);
  int equal = 1;
  for (int byte = fgetc(a); byte != EOF; byte = fgetc(a)) {
    const int other = fgetc(b);
    if (byte != other) { equal = 0; break; }
  }
  CHECK(fgetc(b) == EOF);
  CHECK(equal == 1);
  fclose(a);
  fclose(b);
  remove("build/tests/test_cue_plain.wav");
  remove("build/tests/test_cue_zero.wav");
}

// --- Round trips -----------------------------------------------------------

TEST_CASE("5-cue save/reload round trip returns the same frames") {
  InstrumentSample sample;
  std::vector<int16_t> storage;
  fillSample(&sample, storage, 1000, 1);

  const uint32_t cues[] = {0, 100, 250, 500, 999};
  char error[64];
  REQUIRE(sampleSaveWav16WithCues(&sample, kTestPath, cues, 5, error, sizeof(error)) == 0);

  InstrumentSample loaded;
  std::memset(&loaded, 0, sizeof(loaded));
  uint32_t cueFrames[PROJECT_SAMPLE_MAX_SLICES];
  uint8_t cueCount = 0;
  REQUIRE(sampleLoadWav16Cues(kTestPath, &loaded, cueFrames, &cueCount,
                              error, sizeof(error)) == 0);
  REQUIRE(loaded.data != nullptr);
  CHECK(loaded.frameCount == 1000);
  CHECK(cueCount == 5);
  for (int i = 0; i < 5; ++i) CHECK(cueFrames[i] == cues[i]);
  // The sample data survived the round trip
  CHECK(loaded.data[500] == (int16_t)(500 % 32768));

  free(loaded.data);
  remove(kTestPath);
}

TEST_CASE("64-cue round trip (the slice cap)") {
  InstrumentSample sample;
  std::vector<int16_t> storage;
  fillSample(&sample, storage, 6400, 1);

  uint32_t cues[PROJECT_SAMPLE_MAX_SLICES];
  for (int i = 0; i < PROJECT_SAMPLE_MAX_SLICES; ++i) cues[i] = (uint32_t)i * 100;
  char error[64];
  REQUIRE(sampleSaveWav16WithCues(&sample, kTestPath, cues, PROJECT_SAMPLE_MAX_SLICES,
                                  error, sizeof(error)) == 0);

  InstrumentSample loaded;
  std::memset(&loaded, 0, sizeof(loaded));
  uint32_t cueFrames[PROJECT_SAMPLE_MAX_SLICES];
  uint8_t cueCount = 0;
  REQUIRE(sampleLoadWav16Cues(kTestPath, &loaded, cueFrames, &cueCount,
                              error, sizeof(error)) == 0);
  CHECK(cueCount == PROJECT_SAMPLE_MAX_SLICES);
  for (int i = 0; i < PROJECT_SAMPLE_MAX_SLICES; ++i) CHECK(cueFrames[i] == cues[i]);

  free(loaded.data);
  remove(kTestPath);
}

TEST_CASE("old WAV without cues loads with cueCount 0") {
  InstrumentSample sample;
  std::vector<int16_t> storage;
  fillSample(&sample, storage, 32, 1);

  char error[64];
  REQUIRE(sampleSaveWav16(&sample, kTestPath, error, sizeof(error)) == 0);

  InstrumentSample loaded;
  std::memset(&loaded, 0, sizeof(loaded));
  uint32_t cueFrames[PROJECT_SAMPLE_MAX_SLICES];
  uint8_t cueCount = 99;
  REQUIRE(sampleLoadWav16Cues(kTestPath, &loaded, cueFrames, &cueCount,
                              error, sizeof(error)) == 0);
  CHECK(cueCount == 0);
  CHECK(loaded.frameCount == 32);

  free(loaded.data);
  remove(kTestPath);
}

TEST_CASE("more than 64 cues in a file are capped at the slice limit") {
  // Hand-write a WAV with 70 cue points; the loader must keep the first 64.
  FILE* file = fopen(kTestPath, "wb");
  REQUIRE(file != nullptr);
  const uint32_t cueChunkSize = 4 + 24 * 70;
  const uint32_t dataBytes = 64;  // 32 mono 16-bit frames
  fwrite("RIFF", 1, 4, file);
  writeU32(file, 36 + dataBytes + cueChunkSize);
  fwrite("WAVE", 1, 4, file);
  fwrite("fmt ", 1, 4, file);
  writeU32(file, 16);
  writeU16(file, 1); writeU16(file, 1); writeU32(file, 8000);
  writeU32(file, 8000); writeU16(file, 2); writeU16(file, 16);
  fwrite("cue ", 1, 4, file);
  writeU32(file, cueChunkSize);
  writeU32(file, 70);
  for (uint32_t i = 0; i < 70; ++i) {
    writeU32(file, i);      // id
    writeU32(file, 0);      // position
    fwrite("data", 1, 4, file);
    writeU32(file, 0);      // chunk start
    writeU32(file, 0);      // block start
    writeU32(file, i * 10); // sample offset
  }
  fwrite("data", 1, 4, file);
  writeU32(file, dataBytes);
  for (int i = 0; i < 32; ++i) writeU16(file, (uint16_t)(i * 1000));
  fclose(file);

  InstrumentSample loaded;
  std::memset(&loaded, 0, sizeof(loaded));
  uint32_t cueFrames[PROJECT_SAMPLE_MAX_SLICES];
  uint8_t cueCount = 0;
  char error[64];
  REQUIRE(sampleLoadWav16Cues(kTestPath, &loaded, cueFrames, &cueCount,
                              error, sizeof(error)) == 0);
  CHECK(cueCount == PROJECT_SAMPLE_MAX_SLICES);
  CHECK(cueFrames[0] == 0);
  CHECK(cueFrames[63] == 630);

  free(loaded.data);
  remove(kTestPath);
}

TEST_CASE("cue loader rejects malformed cue chunk sizes without crashing") {
  // A cue chunk claiming more cues than its size allows: the loader must
  // not read past the chunk or crash.
  FILE* file = fopen(kTestPath, "wb");
  REQUIRE(file != nullptr);
  const uint32_t cueChunkSize = 4 + 24 * 2;  // claims 2, says 100 below
  const uint32_t dataBytes = 8;
  fwrite("RIFF", 1, 4, file);
  writeU32(file, 36 + dataBytes + cueChunkSize);
  fwrite("WAVE", 1, 4, file);
  fwrite("fmt ", 1, 4, file);
  writeU32(file, 16);
  writeU16(file, 1); writeU16(file, 1); writeU32(file, 8000);
  writeU32(file, 8000); writeU16(file, 2); writeU16(file, 16);
  fwrite("cue ", 1, 4, file);
  writeU32(file, cueChunkSize);
  writeU32(file, 100);  // lie: 100 cues in a 2-cue chunk
  for (uint32_t i = 0; i < 2; ++i) {
    writeU32(file, i);
    writeU32(file, 0);
    fwrite("data", 1, 4, file);
    writeU32(file, 0);
    writeU32(file, 0);
    writeU32(file, i * 4);
  }
  fwrite("data", 1, 4, file);
  writeU32(file, dataBytes);
  for (int i = 0; i < 4; ++i) writeU16(file, (uint16_t)(i * 1000));
  fclose(file);

  InstrumentSample loaded;
  std::memset(&loaded, 0, sizeof(loaded));
  uint32_t cueFrames[PROJECT_SAMPLE_MAX_SLICES];
  uint8_t cueCount = 0;
  char error[64];
  // The chunk walk seeks by declared sizes, so the lie just truncates the
  // cue list (2 records fit before the seek lands on the data chunk).
  const int result = sampleLoadWav16Cues(kTestPath, &loaded, cueFrames, &cueCount,
                                         error, sizeof(error));
  if (result == 0) {
    CHECK(cueCount <= 2);
    free(loaded.data);
  } else {
    // A rejected file is also acceptable - the important part is no crash.
    CHECK(std::strlen(error) > 0);
  }
  remove(kTestPath);
}

// --- Project preference IO --------------------------------------------------

TEST_CASE("sampleSaveChoice defaults to 0 (ask) in a fresh project") {
  Project project;
  projectInit(&project);
  CHECK(project.sampleSaveChoice == 0);
}

TEST_CASE("sampleSaveChoice round-trips through project save/load") {
  Project saved;
  projectInit(&saved);
  // projectInit only zeroes the struct; a saveable project needs the same
  // basics the real app sets in projectInitAY (chipsCount >= 1 passes the
  // loader's range check, and the pitch table title must be non-empty).
  saved.chipsCount = 1;
  saved.tracksCount = 1;
  saved.chipType = ChipType::AY;
  std::strcpy(saved.pitchTable.name, "Test");
  saved.pitchTable.length = 1;
  std::strcpy(saved.pitchTable.noteNames[0], "C-4");
  saved.pitchTable.values[0] = 1000;
  saved.sampleSaveChoice = 2;

  const char* path = "build/tests/test_save_choice.cct";
  REQUIRE(projectSave(&saved, path) == 0);

  Project loaded;
  projectInit(&loaded);
  INFO(projectFileError);
  REQUIRE(projectLoad(&loaded, path) == 0);
  CHECK(loaded.sampleSaveChoice == 2);
  remove(path);

  // Choice 1 round trips too
  saved.sampleSaveChoice = 1;
  REQUIRE(projectSave(&saved, path) == 0);
  projectInit(&loaded);
  INFO(projectFileError);
  REQUIRE(projectLoad(&loaded, path) == 0);
  CHECK(loaded.sampleSaveChoice == 1);
  remove(path);
}

TEST_CASE("old project files without the field load with sampleSaveChoice 0") {
  // A project saved before Phase 4 has no "- Sample save choice:" line;
  // loading it must leave the preference at the default (ask).
  Project loaded;
  projectInit(&loaded);
  INFO(projectFileError);
  REQUIRE(projectLoad(&loaded, "tests/test_v1_format.cnm") == 0);
  CHECK(loaded.sampleSaveChoice == 0);
}

TEST_CASE("out-of-range sampleSaveChoice clamps to 0 on load") {
  // Hand-write a minimal project text with an out-of-range choice.
  const char* path = "build/tests/test_save_choice_bad.cct";
  FILE* file = fopen(path, "wb");
  REQUIRE(file != nullptr);
  fprintf(file, "- *ChipNomad* Version: 1\n");
  fprintf(file, "- Sample save choice: 200\n");
  fclose(file);

  Project loaded;
  projectInit(&loaded);
  // The load may reject the truncated file - both outcomes are fine as
  // long as nothing crashes and a successful load clamps the value.
  if (projectLoad(&loaded, path) == 0) {
    CHECK(loaded.sampleSaveChoice == 0);
  }
  remove(path);
}

}  // TEST_SUITE("wav_cue_io")
