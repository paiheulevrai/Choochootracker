# FORMANT insert

FORMANT is appended as insert module 16; IDs 0–15, project format and the
existing TF1/TF2, phrase FX and MOD routing stay intact. It also works in the
existing master slots, which have no track MOD sources. User controls are
explained in the Track insert effects section of USER_MANUAL.md.

## Source and license audit

Reference: Józef Maćkowiak's [JM Vowel Filter](https://github.com/jozmac/reapack-jm/blob/4f507485554c90dcb6a3670058a8f4812c851518/Effects/JM_fx_Filter_Vowel.jsfx),
MIT, copyright 2023 Józef Maćkowiak. Its license is retained in
`chipnomad_lib/external/insert_fx/LICENSE-JM-Formant` and copied into PortMaster
packages. The inspected repository tree was
`4f507485554c90dcb6a3670058a8f4812c851518`.

JM cascades two peaking EQ stages per stereo channel, maps normalized F1/F2
logarithmically to 200–1200/500–2800 Hz, supports Q 2–16, and smooths/interpolates
parameters. Its REAPER GUI, slider interpolation runtime and JSFX code are not
included. The imported `ThirdParty/Tale/rbj_filter.jsfx-inc` declares copyright
2012–2021 Theo Niessink and WTFPL version 2. No Tale source was reused. The native
implementation writes the standard Robert Bristow-Johnson peaking EQ equations
directly, with independent double-precision histories for each stage/channel.

FORMANT fixes each peak's gain at +18 dB, compensates the wet path by -18 dB and
soft-limits wet excursions above 0.9 toward 1.2. These sound-design choices differ
from JM's defaults. They keep coincident formants bounded without
adding another user parameter. A–E–I–O–U positions are 00/20/40/60/7F; exact
anchors use the requested calibration pairs. CUSTOM retains two full bytes.

## DSP and state

No dynamic allocation, locks or UI calls occur in FormantFilter::process.
Frequencies are clamped to 20 Hz–0.45 × sample rate. Frequency/Q targets are
smoothed in logarithmic space with an 8 ms time constant; normalized stable
biquad coefficients are calculated every 32 samples and interpolated across
that interval. Settled controls skip coefficient calculation. MIX has 5 ms
smoothing. The existing insert bypass/module crossfade remains in charge of
slot activation. Mode changes update targets without clearing filter history.

Inactive parameter values, phrase locks and MOD assignments remain stored;
only modulation application is gated. Effective offsets are recomputed from
current sources every engine update, so an old offset is not latched when a
mode returns. MODE itself is discrete and phrase-automatable, but deliberately
not a MOD destination. Existing MOD sources and destination IDs are reused.

## Reproduce local checks

From `tracker`:

```sh
make -f Makefile.test -j4
make -f ../tools/formant/host.mk -j4 formant-trial
mkdir -p ../.tmp/formant/trial
build/tests/formant-trial ../.tmp/formant/trial
```

The regular suite discovers `tests/test_formant.cpp`. The real-engine trial
creates 12 disposable projects with aChChid saw sources (1/4/8 tracks, 1/2
slots, VOW/CUSTOM), fast independent tick LFOs (periods 4/5/7 ticks) and maximum starting resonance.
It destroys each original engine, reopens the saved project, and verifies
non-silent finite output, live modulation and inactive control behavior.
It uses the installed R36H configuration (48 kHz, 512-frame callbacks) and
reports total render time and worst callback time; debug host timings are
not R36H performance measurements. It also creates a stereo WAV with five
one-second vowel anchors followed by a five-second sweep.

## Listening feedback and validation scope

Vowel calibration and output level still need listening feedback. On R36H,
the vowel sweep could be slower even with the ordinary LFO period at its
maximum of 255 ticks. This is feedback about the available modulation range;
this contribution does not extend that range or change the modulation scheduler.

The trial harness uses tick LFOs and reports host render timings only; those
timings do not establish hardware performance. This contribution preserves the
tested DSP and parameter behavior and does not include personal-build integration
or device installation scripts.
