# Development notes

## Braids integration

- Braids exposes 47 production models, indexed from 0 to 46. `QUESTION_MARK` is not part of the accessible set.
- The original DSP and lookup tables assume a 96 kHz render rate. ChooChooTracker therefore uses 96 kHz until measurements on the RG353V justify a resampling path.
- One `BraidsVoice` belongs to each tracker track. Voices are monophonic, while AY and Braids instruments can coexist in one song.
- Percussive models from `STRUCK_BELL` through `SNARE` keep Braids' internal `STRIKE` decay. Tonal models use the added audio-rate ADSR.
- Braids must bypass AY register output. Treating every non-empty instrument as AY caused invalid register work until the AY path was explicitly restricted to AY1, AY2 and AYSample.
- `ChipNomadState` is allocated with `malloc`, so C++ voice objects cannot safely be embedded in it without changing its construction. The current minimal solution stores pointers and owns their lifetime explicitly.
- Only the required Mutable Instruments Braids and stmlib files are vendored under `chipnomad_lib/external/mutable`; the large reference checkout in `inspirations/` is intentionally ignored by Git.
- App-wide `BITS`, `DRFT`, and `SIGN` reuse the original Braids algorithms. `SIGN` receives an installation seed generated on first launch and persisted in `settings.txt`; copying that file intentionally copies the voice's character.

## Build and validation

- Native Windows development uses MSYS2 UCRT64, GCC and SDL2. Docker is not required.
- The Windows build defaults to 96 kHz. An old generated `settings.txt` can still override that value and must be migrated or removed.
- The PortMaster source package should target `aarch64` first: the RG353V uses a 64-bit RK3566 CPU, and keeping an untested ARMHF build doubles the work without helping this target.
- PortMaster officially supports WSL2/chroot, cross-compilation and Docker. This project uses WSL2/cross-compilation; Docker remains optional.

## Instrument FX

- The phrase and table formats keep their existing FX lanes; Braids, Plaits and Sample do not introduce another automation system.
- The FX selector shows universal FX plus the group matching the active instrument. Simple FX scrolling applies the same filtering.
- Braids FX are `BMD` (model), `BTM` (timbre), `BCL` (color), `BCF` (cutoff) and `BRS` (resonance).
- Sample FX are `SPT` (pitch), `SST` (start), `SEN` (end), `SVL` (volume), `SCF` (cutoff) and `SRS` (resonance).
- Instrument FX use absolute values. They remain active until the next note trigger; that trigger restores instrument values before applying its own FX.
- `M1A`…`M4A` and `M11`…`M44` are the existing relative tracker FX for modulation Amount and P1…P4. Their signed offsets accumulate in the FX state and are reapplied after per-frame offset reset.
- Cutoff FX map `00` to 20 Hz and `FF` to 20 kHz on an exponential curve.
- Generic modulation destinations can control Reverb/Delay sends or another modulation slot's four parameters. Cross-modulation reads the source's previous tick and ignores self-targeting, avoiding recursive evaluation.

## PCM Sample instrument

- `Sample` is separate from the legacy `AYSample` instrument and bypasses the AY DAC.
- It loads PCM8 or PCM16 mono/stereo WAV files into RAM, converts PCM8 to signed PCM16, and renders interpolated stereo audio directly into the floating-point mixer.
- Instrument controls are pitch, start, end, common instrument volume, ADSR and an LP/BP/HP filter with 12/24 dB slopes, cutoff and resonance.
- Playback is one-shot. Looping and streaming are intentionally not implemented yet.
- Project save/load preserves the sample path and controls. Paths are not portable yet: copying WAV files into a project `samples/` directory and storing relative paths remains required.
- The file browser previews the selected WAV with Play. Edit + Left/Right on the Sample screen selects the previous or next WAV in the same folder.

## Plaits and master sends

- Plaits runs its original voice at 48 kHz and is linearly resampled into the 96 kHz master mix. Each tracker track owns one monophonic `PlaitsVoice` and a 16 KiB work allocator.
- The 24 engine indices and Main/Aux blend are stored in the instrument. Plaits-specific FX are filtered by instrument type in the existing FX lanes.
- Plaits has two envelope routings. `TRIG` matches TRIG patched/LEVEL unpatched and reuses the envelope row's D/S storage as LPG decay/color. `VCA` holds LEVEL open and applies the ADSR after the Mutable voice. Each note produces a one-block trigger pulse so adjacent tracker notes retrigger the Mutable envelope. VCA retriggers attack from the current level instead of resetting to zero, avoiding an amplitude discontinuity. Legacy `LEVEL` values load as `VCA`.
- The Plaits wrapper passes pitch once to the Mutable voice; the previous integration also repeated it through the modulation input. A one-octave frequency-ratio test guards this path.
- Reverb and Delay are shared master effects. Tracks accumulate post-fader send buses; each effect is processed once per callback, not once per track.
- The Clouds reverb delay lengths and memory were scaled for 96 kHz. The ping-pong delay derives its sample delay from project tick rate and delay ticks.
- `PRO` uses `00-64` as 0-100%, `MOD AB` uses a per-track phrase visit count, and conditions on one row are ANDed. `SPD` uses a rational phase accumulator and persists until replaced.
- The scheduler advances at most one row per audio tick. High `SPD` multipliers therefore need hardware and musical validation with short grooves.

## Shared instrument controls

- Every instrument has one `00-FF` volume before the track fader. The old Sample-only volume field is accepted while loading older projects but is no longer saved separately.
- Braids, Plaits and Sample use one `MultimodeFilter` implementation. Cutoff is limited to 20 Hz through 20 kHz and edited on an exponential curve.
- Braids and Plaits model selection and modulation destinations use the same two-level popup. Catalog validation ensures every Braids model and Plaits engine appears exactly once.
- New projects default to period pitch (`Linear pitch: Off`), the mode validated on hardware for correct AY, Braids and Plaits octave tracking.
- PSG and VGM export paths were removed. WAV mix and stem export remain.

## PortMaster build

The ARM64 build uses Ubuntu 20.04 under WSL2. Install `make`, `g++-aarch64-linux-gnu`, `pkg-config`, `libsdl2-dev:arm64`, `zip`, `unzip` and `file`.

Ubuntu multiarch needs separate package mirrors. Keep the x86 repositories on `archive.ubuntu.com` and `security.ubuntu.com` with `[arch=amd64]`. Add the Focal ARM64 repositories from `http://ports.ubuntu.com/ubuntu-ports` with `[arch=arm64]`. Without those qualifiers, APT requests ARM64 indexes from the x86 mirror and returns 404 errors.

From the `tracker` directory inside WSL:

```sh
make -f Makefile.portmaster PortMaster
make -f Makefile.portmaster PortMaster-deploy
```

The first command creates `tracker/build/portmaster/choochootracker.aarch64`. The second creates `releases/choochootracker.zip` with the current PortMaster directory layout.

The release build uses LTO. It takes about two minutes when the repository lives under `/mnt/c`, but produces a stripped binary of roughly 415 KiB. The current binary requires glibc 2.27 and links dynamically to SDL2, libstdc++, libgcc, libm and libc. SDL2 is not bundled because PortMaster and the target firmware provide it.

Useful checks:

```sh
file tracker/build/portmaster/choochootracker.aarch64
aarch64-linux-gnu-readelf -d tracker/build/portmaster/choochootracker.aarch64
unzip -t releases/choochootracker.zip
```

References: [PortMaster build environments](https://portmaster.games/build-environments.html) and [PortMaster packaging guide](https://portmaster.games/packaging.html).

## Current validation

- All 47 Braids models render finite, non-silent output in the desktop tests.
- Filter modes and slopes, ADSR release, tracker routing and AY muting are covered by tests.
- Instrument FX lifetime and Sample playback boundaries/filter stability are covered by tests.
- The current suite contains 161 passing test cases and 149,328 assertions.
- The Windows executable compiles and survives an SDL dummy audio/video smoke test.
- The remaining hardware question is whether eight simultaneous Plaits voices plus Reverb and Delay hold 96 kHz without underruns on the RG353V.
- The ARM64 binary and PortMaster ZIP build successfully under WSL2. They still need to run on the RG353V.
