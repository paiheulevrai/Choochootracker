# Signalsmith Stretch (vendored)

Vendored from https://github.com/Signalsmith-Audio/stretch at commit `a670068`
(version 1.4.0, header version {1,3,2}).

Header-only time-stretch library used by the sample instrument "Stretch" option
(musical-division sample stretching). MIT licensed — see `LICENSE.txt`.

Files:
- `signalsmith-stretch.h` — the stretch processor (unmodified).
- `signalsmith-linear/` — dependency headers from
  https://github.com/Signalsmith-Audio/linear at commit `de55e6a` (v0.6.4):
  `fft.h`, `stft.h`, `linear.h`, `approx.h` plus the `platform/` backend
  shims. Unmodified.

Include path: `-I.../chipnomad_lib/external/signalsmith`
Usage: `#include "signalsmith-stretch.h"`
