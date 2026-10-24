# c64SIDkit

SID chip sound effects toolkit for Commodore 64 game development.

![demo](demo/demo.gif)

## Features

- Python SID emulation with multiple backends: `sid_emulator.py`, `resid_emulator.py`, `vice_emulator.py`
- JSON-based SFX patch format with dataclass/schema validation in `schema.py`
- Built-in SFX preset library in `presets.py`
- WAV preview/export pipeline (`wav_export.py`)
- 6502 ASM export for C64 integration (`asm_export.py`)
- Command-line tool (`sid_sfx/cli.py`)
- Pygame SFX Tweaker GUI (`tools/sfx_tweaker.py`)
- Native C64 audition tool (`tools/sfx_audition.py` + `tools/sfx_audition.asm`)

## Installation

```bash
pip install .
```

Optional GUI dependencies (Pygame tools):

```bash
pip install '.[tools]'
```

## Quick Start

Render a built-in preset to WAV:

```bash
sid-sfx play fire
```

Render a patch JSON to WAV with a specific backend:

```bash
sid-sfx preview patches/powerup_speed.json --emulator resid --chip 8580 -o out/powerup_speed.wav
```

Export one or more patch JSON files to 6502 ASM:

```bash
sid-sfx export patches/xwing_blaster_filtered.json patches/tie_cannon_filtered.json -o out/sfx_data.asm
```

Export in separate table format:

```bash
sid-sfx export patches/xwing_blaster_filtered.json -f tables -o out/sfx_tables.asm
```

## SFX Tweaker

`tools/sfx_tweaker.py` is a real-time patch editor for rapid sound design iteration. It loads presets, lets you tweak waveform/envelope/frequency/sweep/filter/vibrato/loop parameters with sliders, and re-renders audio live via the SID renderer.

Key controls:
- `P`: play once
- `Space`: toggle auto-repeat
- `S`: print/save current patch (`patches/<name>.json`)
- `L`/`Right` and `Left`: cycle presets
- `R`: randomize parameters
- `N`: create a new preset

Run:

```bash
python tools/sfx_tweaker.py
```

Or start on a specific preset:

```bash
python tools/sfx_tweaker.py fire
```

## JSON Patch Format

Each patch is a JSON object mapping directly to SID-oriented parameters (`SfxPatch` in `schema.py`). Core fields include:

- Identity/voice: `name`, `voice`
- Oscillator: `waveform`, `freq_hi`, `freq_lo`, `pw_hi`
- Envelope: `attack`, `decay`, `sustain`, `release`
- Timing: `duration_frames`
- Optional modulation: `sweep_target_hi`, `sweep_target_lo`, `sweep_frames`, `sweep_type`, `vibrato_rate`, `vibrato_depth`
- Optional preview filter: `filter_mode`, `filter_cutoff`, `filter_resonance`
- Optional sustain behavior: `loop`, `loop_preview_seconds`

Example:

```json
{
  "name": "fire",
  "voice": 3,
  "waveform": "TRIANGLE",
  "freq_hi": 143,
  "freq_lo": 92,
  "attack": 8,
  "decay": 3,
  "sustain": 8,
  "release": 4,
  "pw_hi": 57,
  "duration_frames": 15,
  "sweep_target_hi": 14,
  "sweep_target_lo": 33,
  "sweep_frames": 13,
  "sweep_type": "exponential",
  "filter_mode": "lowpass",
  "filter_cutoff": 47,
  "filter_resonance": 5,
  "vibrato_rate": 0.0,
  "vibrato_depth": 0,
  "loop": false,
  "loop_preview_seconds": 5.0,
  "description": "Triangle sweep laser pew"
}
```

## Presets

Built-in presets (`sid-sfx play --list`):

- `fire`
- `explode`
- `hit`
- `weakpoint_hit`
- `enemy_hit`
- `march`
- `kraft_alarm`
- `game_over`
- `death`
- `warp`
- `portal_ping`
- `powerup`
- `bounce`
- `blaster_bolt`
- `xwing_blaster`
- `heavy_repeater`
- `turbolaser`
- `tie_cannon`
- `shield_on_v3`
- `ion_cannon`
- `powerup_spread`
- `powerup_shield`
- `combo`
- `powerup_speed`

## License

MIT
