> **NOT EVEN ALPHA. Testing is not finished. CHOO CHOO.**

```*        .         *               .            *
   ______ __ __  ____   ____   ______ __ __  ____   ____
  / ____// // / / __ \ / __ \ / ____// // / / __ \ / __ \
 / /___ /  __  // /_/ // /_/ // /___ /  __  // /_/ // /_/ /
 \____//_/ /_/ \____/ \____/ \____//_/ /_/ \____/ \____/

             ______  ___    ___    ______  __  __  ______  ____
            /_  __/ / _ \  / _ |  / ____/ / /_/ / / ____/ / _  \
             / /   / , _/ / __ | / /___  /  _  / / _/   / , _/
            /_/   /_/|_| /_/ |_| \____/ /_/ |_| /____/ /_/|_|

       -=[ HANDHELD CONSOLE MULTI-ENGINE TRACKER ]=-
             _________________________________
       _____/  ___                ___         \_____
   ___/_____|_|___|______________|___|_____________\___
  /  _   _   _   _   _   _   _   _   _   _   _         \
 |  |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |_| |RG353| |
 |______________________________________________________|
    O==O       O==O             O==O       O==O
 _.-'--`-.___.-'--`-.___==___.-'--`-.___.-'--`-._
==========================================================>
     CAHORS                                       MONTAUBAN
          >>>  8 TRACKS / 96 kHz / CHIPNOMAD-BASED  >>>

     .oO[ AY / BRAIDS / PLAITS / PCM SAMPLES / FX SENDS ]Oo.
````

ChooChooTracker is a fork of [ChipNomad](https://github.com/Megus/chipnomad-tracker), with 70+ extra synth engines, high quality PCM samples playback, global reverb/delay, and some other small changes. It keeps ChipNomad's supafast LSDJ-inspired tracker workflow but departs from the chiptune-only vision of Megus to offer a metric ton of modern sound design options. The name comes from the first proof of concept, written on a train between Cahors and Montauban.

ChooChooTracker is a music tracker for handheld consoles.
Write a beat on the train, automate a weird little synth line, send it through reverb, then keep going until you miss your stop.

The main target is the Anbernic RG353V through PortMaster and ArkOS. 
A native Windows build is available for development and desktop testing.
You can also test it in your browser on https://choochootracker.vercel.app/ (use the keyboard on a computer, use the on-screen gamepad on a mobile device)
It should also work on any Portmaster capable system, and should also compile on Android though I haven't tested that yet.

An additional browser build is available under [`web/`](web/). It compiles the
same tracker to WebAssembly with SDL2; see [`web/README.md`](web/README.md) for
the Emscripten and Vercel setup. This target is independent from Windows and
PortMaster.

## What it can do

- Eight fixed monophonic tracks (with independent instruments)
- AY Classic, AY Plus, and crunchy AY Sample playback (from Chipnomad)
- All 47 Braids engines, 24 stock Plaits engines, and 24 non-duplicated Plaits-Alt engines
- Clean mono or stereo PCM8/PCM16 sample playback (one-shot samples, like your Digitakt)
- Multimode LP/HP/BP 12/24dB filter for all new synth/sample engines
- Per-track volume, mute, solo, Reverb send, and Delay send
- Mutable Instruments Clouds meme lush reverb
- Tick-synchronized filtered ping-pong delay
- Three tracker FX columns per row
- Added tracker FX inspired by Elektron and Nerdseq: Probability, modulo conditions, and per-track invididual playback speed
- Synth engines parameters can be set by FX, kinda like P-locks.
- Modulation sources per track: ADSR, AHD, 2x LFO. Modulations can modulate modulations. 
- Tracker tables, grooves, chains, and songs

## One tracker, very different voices

Instruments in LSDJ/Chipnomad work like "Machines" in the Elektron world.

Each instrument has its sound engine and can be mixed/matched at will: you can have an AY bass on one track, a Braids drum model on another, a Plaits chord engine or a Plaits-Alt texture on the next, and some repitched heehaa samples beside them.

There is no project-wide chip selection and no forced group of three AY channels. And of course you can use several instruments in a track so you can do "single track challenges".


## Handheld workflow

The main screens follow the `MSCPIT` layout:

```text
Mixer - Song - Chain - Phrase - Instrument - Table
```
Just like LSDJ, but with a Mixer on the left.

## Try the pre-alpha

Download the PortMaster package and PDF manual from the [GitHub Releases page](https://github.com/paiheulevrai/Choochootracker/releases).

The current package targets ARM64 PortMaster devices and has been developed primarily for ArkOS on RG353V. Install `choochootracker.zip` through PortMaster, or extract it into the console's `ports` directory.

This is an early test build. Save often and don't get too attached to your projects.

## Documentation

- [User manual](docs/USER_MANUAL.md)
- [User manual PDF](docs/ChooChooTracker-User-Manual.pdf)
- [Development overview and roadmap](docs/dev_readme.md)
- [Development notes](docs/development-notes.md)
- [Feasibility study](docs/feasibility-2026-08-09.md)
- [Fork maintenance guide](docs/fork-maintenance.md)

## Current limits

- Braids, Samples, track FX, mixer and sends: working or it seems.
- Plaits isn't fully tested.
- Visual identity will be finalized later, after functional stabilization.

## Why the train name?

The first proof of concept was written during a train ride between Cahors and Montauban. The name stuck. Choo choo. Don't miss your stop.

## Credits and license

ChooChooTracker is a fork of [ChipNomad](https://github.com/Megus/chipnomad-tracker). Its Braids, Plaits, Clouds, and stmlib code comes from Mutable Instruments' open-source releases. See the included license files for exact attribution.

The project is released under the [MIT License](LICENSE).

Mad respects to the people I stole code from:
- Megus, the insanely smart creator of Chipnomad.
- Emilie Gillet, the genius behind Mutable Instruments.

Mad respects to the people I stole ideas from:
- Thomas, the absolute beast behind Nedseq
- all the people at Elektron
