// Unit tests for RecordingWavWriter (Phase 3, task T18) and the writer
// thread lifecycle (T19).
#include "doctest.h"
#include "audio_recorder.h"
#include "audio_input_manager.h"
#include "corelib_audio.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

static void removeFile(const char* path) { remove(path); }

TEST_CASE("RecordingWavWriter open/write/close produces a valid WAV") {
  const char* path = "build/tests/test_recording.wav";
  removeFile(path);

  RecordingWavWriter writer;
  REQUIRE(writer.open(path, 48000, 2, 16) == 0);

  const int kFrames = 4800;
  std::vector<int16_t> block(kFrames * 2);
  for (int i = 0; i < kFrames * 2; ++i) block[i] = (int16_t)(i * 7);
  REQUIRE(writer.write(block.data(), kFrames) == 0);
  REQUIRE(writer.totalSamples() == (uint64_t)kFrames);
  REQUIRE(writer.droppedSamples() == 0);
  REQUIRE(writer.close() == 0);

  FILE* f = fopen(path, "rb");
  REQUIRE(f != nullptr);
  uint8_t header[44];
  REQUIRE(fread(header, 1, 44, f) == 44);
  // RIFF/WAVE markers
  CHECK(memcmp(header, "RIFF", 4) == 0);
  CHECK(memcmp(header + 8, "WAVE", 4) == 0);
  // data chunk size = 4800 frames * 2 ch * 2 bytes = 19200
  uint32_t dataSize = header[40] | (header[41] << 8) | (header[42] << 16) | ((uint32_t)header[43] << 24);
  CHECK(dataSize == 19200u);
  // RIFF size = 36 + dataSize
  uint32_t riffSize = header[4] | (header[5] << 8) | (header[6] << 16) | ((uint32_t)header[7] << 24);
  CHECK(riffSize == 36u + 19200u);
  // sample rate 48000 at offset 24
  uint32_t rate = header[24] | (header[25] << 8) | (header[26] << 16) | ((uint32_t)header[27] << 24);
  CHECK(rate == 48000u);
  // channels 2 at offset 22
  uint16_t channels = header[22] | (header[23] << 8);
  CHECK(channels == 2);
  // bits per sample 16 at offset 34
  uint16_t bits = header[34] | (header[35] << 8);
  CHECK(bits == 16);
  // Payload round-trip: first frame
  int16_t first[2];
  REQUIRE(fread(first, sizeof(int16_t), 2, f) == 2);
  CHECK(first[0] == 0);
  CHECK(first[1] == 7);
  fclose(f);
  removeFile(path);
}

TEST_CASE("RecordingWavWriter rejects bad arguments and double open") {
  const char* path = "build/tests/test_recording_bad.wav";
  RecordingWavWriter writer;
  CHECK(writer.open(nullptr, 48000, 2, 16) != 0);
  CHECK(writer.open(path, 0, 2, 16) != 0);
  CHECK(writer.open(path, 48000, 0, 16) != 0);
  CHECK(writer.open(path, 48000, 2, 12) != 0);
  CHECK(writer.open(path, 48000, 2, 16) == 0);
  CHECK(writer.open(path, 48000, 2, 16) != 0); // double open
  writer.close();
  removeFile(path);
}

TEST_CASE("RecordingWavWriter write without open drops and counts") {
  RecordingWavWriter writer;
  int16_t block[8] = {0};
  CHECK(writer.write(block, 4) != 0);
  CHECK(writer.droppedSamples() == 4);
  CHECK(writer.totalSamples() == 0);
}

TEST_CASE("Writer thread lifecycle start/stop without capture") {
  // Stub environment: no capture devices, so arm() fails and the writer
  // thread must still start/stop cleanly against a null ring.
  RecordingWavWriter writer;
  audioInputManagerStartRecording(&writer, 0);
  audioInputManagerStopRecording();
  // Double stop must be safe.
  audioInputManagerStopRecording();
  CHECK(true);
}
