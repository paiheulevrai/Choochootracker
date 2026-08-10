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

The mixer is track-based. If a track changes instruments, its level and sends remain attached to the track.

If needed, you should be able to change individual instruments volumes (if not possible, open a github issue)

### Clouds Reverb

Press **Select + Up** from Mixer to open the reverb settings screen

- **Return**: wet reverb level in the master mix.
- **Time**: decay/time control, `00–FF`.
- **Damping**: high-frequency absorption, `00–FF`.
- **Filter**: low-pass filter applied before the reverb.

It is not the complete Clouds granular processor, only its meme lush reverb.

### Ping-pong Delay

Press **Select + Down** from Mixer to open Delay; repeat it to return.

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

An instrument stores a name, type, optional transpose setting, table speed and engine-specific parameters. Use the Instrument Pool to browse all 128 slots, move instruments, copy/paste them and preview them.

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

Braids terminology follows the original Mutable Instruments module: the audible result of Timbre and Color depends on the selected model. Refer ot the original Braids manual (available on Mutable's Github) for a list and details of all the models.

### Plaits

Plaits provides 24 engines in this build:

| 00–07 | 08–0F | 10–17 |
|---|---|---|
| VA VCF | Virtual Analog | Swarm |
| Phase Distortion | Waveshaping | Noise |
| 6-op FM 1 | 2-op FM | Particle |
| 6-op FM 2 | Formant | String |
| 6-op FM 3 | Harmonic | Modal |
| Wave Terrain | Wavetable | Bass Drum |
| String Machine | Chord | Snare Drum |
| Chiptune | Speech | Hi-hat |

The common Plaits macros follow Mutable's design:

- **Harmonic** controls harmonic relationships, balance or model choice inside an engine.
- **Timbre** generally moves from dark/sparse to bright/dense spectra.
- **Morph** explores a second timbral dimension.
- **Main/Aux** blends the main output with the engine's alternate output.

Their precise meaning is engine-dependent. For example, chord engines use them for chord type/inversion/waveform, physical models use them for material/excitation/decay, and drum engines use them for tone, character and decay. Refer to the Plaits user manual for details.

The tracker adds a LP/BP/HP filter and ADSR after the Plaits oscillator.

### PCM Sample

The clean Sample engine plays mono or stereo PCM independently of AY emulation.

- Tap **Sample** to load an uncompressed 16-bit PCM WAV.
- **Pitch** transposes by semitones (`-48` to `+48`).
- **Start** and **End** set normalized playback boundaries (`00–FF`).
- **Volume** is `00–FF`.
- The optional filter provides LP/BP/HP, 12/24 dB slope, cutoff and resonance.
- The ADSR shapes amplitude.

Unsupported WAV formats display a longer error message. Convert unusual files to PCM16 WAV before importing.

## 8. Modulation, Tables, Groove and Wavetables

Each instrument has four modulation slots. A slot can use ADSR, AHD or LFO behavior and targets a destination offered by that engine. Engine-specific destinations include Braids Timbre/Color, Plaits Harmonic/Timbre/Morph/Main-Aux and Sample playback/filter parameters. If you need more modulation targets, open a Github issue.

Tables are small per-instrument sequences that can automate commands over time. Groove defines tick lengths used by phrase rows. Wavetables are used by compatible AY software oscillator modes (these won't work with the Braids/Plaits wavetable modes. Maybe later? Open a github issue if you're into that)

## 9. Tracker FX

FX have a three-letter command and a hexadecimal value. The in-app help panel gives a short description of the selected command.

### Conditions

Inspired by Swedish "trig conditions", the tracker supports trig probabilities and modulo

| FX | Value | Meaning |
|---|---|---|
| `PRO` | `00–64` | Absolute trigger probability from 0 to 100% |
| `MOD` | `AB` | Trigger on iteration A of B; for example `12`, `22`, `14`, `34` |

`MOD` counters are local to the track and phrase. Invalid combinations, such as A greater than B, do not trigger.

MOD34 will trigger 3rd out of 4 times. MOD1F will trigger on the first occurence out of 16.

### Playback speed

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

### Braids FX

| FX | Meaning |
|---|---|
| `BMD` | Model |
| `BTM` | Timbre |
| `BCL` | Color |
| `BCF` | Filter cutoff |
| `BRS` | Filter resonance |

### Plaits FX

| FX | Meaning |
|---|---|
| `PMD` | Engine/model |
| `PHA` | Harmonic |
| `PTM` | Timbre |
| `PMO` | Morph |
| `PAX` | Main/Aux blend |
| `PCF` | Filter cutoff |
| `PRS` | Filter resonance |

### Sample FX

| FX | Meaning |
|---|---|
| `SPT` | Signed semitone pitch |
| `SST` | Playback start |
| `SEN` | Playback end |
| `SVL` | Volume |
| `SCF` | Filter cutoff |
| `SRS` | Filter resonance |

Engine parameter FX apply to their matching instrument type and are reset at the next note trigger unless stated otherwise. `SPD` is intentionally persistent.

### Common and AY FX

The common group includes arpeggio, pitch, volume, retrigger, delay, note offset/kill, tick, table, groove, hop and song commands. AY-specific groups expose mixer, noise, hardware envelope, tone and software oscillator controls. Use the FX selection screen and its contextual help for the valid command set of the current instrument. Chipnomad user manual can help.

## 10. Project screen

Project provides Load, Save, New, Export and Manage commands, plus filename, title and author metadata.

- **Linear pitch** selects the pitch-table mode. For the current Braids integration, keep it **Off**: this is the validated setting and the default must produce correct octave tracking.
- **Tick rate** sets tracker timing and displays the corresponding BPM (`tick rate × 60 / 24`).
- ChooChooTracker saves projects as `.cct`, our proprietary format that's not compatible with Chipnomad.

Save before changing instrument types or loading another project.

## 11. Settings

- **Repeat delay / speed** tune held-button repeat.
- **Mix volume** controls final application output.
- **AY Quality** changes AY/YM emulation quality only.
- **Sample dithering** controls AY Sample dithering only.
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
