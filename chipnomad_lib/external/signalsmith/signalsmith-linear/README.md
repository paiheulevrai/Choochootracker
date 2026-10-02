# Signalsmith Linear (vendored)

Vendored from https://github.com/Signalsmith-Audio/linear at commit `de55e6a`
(version 0.6.4).

FFT/STFT support headers required by `signalsmith-stretch.h`. MIT licensed —
see `LICENSE.txt`.

Files (unmodified):
- `fft.h`, `stft.h`, `linear.h`, `approx.h`
- `platform/` — optional FFT backend shims (pffft, Accelerate, IPP, CMSIS-DSP,
  etc.). None are enabled by default; the default `SimpleFFT` auto-vectorizes.
