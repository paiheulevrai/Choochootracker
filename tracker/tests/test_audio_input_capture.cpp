// Smoke test for the corelib audio capture API (Phase 1.6).
//
// The test build links tests/mocks/corelib_audio_mock.cpp, so this test
// verifies the stub contract: no devices, setup fails cleanly, pause and
// cleanup are safe no-ops. On real SDL2 platforms the same API surface is
// exercised manually (see docs/build-notes.md).
//
// Gated to platforms that ship the feature; the mock provides the same
// symbols everywhere, so this compiles in all test configurations.
#include "doctest.h"
#include "corelib_audio.h"
#include "common.h"

#include <cstring>

TEST_CASE("audio input capture API stub contract") {
  // Enumeration reports no devices in the test environment.
  CHECK(audioInputDeviceCount() == 0);

  // Out-of-range name query fails and leaves the buffer empty.
  char name[AUDIO_DEVICE_NAME_LENGTH + 1] = {0};
  REQUIRE(audioInputDeviceName(0, name, sizeof(name)) != 0);
  CHECK(name[0] == 0);

  // Setup without a device fails with a non-zero result and must not crash.
  AudioCaptureCallback* cb = nullptr;
  CHECK(audioInputSetup(cb, 48000, 512, 2, -1) != 0);

  // Pause/cleanup are safe no-ops when nothing is open.
  audioInputPause(1);
  audioInputPause(0);
  audioInputCleanup();
}
