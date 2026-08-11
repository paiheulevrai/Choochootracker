# ChooChooTracker User Manual

ChooChooTracker is an eight-track music tracker designed for small screens and handheld consoles. Its workflow follows the LSDj family of trackers: a song contains chains, chains contain phrases, and phrases contain notes, instruments and FX. Each instrument chooses its own sound engine, so AY, Braids, Plaits and PCM samples can be freely mixed in one project.

This manual describes the current development build. Features marked as requiring hardware validation should be tested on the target ArkOS/PortMaster console before live use.

Since this is a fork from Chipnomad and using Mutable Instruments Braids and Plaits engine, I strongly recommend you also read the user manuals from these projects.

## 1. Installation and files

### ArkOS / PortMaster

Install `choochootracker.zip` through PortMaster, or copy the extracted package to the console's `ports` directory. Start **ChooChooTracker** from the Ports menu.

Projects use the `.cct` extension. Instrument files use `.cni`. PCM instruments load uncompressed WAV files. Keep projects, instruments and samples in their configured folders; a PCM project stores the sample path, not a second copy of the audio file.

### Windows

Run `choochootracker.exe` with `SDL2.dll` and `libwinpthread-1.dll` in the same directory.

## 2. Controls

ChooChooTracker uses eight logical controls.

| Logical control | Handheld default | Windows default |
|---|---|---|
| Left, Right, Up, Down | D-pad | Arrow keys |
| Edit | A | X |
| Opt | B | Z |
| Play | Start | Space |
| Select | Select | Shift |

Mappings can be changed in **Settings > Key mapping**.

### Navigation

- Use the D-pad to move the cursor.
- Hold **Select** and press a direction to move between screens.
- Hold **Opt** and press a direction for screen-specific navigation.
- On a value, hold **Edit** and press Left/Right for fine changes or Up/Down for coarse changes.
- Tap **Edit** to enter a value or activate a command.
- Double-tap **Edit** to create a new chain, phrase or instrument where supported.
- **Opt + Edit** clears or cuts the current value.
- Holding a direction repeats after the delay configured in Settings.

### Selection and clipboard

- **Select + Opt** enters selection mode and cycles through useful selection ranges.
- Move the D-pad to extend the range.
- **Opt** copies the selection.
- **Opt + Edit** cuts it.
- **Select + Edit** pastes it.
- **Edit + direction** edits the selected cells together.

### Playback

- **Play** starts from the cursor; press it again to stop.
- **Select + Play** starts all tracks when used outside the Song screen.
- **Edit + Play** previews an instrument from the Instrument Pool.

## 3. Song structure and screen map

The main navigation row is:

```text
M S C P I T
```

- **M**: Mixer
- **S**: Song
- **C**: Chain
- **P**: Phrase
- **I**: Instrument
- **T**: Table

Same as LSDJ but I added the Mixer as first entry.

Secondary screens appear above or below their parent:
- Project and Settings surround Song.
- Reverb and Delay surround Mixer.
- Groove belongs to Phrase;
- Instrument Pool and Modulation belong to Instrument;
- Wavetable belongs to Table.

The hierarchy is:

```text
Song track -> Chain -> Phrase row -> Note + Instrument + FX
```

There are always eight independent tracks. An AY instrument used on one track does not share a forced three-channel chip with other tracks (this is a break from Chipnomad)

## 4. A first pattern

1. Open the Song screen and create or enter a chain number on a track (like "0")
2. Move to Chain and enter a phrase number (like "0")
3. Move to Phrase and enter notes.
4. Enter an instrument number next to a note ("0" is fine). Create that instrument in the Instrument screen or Instrument Pool.
5. Press Play (start button on the RG console)
6. Add FX in the phrase FX columns as needed.
7. Set track levels and sends in Mixer, then save the project from Project.

## 5. Mixer, Reverb and Delay

The Mixer is the leftmost main screen. Its CPU display reports audio callback load.

Each track has:

| Field | Meaning |
|---|---|
| LVL | Post-engine track level, 000–100 |
| REV | Send to the shared Clouds reverb, 000–100 |
| DLY | Send to the shared ping-pong delay, 000–100 |
| MUTE | Silence this track |
| SOLO | Listen to this track alone |

The mixer is track-based. If a track changes instruments, its level and sends remain attached to the track. Each instrument also has its own `00-FF` volume on the Instrument screen, before the track level.

### Clouds Reverb

Press **Select + Up** from Mixer to open the reverb settings screen

- **Return**: wet reverb level in the master mix.
- **Time**: decay/time control, `00–FF`.
- **Damping**: high-frequency absorption, `00–FF`.
- **Filter**: low-pass filter applied before the reverb.

It is not the complete Clouds granular processor, only its meme lush reverb.

### Ping-pong Delay

Press **Select + Down** from Mixer to open Delay. Press **Select + Up** from Delay to return to Mixer. Reverb follows the opposite direction: Select + Up opens it and Select + Down returns.

- **Return**: wet delay level.
- **Ticks**: delay time in tracker ticks (by default 6 ticks = 1 beat)
- **Feedback**: cross-feedback amount, limited to 95%.
- **Filter**: low-pass filter in the delayed signal path.

The first repeat alternates according to the stereo input and subsequent feedback crosses between left and right.

## 6. Song, Chain and Phrase

### Song

The Song screen contains eight columns, one per track. Each cell refers to a chain. The song can contain up to 256 rows (`00–FF`).

### Chain

A chain is an ordered list of phrases (a phrase is a 16 steps pattern) with optional transposition. Reusing a chain or phrase lets one edit repeated musical material once.

### Phrase

That's what you'd call a track pattern in a traditional step sequencer.

A phrase has 16 rows (equivalent to your 16 steps). A  row contains a note, instrument and FX columns. Notes use tracker notation such as `C-4`; an empty note does not retrigger the voice. Instrument changes are allowed on every row and a track may alternate freely between AY, Braids, Plaits and Sample instruments. You can change the note lengh either by using the kill note FX, or by playing a subsequent note with an unset instrument.

The FX selector only shows common FX plus the FX relevant to the instrument on that row. This prevents engine-specific commands from becoming one unmaintainable global list. Scroll down screen, we have more FXs than Chipnomad.

## 7. Instruments

An instrument stores a name, type, `00-FF` volume, optional transpose setting, table speed and engine-specific parameters. Use the Instrument Pool to browse all 128 slots, move instruments, copy/paste them and preview them.

Changing an instrument type resets the engine-specific data for that slot.

### AY Classic, AY Plus and AY Sample

These are the original ChipNomad AY/YM engines. AY Classic exposes hardware-style tone, noise and envelope controls. AY Plus adds software oscillators and richer modulation. AY Sample reproduces a sample through AY-style volume levels and is distinct from the PCM Sample engine.

**AY Quality** in Settings affects only AY/YM rendering. It does not change Braids, Plaits or PCM sample quality. **Sample dithering** applies to AY Sample quantization.

### Braids

Braids provides 47 synthesis models. Its main controls are:

- **Model**: oscillator/synthesis algorithm.
- **Timbre** and **Color**: model-dependent macro parameters.
- **Filter**: additional LP/BP/HP filter, 12 or 24 dB slope, cutoff and resonance.
- **ADSR**: attack, decay, sustain and release.

Tap **Model** to choose from categorized model lists. **BITS**, **DRFT**, and **SIGN** are global Braids settings rather than instrument parameters. BITS reduces output resolution, DRFT adds oscillator pitch instability, and SIGN blends the original Braids waveform-imperfection algorithm. SIGN uses a stable character generated for this installation and stored in `settings.txt`.

Braids terminology follows the original Mutable Instruments module: the audible result of Timbre and Color depends on the selected model. Refer ot the original Braids manual (available on Mutable's Github) for a list and details of all the models.

The model popup contains every model exactly once:

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

### Plaits

Plaits provides 24 engines, grouped in the selection popup as follows:

| Category | Engines |
|---|---|
| Analog / Waves | `00 VA VCF`, `01 PHASE DIST`, `05 WAVE TERRAIN`, `06 STRING MACH`, `08 VIRTUAL ANALOG`, `09 WAVESHAPING`, `11 FORMANT`, `12 HARMONIC`, `13 WAVETABLE`, `14 CHORD` |
| FM | `02 6-OP FM 1`, `03 6-OP FM 2`, `04 6-OP FM 3`, `10 2-OP FM` |
| Digital | `07 CHIPTUNE`, `15 SPEECH` |
| Texture / Noise | `16 SWARM`, `17 NOISE`, `18 PARTICLE` |
| Physical | `19 STRING`, `20 MODAL` |
| Drums | `21 BASS DRUM`, `22 SNARE DRUM`, `23 HI-HAT` |

The common Plaits macros follow Mutable's design:

- **Harmonic** controls harmonic relationships, balance or model choice inside an engine.
- **Timbre** generally moves from dark/sparse to bright/dense spectra.
- **Morph** explores a second timbral dimension.
- **Main/Aux** blends the main output with the engine's alternate output.

Their precise meaning is engine-dependent. For example, chord engines use them for chord type/inversion/waveform, physical models use them for material/excitation/decay, and drum engines use them for tone, character and decay. Refer to the Plaits user manual for details.

Tap **Engine** to choose from categorized engine lists. **Env Mode** has two routings: `TRIG` reproduces the module with TRIG connected and LEVEL unpatched; `VCA` holds LEVEL open and applies the tracker ADSR after the voice. Every tracker note sends a new trigger pulse, including consecutive notes without an empty row. In VCA mode, a retrigger starts the attack from the current envelope level to avoid abrupt jumps and clicks. In TRIG mode, the envelope row becomes `D` for LPG decay and `C` for LPG color. Projects saved with the former `LEVEL` mode load as `VCA`.

### PCM Sample

The clean Sample engine plays mono or stereo PCM independently of AY emulation.

- Tap **Sample** to load an uncompressed 8-bit or 16-bit PCM WAV. Press **Play** in the browser to audition the highlighted file.
- On the Sample instrument screen, use **Edit + Left/Right** to load the previous or next WAV in the same folder.
- **Pitch** transposes by semitones (`-48` to `+48`).
- **Start** and **End** set normalized playback boundaries (`00–FF`).
- **Volume** is `00–FF`.
- The optional filter provides LP/BP/HP, 12/24 dB slope, cutoff and resonance.
- The ADSR shapes amplitude.

Unsupported WAV formats display a longer error message. Convert unusual files to PCM8 or PCM16 WAV before importing.

## 8. Modulation, Tables, Groove and Wavetables

Each instrument has four modulation slots. A slot can use ADSR, AHD or LFO behavior and targets a destination offered by that engine. Engine-specific destinations include Braids Timbre/Color, Plaits Harmonic/Timbre/Morph/Main-Aux and Sample playback/filter parameters. Generic destinations can modulate the track's Reverb/Delay sends or one of the four parameters of another modulation slot. Cross-modulation uses the source's previous tick and cannot target itself.

Tap a destination to open the categorized selector. Synth macro parameters are shown as `000-1023`; this is a normalized interface range mapped to the engine's internal resolution. Instrument and send filters cover 20 Hz to 20 kHz on an exponential control curve.

Tables are small per-instrument sequences that can automate commands over time. Groove defines tick lengths used by phrase rows. Wavetables are used by compatible AY software oscillator modes (these won't work with the Braids/Plaits wavetable modes. Maybe later? Open a github issue if you're into that)

## 9. Tracker FX

FX have a three-letter command and a hexadecimal value. The in-app help panel gives a short description of the selected command.

### Sequencer FX

| FX | Value | Detailed behavior |
|---|---|---|
| `ARP` | `XY` | Arpeggiates the base note, +X steps and +Y steps. `37` produces a minor-chord pattern. |
| `ARC` | `XY` | X selects the arpeggio direction/range mode; Y is its speed in ticks. |
| `PVB` | `XY` | Pitch vibrato: X is speed and Y is depth. In Linear mode, depth uses 10-cent steps. |
| `PBN` | signed `XX` | Adds XX pitch units every phrase/table row; `FF` means -1. Use `00` to stop. |
| `PSL` | `XX` ticks | Slides from the preceding pitch to the new note over XX ticks. |
| `PIT` | signed `XX` | Accumulated relative offset in pitch-table steps. |
| `FIN` | signed `XX` | Accumulated fine offset in cents with Linear pitch, period units otherwise. |
| `PRD` | signed `XX` | Accumulated relative oscillator-period offset. |
| `VOL` | signed `XX` | Accumulated relative volume offset. |
| `VSL` | signed `XX` | Adds XX to volume on every phrase/table row. Use `00` to stop. |
| `RET` | `XY` | Retriggers every Y ticks; X applies a volume change. Y=`0` stops retriggering. |
| `DEL` | `XX` ticks | Delays note-on. A delay longer than the current groove step skips the note. |
| `OFF` | `XX` ticks | Sends note-off after XX ticks and enters an ADSR release stage. |
| `KIL` | `XX` ticks | Hard-kills the voice after XX ticks without running ADSR release. |
| `TIC` | `XX` ticks | Sets table ticks per row. In a table it changes that FX column's speed. |
| `TBL` | `00-FE`, `FF` off | Replaces the instrument table; `FF` stops it. |
| `TBX` | `00-FE`, `FF` off | Starts an auxiliary table alongside the instrument table; `FF` stops it. |
| `THO` | row `XX` | Jumps all instrument-table columns to row XX. |
| `TXH` | row `XX` | Jumps all auxiliary-table columns to row XX; it is not used from inside a table. |
| `GRV` | groove `XX` | Selects a groove for the current track. |
| `GGR` | groove `XX` | Selects a groove for every track. |
| `HOP` | `XY` | Jumps to row Y, X times; X=`0` loops forever. Table HOP affects its own column. |
| `SNG` | signed `XX` | Moves playback by XX song rows. |
| `PRO` | `00-64` | Evaluates an absolute trigger probability from 0 to 100%. |
| `MOD` | `AB` | Triggers on pass A of a B-pass cycle. |
| `SPD` | `00-10` | Selects a persistent per-track clock ratio; see the speed table below. |

Only one instance of each FX type runs on a track, regardless of which FX column introduced it. Phrase FX override auxiliary-table FX, which override instrument-table FX. Triggering a new instrument resets active FX; a note with no instrument lets compatible FX continue or restart. Most continuous FX stop with `00`; table selectors stop with `FF`.

#### Conditions

Inspired by Swedish "trig conditions", the tracker supports trig probabilities and modulo

| FX | Value | Meaning |
|---|---|---|
| `PRO` | `00–64` | Absolute trigger probability from 0 to 100% |
| `MOD` | `AB` | Trigger on iteration A of B; for example `12`, `22`, `14`, `34` |

`MOD` counters are local to the track and phrase. Invalid combinations, such as A greater than B, do not trigger.

`MOD34` triggers on the third pass of every four-pass cycle. `MOD1F` triggers on the first pass of every sixteen-pass cycle.

#### Playback speed

Inspired by Nerdseq, we support individual playback speed per track

| `SPD` | `00–10` | Persistent playback speed for this track |

`SPD` remains active until another `SPD` command changes it:

| Value | Speed | Value | Speed |
|---|---:|---|---:|
| 00 | /7 | 09 | Normal |
| 01 | /6 | 0A | ×2 |
| 02 | /5 | 0B | ×4 |
| 03 | /3 | 0C | ×8 |
| 04 | /32 | 0D | ×3 |
| 05 | /16 | 0E | ×5 |
| 06 | /8 | 0F | ×6 |
| 07 | /4 | 10 | ×7 |
| 08 | /2 | | |

Very high multipliers can exceed the resolution of the current tracker tick scheduler and haven't been tested (yet).

### Modulation FX

Phrase and table FX can change a modulation slot without editing the instrument:

| FX pattern | Meaning |
|---|---|
| `M1A`, `M2A`, `M3A`, `M4A` | Relative Amount offset for modulation slot 1…4 |
| `M11`, `M12`, `M13`, `M14` | Relative P1…P4 offsets for modulation slot 1 |
| `M21`, `M22`, `M23`, `M24` | Relative P1…P4 offsets for modulation slot 2 |
| `M31`, `M32`, `M33`, `M34` | Relative P1…P4 offsets for modulation slot 3 |
| `M41`, `M42`, `M43`, `M44` | Relative P1…P4 offsets for modulation slot 4 |

The value is interpreted as a signed 8-bit relative change (`01` adds one, `FF` subtracts one). Repeated commands accumulate. Effective values are clamped to their valid range.

| Modulation type | P1 | P2 | P3 | P4 |
|---|---|---|---|---|
| ADSR | Attack | Decay | Sustain | Release |
| AHD | Attack | Hold | Decay | Unused |
| LFO | Shape | Trigger mode | Period | Unused |

### Braids FX

| FX | Value | Meaning |
|---|---|---|
| `BMD` | `00-2E` | Braids model |
| `BTM` | `00-FF` | Absolute normalized Timbre |
| `BCL` | `00-FF` | Absolute normalized Color |
| `BCF` | `00-FF` | Exponential cutoff, 20 Hz to 20 kHz |
| `BRS` | `00-FF` | Filter resonance |

### Plaits FX

| FX | Value | Meaning |
|---|---|---|
| `PMD` | `00-17` | Plaits engine |
| `PHA` | `00-FF` | Absolute normalized Harmonics |
| `PTM` | `00-FF` | Absolute normalized Timbre |
| `PMO` | `00-FF` | Absolute normalized Morph |
| `PAX` | `00-FF` | Main/Aux blend: `00` Main, `FF` Aux |
| `PCF` | `00-FF` | Exponential cutoff, 20 Hz to 20 kHz |
| `PRS` | `00-FF` | Filter resonance |

### Sample FX

| FX | Value | Meaning |
|---|---|---|
| `SPT` | signed `XX` | Sample transposition in semitones |
| `SST` | `00-FF` | Normalized playback start |
| `SEN` | `00-FF` | Normalized playback end |
| `SVL` | `00-FF` | Absolute sample volume |
| `SCF` | `00-FF` | Exponential cutoff, 20 Hz to 20 kHz |
| `SRS` | `00-FF` | Filter resonance |

### AY FX shared by AY instruments

| FX | Value | Detailed behavior |
|---|---|---|
| `AYM` | `XY` | X is envelope shape; Y selects off/tone/noise/tone+noise (`0/1/2/3`). |
| `NOI` | signed `XX` | Accumulated relative noise-period offset. |
| `NOA` | `00-1F`, `FF` | Absolute noise period; `FF` yields noise-period priority to earlier tracks. |
| `ERT` | any | Retriggers the current hardware envelope shape. |
| `EAU` | `XY` | Automatic envelope ratio X:Y; X=`0` disables it. |

### AY Classic-only FX

| FX | Value | Detailed behavior |
|---|---|---|
| `EVB` | `XY` | Envelope-period vibrato: X speed, Y depth. |
| `EBN` | signed `XX` | Adds XX to envelope period every phrase/table row. |
| `ESL` | `XX` ticks | Slides from the preceding envelope period to the new value. |
| `ENT` | note `XX` | Sets envelope period from the pitch-table note shown by the UI. |
| `EPT` | signed `XX` | Accumulated relative envelope-period offset. |
| `EPL` | byte `XX` | Sets the low byte of the envelope period. |
| `EPH` | byte `XX` | Sets the high byte of the envelope period. |

### AY Plus FX

| FX | Value | Detailed behavior |
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

| FX | Value | Detailed behavior |
|---|---|---|
| `SMS` | `00-FF` | Sets legacy AY Sample playback start to `XX × 64` source samples. |

Engine parameter FX apply to their matching instrument type and are reset at the next note trigger unless stated otherwise. `SPD` is intentionally persistent.

The FX selector filters this reference to common commands plus the group supported by the active instrument. The contextual help panel provides value details while editing.

## 10. Project screen

Project provides Load, Save, New, Export and Manage commands, plus filename, title and author metadata.

- **Linear pitch** selects the pitch-table mode. **Off** is the default and the hardware-validated setting for correct AY, Braids and Plaits octave tracking.
- **Tick rate** sets tracker timing and displays the corresponding BPM (`tick rate × 60 / 24`).
- ChooChooTracker saves projects as `.cct`, our proprietary format that's not compatible with Chipnomad.

Save before changing instrument types or loading another project.

## 11. Settings

- **Repeat delay / speed** tune held-button repeat.
- **Mix volume** controls final application output.
- **AY Quality** changes AY/YM emulation quality only.
- **Sample dithering** controls AY Sample dithering only.
- **Braids BITS / DRFT / SIGN** apply globally to every Braids instrument. Their behavior and saved installation character match the description in the Braids section.
- **Key mapping**, **Load font**, and **Edit color theme** customize the interface. Should be compatible with Chipnomad font and themes.
- **Quit ChooChooTracker** exits cleanly.

## 12. Performance and troubleshooting

### CPU

CPU cost depends on active engines and effects. Plaits physical models and Clouds Reverb are heavier than basic AY voices. Measure on the target console, especially with eight Plaits voices and both sends active. A CPU reading near 100% can cause crackles or missed audio deadlines.

### No sound

Check the instrument number, track mute/solo state, track LVL, application Mix volume and the instrument envelope. For samples, verify that the original WAV is still at its saved path. If you are using ArkOS, push menu+L3 (joystick press) to enable/disable OS mute.

### Wrong pitch

Use tonal material and verify several octaves. Keep Linear pitch Off for the currently validated Braids behavior. Percussive or noisy Plaits engines are not suitable tuner references. Open a github issue if that's too weird.

### Input feels wrong

Adjust Repeat delay and Repeat speed in Settings. If a single press moves twice, verify that only one physical control is mapped to that direction and report the exact screen and shortcut.

## 13. Credits and licensing

ChooChooTracker is a fork of ChipNomad and retains its MIT licensing approach. Braids, Plaits, Clouds DSP and stmlib code are derived from Mutable Instruments' open-source releases under their applicable MIT notices. See the packaged license files for exact attribution.

The descriptions of Mutable synthesis behavior in this manual are adapted to ChooChooTracker's controls from the original Mutable Instruments Braids, Plaits and Clouds documentation included in the repository's `inspirations` directory.
