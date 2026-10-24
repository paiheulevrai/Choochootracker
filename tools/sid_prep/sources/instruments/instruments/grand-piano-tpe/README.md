# Grand Piano -- TPE Variant (SID Instrument)

TPE-optimized (Optuna Tree-structured Parzen Estimator) variant of the
grand piano instrument, included for benchmarking comparison against the
CMA-ES default in `instruments/grand-piano/`.

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
| attack | 8 |
| decay | 10 |
| sustain | 0 |
| release | 4 |
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
| wt_attack_waveform | None |
| wt_sustain_waveform | saw |
| wt_attack_frames | 1 |
| wt_use_test_bit | False |
| volume | 15 |

### 8580 Parameters

| Parameter | Value |
|---|---|
| waveform | saw |
| attack | 11 |
| decay | 7 |
| sustain | 14 |
| release | 12 |
| pulse_width | 2048 |
| pw_start | 2048 |
| pw_delta | 0 |
| pw_mode | sweep |
| filter_cutoff | 115 |
| filter_cutoff_start | 115 |
| filter_cutoff_end | 44 |
| filter_sweep_frames | 61 |
| filter_resonance | 11 |
| filter_mode | lp |
| filter_voice1 | True |
| wt_attack_waveform | None |
| wt_sustain_waveform | saw |
| wt_attack_frames | 4 |
| wt_use_test_bit | True |
| volume | 15 |

## Tags

`grand-piano-tpe`

## Files

Each chip subdirectory contains:

| File | Description |
|---|---|
| `grand-piano-tpe-<chip>-params.json` | Machine-readable SID parameters |
| `grand-piano-tpe-<chip>.asm` | ACME-includable assembly tables |
| `grand-piano-tpe-<chip>.ins` | GoatTracker 2.x instrument binary |
| `grand-piano-tpe-<chip>-scale.wav` | SID patch rendered at each reference note |

Top-level files:

| File | Description |
|---|---|
| `grand-piano-tpe-reference-scale.wav` | Concatenated reference samples for comparison |
