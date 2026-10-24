# Ableton Link implementation — execution records and deviations

Record of the implementation run against `ableton-link-coding-llm-spec.md` (T1-T54) and `ableton-link-implementation-plan.md`. Both source documents are copied into this directory for reference.

## Phase summary

| Phase | Tasks | Status |
|-------|-------|--------|
| 0. Vendor + build wiring | T1-T12 | Done (T12 hardware-gated: pending maintainer verification) |
| 1. Link client module | T13-T21 | Done |
| 2. Tempo sync wiring | T22-T28 | Done (T22 hardware-gated: pending maintainer verification) |
| 3. Tempo follow + tests | T29-T38 | Done |
| 4. Play-path alignment | T39-T42 | Done |
| 5. Project screen UI | T43-T45 | Done |
| 6. Docs | T46-T50 | Done |
| 7. Hardware verification | T51-T53 | Pending maintainer verification (hardware-gated) |
| 8. Final validation | T54 | Done (see below) |

## Test results

- Full doctest suite: **650/650 passing** (`make -f Makefile.test`).
- ThreadSanitizer sweep (T42): **zero data races**. The full suite under TSan exceeds a 1200 s budget because of the ~112M-assertion stretch tests; 561/650 cases were validated before the timeout, and the single "failure" was the timeout's own SIGTERM (the 4 "signal-unsafe call inside of a signal" warnings are doctest's `FatalConditionHandler` reacting to that SIGTERM, not real findings). The Link-relevant suites were then run directly under TSan and pass clean with zero warnings: "timed song start" (4 cases), "Link tempo sync" (3 cases), "Host time filter" (5 cases).
- Linux desktop build: EXIT=0.

## Deviations from the spec

1. **`asio_include/` layout** — vendored asio headers live under `chipnomad_lib/link/asio_include/` rather than the spec's flat layout; platform makefiles point at that path.
2. **`LINK_PLATFORM_MACOSX`** — used as the macOS platform define (the spec's `LINK_PLATFORM_MACOS` does not exist in the vendored Link version).
3. **Client API surface** — the client exposes `enable(bool)` / `numPeers()` helpers beyond the spec's minimal surface; used by the UI and app wiring.
4. **`commitAudioSessionState` scope** — used only for the local quantized-launch alignment (`linkClientRequestBeatAtLaunch` = capture + `requestBeatAtTime(0.0, target, quantum)` + commit). The app never calls `setTempo`/`setIsPlaying` and never pushes transport: receiver-only, per the spec's hard constraint.
5. **TEMPO-5 test** — checks `connecting` (not `noPeers`) as the transient state after `connect()` before discovery completes.
6. **T39 output latency** — implemented as a `__attribute__((weak))` function `outputLatencyMicros()` in `link_client.cpp`, overridden by the SDL2 corelib audio backend with the measured buffer latency (`spec.samples * 1e6 / spec.freq`, set after `SDL_OpenAudio`), instead of the spec's extern variable. PortMaster 512 frames @ 48 kHz ≈ 10667 µs.
7. **T40 timed start** — `AudioCommandQueue` pending-start uses slot handoff (`pushPendingStart`/`takePendingStart`, newer replaces older) and the audio thread services the start against the per-chunk `dueMicros` computed in `advancePlaybackFrame`, rather than a separate timer thread.
8. **T41 wiring sites** — `queueLinkAlignedSongStart` is wired at three sites: song-level Play, Shift+Play song-level, Shift+Play chain/phrase-level. The MIDI-clock transport site and the MIDI CC songPlayStop site intentionally remain immediate (transport there is driven by the external sender).
9. **T43 button position** — the Connect/Disconnect button is drawn at column 26 (the spec's column 25 would overlap the clock-source label's clear rect).
10. **T44 sub-focus** — implemented via `getColumnCount` returning 2 for the clock-source row in Link mode (framework column navigation + `validateCursorBounds`), with the `clear` action on the button cell disconnecting.
11. **T45 tempo display** — reuses the existing whole-BPM rendering (`bpmValue` / `liveClockBpm()`, `%u BPM`) instead of the spec's `bpmTenths` `%u.%u BPM`; the tracker's tempo is whole-BPM in this build.
12. **Status refresh** — the Project screen `draw()` overlay refreshes the Link status line every frame (beyond the spec's full-redraw-only updates) because the status changes asynchronously between full redraws.
13. **T42 scope** — no code changes; the TSan sweep is recorded above.

## Hardware-gated items (pending maintainer verification)

- T12: PortMaster toolchain build on-device.
- T22: Wi-Fi multicast spike on TrimUI Brick.
- T23: CPU budget spike on TrimUI Brick.
- T51/T52/T53: hardware verification runs (PortMaster, Windows/Linux/macOS, Android) — checklists in `docs/ableton-link-portmaster-criteria.md`.
