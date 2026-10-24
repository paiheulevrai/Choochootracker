// Unit tests for the recording pipeline (Phase 8, T28): WAV validity of a
// captured session, track-bounce content, and clear primitives.
#include "doctest.h"
#include "audio_recorder.h"
#include "audio_input_manager.h"
#include "track_bounce.h"
#include "corelib_audio.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>

static void removeFile(const char* path) { remove(path); }

TEST_SUITE("recording") {

TEST_CASE("external recording: capture -> ring -> writer produces valid WAV") {
  const char* path = "build/tests/test_recording_session.wav";
  removeFile(path);

  // Arm slot 0 via the test peer (no real device needed).
  AudioInputManagerTestPeer::captureOpenForTest(audioInputManager, true);
  AudioInputManagerTestPeer::armSlotForTest(audioInputManager, 0, 0);
  // Arming starts capture before REC, but must not feed the recorder ring.
  audioInputManager.ring(0)->clear();
  int16_t stale[512 * 2]{};
  audioInputManager.onCapture(stale, 512, 2);
  CHECK(audioInputManager.ring(0)->head.load() == audioInputManager.ring(0)->tail.load());
  // A previous take can still leave buffered data; start discards it.
  REQUIRE(audioInputManager.ring(0)->push(stale, 512));

  RecordingWavWriter writer;
  REQUIRE(writer.open(path, 48000, 2, 16) == 0);
  audioInputManagerStartRecording(&writer, 0);

  // Drive the capture path: 4800 stereo frames of a ramp.
  const int kFrames = 4800;
  std::vector<int16_t> block(kFrames * 2);
  for (int i = 0; i < kFrames; ++i) {
    block[i * 2] = (int16_t)((i % 32768) - 16384);
    block[i * 2 + 1] = (int16_t)(16384 - (i % 32768));
  }
  // Feed in chunks like the audio callback would (512 stereo frames).
  for (int offset = 0; offset < kFrames; offset += 512) {
    int chunk = kFrames - offset < 512 ? kFrames - offset : 512;
    audioInputManager.onCapture(block.data() + offset * 2, chunk, 2);
  }

  // The writer thread drains asynchronously; wait (bounded) until every
  // pushed frame has been written before stopping, so the join cannot cut
  // the capture short.
  for (int i = 0; i < 2000 && writer.totalSamples() < (uint64_t)kFrames; ++i)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  REQUIRE(writer.totalSamples() == (uint64_t)kFrames);
  audioInputManagerStopRecording();
  REQUIRE(writer.close() == 0);
  AudioInputManagerTestPeer::disarmSlotForTest(audioInputManager, 0);
  AudioInputManagerTestPeer::captureOpenForTest(audioInputManager, false);

  // Validate the WAV.
  FILE* f = fopen(path, "rb");
  REQUIRE(f != nullptr);
  uint8_t header[44];
  REQUIRE(fread(header, 1, 44, f) == 44);
  CHECK(memcmp(header, "RIFF", 4) == 0);
  CHECK(memcmp(header + 8, "WAVE", 4) == 0);
  uint32_t dataSize = header[40] | (header[41] << 8) | (header[42] << 16) | ((uint32_t)header[43] << 24);
  CHECK(dataSize == (uint32_t)(kFrames * 2 * 2));
  uint32_t rate = header[24] | (header[25] << 8) | (header[26] << 16) | ((uint32_t)header[27] << 24);
  CHECK(rate == 48000u);
  // Payload: first frame must match the first pushed sample pair.
  int16_t first[2];
  REQUIRE(fread(first, sizeof(int16_t), 2, f) == 2);
  CHECK(first[0] == block[0]);
  CHECK(first[1] == block[1]);
  fclose(f);
  removeFile(path);
}

TEST_CASE("track bounce: engine hook -> ring -> pop round-trip") {
  trackBounceStart(0);
  CHECK(trackBounceActiveTrack() == 0);

  // Simulate the engine: 100 stereo frames via the capture hook.
  const int kFrames = 100;
  for (int i = 0; i < kFrames; ++i) {
    float left = (i % 64) / 64.0f - 0.5f;
    float right = -left;
    audioRecorderCaptureTrackSample(0, i * 2, left);
    audioRecorderCaptureTrackSample(0, i * 2 + 1, right);
  }

  int16_t out[kFrames * 2];
  CHECK(trackBouncePop(out, kFrames) == kFrames);
  // First pair round-trips through the float->int16 conversion.
  CHECK(out[0] == (int16_t)(-0.5f * 32767.0f));
  CHECK(out[1] == (int16_t)(0.5f * 32767.0f));
  // Ring is empty now.
  CHECK(trackBouncePop(out, kFrames) == 0);

  trackBounceStop();
  CHECK(trackBounceActiveTrack() == -1);
}

TEST_CASE("track bounce: wrong track index is ignored") {
  trackBounceStart(2);
  // Captures for other tracks must be dropped.
  audioRecorderCaptureTrackSample(0, 0, 0.5f);
  audioRecorderCaptureTrackSample(0, 1, -0.5f);
  int16_t out[8];
  CHECK(trackBouncePop(out, 4) == 0);
  // Captures for the active track work.
  audioRecorderCaptureTrackSample(2, 0, 0.25f);
  audioRecorderCaptureTrackSample(2, 1, -0.25f);
  CHECK(trackBouncePop(out, 4) == 1);
  CHECK(out[0] == (int16_t)(0.25f * 32767.0f));
  trackBounceStop();
}

TEST_CASE("track bounce: preview returns decimated mono samples") {
  trackBounceStart(1);
  // 6400 stereo samples -> 200 decimated preview entries (16:1 per frame).
  for (int i = 0; i < 6400; ++i)
    audioRecorderCaptureTrackSample(1, i, (i % 32) / 32.0f - 0.5f);
  float preview[256];
  int count = trackBouncePreview(preview, 256);
  CHECK(count == 200);
  for (int i = 0; i < count; ++i) {
    CHECK(preview[i] >= -1.0f);
    CHECK(preview[i] <= 1.0f);
  }
  trackBounceStop();
}

TEST_CASE("track bounce: capture gap is filled with silence") {
  trackBounceStart(0);
  // Frames 0..1 captured, then the voice goes silent and the next capture
  // arrives at frame 50: frames 1..49 must be zero-filled so the WAV stays
  // time-aligned (the tempo bug was exactly this gap being skipped).
  audioRecorderCaptureTrackSample(0, 0, 0.5f);
  audioRecorderCaptureTrackSample(0, 1, -0.5f);
  audioRecorderCaptureTrackSample(0, 100, 0.25f);
  audioRecorderCaptureTrackSample(0, 101, -0.25f);
  // 51 stereo frames total (2 real + 49 silence).
  int16_t out[51 * 2];
  CHECK(trackBouncePop(out, 51) == 51);
  CHECK(out[0] == (int16_t)(0.5f * 32767.0f));
  CHECK(out[1] == (int16_t)(-0.5f * 32767.0f));
  for (int i = 2; i < 100; ++i) {
    CHECK(out[i] == 0);
  }
  CHECK(out[100] == (int16_t)(0.25f * 32767.0f));
  CHECK(out[101] == (int16_t)(-0.25f * 32767.0f));
  trackBounceStop();
}

TEST_CASE("track bounce: end-of-chunk fills remainder with silence") {
  trackBounceStart(0);
  // No capture at all in this chunk: end must emit exactly `frames` zero
  // stereo frames.
  audioRecorderBeginTrackChunk();
  audioRecorderEndTrackChunk(10);
  int16_t out[20];
  CHECK(trackBouncePop(out, 10) == 10);
  for (int i = 0; i < 20; ++i) {
    CHECK(out[i] == 0);
  }
  // A chunk with a partial capture: 3 frames captured, 7 filled.
  audioRecorderBeginTrackChunk();
  audioRecorderCaptureTrackSample(0, 0, 0.5f);
  audioRecorderCaptureTrackSample(0, 1, -0.5f);
  audioRecorderCaptureTrackSample(0, 4, 0.25f);
  audioRecorderCaptureTrackSample(0, 5, -0.25f);
  audioRecorderEndTrackChunk(10);
  CHECK(trackBouncePop(out, 10) == 10);
  CHECK(out[0] == (int16_t)(0.5f * 32767.0f));
  CHECK(out[1] == (int16_t)(-0.5f * 32767.0f));
  CHECK(out[4] == (int16_t)(0.25f * 32767.0f));
  CHECK(out[5] == (int16_t)(-0.25f * 32767.0f));
  for (int i = 6; i < 20; ++i) {
    CHECK(out[i] == 0);
  }
  trackBounceStop();
}

TEST_CASE("track bounce: dropped counter starts at zero") {
  trackBounceStart(0);
  CHECK(trackBounceDropped() == 0u);
  trackBounceStop();
}

TEST_CASE("track bounce: inactive by default") {
  CHECK(trackBounceActiveTrack() == -1);
  CHECK_FALSE(audioRecorderWantsTrackSample(0));
  // stop() keeps buffered data available for draining by design, so clear
  // leftovers from earlier cases before asserting the empty behavior.
  int16_t out[64 * 2];
  while (trackBouncePop(out, 64) > 0) {}
  CHECK(trackBouncePop(out, 4) == 0);
}

} // TEST_SUITE
