# Violin (SID Instrument)

A SID chip approximation of a bowed violin, optimized via the v5
tracker-style pipeline with multi-note chromatic evaluation across
G3--G5 (9 pitches).

**Source**: University of Iowa Musical Instrument Samples (public domain).
Anechoic chamber recordings, 24-bit/44.1kHz stereo, by Lawrence Fritts.

## Chip Variants

| | 6581 | 8580 |
|---|---|---|
| **Status** | available | available |
| **Fitness** | 0.9138 | -- |
| **Version** | 1 | 1 |

Both variants use a saw-based sustain waveform with a pulse+saw attack
transient and test-bit oscillator reset. The 6581 variant uses a longer
attack phase (5 frames) and steeper PW sweep; the 8580 variant uses a
slower attack (A=8) with high sustain level (S=15) and multi-step
wavetable sequencing.

### 6581 Parameters

| Parameter | Value |
|---|---|
| waveform | saw |
| attack | 1 |
| decay | 6 |
| sustain | 4 |
| release | 6 |
| pulse_width | 3359 |
| pw_start | 3359 |
| pw_delta | -43 |
| pw_mode | sweep |
| filter_cutoff | 44 |
| filter_cutoff_start | 44 |
| filter_cutoff_end | 44 |
| filter_sweep_frames | 50 |
| filter_resonance | 8 |
| filter_mode | off |
| filter_voice1 | False |
| wt_attack_waveform | pulse+saw |
| wt_sustain_waveform | saw |
| wt_attack_frames | 5 |
| wt_use_test_bit | True |
| volume | 15 |

### 8580 Parameters

| Parameter | Value |
|---|---|
| waveform | saw |
| attack | 8 |
| decay | 4 |
| sustain | 15 |
| release | 1 |
| pulse_width | 1185 |
| pw_start | 1185 |
| pw_delta | 21 |
| pw_mode | sweep |
| filter_cutoff | 44 |
| filter_cutoff_start | 44 |
| filter_cutoff_end | 44 |
| filter_sweep_frames | 50 |
| filter_resonance | 8 |
| filter_mode | off |
| filter_voice1 | False |
| wt_attack_waveform | pulse+saw |
| wt_sustain_waveform | saw |
| wt_attack_frames | 4 |
| wt_use_test_bit | True |
| volume | 15 |

## Tags

`violin`, `strings`, `bowed`

## Files

Each chip subdirectory contains:

| File | Description |
|---|---|
| `violin-<chip>-params.json` | Machine-readable SID parameters |
| `violin-<chip>.asm` | ACME-includable assembly tables |
| `violin-<chip>.ins` | GoatTracker 2.x instrument binary |
| `violin-<chip>-scale.wav` | SID patch rendered at each reference note |

Top-level files:

| File | Description |
|---|---|
| `violin-reference-scale.wav` | Concatenated reference samples for comparison |
