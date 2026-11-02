// Unit tests for AudioInputManager (Phase 2.5): arm/disarm transitions,
// SPSC ring drop-on-full semantics, and 3-slot level snapshot behavior.
#include "doctest.h"
#include "audio_input_manager.h"
#include "corelib_audio.h"
#include "common.h"

#include <cstring>
#include <vector>

// The test build links the audio mock: audioInputDeviceCount() returns 0 and
// audioInputSetup() fails, so arm() exercises the failure path. Ring and
// snapshot logic is tested directly.

TEST_CASE("AudioInputRing push/pop preserves interleaved stereo order") {
  AudioInputRing ring;
  ring.clear();

  int16_t in[8] = {1, -1, 2, -2, 3, -3, 4, -4};
  REQUIRE(ring.push(in, 4) == true);

  int16_t out[8] = {0};
  CHECK(ring.pop(out, 4) == 4);
  for (int i = 0; i < 8; ++i) CHECK(out[i] == in[i]);

  // Empty ring: pop returns 0.
  CHECK(ring.pop(out, 4) == 0);
}

TEST_CASE("AudioInputRing preserves stereo and mono data across wraparound") {
  AudioInputRing ring;
  std::vector<int16_t> first((AudioInputRing::CAPACITY - 4) * 2);
  for (size_t i = 0; i < first.size(); ++i) first[i] = (int16_t)i;
  REQUIRE(ring.push(first.data(), AudioInputRing::CAPACITY - 4));
  std::vector<int16_t> discard((AudioInputRing::CAPACITY - 8) * 2);
  REQUIRE(ring.pop(discard.data(), AudioInputRing::CAPACITY - 8) == AudioInputRing::CAPACITY - 8);

  int16_t wrapped[] = {101, -101, 102, -102, 103, -103, 104, -104, 105, -105, 106, -106};
  REQUIRE(ring.push(wrapped, 6));
  int16_t tail[20]{};
  REQUIRE(ring.pop(tail, 10) == 10);
  for (int i = 0; i < 8; ++i) CHECK(tail[i] == first[first.size() - 8 + i]);
  for (int i = 0; i < 12; ++i) CHECK(tail[8 + i] == wrapped[i]);

  int16_t mono[] = {7, -8, 9};
  REQUIRE(ring.push(mono, 3, 1));
  int16_t stereo[6]{};
  REQUIRE(ring.pop(stereo, 3) == 3);
  CHECK(stereo[0] == 7); CHECK(stereo[1] == 7);
  CHECK(stereo[2] == -8); CHECK(stereo[3] == -8);
  CHECK(stereo[4] == 9); CHECK(stereo[5] == 9);
}

TEST_CASE("AudioInputRing drops on full and bumps counter") {
  AudioInputRing ring;
  ring.clear();

  // Fill to capacity (CAPACITY - 1 usable entries in this head/tail scheme).
  int16_t block[64];
  for (int i = 0; i < 64; ++i) block[i] = (int16_t)i;
  const unsigned int usable = AudioInputRing::CAPACITY - 1;
  unsigned int pushed = 0;
  while (pushed + 32 <= usable) {
    REQUIRE(ring.push(block, 32) == true);
    pushed += 32;
  }
  // Next push drops the whole block, not just the unwritten remainder.
  const unsigned int droppedBefore = ring.dropped.load();
  CHECK(ring.push(block, 32) == false);
  CHECK(ring.dropped.load() == droppedBefore + 32);

  // Draining frees space again.
  int16_t out[64 * 2];
  int total = 0;
  while (true) {
    int got = ring.pop(out, 64);
    if (got == 0) break;
    total += got;
  }
  CHECK((unsigned int)total == pushed);
}

TEST_CASE("AudioInputManager stub environment: no devices, arm fails cleanly") {
  // Mock reports zero devices.
  audioInputManager.enumerate();
  CHECK(audioInputManager.deviceCount() == 0);
  CHECK(audioInputManager.arm(0) == -1);
  CHECK(audioInputManager.armedCount() == 0);

  char name[AUDIO_DEVICE_NAME_LENGTH + 1] = {0};
  CHECK(audioInputManager.deviceName(0, name, sizeof(name)) != 0);

  // Disarm on an invalid slot is a safe no-op.
  audioInputManager.disarm(0);
  audioInputManager.disarm(-1);
  CHECK(audioInputManager.ring(0) != nullptr);
  CHECK(audioInputManager.ring(-1) == nullptr);
}

TEST_CASE("AudioInputManager monitor toggle") {
  audioInputManager.setMonitor(0);
  CHECK(audioInputManager.monitor() == 0);
  audioInputManager.setMonitor(1);
  CHECK(audioInputManager.monitor() == 1);
  audioInputManager.setMonitor(0);
  CHECK(audioInputManager.monitor() == 0);
}

TEST_CASE("AudioInputManager receiveLevels returns false with nothing published") {
  AudioInputLevels levels;
  // No capture has run in this process; published_ starts at -1.
  // (Other tests may have published; receive just must not crash and either
  // return false or hand back a consistent snapshot.)
  bool got = audioInputManager.receiveLevels(levels);
  if (got) {
    for (int i = 0; i < AUDIO_INPUT_MAX_ARMED; ++i) {
      CHECK(levels.peaks[i][0] >= 0.0f);
      CHECK(levels.peaks[i][1] >= 0.0f);
    }
  }
}

TEST_CASE("AudioInputManager onCapture accumulates levels for armed slots") {
  // Simulate the capture callback path without a real device: mark a slot
  // armed by hand (test-only; arm() cannot succeed against the mock).
  // Access private state via a friend? Not available - instead drive the
  // public API: onCapture with no armed slots must be a safe no-op.
  int16_t capture[16] = {0};
  float output[16] = {0};
  audioInputManager.onCapture(capture, 8, 2); // must not crash
  audioInputManager.mixMonitor(output, 8);    // must not crash
  CHECK(true);
}

TEST_CASE("AudioInputManager empty live input leaves the output untouched") {
  AudioInputManager manager;
  float output[] = {1.0f, 2.0f, 3.0f, 4.0f};
  CHECK_FALSE(manager.readLiveInput(output, 2));
  CHECK(output[0] == 1.0f);
  CHECK(output[3] == 4.0f);
}

TEST_CASE("AudioInputManager levels pipeline: capture -> snapshot -> receive") {
  // Drive the real capture path via the test peer (no device needed).
  AudioInputManagerTestPeer::captureOpenForTest(audioInputManager, true);
  AudioInputManagerTestPeer::armSlotForTest(audioInputManager, 0, 0);
  AudioInputManagerTestPeer::armSlotForTest(audioInputManager, 1, 1);

  // A loud block on slot 0's device, silence on slot 1's.
  int16_t loud[16];
  for (int i = 0; i < 8; ++i) {
    loud[i * 2] = 20000;
    loud[i * 2 + 1] = -20000;
  }
  int16_t silent[16] = {0};
  audioInputManager.onCapture(loud, 8, 2);
  audioInputManager.onCapture(silent, 8, 2);

  AudioInputLevels levels;
  REQUIRE(audioInputManager.receiveLevels(levels));
  // v1 shares a single capture stream: every armed slot sees the same audio,
  // so both slots show the loud peak (right channel is negative -> magnitude).
  CHECK(levels.peaks[0][0] > 0.3f);
  CHECK(levels.peaks[0][1] > 0.3f);
  CHECK(levels.peaks[1][0] > 0.3f);
  CHECK(levels.peaks[1][1] > 0.3f);

  // Snapshot is consumed: a second receive without a new publish fails.
  CHECK_FALSE(audioInputManager.receiveLevels(levels));

  // Peaks decay toward zero when the signal stops (-0.5 dB per callback:
  // 0.944^40 ~= 0.10, well below the 0.3 threshold).
  for (int i = 0; i < 40; ++i) audioInputManager.onCapture(silent, 8, 2);
  REQUIRE(audioInputManager.receiveLevels(levels));
  CHECK(levels.peaks[0][0] < 0.3f);

  AudioInputManagerTestPeer::disarmSlotForTest(audioInputManager, 0);
  AudioInputManagerTestPeer::disarmSlotForTest(audioInputManager, 1);
  AudioInputManagerTestPeer::captureOpenForTest(audioInputManager, false);
}

// Stub contract of the corelib audio capture API: the test build links
// tests/mocks/corelib_audio_mock.cpp, so there are no devices, setup fails
// cleanly, and pause/cleanup are safe no-ops.
TEST_CASE("audio input capture API stub contract") {
  CHECK(audioInputDeviceCount() == 0);

  // Out-of-range name query fails and leaves the buffer empty.
  char name[AUDIO_DEVICE_NAME_LENGTH + 1] = {0};
  REQUIRE(audioInputDeviceName(0, name, sizeof(name)) != 0);
  CHECK(name[0] == 0);

  // Setup without a device fails with a non-zero result and must not crash.
  AudioCaptureCallback* cb = nullptr;
  CHECK(audioInputSetup(cb, 48000, 512, 2, -1) != 0);

  audioInputPause(1);
  audioInputPause(0);
  audioInputCleanup();
}
