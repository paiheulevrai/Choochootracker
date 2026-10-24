# Ableton Link — PortMaster ship criteria

Gate document referenced by the PortMaster ship decision. Records the pass/fail outcomes of the hardware-gated spikes (T22 Wi-Fi multicast, T23 CPU budget) and the hardware verification runs (T51-T53).

## T22 — Wi-Fi multicast spike (PortMaster / TrimUI Brick)

**Goal:** verify the device can join Ableton Link discovery (UDP multicast on port 20808) over ArkOS Wi-Fi and stay connected.

**Outcome:** PENDING MAINTAINER VERIFICATION — requires physical hardware. The implementation path is in place (link client discovery + Android multicast lock equivalent is not needed on Linux/ArkOS; multicast socket options are set by the vendored Link library).

## T23 — CPU budget spike (PortMaster / TrimUI Brick)

**Goal:** verify the Link client's audio-thread cost (host time filter + tempo follow) fits the existing CPU budget on dense songs.

**Outcome:** PENDING MAINTAINER VERIFICATION — requires physical hardware. The audio-thread work is bounded: one host-time filter evaluation per render chunk plus a tempo read; no Link network I/O happens on the audio thread.

## T51 — Hardware verification: PortMaster

**Checklist** (run LinkHut at 110 BPM on a phone on the same Wi-Fi):

- [ ] Project screen Link row shows `connected` / `Peers: 1` (alternating)
- [ ] Session tempo 110 BPM applied (no BPM field is shown in Link mode)
- [ ] Grooves/SPD/DELAY audibly follow 110 BPM
- [ ] Loading a different `.cct` doesn't change the session tempo
- [ ] Press Play — playback aligns to the next bar (within ~10-15 ms on PortMaster)
- [ ] No audible dropouts on dense songs (`psy.cct`, `DNB.cct`)

**Outcome:** PENDING MAINTAINER VERIFICATION.

## T52 — Hardware verification: Windows, Linux, macOS

Same checklist as T51 with alignment target < 3 ms (lab measurement, 256-frame buffer).

**Outcome:** PENDING MAINTAINER VERIFICATION.

## T53 — Hardware verification: Android

Same checklist as T51 on a real device, plus:

- [ ] The four manifest permissions (INTERNET, ACCESS_WIFI_STATE, ACCESS_NETWORK_STATE, CHANGE_WIFI_MULTICAST_STATE) are requested at runtime on first Connect.
- [ ] `MulticastLock` acquired/released correctly (device logs: "MulticastLock acquired choochootracker-link" / "MulticastLock released").
- [ ] No ANRs.

**Outcome:** PENDING MAINTAINER VERIFICATION.

## Ship decision

The PortMaster ship decision may proceed on the documented exclusion: tempo sync is exact, beat alignment is best-effort within ~10-15 ms (see `docs/ableton-link-test-plan.md`). The hardware rows above must be filled in by the maintainer before the final ship call.
