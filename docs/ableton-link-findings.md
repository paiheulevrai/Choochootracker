# Ableton Link findings

Date: 2026-09-28

Status: research only. Do not add Link code from this note.

This document maps Ableton Link onto ChooChooTracker's clock, threads, licenses, and platforms. It is not a promise to ship Link.

Related agent docs: `docs/agents/CORE_ARCHITECTURE.md`, `docs/agents/AUDIO.md`, `docs/agents/HANDHELD_LINUX.md`, `docs/agents/PORTMASTER.md`.

Upstream: https://github.com/Ableton/link and https://ableton.github.io/link/

## Verdict

| Question | Answer |
| --- | --- |
| Can Link follow this sequencer without a second clock? | Yes, if Link BPM drives `tickRate` and grooves/`SPD` stay local |
| Can we meet Ableton's 3 ms alignment test with the default buffer? | Unlikely on PortMaster; Windows might with a small buffer and honest latency |
| Can we vendor Link into the MIT tree? | Not without a license change or a proprietary Ableton grant |
| Web | Not viable (no UDP multicast) |
| PortMaster / RG353V | High risk even if license were solved |
| Recommended first ship target if ever | Opt-in Windows/Linux tempo+phase only |

Do not implement until the license question is decided. Do not shrink the default 4906-frame buffer to make Link look tighter.

## What Link is

Link is not a shared song pointer. Each app keeps its own timeline. The library maintains a mapping so participants share:

- **tempo** (BPM; last proposal wins; session converges)
- **beat alignment** (integer beats line up; magnitudes may differ)
- **phase** for a client-chosen **quantum** in beats (bar/loop size)
- **start/stop intent** (v3, optional; follows user actions only; joining must not start or stop)

Link Audio (network audio channels) is a separate feature. Out of scope. Do not include `LinkAudio.hpp`.

Capture/commit session state from the **audio thread** for any tempo or transport change. `enable()` is not realtime-safe. Network I/O stays on Link-managed threads; the callback only snapshots.

Host time, not sample time, is the Link clock. SDL does not pass a host timestamp into the callback, so a `HostTimeFilter` (linear regression of system time vs sample time) plus output-latency compensation is required. Ableton recommends ASIO on Windows; this app uses SDL.

Test plan (Ableton `TEST-PLAN.md`) that would bind us:

- Joining must adopt the existing session tempo, not broadcast ours (`TEMPO-2`)
- Loading a song must not change the session tempo (`TEMPO-3`)
- Enabling Link alone with no peers must not jump tempo or beat (`TEMPO-5`, `BEATTIME-1`)
- Audio onsets vs LinkHut clicks: < 3 ms (`AUDIOENGINE-1`)
- Start/stop sync is optional and must not fire on join

## Tracker clock (source of truth)

| Piece | Behaviour | Files |
| --- | --- | --- |
| Tick rate | Project float `tickRate` (Hz). Default 50. UI BPM = `tickRate * 60 / 24` | `project.h`, `screen_project.cpp`, `docs/USER_MANUAL.md` |
| Beat | **24 ticks = 1 beat.** Default 50 Hz => 125 BPM | same |
| Audio tick | `frameSampleCounter += sampleRate / audioProject.tickRate` then `playbackNextFrame` | `chipnomad_lib.cpp` `advancePlaybackFrame` |
| Groove | Per-row tick count, 16-step table. Default groove 0 is `6, 6` then empty | `project.cpp`, `playback.cpp` |
| Phrase | Always 16 rows. At groove 6: 96 ticks = 4 beats = one 4/4 bar of 16ths | `project_constants.h` |
| `SPD` | Persistent per-track rational (or signed) clock. Scheduler still advances at most one row per audio tick | `playback.cpp` `speedNumerator` / `speedDenominator` |
| Delay | Length = `sampleRate * delayTicks / tickRate`. Default `delayTicks` 9 | `master_effects.cpp` |
| Tracks | Independent chains; they can drift on purpose | `docs/USER_MANUAL.md` |

At 48 kHz and 50 Hz ticks, one audio tick is 960 samples (20 ms). The default callback is 4906 frames (~102 ms), so several sequencer ticks can fall inside one SDL buffer.

`playbackState.p` must stay on `audioProject`. UI owns `project`. Audio must not write the project.

Note: the Delay help text says `9 ticks` = `1 beat`. That does not match the Project BPM formula (24 ticks/beat). For Link, use 24 ticks/beat. Do not change delay defaults as part of a Link design.

## Mapping Link onto ticks

Do not invent a parallel transport. Warp the existing tick accumulator.

### Tempo

```text
tickRate = linkBpm * 24 / 60
linkBpm  = tickRate * 60 / 24
```

Apply session tempo to **`audioProject.tickRate` only** while Link is enabled. Grooves, `SPD`, `DEL`, tables, and ping-pong delay all consume ticks, so they automatically follow the session speed.

Do not write that tempo into `project.tickRate` from the callback. If the UI must show the live BPM, publish it through the existing playback-status snapshot.

While linked, ignore the loaded song's stored tick rate for the session (`TEMPO-3`). Keep the file value so Save still writes the user's document, or save the last local tempo separately. Joining with Link already on must take the session tempo, not 125 BPM from a new project.

Tempo range: LinkHut tests 20–999 BPM. Our tick rate editor is 1–200 Hz (2.5–500 BPM). Below/above that, Ableton allows a multiple of session tempo. If we cannot follow 20 BPM, 40 or 80 is acceptable. Document the clamp.

### Beat and phase

Link beats are the 24-tick beats above, **not** phrase rows.

At default groove 6, one phrase is 4 beats. Recommended quantum: **4**. That matches a 16-step bar. Do not recompute quantum from the live groove table; swing would then move the session bar line.

Phase-align only at **quantized launch** (Play, or start-stop sync). While running, follow tempo; do not yank `frameCounter` / groove row to chase phase. Grooves and `SPD` are local expressive timing. Chasing phase every buffer would fight them.

On Play with peers: `requestBeatAtTime(0, quantum)` (or start-playing helper) and delay `playbackStart*` until the host time for that beat, compensated for output latency. Solo (no peers) starts immediately.

Song row 0 / phrase row 0 should correspond to phase 0 at launch. After that, independent tracks may wander. That is existing tracker behaviour, not a Link bug.

### Start/stop

v1 recommendation: **off**. Play/Stop stay local. Link start/stop is a second settings flag (`enableStartStopSync`). Joining must not start or stop us.

If enabled later: remote start is a quantized Play; remote stop is the existing atomic Stop. Do not commit start/stop from the UI thread while the audio callback is running.

### What not to sync

- Groove tables, `GRV` / `GGR`
- Per-track `SPD`
- Song / chain / phrase cursors
- Mute, solo, mixer
- Link Audio streams

## Threading and latency

Realtime rules still apply (`docs/realtime-architecture.md`, `docs/agents/AUDIO.md`):

- No malloc, files, or locks in the callback
- No network sockets from `chipnomadRender`
- `Link::enable` from UI only
- Audio: `captureAudioSessionState` / `commitAudioSessionState` only
- Tempo/transport commits from audio, not from `setTempoCallback` (that callback is not audio-thread)

SDL callback time is "now", not "this buffer hits the DAC". Add device output latency, then filter. On PortMaster the true DAC latency is not measured. Guessing wrong by tens of ms fails `AUDIOENGINE-1` even if tempo is correct.

Default native buffer ~102 ms vs 3 ms spec: latency compensation can still align **speaker** hits if the reported latency is right. It cannot make UI play cursors or table scopes feel tight. Handheld CFW scheduling is why the buffer is large (`docs/agents/HANDHELD_LINUX.md`). Do not lower the default for Link.

Web buffer is 1024 frames (~21 ms). Still no multicast.

## License

ChooChooTracker is MIT (`LICENSE`). Vendored DSP (Mutable snapshots, ayumi, Open303) is MIT.

Ableton Link is **GPLv2+ or proprietary**. Shipping the library in the binary infects the combined work with GPL unless Ableton grants a proprietary license (`link-devs@ableton.com`).

Options if we still want the feature:

1. Relicense the application to GPLv2+. Upstream ChipNomad is MIT; that is allowed for our fork, but the PortMaster zip and Play listing become GPL. Confirm contributor consent for this tree.
2. Ask Ableton for a proprietary Link license and keep MIT.
3. Keep MIT and do not link Link. A separate user-run bridge (e.g. Carabiner) could stay out of process; that is a different product, not this engine.
4. A GPL extra binary (`choochootracker-link`) beside the MIT engine. High maintenance; PortMaster still ships one zip.

Do not vendor `Ableton/link` "just to try" in `chipnomad_lib/external/`. That would already be a license event.

Link also pulls **Asio standalone** (header-only). Build needs `LINK_PLATFORM_LINUX=1` or `LINK_PLATFORM_WINDOWS=1`, `_WIN32_WINNT` on Windows, and the Asio include path. Official minimum: GCC 10 / Clang 13 / MSVC 2022. PortMaster's documented Ubuntu 20.04 WSL toolchain is **GCC 9** (`docs/build-notes.md` LTO ICE). That is below Ableton's verified floor.

## Platforms

| Target | Link protocol | Practical notes |
| --- | --- | --- |
| Windows | Yes | Best lab. SDL not ASIO. Firewall must allow UDP multicast. HostTimeFilter required |
| Linux / Steam Deck | Yes | Buffer 512 was a local Deck test only; do not make it default |
| PortMaster ARM64 | Maybe | Wi-Fi power save, CFW firewall/multicast unknown, extra asio thread vs CPU budget, GCC 9, no hardware measurement. Privacy policy currently claims no network |
| Android | Maybe | Needs `INTERNET` + multicast lock. Privacy policy and Play text would change. AAudio timestamps beat SDL if we ever leave SDL |
| Web / Emscripten | No | Browsers cannot join a Link UDP session |

Privacy policy (`docs/privacy-policy.md`): "The app has no ... network service." Link is local multicast, not analytics, but it is still network. Any ship needs an opt-in, a policy update, and (Android) permission copy. Do not enable Link by default.

PortMaster is packaging/launch only. A Link build would still be the same `choochootracker.aarch64`; gptokeyb is irrelevant. Wi-Fi on RG353V is a handheld/OS problem, not a zip-layout problem.

## Suggested architecture if license clears

Keep ChipNomad structure. No new sequencer.

1. Settings flag `linkEnabled` (not a `.cct` field). Optional `linkStartStop`.
2. Platform-owned `ableton::Link` instance, constructed on UI thread, destroyed after `audioManager.stop()`.
3. In `advancePlaybackFrame`, if enabled: capture audio session state; set a live tick rate from `tempo()`; do not assign through UI `project`.
4. Play path: quantized launch using `requestBeatAtTime` + latency-compensated host time; then existing FIFO play commands.
5. UI reads peer count and live BPM from playback status. 40x20: one Project or Settings line, not a new screen chrome dump.
6. Tests: tempo mapping 24 ticks/beat; join does not write project tempo; enable with no peers does not jump; start/stop flag off ignores remote play. Hardware: RG353V CPU with Link on and a hybrid song; Windows LinkHut click alignment.

Link Audio, Web, and silent PortMaster-on remain out.

## Risks (ordered)

1. GPL vs MIT (ship blocker)
2. Default ~102 ms buffer vs 3 ms Ableton test
3. Unknown SDL latency on ArkOS
4. GCC 9 vs Link's GCC 10 floor on the PortMaster toolchain
5. Extra network thread + Wi-Fi on RG353V (unmeasured)
6. `tickRate` as both document field and live session parameter
7. Grooves/`SPD` vs users expecting lock-step bars
8. Privacy policy / Android permissions
9. Quantized launch vs current immediate Play

## Do not do

- Network or Asio from the audio callback
- `commitAppSessionState` for tempo while audio is running
- Mapping phrase rows or groove steps as Link beats
- Changing `project.tickRate` from audio
- Shipping Link Audio
- Emscripten "Link"
- Lowering default `audioBufferSize` without an RG353V check
- Treating this note as a feature checklist
