# DSP optimization notes — September 1, 2026

## Outcome

The Windows and PortMaster master mix now runs at 48 kHz. On the target console, `alf dance` measures about 8% CPU with a 10% peak and `psy` about 21%. These figures leave much more deadline headroom than the earlier greater-than-50% peaks that produced crackles and UI slowdown.

The desktop song benchmark is useful for comparisons, not as a prediction of ARM console load. It renders the same project repeatedly, reports median wall time and can disable effects, Tilt or nonlinear filter drive to isolate costs.

```sh
cd tracker
make -f Makefile.test -j4 BUILD_DIR=build/benchmark \
  'CFLAGS=-std=c++17 -Wall -O3 -DNDEBUG -DTEST' benchmark-song
build/benchmark/benchmark_song "packaging/common/projects/alf dance.cct" 48000 full
build/benchmark/benchmark_song "packaging/common/projects/psy.cct" 48000 full
```

Always finish with the in-app CPU meter on the actual console. Desktop x86 and ARM64 have different math-library, cache and SIMD costs.

## Sample-rate strategy

48 kHz nearly halves the number of master-mix, filter, Tilt, send and effect samples processed compared with 96 kHz. It also halves the output bandwidth: Nyquist moves from 48 kHz to 24 kHz. That loses ultrasonic bandwidth but should not remove useful audible content on this target.

The engines are not forced blindly into one native rate:

- Braids keeps its 96 kHz oscillator and lookup-table domain. A stateful 31-tap half-band FIR decimates its output to 48 kHz, preventing a raw every-other-sample drop and reducing alias foldback.
- Plaits and Plaits-Alt already produce a 48 kHz stream. Models with an intentionally oversampled inner loop retain the upstream conversion inside the engine.
- PCM Sample, 2xSCWF, BYOWTBL, aChChid, AY, filters, envelopes, Tilt and the master buses receive the actual 48 kHz host rate.
- Clouds Reverb keeps its original topology, with delay lengths scaled from 32 to 48 kHz and LFO increments derived from the host rate. The synchronized delay already derives its length from seconds/ticks and sample rate.

Pitch and tracker timing are independent of this change. The main risks are a different extreme-high-frequency/aliasing character and rate-dependent constants hidden in imported DSP. New engines must therefore declare their native rate and either accept the host rate, rescale time constants, or provide explicit sample-rate conversion.

## Hot-path changes

- Delay and reverb low-pass coefficients are computed once per callback instead of once per sample.
- Delay ring indices use increment-and-wrap instead of integer modulo in the sample loop.
- Tilt stops updating its smoothing multiplications once it reaches the target.
- Saturating filter/Tilt drive uses a 1025-entry interpolated `tanh` table. Its measured maximum error against `tanhf` over `[-8, 8]` is below `0.00003`; values outside that range saturate to ±1. The musical curve therefore remains effectively the same.

The Schraudolph IEEE-754 approximation for `expf` was not adopted. The expensive filter exponentials were better removed from the per-sample loop entirely, and the bit approximation adds range, portability and precision risks where coefficient accuracy affects cutoff and time constants. SIMD was also deferred: several dominant paths are recursive/stateful, while the 48 kHz change and loop-invariant hoisting delivered broader gains with less maintenance risk.

## Profiling lessons

- Benchmark complete songs first; synthetic oscillator tests miss sends, filters, modulation and inactive-voice behavior.
- Compare one change at a time and use medians after a warm-up.
- An SLFO is tick-rate work and is normally cheap. FLFO/audio-rate modulation can be costly because it forces voice parameter updates per sample.
- Neutral Tilt is cheap; driven Tilt and nonlinear filter characters cost more because they add saturation. The lookup removes most of the transcendental-function penalty, but the filter stages still run.
- Shared Reverb is processed once per callback, not once per track. Its cost scales primarily with sample rate and active return, while source-engine cost scales with active voices/models.
- Keep quality changes audible and testable. Approximation error, pitch preservation, finite output and sample-rate conversion each have regression coverage.

## Related correctness fixes

`SLE` now reaches all six BYOWTBL engine FX: detune, mix, table A index, table B index, cutoff and resonance. The shared SCWF destinations are slewed as well.

Autosave is storage, not project identity. Restoring `autosave.cct` preserves the last loaded/saved project name, so an autosaved `psy` reopens as `psy` and a manual Save targets `psy.cct`. Project identity is persisted immediately after Load, Save and New.
