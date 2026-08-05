# Development notes

## Braids integration

- Braids exposes 47 production models, indexed from 0 to 46. `QUESTION_MARK` is not part of the accessible set.
- The original DSP and lookup tables assume a 96 kHz render rate. Mobile Groove therefore uses 96 kHz until measurements on the RG353V justify a resampling path.
- One `BraidsVoice` belongs to each tracker track. Voices are monophonic, while AY and Braids instruments can coexist in one song.
- Percussive models from `STRUCK_BELL` through `SNARE` keep Braids' internal `STRIKE` decay. Tonal models use the added audio-rate ADSR.
- Braids must bypass AY register output. Treating every non-empty instrument as AY caused invalid register work until the AY path was explicitly restricted to AY1, AY2 and AYSample.
- `ChipNomadState` is allocated with `malloc`, so C++ voice objects cannot safely be embedded in it without changing its construction. The current minimal solution stores pointers and owns their lifetime explicitly.
- Only the required Mutable Instruments Braids and stmlib files are vendored under `chipnomad_lib/external/mutable`; the large reference checkout in `inspirations/` is intentionally ignored by Git.

## Build and validation

- Native Windows development uses MSYS2 UCRT64, GCC and SDL2. Docker is not required.
- The Windows build defaults to 96 kHz. An old generated `settings.txt` can still override that value and must be migrated or removed.
- The PortMaster source package should target `aarch64` first: the RG353V uses a 64-bit RK3566 CPU, and keeping an untested ARMHF build doubles the work without helping this target.
- PortMaster officially supports WSL2/chroot, cross-compilation and Docker. This project uses WSL2/cross-compilation; Docker remains optional.

## Current validation

- All 47 Braids models render finite, non-silent output in the desktop tests.
- Filter modes and slopes, ADSR release, tracker routing and AY muting are covered by tests.
- The Windows executable compiles and survives an SDL dummy audio/video smoke test.
- The remaining hardware question is whether eight simultaneous voices hold 96 kHz without underruns on the RG353V.
