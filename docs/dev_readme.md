


# ChooChooTracker

Developer readme so human and AI have a common understanding.

ChooChooTracker is a fork of [ChipNomad](https://github.com/Megus/chipnomad-tracker). It keeps ChipNomad's LSDJ-inspired tracker and expands its sound palette with modern synthesis engines. The name comes from the first proof of concept, written on a train between Cahors and Montauban.

> **Project status:** active development. The Windows and PortMaster builds work, but this version is for testing. Other targets from Chipnomad should also work albeit untested.

The main target is the Anbernic RG353V through PortMaster. A native Windows build is kept for development and debugging. Any Portmaster capable system should work.

This is not a DAW. It is a small, self-contained instrument for writing music on the move, with a deliberately playful side. You could call this a portable groovebox.

## Design rules

- Keep ChipNomad's tracker, sequencer, and workflow where they still fit.
- Reuse ChipNomad architecture, functions, and conventions whenever they fit instead of building parallel systems.
- Keep shared code recognizable and practically compatible so fixes and ideas can move in either direction. Diverge when ChooChooTracker's product goals require it, not for style alone.
- Allow AY/YM and modern instruments in the same song.
- Add synthesis features without rewriting working code.
- Keep the interface usable on a small screen with few buttons.
- Prefer a simple, predictable architecture that is easy to port.
- All added engines share a multimode filter if that's not already part of the engine.

## Reference sources

Github-ignored `inspirations/` directory (on my local dev machine) contain:

- `chipnomad-tracker-main/` contains the ChipNomad base.
- `mutable-eurorack/` contains the official Mutable Instruments Braids, Plaits, Clouds, and stmlib sources.
- `mutable-instruments-documentation-main/` contains the Mutable manuals used while writing the ChooChooTracker manual.

Braids is pinned to commit `08460a69a7e1f7a81c5a2abcc7189c9a6b7208d4`. stmlib is pinned to `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec`.

These directories are references only. Working code belongs in the fork so local changes do not become mixed with upstream source trees.

## What comes from ChipNomad

our modified ChipNomad now separates sequencing from audio generation. The UI owns the editable
project while the audio callback renders an execution snapshot. Edits are
published through fixed slots and adopted at the next tick; transport commands
use a fixed queue. `chipnomadRender()` then asks each engine for an audio block
and mixes the results into a floating-point stereo buffer without allocating.

The original base provides:

- AY1, AY2, and AYSample instruments
- four generic modulation slots per instrument
- ADSR, AHD, and LFO modulation
- WAV import and export
- SDL2 targets for Windows, Linux, and PortMaster

Some chiptune features don't need to be kept (for exemple export to chiptune file formats)

ChooChooTracker reuses that modulation system. Modern engine amplitude is calculated or smoothed at audio rate to avoid clicks and stepped values.

## Send effects

Reverb and delay are shared effects. Each track has independent sends, while
the effects are processed once per audio callback.

### Signal path

Tonal models use:

```text
Braids -> filter -> ADSR amplifier -> mixer
```

Percussive models use:

```text
Braids with its original STRIKE behavior -> filter -> mixer
```

Percussive models keep their internal envelope and decay. ChooChooTracker does not add another ADSR on top.

Plaits keeps each engine's internal behavior, then passes through the shared
filter and ChooChooTracker ADSR. Samples use the shared filter and ADSR after
one-shot playback.

### Filter

Each synth engine voice has a digital filter with:

- 12 or 24 dB/octave slope
- low-pass, band-pass, and high-pass modes
- adjustable cutoff
- adjustable resonance

The 12 dB mode uses one state-variable filter. The 24 dB mode cascades two stages.

Future implementations may add emulations of acid (303), classic japanese (Korg), or american (Moog) filters, why not.


## PCM samples

The original `AYSample` instrument remains available for deliberately crunchy sounds. It converts WAV files to unsigned 8-bit mono, limits them to 16,384 samples, and plays them through AY-style 4-bit volume levels.

We added a new Sample instrument, inspired by Piggy tracker and simple trackers such as the Digitakt. It's not mean't to be an Octatrack / MPC / slicer.

The `Sample` instrument provides clean playback:

- external WAV files loaded from the project's `samples/` directory
- 8-bit or 16-bit PCM converted to signed PCM16 in memory
- mono or stereo playback, preserving the original channel layout
- preserved source sample rate
- direct output to the floating-point mixer without the AY path
- one-shot playback with start and end points
- useful transposition over roughly one or two octaves in either direction
- linear interpolation
- multimode filter and envelope

The current implementation loads each WAV into RAM and stores its path in the project. It does not yet copy the file into `samples/` or convert the path to a portable relative path. SD card streaming is out of scope because this engine is intended for drums and short one-shots.

A project still loads when a sample is missing. The affected instrument remains silent and the interface displays a warning.

WAV data is not embedded in `.cct` project files. This keeps projects readable and avoids inflating them with audio data. Good luck with portability ahah.


## Building

### Windows development

Native Windows development uses:

- MSYS2
- MinGW-w64
- SDL2
- the existing Makefiles

This route produces a Windows executable for interface, sequencing, and audio tests. There is no current reason to replace the Makefiles with CMake.

### PortMaster

The RG353V runs a Linux ARM64 binary. PortMaster builds use WSL2 with Ubuntu, an AArch64 toolchain, and ARM64 SDL2 development libraries.

```text
edit on Windows
        |
native Windows build and tests
        |
ARM64 cross-build under WSL2
        |
copy by SSH or SD card
        |
test and benchmark on RG353V
```
Docker is not required for daily work. It can wait until reproducible release builds or shared CI make it useful.

The PortMaster package uses the ChooChooTracker name, an ARM64 binary, and the `.cct` project format. From `tracker`, run:

```sh
make -f Makefile.portmaster PortMaster-deploy
```

This builds and checks the ZIP before ArkOS testing.



## Out of scope

- long audio tracks
- streaming from the SD card
- multitrack recording
- mastering effects
- plugins
- DAW-style automation
- compatibility with microcontrollers, dedicated DSP hardware, or Eurorack modules

ChooChooTracker remains a handheld tracker. It adds richer synthesis and clean sample playback, but it should stay immediate and fun. Title screen and visual identity work can wait until the functional core is stable.
