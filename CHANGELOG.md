# ChooChooTracker changelog

## Unreleased

- Expanded the Sample Edit Slice row into three modes: **EQUAL** divides the
  Start/End window evenly, **AUTO** places slices on transients found by a
  spectral-flux detection (adjustable sensitivity, falls back to an even
  division on silence), and **LAZY** starts with one whole-window slice that
  grows by editing. Slice points are stored per instrument and saved in the
  project as `- Sample slice bounds:`.
- Added slice editing on the Slice row: navigate slices with EDIT + UP/DOWN,
  split the current slice at its midpoint with EDIT (tap), delete it with
  EDIT + OPT, and nudge its start frame with EDIT + LEFT/RIGHT (fine) or
  UP/DOWN (coarse); markers stay inside their own slice.
- Added the LAZY playback workflow: tap PLAY to toggle a full-sample preview
  that keeps running after the key is released, with a bright position marker
  on the waveform; every EDIT click while it plays drops a slice at the
  playback position (slices closer than 50 ms to an existing one are
  rejected). Song playback treats LAZY samples as unsliced.
- Added a save-destination dialog for sliced samples: SAVE TO SAMPLE writes
  the slice points into the WAV as cue chunks (visible as markers in DAWs),
  SAVE TO PROJECT keeps them in the project only, and a "Don't ask again in
  this project" checkbox stores the choice as `- Sample save choice:` in the
  .cct.
- Loading a WAV that contains cue chunks into an unsliced Sampler instrument
  now seeds AUTO slicing from the cue positions.
- Slice and Stretch are mutually exclusive: enabling one disables the other.

## v0.1.0-prealpha.5 (October 3, 2026)

- Added a Mode row to the Scale screen with a second mode, Note Lock:
  notes typed into phrases are snapped to the nearest note of the picked
  scale (EDIT + RIGHT/UP step up, other edits snap down), while notes
  already stored stay intact when the scale changes. SCL FX is a
  Quantizer-mode command and is ignored while Note Lock is active; chord
  quantization via CRD still applies in both modes.
- Added pattern fill and randomize for the phrase selection's note column:
  OPT + LEFT cycles eight fixed 16-step rhythm patterns (aligned to absolute
  phrase rows, so partial selections only receive the steps inside them),
  OPT + RIGHT applies randomized fills of 3-8 notes with 7 and 8 less
  probable, cycling indefinitely while OPT is held; every press erases the
  selection's notes first, the step after the last pattern erases only
  before the pattern cycle restarts, releasing OPT resets it, and placed
  notes carry the last used instrument.
- Added mutate for the phrase selection's note column (OPT + UP): every
  press shifts 1-3 of the placed notes by a random amount within an
  octave, weighted toward small shifts; Note Lock snaps the results back
  into the scale (sliced Sampler instruments stay chromatic), nothing is
  erased, the 8th press restores the original notes and the cycle
  restarts, and releasing OPT resets it.
- Added slice spread for the phrase selection's note column (OPT + DOWN):
  the placed notes of every sliced Sampler instrument become a chromatic
  run - the instrument's first note keeps its value and each following
  note rises one semitone above the previous one - with several sliced
  instruments spreading independently, notes on non-sliced instruments
  untouched and no scale snapping applied; the last note the spread writes
  becomes the last note used in note input.
- Added random arp for the phrase selection's note column (OPT + DOWN
  when the selection holds no sliced-instrument note): a random chord
  from the CRD palette - sized so selections of 1-3 notes don't repeat
  it - with its root on a random note of the project scale inside the
  selection's register (any chromatic note on tracks the scale doesn't
  apply to), spread across the placed notes randomized, up, down and
  up+down on consecutive presses; the 5th press restores the original
  notes, the next press picks a fresh chord, the chord name shows in the
  status bar and releasing OPT resets the process. Selections mixing
  sliced and non-sliced instruments spread the slices on the first press
  and hand the remaining notes to the arp from the second press on.
- Expanded the PCM Sample screen into a sample editor: the waveform zooms
  in around the Start/End marker while fine-adjusting and returns to the
  full view on coarse steps.
- Added an independent processing selection (Sel.S/Sel.E) in absolute
  frames with zoom-aware handles, tap-to-copy from the playback markers
  and one-key clear.
- Added destructive process operations — Crop, Normalize, Delete, Silence,
  Fade In, Fade Out — with a one-level toggle undo, running with the audio
  callback suspended.
- Added Save (overwrite with confirm), Save As (name + folder browser) and
  Rename flows backed by a new 16-bit PCM WAV writer, plus a `*` marker
  when the sample in RAM differs from the file on disk.
- Added end-to-end engine tests for the editor workflows and a 32 MB
  large-sample stress case (392 test cases total).
- Compressed the Sample Edit screen into five rows (Region, Select, Slice,
  Process, File) with a taller waveform: the zoom readout and the frame
  count in the format line are gone, Region/Select show START and END
  values side by side, process operations use their full names, and the
  File row cycles Save/Save As with a single GO (Rename removed).
- Rebound the Region and Select rows: Region now owns the playback
  Start/End markers and Select is the processing selection only, and the
  rows swapped places so playback boundaries sit on top.
- Fixed the waveform zoom to a constant one-second window on fine
  adjustments instead of one eighth of the sample; samples that fit inside
  the window keep the full 1:1 view and coarse steps still return to it.
- Made the fine-adjust zoom transient: the waveform zooms in while EDIT is
  held and returns to the full 1:1 view when EDIT is released.
- Stepped the fine adjustment on the Region row by 1 marker unit and on
  the Select row by 15 frames.
- Reset the playback Region to the full sample when a new WAV is loaded
  into a Sampler instrument.
- Seeded the processing selection with the playback Region span when the
  Sample Edit screen is entered - the whole sample with the default
  markers - so process operations act on the region out of the box.
- Added the Reverse process operation, which plays the selected region
  backwards in place (stereo image preserved, length and markers
  unchanged).
- The Sampler instrument (renamed from PCM Sample) now loads 24-bit PCM
  WAV files in addition to 8-bit and 16-bit, and sits at the top of the
  SAMPLE category in the instrument type picker.
- Fixed a build failure on non-ARM hosts: the vendored stmlib dsp header
  selected ARM `vsqrt` inline assembly whenever `TEST` was undefined;
  the guards are now architecture-aware (398 test cases total).

## v0.1.0-prealpha.4 (August 17, 2026)

- Added PCM Sample speed control and one-shot, looping and ping-pong playback.
- Added sample time-stretch and per-track auto-mix controls.
- Refined the shared instrument layout and envelope display.
- Centralized synth model labels and the shared filter/ADSR instrument panel.
- Regenerated the browser bundle for the current tracker sources.

## v0.1.0-prealpha.3 (August 13, 2026)

- Added a continuous `00`–`FF` envelope Shape control, shared by Sample,
  Braids and Plaits voices: logarithmic through linear to exponential.
- Unified tracker RET and KIL note events across AY, Sample, Braids, Plaits
  and Plaits-Alt voices.
- Added live waveform activity displays for Sample, Braids, Plaits and
  Plaits-Alt tracks, with an envelope-level guide.
- Kept Filter, Slope, Cutoff and Resonance grouped in the instrument UI;
  corrected the Project and Settings screen-map indicators.
- Updated the PortMaster package metadata, cover reference and credits to
  Pierre-Emmanuel SURGA.

## v0.1.0-prealpha.1 (August 9, 2026)

- Added the 47 accessible Mutable Instruments Braids models as one instrument type.
- Added one monophonic Braids voice per tracker track with AY/Braids mixing.
- Added 12/24 dB low-pass, band-pass and high-pass filtering plus audio-rate ADSR.
- Added Braids modulation destinations, instrument editing and project persistence.
- Added a native MSYS2 Windows build at 96 kHz and automated Braids integration tests.
- Added a Docker-free WSL2 ARM64 build and PortMaster package for the RG353V.
- Added the `MSCPIT` mixer navigation, fixed its startup crash and added an audio CPU meter.
- Renamed the native project format to `.cct` and the application to ChooChooTracker.
- Added a clean PCM16 mono/stereo Sample instrument with pitch, start/end, volume, ADSR and 12/24 dB LP/BP/HP filtering.
- Added instrument-aware Braids and Sample FX in the existing FX lanes.
- Fixed default linear-pitch initialization and Braids' non-linear octave offset.
- Added PCM8 WAV loading and longer Sample import error messages.
- Skipped unused AY renderers on empty, Braids and Sample tracks.
- Added all 24 Mutable Instruments Plaits engines with Main/Aux blend, filtering, ADSR, modulation, project persistence and instrument-aware FX.
- Added per-track Reverb and Delay sends, a shared Clouds Reverb, and a tick-synchronized filtered ping-pong delay.
- Added `PRO` probability, `MOD` iteration conditions and persistent per-track `SPD` ratios.
- Added Reverb and Delay sub-screen indicators around Mixer in the screen map.
- Added an English ChooChooTracker user manual based on the local ChipNomad and Mutable documentation.
- Renamed the settings label to AY Sample dithering to make its scope explicit.
- Fixed Plaits retriggering on consecutive notes and made VCA envelope retriggers continue from the current level to prevent clicks.

## v1.0.4 (July 12, 2026)

- The all-new modulation system similar to M8 tracker. Use 4 sources to modulate different values.
  - 3 modulation types:
    - ADSR — attack-decay-sustain-release
    - AHD — attack-hold-decay
    - LFO with 10 types: Triangle, Sine, Unipolar Triangle, Unipolar Sine, Ramp Down, Ramp Up, Exp Down, Exp Up, Square, Random
- AY Plus Instrument. It uses synth-like approach to AY with using the oscillator concept.
  - Tone, Noise, and Envelope oscillators
  - Software oscillator with multiple types:
    - Pulse — software square wave with pulse width control (16/256 steps)
    - Sync Tone — hard sync'ish effect for the Tone oscillator
    - Sync Envelope — hard sync for the Envelope oscillator
    - Wavetable — 32-step wavetables (up to 256 waves per project)
    - Tone FM — simple FM for the Tone oscillator
    - Envelope FM — simple FM for the Envelope oscillator
  - All software oscillators support simple FM
- AY Sample Instrument:
  - Load 8/16/24/32 mono/stereo WAVs of any lenth. They will be converted to 8-bit mono and truncated up to 16kb.
- New phrase/table FX commands to control new instruments and the modulation system
- New Wavetable screen (under the Table screen) to edit/load/save AY wavetables
  - Copy/paste wavetables same way as Instruments: **SHIFT**+**OPT** — Copy, **SHIFT**+**EDIT** — Paste
- VSL FX command — volume slide
- Mix Volume scaling was adjusted to make 100% a usable value (values over 65% could previously cause audible distortion)
- VGM Export
- 24TET (quarter-tone) linear pitch table added to the bundled content
- *FIX*: Automatic trimming of text fields to prevent accidental leading and trailing spaces
- *FIX*: Screen Map could disappear after visiting modal screens (character edit, FX selection, etc)
- *FIX*: Arpeggio settings (ARC FX) were not reset on loading a project
- *FIX*: Pitch tables with more than 127 notes worked incorrectly

## v1.0.0 (April 25, 2026)

- *PLATFORM*: Android (phones, tablets, handhelds), Miyoo Mini (Miyoo Ports)
- Press Opt+Left/Right at the Song screen to solo all tracks to the left/right
- Custom font loading
- FX implementation was re-built to match M8 and LSDj behavior
- Confirmation dialog before creating/loading a project is there are unsaved changes
- When entering a note with empty instrument, note preview looks up the instrument above in the track
- *FIX*: Division by zero in auto envelope logic
- *FIX*: You could open table FF for editing which led to unexpected behavior
- *FIX*: Env shape display was incorrect at the instrument screen
- *FIX*: Unexpected behavior when "cutting" values at screens other than Phrase and Table
- *FIX*: Phrase transpose was affecting notes playing from the previous phrase
- *FIX*: THO behavior in tables now match M8
- *FIX*: RET FX was stopping after a single row and didn't work in tables
- *FIX*: Changing vibrato speed over time caused weird phase issues
- *FIX*: Volume column in aux tables didn't work
- *FIX*: Random crash on project load and changing the number of chips
- *FIX*: Loading instruments didn't load the instrument table
- *FIX*: Loading color theme wasn't setting the theme name

## v0.1.0b (January 25, 2026)

- *PLATFORM*: Linux x86_64 package for Linux desktops and Steam Deck
- *BREAKING CHANGE*: PIT now sets offset in semitones. New FIN command sets fine pitch offset
- Vortex Tracker 2 tracks (.vt2) import (by [Pator](https://github.com/paator))
- Gamepad support for desktop builds
- Support QWERTZ and other keyboard layouts for desktop builds (by [koppi](https://github.com/koppi))
- User-definable key mapping with up to 3 physical buttons for each of 8 logical buttons
- Support for 2x and 3x AY/YM chips
- Linear pitch option (pitch tables are defined in cents)
- SNG FX to jump between song positions
- HOP FX now supports conditional loops both in Tables and Phrases
- ARP should work with octaves of any size, not just 12 notes
- Schematic waveform display
- Loop selection: select a range at Phrase, Chain, or Song screens and playback will loop over this range
- Mixer controls in AY instrument screen (tone on/off, noise on/off, env shape)
- LSDJ-style paired rows edit in Groove screen for easier swing creation (by [laamaa](https://github.com/laamaa))
- Triple-tap B on a chain at Song screen to highlight it (useful to visualize song structure)
- Mute/solo tracks (B + Select/Start on Song screen. Release B first to keep mute/solo)
- Clean up of unused instruments, unused/duplicate phrases and chains
- Chain and Phrase screens show asterisk next to chain/phrase number if it's used elsewhere in the song
- B+A on an empty cell at the Song screen now moves the whole column up (same as in LSDJ and M8)
- Project and Instrument save functions now check for empty filename before saving
- AY/YM emulator filter quality setting (lower quality - lower CPU load)
- Looping cursor in the file browser
- Color theme edit, load, and save
- Stems export and starting row for export
- *FIX*: Chip settings were not initialized when loading a project
- *FIX*: UI was monochrome in RG35xx build
- *FIX*: All saved values are correctly reset on loading or creating a new project now
- *FIX*: Multi edit bug on FX columns in phrases and tables
- *FIX*: Project title is lost when you load a project with an empty author
- *FIX*: Chain deep clone created one more copy of a chain

## v0.0.3a (November 22, 2025)

- *PLATFORM*: proper macOS app bundle with the icon
- Added support for different screen resolutions (bonus: Mac Retina display support)
- New font and a convenient bitmap font generator for all screen resolutions from TTF fonts
- Copy/cut/paste functionality on Song, Chain, Phrase, Table screens
- Deep cloning chains (clones both chain and phrases in the chain)
- Edit multiple values in selection mode on Song, Chain, Phrase, Table screens
- Copy/paste instruments
- Save/load instruments
- Vortex Tracker 2 instruments (.vts) import (by [Pator](https://github.com/paator))
- Instrument pool screen with instrument reordering functionality
- Instrument preview on Instrument and Instrument Pool screens (A + Start)
- Additional FX help on FX selection screen
- New TXH effect: same as THO, but for aux table. THO now jumps in the instrument table only.
- Special case for TIC: when TIC is on the last row in the table, it sets the column speed on table start
- Special case for NOA: when NOA value is FF, it stops noise period output in the track/instrument. Convenient for resolving noise period conflicts
- Improved logic for finding next empty chain/phrase - looking for item not yet referenced in the project
- Settings screen with two functions: AY phasing conflict highlight, and Quit button
- Lowercase character entry in all text fields
- Create folder function in the file browser
- Show BPM for tick rate (only for default groove with 6 ticks per phrase row)
- 0.75MHz AY/YM clock option
- Export to WAV and PSG (AY register dump) formats
- *FIX*: Crash on deleting a chain under the playhead during playback
- *FIX*: OFF in note field stopped active track FX
- *FIX*: Playback now stops on project load and song position is reset to start
- *FIX*: Cursor could be drawn incorrectly at some screens
- *FIX*: Chain transpose column color could be wrong
- *FIX*: GGR command was working incorrectly

## v0.0.2a (October 11, 2025)

- *PLATFORM*: PortMaster build
- Project settings screen
- AY/YM chip settings
- Project save/load
- Pitch table save/load
- ARP and ARC effects for arpeggio (by [laamaa](https://github.com/laamaa))
- *FIX*: Random crash on app startup because of audio callback race condition (by [Alexander Kovalenko](https://github.com/alexanderk23))
- *FIX*: Random loss of instrument and volume values in Phrase editor
- *FIX*: App crash when deleting a chain or a phrase under playhead during playback
- *FIX*: Instruments of NONE type now don't output any sound
- *FIX*: PVB started from the lowest pitch offset instead of zero

## v0.0.1a (May 8, 2025)

- *PLATFORM*: pre-2024 Anbernic RG35xx with GarlicOS 1, Windows, macOS
- Core editing functionality
- Project auto save/load
