# ChooChooTracker

> **NOT EVEN ALPHA. Testing is not finished. CHOO CHOO.**

ChooChooTracker is a music tracker built for handheld consoles. Write a beat on the train, turn one row into a strange little synth line, send it through reverb, then keep going until you miss your stop.

It combines the fast, button-driven workflow of LSDJ-style trackers with a much wider set of sounds: AY/YM chip synthesis, Mutable Instruments Braids and Plaits, clean PCM samples, per-track FX, and shared Reverb and Delay sends. All eight tracks can use different instruments in the same song.

The main target is the Anbernic RG353V through PortMaster and ArkOS. A native Windows build is available for development and desktop testing.

## What it can do

- Eight fixed monophonic tracks with independent instruments
- AY Classic, AY Plus, and crunchy AY Sample playback
- All 47 accessible Braids models
- All 24 Plaits engines with Main/Aux blend
- Clean mono or stereo PCM8/PCM16 sample playback
- Per-track volume, mute, solo, Reverb send, and Delay send
- Mutable Instruments Clouds Reverb
- Tick-synchronized filtered ping-pong delay
- Three FX columns per row, filtered to match the active instrument
- Probability, modulo conditions, glide, and per-track speed changes
- ADSR, AHD, LFO, filters, modulation, tables, grooves, chains, and songs
- `.cct` project save/load and WAV export
- A CPU meter for keeping ambitious patches honest

## One tracker, very different voices

Each instrument owns its sound engine. A song can put a sharp AY bass on one track, a Braids drum model on another, a Plaits chord engine on the next, and a stereo sample beside them. There is no project-wide chip selection and no forced group of three AY channels.

The tracker stays familiar while the FX list follows the selected instrument. Braids rows can change model, timbre, color, cutoff, or resonance. Plaits rows can reach its engine parameters and output blend. Sample rows can change pitch, playback points, volume, and filter settings. Universal FX remain available everywhere.

`PRO`, `MOD`, and `SPD` make patterns move without turning the tracker into a DAW. Give a hit a percentage chance, trigger it on a specific loop iteration, or let one track run at a different clock ratio.

## Handheld workflow

The main screens follow the `MSCPIT` layout:

```text
Mixer -> Song -> Chain -> Phrase -> Instrument -> Table
```

The Mixer sits directly beside Song so mute, solo, levels, and sends are never buried in a settings menu. Reverb and Delay settings are one step away from the Mixer. Key repeat and controller handling are implemented inside the app for reliable input on handheld hardware.

ChooChooTracker is intentionally small-screen software. It is meant to be played with buttons, not operated like a desktop DAW through a tiny display.

## Try the pre-alpha

Download the PortMaster package and PDF manual from the [GitHub Releases page](https://github.com/paiheulevrai/Choochootracker/releases).

The current package targets ARM64 PortMaster devices and has been developed primarily for ArkOS on RG353V. Install `choochootracker.zip` through PortMaster, or extract it into the console's `ports` directory.

This is an early test build. Save often and keep copies of projects you care about.

## Documentation

- [User manual](docs/USER_MANUAL.md)
- [User manual PDF](docs/ChooChooTracker-User-Manual.pdf)
- [Development overview and roadmap](docs/dev_readme.md)
- [Development notes](docs/development-notes.md)
- [Feasibility study](docs/feasibility-2026-08-09.md)
- [Fork maintenance guide](docs/fork-maintenance.md)

## Current limits

- Sample paths are not portable between systems yet.
- Eight Plaits voices with both master sends still need a full RG353V performance pass.
- The Windows build is not packaged for end users yet.
- Visual identity and the final title screen come after functional stabilization.

The automated suite currently passes 152 test cases and 134,843 assertions. Hardware testing still matters because a desktop test cannot catch controller feel, audio underruns, or the behavior of a specific firmware image.

## Why the train name?

The first proof of concept was written during a train ride between Cahors and Montauban. The name stuck. Choo choo.

## Credits and license

ChooChooTracker is a fork of [ChipNomad](https://github.com/Megus/chipnomad-tracker). Its Braids, Plaits, Clouds, and stmlib code comes from Mutable Instruments' open-source releases. See the included license files for exact attribution.

The project is released under the [MIT License](LICENSE).
