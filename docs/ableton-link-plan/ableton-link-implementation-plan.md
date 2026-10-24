# Ableton Link Sync — Phased Implementation Plan

**Repository:** `github.com/am0k161/Choochootracker` (branch: `connectivity`)
**Primary ship target:** PortMaster / TrimUI Brick (Allwinner H700, aarch64)
**Also-supported ship targets:** Windows, Linux, macOS, Android (all first-class, ship together)
**Inherited from upstream (work but not primary test target):** Anbernic RG353V/RG353VVS/RG503 (same aarch64 PortMaster binary; Anbernic ArkOS Wi-Fi behavior differs from TrimUI Brick)
**Excluded:** Web / Emscripten, Miyoo / RG35xx (SDL 1.2)
**Operating mode:** Receiver-only (never `setTempo`, `setIsPlaying`, `commitAudioSessionState`, `startPlaying`, `stopPlaying`)
**License status:** Solved (per maintainer)
**Last updated:** 2026-10-10

> **Companion document:** `ableton-link-coding-llm-spec.md` — task-by-task spec ready to feed to a coding LLM. This plan is the high-level structure; the spec is the executable detail.

## Locked decisions

| Decision | Choice |
|---|---|
| Primary ship target | PortMaster / TrimUI Brick (Allwinner H700, aarch64, TrimUI stock firmware) |
| Inherited from upstream | Anbernic RG353V (same aarch64 binary, Rockchip RK3566, ArkOS CFW) — works but not the primary test target |
| Co-supported ship targets | Windows (MSYS2 UCRT64), Linux (incl. Steam Deck), macOS (universal arm64+x86_64, macOS 11+), Android (arm64-v8a, API 26+) |
| Operating mode | Receiver-only — never push tempo/transport to peers |
| Connect button | Toggles `[CONNECT]` ↔ `[DISCONNECT]`; clock source stays as Link after disconnect |
| `noNetwork` recovery | Manual retry only (user presses Connect again) |
| `noPeers` transition (last peer left) | Hold last session tempo; BPM row stays blurred; status `No sess` |
| Solo Play (no peers present) | Immediate, no quantum wait |
| Solo Connect (no peers present) | Allow; project tempo seeds future joiners |
| Tempo persistence | **Skip `originalTickRate` fix** — the existing "Save writes live slaved tempo to file" behavior is intentional backwards compat for older projects |
| Status strings | Short: `No net`, `No sess`, `Conn:3`, `Wait...` |
| GCC verification | Maintainer verifies on their end; Phase 1.0 compile spike catches any issue |

## Architecture summary

Link drives a single field: `audioProject.tickRate` (Hz; 24 ticks = 1 beat → `tickRate = bpm × 0.4`). The engine's existing tick accumulator (`chipnomad_lib.cpp:1155`: `frameSampleCounter += sampleRate / audioProject.tickRate`) follows automatically; grooves, `SPD`, `DELAY`, ping-pong, and tables all consume ticks so they inherit the session tempo without any change.

The Link library instance lives in a new `chipnomad_lib/link/` module that mirrors `midi/midi_router.{h,cpp}`: a `LinkClient` with `select()`/`deselect()`, `connect()`/`disconnect()`, `getStatus()`, `getTempo()`, `requestBeatAtLaunch()`, backend-abstracted so a no-op stub builds on Web/Miyoo/RG35xx. The UI thread polls Link state every UI tick (60 Hz, mirroring the MIDI clock poll at `app.cpp:770-794`), applies the tempo to `project.tickRate` (UI-owned, never from audio), marks `audioProjectDirty = 1`, and the existing snapshot path propagates it to audio. At Play-while-peers, the audio callback captures Link session state and `requestBeatAtTime(0, hostTime, quantum=4)` delays `playbackStartSong` to a host-time-anchored beat boundary, compensated for SDL output latency. **Start/stop sync is off in v1.**

Platform matrix:

| Target | Link platform define | Vendor location | Audio backend | Threads | Wi-Fi multicast | Ship priority |
|---|---|---|---|---|---|---|
| PortMaster / TrimUI Brick | `LINK_PLATFORM_LINUX=1` | `Makefile.portmaster` | SDL2 + ALSA | Yes | Built-in Wi-Fi; Phase 1.10 spike | **Primary** |
| PortMaster / Anbernic RG353V | `LINK_PLATFORM_LINUX=1` | `Makefile.portmaster` (same binary) | SDL2 + ALSA | Yes | ArkOS Wi-Fi variable; inherited from upstream, not primary test target | Co-supported (same binary) |
| Windows | `LINK_PLATFORM_WINDOWS=1`, `_WIN32_WINNT=0x0601` | `Makefile.windows` | SDL2 | Yes | Windows Firewall UDP allow | Co-supported |
| Linux / Steam Deck | `LINK_PLATFORM_LINUX=1` | `Makefile.linux` | SDL2 | Yes | Standard | Co-supported |
| macOS | `LINK_PLATFORM_MACOS=1` | `Makefile.macos` | SDL2 + CoreMIDI | Yes | Standard | Co-supported |
| Android | `LINK_PLATFORM_LINUX=1` | `Makefile.android` | SDL2 (AudioTrack) | Yes | Manifest permissions + MulticastLock JNI | Co-supported |
| Web / Emscripten | (stub) | (stub) | SDL2 (main thread) | No | N/A | Excluded |
| Miyoo / RG35xx | (stub) | (stub) | SDL2 / SDL 1.2 | Yes | Untested | Excluded |

---

# Phase 0 — Pre-flight (no Link code yet)

**Goal:** Generalize the sync-source plumbing so Link can be a third peer of MIDI clock, not a parallel system. Pure refactor, no behavior change.

### Step 0.1 — Add `ClockSource` enum to `common.h`
Add a 3-state enum (`internal`/`midiClock`/`link`) that generalizes the existing `int midiClockMode`. File: `tracker/src/common.h` after line 53. The enum is the single source of truth for "which sync source is active".

### Step 0.2 — Replace `AppSettings.midiClockMode` with `clockSource`
File: `tracker/src/common.h:87`. Change `int midiClockMode;` to `ClockSource clockSource;`. Update the comment to document the v2→v3 alias rule.

### Step 0.3 — Update default init in `common.cpp`
File: `tracker/src/common.cpp:64`. Change `appSettings.midiClockMode = 0;` to `appSettings.clockSource = ClockSource::internal;`.

### Step 0.4 — Update settings save in `common.cpp`
File: `tracker/src/common.cpp:210`. Write both `clockSource: %d` (canonical, 0/1/2) and `midiClockMode: %d` (legacy alias, 0 or 1 only — Link silently drops to 0 on the alias line for old binaries).

### Step 0.5 — Update settings load in `common.cpp`
File: `tracker/src/common.cpp:353-356`. Add a new branch for `clockSource:` (canonical) BEFORE the existing `midiClockMode:` branch (legacy alias). Both must remain for one release.

### Step 0.6 — Update router init in `app.cpp`
File: `tracker/src/app.cpp:450-451`. Replace `appSettings.midiClockMode == 1` with `appSettings.clockSource == ClockSource::midiClock`. Add a comment noting that `ClockSource::link` is a no-op for the MIDI router in Phase 0; the future Link client module owns its own state.

### Step 0.7 — Update Project screen row 5 mirror init
File: `tracker/src/screens/screen_project.cpp:235`. Change `clockSourceMirror = appSettings.midiClockMode ? 1 : 0;` to `clockSourceMirror = (uint8_t)appSettings.clockSource;`.

### Step 0.8 — Update Project screen row 5 display
File: `tracker/src/screens/screen_project.cpp:358-361`. Replace the 2-state display with a 3-state ternary showing `"Internal"` / `"Midi Clock"` / `"Ableton Link"`.

### Step 0.9 — Update Project screen row 5 input handler to 3-state cycle
File: `tracker/src/screens/screen_project.cpp:455-487`. Replace the 2-state toggle with a 3-state cycle: tap/increase cycles forward (`(clockSourceMirror + 1) % 3`), decrease cycles backward (`(clockSourceMirror + 2) % 3`), clear resets to 0. On `handled`, update `appSettings.clockSource` and `midiRouterSetClockMode` (Link mode leaves MIDI router off — placeholder for Phase 2+).

### Step 0.10 — Add TSan/ASan CI build
File: `tracker/Makefile.test`. Add `test-tsan` and `test-asan` make targets with `-fsanitize=thread` and `-fsanitize=address` respectively. The existing mocked audio + mainloop (`tracker/tests/mocks/`) supports running without a real audio device.

### Step 0.11 — Verify build
Run `make -f Makefile.test` and the new TSan/ASan targets on a Linux desktop build. All existing tests must pass. Manually cycle row 5 through all 3 states; confirm `settings.txt` saves both `clockSource:` and `midiClockMode:` lines.

**Phase 0 deliverable:** `clockSource` enum in place; 3-state UI cycle works; settings round-trip preserves both forms; TSan/ASan targets added; no Link code committed yet.

---

# Phase 1 — Vendor Link + multi-platform compile spikes

**Goal:** Get `#include <ableton/Link.hpp>` to compile and a smoke test to live across a process lifetime on **all five ship targets** (PortMaster, Windows, Linux, macOS, Android). No audio integration yet.

### Step 1.0 — PortMaster compile spike (gate)

**Action:** Fetch `Ableton/link` and standalone `Asio` into a scratch directory outside the repo. Compile `link_smoke_test.cpp` using the PortMaster Docker build image (`ghcr.io/monkeyx-net/portmaster-build-templates/portmaster-builder:aarch64-latest`) on the Ubuntu 24.04 ARM CI runner.

Smoke test body:
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

**Pass criterion:** Binary builds, links, runs, prints `peers=0 tempo=125.000000` on a PortMaster device.

**Fail criterion:** If GCC < 10 (Link's verified floor) in the Docker image, or if Link uses C++ features the toolchain doesn't support, Phase 1 is blocked. Either wait for toolchain upgrade or patch Link for the specific compiler. Document the blocker.

**Why first:** PortMaster is the primary ship target. The maintainer's verification step (per locked decision) will produce the actual GCC version. If it's ≥ 10 (likely, given the CI runs on Ubuntu 24.04 ARM), proceed. If < 10, stop.

### Step 1.1 — Vendor Link SDK

**Action:** Vendor `Ableton/link` into `chipnomad_lib/external/ableton_link/`:
- `include/ableton/*.hpp` (Link headers)
- `LICENSE` (GPLv2+ or proprietary per maintainer's license grant)
- `provenance.json` (mirrors `chipnomad_lib/external/msfa/provenance.json` pattern — upstream commit hash, license, patches)

### Step 1.2 — Vendor Asio standalone

**Action:** Vendor Asio header-only into `chipnomad_lib/external/asio/`:
- `asio/asio/include/asio/**/*.hpp` and `.ipp`
- `LICENSE` (BSD-style)
- `provenance.json`

### Step 1.3 — Add Link + Asio to PortMaster Makefile

**File:** `tracker/Makefile.portmaster:8` and `:28`

Extend `LIBS`:
```makefile
LIBS = $(COMMON_LIBS) platforms/sdl2 ../chipnomad_lib/external/rtmidi \
  ../chipnomad_lib/external/ableton_link ../chipnomad_lib/external/asio
```

Add platform define to `CFLAGS`:
```makefile
-DLINK_PLATFORM_LINUX=1
```

### Step 1.4 — Add Link + Asio to Linux Makefile

**File:** `tracker/Makefile.linux`

Same `LIBS` extension as 1.3. Add `-DLINK_PLATFORM_LINUX=1` to defines.

### Step 1.5 — Add Link + Asio to Windows Makefile

**File:** `tracker/Makefile.windows`

Same `LIBS` extension. Add `-DLINK_PLATFORM_WINDOWS=1 -D_WIN32_WINNT=0x0601` to defines. The `-lws2_32` is already linked transitively via RtMidi on Windows; verify it covers Asio's needs (it should — Asio uses Winsock).

### Step 1.6 — Add Link + Asio to macOS Makefile

**File:** `tracker/Makefile.macos`

Same `LIBS` extension. Add `-DLINK_PLATFORM_MACOS=1` to defines. Asio uses BSD sockets on macOS, no extra library needed.

### Step 1.7 — Add Link + Asio to Android Makefile

**File:** `tracker/Makefile.android`

Same `LIBS` extension. Add `-DLINK_PLATFORM_LINUX=1` to defines (Android uses the Linux kernel + bionic libc, so Asio's Linux path works). Add the necessary `-fPIC` (already present per existing Makefile).

### Step 1.8 — Add Android manifest permissions

**File:** `tracker/platforms/android/app/src/main/AndroidManifest.xml`

Add the four permissions required for Link discovery on Android:
- `INTERNET` — basic network access
- `ACCESS_WIFI_STATE` — check Wi-Fi connection state
- `ACCESS_NETWORK_STATE` — check network availability
- `CHANGE_WIFI_MULTICAST_STATE` — allow Wi-Fi multicast for mDNS discovery

### Step 1.9 — Add Link smoke test

**File:** `tracker/tests/test_link_smoke.cpp` (new)

A doctest-based version of the Step 1.0 smoke test. Gated by `#if defined(DESKTOP_BUILD) || defined(PORTMASTER_BUILD) || defined(ANDROID_BUILD)`. Runs as part of the test suite on desktop and PortMaster builds.

### Step 1.10 — PortMaster Wi-Fi multicast spike (gate)

**Action:** Build the Step 1.0 smoke test binary for PortMaster. Deploy to a real TrimUI Brick. Start LinkHut (or another Link-enabled app) on a phone/laptop on the same Wi-Fi. Verify `peers >= 1` prints within 5 seconds.

**Pass criterion:** Peer discovery works on TrimUI Brick stock firmware + Wi-Fi.

**Fail criterion:** If multicast doesn't propagate (firmware firewall, Wi-Fi power save, multicast routing), investigate TrimUI firmware multicast settings. If unresolved, this is a real ship blocker for TrimUI Brick specifically — Windows/Linux/macOS/Android are NOT affected by this gate. The Anbernic RG353V path (inherited from upstream) may have different Wi-Fi multicast behavior on ArkOS CFW and would need a separate verification if it's to remain an officially supported variant.

### Step 1.11 — PortMaster CPU budget spike (gate)

**Action:** On the same TrimUI Brick with Link enabled, run a dense song (`packaging/common/projects/psy.cct` or `DNB.cct`) at the default 512-frame buffer. Measure frame drops / audio underruns / CPU load vs. baseline (Link disabled).

**Pass criterion:** No audible dropouts; CPU overhead from Link's network thread is acceptable (target: < 10% extra CPU vs. baseline).

**Fail criterion:** If we lose frames or hear dropouts when Link is on, profile Asio's polling rate or tune Link's thread priorities. **PortMaster-specific gate.**

### Step 1.12 — Windows / Linux / macOS / Android smoke verification

**Action:** On each of Windows, Linux, macOS, and Android, build the Step 1.0 smoke test. Run with LinkHut on the same Wi-Fi. Verify `peers >= 1` prints within 5 seconds.

These are not gates — they're verification that the Link SDK actually works on each platform. Failures here indicate environment issues (firewall, permissions) rather than code issues.

### Step 1.13 — Document PortMaster ship criteria

**File:** `docs/ableton-link-portmaster-criteria.md` (new)

Record the pass/fail outcomes of Steps 1.0, 1.10, 1.11. This becomes the gate that Phase 6.1 (PortMaster ship) references.

**Phase 1 deliverable:** `Ableton/link` and Asio vendored; all five ship targets (PortMaster, Linux, Windows, macOS, Android) compile and link Link; `link_smoke_test` runs on every ship target with `peers >= 1` when LinkHut is on the same Wi-Fi; PortMaster CPU + Wi-Fi spikes pass; no `chipnomad_lib/link/` module yet.

---

# Phase 2 — `chipnomad_lib/link/` module (no audio wiring yet)

**Goal:** A `LinkClient` abstraction that mirrors `midi_router.h` shape — backend-abstracted, no Link type in the header, no-op stub on excluded platforms.

### Step 2.1 — Create the module directory

New directory `chipnomad_lib/link/` with five files:
- `link_client.h` — public API (no `ableton::` type appears here)
- `link_client.cpp` — implementation, includes `ableton/Link.hpp`
- `link_backend.h` — backend interface (mirrors `MidiBackend` struct)
- `link_backend_link.cpp` — real backend: wraps `ableton::Link` (gated by `DESKTOP_BUILD`/`PORTMASTER_BUILD`/`ANDROID_BUILD`)
- `link_backend_stub.cpp` — no-op stub for `WEB_BUILD`/`MIYOOPORTS_BUILD`/`RG35XX_BUILD`

### Step 2.2 — Define the `LinkStatus` enum

```cpp
enum class LinkStatus {
  disabled,        // not selected as clock source, link disabled
  selected,        // clock source = Link, but Connect not pressed yet
  noNetwork,       // Connect pressed, no usable network interface
  connecting,      // Connect pressed, mDNS discovery in flight
  noPeers,         // discovery running, no Link sessions found
  connected,       // 1+ peers, session tempo being applied
  error,           // Link library reported an internal failure
};
```

Maps directly to the status row strings (`No net`, `Wait...`, `No sess`, `Conn:N`).

### Step 2.3 — Define the `LinkClient` API contract

Full receiver-only contract documented in the header:
- **MUST NOT call:** `setTempo`, `setIsPlaying`, `commitAudioSessionState`, `startPlaying`, `stopPlaying`
- **Allowed:** `enable`/`disable`/`isEnabled`, `captureAudioSessionState` (read), `sessionState.tempo/beatAtTime/phaseAtTime/numPeers`, `sessionState.requestBeatAtTime` (local alignment only)

Public API:
```cpp
struct LinkClientState;
LinkClientState* linkClientCreate(void);
void linkClientDestroy(LinkClientState*);
void linkClientSelect(LinkClientState*);      // status: * -> selected
void linkClientDeselect(LinkClientState*);    // status: * -> disabled
void linkClientConnect(LinkClientState*);     // status: selected -> connecting -> (noNetwork | noPeers | connected)
void linkClientDisconnect(LinkClientState*);  // status: any -> selected
LinkStatus linkClientGetStatus(const LinkClientState*, uint32_t* serial);
int        linkClientGetPeerCount(const LinkClientState*);
float      linkClientGetSessionTempo(const LinkClientState*, uint32_t* serial);
int        linkClientCaptureAudioSessionState(LinkClientState*, double* tempo, uint64_t* peers);
uint64_t   linkClientRequestBeatAtLaunch(LinkClientState*, int64_t quantum);
```

### Step 2.4 — Implement `link_backend_link.cpp` (real backend)

Gated by `#if defined(DESKTOP_BUILD) || defined(PORTMASTER_BUILD) || defined(ANDROID_BUILD)`. Holds a `std::unique_ptr<ableton::Link>` member. Implements every function from Step 2.3 by delegating to the Link library. `linkClientRequestBeatAtLaunch` calls `sessionState.requestBeatAtTime(0, hostTime + outputLatencyMicros + schedulingHeadroomMicros, quantum)` (THREE args, not two — verified against upstream).

### Step 2.5 — Implement `link_backend_stub.cpp` (no-op for excluded platforms)

Gated by `#if !defined(DESKTOP_BUILD) && !defined(PORTMASTER_BUILD) && !defined(ANDROID_BUILD)`. Every function returns 0/disabled. `linkClientCreate` returns a non-null `LinkClientState*` so callers don't crash.

### Step 2.6 — Implement `link_client.cpp` (router-layer logic)

Owns the `LinkClientState` struct, manages status transitions, fires the status serial on every change. Delegates to backend functions.

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

### Step 2.7 — Android-specific: `WifiManager.MulticastLock` JNI bridge

**File:** `tracker/platforms/android/link_backend_android.cpp` (new, parallel to existing `midi_backend_android.cpp`)

JNI calls into `WifiManager` to acquire/release a `MulticastLock` on `linkClientConnect`/`Disconnect`. Without this, mDNS discovery packets don't reach the app on Android (Wi-Fi power saving drops them at the driver level).

### Step 2.8 — Add `link/` to `chipnomad_lib.h`

Add `#include "link/link_client.h"` to the public API surface.

### Step 2.9 — Add `linkClient` to `ChipNomadState`

**File:** `chipnomad_lib/chipnomad_lib.h` (the `ChipNomadState` struct around line 68)

Add `LinkClientState* linkClient;` next to `midiRouter`.

### Step 2.10 — Construct/destroy `linkClient` in `chipnomadCreate`/`Destroy`

**File:** `chipnomad_lib/chipnomad_lib.cpp`

In `chipnomadCreate()`: `state->linkClient = linkClientCreate();`
In `chipnomadDestroy()`: `linkClientDestroy(state->linkClient);` before `midiRouterDestroy(state->midiRouter);`

### Step 2.11 — Add `link/` to `Makefile.common`'s `COMMON_LIBS`

Add `../chipnomad_lib/link` to `COMMON_LIBS`. The stub vs. real backend selection is handled by the `#if` gates inside the .cpp files.

### Step 2.12 — Unit tests

**File:** `tracker/tests/test_link_client.cpp` (new)

Tests for:
- Serial-bump semantics
- Stub backend returns 0/disabled on every call
- `select` then `deselect` returns to `disabled` without firing any network event
- `connect` then `disconnect` returns to `selected`
- State transitions per the state machine

**Phase 2 deliverable:** `chipnomad_lib/link/` module exists; real backend compiles on desktop/PortMaster/Android; stub compiles on Web/Miyoo/RG35xx; `ChipNomadState` owns `linkClient`; unit tests pass; no audio callback integration yet.

---

# Phase 3 — Tempo sync (first audible feature)

**Goal:** When `clockSource == link` AND `status == connected`, the project follows the session tempo. Beat/phase alignment NOT in this phase.

### Step 3.1 — Add Link tempo poll to `app.cpp` UI tick

**File:** `tracker/src/app.cpp` immediately after the MIDI-clock-slave block at lines 770-794.

Add a parallel Link block mirroring the MIDI clock pattern. UI thread writes `project.tickRate = linkBpm * 0.4f`, sets `audioProjectDirty = 1`, leaves `projectModified = 0`. The audio callback never reaches into Link.

### Step 3.2 — Apply `linkClientSelect`/`Deselect` at app startup

**File:** `tracker/src/app.cpp` near line 450.

If `appSettings.clockSource == ClockSource::link`, call `linkClientSelect`. Otherwise `linkClientDeselect`. (Selecting Link marks the preference; Connect is a separate user action per the privacy policy.)

### Step 3.3 — Apply `linkClientSelect`/`Deselect` on live toggle

**File:** `tracker/src/screens/screen_project.cpp:487-493`.

After updating `appSettings.clockSource` and the MIDI router, also update the Link client.

### Step 3.4 — Project screen Tempo row (row 6) becomes read-only when Link is connected

**File:** `tracker/src/screens/screen_project.cpp:494-538`.

Extend the existing `if (clockSourceMirror)` check to also fire when `clockSourceMirror == 2 && linkStatus == LinkStatus::connected`. When Link is selected but not connected, the BPM editor stays active (the user's project tempo becomes the seed if they Connect with no peers).

### Step 3.5 — TEMPO-2 test (joining adopts session tempo)

**File:** `tracker/tests/test_link_tempo.cpp` (new)

Mock Link backend returns session tempo 110 BPM. `select` → `connect`. Verify status = `connected`, `getTempo` returns 110, simulated UI tick sets `project.tickRate = 44.0`.

### Step 3.6 — TEMPO-3 test (loading a song doesn't change session tempo)

Load a `.cct` with `tickRate = 50` while connected. Verify `getTempo` still returns 110. Verify no `setTempo` was called on the backend (receiver-only contract).

### Step 3.7 — TEMPO-5 test (enable with no peers doesn't jump tempo)

`select` → `connect` with no peers in mock. Verify status = `noPeers`, `getTempo` returns 0 (or project tempo — TBD), `project.tickRate` unchanged.

### Step 3.8 — Hardware verification on PortMaster

Build for PortMaster, deploy to TrimUI Brick, start LinkHut at 110 BPM, enable Link. Verify:
- Project screen shows `Conn:1`
- BPM row shows `110.0 BPM` (blurred)
- Grooves/SPD/DELAY audibly follow 110 BPM
- Loading a different `.cct` doesn't change the session tempo

### Step 3.9 — Hardware verification on Windows, Linux, macOS, Android

Same as 3.8 on each co-supported ship target. Same pass criteria. Android requires running the app on a real device (not emulator — emulators have weird network behavior).

**Phase 3 deliverable:** Link tempo drives `project.tickRate` from UI thread poll; Project screen shows live session BPM (blurred); TEMPO-2/3/5 tests pass; hardware verified on all five ship targets.

---

# Phase 4 — Phase alignment at quantized Play

**Goal:** When Play is pressed with peers present, `playbackStartSong` is delayed until the next beat-0 of a quantum-4 boundary. This is the phase that touches the audio callback.

### Step 4.1 — Implement `HostTimeFilter`

**File:** `chipnomad_lib/link/link_backend_link.cpp`

Linear regression filter that maps SDL sample counter to host time (microseconds). Track `(sampleCounter, hostTimeMicros)` pairs sampled at audio callback entry. Sliding window of last 64 samples. Reset on `enable()`, sample-rate change, buffer-size change.

### Step 4.2 — Add `outputLatencyMicros` to `audio_manager`

**File:** `tracker/src/audio_manager.h` (declaration) and `tracker/platforms/sdl2/corelib_audio.cpp` (population in `audioSetup`).

`outputLatencyMicros = (uint64_t)(spec.samples * 1000000ULL / spec.freq);`

Document the ArkOS caveat: true DAC latency is unmeasured, so AUDIOENGINE-1 will be a documented ship exclusion on PortMaster (~10.7 ms buffer-induced latency + unknown DAC latency). Windows/Linux/macOS desktop at 256-frame buffer should pass < 3 ms easily.

### Step 4.3 — Implement `linkClientRequestBeatAtLaunch`

**File:** `chipnomad_lib/link/link_client.cpp`

If `link->numPeers() == 0`, return 0 (immediate start, per locked decision). Otherwise: capture host time via HostTimeFilter, add `outputLatencyMicros + schedulingHeadroomMicros` (headroom = 1 buffer), call `sessionState.requestBeatAtTime(0, hostTime + offsets, 4)`, return the host time.

### Step 4.4 — Add queued-start mechanism to audio engine

**File:** `chipnomad_lib/chipnomad_lib.h` (new API) and `chipnomad_lib/chipnomad_lib.cpp` (extend `advancePlaybackFrame`)

New API: `chipnomadQueuePlaybackStartSongAtTime(state, songRow, chainRow, loop, waitUntilMicros)`. If `waitUntilMicros == 0`, fires immediately (existing behavior). Otherwise, `advancePlaybackFrame` checks at the top: if pending start exists and `midiRouterNowMicros() >= waitUntilMicros`, fire the start.

### Step 4.5 — Wire the Play path through Link

**File:** `tracker/src/app.cpp` (Play button handler — exact dispatch site TBD by maintainer)

When Play pressed AND `clockSource == link` AND `status == connected`:
1. Call `linkClientRequestBeatAtLaunch(state->linkClient, 4)` → host time
2. If 0 (no peers), call existing `chipnomadQueuePlaybackStartSong` (immediate)
3. If non-zero, call `chipnomadQueuePlaybackStartSongAtTime` with the returned time

If `clockSource == link` but status not `connected`, fall through to immediate start (solo Play).

### Step 4.6 — AUDIOENGINE-1 test plan

**File:** `docs/ableton-link-test-plan.md` (new)

- Windows lab: buffer 256 → ~5.3 ms measured; target < 3 ms alignment vs LinkHut click
- Linux desktop: same
- macOS: same
- Android: target < 5 ms (slightly looser due to AudioTrack scheduling jitter)
- PortMaster: buffer 512 → ~10.7 ms measured; **documented ship exclusion** — tempo sync works, beat alignment best-effort within ~10-15 ms depending on ArkOS Wi-Fi scheduling. User manual calls this out.

### Step 4.7 — TSan sweep of `advancePlaybackFrame` with Link capture on

Run `make -f Makefile.test test-tsan`. Verify no unintended shared accesses between UI thread (polls `linkClientGetSessionTempo`) and audio thread (calls `linkClientCaptureAudioSessionState` + `linkClientRequestBeatAtLaunch`).

**Phase 4 deliverable:** HostTimeFilter implemented; `outputLatencyMicros` exposed; `chipnomadQueuePlaybackStartSongAtTime` API exists; Play with peers quantizes to next bar; Play without peers is immediate; AUDIOENGINE-1 test plan documented; TSan clean.

---

# Phase 5 — UI integration (your design)

**Goal:** Connect button, status row, blur treatment — the visual design you specified.

### Step 5.1 — Project screen row 5: Connect/Disconnect button

**File:** `tracker/src/screens/screen_project.cpp:358-365`

When `clockSourceMirror == 2`, render a button cell to the right of "Ableton Link" label. Label: `[Connect]` (status `selected`/`noNetwork`/`noPeers`/`error`), `[Connecting]` (status `connecting`), `[Disconnect]` (status `connected`).

### Step 5.2 — Project screen row 5: handle Connect/Disconnect press

**File:** `tracker/src/screens/screen_project.cpp:455-493`

Add sub-handler for the button cell. Tap on `[Connect]` → `linkClientConnect`. Tap on `[Disconnect]` → `linkClientDisconnect`. Tap on `[Connecting]` is swallowed. Focus model TBD by maintainer (may need column-based sub-focus for this row).

### Step 5.3 — Project screen row 6: blur BPM when Link is connected

**File:** `tracker/src/screens/screen_project.cpp:362-377`

When Link is connected, render BPM in a dim color (`colorScheme.textEmpty` or `textDim` — verify which exists). Number stays visible (live session tempo) but clearly non-editable. Mirrors the existing MIDI-clock-slave blur at lines 366-369.

### Step 5.4 — Project screen row 6: status message on right side

**File:** `tracker/src/screens/screen_project.cpp:362-377`

When `clockSourceMirror == 2`, render status string at column ~30:
- `selected` → empty
- `noNetwork` → `No net`
- `connecting` → `Wait...`
- `noPeers` → `No sess`
- `connected` → `Conn:N` (N = peer count)
- `error` → `Err`
- `disabled` → empty

### Step 5.5 — Verify input swallowing during `connecting` / `noNetwork`

Already done in Phase 3.4 — verify input is swallowed not only when `connected` but also during `connecting` and `noNetwork` (user can't edit BPM while a connection is being attempted).

### Step 5.6 — Settings persistence

Verify `clockSource: 2` serializes correctly (already implemented in Phase 0.4-0.5).

### Step 5.7 — Privacy policy update

**File:** `docs/privacy-policy.md`

Replace "or network service" with a new paragraph describing Link as opt-in local network activity for sync (LAN UDP multicast, no remote endpoints, no telemetry, receiver-only). Document the Android permissions.

### Step 5.8 — User manual update

**File:** `docs/USER_MANUAL.md`

New "Ableton Link" section: what it does, what it doesn't sync, how to use, per-platform availability table, PortMaster AUDIOENGINE-1 deviation.

### Step 5.9 — PortMaster packaging note

**File:** `packaging/portmaster/README.md`

Note Wi-Fi requirement.

**Phase 5 deliverable:** Connect/Disconnect button visible on row 5 when Link is selected; status messages render on right side of row 6; BPM row blurs when connected; privacy policy + user manual + packaging README updated; settings round-trip preserves `clockSource: 2`.

---

# Phase 6 — Platform rollout

**Goal:** Ship on all five supported platforms. PortMaster is primary; Windows, Linux, macOS, Android all ship as first-class targets.

### Step 6.1 — PortMaster / TrimUI Brick (primary)

**Gates:** Phase 1.0 (compile spike), 1.10 (Wi-Fi multicast), 1.11 (CPU budget) all green.

**Ship criteria:**
- Tempo sync works on real Wi-Fi with LinkHut peer
- AUDIOENGINE-1 deviation documented in `docs/ableton-link-test-plan.md` (~10-15 ms best-effort)
- CPU budget acceptable (no frame drops on dense songs)
- Privacy policy + user manual + PortMaster README updated

**Inherited variant:** Anbernic RG353V uses the same `choochootracker.aarch64` binary (same architecture, same Makefile.portmaster output). It works on RG353V hardware but is not the primary test target; the maintainer may want to add a separate verification step for RG353V's ArkOS CFW Wi-Fi behavior, which can differ from TrimUI Brick's stock firmware Wi-Fi behavior.

### Step 6.2 — Windows (MSYS2 UCRT64)

**Ship criteria:**
- Buffer 256 → ~5.3 ms measured latency
- AUDIOENGINE-1 < 3 ms alignment vs LinkHut click
- Firewall UDP-multicast allow note in install README (`docs/build-notes.md`)

### Step 6.3 — Linux / Steam Deck

**Ship criteria:**
- Same as Windows
- Steam Deck Wi-Fi tested (already documented in `docs/build-notes.md:53-60`)

### Step 6.4 — macOS (universal arm64+x86_64, macOS 11+)

**Ship criteria:**
- Same as Windows
- CoreMIDI unaffected (separate sync source)
- App Store sandbox network entitlement if applicable

### Step 6.5 — Android (arm64-v8a + armeabi-v7a, API 26+)

**Ship criteria:**
- All four manifest permissions merged (`INTERNET`, `ACCESS_WIFI_STATE`, `ACCESS_NETWORK_STATE`, `CHANGE_WIFI_MULTICAST_STATE`)
- `WifiManager.MulticastLock` acquired on Connect, released on Disconnect (Step 2.7)
- Works on a real Android device (not emulator) with LinkHut peer
- Play Store listing copy updated to mention Link capability + permissions

### Step 6.6 — Web / Emscripten — permanently excluded

Stub. Document in user manual. Browsers cannot join a Link UDP session.

### Step 6.7 — Miyoo / RG35xx (SDL 1.2) — excluded

Stub. Same exclusions as PortMaster if it doesn't compile, plus SDL 1.2 host-time limitations.

**Phase 6 deliverable:** All five supported platforms (PortMaster, Windows, Linux, macOS, Android) ship; Web/Miyoo/RG35xx stubbed and documented.

---

# Phase 7 — Start/stop sync (v2, deferred)

Off in v1. When v2 happens:

### Step 7.1 — Add `linkStartStopSync` settings flag

Off by default. When on, the Link client listens for `setStartStopCallback` events.

### Step 7.2 — Remote start → quantized Play

Link client's start-stop callback bumps a serial. UI tick poll sees the serial and calls `chipnomadQueuePlaybackStartSongAtTime` with the next quantum-4 boundary (reusing Phase 4 mechanism).

### Step 7.3 — Remote stop → existing atomic Stop

UI tick poll calls `chipnomadQueuePlaybackStop` (existing atomic stop path).

### Step 7.4 — Joining must not start or stop us

Per Link v3 contract: ignore start/stop events until we've been enabled for at least one peer-discovery cycle. Implement by `joinedAt` timestamp check.

---

# Risks (ordered, with mitigating phase)

| # | Risk | Mitigated in |
|---|---|---|
| 1 | Default buffer vs 3 ms spec | Phase 4.2 (latency compensation). Buffer defaults: PortMaster 512 (~10.7 ms, documented ship exclusion), Windows/Linux/macOS desktop 256 (~5.3 ms, passes spec), Android 256, Web 1024 (excluded anyway), Miyoo 4906 (~102 ms, excluded). |
| 2 | Unknown SDL latency on ArkOS | Phase 6.1 (PortMaster documented as ship exclusion for AUDIOENGINE-1) |
| 3 | Extra network thread + Wi-Fi on TrimUI Brick | Phase 1.11 (CPU budget spike) + Phase 6.1 |
| 4 | `tickRate` as both document field and live session parameter | **Resolved by maintainer decision** — existing "Save writes live tempo to file" behavior is intentional backwards compat. |
| 5 | Grooves/`SPD` vs users expecting lock-step bars | Phase 5.8 (user manual documents what isn't synced) |
| 6 | Privacy policy / Android permissions | Phase 5.7 + Phase 6.5 |
| 7 | Quantized launch vs immediate Play | Phase 4.5 (solo Play with no peers = immediate) |
| 8 | HostTimeFilter accuracy on SDL | Phase 4.1 + Phase 4.6 |
| 9 | Receiver-only contract violation by future contributor | Phase 2.3 (header comment + no wrapper functions for forbidden APIs) |
| 10 | Android `MulticastLock` lifecycle bugs (leaks, missed release) | Phase 2.7 (mirrored acquire/release pattern) + Phase 6.5 (real-device verification) |

---

# What is NOT in scope

- **Link Audio** (network audio channels)
- **Web / Emscripten Link** (no UDP multicast in browsers)
- **Start/stop sync in v1** (deferred to Phase 7 / v2)
- **MIDI clock master mode** (already reserved as "future PR" per `midi_router.h:101`)
- **Lowering the default `audioBufferSize`** (research note L193 forbids; buffer defaults stay per-platform)
- **The `originalTickRate` field** (per maintainer decision, existing save-while-slaved behavior is intentional backwards compat)

---

# Pre-Phase 0 maintainer verification checklist

Before starting Phase 0:

- [ ] `g++ --version` inside `ghcr.io/monkeyx-net/portmaster-build-templates/portmaster-builder:aarch64-latest` reports ≥ GCC 10
- [ ] TrimUI Brick device available for Phase 1.10 Wi-Fi multicast spike
- [ ] (Optional) Anbernic RG353V device available for inherited-variant verification
- [ ] Android device available for Phase 3.9 / 6.5 (not emulator)
- [ ] LinkHut (or another Link-enabled app) available on a phone/laptop for peer testing
- [ ] License paperwork filed (Ableton proprietary grant OR GPLv2+ relicense decision)
- [ ] Contributor consent for the chosen license path documented
