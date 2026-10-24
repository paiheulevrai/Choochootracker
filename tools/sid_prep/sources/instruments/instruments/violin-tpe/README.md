# Violin -- TPE Variant (SID Instrument)

TPE-optimized (Optuna Tree-structured Parzen Estimator) variant of the
violin instrument, included for benchmarking comparison against the
CMA-ES default in `instruments/violin/`.

## Chip Variants

| | 6581 | 8580 |
|---|---|---|
| **Status** | available | available |
| **Fitness** | 0.0000 | 0.0000 |
| **Version** | 1 | 1 |

### 6581 Parameters

| Parameter | Value |
|---|---|
| waveform | saw |
| attack | 12 |
| decay | 9 |
| sustain | 0 |
| release | 5 |
| pulse_width | 2048 |
| pw_start | 2048 |
| pw_delta | 0 |
| pw_mode | sweep |
| filter_cutoff | 44 |
| filter_cutoff_start | 44 |
| filter_cutoff_end | 44 |
| filter_sweep_frames | 50 |
| filter_resonance | 8 |
| filter_mode | off |
| filter_voice1 | False |
| wt_attack_waveform | saw+triangle |
| wt_sustain_waveform | saw |
| wt_attack_frames | 2 |
| wt_use_test_bit | True |
| volume | 15 |

### 8580 Parameters

| Parameter | Value |
|---|---|
| waveform | saw |
| attack | 10 |
| decay | 9 |
| sustain | 8 |
| release | 2 |
| pulse_width | 1707 |
| pw_start | 1707 |
| pw_delta | -50 |
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
| wt_attack_frames | 2 |
| wt_use_test_bit | False |
| volume | 15 |

## Tags

`violin-tpe`

## Files

Each chip subdirectory contains:

| File | Description |
|---|---|
| `violin-tpe-<chip>-params.json` | Machine-readable SID parameters |
| `violin-tpe-<chip>.asm` | ACME-includable assembly tables |
| `violin-tpe-<chip>.ins` | GoatTracker 2.x instrument binary |
| `violin-tpe-<chip>-scale.wav` | SID patch rendered at each reference note |

Top-level files:

| File | Description |
|---|---|
| `violin-tpe-reference-scale.wav` | Concatenated reference samples for comparison |
