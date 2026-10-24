# Ableton Link Sync — Coding LLM Implementation Spec

**Project:** ChooChooTracker (C++17 chip music tracker)
**Repository:** `github.com/am0k161/Choochootracker`, branch `connectivity`
**Working directory for all paths below:** the repo root (paths are relative to repo root)
**Compiler:** C++17 (`-std=c++17`), GCC ≥ 10 on PortMaster, GCC 13+ on Linux desktop, MSVC-equivalent on Windows MSYS2, NDK clang on Android
**Goal:** Add Ableton Link sync to the tracker. Receiver-only. Primary ship target is PortMaster / TrimUI Brick (Allwinner H700, aarch64, TrimUI stock firmware); Windows, Linux, macOS, Android all ship as first-class co-supported targets. Anbernic RG353V is inherited from upstream and runs the same `choochootracker.aarch64` binary but is not the primary test target.

## Critical context (read before starting)

### Receiver-only contract (HARD CONSTRAINT)

The tracker is a **Link receiver only** — it never pushes tempo or transport to other participants. The implementation MUST NOT call any of these Ableton Link APIs:

- `ableton::Link::setTempo()`
- `ableton::Link::Client::setIsPlaying()`
- `ableton::Link::Client::commitAudioSessionState()`
- `ableton::Link::startPlaying()`
- `ableton::Link::stopPlaying()`

The implementation MAY call:
- `ableton::Link::enable()` / `disable()` / `isEnabled()`
- `ableton::Link::captureAudioSessionState()` (read-only, audio thread)
- `sessionState.tempo()` / `beatAtTime(hostTime, quantum)` / `phaseAtTime(hostTime, quantum)` / `numPeers()`
- `sessionState.requestBeatAtTime(beat, hostTime, quantum)` (LOCAL alignment only — does not push to peers)

This constraint is enforced by the `link_client.h` header not exposing any wrapper for the forbidden calls. A future contributor who wants to add `linkClientSetSessionTempo` would have to add a new function — a deliberate, reviewable act.

### Realtime rules (HARD CONSTRAINT)

From `docs/realtime-architecture.md`:
- The audio callback does not edit the project
- The audio callback does not allocate
- The UI owns `Project`. The audio engine owns an execution state with its own copy of the project (`audioProject`).
- UI → audio: SPSC command queue (`chipnomad_lib.cpp:171-422`, capacity 64, 3-slot snapshot)
- Audio → UI: 3-slot atomic snapshot (`PlaybackStatus`)
- The audio callback never reaches back into UI-owned data

All Link integration must respect these rules. Specifically:
- `link->enable()` is NOT realtime-safe — call from UI thread only
- `link->captureAudioSessionState()` IS realtime-safe — call from audio thread at tick boundary
- The Link library's `setTempoCallback` / `setNumPeersCallback` fire on Link-internal threads, NOT the audio thread — they must bump a serial that the UI tick polls

### Tracker clock (existing, do NOT change)

- `Project.tickRate` (Hz, float). Default 50. **24 ticks = 1 beat**.
- `BPM = tickRate * 60 / 24 = tickRate * 2.5`. `tickRate = BPM * 24 / 60 = BPM * 0.4`.
- Engine tick advance at `chipnomad_lib/chipnomad_lib.cpp:1155`:
  ```cpp
  state->frameSampleCounter += state->sampleRate / state->audioProject.tickRate;
  int allTracksStopped = playbackNextFrame(state);
  ```
- Grooves, `SPD`, `DELAY`, ping-pong, tables all consume ticks — they automatically follow `audioProject.tickRate` if it changes.
- The audio callback reads `audioProject.tickRate` (audio-owned), never `project.tickRate` (UI-owned).
- `project_io.cpp:1582` saves `project->tickRate` as `- Frame rate: %f`. This is intentional backwards-compat behavior — **do NOT add an `originalTickRate` field or any file-format change to fix save-while-slaved**.

### Existing precedent: MIDI clock slave

The tracker already has a working sync-source path for MIDI clock slave. Use it as the architectural template:

- `chipnomad_lib/midi/midi_router.h:96-179` — public API with `MidiClockMode` enum, `setClockMode`, `getClockTransport`/`getClockBpm` (serial-bump pattern)
- `chipnomad_lib/midi/midi_router.cpp` — `handleClockTick` at line 220-246 measures inter-clock intervals, runs EMA, quantizes, bumps serial
- `tracker/src/app.cpp:770-794` — UI tick polls `midiRouterGetClockTransport`/`getClockBpm` and writes `project.tickRate`, sets `audioProjectDirty = 1`

The Ableton Link implementation should mirror this exactly: backend-abstracted, serial-bump pattern, UI thread polls and writes to `project.tickRate`.

### Locked UX decisions

- **Connect button**: `[CONNECT]` ↔ `[DISCONNECT]` toggle. Clock source stays as Link after Disconnect.
- **noNetwork recovery**: Manual retry only (user presses Connect again).
- **noPeers transition** (last peer leaves): Hold last session tempo; BPM row stays blurred; status `No sess`.
- **Solo Play** (no peers): Immediate, no quantum wait.
- **Solo Connect** (no peers): Allow; project tempo seeds future joiners.
- **Status strings (short)**: `No net`, `No sess`, `Conn:3` (N = peer count), `Wait...`, `Err`.

### Platform matrix

| Target | Makefile | Link platform define | Status |
|---|---|---|---|
| PortMaster / TrimUI Brick | `Makefile.portmaster` | `-DLINK_PLATFORM_LINUX=1` | Primary ship target (Allwinner H700, aarch64, TrimUI stock firmware, built-in Wi-Fi) |
| PortMaster / Anbernic RG353V | `Makefile.portmaster` (same binary) | `-DLINK_PLATFORM_LINUX=1` | Inherited from upstream (Rockchip RK3566, ArkOS CFW, Wi-Fi variable); works but not primary test target |
| Windows (MSYS2 UCRT64) | `Makefile.windows` | `-DLINK_PLATFORM_WINDOWS=1 -D_WIN32_WINNT=0x0601` | Co-supported |
| Linux / Steam Deck | `Makefile.linux` | `-DLINK_PLATFORM_LINUX=1` | Co-supported |
| macOS (arm64+x86_64) | `Makefile.macos` | `-DLINK_PLATFORM_MACOS=1` | Co-supported |
| Android (arm64-v8a, API 26+) | `Makefile.android` | `-DLINK_PLATFORM_LINUX=1` | Co-supported |
| Web / Emscripten | `Makefile.web` | (stub) | Excluded |
| Miyoo / RG35xx | `Makefile.miyooports` / `Makefile.rg35xx` | (stub) | Excluded |

---

# Task list

Each task is a discrete unit of work with concrete file changes and verification. Tasks are ordered by dependency. Do not skip tasks; do not reorder.

---

## T1 — Add `ClockSource` enum to `common.h`

**Files:**
- `tracker/src/common.h`

**Change:** Add a new enum immediately after line 53 (after `struct TrackVisualSettings { ... };` and before `struct AppSettings {`):

```cpp
// Clock source for engine tempo. Generalized from the old 0/1
// midiClockMode int to a 3-state enum: internal (project.tickRate),
// midiClock (follow incoming MIDI Clock 0xF8 + transport), or link
// (Ableton Link session). On file load, an old `midiClockMode:`
// line in settings.txt is read as a backwards-compat alias: 0 ->
// internal, 1 -> midiClock. New settings write `clockSource: 0|1|2`.
enum class ClockSource : int {
  internal = 0,
  midiClock = 1,
  link = 2,
};
```

**Verify:** `make -f Makefile.linux` builds clean. No new warnings.

---

## T2 — Replace `AppSettings.midiClockMode` with `clockSource`

**Files:**
- `tracker/src/common.h`

**Change:** At line 87, replace:

```cpp
  // Saved to settings.txt: 0 = Internal clock (project tempo), 1 = follow
  // incoming MIDI clock (Project screen "Clock source" row). Applied to the
  // router in appSetup() and changed live from the Project screen.
  int midiClockMode;
```

with:

```cpp
  // Saved to settings.txt as `clockSource: 0|1|2` (internal / midiClock
  // / link). Applied to the router in appSetup() and changed live from
  // the Project screen. Old `midiClockMode:` lines in settings.txt are
  // read as a backwards-compat alias (0 -> internal, 1 -> midiClock).
  ClockSource clockSource;
```

**Verify:** `make -f Makefile.linux` fails to compile (callers of `midiClockMode` still need updating) — that's expected; T3-T9 fix them.

---

## T3 — Update default init in `common.cpp`

**Files:**
- `tracker/src/common.cpp`

**Change:** At line 64, replace `appSettings.midiClockMode = 0;` with `appSettings.clockSource = ClockSource::internal;`

**Verify:** Compiles.

---

## T4 — Update settings save in `common.cpp`

**Files:**
- `tracker/src/common.cpp`

**Change:** At line 210, replace:

```cpp
  fprintf(file, "midiClockMode: %d\n", appSettings.midiClockMode);
```

with:

```cpp
  // Write the canonical `clockSource:` line for v3+; also write the
  // legacy `midiClockMode:` alias (0/1 only, never 2) so older binaries
  // that read the old field still load the Internal/Midi-Clock choice.
  // Link (2) is silently dropped to 0 (Internal) on the alias line.
  fprintf(file, "clockSource: %d\n", (int)appSettings.clockSource);
  fprintf(file, "midiClockMode: %d\n",
    appSettings.clockSource == ClockSource::midiClock ? 1 : 0);
```

**Verify:** Build OK; settings.txt now contains both lines.

---

## T5 — Update settings load in `common.cpp`

**Files:**
- `tracker/src/common.cpp`

**Change:** At lines 353-356, replace:

```cpp
    } else if (strncmp(line, "midiClockMode: ", 15) == 0) {
      int mode = 0;
      if (sscanf(line + 15, "%d", &mode) == 1 && (mode == 0 || mode == 1))
        appSettings.midiClockMode = mode;
```

with:

```cpp
    } else if (strncmp(line, "clockSource: ", 13) == 0) {
      // Canonical v3+ field: 0 = internal, 1 = midiClock, 2 = link.
      int src = 0;
      if (sscanf(line + 13, "%d", &src) == 1 && src >= 0 && src <= 2)
        appSettings.clockSource = (ClockSource)src;
    } else if (strncmp(line, "midiClockMode: ", 15) == 0) {
      // Legacy v2 alias: only used if `clockSource:` was absent earlier
      // in the file. 0 -> internal, 1 -> midiClock. Link (2) cannot be
      // expressed in this field, so the alias is lossy for that state.
      int mode = 0;
      if (sscanf(line + 15, "%d", &mode) == 1 && (mode == 0 || mode == 1))
        appSettings.clockSource = mode ? ClockSource::midiClock : ClockSource::internal;
```

**Verify:** Build OK; load an old settings.txt with only `midiClockMode: 1` → `appSettings.clockSource == ClockSource::midiClock`. Load a new settings.txt with `clockSource: 2` → `appSettings.clockSource == ClockSource::link`.

---

## T6 — Update router init in `app.cpp`

**Files:**
- `tracker/src/app.cpp`

**Change:** At lines 450-451, replace:

```cpp
  midiRouterSetClockMode(chipnomadState->midiRouter,
    appSettings.midiClockMode == 1 ? MidiClockMode::slave : MidiClockMode::off);
```

with:

```cpp
  midiRouterSetClockMode(chipnomadState->midiRouter,
    appSettings.clockSource == ClockSource::midiClock ? MidiClockMode::slave : MidiClockMode::off);
  // Note: ClockSource::link is handled by the future Ableton Link client
  // module (Phase 2+); for now, selecting Link leaves the MIDI router
  // off and is a no-op until that module is wired in.
```

**Verify:** Build OK.

---

## T7 — Update Project screen row 5 mirror init + comment

**Files:**
- `tracker/src/screens/screen_project.cpp`

**Change 1:** At line 35, replace the comment:
```cpp
// Mirror of appSettings.midiClockMode for the Clock source row.
static uint8_t clockSourceMirror = 0;
```
with:
```cpp
// Mirror of appSettings.clockSource for the Clock source row.
// 0 = Internal, 1 = Midi Clock, 2 = Ableton Link (future).
static uint8_t clockSourceMirror = 0;
```

**Change 2:** At line 235, replace:
```cpp
  clockSourceMirror = appSettings.midiClockMode ? 1 : 0;
```
with:
```cpp
  clockSourceMirror = (uint8_t)appSettings.clockSource;
```

**Verify:** Build OK.

---

## T8 — Update Project screen row 5 display to 3-state

**Files:**
- `tracker/src/screens/screen_project.cpp`

**Change:** At lines 358-361, replace:

```cpp
  } else if (row == 5) {
    // Clock source
    gfxClearRect(13, 10, 10, 1);
    gfxPrint(13, 10, clockSourceMirror ? "Midi Clock" : "Internal");
  } else if (row == 6) {
```

with:

```cpp
  } else if (row == 5) {
    // Clock source: Internal / Midi Clock / Ableton Link
    gfxClearRect(13, 10, 27, 1);
    gfxPrint(13, 10,
      clockSourceMirror == 2 ? "Ableton Link" :
      clockSourceMirror == 1 ? "Midi Clock"  :
      "Internal");
  } else if (row == 6) {
```

**Verify:** Build OK. Run the app, navigate to Project screen, row 5 shows "Internal". Tap to cycle through "Midi Clock" then "Ableton Link" then back to "Internal".

---

## T9 — Update Project screen row 5 input handler to 3-state cycle

**Files:**
- `tracker/src/screens/screen_project.cpp`

**Change:** At lines 455-487 (the entire `} else if (row == 5) { ... if (handled) { ... } }` block), replace with:

```cpp
  } else if (row == 5) {
    // Clock source: Internal / Midi Clock / Ableton Link. Persisted in
    // settings.txt as `clockSource: 0|1|2` (the `midiClockMode:` line is
    // also written as a backwards-compat alias for older binaries).
    // Applied to the router live; the audio thread picks it up through
    // the regular project refresh. Link mode (2) is a UI placeholder
    // for now - the actual Link client module is wired up in Phase 2+.
    action = convertMultiAction(action);
    switch (action) {
      case CellEditAction::tap:
      case CellEditAction::doubleTap:
        clockSourceMirror = (clockSourceMirror + 1) % 3;
        handled = 1;
        break;
      case CellEditAction::clear:
        clockSourceMirror = 0;
        handled = 1;
        break;
      case CellEditAction::increase:
      case CellEditAction::increaseBig:
        clockSourceMirror = (clockSourceMirror + 1) % 3;
        handled = 1;
        break;
      case CellEditAction::decrease:
      case CellEditAction::decreaseBig:
        clockSourceMirror = (clockSourceMirror + 2) % 3;
        handled = 1;
        break;
      default:
        break;
    }
    if (handled) {
      appSettings.clockSource = (ClockSource)clockSourceMirror;
      // Only the MIDI router responds today; Link mode (2) leaves the
      // router off - the future Link client module owns its own state.
      midiRouterSetClockMode(chipnomadState->midiRouter,
        appSettings.clockSource == ClockSource::midiClock ? MidiClockMode::slave : MidiClockMode::off);
    }
  } else if (row == 6) {
```

**Verify:** Build OK. Run the app, cycle row 5 through all 3 states, exit, check `settings.txt` contains `clockSource: N` and `midiClockMode: M` lines.

---

## T10 — Add TSan/ASan CI targets

**Files:**
- `tracker/Makefile.test`

**Change:** Add two new targets. The exact recipe depends on the existing test target structure; the additions should be:

```makefile
test-tsan: $(OBJECTS)
        $(CXX) -fsanitize=thread -g -O1 $(OBJECTS) -o $(BUILD)/test-tsan $(LDFLAGS)
        $(BUILD)/test-tsan

test-asan: $(OBJECTS)
        $(CXX) -fsanitize=address -g -O1 $(OBJECTS) -o $(BUILD)/test-asan $(LDFLAGS)
        $(BUILD)/test-asan
```

If the existing Makefile.test doesn't have a single `$(OBJECTS)` list with `$(CXX)` and `$(LDFLAGS)` (e.g. it has its own custom build rule), adapt the additions to match the existing pattern.

**Verify:** `make -f Makefile.test test-tsan` and `make -f Makefile.test test-asan` both run and pass on Linux desktop.

---

## T11 — Verify Phase 0 build

**Action:** Run `make -f Makefile.linux` and `make -f Makefile.test`. Both must build and pass. Then manually run the app on Linux desktop, navigate to the Project screen, cycle row 5 through all 3 states, exit, inspect `settings.txt` to confirm it contains:

```
clockSource: N
midiClockMode: M
```

where N is 0/1/2 and M is 0 or 1 (never 2). Also test loading an old settings.txt that only has `midiClockMode: 1` — `appSettings.clockSource` should be `ClockSource::midiClock`.

**Gate:** Do not proceed to T12 until this passes.

---

## T12 — PortMaster compile spike (gate)

**Action:** Outside the repo tree, fetch `Ableton/link` and `asio/asio` (standalone) into a scratch directory. Write `link_smoke_test.cpp`:

```cpp
#include <ableton/Link.hpp>
#include <chrono>
#include <thread>
#include <cstdio>
int main() {
  ableton::Link link(125.0);
  link.enable();
  std::this_thread::sleep_for(std::chrono::seconds(2));
  auto s = link.captureAudioSessionState();
  std::printf("peers=%d tempo=%f\n", (int)s.numPeers(), s.tempo());
  return 0;
}
```

Compile and run this on the PortMaster Docker build image:

```sh
docker run --rm -v "$(pwd):/workspace" -w /workspace \
  ghcr.io/monkeyx-net/portmaster-build-templates/portmaster-builder:aarch64-latest \
  bash -lc "g++ -std=c++17 -I ableton_link/include -I asio/asio/include \
    link_smoke_test.cpp -o link_smoke_test -lpthread && ./link_smoke_test"
```

(Adjust `-I` paths to where you put the Link and Asio headers.)

**Pass criterion:** Binary builds, links, runs, prints `peers=0 tempo=125.000000` (or close).

**Fail criterion:** If GCC < 10, or if Link uses C++ features the toolchain lacks, STOP. Document the specific compiler error. Either wait for toolchain upgrade or patch Link for the specific compiler.

**Gate:** Do not proceed to T13 until this passes.

---

## T13 — Vendor Link SDK into the tree

**Action:** Vendor `Ableton/link` into `chipnomad_lib/external/ableton_link/`:
- Copy `include/ableton/` (Link headers) → `chipnomad_lib/external/ableton_link/include/ableton/`
- Copy Link's `LICENSE` → `chipnomad_lib/external/ableton_link/LICENSE`
- Create `chipnomad_lib/external/ableton_link/provenance.json` (mirror the format of `chipnomad_lib/external/msfa/provenance.json` — record upstream repo URL, commit hash, license, any patches applied, date vendored)

**Verify:** The directory tree exists with all files. `provenance.json` is valid JSON.

---

## T14 — Vendor Asio standalone into the tree

**Action:** Vendor Asio (standalone, header-only) into `chipnomad_lib/external/asio/`:
- Copy `asio/asio/include/asio/` (headers + .ipp files) → `chipnomad_lib/external/asio/asio/asio/include/asio/`
- Copy Asio's `LICENSE` (BSD-style, in `asio/asio/LICENSE`) → `chipnomad_lib/external/asio/LICENSE`
- Create `chipnomad_lib/external/asio/provenance.json` (same format as T13)

**Verify:** Directory tree exists. A trivial `#include <asio.hpp>` test program compiles on PortMaster Docker image.

---

## T15 — Add Link + Asio to PortMaster Makefile

**Files:**
- `tracker/Makefile.portmaster`

**Change 1:** At line 8, extend `LIBS`:

```makefile
LIBS = $(COMMON_LIBS) platforms/sdl2 ../chipnomad_lib/external/rtmidi \
  ../chipnomad_lib/external/ableton_link ../chipnomad_lib/external/asio
```

**Change 2:** At line 28, add the platform define:

```makefile
CFLAGS = $(PORTMASTER_CFLAGS) $(INCLUDES) $(SDL_CFLAGS) \
  -DPORTMASTER_BUILD -D__LINUX_ALSASEQ__ -DGAMEPAD_SUPPORT -DTOUCH_INPUT \
  -DLINK_PLATFORM_LINUX=1
```

**Verify:** `make -f Makefile.portmaster PortMaster` builds clean (this will only fully succeed after T16-T19 add the same to other Makefiles and T25+ adds the LinkClient module; for now, just verify the Makefile changes don't break the build of the existing code).

---

## T16 — Add Link + Asio to Linux Makefile

**Files:**
- `tracker/Makefile.linux`

**Change 1:** Extend `LIBS` to include `../chipnomad_lib/external/ableton_link ../chipnomad_lib/external/asio` (match the existing pattern in that file).

**Change 2:** Add `-DLINK_PLATFORM_LINUX=1` to the defines.

**Verify:** `make -f Makefile.linux` builds.

---

## T17 — Add Link + Asio to Windows Makefile

**Files:**
- `tracker/Makefile.windows`

**Change 1:** Extend `LIBS`.

**Change 2:** Add `-DLINK_PLATFORM_WINDOWS=1 -D_WIN32_WINNT=0x0601` to defines. The existing `-lws2_32` is sufficient for Asio on Windows.

**Verify:** `make -f Makefile.windows` builds on MSYS2 UCRT64.

---

## T18 — Add Link + Asio to macOS Makefile

**Files:**
- `tracker/Makefile.macos`

**Change 1:** Extend `LIBS`.

**Change 2:** Add `-DLINK_PLATFORM_MACOS=1` to defines. Asio uses BSD sockets on macOS, no extra library needed.

**Verify:** `make -f Makefile.macos` builds.

---

## T19 — Add Link + Asio to Android Makefile

**Files:**
- `tracker/Makefile.android`

**Change 1:** Extend `LIBS`.

**Change 2:** Add `-DLINK_PLATFORM_LINUX=1` to defines (Android uses the Linux kernel + bionic libc, so Asio's Linux path works). The existing `-fPIC` is sufficient.

**Verify:** `make -f Makefile.android android ARCH=arm64-v8a` builds.

---

## T20 — Add Android manifest permissions

**Files:**
- `tracker/platforms/android/app/src/main/AndroidManifest.xml`

**Change:** Add the four permissions inside the `<manifest>` tag (before `<application>`):

```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_WIFI_STATE" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
<uses-permission android:name="android.permission.CHANGE_WIFI_MULTICAST_STATE" />
```

**Verify:** Manifest is valid XML. The Android build (T19) succeeds.

---

## T21 — Add Link smoke test to the test suite

**Files:**
- `tracker/tests/test_link_smoke.cpp` (new)

**Content:** A doctest-based version of T12's smoke test. Gated by `#if defined(DESKTOP_BUILD) || defined(PORTMASTER_BUILD) || defined(ANDROID_BUILD)`:

```cpp
#include "doctest.h"
#if defined(DESKTOP_BUILD) || defined(PORTMASTER_BUILD) || defined(ANDROID_BUILD)
#include <ableton/Link.hpp>
#include <chrono>
#include <thread>

TEST_CASE("Ableton Link smoke test: enable, capture, disable") {
  ableton::Link link(125.0);
  link.enable();
  // Brief sleep to let discovery threads spin up; in a unit test we
  // don't expect peers, we just verify the basic API path works.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  auto session = link.captureAudioSessionState();
  CHECK(session.tempo() == doctest::Approx(125.0).epsilon(0.01));
  CHECK(session.numPeers() == 0);
  link.disable();
}
#endif
```

Add the file to the test build (`Makefile.test`'s test sources list — adapt to the existing pattern).

**Verify:** `make -f Makefile.test` runs the new test on Linux desktop and passes.

---

## T22 — PortMaster Wi-Fi multicast spike (gate)

**Action:** Build T12's `link_smoke_test` for PortMaster. Deploy to a real TrimUI Brick device. Start LinkHut (https://github.com/Ableton/link) on a phone or laptop on the same Wi-Fi. Run the smoke test on the TrimUI Brick. Verify `peers >= 1` prints within 5 seconds.

**Pass criterion:** Peer discovery works on TrimUI Brick stock firmware + Wi-Fi.

**Fail criterion:** If multicast doesn't propagate (firmware firewall, Wi-Fi power save, multicast routing), investigate TrimUI firmware multicast settings. If unresolved, this is a real ship blocker for TrimUI Brick specifically — Windows/Linux/macOS/Android are NOT affected by this gate. The Anbernic RG353V inherited-variant path may have different Wi-Fi multicast behavior on ArkOS CFW; if the maintainer wants RG353V as an officially-supported variant, a separate verification step is needed.

**Gate:** Do not proceed to T23 (PortMaster-specific) until this passes. If it fails, proceed to T23-T31 (the non-PortMaster-specific work) anyway, but flag PortMaster ship as blocked.

---

## T23 — PortMaster CPU budget spike (gate)

**Action:** On the same TrimUI Brick with Link enabled (from T22), run a dense song (`packaging/common/projects/psy.cct` or `DNB.cct`) at the default 512-frame buffer. Use the in-app audio diagnostics (if available) or external measurements (CPU monitor, scope frame drops, audible underruns) to compare against the same song with Link disabled.

**Pass criterion:** No audible dropouts; CPU overhead from Link's network thread is acceptable (target: < 10% extra CPU vs. baseline).

**Fail criterion:** If we lose frames or hear dropouts when Link is on, profile Asio's polling rate. Consider tuning Link's thread priorities. **PortMaster-specific gate.**

**Gate:** Same as T22 — PortMaster-specific.

---

## T24 — Smoke verification on Windows, Linux, macOS, Android

**Action:** On each of Windows, Linux, macOS, Android (real device, not emulator), build and run T12's smoke test with LinkHut running on the same Wi-Fi. Verify `peers >= 1` prints within 5 seconds.

These are NOT gates — they're verification that Link works on each platform. Failures here indicate environment issues (firewall, Android permissions not granted at runtime, etc.) rather than code issues.

---

## T25 — Create the `chipnomad_lib/link/` module directory

**Action:** Create the directory `chipnomad_lib/link/` with five empty files:
- `link_client.h`
- `link_client.cpp`
- `link_backend.h`
- `link_backend_link.cpp`
- `link_backend_stub.cpp`

**Verify:** Directory and empty files exist.

---

## T26 — Define `LinkStatus` enum in `link_client.h`

**Files:**
- `chipnomad_lib/link/link_client.h`

**Content:**

```cpp
#ifndef __CHIPNOMAD_LIB__LINK_CLIENT_H__
#define __CHIPNOMAD_LIB__LINK_CLIENT_H__

#include <stdint.h>

// Receiver-only Ableton Link client. Mirrors the architectural shape of
// chipnomad_lib/midi/midi_router.{h,cpp}: a backend-abstracted client
// that the UI thread polls, with serial-bumped getters so callers apply
// each state change exactly once.
//
// RECEIVER-ONLY CONTRACT — Choochootracker never acts as a Link session
// host. This module MUST NOT call:
//   - ableton::Link::setTempo()
//   - ableton::Link::Client::setIsPlaying()
//   - ableton::Link::Client::commitAudioSessionState()
//   - ableton::Link::startPlaying() / stopPlaying()
// Allowed calls:
//   - enable() / disable() / isEnabled()
//   - captureAudioSessionState() (read-only, audio thread)
//   - sessionState.tempo() / beatAtTime() / phaseAtTime() / numPeers()
//   - sessionState.requestBeatAtTime() (LOCAL alignment only — does not
//     push to peers)
// User BPM edits while slaved are swallowed by the UI (BPM row is
// blurred); they are NOT propagated to the session.
//
// The header never includes <ableton/Link.hpp> and exposes no
// ableton:: type, so it compiles unchanged on platforms where Link is
// stubbed out (Web, Miyoo, RG35xx).

enum class LinkStatus : int {
  disabled  = 0,  // not selected as clock source, link disabled
  selected  = 1,  // clock source = Link, but Connect not pressed yet
  noNetwork = 2,  // Connect pressed, no usable network interface
  connecting = 3, // Connect pressed, mDNS discovery in flight
  noPeers   = 4,  // discovery running, no Link sessions found
  connected = 5,  // 1+ peers, session tempo being applied
  error     = 6,  // Link library reported an internal failure
};

struct LinkClientState;

LinkClientState* linkClientCreate(void);
void linkClientDestroy(LinkClientState*);

// Selecting Link as clock source = a UI preference (no network event).
// Status transitions: any -> selected (or any -> disabled on Deselect).
// link.disable() is called if was previously enabled.
void linkClientSelect(LinkClientState*);
void linkClientDeselect(LinkClientState*);

// Pressing Connect = the actual network opt-in (honors privacy policy).
// Status transitions: selected -> connecting -> (noNetwork | noPeers | connected).
// On Connect: link.enable() is called; status transitions through
// connecting; if no network interface, go to noNetwork; if no peers
// within the discovery threshold, go to noPeers; if peers appear, go
// to connected.
void linkClientConnect(LinkClientState*);

// Pressing Disconnect = leave the session but stay selected as Link.
// Status transitions: any -> selected. link.disable() is called.
void linkClientDisconnect(LinkClientState*);

// Polling API (UI thread, mirrors midi_router's serial-bump pattern).
// getStatus returns the latest status; *serial bumps on every
// transition, so callers comparing serials apply each change exactly
// once. Returns 0 if status has never been observed.
LinkStatus linkClientGetStatus(const LinkClientState*, uint32_t* serial);
int        linkClientGetPeerCount(const LinkClientState*);  // 0 if not connected
float      linkClientGetSessionTempo(const LinkClientState*, uint32_t* serial);  // 0 if not connected

// Called from the audio callback at the tick boundary. Captures the
// latest session state (tempo, peers, phase). Realtime-safe per Link's
// contract. Returns 1 on success, 0 if Link is not enabled.
int linkClientCaptureAudioSessionState(LinkClientState*, double* tempo, uint64_t* peers);

// Called from the audio callback right before playbackStart with peers
// present. Captures the session state, calls
// sessionState.requestBeatAtTime(0, hostTime + outputLatencyMicros +
// schedulingHeadroomMicros, quantum), and returns the absolute host
// time (microseconds) at which playback should begin. If no peers,
// returns 0 (immediate start, per locked decision).
//
// NOTE: requestBeatAtTime takes THREE args (beat, hostTime, quantum) —
// not two. The research note's shorthand "(0, quantum)" was wrong.
uint64_t linkClientRequestBeatAtLaunch(LinkClientState*, int64_t quantum);

#endif // __CHIPNOMAD_LIB__LINK_CLIENT_H__
```

**Verify:** File compiles standalone (just the header).

---

## T27 — Implement `link_backend_link.cpp` (real backend)

**Files:**
- `chipnomad_lib/link/link_backend_link.cpp` (new)

**Content:** Gated by `#if defined(DESKTOP_BUILD) || defined(PORTMASTER_BUILD) || defined(ANDROID_BUILD)`. Holds a `std::unique_ptr<ableton::Link>` member inside `LinkClientState`. Implements every function by delegating to the Link library:

- `linkClientCreate`: construct `Link(125.0)` (initial tempo is the project default; will be overridden by the session tempo once connected)
- `linkClientSelect`/`Deselect`: update status enum only; do NOT call `link.enable()`/`disable()` (Select is preference, Connect is the network opt-in)
- `linkClientConnect`: call `link->enable()`; transition through `connecting`; if `link->numPeers() == 0` after a discovery threshold, go to `noPeers`; if 1+ peers, go to `connected`. Network availability check is platform-specific (Android requires `MulticastLock` first per T28).
- `linkClientDisconnect`: call `link->disable()`; transition to `selected`
- `linkClientGetStatus`: return polled status with bumped serial
- `linkClientGetPeerCount`: return `link->numPeers()` if enabled, else 0
- `linkClientGetSessionTempo`: return `link->captureAudioSessionState().tempo()` if enabled, else 0
- `linkClientCaptureAudioSessionState`: call `link->captureAudioSessionState()` (realtime-safe per Link's contract)
- `linkClientRequestBeatAtLaunch`: if `link->numPeers() == 0`, return 0. Otherwise capture host time via the `HostTimeFilter` (T29), add `outputLatencyMicros + schedulingHeadroomMicros`, call `sessionState.requestBeatAtTime(0, hostTime + offsets, quantum)`, return the host time.

State machine:

```
disabled ──select()──> selected ──connect()──> connecting
                                              │
                                              ├── no network ──> noNetwork (manual retry)
                                              ├── no peers ────> noPeers
                                              └── peers found ─> connected
selected ──deselect()──> disabled
connected ──disconnect()──> selected
noNetwork/noPeers ──disconnect()──> selected
```

**Verify:** Compiles on DESKTOP_BUILD (Linux). Will not link yet — needs `LinkClientState` struct definition and `HostTimeFilter` from T29.

---

## T28 — Implement Android `MulticastLock` JNI bridge

**Files:**
- `tracker/platforms/android/link_backend_android.cpp` (new, parallel to existing `tracker/platforms/android/midi_backend_android.cpp`)

**Content:** JNI calls into `WifiManager` to acquire/release a `MulticastLock`:

- `linkBackendAndroidAcquireMulticastLock()`: get the `WifiManager` via `getSystemService(Context.WIFI_SERVICE)`, create a `MulticastLock` with tag `"choochootracker-link"`, call `acquire()`.
- `linkBackendAndroidReleaseMulticastLock()`: call `release()` on the existing `MulticastLock`.

The `link_backend_link.cpp` (T27) on Android calls these functions in `linkClientConnect` (acquire before `link->enable()`) and `linkClientDisconnect` (release after `link->disable()`).

JNI signatures: match the existing pattern in `midi_backend_android.cpp` (use `JNIEnv*`, `FindClass`, `GetMethodID`, etc.).

**Verify:** Compiles on Android. Real-device test in T24 confirms `peers >= 1` works.

---

## T29 — Implement `HostTimeFilter`

**Files:**
- `chipnomad_lib/link/link_backend_link.cpp` (add to the same file, above the existing functions)

**Content:** Linear regression filter that maps SDL sample counter to host time (microseconds):

- Track pairs of `(sampleCounter, hostTimeMicros)` sampled at audio callback entry. The sample counter is the cumulative count of samples processed since `link.enable()`; the host time is `midiRouterNowMicros()` at callback entry (the existing monotonic clock).
- Maintain a sliding window of the last 64 samples.
- Compute linear regression (`y = a + b*x` where `x = sampleCounter`, `y = hostTimeMicros`) to estimate host time for any sample offset within the current buffer.
- Reset the window on `link.enable()`, on sample-rate change, on buffer-size change.

Implementation note: use `std::chrono::steady_clock` for the host time (matches `midiRouterNowMicros()`). The filter is owned by `LinkClientState` and only accessed from the audio thread (no locks needed).

**Verify:** Unit test in `tracker/tests/test_host_time_filter.cpp` — feed in known pairs, verify the regression produces the right estimate for sample offsets within the buffer.

---

## T30 — Implement `link_backend_stub.cpp` (no-op)

**Files:**
- `chipnomad_lib/link/link_backend_stub.cpp` (new)

**Content:** Gated by `#if !defined(DESKTOP_BUILD) && !defined(PORTMASTER_BUILD) && !defined(ANDROID_BUILD)`. Every function returns 0/disabled:

```cpp
#include "link_client.h"

#if !defined(DESKTOP_BUILD) && !defined(PORTMASTER_BUILD) && !defined(ANDROID_BUILD)

struct LinkClientState { uint32_t statusSerial; };

LinkClientState* linkClientCreate(void) {
  LinkClientState* s = new LinkClientState;
  s->statusSerial = 0;
  return s;
}
void linkClientDestroy(LinkClientState* s) { delete s; }
void linkClientSelect(LinkClientState*) {}
void linkClientDeselect(LinkClientState*) {}
void linkClientConnect(LinkClientState*) {}
void linkClientDisconnect(LinkClientState*) {}

LinkStatus linkClientGetStatus(const LinkClientState*, uint32_t* serial) {
  if (serial) *serial = 0;
  return LinkStatus::disabled;
}
int linkClientGetPeerCount(const LinkClientState*) { return 0; }
float linkClientGetSessionTempo(const LinkClientState*, uint32_t* serial) {
  if (serial) *serial = 0;
  return 0.0f;
}
int linkClientCaptureAudioSessionState(LinkClientState*, double* tempo, uint64_t* peers) {
  if (tempo) *tempo = 0.0;
  if (peers) *peers = 0;
  return 0;
}
uint64_t linkClientRequestBeatAtLaunch(LinkClientState*, int64_t) { return 0; }

#endif
```

**Verify:** Compiles on WEB_BUILD, MIYOOPORTS_BUILD, RG35XX_BUILD.

---

## T31 — Implement `link_client.cpp` (router-layer logic)

**Files:**
- `chipnomad_lib/link/link_client.cpp` (new)

**Content:** Owns the `LinkClientState` struct (declared in `link_client.h` as opaque), manages status transitions, fires the status serial on every change. Delegates to backend functions for actual Link library calls. The struct holds:
- `std::atomic<LinkStatus> status`
- `std::atomic<uint32_t> statusSerial`
- `std::atomic<float> sessionTempo` (cached from last capture, for UI poll)
- `std::atomic<uint32_t> tempoSerial`
- `std::atomic<int> peerCount`
- Backend-specific state (e.g. `std::unique_ptr<ableton::Link>` on real backend)

State transitions per the state machine in T27. Every transition bumps `statusSerial` (use `fetch_add(1, std::memory_order_acq_rel)`).

UI thread reads via `linkClientGetStatus`/`getTempo`/`getPeerCount` — these only read atomics, never touch the Link library, safe to call from UI.

Audio thread calls `linkClientCaptureAudioSessionState`/`requestBeatAtLaunch` — these DO touch the Link library, must only be called from `advancePlaybackFrame`.

**Verify:** Compiles on all platforms (real backend + stub). Unit tests in T33 pass.

---

## T32 — Add `link/` to `chipnomad_lib.h`, `ChipNomadState`, and `Makefile.common`

**Files:**
- `chipnomad_lib/chipnomad_lib.h`
- `chipnomad_lib/chipnomad_lib.cpp`
- `tracker/Makefile.common`

**Changes:**

In `chipnomad_lib.h`:
- Add `#include "link/link_client.h"` to the public API surface (near the existing `#include "midi/midi_router.h"`)
- Add `LinkClientState* linkClient;` to the `ChipNomadState` struct (near the existing `MidiRouterState* midiRouter;`)

In `chipnomad_lib.cpp`:
- In `chipnomadCreate()`: add `state->linkClient = linkClientCreate();` after `state->midiRouter = midiRouterCreate();`
- In `chipnomadDestroy()`: add `linkClientDestroy(state->linkClient);` before `midiRouterDestroy(state->midiRouter);`

In `tracker/Makefile.common`:
- Add `../chipnomad_lib/link` to `COMMON_LIBS`. The stub vs. real backend selection is handled by the `#if` gates inside the .cpp files.

**Verify:** `make -f Makefile.linux` builds. `chipnomadCreate()` returns a state with a non-null `linkClient`.

---

## T33 — Add `LinkClient` unit tests

**Files:**
- `tracker/tests/test_link_client.cpp` (new)

**Content:** Tests for:
- Serial-bump semantics: `getStatus` returns the same value until a state change bumps the serial
- Stub backend returns 0/disabled on every call (verify on a non-DESKTOP/non-PORTMASTER build, or via a mock)
- `select` then `deselect` returns to `disabled` without firing any network event
- `connect` then `disconnect` returns to `selected`
- State transitions per the state machine in T27

For testing on desktop without real Link hardware, the test should use a mock backend (similar to `tracker/tests/mocks/corelib_audio_mock.cpp`) that records every call to `enable`/`disable`/`captureAudioSessionState`/`requestBeatAtTime` and lets the test verify them.

**Verify:** `make -f Makefile.test` runs and passes the new tests.

---

## T34 — Add Link tempo poll to `app.cpp` UI tick

**Files:**
- `tracker/src/app.cpp`

**Change:** Immediately after the existing MIDI-clock-slave block at lines 770-794, add a parallel Link block:

```cpp
      // Ableton Link tempo sync: receiver-only. The UI thread polls
      // the Link client's session tempo (the serial bumps on every
      // change), and applies it to project.tickRate via the regular
      // project-refresh path (audioProjectDirty = 1; the snapshot
      // propagates on the next tick boundary). projectModified stays
      // 0 - following a session tempo isn't a user edit. The Project
      // screen's Tempo row shows the live value while Link is connected.
      //
      // Per receiver-only contract: we never call setTempo or
      // commitAudioSessionState. We only read the session state captured
      // by the audio callback (linkClientCaptureAudioSessionState) and
      // apply it here on the UI thread.
      if (appSettings.clockSource == ClockSource::link) {
        float linkBpm = 0; uint32_t linkBpmSerial = 0;
        if (linkClientGetSessionTempo(chipnomadState->linkClient, &linkBpm, &linkBpmSerial) &&
            linkBpmSerial != linkBpmAppliedSerial) {
          linkBpmAppliedSerial = linkBpmSerial;
          // BPM -> Hz: 24 ticks/beat, so tickRate = BPM * 24 / 60 = BPM * 0.4.
          chipnomadState->project.tickRate = linkBpm * 0.4f;
          audioProjectDirty = 1;
        }
      }
```

Add the `linkBpmAppliedSerial` static at the top of the function near `clockBpmAppliedSerial`.

**Verify:** Build OK. With Link connected (manually mock the backend in a unit test), the project tempo follows the session tempo.

---

## T35 — Apply `linkClientSelect`/`Deselect` at app startup

**Files:**
- `tracker/src/app.cpp`

**Change:** Near line 450 (after T6's changes), add:

```cpp
  if (appSettings.clockSource == ClockSource::link) {
    linkClientSelect(chipnomadState->linkClient);
  } else {
    linkClientDeselect(chipnomadState->linkClient);
  }
```

**Verify:** Build OK. With `clockSource: 2` in settings.txt, app startup leaves the Link client in `selected` status (not `connected`).

---

## T36 — Apply `linkClientSelect`/`Deselect` on live toggle

**Files:**
- `tracker/src/screens/screen_project.cpp`

**Change:** In the row 5 `if (handled)` block (T9), after the `midiRouterSetClockMode` call, add:

```cpp
    if (appSettings.clockSource == ClockSource::link) {
      linkClientSelect(chipnomadState->linkClient);
    } else {
      linkClientDeselect(chipnomadState->linkClient);
    }
```

**Verify:** Build OK. Cycling row 5 to "Ableton Link" puts the Link client in `selected` status. Cycling back to "Internal" puts it in `disabled`.

---

## T37 — Project screen row 6 (BPM) becomes read-only when Link is connected

**Files:**
- `tracker/src/screens/screen_project.cpp`

**Change:** In the row 6 input handler (lines 494-538 area), change the existing `if (clockSourceMirror)` check to also fire when Link is connected:

```cpp
      // Tempo in BPM. Read-only while any non-internal clock source is
      // active (MIDI clock slave, or Ableton Link connected). The field
      // shows the live followed tempo; user edits are swallowed.
      LinkStatus linkStatus = LinkStatus::disabled;
      uint32_t linkStatusSerial = 0;
      linkClientGetStatus(chipnomadState->linkClient, &linkStatus, &linkStatusSerial);
      int tempoReadOnly = clockSourceMirror == 1 /*midiClock*/ ||
        (clockSourceMirror == 2 /*link*/ && linkStatus == LinkStatus::connected);
      if (tempoReadOnly) {
        handled = 1;  // swallow input
      } else {
        // ... existing BPM editor code unchanged
      }
```

**Verify:** Build OK. With Link connected, BPM row input is swallowed. With Link selected but not connected, BPM editor stays active (user can set the seed tempo).

---

## T38 — TEMPO-2/3/5 tests

**Files:**
- `tracker/tests/test_link_tempo.cpp` (new)

**Content:** Three tests using a mock backend (per T33):

**TEMPO-2** (joining adopts session tempo):
- Mock returns session tempo 110 BPM, 1 peer
- `linkClientSelect` then `linkClientConnect`
- Verify status = `connected`
- Verify `linkClientGetSessionTempo` returns 110
- Simulate UI tick: verify `project.tickRate` set to `110 * 0.4 = 44.0`

**TEMPO-3** (loading a song doesn't change session tempo):
- Mock tracks every `setTempo` call (must be 0 calls — receiver-only)
- Load a `.cct` with `tickRate = 50`
- Verify `linkClientGetSessionTempo` still returns 110
- Verify no `setTempo` was called on the backend

**TEMPO-5** (enable with no peers doesn't jump tempo):
- Project tempo is 50 Hz
- `select` → `connect` with 0 peers in mock
- Verify status = `noPeers`
- Verify `linkClientGetSessionTempo` returns 0
- Verify `project.tickRate` is unchanged (still 50)

**Verify:** `make -f Makefile.test` runs all three and passes.

---

## T39 — Implement `outputLatencyMicros` exposure

**Files:**
- `tracker/src/audio_manager.h`
- `tracker/platforms/sdl2/corelib_audio.cpp`

**Changes:**

In `audio_manager.h`, add:
```cpp
extern uint64_t outputLatencyMicros;
```

In `platforms/sdl2/corelib_audio.cpp`'s `audioSetup()` (after `SDL_OpenAudio` succeeds):
```cpp
extern uint64_t outputLatencyMicros;
uint64_t outputLatencyMicros = 0;
// ... in audioSetup after SDL_OpenAudio:
outputLatencyMicros = (uint64_t)(spec.samples * 1000000ULL / spec.freq);
```

Note: PortMaster buffer 512 at 48 kHz = ~10667 µs. Desktop buffer 256 at 48 kHz = ~5333 µs. Document the ArkOS caveat in a comment: true DAC latency is unmeasured, so AUDIOENGINE-1 will be a documented ship exclusion on PortMaster.

**Verify:** Build OK. Print `outputLatencyMicros` from a debug log to verify the value is correct per platform.

---

## T40 — Implement `chipnomadQueuePlaybackStartSongAtTime`

**Files:**
- `chipnomad_lib/chipnomad_lib.h`
- `chipnomad_lib/chipnomad_lib.cpp`

**Changes:**

In `chipnomad_lib.h`, add to the public API (near the existing `chipnomadQueuePlaybackStartSong`):
```cpp
// Queued start: playback will begin at the given host time (microseconds).
// If waitUntilMicros == 0, fires immediately (existing behavior).
int chipnomadQueuePlaybackStartSongAtTime(ChipNomadState*, int songRow, int chainRow, int loop, uint64_t waitUntilMicros);
```

In `chipnomad_lib.cpp`:
- Add a `PendingStart` struct to `ChipNomadState` (or to `AudioCommandQueue`) holding `{int songRow; int chainRow; int loop; uint64_t waitUntilMicros;}`
- Add a new command type to the existing SPSC queue that carries `PendingStart`
- In `advancePlaybackFrame` (around line 1124), add a check at the top:
  ```cpp
  if (state->pendingStart.waitUntilMicros != 0 &&
      midiRouterNowMicros() >= state->pendingStart.waitUntilMicros) {
    playbackStartSong(state, state->pendingStart.songRow, state->pendingStart.chainRow, state->pendingStart.loop);
    state->pendingStart.waitUntilMicros = 0;  // clear
  }
  ```

**Verify:** Unit test `chipnomadQueuePlaybackStartSongAtTime(state, 0, 0, 1, nowMicros + 1000)` then render audio for 2ms; verify playback starts at the ~1ms mark (not immediately).

---

## T41 — Wire Play path through Link

**Files:**
- `tracker/src/app.cpp` (Play button handler — exact dispatch site TBD by maintainer; search for `chipnomadQueuePlaybackStartSong` calls)

**Change:** When Play is pressed AND `appSettings.clockSource == ClockSource::link` AND `linkClientGetStatus() == LinkStatus::connected`:

1. Call `linkClientRequestBeatAtLaunch(chipnomadState->linkClient, 4)` to get the host-time to wait for
2. If the returned time is 0 (no peers), call the existing `chipnomadQueuePlaybackStartSong` (immediate)
3. If non-zero, call the new `chipnomadQueuePlaybackStartSongAtTime` with the returned time

If `clockSource == link` but status is not `connected`, fall through to immediate start.

**Verify:** On a desktop build with LinkHut running on the same Wi-Fi, press Play — playback should align to the next bar boundary (within ~3 ms on desktop, ~10-15 ms on PortMaster).

---

## T42 — TSan sweep

**Action:** Run `make -f Makefile.test test-tsan` with the new Link integration. Verify no unintended shared accesses between UI thread (which polls `linkClientGetSessionTempo`) and audio thread (which calls `linkClientCaptureAudioSessionState` + `linkClientRequestBeatAtLaunch`).

If TSan reports races:
- The Link client's atomics must use `std::memory_order_acq_rel`/`acquire`/`release` (matching the existing pattern in `chipnomad_lib.cpp:171-422`)
- The HostTimeFilter is owned by `LinkClientState` and only accessed from the audio thread (no synchronization needed)

**Verify:** TSan reports zero races.

---

## T43 — Project screen row 5: Connect/Disconnect button display

**Files:**
- `tracker/src/screens/screen_project.cpp`

**Change:** In the row 5 display block (T8), after the "Ableton Link" label, render the button cell:

```cpp
  } else if (row == 5) {
    gfxClearRect(13, 10, 27, 1);
    gfxPrint(13, 10,
      clockSourceMirror == 2 ? "Ableton Link" :
      clockSourceMirror == 1 ? "Midi Clock"  :
      "Internal");
    // When Link is selected, render the Connect/Disconnect button to the right
    if (clockSourceMirror == 2) {
      LinkStatus status; uint32_t serial;
      linkClientGetStatus(chipnomadState->linkClient, &status, &serial);
      const char* label = (status == LinkStatus::connected) ? "[Disconnect]" :
                         (status == LinkStatus::connecting) ? "[Connecting]" :
                         "[Connect]";
      gfxPrint(25, 10, label);  // column 25 = after "Ableton Link" (12 chars) + 1 space
    }
  } else if (row == 6) {
```

**Verify:** Run the app, cycle row 5 to "Ableton Link" — `[Connect]` button appears to the right. Connect → button changes to `[Disconnect]`. Disconnect → back to `[Connect]`.

---

## T44 — Project screen row 5: Connect/Disconnect input handler

**Files:**
- `tracker/src/screens/screen_project.cpp`

**Change:** In the row 5 input handler (T9), add a sub-handler for the button cell. The exact focus model depends on the existing cell-navigation code (TBD by maintainer — may need column-based sub-focus for this row).

Pseudocode:
```cpp
if (clockSourceMirror == 2 && action == CellEditAction::tap) {
  LinkStatus status; uint32_t serial;
  linkClientGetStatus(chipnomadState->linkClient, &status, &serial);
  if (status == LinkStatus::connected) {
    linkClientDisconnect(chipnomadState->linkClient);
    handled = 1;
  } else if (status == LinkStatus::selected || status == LinkStatus::noNetwork ||
             status == LinkStatus::noPeers || status == LinkStatus::error) {
    linkClientConnect(chipnomadState->linkClient);
    handled = 1;
  }
  // status == connecting: swallow (button non-functional during connection)
  if (status == LinkStatus::connecting) handled = 1;
}
```

**Verify:** Tap on `[Connect]` triggers connection. Tap on `[Disconnect]` triggers disconnection. Tap on `[Connecting]` is swallowed.

---

## T45 — Project screen row 6: blur BPM + status message

**Files:**
- `tracker/src/screens/screen_project.cpp`

**Change:** In the row 6 display block (lines 362-377), update the blur condition and add the status message:

```cpp
  } else if (row == 6) {
    gfxClearRect(13, 11, 27, 1);
    LinkStatus linkStatus; uint32_t linkSerial;
    linkClientGetStatus(chipnomadState->linkClient, &linkStatus, &linkSerial);
    int blurred = clockSourceMirror == 1 /*midiClock*/ ||
      (clockSourceMirror == 2 /*link*/ && linkStatus == LinkStatus::connected);
    if (blurred) {
      gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textInfo : appSettings.colorScheme.textEmpty);
      gfxPrintf(13, 11, "%u.%u BPM", bpmTenths / 10, bpmTenths % 10);
    } else {
      // ... existing internal-mode rendering
    }
    // Status message on the right side when Link is selected
    if (clockSourceMirror == 2) {
      const char* statusStr = "";
      switch (linkStatus) {
        case LinkStatus::selected:    statusStr = "";          break;
        case LinkStatus::noNetwork:   statusStr = "No net";    break;
        case LinkStatus::connecting:  statusStr = "Wait...";   break;
        case LinkStatus::noPeers:     statusStr = "No sess";   break;
        case LinkStatus::connected:   statusStr = NULL;        break;  // dynamic "Conn:N"
        case LinkStatus::error:       statusStr = "Err";       break;
        case LinkStatus::disabled:    statusStr = "";          break;
      }
      if (linkStatus == LinkStatus::connected) {
        int peers = linkClientGetPeerCount(chipnomadState->linkClient);
        gfxPrintf(30, 11, "Conn:%d", peers);
      } else if (statusStr && statusStr[0]) {
        gfxPrint(30, 11, statusStr);
      }
    }
  }
```

**Verify:** Run the app. With Link selected, the BPM row shows the live session tempo in a dim color when connected. Status messages appear on the right side: "Wait..." during connection, "No net" when no network, "No sess" when no peers, "Conn:N" when connected.

---

## T46 — Privacy policy update

**Files:**
- `docs/privacy-policy.md`

**Change:** Replace "or network service." with a new paragraph:

> **Ableton Link (opt-in).** When the user selects Ableton Link on the Project screen and presses Connect, the app joins a local Link session via UDP multicast on the local network (LAN). This is opt-in network activity: it does not run unless the user explicitly enables it. The app acts as a receiver only — it follows the session tempo, never pushes tempo or transport to other participants. No data is sent to remote endpoints; multicast packets stay on the LAN. Link is disabled by default. On Android, the app requests the INTERNET, ACCESS_WIFI_STATE, ACCESS_NETWORK_STATE, and CHANGE_WIFI_MULTICAST_STATE permissions; these are used solely for Link session discovery and have no other purpose.

**Verify:** Privacy policy reads cleanly. The "no network service" claim is gone.

---

## T47 — User manual update

**Files:**
- `docs/USER_MANUAL.md`

**Change:** Add a new section "Ableton Link" covering:
- What it does: follows the session tempo of other Link participants on the same Wi-Fi
- What it doesn't sync: grooves, `SPD`, song/chain/phrase cursors, mixer
- How to use: Project screen → Clock source → cycle to "Ableton Link" → press Connect → wait for "Conn:N" status
- Per-platform availability: PortMaster (primary), Windows, Linux, macOS, Android — all supported; Web and Miyoo excluded
- PortMaster AUDIOENGINE-1 deviation: best-effort phase alignment ~10-15 ms, not sample-accurate

**Verify:** Section reads cleanly. Per-platform availability is accurate.

---

## T48 — PortMaster packaging note

**Files:**
- `packaging/portmaster/README.md`

**Change:** Add a note that Link requires Wi-Fi and only works when the device is on a network with other Link participants.

**Verify:** README reads cleanly.

---

## T49 — AUDIOENGINE-1 test plan document

**Files:**
- `docs/ableton-link-test-plan.md` (new)

**Content:** Document the test methodology per platform:
- Windows lab: buffer 256 → ~5.3 ms measured; target < 3 ms alignment vs LinkHut click
- Linux desktop: same
- macOS: same
- Android: target < 5 ms (slightly looser due to AudioTrack scheduling jitter)
- PortMaster: buffer 512 → ~10.7 ms measured; **documented ship exclusion** — tempo sync works, beat alignment best-effort within ~10-15 ms depending on ArkOS Wi-Fi scheduling

**Verify:** Document exists, references the right values.

---

## T50 — PortMaster ship criteria document

**Files:**
- `docs/ableton-link-portmaster-criteria.md` (new)

**Content:** Record the pass/fail outcomes of T22 (Wi-Fi multicast spike) and T23 (CPU budget spike). This is the gate that the PortMaster ship decision references.

**Verify:** Document exists with the actual pass/fail outcomes from the maintainer's hardware verification.

---

## T51 — Hardware verification: PortMaster

**Action:** Build for PortMaster, deploy to TrimUI Brick. Start LinkHut at 110 BPM on a phone on the same Wi-Fi. Enable Link in Choochootracker. Verify:
- Project screen shows `Conn:1`
- BPM row shows `110.0 BPM` (blurred)
- Grooves/SPD/DELAY audibly follow 110 BPM
- Loading a different `.cct` doesn't change the session tempo
- Press Play — playback aligns to the next bar (within ~10-15 ms on PortMaster)
- No audible dropouts on dense songs (`psy.cct`, `DNB.cct`)

**Pass criteria:** All of the above. Document the outcome in T50.

---

## T52 — Hardware verification: Windows, Linux, macOS

**Action:** On each of Windows, Linux, macOS, repeat T51 with the same LinkHut peer. The pass criteria are the same, except AUDIOENGINE-1 target is < 3 ms (lab measurement).

**Pass criteria:** All of the above. Tempo sync + beat alignment within < 3 ms on each.

---

## T53 — Hardware verification: Android

**Action:** On a real Android device (not emulator), repeat T51. Android requires the four manifest permissions (T20) and runtime grant on first Connect. Verify the `MulticastLock` (T28) is acquired/released correctly (check device logs for "MulticastLock acquired choochootracker-link" / "MulticastLock released").

**Pass criteria:** All of T51 + MulticastLock lifecycle correct + no ANRs.

---

## T54 — Release

**Action:** Ship the build on all five supported platforms (PortMaster, Windows, Linux, macOS, Android). Update release notes mentioning Link capability, the receiver-only behavior, and the per-platform AUDIOENGINE-1 status.

**Verify:** All ship targets build, install, run. Release notes mention Link.

---

# Glossary

- **Ableton Link**: A peer-to-peer sync protocol for musical tempo and beat alignment across apps on the same local network. Each participant maintains its own timeline; the library maintains a mapping so participants share tempo, beat alignment, and phase for a client-chosen quantum.
- **Quantum**: The beat-grid size for phase alignment. We use 4 (one 16-step bar at default groove 6).
- **Receiver-only**: The tracker follows the session tempo and aligns to the quantum grid on launch, but never pushes tempo or transport changes to other participants.
- **Tick (tracker)**: The smallest time unit in the tracker's sequencer. `24 ticks = 1 beat`. The engine tick rate is `Project.tickRate` (Hz); default 50 Hz = 125 BPM.
- **Host time**: Microseconds since some epoch, monotonic. Used by Link for `requestBeatAtTime`'s `hostTime` argument. On this codebase, `midiRouterNowMicros()` returns the same monotonic clock.
- **AUDIOENGINE-1**: Ableton's test that audio onsets vs LinkHut clicks align within 3 ms. We pass this on Windows/Linux/macOS desktop; we ship-exclude it on PortMaster (~10-15 ms best-effort) with a documented rationale.
- **HostTimeFilter**: Linear regression of host time vs sample counter, used to estimate host time for any sample offset within the current audio buffer. Required because SDL's audio callback gives no host timestamp.

---

# Pre-flight checklist

Before starting T1:

- [ ] `g++ --version` inside `ghcr.io/monkeyx-net/portmaster-build-templates/portmaster-builder:aarch64-latest` reports ≥ GCC 10
- [ ] TrimUI Brick device available for T22 and T23
- [ ] (Optional) Anbernic RG353V device available for inherited-variant verification (same `choochootracker.aarch64` binary, but ArkOS CFW Wi-Fi behavior may differ from TrimUI Brick)
- [ ] Android device (not emulator) available for T24 and T53
- [ ] LinkHut (https://github.com/Ableton/link) available on a phone/laptop for peer testing
- [ ] License paperwork filed (Ableton proprietary grant OR GPLv2+ relicense decision)
- [ ] Contributor consent for the chosen license path documented
