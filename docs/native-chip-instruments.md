# Native chip and FM instruments

This contribution is based on upstream main `8c87091`. It extracts the current
instrument implementation from the user-accepted R36H personal build. Personal
module discovery, launcher branding, deployment receipts and unrelated sample
stretch/import fixes are not part of this branch. The original development and
deployment history remains on `personal/r36h` and `feature/native-chip-instruments`.

## Included

SID; OPLL and VRC7; AdLib/OPL2 and OPL3; Sega PSG; Game Boy Pulse and Noise;
Genesis/YM2612 and Arcade/YM2151 FM; and DX7/MSFA instruments. Native patches
are owned by the instrument and saved in CNI/CCT files, including song archives.
The contribution retains the tested archive integration for mixed sample/native
songs and embedded wavetable layouts.

The shared preset browser provides bank/category selection and owned audition
previews. DX7 also supports a persistent folder of original SysEx banks.
The checked-in library contains 1,196 preset files, with source manifests,
conversion tools and runtime notices. Entries and unique sounds are different
counts; duplicate source identities and exclusions are recorded by the tools.

Tracker FX use the final compact per-engine list documented in the user manual.
The operator selector has been removed, retained operator commands address
operator 1, and live modulation keeps its explicit operator targets. Preset/range
readouts use native bounds. No retired personal FX translation is included.

## Dependencies and limits

The sources include ymfm, emu76489, gb_apu, the MSFA scalar DX7 component, and
the floooh/chips SID implementation. License/provenance notices accompany each.
Preset content has its own source licenses; consult the packaged notices.
No Dexed application, JUCE, alternate SID model or network importer is added.

SID is a digital/per-cycle 6581-style approximation with a per-note filter,
not a calibrated analog revision model. Native voice allocation and handheld
budgets remain as tested in the accepted personal build. Polyphony, release tails,
other instruments and inserts share the CPU budget; the user manual describes
practical limits. The historical chip report/CSV files retain earlier workload
measurements and their qualifications.

## Review checks

The accepted combined build has host/device validation and user listening behind
it. This isolated contribution builds against current upstream. Five focused cases
passed (927 assertions): compact FX availability, single-command motion recording,
OPLL CNI/CCT persistence, mixed native/sample archives, and DX7 folder rescanning.
The regenerated web build also passed JavaScript syntax and WebAssembly validation.
The extraction was checked with focused tests, without a full suite, stress run,
or new device installation.

### PR #39 MIDI lifetime follow-up (2026-10-06)

Review found that the MIDI tests and standalone converter passed uninitialized
projects into loaders that release the previous instrument data on success.
These callers now initialize an empty project first, matching the tracker UI.
The MIDI API documents that successful imports replace the destination and
failed imports leave it unchanged. No audio-engine behavior changed.

The five MIDI tests pass (49 assertions), including repeated replacement of a
sample-owning project and preservation after missing, malformed, or empty MIDI
input. The converter entry point also passed a MIDI/CCT/MIDI/CCT round trip with
allocation scribbling enabled, linked against the cached core and test support
objects; this was not a standalone release-package build.

### PR #39 FM controls and full-suite follow-up (2026-10-06)

The failing FM test still required all live modulation destinations to appear
in tracker FX. Its assertions now distinguish native modulation metadata from
the intentionally smaller per-engine tracker list, which retains its explicit
availability tests. No removed tracker command was restored.

Additional coverage switches one OPL3 instrument between two-operator,
four-operator, dual-voice, and back to two-operator configurations, checking
actual operator availability. CNI and CCT reload checks preserve each topology,
operator settings, and modulation destinations. The existing instance-level
availability rules passed these checks without synth or runtime changes.

`make -C tracker -f Makefile.test -j4` passed locally: 501 test cases,
112,209,328 assertions, zero failures or skipped cases. This includes the MIDI,
native preset, and existing project/instrument persistence tests. Linux GitHub
CI must still rerun after the branch update; local Docker was unavailable.

### PR #39 Windows portability follow-up (2026-10-06)

GitHub Linux CI and Android passed for `7602f23`. The Windows build found that
the OPLL resampler used the nonstandard `M_PI` macro, which is unavailable in
the Windows C++17 configuration. It now uses a local `constexpr double` with
the same value. The failure reproduced with the MinGW cross-compiler, and the
updated source compiles successfully there. Optimized host object files before
and after this change are byte-identical; the resampling calculation is unchanged.
The full local suite passed again (501 cases, 112,209,328 assertions), and the
regenerated web bundle passed JavaScript syntax and WebAssembly validation.

### PR #39 Windows SID build follow-up (2026-10-06)

The next Windows CI run progressed past OPLL and found the same optional CRT
math constant in the pinned SID implementation. Native Windows and Docker
cross-build flags now enable `_USE_MATH_DEFINES`; the vendored source and sound
calculations remain unchanged. A complete local Windows executable cross-build
passed with MinGW GCC 16.2 and the repository's SDL2 2.32.6 development package.
This validates compilation and linking, not Windows playback or release packaging.
Linux tests, web-contract, and Android passed on the preceding `5e73b36` commit;
current-head GitHub checks must rerun after this build-configuration update.

### PR #39 Windows archive-name follow-up (2026-10-06)

Windows CI compiled and linked `8807037` successfully, then failed while creating
the ZIP because the PR ref `39/merge` became part of the output path. Packaging
now replaces ref slashes with hyphens and quotes the archive filename. The exact
workflow packaging commands passed local ZIP creation/content checks for PR refs,
branch refs, release tags, and the manual fallback. Application and audio code
are unchanged; Windows CI still needs to confirm the complete package upload.

### Maintainer preview and Windows test follow-up (2026-10-06)

The maintainer's waveform previews (`ec50dd1`), native FX grouping (`0dafab7`),
and SID static preview without a DSP voice (`5a2c9f2`) are retained. Review of
the shared preview redraw found that SID bank/preset
fields also use it, although SID has no optional FM amp. The helper now checks
for an amp before drawing its envelope and keeps SID's waveform at its existing
row when the persistent waveform setting is enabled. The original null access
was reproduced with UndefinedBehaviorSanitizer.

The regular test target now includes the real preview helper, with graphics and
envelope-overlay boundaries mocked. Its regression case covers SID and FM,
both persistent-waveform settings, enabled/bypassed FM overlays, and preservation
of the instrument data. `Makefile.test` also enables `_USE_MATH_DEFINES` for
Windows, matching the application build without modifying the vendored SID core.

Validation: all 502 local test cases passed, including the focused preview
regression's 36 assertions. The full Windows test executable cross-compiled and
linked with MinGW; it was not run on
Windows locally. The checked-in web bundle was regenerated, and its JavaScript
syntax and WebAssembly binary validation passed. Audio engine code and the
simplified tracker FX list are unchanged.

### Native FM render-cost audit (2026-10-18)

This local audit covers DX7, OPLL/MSX, VRC7, AdLib/OPL2, OPL3,
Genesis/YM2612 and Arcade/YM2151. Each ymfm-backed tracker chord slot owns a
separate chip but uses only channel 0, plus channel 3 for OPL3 four-operator
or dual-voice patches. The old adapters advanced every chip channel for each
native sample: nine for OPLL/OPL2, eighteen for OPL3, six for Genesis and eight
for Arcade. This repeats unused operator work for every note of a CRD chord.

Optional channel masks now restrict those operator clocks to the occupied
channels. Default callers still render the entire chip. Chip-wide envelope,
LFO and noise clocks, output rate, DAC conversion, resampling and gain remain
unchanged. The restricted mask must stay fixed between resets, and omitted
channels must remain unused. The adapters leave rhythm, channel-3 special
mode, CSM and sample DAC playback disabled. Genesis still includes all six
channels in its DAC calculation: even an idle channel contributes to the
ladder offset, and the adapter's exact idle baseline remains 504 per side.

DX7 is unchanged. Its MSFA core already renders 64-sample blocks, skips
inactive voices and avoids expensive operator rendering below its existing
gain threshold while advancing phase. Its shared part clock intentionally
advances through empty slots to preserve LFO and envelope timing. The other
FM engines still generate one native sample at a time; this change does not
convert them to block rendering or change polyphony limits.

Regression coverage compares full-chip and selected-channel PCM for 64 OPL
configurations, 64 Genesis configurations, 64 Arcade configurations and all
30 OPLL/VRC7 preset tones. It covers algorithms, waveform/SSG settings,
feedback, pan, modulation, live pitch and level edits, release and retrigger.
A separate Genesis check verifies the complete idle DAC baseline. A sequencer
test plays four-note CRD chords across all seven families and all three OPL3
topologies, checking the expected pitches and active, audible chord voices.

The host measurements use the unchanged clean-main baseline `7deefa32`,
Apple Clang 17 on x86_64 macOS, `-O3`, 48 kHz and 512-frame buffers.
Three alternating baseline/optimized runs render ten seconds per configuration
following the existing benchmark warmup. The table reports the median of the
three per-run p50 times. Raw rows are in `native-fm-host-benchmark-20261018.csv`.
The full regression suite was running concurrently; these are offline host
render times, not device callback CPU percentages or thermal measurements.

| Engine | Notes | Before (µs/block) | After (µs/block) | Reduction |
| --- | ---: | ---: | ---: | ---: |
| DX7 | 1 | 17.292 | 17.367 | -0.4% |
| DX7 | 8 | 79.041 | 84.300 | -6.7% |
| OPLL / MSX | 1 | 53.355 | 27.345 | 48.7% |
| OPLL / MSX | 8 | 425.628 | 222.767 | 47.7% |
| VRC7 | 1 | 53.421 | 27.346 | 48.8% |
| VRC7 | 8 | 438.142 | 249.799 | 43.0% |
| OPL2 | 1 | 58.311 | 28.949 | 50.4% |
| OPL2 | 8 | 548.813 | 298.251 | 45.7% |
| OPL3 four | 1 | 95.699 | 41.383 | 56.8% |
| OPL3 four | 8 | 811.909 | 381.672 | 53.0% |
| OPL3 dual | 1 | 92.615 | 41.043 | 55.7% |
| OPL3 dual | 8 | 826.833 | 392.825 | 52.5% |
| Genesis FM | 1 | 78.219 | 55.052 | 29.6% |
| Genesis FM | 8 | 626.192 | 386.130 | 38.3% |
| Arcade FM | 1 | 77.899 | 46.497 | 40.3% |
| Arcade FM | 8 | 611.554 | 355.500 | 41.9% |

DX7's measured variation is an unchanged-code control, not a claimed change
in its performance. All six complete benchmark runs produced the same sampled
checksum, `-93.306319633`. That checksum samples the output; the separate
PCM regressions compare every generated sample in their fixtures.

Reproduce from `tracker`:

```sh
make -f Makefile.test -j4 benchmark-native-chips BUILD_DIR=build/fm-release \
  CFLAGS='-std=c++17 -O3 -DNDEBUG -DTEST -DTEST_PORTMASTER_INPUT'
build/fm-release/benchmark_native_chips 10 --bounded
```

The optimized focused suite passed five cases and 86,274 assertions, including
CRD playback for every family. The web target rebuilt with the existing SDK;
JavaScript syntax and WebAssembly validation passed, and all 760 preload files
matched their source inputs byte for byte. The generated data bundle differs
from the previous bundle only through CRLF-to-LF conversion in 94 text assets;
no preload file was added or removed. Browser interaction and handheld playback
have not been tested for this audit.

The required full debug run completed with 798 of 799 cases passing and
119,882,353 of 119,882,354 assertions passing; neither cases nor assertions
were skipped. Both separate allocation/lifetime checks passed. The one failure
is the unchanged M8 importer test at `tracker/tests/test_import_m8s.cpp:214`:
it requires lowercase `samples/Pack/Kick_4.wav`, while this case-insensitive
macOS filesystem resolves and loads `m8s_t/lib/Samples/Pack/Kick_4.wav`.
The diagnostic confirmed that the WAV loads with the expected two frames;
only the path spelling assertion fails. The preceding AdLib-only clean-main
run reproduced the same failure. No M8 importer or sample code was changed.
