# ChooChooTracker

> **NOT EVEN ALPHA. Testing is not finished. CHOO CHOO.**

ChooChooTracker is a fork of [ChipNomad](https://github.com/Megus/chipnomad-tracker). It keeps ChipNomad's LSDJ-inspired tracker and expands its sound palette with modern synthesis engines. The name comes from the first proof of concept, written on a train between Cahors and Montauban.

> **Project status:** active development. The Windows and PortMaster builds work, but this version is for testing.

The main target is the Anbernic RG353V through PortMaster. A native Windows build is kept for development and debugging.

This is not a DAW. It is a small, self-contained instrument for writing music on the move, with a deliberately playful side.

## Current status

The Windows build compiles and runs. Braids (47 models), Plaits (24 engines), the PCM sampler, and AY instruments can share eight fixed tracks. The mixer has per-track volume, mute, solo, and sends to a Clouds Reverb and a tick-synchronized ping-pong delay. The `PRO`, `MOD`, and `SPD` FX add trigger conditions and per-track speed. WSL2 produces the ARM64 binary and PortMaster package. See the [English user manual](docs/USER_MANUAL.md) for controls and workflow.

## Design rules

- Keep ChipNomad's tracker, sequencer, and workflow where they still fit.
- Allow AY/YM and modern instruments in the same song.
- Add synthesis features without rewriting working code.
- Keep the interface usable on a small screen with few buttons.
- Prefer a simple, predictable architecture that is easy to port.

## Reference sources

Reference checkouts live in the ignored `inspirations/` directory:

- `chipnomad-tracker-main/` contains the ChipNomad base.
- `mutable-eurorack/` contains the official Mutable Instruments Braids, Plaits, Clouds, and stmlib sources.
- `mutable-instruments-documentation-main/` contains the Mutable manuals used while writing the ChooChooTracker manual.

Braids is pinned to commit `08460a69a7e1f7a81c5a2abcc7189c9a6b7208d4`. stmlib is pinned to `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec`.

These directories are references only. Working code belongs in the fork so local changes do not become mixed with upstream source trees.

## What comes from ChipNomad

ChipNomad already separates sequencing from audio generation. The sequencer advances on each tick, then `chipnomadRender()` asks each engine for an audio block and mixes the results into a floating-point stereo buffer.

The original base provides:

- AY1, AY2, and AYSample instruments
- four generic modulation slots per instrument
- ADSR, AHD, and LFO modulation
- WAV import and export
- SDL2 targets for Windows, Linux, and PortMaster

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

### Filter

Each Braids voice has a digital filter with:

- 12 or 24 dB/octave slope
- low-pass, band-pass, and high-pass modes
- adjustable cutoff
- adjustable resonance

The 12 dB mode uses one state-variable filter. The 24 dB mode cascades two stages.

### Audio rate and polyphony

The original Braids code and lookup tables expect 96 kHz, so ChooChooTracker currently runs its master audio engine at 96 kHz. This avoids changing the tables or adding a more complex internal resampling path.

The target is eight simultaneous Braids voices at 96 kHz on an Anbernic RG353V. Tests also cover three and six voices to measure headroom. Eight-voice support is not considered proven until it passes on the console.

Silent voices skip unnecessary DSP work.

## PCM samples

The original `AYSample` instrument remains available for deliberately crunchy sounds. It converts WAV files to unsigned 8-bit mono, limits them to 16,384 samples, and plays them through AY-style 4-bit volume levels.

The separate `Sample` instrument provides clean playback:

- external WAV files loaded from the project's `samples/` directory
- 8-bit or 16-bit PCM converted to signed PCM16 in memory
- mono or stereo playback, preserving the original channel layout
- preserved source sample rate
- direct output to the floating-point mixer without the AY path
- one-shot playback with start and end points
- useful transposition over roughly one or two octaves in either direction
- linear interpolation
- a modern filter and envelope

The current implementation loads each WAV into RAM and stores its path in the project. It does not yet copy the file into `samples/` or convert the path to a portable relative path. SD card streaming is out of scope because this engine is intended for drums and short one-shots.

A project still loads when a sample is missing. The affected instrument remains silent and the interface displays a warning.

WAV data is not embedded in `.cct` project files. This keeps projects readable and avoids inflating them with audio data.

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

The [August 9, 2026 feasibility study](docs/feasibility-2026-08-09.md) records the initial analysis. Plaits, the sends, and the first trigger conditions have since been implemented. Musical testing and CPU measurements on RG353V are still required.

The next engine feature under consideration is pitched single-cycle sample looping. It should remain part of the Sample engine if that is the simplest design.

### 1. Establish the base

- Build and run the unmodified ChipNomad base on Windows.
- Run its existing tests.
- Record the SDL2 and compiler versions.

### 2. Validate Braids in isolation

- Build the desktop test program supplied with Braids.
- Render a 96 kHz WAV.
- Keep `BraidsVoice` independent from the tracker.
- Check pitch, `TIMBRE`, `COLOR`, `MODEL`, and `STRIKE`.
- Confirm that every model produces finite, non-silent output without crashing.

### 3. Build the modern signal path

- Add 12 and 24 dB filtering.
- Add audio-rate ADSR for tonal models.
- Add safe gain staging and protect the mixer from overflow.
- Test live parameter changes for audible clicks.

### 4. Integrate Braids

- Add `InstrumentType::Braids` and its data.
- Give each track its own voice.
- Route note-on, note-off, and retrigger events.
- Mix modern voices with AY instruments.
- Save and load eight-track `.cct` projects.
- Expose every Braids model in the instrument screen.
- Expose useful modulation destinations: volume, pitch, timbre, color, cutoff, and resonance.

### 5. Port and measure on RG353V

- [x] Build the ARM64 binary under WSL2.
- [x] Create the PortMaster package.
- Test controls, audio output, and stability on the console.
- Measure three, six, and eight voices at 96 kHz.
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
- [ ] Add looping only if musical tests justify it.
- [x] Keep projects loadable when a sample file is missing.

### 9. Stabilization

- [ ] Verify 16-bit WAV export.
- [ ] Test hybrid AY, Braids, Plaits, and Sample projects.
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
- [ ] Measure eight Plaits voices with Reverb and Delay on RG353V.
- [ ] Listen to all 24 engines, both sends, and every `SPD` ratio on ArkOS.

## First-version acceptance criteria

- ChooChooTracker remains usable without major tracker regressions.
- AY, Braids, Plaits, and Sample instruments can share a song.
- Every accessible Braids model and Plaits engine is selectable.
- Filters and envelopes do not introduce obvious clicks.
- Eight Braids voices hold 96 kHz on RG353V.
- The project builds natively on Windows.
- The PortMaster package installs and runs on the console.
- `.cct` projects preserve the levels of all eight tracks.
- Mixer is a main screen instead of a Project submenu.
- Braids, Plaits, and Sample instruments accept their own per-step FX.

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
