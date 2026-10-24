# Native chip instruments — local implementation report

This extends `feature/native-chip-instruments`, based on personal/r36h
`c0a8c7e9ee7e177e94294ede7e6cad09c59f06c8`. The existing OPLL checkpoint
`1118786` is preserved. The DX7 addendum supersedes only the earlier DX7 exclusion;
full Dexed, JUCE and unrelated engines remain excluded. The user has now authorized a branch on their fork and an update of the device
personal build, preserving its existing features. No pull request is authorized.
The ARM64 personal build and full enabled suite pass using the device’s existing
GCC 9 toolchain. The 64-case benchmark, ten-minute balanced-arrangement soak,
real SDL/ALSA playback and combined production startup checks are complete.
The validated source extends the existing device personal build; installation
identity and rollback are recorded in its `personal-build.json`.

## R2 fixes — host and handheld machine validation passed

The prior ARM64 results below describe the installed `0146fcf` lineage, not the
new R2 DSP. R2 adds a 3 ms per-voice FM onset/retrigger transition, 1 ms gain
smoothing, optional FM amp ADSR, brightness/feedback controls, Sega extended
bass range, engine-specific Sega/GB phrase FX and modulation, and ADSR graph
clearing. Native envelopes, existing IDs, the 16-note DX7 budget, eight tracks,
two inserts per track, and all personal features remain in place.
Instrument (680 bytes) and Project (313,256 bytes) sizes remain unchanged.
The appended FX state increases host ChipNomadState from 782,032 to 782,928
bytes; the previous size figures below describe the installed baseline.

Host validation: 370 test cases / 74,085,553 assertions passed; all 13 bank
conversion/import tests passed; MSFA ASan/UBSan harness exited 0, and all vendored
hashes match the Apache-2.0 provenance manifest. Mac personal production and Web
builds passed; regenerated WebAssembly validates in Node. The real SDL UI
regression covers ten ADSR pages, both waveform-header states and twelve edits
each, comparing each incremental render with a fresh render. No new dependencies
or bank sources were added. Full Dexed and JUCE remain excluded.
The personal-feature-enabled suite also passed: 382 cases / 74,095,590
assertions, with two opt-in tests skipped. The final offscreen SDL run passed
the type popup, ADSR pixels, browser/import/audition and playback checks.

The actual Sega phrase test reproduces G-2 and F-2 collapsing near 109 Hz with
native range and restores their distinct pitches with Extended enabled, for
both linear and AY-period projects. FM tone/amp settings round-trip through
CNI/CCT; phrase FX produce the same audio as equivalent saved settings without
mutating the project.

The default-patch offline diagnostic at 48 kHz measured smaller maximum adjacent
sample changes during the first 3 ms of onset and retrigger for all seven FM
types. Six chip adapters now bridge the exact retrigger boundary continuously;
DX7 applies its transition at the next native quantum before FIR resampling.
This is limited numerical evidence, not proof that every reported audible click
is resolved. Hard cut/panic remains immediate; zero-release envelopes and native
patch transients can still be abrupt. Handheld machine validation is recorded below. Listening acceptance remains
pending; installation identity is recorded in the progress document.

For the R2 hardware benchmark, use `benchmark_native_chips 30 --songs-only
--sample-mix --four-inserts --song-fm-amp` alongside the same command without
`--song-fm-amp`. The optional flag enables ADSR in each FM song instrument and
can export that project through the existing `CHOOCHOO_BENCH_PROJECT` path for
the physical ALSA check. Host timing is not a substitute for handheld timing.

The library remains **876 CNI files / 812 FM catalogue entries / 704 distinct
FM parameter sets**, including **67 distinct DX7 patches**. The 1,000 cleared
DX7-preset goal remains unmet; 10,000 is only a synthetic browser stress fixture.

### R2 measured handheld results

The personal ARM build passed 382 tests / 74,095,590 assertions,
with 2 opt-in tests skipped. The real SDL type popup, repeated ADSR pixel,
browser/import/audition and playback checks passed. The combined offscreen
audio-render plus UI-draw loop measured p95 28.766 ms and worst 29.223 ms
for 1,024 frames at 48 kHz, exceeding that block duration of 21.333 ms.
This functional UI check is not a passing small-buffer real-time claim; the
separate physical audio checks below use the preserved 4,906-frame setting.

Thirty-second offline render fixtures at 48 kHz / 512 frames:

| Arrangement | FM amp | Mean µs | p95 µs | Worst µs | Deadline misses |
|---|---|---:|---:|---:|---:|
| Single notes; 4 WAV + 2 chip + 2 FM, four inserts | Bypass | 7441.224 | 7989.042 | 8299.375 | 0 |
| DX7 chord; 4 WAV + 2 chip + 2 FM, four inserts | Bypass | 7709.263 | 8283.333 | 8670.958 | 0 |
| Single notes; 4 WAV + 2 chip + 2 FM, four inserts | ADSR | 7513.280 | 8151.208 | 9367.167 | 0 |
| DX7 chord; 4 WAV + 2 chip + 2 FM, four inserts | ADSR | 7888.301 | 8583.750 | 8994.125 | 0 |

Two 70-second physical SDL/ALSA checks used the existing direct-card route
`plughw:0,0`, S16/48kHz and 4906 frames, with test-only master gain 0.4.

| Fixture | p95 µs | p99 µs | Worst µs | Render misses |
|---|---:|---:|---:|---:|
| bank | 23086.291 | 23436.000 | 23742.833 | 0 |
| balanced-amp | 82729.208 | 83447.875 | 85060.500 | 0 |

Neither check logged ALSA underruns or nonfinite samples. Production startup
passed with the waveform header OFF and ON; installed settings and autosave
remained unchanged. The launcher was restored after the tests. These checks
measure rendering and delivery; they do not confirm subjective click removal.
No voice/track/insert limits changed. Full receipts: chip-r36h-r2-validation.json
and chip-r36h-r2-benchmark.csv.

## Delivered instrument paths

| Type / stable ID | Implemented and native persistence | Machine tests | Packaged CNI | R36H |
|---|---|---|---:|---|
| OPLL / YM2413, 17 | Yes | Pass | 15 | Tests/UI pass |
| VRC7, 18 | Yes | Pass | 15 | Tests/UI pass |
| OPL2, 19 | Yes | Pass | 241 | Tests/UI pass |
| OPL3, 20 | True four-op and dual voice | Pass | 456 | Tests/UI pass |
| Sega PSG, 21 | Tone, white/periodic noise | Pass | 14 | Tests/UI pass |
| GB Pulse, 22 | Native duty/envelope/sweep | Pass | 10 | Tests/UI pass |
| GB Noise, 23 | Native divisor/shift/width/envelope | Pass | 10 | Tests/UI pass |
| DX7 FM, 24 | Six operators, all original voice parameters | Pass | 67 | Tests/UI pass |
| Genesis FM / YM2612, 25 | Four-op native envelope/routing/LFO | Pass | 24 | Tests/UI pass |
| Arcade FM / YM2151, 26 | Four-op native envelope/routing/LFO | Pass | 24 | Tests/UI pass |

The instrument hierarchy, metadata, common FM browser, bounded voice lifecycle,
existing track inserts/sends, project handoff and serializers are extended in
place. Retired IDs 14/15 and MIDI 16 remain unchanged; no FX identifiers were
renumbered. Existing fonts/theme, piano, waveform options, mixer, MIDI and
personal experiments are retained. The personal desktop package enables
`CHOOCHOO_EXPERIMENTAL_MOD_LUCKY=1`; web retains that experiment's existing off policy.

FM has bank/category/All browsing, quick previous/next, full-name helper,
EDIT+PLAY audition, EDIT confirm and OPT cancel. Preview owns its patch and
never commits tables or track inserts. The same browser handles local DX7
SysEx. Simple chip pages expose their native controls and starter presets.
Optional favorites/search and a full operator editor were not added. R2 adds
the FM and chip controls documented in the user manual.

## Dependencies and ownership

| Core | Revision | License / audit |
|---|---|---|
| ymfm | `81aec25ccbb98f4873a255f7551ac4dadac59b4a` | BSD-3-Clause, unchanged selected OPL/OPN/OPM plus closure |
| emu76489 | `c0fa097060e022db237163d79704025435042997` | MIT, documented Sega noise-clock correction |
| gb_apu | `3d73d0df027a82d854cacd72a179c2d6a1a9703e` | MIT, selected relicensed C++ Blip closure, C++ allocation cast and equivalent positional tables for GCC 9 |
| MSFA scalar core in Dexed | `2e182b3db85c09083ab13c8b9b00565ce7d9ff85` | Apache-2.0 file audit, namespace/host decoupling, small note adapter |

File hashes and modifications are under each `chipnomad_lib/external/*` vendor
folder. Runtime licenses and Apache NOTICE are under `packaging/common/licenses`.
No full Dexed/JUCE/MTS/MPE or extra DX7 core is linked. Source collections and
engine permissions were reviewed separately.

Patches are fixed owned values; Instrument remains 680 bytes, its union 624
bytes and Project 313,256 bytes. The audio command payload layout is 568 bytes (64 slots: 36,352 bytes);
ChipNomadState is 782,032 bytes on this host. Project snapshots have not grown.
Voices are allocated by bounded track/chord
slot, not by catalogue entry. DX7 has one shared LFO per track part and four
independent note states per part; unrelated tracks and preview are isolated.
DX7 uses a measured **16-active-note global budget** within those existing owned
slots, including release tails. Over-budget note events take releasing voices
first, then quiet held notes, then fresh attacks. Equal attacks preserve roots
across tracks before chord extensions, with deterministic slot/track ordering.
Other instrument polyphony is unchanged; DX7 preview is blocked during playback.
The same policy applies across platforms. Raw backend 32-note stress remains
available for measurement, bypassing the sequencer's admission policy. MSFA global tables are
initialized once off callback at 44.1 kHz, then streamed through the common FIR
for arbitrary host rates. This avoids changing global tables beneath another
renderer. Event quantization is bounded by 64 native samples (1.45 ms) plus
approximately 0.25 ms FIR delay. No remainder samples are dropped. Yamaha key
retrigger observes one native key-off sample before key-on. Genesis uses a
20 Hz DC blocker around the modeled ladder output. Native FM rate/level envelopes
are preserved; volume changes are post gain, DX7 strike velocity is an owned
wrapper default of 100, and pitch updates preserve live phase/envelope state.

CNI/CCT version 6 stores complete patches, tuning and bounded source metadata.
Old formats 1–5 remain readable; older applications cannot load version 6.
Copy/paste/clone and source-independent reopen are tested. Bad native imports
are transactional. Loading a bank never sends MIDI messages.

## Content and imports

| Bank | Source entries / packaged | Distinctness and exclusions |
|---|---:|---|
| FatMan 2-op | 181 / 181 | 53 percussion, explicit embedded MIT notice |
| FatMan 4-op | 181 / 181 | 180 true four-op, 53 percussion, MIT |
| DMXOPL3 | 335 / 335 | 252 dual, 5 four-op, 183 percussion, MIT |
| OpenDX7 originals | 31 musical / 31 | MIT; INIT excluded; data-only literal conversion |
| YSE author CC0 bank | 32 / 4 | Four unique templates; renamed duplicates excluded |
| ChooChoo DX7 originals | 32 / 32 | Independent CC0 recipes, not ROM copies |
| ChooChoo Genesis | 24 / 24 | Original MIT recipes |
| ChooChoo Arcade | 24 / 24 | Original MIT recipes |
| OPLL / VRC7 tables | 15 + 15 | Pinned BSD-licensed ymfm tone data |
| Sega / GB Pulse / GB Noise | 14 + 10 + 10 | Original MIT recipes |

Total: **876 CNI files**, **812 shared FM catalogue entries** and **704 distinct
normalized FM parameter sets**. OPL's 697 source aliases represent 589 parameter
sets; source-bank identities remain preserved. DX7 has **67 distinct parameter
patches from 95 parsed entries**. Source full-record hashes, parameter hashes and
aliases are separate in the manifests. The 10,000-entry index test is synthetic
and does not inflate the factory count.

The DX7 1,000-cleared-preset goal is **unmet**. Benson's mixed collection includes
factory/unknown origins; Bobby Blues' broad web compilation is not blanket
cleared, and the reviewed direct Pafreak submission contains DX7II extensions.
BlackWinny and CC0 mirrors do not establish every contributing author's grant.
These files stay outside shipping assets. Source-specific review records under
`tools/chip_banks/evidence` identify the evidence and exclusions. Broad Genesis
collections similarly had no verified bank-specific clearance; original banks
provide the requested fallback. Human listening/category refinement remains open.

Offline `import_bank.py` supports exact 42-byte TFI, original VOPM text,
verified WOPLX, and original DX7 SysEx. VOPM pan 0/64/127 maps to left/both/right;
partial pan, noise data and extended fields are rejected. AM enable uses the
original file's 0/128 field. Binary WOPL is not implemented. Runtime DX7 import
supports framed 155-byte voice and 4096-byte packed-bank payloads, including
bounded multiple messages, strict lengths/ranges/seven-bit/checksum/terminator
validation, and atomic rejection. Headerless dumps and bad-checksum overrides
are not enabled. Imported DX7 banks remain session metadata; selected instruments
can be saved as portable CNI/CCT. Offline user imports get a separate manifest,
with no redistribution permission inferred. ZIP inspection is offline, bounded,
path-safe and non-executing.

## Verification and artifacts

Baseline: 328 tests / 8,101,169 assertions passed before integration.
Final standard host suite: **361 tests / 72,681,789 assertions passed**.
Final ARM64 personal experiment suite: **373 passed, 2 deliberately skipped**,
72,691,826 assertions passed. Python content tests: **13 passed**. A latent MIDI test passed
an uninitialized destination into a replacing/freeing loader; its setup is now
initialized. Production loader behavior was not changed for that test issue.

Coverage includes native save/reopen, failure transactions, same-patch independent
voices, fine pitch at multiple output rates, uneven render blocks, release/cut,
retrigger, all DX7 algorithms, direct MSFA and ymfm register references, all 876
native file reloads, every FM preset's finite audible render, 10k metadata scale,
local SysEx parsing, and production SDL browser/audition/clone workflows. Selected
MSFA scalar code also passed AddressSanitizer/UndefinedBehaviorSanitizer. Host
visual testing used SDL dummy output; no visible emulator/editor was launched.

Actual commands and build details are in `docs/build-notes.md` and the progress
checkpoint. Desktop macOS x86_64 personal build and the existing Emscripten web
build succeed. The checked-in web/dist bundle is regenerated. Windows/Android
builds were not run. The ARM64 personal build and package pass on the handheld
with its existing compiler. Existing Docker images were inspected only; none
was an ARM64 Linux project builder. No second SDK was installed.

`tracker/packaging/common/projects/native-chip-audition.cct` owns thirteen bank
representatives and plays without the factory folder. `.tmp/chip-audit/auditions/`
contains thirteen WAVs with four selected sounds each (52 machine auditions),
short phrases, held notes and release segments. `docs/chip-preset-auditions.tsv`
records source preset paths and peak/RMS/DC. No subjective listening is claimed.
Large WAVs stay out of Git. All 876 generated CNI files and their catalogues/
manifests regenerated byte-identically in a separate ignored output directory.

OPL native register values and source volume-model/velocity-offset/duration
metadata are retained. Playback uses native full-velocity levels and tracker
post gain; it does not reproduce ADLMIDI player-specific MIDI volume curves,
and duration estimates never truncate a held note or release tail.

## Measured host performance

Optimized macOS x86_64 build, `-O3 -DNDEBUG`, 62 configurations, 30 seconds of
rendered audio per configuration. Exact CPU model was unavailable in this session.
At 48 kHz / 512 frames, DX7 with 16 voices measured mean 143.248 µs,
p95 196.915 µs, p99 281.532 µs, worst 386.248 µs. With 32 voices:
mean 292.164 µs, p95 419.497 µs, p99 550.174 µs, worst 689.200 µs.
Neither DX7 case missed its 10.667 ms render deadline.

The full stress sweep recorded **three deadline misses**, all in the 32-voice
OPL2 case (worst 22.245 ms). Other validation/build work overlapped portions of
host measurement. These outliers remain recorded; no universal glitch-free
polyphony claim is made. See `docs/chip-all-chip-benchmark.csv` for every row.

A separate paced 600-second FM-heavy eight-track song, with inserts/sends and
9,566 concurrent scans of a synthetic 10,000-preset catalog, measured p95 2.903 ms,
p99 2.939 ms, worst 4.409 ms against a 10.667 ms render deadline, with **zero
render deadline misses**. Peak resident memory was 112,623,616 bytes; this is a
high-water mark, not a memory-growth trace. See `docs/chip-soak-benchmark.csv`.
Measurements cover render duration, not scheduler wakeups, physical audio-device
underruns, handheld governor/thermals or a human listening test. The DX7 16-note budget was subsequently selected using R36H measurements,
including release tails within the existing four owned slots per track.

Final production SDL dummy UI smoke passed, including all ten pages, preset
preview/cancel/confirm, local DX7 import, clone, and sequencer playback. Its
48 kHz / 1024-frame song-plus-UI p95 was 1.456 ms, p99 2.041 ms, worst 2.667 ms.
It also checks that playback blocks preview and displays the stop-song hint.
The host tools now include their own generated header dependencies to prevent
stale object layouts after voice-header changes.

## Measured R36H performance

The production profile uses GCC 9 with the existing size optimization, plus
`-O3` for the isolated new chip cores and their adapters only. A diagnostic
comparison also optimized four existing audio-processing files, but those
alternate objects were not adopted. No fast-math or reduced-quality mode is used.
The device's interactive governor, clock and installed audio settings are retained.

`docs/chip-r36h-benchmark.csv` contains **64 measured configurations**, each with
30 seconds of rendered audio after warmup. All **50 isolated engine cases** had
zero render deadline misses: DX7 at 1/4/8/16/32 notes, 44.1/48/96 kHz and 128/512
frames, plus ten other chip/topology cases at 1/8 notes, 48 kHz and 512 frames.
At 48 kHz/512, DX7 16 notes measured mean **1.911 ms**, p95 **2.015 ms**,
p99 **2.038 ms**, worst **2.143 ms**. The earlier size-optimized 32-note sweep
had occasional deadline misses; the retained 16-note policy also leaves room
for mixing and effects. The final optimized 32-note raw case passed this run;
that is not a promise of a complete 32-note song plus arbitrary effects.

The remaining fourteen configurations measure actual sequencer songs. Two
insert slots exist **per track**, sixteen total; they are not a CPU guarantee.
The earlier personal-build manual already documents the cost of expensive
inserts. All eight-track FM-heavy scenes with sixteen inserts overloaded.
Four inserts also overloaded the heavier eight-track FM scenes; even two
inserts had occasional deadline misses with those dense arrangements.
Four-track AY/Plaits/FM arrangements also had timing spikes. Track count alone
does not describe the workload. These results remain in the CSV: **13,009
misses across all 64 cases**, including the deliberately overloaded cases.

The user's balanced arrangement is four WAV samples, Sega PSG, GB Pulse,
DX7 and OPL3, with shared reverb/delay sends and **four inserts** (two
Compressors, Doubler, TAPESCAM). Existing packaged WAVs are loaded through the
native loader and loop continuously. At 48 kHz/512 frames (10.667 ms deadline):

| Balanced arrangement | Mean | p95 | p99 | Worst | Misses |
|---|---:|---:|---:|---:|---:|
| Eight single notes | 7.446 ms | 7.988 ms | 8.060 ms | 8.647 ms | 0 |
| DX7 four-note chord, eleven notes total | 7.689 ms | 8.254 ms | 8.333 ms | 12.143 ms | 1 |

The paced **600-second** balanced chord/four-insert soak completed with **3,948
scans** of the synthetic 10,000-entry index. Render time: mean **7.911 ms**,
p95 **8.647 ms**, p99 **9.041 ms**, worst **20.441 ms**; **28 deadline misses**
among 56,250 measured blocks. It is not reported as glitch-free. See
`docs/chip-r36h-soak.csv` and `docs/chip-r36h-validation.json`.

Sixty memory samples span 593 seconds. RSS rose during startup, then stayed
between **50,444 and 50,708 KiB** after the first minute; high-water RSS was
**52,508 KiB**. This is an observed plateau, not a proof against all memory leaks.
Sampled temperature ranged **62.083–74.583°C**; observed CPU frequencies were
1.008, 1.248 and 1.512 GHz with the existing interactive governor.

Initial SDL/ALSA probes used the system default PCM and logged underruns even
when callbacks met their render deadlines. Inspection found that default PCM
routes through a fixed 44.1 kHz dmix, while the regular launcher already sets
`AUDIODEV=plughw:0,0`. Final hardware checks must match that direct-card route.
The S16 fixture uses the preserved 4906-frame setting and a separate test-only
master gain of 0.4 for the deliberately dense summed arrangement; no preset or
installed mix setting is changed. The initial dense F32 fixture reached peak
1.981 before that separate master attenuation; it was not claimed unclipped.

Both final **70-second direct-card S16 tests** passed with **zero render deadline
misses, zero logged ALSA underruns and no nonfinite samples**. Hardware readback
confirmed 48 kHz, a 4906-frame period and a 9812-frame hardware buffer. The callback
deadline was 102.208 ms:

| Physical audio fixture | p95 | p99 | Worst | Peak |
|---|---:|---:|---:|---:|
| Bank demo | 22.132 ms | 24.590 ms | 27.392 ms | 0.093 |
| 4 WAV + 2 chip + 2 FM, DX7 chord and four inserts | 80.990 ms | 81.577 ms | 81.819 ms | 0.792 |

Each test completed 685 callbacks. The production executable also passed isolated
startup with persistent waveform OFF and ON on the same direct-card route.
Installed settings and autosave hashes stayed unchanged during validation, and
the idle frontend was restored. The executable SHA-256 is
`1966139c400e5fc25ca93107b5d442816f5d60c84d026814bef750a6945bc247`.
No human listening or comprehensive ALSA xrun instrumentation is claimed beyond
the captured driver messages. Small-buffer probes and overload results remain
reported rather than being replaced by these successful configured-buffer checks.
The installed 48 kHz / configured 4906-frame setting is preserved; these smaller
512-frame tests do not change it. A larger buffer can absorb brief scheduling
spikes, but cannot make a sustained over-budget workload run in real time.

## Packaging

The reproducible desktop packaging command is `tools/chip_banks/package_desktop.py`.
The local archive is `releases/ChooChooTracker-native-chips-macos-x86_64.zip`;
it contains the personal executable, SDL framework, runtime assets, all 876
native presets, the portable demo, source-specific notices and this report.
The packager checks preset count, required notices and ZIP CRCs and records a
SHA-256 inventory. This macOS archive is not a handheld installation package.

## Remaining acceptance work

- Complete human listening of the supplied bank WAVs and demo; refine ambiguous
  categories and balance only with explicit reversible wrapper settings.
- Acquire further author-cleared DX7 content if the 1,000-sound goal remains
  desired. The 67-preset starter and large user-library import path are delivered.
- Binary WOPL, OPM noise/partial-pan variants, DX7II/performance extensions,
  runtime archives, search/favorites and operator editing are unsupported as
  described above. No claim is made that these formats are silently equivalent.
- Heavy FM/insert combinations and smaller audio buffers can exceed the measured
  budget. Preserve the current launcher route and audio settings when comparing
  results. The validated balanced example is not a universal performance guarantee.
- Device updates preserve user assets/settings and the regular launcher, verify a
  full rollback copy and record the installed source/binary identity. No PR or
  unrelated system configuration change is part of this delivery.
