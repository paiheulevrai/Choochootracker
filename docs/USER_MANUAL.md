# ChooChooTracker User Manual

ChooChooTracker is an 8-track music tracker for handheld consoles.

It follows the classic LSDj workflow and navigation system, with several sound design options:

- AY chip emulation
- Mutable Instruments Braids, Plaits and Plaits-Alt oscillators, with a multimode filter
- PCM samples, single-cycle waveforms and wavetable oscillators, with a multimode filter
- aChChid, a versatile acid synth based on Open303

## 1. Installation and files

### ArkOS / PortMaster

Install `choochootracker.zip` through PortMaster, or copy the extracted package to the console's `ports` directory. Start **ChooChooTracker** from the Ports menu.

### Windows

Run `choochootracker.exe` with `SDL2.dll` and `libwinpthread-1.dll` in the same directory.
Press **ALT + ENTER** to toggle fullscreen. The window is resizable; the image
scales while keeping its original aspect ratio.

### Directory configuration

| Folder | What it contains |
|---|---|
| `AY_Wavetables` | `.aywave` AY chip wavetables |
| `fonts` | `.cnfont` ChipNomad font files |
| `instruments` | `.cni` instrument presets, including ChipNomad files |
| `pitch-tables` | `.csv` alternative tunings |
| `projects` | `.cct` song and project files |
| `samples` | `.wav` files for the PCM Sample engine |
| `SR_wavetables` | Serum-format wavetables for the BYOWTBL engine |
| `themes` | `.cth` ChipNomad themes |
| `title` | Title screen assets |
| `waveforms` | `.wav` single-cycle waveforms for the 2xSCWF engine |

## 2. Controls

ChooChooTracker works best with a gamepad that has a D-pad, 2 analogue sticks and 7 buttons.

| Logical control | Handheld default | Windows default |
|---|---|---|
| **[LEFT/RIGHT/UP/DOWN]** | D-pad | Arrow keys |
| **EDIT** | A | X |
| **OPT** | B | Z |
| **PLAY** | Start | Space |
| **SELECT** | Select | Shift |
| Modulation axes | Left and right sticks | Game-controller sticks |
| **STICK LIVE** | L1 | Q |
| **MOTION RECORD** | L2 | W |
| **MOTION ERASE** | R2 | E |

Mappings can be changed in **Settings > Key mapping**.

Windows and web users can also use a game controller. Gamepad input has been tested on Windows and Android.

### Navigation

- Use **[UP/DOWN/LEFT/RIGHT]** to move the cursor.
- Hold **SELECT + [DIRECTION]** to move between screens.
- Hold **OPT + [DIRECTION]** for screen-specific navigation.
- On a value, use **EDIT + [LEFT/RIGHT]** for fine changes or **EDIT + [UP/DOWN]** for coarse changes.
- Tap **EDIT** to enter a value or activate a command.
- Double-tap **EDIT** to create a new chain, phrase or instrument where supported.
- Holding a direction repeats after the delay configured in Settings.

### Selection and clipboard

- **SELECT + OPT** enters selection mode and cycles through useful selection ranges.
- Use **[DIRECTION]** to extend the range.
- **OPT** copies the selection.
- **OPT + EDIT** cuts it.
- **SELECT + EDIT** pastes it.
- **EDIT + [DIRECTION]** edits the selected cells together.

### Playback

- **PLAY** starts from the cursor. Press it again to stop.
- You can edit notes, instruments and chains while playing; changes are heard
  at the next sequencer tick.
- **SELECT + PLAY** starts all tracks when used outside the Song screen.
- **EDIT + PLAY** previews an instrument from the Instrument Pool.

## 3. Song structure and screen map

Hold **SELECT + [DIRECTION]** to move between screens.

The main navigation row is:

<pre>
R P   G M
<strong>M S C P I T</strong>
D S     P W
</pre>

Central row:

- **M**: Mixer (branches to Reverb, Delay)
- **S**: Song (branches to Project, Settings)
- **C**: Chain
- **P**: Phrase (branches to Groove)
- **I**: Instrument (branches to Modulation and Instrument Pool)
- **T**: Table (branches to the AY Wavetable editor)

Top row:

- **R**: Reverb
- **P**: Project
- **G**: Groove
- **M**: Modulation

Bottom row:

- **D**: Delay
- **S**: Settings
- **P**: Instrument Pool
- **W**: AY Wavetable editor

A ChooChooTracker song follows this hierarchy:

```text
Song -> Chain -> Phrase row -> Note + Instrument + FX
```

A song has 8 tracks. Each track contains a sequence of chains, and each chain contains a sequence of phrases. Phrases are 16-step sequences of notes. A note contains a pitch, an instrument, a velocity and up to 3 track FX. An instrument is the thing that makes the sound.

## 4. Song, Chain and Phrase

### Song

The Song screen contains 8 columns, 1 per audio track. Each cell refers to a chain. A song can contain up to 256 rows (`00-FF`).

Tracks run from left to right, while their chains run vertically. Each track is independent, and chains can contain from 1 to 16 phrases, so tracks can drift out of sync. Design your chains to stay together, or let them wander if that is what the track needs.

It is common to reserve 1 chain for empty phrases, usually `00` or `FE`.

You can keep multiple sub-songs in a project by creating "islands": sections separated by empty rows.

#### Controls

- **OPT + [UP/DOWN]**: jump 16 positions up or down
- Select a range, then use **SHIFT + EDIT**: shallow-clone the chains while keeping their original phrases
- Select a range, then double-tap **SHIFT + EDIT**: deep-clone both the chains and their phrases
- Select a range, then use **EDIT + [UP/DOWN]**: move the selection up or down
- Tap **OPT** 3 times: add or remove a highlight for visual organisation
- **OPT + SHIFT**: mute the current track (release **OPT** first to keep the mute active)
- **OPT + PLAY**: solo the current track (release **OPT** first to keep the solo active)
- **OPT + [LEFT/RIGHT]**: solo every track to the left or right of the current track

### Chain

A chain is an ordered list of 16-step phrases with optional transposition. It can contain up to 16 phrases, and the same phrase can appear more than once. The 2nd column sets the transposition in semitones.

An asterisk (`*`) appears next to a chain that is reused in the project. You can clone chains from the Song screen.

#### Controls

- **OPT + [LEFT/RIGHT]**: move between tracks
- **OPT + [UP/DOWN]**: move between chains in the current track
- Select a range, then use **SHIFT + EDIT**: clone phrases

### Phrase

A phrase is the track pattern in a traditional step sequencer.

A phrase is 16 steps long, with 1 row per step. Each row contains a note, an instrument and FX columns. Notes use tracker notation such as `C-4` (note and octave).

To stop a note, insert `NOTE OFF`, use the kill-note FX, or play another note with no instrument set.

When a chain contains several phrases, the Phrase screen works like a continuous pattern editor. Moving below row `F` or above row `0` opens the next or previous phrase in the chain.

An asterisk (`*`) appears next to a phrase that is reused in the project. You can clone phrases from the Chain screen.

The FX selector shows common commands plus those supported by the instrument on that row. This avoids a long global list of engine-specific commands. ChooChooTracker supports more FX than ChipNomad.

### Controls

- **OPT + EDIT** on an empty note: insert `NOTE OFF`
- **EDIT + [UP/DOWN]** on an FX name column: open the FX selection screen
- **OPT + [LEFT/RIGHT]**: move between tracks
- **OPT + [UP/DOWN]**: move between phrases in the current chain
- Select a range in the instrument column, then use **SHIFT + EDIT**: clone instruments
- Select a range, then use **EDIT + [UP/DOWN]**: rotate the phrase rows

## 5. Instruments

All instruments have a few common parameters:

- Instrument type
- Name, up to 15 characters
- Default table speed, in ticks per table row
- Transpose on or off

Each instrument has a default table with the same number in the `00-7F` range. You can use these as auxiliary tables, but keeping auxiliary tables in the `80-FE` range avoids conflicts and confusion. Tables are among the main sound design tools in ChooChooTracker.

### Controls

- **OPT + [LEFT/RIGHT]**: move between instruments
- **EDIT + PLAY**: preview the instrument
- **SHIFT + OPT**: copy the instrument
- **SHIFT + EDIT**: paste the instrument

### AY Classic, AY Plus and AY Sample

These are the original ChipNomad AY/YM engines. AY Classic exposes hardware-style tone, noise and envelope controls. AY Plus adds software oscillators and richer modulation. AY Sample reproduces a sample through AY-style volume levels and is distinct from the PCM Sample engine.

#### AY Classic

This is a legacy instrument type from ChipNomad `v1.0.0`. The newer AY Plus instrument offers more features.

AY Classic instruments have these parameters:

- Mixer: tone on or off, noise on or off, envelope shape (`0-F`)
- Volume: software-generated ADSR envelope
- Automatic envelope period: on or off, with a rate from `1:1` to `F:F`

**AY Quality** in Settings affects only AY/YM rendering. It does not change Braids, Plaits or PCM Sample quality. **Sample dithering** applies to AY Sample quantisation.

See the [ChipNomad AY-3-8910 documentation](https://chipnomad.org/chips/ay-3-8910/) for chip details.

#### AY Plus

AY Plus applies classic synthesiser concepts to AY-3-8910/YM2149F sound design. You can think of AY as a synthesiser with 5 oscillators:

- 3 square wave tone oscillators
- 1 noise oscillator
- 1 amplitude envelope generator which can be used as an oscillator

Under this model, each instrument has 3 hardware oscillators: tone, noise and envelope. AY Plus adds a 4th, software oscillator. Updating AY registers faster than the project tick rate opens up more sound design options. Because of the way AY works, the oscillators are mixed using ring modulation. Try different pitch offsets, detuning and oscillator combinations to see what falls out.

Each tonal oscillator can be tuned independently with pitch and fine tune offsets.

Software oscillator types and their parameters:

- Pulse - square wave with controllable pulse width
  - FM Depth - simple FM depth
  - Pulse width - the full range is `00-FF`, while the number of steps is set on the Project screen
  - Pulse low - controls the low value in a repeating pair whose high value is always `15`
- Sync Tone - hard-sync effect for the tone oscillator
  - FM Depth - simple FM depth
- Sync Env - hard-sync effect for the envelope oscillator
  - FM Depth - simple FM depth
  - Pulse width - duration ratio between the 2 shapes in the envelope pair
  - Envelope Pair - 2 envelope shapes to switch between
- Wavetable - 32-step wavetables, edited on the Wavetable screen
  - FM Depth - simple FM depth
  - Wavetable index
- Tone FM - simple FM-like effect for the tone oscillator
  - FM Depth - tone FM depth
- Env FM - simple FM-like effect for the envelope oscillator
  - FM Depth - envelope FM depth

Pulse and Wavetable software oscillators cannot be used with the Envelope oscillator. The Envelope oscillator switches off automatically.

Simple FM switches quickly between 2 period values, 1 lower and 1 higher. This is similar to using a square wave as an FM modulator. Pulse, Sync Tone, Sync Env and Wavetable use a `1:1` ratio between the FM carrier and modulator. Tone FM and Env FM can imitate other ratios through software oscillator pitch offsets.

#### AY Sample

You can load mono or stereo WAV files at `8-bit`, `16-bit`, `24-bit` or `32-bit`. They are converted to `8-bit` and truncated to `16 KB`. This limit matches the RAM page size of the ZX Spectrum.

AY plays samples through fast writes to the volume register. Its output is unipolar, so samples need to be "lifted to zero" for cleaner playback. This removes end-of-sample clicks and improves quiet tails.

AY has `4-bit` DAC resolution with a non-linear volume scale, so `8-bit` samples are converted to `4-bit` during playback. The Sample dithering option in Settings can improve the perceived bit depth. Dithering is not practical on retro platforms such as the ZX Spectrum and Atari ST, so leave it off when writing for real hardware or chasing a crunchy lo-fi sound.

Samples behave like another AY Plus software oscillator, but their extra parameters make them easier to handle as a separate instrument type.

Sample parameters:

- Sample rate - playing a sample at its native rate produces a `C-4`
- Sample start
- Sample length
- Sample loop start
- Sample pitch offset
- Sample fine pitch offset

The Tone and Noise oscillator sections match those of AY Plus. Because of the way AY works, all oscillators are mixed using ring modulation.

### Braids

Braids provides 47 synthesis models. Its main controls are:

- **Model**: oscillator or synthesis algorithm
- **Timbre** and **Color**: model-dependent macro parameters
- **Filter**: additional LP/BP/HP filter, `12` or `24 dB` slope, cutoff and resonance
- **ADSR**: attack, decay, sustain and release

Tap **Model** to choose from categorised model lists.

| Category | Models |
|---|---|
| Analog | `00 CSAW`, `01 MORPH`, `02 SAW-SQUARE`, `03 SINE-TRI`, `04 BUZZ`, `05 SQUARE-SUB`, `06 SAW-SUB`, `07 SQUARE-SYNC`, `08 SAW-SYNC` |
| Multi Osc | `09 TRIPLE-SAW`, `10 TRIPLE-SQR`, `11 TRIPLE-TRI`, `12 TRIPLE-SINE`, `13 TRIPLE-RING`, `14 SAW-SWARM`, `15 SAW-COMB`, `16 TOY` |
| Filter / Voice | `17 FILTER-LP`, `18 FILTER-PEAK`, `19 FILTER-BP`, `20 FILTER-HP`, `21 VOSIM`, `22 VOWEL`, `23 VOWEL-FOF`, `24 HARMONICS` |
| FM / Chaos | `25 FM`, `26 FEEDBACK-FM`, `27 CHAOTIC-FM` |
| Physical | `28 PLUCKED`, `29 BOWED`, `30 BLOWN`, `31 FLUTED`, `32 STRUCK-BELL`, `33 STRUCK-DRUM` |
| Drums | `34 KICK`, `35 CYMBAL`, `36 SNARE` |
| Wavetables | `37 WAVETABLES`, `38 WAVE-MAP`, `39 WAVE-LINE`, `40 WAVE-PARA` |
| Noise / Granular | `41 FILTER-NOISE`, `42 TWIN-PEAKS`, `43 CLOCK-NOISE`, `44 GRAN-CLOUD`, `45 PARTICLE`, `46 DIGI-MOD` |

#### Parameters

Timbre and Color depend on the selected model. Refer to the original Braids manual from Mutable Instruments for the full model list and details, or just play by ear.

#### VCF and VCA

Braids feeds the standard ChooChooTracker multimode filter and ADSR envelope.

#### Settings

Global Braids settings are available in the app settings:

- **BITS**: reduces output resolution
- **DRFT**: adds oscillator pitch instability
- **SIGN**: reproduces the original Braids waveform-imperfection algorithm

### aChChid

**aChChid** is a monophonic acid bass engine based on Open303. `Square` and `Saw` use its native TB-303 oscillator, filter, envelope and accent behaviour. `Braids` replaces only the oscillator, then continues through the same 303 filter and amplifier path. It exposes Model, Timbre and Color instead of Fine tune. aChChid does not use ChooChooTracker's unified post-filter or ADSR.

An `F` in the note volume column triggers an accent. `ASL` slides to that note from the previous pitch without retriggering the 303 envelope. `ASL 00` gives a `60 ms` glide. Notes without `ASL` always retrigger. Modulation destinations include Decay and Accent, plus Timbre and Color in Braids wave mode.

### Subtractive engines

The engines in this category share a VCO to VCF to VCA architecture.

Several VCO types are available:

- **Braids**
- **Plaits**
- **Plaits-Alt**
- **PCM Sample**
- **2xSCWF**
- **BYOWTBL**

Each instrument feeds its VCO into a multimode filter.

The filter has a switchable `12 dB` or `24 dB` slope and can work as a low-pass (LP), band-pass (BP) or high-pass (HP) filter.

Several filter characters are available:

- **Clean**: sterile and digital
- **Classic**: inspired by classic American analogue synths
- **Aggro**: inspired by roaring Japanese analogue synths
- **Acid**: inspired by psychedelics

Classic, Aggro and Acid can get seriously resonant in this alpha. Start low and turn them up gently.

The filter feeds a VCA controlled by an ADSR envelope. The envelope shape can morph between exponential, linear and logarithmic curves.

#### Plaits and Plaits-Alt

**Plaits** provides 24 engines, grouped in the selection popup as follows:

| Category | Engines |
|---|---|
| Analog / Waves | `00 VA VCF`, `01 PHASE DIST`, `05 WAVE TERRAIN`, `06 STRING MACH`, `08 VIRTUAL ANALOG`, `09 WAVESHAPING`, `11 FORMANT`, `12 HARMONIC`, `13 WAVETABLE`, `14 CHORD` |
| FM | `02 6-OP FM 1`, `03 6-OP FM 2`, `04 6-OP FM 3`, `10 2-OP FM` |
| Digital | `07 CHIPTUNE`, `15 SPEECH` |
| Texture / Noise | `16 SWARM`, `17 NOISE`, `18 PARTICLE` |
| Physical | `19 STRING`, `20 MODAL` |
| Drums | `21 BASS DRUM`, `22 SNARE DRUM`, `23 HI-HAT` |

**Plaits-Alt** is a collection of alternative models for the original Plaits module. Most are stranger or more experimental than the average VCO.

| Category | Engines |
|---|---|
| Granular / Micro | `GLISSON`, `PULSAR`, `GENDY`, `SCANNED`, `LOOPBACK` |
| Phase / Harmonic | `PHASE WEAVE`, `SIDEBAND BANK`, `UNDERTOW`, `ATTRACTOR`, `LOCKSTEP` |
| Acoustic / Physical | `REED PIPE`, `BRASS`, `SHAKERS`, `CLAPS`, `FRESHETS FORMANT` |
| Polyphony / Harmony | `DIATONIC CHORD`, `SCALE STACK`, `WT DIATONIC CHORD`, `WT SCALE STACK`, `HELIX` |
| Digital / Weird | `BYTEBEAT`, `RULEFIELD`, `SPECTRAL SPIRAL`, `PHASE FLOCK` |

The parameters follow Mutable's design:

- **Harmonic** controls harmonic relationships, balance or model choice inside an engine.
- **Timbre** generally moves from dark/sparse to bright/dense spectra.
- **Morph** explores another timbral dimension.
- **Main/Aux** blends the main output with the engine's alternate output.

Their precise meaning depends on the engine. For example, chord engines use them for chord type, inversion and waveform. Physical models use them for material, excitation and decay, while drum engines use them for tone, character and decay. Refer to the Plaits user manual for details.

Tap **Engine** to choose from categorised engine lists.

**Env Mode** has 2 routings:

- `TRIG` reproduces the module with TRIG connected and LEVEL unpatched
- `VCA` holds LEVEL open and applies the tracker ADSR after the voice

#### PCM Sample

This clean Sample engine plays mono or stereo PCM samples loaded into RAM.

- Tap **Sample** to load an uncompressed `8-bit` or `16-bit` PCM WAV. Press **PLAY** in the browser to audition the highlighted file.
- On the Sample instrument screen, use **EDIT + [LEFT/RIGHT]** to load the previous or next WAV in the same folder.
- **Pitch** transposes by semitones (`-48` to `+48`).
- **Start** and **End** set normalised playback boundaries (`00-FF`). If Start is after End, the sample plays in reverse.
- **Loop** selects Off, Loop or Ping-Pong.
- **Speed** controls granular time-stretching from `0%` to `500%` (`100%` is normal).

Unsupported WAV formats display an error. Convert unusual files to `PCM8` or `PCM16` WAV before importing them.

#### 2xSCWF

2xSCWF is a dual single-cycle waveform oscillator. Load 1 mono WAV containing exactly 1 period into each oscillator. Both oscillators read forwards and wrap at the end of their cycle.

Mix crossfades A and B.

Detune is fine and exponential from unison through `+200 ct`, then moves in semitone steps from `+3 st` to `+24 st`.

2xSCWF treats each file as a 1-cycle table and reads it exactly once per oscillator cycle. This lets you blend and detune 2 single-cycle waves.

The factory `waveforms/AKWF/` folder contains waveforms from the Adventure Kid library. Plenty more are available online if you want to expand the collection.

#### BYOWTBL

BYOWTBL is a dual wavetable oscillator compatible with Serum tables. Each oscillator loads a mono WAV containing consecutive single-cycle frames. `Pos A` and `Pos B` scan their tables independently from `00` to `FF`. The engine interpolates linearly within each frame and between adjacent frames. `Mix` crossfades the oscillators, and `Detune` offsets oscillator B.

## 6. Modulation and motion recording

Each instrument can have up to 4 modulation slots.

The available modulation types are:

- ADSR - classic attack-decay-sustain-release envelope
- AHD - attack-hold-decay envelope
- LFO - low-frequency oscillator with several shapes
- SLFO - LFO with a period multiplier for slow cycles
- FLFO - audio-rate LFO that can reach the kHz range; FLFO can be heavy on the CPU
- SLIN - linear joystick control
- SRAT - rate-based joystick control

The list of modulation destinations depends on the instrument type.

When an ADSR or AHD envelope targets Volume, it becomes the instrument's volume envelope. An LFO targeting Volume offsets the current output volume instead.

The modulation amount can be positive or negative. Its range depends on the destination. If the destination's full range is `127` or less, as it is for most parameters, the amount defines an absolute range. Wider destinations such as pitch use a scaled amount.

All duration parameters, including attack, hold, decay and period, are measured in ticks.

LFO shapes:

- Tri - bipolar triangle wave
- Sin - bipolar sine wave
- UniTri - unipolar triangle wave
- UniSin - unipolar sine wave
- RampDn - linear ramp down (saw wave)
- RampUp - linear ramp up (saw wave)
- ExpDn - exponential ramp down (exponential saw wave)
- ExpUp - exponential ramp up (exponential saw wave)
- Square - square wave
- Random - sample-and-hold random wave

LFO trigger types:

- Free - restarts the LFO only when the instrument changes
- Retrig - restarts the LFO on every note
- Hold - stops after 1 cycle and holds the last value
- Once - stops after 1 cycle and returns to zero

### Motion recording

- Hold **STICK LIVE** (`L1` by default) to apply stick modulation without changing the phrase.
- During playback, hold **MOTION RECORD** (`L2`) to apply stick modulation and record changed destinations as absolute FX values.
- Hold **MOTION ERASE** (`R2`) to remove matching destination FX from the current row.

Motion recording writes track FX into the phrase currently playing. It updates matching FX first, then uses empty slots from right to left. It never overwrites a different FX. If all 3 slots are full, that motion is not recorded on the step. A `!` in the bottom-right corner means that more destinations changed than the 3 FX columns could hold.

Motion recording supports Braids, Plaits, PCM Sample, 2xSCWF and BYOWTBL destinations, including sample start, end, speed, loop and filter controls where applicable.

## 7. Tables

Tables are a core sound design tool in trackers. In Vortex Tracker terms, they combine instruments and ornaments, but they can do much more. If you know LSDj or NerdSEQ, the idea should already feel familiar.

The Pitch column accepts relative (`~`) or absolute (`=`) pitch values in semitones. Volume is applied on top of the ADSR envelope. The 4 FX lanes mostly match the lanes in a phrase, although a few FX behave differently in tables.

Putting a `TIC` FX on the last table row sets the speed of that column and overrides the instrument's default table speed. Each FX column can run at a different tick speed. The Pitch and Volume columns follow the speed of the 1st FX column.

A 16-row table may look short next to a long Vortex Tracker instrument, but the `HOP` FX can create conditional loops such as "repeat these rows 5 times", as well as nested loops. With loops and independent FX column speeds, those 16 rows can go a surprisingly long way.

Tables `00-7F` are reserved for default instrument tables. Tables `80-FE` are intended for auxiliary tables started by the `TBX` effect. The lower range also works for auxiliary tables, but using it that way can cause unexpected conflicts and confusion.

### Controls

- **EDIT + [UP/DOWN]** on an FX name column: open the FX selection screen
- **OPT + [DIRECTION]**: move between tables

## 8. Other screens

### Instrument Pool

The Instrument Pool shows every instrument in the project and lets you reorder them. During playback, it also shows which instruments are currently active.

#### Controls

- **EDIT**: edit the instrument and jump to the Instrument screen
- **SHIFT + OPT**: copy the instrument
- **SHIFT + EDIT**: paste the instrument
- **EDIT + [UP/DOWN]**: reorder instruments
- **EDIT + PLAY**: preview the instrument

### AY Wavetable

Wavetable is an AY Plus software oscillator type. A project can contain up to 256 wavetables, each with 32 steps. All wavetable instruments share the same set of waves. This screen is where you edit them.

AY volume levels are non-linear, so the waveform is drawn to match the actual output levels.

The screen has 2 logical rows. The top row contains the **Load** and **Save** buttons, while the 2nd contains the wavetable editor.

#### Controls

- **OPT + [LEFT/RIGHT]**: move through the wavetable list
- **OPT + [UP/DOWN]**: move through the list by 16 waves
- **SHIFT + OPT**: copy the wavetable
- **SHIFT + EDIT**: paste the wavetable

## 9. Tracker FX

Track FX force sequencer or sound-engine values on individual steps. Other grooveboxes often call these "parameter locks". Each Phrase row has 3 FX slots.

Each FX has a 3-letter command and a hexadecimal value. The in-app help panel gives a short description of the selected command.

### Sequencer FX

| FX | Value | Detailed behaviour |
|---|---|---|
| `ARP` | `XY` | Arpeggiates the base note, `+X` steps and `+Y` steps. `37` produces a minor-chord pattern. |
| `ARC` | `XY` | `X` selects the arpeggio direction or range mode; `Y` is its speed in ticks. |
| `PVB` | `XY` | Pitch vibrato: `X` is speed and `Y` is depth. In Linear mode, depth uses `10-cent` steps. |
| `PBN` | signed `XX` | Adds `XX` pitch units every phrase/table row; `FF` means `-1`. Use `00` to stop. |
| `PSL` | `XX` ticks | Slides from the preceding pitch to the new note over `XX` ticks. |
| `PIT` | signed `XX` | Accumulated relative offset in pitch-table steps. |
| `FIN` | signed `XX` | Accumulated fine offset in cents with Linear pitch, period units otherwise. |
| `PRD` | signed `XX` | Accumulated relative oscillator-period offset. |
| `VOL` | signed `XX` | Accumulated relative volume offset. |
| `VSL` | signed `XX` | Adds `XX` to volume on every phrase/table row. Use `00` to stop. |
| `RET` | `XY` | Retriggers every `Y` ticks; `X` applies a volume change. `Y=0` stops retriggering. |
| `DEL` | `XX` ticks | Delays note-on. A delay longer than the current groove step skips the note. |
| `OFF` | `XX` ticks | Sends note-off after `XX` ticks and enters an ADSR release stage. |
| `KIL` | `XX` ticks | Hard-kills the voice after `XX` ticks without running ADSR release. |
| `TIC` | `XX` ticks | Sets table ticks per row. In a table it changes that FX column's speed. |
| `TBL` | `00-FE`, `FF` off | Replaces the instrument table; `FF` stops it. |
| `TBX` | `00-FE`, `FF` off | Starts an auxiliary table alongside the instrument table; `FF` stops it. |
| `THO` | row `XX` | Jumps all instrument-table columns to row `XX`. |
| `TXH` | row `XX` | Jumps all auxiliary-table columns to row `XX`; it is not used from inside a table. |
| `GRV` | groove `XX` | Selects a groove for the current track. |
| `GGR` | groove `XX` | Selects a groove for every track. |
| `HOP` | `XY` | Jumps to row `Y`, `X` times; `X=0` loops forever. Table HOP affects its own column. |
| `SNG` | signed `XX` | Moves playback by `XX` song rows. |
| `PRO` | `00-64` | Evaluates an absolute trigger probability from `0%` to `100%`. |
| `MOD` | `AB` | Triggers on pass `A` of a `B`-pass cycle. |
| `SPD` | signed `XX` | Selects a persistent per-track clock ratio; see the speed table below. |
| `SLE` | `00-FF` ticks | Sets persistent per-track glide time for continuous engine FX. `00` is immediate. The setting resets to `00` when playback stops. |

### Audio FX sends

| FX | Value | Detailed behaviour |
|---|---|---|
| `RSN` | `00-FF` | Sets this track's reverb send until the next note trigger. |
| `DSN` | `00-FF` | Sets this track's delay send until the next note trigger. |

### ADSR / Trigger FX

| FX | Value | Detailed behaviour |
|---|---|---|
| `EAT` / `EDC` / `ESU` / `ERL` / `ESH` | `00-FF` | Override attack, decay, sustain, release or envelope shape. |
| `TDC` / `TCL` | `00-FF` | Override Trigger-mode Decay or Color on Plaits and Plaits-Alt. |

#### Conditions

Inspired by Swedish "trig conditions", the tracker supports trigger probabilities and modulo conditions.

| FX | Value | Meaning |
|---|---|---|
| `PRO` | `00-64` | Absolute trigger probability from `0%` to `100%` |
| `MOD` | `AB` | Trigger on iteration `A` of `B`; for example `12`, `22`, `14`, `34` |

`MOD` counters are local to the track and phrase. Invalid combinations, such as `A` greater than `B`, do not trigger.

`MOD34` triggers on the 3rd pass of every 4-pass cycle. `MOD1F` triggers on the 1st pass of every 16-pass cycle.

#### Playback speed

Like NerdSEQ, ChooChooTracker supports an independent playback speed for each track.

`SPD` reads its value as a signed byte. `00` is normal speed, positive values make the track faster, and negative values make it slower. The setting remains active until another `SPD` command changes it.

| Value | Speed |
|---|---:|
| `00` | `x1` |
| `01` | `x2` |
| `02` | `x3` |
| `7F` | `x128` |
| `FF` | `/2` |
| `FE` | `/3` |
| `80` | `/129` |

Older projects without signed `SPD` support keep their original `00-10` mapping, so they still play as saved.

Very high multipliers can exceed the resolution of the tracker tick scheduler and have not been hardware-tested.

### Modulation FX

Phrase and table FX can change a modulation slot without editing the instrument:

| FX pattern | Meaning |
|---|---|
| `M1A`, `M2A`, `M3A`, `M4A` | Relative Amount offset for modulation slots 1-4 |
| `M11`, `M12`, `M13`, `M14` | Relative P1-P4 offsets for modulation slot 1 |
| `M21`, `M22`, `M23`, `M24` | Relative P1-P4 offsets for modulation slot 2 |
| `M31`, `M32`, `M33`, `M34` | Relative P1-P4 offsets for modulation slot 3 |
| `M41`, `M42`, `M43`, `M44` | Relative P1-P4 offsets for modulation slot 4 |

The value is interpreted as a signed `8-bit` relative change (`01` adds `1`, `FF` subtracts `1`). Repeated commands accumulate. Effective values are clamped to their valid range.

| Modulation type | P1 | P2 | P3 | P4 |
|---|---|---|---|---|
| ADSR | Attack | Decay | Sustain | Release |
| AHD | Attack | Hold | Decay | Unused |
| LFO | Shape | Trigger mode | Period | Unused |
| SLFO | Shape | Trigger mode | Ticks | Multiplier |
| FLFO | Shape | Trigger mode | Frequency (`1 Hz` to `20 kHz`) | Unused |

### Braids FX

| FX | Value | Meaning |
|---|---|---|
| `BMD` | `00-2E` | Braids model |
| `BTM` | `00-FF` | Absolute normalised Timbre |
| `BCL` | `00-FF` | Absolute normalised Color |
| `BCF` | `00-FF` | Exponential cutoff, `20 Hz` to `20 kHz` |
| `BRS` | `00-FF` | Exponential filter resonance |

### aChChid FX

| FX | Value | Meaning |
|---|---|---|
| `ASL` | `00-FF` | Slide to the note without retriggering. `00` is `60 ms`; higher values extend the glide. |
| `ADC` | `00-FF` | Decay override: `200 ms` to `2 s`. |
| `AAC` | `00-FF` | Accent amount: none to maximum. |
| `ATM` | `00-FF` | Braids Timbre override; active only in Braids wave mode. |
| `ACL` | `00-FF` | Braids Color override; active only in Braids wave mode. |
| `ACF` | `00-FF` | Exponential 303 filter cutoff, `20 Hz` to `20 kHz`. |
| `ARS` | `00-FF` | 303 filter resonance, none to maximum. |
| `AEM` | `00-FF` | 303 filter envelope modulation, none to maximum. |

### Plaits FX

| FX | Value | Meaning |
|---|---|---|
| `PMD` | `00-17` | Plaits engine |
| `PHA` | `00-FF` | Absolute normalised Harmonics |
| `PTM` | `00-FF` | Absolute normalised Timbre |
| `PMO` | `00-FF` | Absolute normalised Morph |
| `PAX` | `00-FF` | Main/Aux blend: `00` Main, `FF` Aux |
| `PCF` | `00-FF` | Exponential cutoff, `20 Hz` to `20 kHz` |
| `PRS` | `00-FF` | Exponential filter resonance |

### Sample FX

| FX | Value | Meaning |
|---|---|---|
| `SPT` | signed `XX` | Sample transposition in semitones |
| `SST` | `00-FF` | Normalised playback start |
| `SEN` | `00-FF` | Normalised playback end |
| `SVL` | `00-FF` | Absolute sample volume |
| `SCF` | `00-FF` | Exponential cutoff, `20 Hz` to `20 kHz` |
| `SRS` | `00-FF` | Exponential filter resonance |
| `SSP` | `00-FF` | Sample speed, mapped from `0%` to `500%` |
| `SLP` | `00-02` | Loop mode: Off, Loop, Ping-Pong |

### AY FX shared by AY instruments

| FX | Value | Detailed behaviour |
|---|---|---|
| `AYM` | `XY` | `X` is envelope shape; `Y` selects off/tone/noise/tone+noise (`0/1/2/3`). |
| `NOI` | signed `XX` | Accumulated relative noise-period offset. |
| `NOA` | `00-1F`, `FF` | Absolute noise period; `FF` yields noise-period priority to earlier tracks. |
| `ERT` | any | Retriggers the current hardware envelope shape. |
| `EAU` | `XY` | Automatic envelope ratio `X:Y`; `X=0` disables it. |

### AY Classic-only FX

| FX | Value | Detailed behaviour |
|---|---|---|
| `EVB` | `XY` | Envelope-period vibrato: `X` speed, `Y` depth. |
| `EBN` | signed `XX` | Adds `XX` to envelope period every phrase/table row. |
| `ESL` | `XX` ticks | Slides from the preceding envelope period to the new value. |
| `ENT` | note `XX` | Sets envelope period from the pitch-table note shown by the UI. |
| `EPT` | signed `XX` | Accumulated relative envelope-period offset. |
| `EPL` | byte `XX` | Sets the low byte of the envelope period. |
| `EPH` | byte `XX` | Sets the high byte of the envelope period. |

### AY Plus FX

| FX | Value | Detailed behaviour |
|---|---|---|
| `TNN` | note `XX` | Sets the tone oscillator to a pitch-table note, ignoring the row note. |
| `TNP` / `TNF` | signed `XX` | Accumulated tone pitch-step / fine offset. |
| `TRT` | any | Retriggers tone oscillator phase where supported. |
| `ENN` | note `XX` | Sets the envelope oscillator to a pitch-table note. |
| `ENP` / `ENF` | signed `XX` | Accumulated envelope pitch-step / fine offset. |
| `SFT` | `00-07` | Software type: None, Pulse, Sync Tone, Sync Env, Wavetable, Tone FM, Env FM, Sample. |
| `SFN` | note `XX` | Sets the software oscillator to a pitch-table note. |
| `SFP` / `SFF` | signed `XX` | Accumulated software oscillator pitch-step / fine offset. |
| `SRT` | any | Resets software oscillator phase. |
| `SFM` | signed `XX` | Accumulated FM-depth offset. |
| `PWM` | signed `XX` | Accumulated pulse-width offset. |
| `SPL` | signed `XX` | Accumulated pulse low-level offset. |
| `SWT` | signed `XX` | Accumulated wavetable-index offset. |

### AY Sample FX

AY Sample accepts the shared AY commands plus `TNN`, `TNP`, `TNF`, `TRT`, `SFN`, `SFP` and `SFF` as described above.

| FX | Value | Detailed behaviour |
|---|---|---|
| `SMS` | `00-FF` | Sets legacy AY Sample playback start to `XX x 64` source samples. |

Engine parameter FX apply to their matching instrument type and are reset at the next note trigger unless stated otherwise. `SPD` is intentionally persistent.

The FX selector filters this reference to common commands plus the group supported by the active instrument. The contextual help panel provides value details while editing.

## 10. Mixer, Reverb and Delay

The Mixer is the leftmost main screen. It displays CPU load and clipping warnings. A red `!` in the **CLIP** column marks the track whose dry signal pushed the mix beyond the safe range.

Use it to balance the 8 tracks, shape each track with Tilt EQ, and send audio to the reverb or delay:

| Field | Meaning |
|---|---|
| LVL | Post-engine track level, `000-100` |
| REV | Send to the shared Clouds reverb, `000-100` |
| DLY | Send to the shared ping-pong delay, `000-100` |
| TLT | Per-track Tilt EQ, `00-FF`; `80` is neutral, low values favour bass and high values favour treble. Low values will introduce a soft overdrive. |
| M | `*` mutes this track |
| S | `*` solos this track |

The mixer is track-based, not instrument-based. If a track changes instruments, its level, Tilt and sends remain attached to the track. Remember that each instrument also has its own `00-FF` volume on the Instrument screen, before the track level.

### Auto Mix

The Mixer contains an **AUTO MIX** button. It renders `6 seconds` offline,
balances track loudness, then checks 8 octave bands against a pink-noise
profile and gently lowers tracks that crowd those bands. It proposes
conservative level changes, leaves some peak headroom and starts a temporary
preview. **Apply** keeps the proposed levels, while **Cancel** restores the previous
8 levels. It gives you a useful starting point, but you will probably still
want to tweak the result.

### Clouds Reverb

Use **SELECT + [UP]** from Mixer to open the reverb settings screen.

- **Return**: wet reverb level in the master mix.
- **Time**: decay/time control, `00-FF`.
- **Damping**: high-frequency absorption, `00-FF`.
- **Filter**: low-pass filter applied before the reverb.

This is the Clouds reverb section, not the complete Clouds granular processor.

### Ping-pong Delay

Use **SELECT + [DOWN]** from Mixer to open Delay, then **SELECT + [UP]** to return. Reverb works in the opposite direction: **SELECT + [UP]** opens it and **SELECT + [DOWN]** returns to Mixer.

- **Return**: wet delay level.
- **To Reverb**: amount of the wet delay signal sent into the shared reverb, `0-100%`.
- **Ticks**: delay time in tracker ticks (`9 ticks` = `1 beat` by default).
- **Feedback**: cross-feedback amount, limited to `95%`.
- **Filter**: low-pass filter in the delayed signal path.

The 1st repeat follows the stereo input. Later feedback crosses between the left and right channels.

## 11. Project screen

The Project screen provides **Load**, **Save**, **New**, **Export** and **Manage** commands, along with filename, title and author metadata.

- **Linear pitch** selects the pitch-table mode. **Off** is the default and the hardware-validated setting for correct AY, Braids and Plaits octave tracking.
- **Tick rate** sets tracker timing and displays the corresponding BPM (`tick rate x 60 / 24`).
- ChooChooTracker saves projects as `.cct`. This format is not compatible with ChipNomad.

Use **Save** before changing instrument types or loading another project.

## 12. Settings

- **Repeat delay / speed** tune held-button repeat.
- **Mix volume** controls final application output.
- **Tilt pivot** is a per-project `250-4000 Hz` frequency, defaulting to `1 kHz`. It sets the centre frequency for all Mixer Tilt controls.
- **AY Quality** changes AY/YM emulation quality only.
- **Sample dithering** controls AY Sample dithering only.
- **Braids BITS / DRFT / SIGN** apply globally to every Braids instrument.
- **Key mapping**, **Load font**, and **Edit color theme** customise the interface. ChipNomad fonts and themes should work.
- **Quit ChooChooTracker** exits cleanly.

## 13. Performance and troubleshooting

### CPU

CPU cost depends on the active engines and effects. Plaits physical models and Clouds Reverb are heavier than basic AY voices. Check performance on the target console, especially with 8 Plaits voices and both sends active. A CPU reading near `100%` can cause crackles or missed audio deadlines.

If you run into pops, crashes or slowdowns, send us the `.cct` file that triggers them.

### No sound

Check the instrument number, track mute or solo state, track LVL, application Mix volume and instrument envelope. For samples, check that the original WAV still exists at its saved path. On ArkOS, press **MENU + L3** to toggle the operating system mute. If that does not help, restart the hardware and reconnect your audio interface.

### Input feels wrong

Adjust Repeat delay and Repeat speed in Settings. If a single press moves twice, check that only 1 physical control is mapped to that direction and report the exact screen and shortcut.

## 14. Credits and licensing

ChooChooTracker is a fork of ChipNomad and retains its MIT licensing approach. Braids, Plaits, Plaits-Alt, Clouds DSP and stmlib code are derived from Mutable Instruments' open-source releases under their applicable MIT notices. Plaits-Alt is sourced from the lylepmills/eurorack Plaits Lab fork; its retained source notices apply. The aChChid engine uses Open303 by Robin Schmidt, copyright 2009, under the MIT License. See the packaged license files for exact attribution.
