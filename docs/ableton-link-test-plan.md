# Ableton Link test plan (AUDIOENGINE-1)

Methodology for verifying Ableton Link tempo sync and beat alignment on each supported platform. The reference peer is LinkHut (or Ableton Live) running on a phone or laptop on the same local network.

## Common setup

1. Start LinkHut at 110 BPM on the peer device, on the same Wi-Fi/LAN as the tracker.
2. In ChooChooTracker: Project screen → Clock source → **Ableton Link** → **[Connect]**.
3. Verify the Link row shows `[Disconnect]` with the status alternating `connected` / `Peers: 1`, and the session tempo (110 BPM) is applied to the project.
4. Press PLAY — playback must align to the next bar of the Link session.
5. Change the session tempo on the peer (e.g. 110 → 128 BPM) and verify grooves, `SPD` and `DELAY` audibly follow.
6. Load a different `.cct` project and verify the session tempo is unchanged (tempo is session-owned while connected).
7. Run a dense song (`psy.cct`, `DNB.cct`) and check for audible dropouts.

## Alignment measurement

Compare the tracker's downbeat against LinkHut's click track:

- Record both outputs (line-out sum or two takes against the same click).
- Measure the offset between the tracker's first downbeat after PLAY and the peer's bar boundary.
- Repeat 10 takes; report the worst-case offset.

## Per-platform targets

| Platform | Audio buffer | Measured latency | Alignment target | Status |
|----------|-------------|------------------|------------------|--------|
| Windows  | 256 frames  | ~5.3 ms          | < 3 ms vs click  | Lab measurement pending maintainer verification |
| Linux    | 256 frames  | ~5.3 ms          | < 3 ms vs click  | Lab measurement pending maintainer verification |
| macOS    | 256 frames  | ~5.3 ms          | < 3 ms vs click  | Lab measurement pending maintainer verification |
| Android  | varies      | varies           | < 5 ms (AudioTrack scheduling jitter) | Pending maintainer verification |
| PortMaster | 512 frames @ 48 kHz | ~10.7 ms | best-effort ~10-15 ms | **Documented ship exclusion** — tempo sync works; beat alignment is best-effort depending on ArkOS Wi-Fi scheduling |

## PortMaster ship exclusion

On PortMaster (TrimUI Brick class devices) the 512-frame audio buffer at 48 kHz yields ~10.7 ms of output latency, and ArkOS Wi-Fi scheduling adds jitter. Tempo sync is exact; beat alignment is best-effort within ~10-15 ms. This is a documented deviation from the < 3 ms lab target and is accepted for ship. See `docs/ableton-link-portmaster-criteria.md` for the hardware pass/fail gate.

## Automated coverage

- `tracker/tests/test_host_time_filter.cpp` — host time filter mapping (audio-thread clock → Link host time).
- `tracker/tests/test_link_tempo.cpp` — tempo follow scenarios (session tempo changes, clamping to BPM_MIN/BPM_MAX).
- `tracker/tests/test_start_at_time.cpp` — timed song start: future target fires at/after the target time, time-0 fires immediately, stop cancels a held start, newer pending starts replace older ones.
- ThreadSanitizer sweep over the Link suites: zero data races (see T42 execution record in `docs/ableton-link-plan/`).
