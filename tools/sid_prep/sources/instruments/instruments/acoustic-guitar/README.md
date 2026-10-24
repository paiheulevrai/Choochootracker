# Acoustic Guitar (SID Instrument)

A SID chip instrument patch: acoustic-guitar.

## Chip Variants

| | 6581 | 8580 |
|---|---|---|
| **Status** | available | available |
| **Fitness** | 0.0000 | 0.0000 |
| **Version** | 5 | 6 |

### 6581 Parameters

| Parameter | Value |
|---|---|
| waveform | saw |
| attack | 0 |
| decay | 3 |
| sustain | 2 |
| release | 14 |
| pulse_width | 3605 |
| pw_start | 3605 |
| pw_delta | -35 |
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
| wt_attack_frames | 3 |
| wt_use_test_bit | False |
| volume | 15 |

### 8580 Parameters

| Parameter | Value |
|---|---|
| waveform | pulse |
| attack | 8 |
| decay | 1 |
| sustain | 7 |
| release | 13 |
| pulse_width | 3481 |
| pw_start | 3481 |
| pw_delta | 43 |
| pw_mode | sweep |
| filter_cutoff | 65 |
| filter_cutoff_start | 65 |
| filter_cutoff_end | 67 |
| filter_sweep_frames | 30 |
| filter_resonance | 9 |
| filter_mode | lp |
| filter_voice1 | True |
| wt_attack_waveform | noise |
| wt_sustain_waveform | pulse |
| wt_attack_frames | 3 |
| wt_use_test_bit | True |
| volume | 15 |

## Tags

`acoustic-guitar`

## Files

Each chip subdirectory contains:

| File | Description |
|---|---|
| `acoustic-guitar-<chip>-params.json` | Machine-readable SID parameters |
| `acoustic-guitar-<chip>.asm` | ACME-includable assembly tables |
| `acoustic-guitar-<chip>.ins` | GoatTracker 2.x instrument binary |
| `acoustic-guitar-<chip>-scale.wav` | SID patch rendered at each reference note |

Top-level files:

| File | Description |
|---|---|
| `acoustic-guitar-reference-scale.wav` | Concatenated reference samples for comparison |
