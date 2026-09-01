


# ChooChooTracker

Developer readme so human and AI have a common understanding.

ChooChooTracker is a fork of [ChipNomad](https://github.com/Megus/chipnomad-tracker). It keeps ChipNomad's LSDJ-inspired tracker and expands its sound palette with modern synthesis engines. The name comes from the first proof of concept, written on a train between Cahors and Montauban.

> **Project status:** active development. The Windows and PortMaster builds work, but this version is for testing. Other targets from Chipnomad should also work albeit untested.

The main target is the Anbernic RG353V through PortMaster. A native Windows build is kept for development and debugging. Any Portmaster capable system should work.

This is not a DAW. It is a small, self-contained instrument for writing music on the move, with a deliberately playful side. You could call this a portable groovebox.

## Current status

Compiles and runs on my system. Debugging is not completed. Screen layout and ergonomics aren't final.
Braids (47 models), Plaits (24 engines), the PCM sampler, and AY instruments can share eight fixed tracks.
The mixer has per-track volume, mute, solo, and sends to the reverb & delay.
Clouds Reverb and tick-synchronized ping-pong delay are implemented.
All sound engines have minimal track FX.
I added some additional FX's inspired by Nerdseq and Elektron, in order to bring the sequencer on par with what's expected in 2026. No microtiming though.

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

## Braids engine

Braids is exposed as one instrument type. The `MODEL` parameter selects a `MacroOscillator` model instead of creating a separate instrument type for every model.

The 47 accessible models, numbered 0 through 46, share these parameters:

- `MODEL`
- `TIMBRE`
- `COLOR`
- note pitch
- `STRIKE` triggering where the model supports it

Each track is monophonic and owns one Braids instance, much like a hardware voice. AY and Braids instruments can run on different tracks in the same project.


## Plaits engine

Same as Braids, but with Plaits parameters.


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

### Audio rate and polyphony

The original Braids code and lookup tables expect 96 kHz, so ChooChooTracker currently runs its master audio engine at 96 kHz. This avoids changing the tables or adding a more complex internal resampling path.

The target is eight simultaneous Braids/Plaits voices at 96 kHz on an Anbernic RG353V. Tested, it works.

Silent voices skip unnecessary DSP work.

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

## Audio format

- Master engine: 96 kHz
- Internal mix: floating-point stereo
- Imported samples: 8-bit or 16-bit PCM, mono or stereo
- Export: 16-bit stereo WAV

The aim is clean sound on a handheld console, not a mastering chain.

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

## Roadmap

The [August 9, 2026 feasibility study](feasibility-2026-08-09.md) records the initial analysis. Plaits, the sends, and the first trigger conditions have since been implemented. Musical testing on RG353V are still required. CPU load is OK.

The next low-priority engine is `SCWF`, a dedicated dual-oscillator single-cycle waveform engine. Each oscillator reads a one-cycle WAV, with detune and mix controls. It remains separate from the monophonic one-shot Sample engine.

### 1. Establish the base

- [x] Build and run the unmodified ChipNomad base on Windows.
- [x] Run its existing tests.
- [x] Record the SDL2 and compiler versions.

### 2. Validate Braids in isolation

- [x] Build the desktop test program supplied with Braids
- [x] Render a 96 kHz WAV.
- [x] Keep `BraidsVoice` independent from the tracker.
- [x] Check pitch, `TIMBRE`, `COLOR`, `MODEL`, and `STRIKE`.
- [x] Confirm that every model produces finite, non-silent output without crashing.

### 3. Build the modern signal path

- [x] Add 12 and 24 dB filtering.
- [x] Add audio-rate ADSR for tonal models.
- [x] Add safe gain staging and protect the mixer from overflow.
- [x] Test live parameter changes for audible clicks.

### 4. Integrate Braids

- [x] Add `InstrumentType::Braids` and its data.
- [x] Give each track its own voice.
- [x] Route note-on, note-off, and retrigger events.
- [x] Mix modern voices with AY instruments.
- [x] Save and load eight-track `.cct` projects.
- [x] Expose every Braids model in the instrument screen.
- [x] Expose useful modulation destinations: volume, pitch, timbre, color, cutoff, and resonance.

### 5. Port and measure on RG353V

- [x] Build the ARM64 binary under WSL2.
- [x] Create the PortMaster package.
- [x] Test controls, audio output, and stability on the console.
- [x] Measure three, six, and eight voices at 96 kHz.
- Optimize only what hardware measurements identify as expensive.

### 6. Eight-track architecture and usability

- [x] Use eight fixed tracks.
- [x] Give each track an independent AY instance.
- [x] Add per-track volume, mute, and solo.
- [x] Save track volume in the project.
- [x] Ignore SDL repeat and use deterministic internal repeat.
- [x] Preserve held directions during button combinations.
- [x] Confirm that short presses are not lost on RG353V.
- [x] Validate repeat delay and speed on hardware.
- [x] Fix the mixer startup crash.
- [x] Add a CPU meter based on audio callback load.

### 7. Navigation and FX

- [x] Move the mixer out of the Project menu.
- [x] Add it as the leftmost main screen: `MSCPIT`.
- [x] Allow direct Mixer and Song navigation.
- [x] Separate universal FX from AY, Braids, Plaits, and Sample FX.
- [x] Show only FX supported by the active instrument.
- [x] Add per-step Braids FX for model, timbre, color, cutoff, and resonance.
- [x] Add per-step Sample FX for pitch, start, end, volume, cutoff, and resonance.
- [x] Reuse the existing three FX columns.
- [x] Restore instrument defaults at the next trigger. Instrument FX remain active until then.

### 8. PCM sampler

- [x] Load 8-bit and 16-bit PCM WAV files and convert 8-bit data to PCM16.
- [x] Preserve mono and stereo files.
- [ ] Copy source files into `samples/` and save a relative path.
- [x] Preload short samples into RAM.
- [x] Add one-shot playback, start, end, volume, and pitch.
- [x] Add ADSR and LP/BP/HP filtering with 12/24 dB slopes.
- [x] Add looping only if musical tests justify it.
- [x] Keep projects loadable when a sample file is missing.

### 9. Stabilization

- [x] Verify 16-bit WAV export.
- [x] Test hybrid AY, Braids, Plaits, and Sample projects.
- [x] Package the licenses for compiled source and libraries.
- [x] Produce an installable RG353V package.

### 10. Plaits, sends, and conditions

- [x] Integrate all 24 Plaits engines with Main/Aux blend, filtering, ADSR, save/load, and modulation.
- [x] Add the Plaits `PMD`, `PHA`, `PTM`, `PMO`, `PAX`, `PCF`, and `PRS` FX.
- [x] Add per-track Reverb and Delay sends.
- [x] Integrate the Clouds reverb algorithm as a shared send effect.
- [x] Add a tick-synchronized ping-pong delay with feedback and filtering.
- [x] Add `PRO 00-64`, `MOD AB`, and `SPD 00-10`.
- [x] Keep `SPD` active until another `SPD` changes it.
- [x] Display the Reverb and Delay sub-screens around `M` in the screen map.
- [x] Write the English user manual.
- [x] Measure eight Plaits voices with Reverb and Delay on RG353V.
- [x] Listen to all 24 engines, both sends, and every `SPD` ratio on ArkOS.

### 11. August 11 usability and synthesis pass

- [x] Correct Plaits pitch integration and add tuning and audible-output tests.
- [x] Display Plaits and Braids ADSR controls on one line.
- [x] Keep every Plaits and Braids model in exactly one popup category; `MISC` is only a fallback.
- [x] Add a reusable hierarchical selection popup for models and modulation destinations.
- [x] Add one common `00-FF` instrument volume, independent from track volume.
- [x] Share one multimode filter implementation across Braids, Plaits, and Sample.
- [x] Limit filter cutoff to 20 Hz through 20 kHz and use an exponential control curve.
- [x] Add app-wide Braids `BITS`, `DRFT`, and `SIGN` settings beside AY quality settings.
- [x] Add modulation destinations for effect sends and safe modulation-parameter targets.
- [x] Add sample audition in the file browser and fast previous/next sample selection.
- [x] Make Mixer, Reverb, and Delay navigation directional instead of toggled.
- [x] Remove PSG and VGM export while preserving WAV and stems.
- [x] Add the dual-oscillator `SCWF` engine after the stabilization work above.

### 12. Plaits-Alt model catalog

- [x] Add `Plaits-Alt` as a separate instrument type; preserve the 24 stock `Plaits` engine IDs and sounds for existing projects.
- [x] Create a separate `PlaitsAltVoice` registry rather than replacing the stock `plaits::Voice` configuration.
- [x] Import all 87 catalog models into `Plaits-Alt`, classified in one hierarchical category each.
- [x] Validate every engine on RG353V for CPU, filter, and retrigger behavior; warn when a model cannot meet the eight-voice target with Reverb and Delay active.
- [x] Add names and a compact “what each family sounds like” guide for every model.

### 13. Title screen

- [ ] Compress the title-screen artwork for distribution.

## First-version acceptance criteria

- [x] ChooChooTracker remains usable without major tracker regressions.
- [x] AY, Braids, Plaits, and Sample instruments can share a song.
- [x] Every accessible Braids model and Plaits engine is selectable.
- [x] Filters and envelopes do not introduce obvious clicks.
- [x] Eight Braids voices hold 96 kHz on RG353V.
- [x] The project builds natively on Windows.
- [x] The PortMaster package installs and runs on the console.
- [x] `.cct` projects preserve the levels of all eight tracks.
- [x] Mixer is a main screen instead of a Project submenu.
- [x] Braids, Plaits, and Sample instruments accept their own per-step FX.

The PCM8/PCM16 sampler is integrated. Portable sample paths and final console validation remain open.

## Out of scope

- long audio tracks
- streaming from the SD card
- multitrack recording
- mastering effects
- plugins
- DAW-style automation
- compatibility with microcontrollers, dedicated DSP hardware, or Eurorack modules

ChooChooTracker remains a handheld tracker. It adds richer synthesis and clean sample playback, but it should stay immediate and fun. Title screen and visual identity work can wait until the functional core is stable.
