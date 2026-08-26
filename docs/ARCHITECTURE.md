# MobileGroove architecture

## Overview

MobileGroove is a portable C++ tracker. `tracker/` provides the application
and its screens; `chipnomad_lib/` contains the project model, sequencer and
audio renderer. Windows, Web and PortMaster share the same engine.

```
UI / screens ── UI Project ── snapshots + commands ── audio engine
     ▲                                                │
     └──────── playback status + motion events ───────┘
```

## Modules

- `tracker/src/`: application loop, SDL audio, tracker screens, editing,
  import/export and saving.
- `chipnomad_lib/project*`: in-memory format, serialization and utilities.
- `chipnomad_lib/playback*`: song/chain/phrase playback, ticks and FX.
- `chipnomad_lib/synth/`: Braids, Plaits, sample, SCWF and BYOWTBL voices,
  plus master effects.
- `chipnomad_lib/chips/`: AY emulation and chip abstraction.
- `chipnomad_lib/export/`: offline WAV rendering.
- `tracker/platforms/`: SDL/Web/PortMaster adapters and packaging.

## Project and instruments

`Project` is the editable song: song, chains, phrases, tables, instruments,
samples and global settings. The saved format remains the historical one.

`project_instruments.*` is the source of truth for instrument families. For
each family it declares:

- UI name and category, associated screen, init/free callbacks;
- modulation destinations, ranges and ADSR/Trigger capabilities;
- visible FX;
- destination-to-FX mappings for motion recording.

Family-specific data is still accessed through typed code. No union member of
`InstrumentChipData` is addressed through offset-based reflection.

## Real-time boundary

The UI owns `ChipNomadState::project`. The callback only reads
`audioProject` and `PlaybackState`.

1. The UI edits the project and publishes a snapshot through a fixed
   three-slot handoff.
2. At a tick boundary, the engine adopts the latest available snapshot,
   applies coalesced settings, then FIFO transport commands.
3. The callback renders frames between ticks without allocation or locking.

An edit made while playing therefore becomes audible on the next tick.
`Stop` is priority; play, preview and phrase queue commands are FIFO; loop,
mute/solo, sticks and project refresh keep their latest value.

The engine publishes `PlaybackStatus` for UI cursors. Screens do not read the
mutable `PlaybackState`. Motion recording travels in the other direction via
a fixed audio-to-UI queue: audio emits events; UI writes the phrase and marks
the project dirty.

Mix, reverb and delay buffers are reserved before start or during explicit
reconfiguration. An oversized callback is bounded and reported; it never
resizes memory inside the callback.

See [realtime-architecture.md](realtime-architecture.md) for detailed
constraints and diagnostics.

## Render flow

`audio_manager` receives the platform callback, calls `chipnomadRender`,
converts the floating-point buffer to the output format, and performs no
editing. `chipnomadRender` drives ticks; `playback` resolves notes, FX and
modulation; voices and master effects produce the stereo mix.

WAV export uses the same engine offline, where it can initialize a local
playback state.

## Validation and builds

- Engine tests: `cd tracker && make -f Makefile.test -j4`.
- Windows: MSYS2 UCRT64, `make -j4 windows`.
- Web: Emscripten, then `Makefile.web web-deploy`; `web/dist/` is versioned.
- PortMaster: `make -j4 PortMaster`, then package and test on RG353V.

Unit tests cover modulation limits, the instrument catalogue, handoffs and
commands, and voices. Final validation remains auditory and hardware-based:
dense playback, live editing, motion recording, effects and saving.
