# Acoustic Guitar -- TPE Variant (SID Instrument)

TPE-optimized (Optuna Tree-structured Parzen Estimator) variant of the
acoustic guitar instrument, included for benchmarking comparison against
the CMA-ES default in `instruments/acoustic-guitar/`.

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
| attack | 2 |
| decay | 2 |
| sustain | 1 |
| release | 12 |
| pulse_width | 3946 |
| pw_start | 3946 |
| pw_delta | 3 |
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
| wt_use_test_bit | False |
| volume | 15 |

### 8580 Parameters

| Parameter | Value |
|---|---|
| waveform | saw |
| attack | 14 |
| decay | 15 |
| sustain | 14 |
| release | 6 |
| pulse_width | 2048 |
| pw_start | 2048 |
| pw_delta | 0 |
| pw_mode | sweep |
| filter_cutoff | 91 |
| filter_cutoff_start | 91 |
| filter_cutoff_end | 10 |
| filter_sweep_frames | 75 |
| filter_resonance | 8 |
| filter_mode | lp |
| filter_voice1 | True |
| wt_attack_waveform | saw+triangle |
| wt_sustain_waveform | saw |
| wt_attack_frames | 4 |
| wt_use_test_bit | True |
| volume | 15 |

## Tags

`acoustic-guitar-tpe`

## Files

Each chip subdirectory contains:

| File | Description |
|---|---|
| `acoustic-guitar-tpe-<chip>-params.json` | Machine-readable SID parameters |
| `acoustic-guitar-tpe-<chip>.asm` | ACME-includable assembly tables |
| `acoustic-guitar-tpe-<chip>.ins` | GoatTracker 2.x instrument binary |
| `acoustic-guitar-tpe-<chip>-scale.wav` | SID patch rendered at each reference note |

Top-level files:

| File | Description |
|---|---|
| `acoustic-guitar-tpe-reference-scale.wav` | Concatenated reference samples for comparison |
