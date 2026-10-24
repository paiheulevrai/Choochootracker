# Dirtywave M8 songs (.m8s)

ChooChooTracker can import an M8 song and export a limited M8 song. Only the
song structure and the notes are handled: the M8 has instruments, tables,
mixer and effect settings that have no equivalent here. This page describes
what is read, what is written, and the file layout, for anyone maintaining the
code or checking a conversion.

- Import: `tracker/src/import/import_m8s.cpp` (`projectLoadM8S`), reached from
  **Project > Load**.
- Export: `chipnomad_lib/export/export_m8s.cpp` (`projectExportM8S`), reached
  from the **M8S** row of the Export screen. That row exists in desktop builds
  only (`DESKTOP_BUILD`): picking a template file is impractical on mobile,
  web and handheld targets. The export code itself is compiled everywhere.
  The import is available on every platform.
- Shared layout constants: `chipnomad_lib/m8s_format.h`.
- Tests: `tracker/tests/test_import_m8s.cpp`.

## What is converted

| M8 | ChooChooTracker |
| --- | --- |
| Song rows (256 x 8 tracks) | Song rows, same row and track, same chain number |
| Chains (255 x 16 steps) | Chains, same number; phrase and transpose per step |
| Phrases (255 x 16 steps) | Phrases, same number; note, velocity, instrument number |
| Song name (12 chars) | Project title |
| Tempo (BPM) | Tick rate, assuming the default 6-tick groove: `tickRate = 2.5 x BPM` (120 BPM is 48 ticks/s) |
| Note | Pitch table index `note - 12` (see below) |
| Note off (`0x80` and above) | Note off |
| Velocity (`0xFF` = empty) | Phrase volume, clamped to 127 |
| Instrument | Instrument slot with the same number (import: see below) |

Not converted, in either direction: phrase FX, tables, grooves, scales, EQ,
mixer, effect settings, MIDI mapping, and every instrument parameter.

### Pitch

An M8 note value is a MIDI note number. Pitch table index N of the default
tables is MIDI note 12 + N, the same rule `import_midi.cpp` and
`export_midi.cpp` use, so import subtracts 12 and export adds 12 and the
sound is the same. The M8 labels octaves two higher than ChooChooTracker
(MIDI 60 is `C-6` on the M8, `C-4` here): that is only a naming difference.
This was found by comparing the same phrase on both screens and by ear, not
from Dirtywave documentation, so treat the exact reason as unknown. Samples
are the exception to "same note, same sound": see Sampler below.
M8 notes below 12 (under 16 Hz) clamp to the lowest index.

## Import

- Accepts firmware 2.x to 4.x. Other versions, bad magic or a short file are
  rejected with an on-screen message.
- Every instrument referenced by a phrase is created in the same slot, named
  after the M8 instrument (or `M8 xx` when it has no name).
- MacroSynth (type 1) becomes a Braids instrument. Shapes 0-46 are Braids
  models 0-46 (same order). Timbre and color are scaled by 129 (8 to 15
  bit). Filter type LP/ZDF LP, BP and HP/ZDF HP map to the Braids filter
  modes; off and band stop disable it. Cutoff is mapped exponentially from
  20 Hz to 20 kHz (approximate, the M8 curve is not documented), resonance is
  copied, pan is copied. Degrade, redux, amp, limiter, sends, modulators and
  the envelope are not converted.
- Sampler (type 2) becomes a Sample instrument. The `.m8s` only stores the
  path of the WAV on the M8 SD card (e.g. `/Samples/Drums/Kick.wav`). The
  importer looks for it under the folder of the `.m8s` and each of its
  parents (so a copy of the SD card layout is found), then under the sample
  folder of the app (the one of the last sample you loaded) and its parents.
  At each place it tries `Samples/Drums/Kick.wav`, `samples/Drums/Kick.wav`
  (Linux is case sensitive), `Drums/Kick.wav`, and the bare `Kick.wav` in the
  song folder and the sample folder themselves. When found the WAV is loaded;
  otherwise the M8 path is kept in the instrument and it stays silent until
  you pick the file (the instrument screen shows the end of that path).
  Converted: start and length (end = start + length, `FF` = to the end), loop
  (forward loop modes to loop, ping-pong modes to ping-pong; reverse is not
  available), filter and pan as for MacroSynth. Slice, loop start, degrade,
  pitch and the rest are not converted. Here a sample plays at its original
  pitch on index 48 (MIDI 60), the M8 on its own C-4 (MIDI 36), so imported
  samples get +24 semitones in their Pitch setting. Checked by ear against a
  real song.
- FMSynth (type 4) becomes a Genesis FM instrument. The 12 M8 algorithms map
  to the 8 YM2612 ones (0-7 directly, 8-11 a guess). Ratio (clamped to 15)
  and level are kept per operator; feedback is the largest of the four, scaled
  to 0-7. Envelopes are a fixed sustained shape, operator waveforms, mod
  amounts and fine ratios are lost. Expect a rough starting point, not the
  same sound.
- WavSynth (type 0) and HyperSynth (type 5, firmware 3.0+) become aChChid.
  WavSynth pulse shapes become the square wave and SAW the saw wave; sine,
  triangle, noise and wavetable shapes use the Braids models SINE-TRI,
  FILTER-NOISE and WAVETABLES (timbre = scan). HyperSynth becomes SAW-SWARM
  (timbre = swarm, color = width). Filter cutoff and resonance are carried
  over; the rest is lost.
- MIDIOut (type 3) becomes a MIDI Out instrument: channel, bank select
  (MSB), program and the first four custom CC numbers. The port is dropped.
- Any other type (External) becomes a default AY instrument. Pick
  real sounds afterward.
- The song-level transpose of the M8 is ignored.

## Export

The M8 file is built from a **template**: an existing `.m8s` picked by the
user. The song rows, chains, phrases, tempo, song transpose (reset to 0) and
title of the template are overwritten; every other byte, including all
instruments, is copied untouched. This keeps the file loadable without
reimplementing every M8 section. The template is remembered for the session;
**EDIT + OPT** on the row forgets it.

- Phrase FX columns are written empty.
- Instrument numbers are exported as they are. Braids and Sample instruments
  used by a phrase are converted back (see below); every other slot plays
  whatever the template holds.
- Braids becomes a MacroSynth: model = shape, timbre and color divided by 129,
  filter mode, cutoff (inverse of the import curve, approximate), resonance,
  pan and name. Sample becomes a Sampler: path `/Samples/<file name>` (copy the
  WAV into that folder on the M8 card yourself, it is not exported), loop mode,
  start, length, filter, pan and name. Sample pitch and slices are not
  exported. An M8 instrument has many parameters we do not know, so the record
  is a copy of the template's instrument of the same type (the slot itself if
  it already has that type, otherwise any other slot). If the template has none
  of that type the slot is left as it is. Other parameters keep the template
  values.
- The M8 has 255 phrases: a song whose chains reference phrase 255 or higher
  fails to export rather than losing notes.
- An exported file has not been verified on real M8 hardware.

## File layout used

All offsets are from the start of the file and are the same in firmware 2.x
to 4.x for the parts below.

| Offset | Size | Content |
| --- | --- | --- |
| `0x00` | 10 | `M8VERSION\0` |
| `0x0A` | 2 | Version: byte 0 = `(minor << 4) \| patch`, byte 1 low nibble = major |
| `0x0E` | 128 | Song directory (text) |
| `0x8E` | 1 | Song transpose |
| `0x8F` | 4 | Tempo, little-endian float, BPM |
| `0x93` | 1 | Quantize |
| `0x94` | 12 | Song name |
| `0x2EE` | 2048 | Song: 256 rows x 8 tracks, chain number, `0xFF` empty |
| `0xAEE` | 36720 | Phrases: 255 x 16 steps x 9 bytes (note, velocity, instrument, 3 x FX command + value) |
| `0x9A5E` | 8160 | Chains: 255 x 16 steps x 2 bytes (phrase, transpose; `0xFF` phrase = empty). Transpose is a signed semitone offset |
| `0x13A3E` | 27520 | Instruments: 128 x 215 bytes; byte 0 is the type, the name starts at byte 1 (12 chars). Offsets from the type byte: MacroSynth shape 18, timbre 19, color 20, filter type 23, cutoff 24, resonance 25, pan 28; Sampler play mode 18, start 20, length 22, filter type 24, pan 29, path 0x57 (128 chars); FMSynth algo 18, ratio/fine pairs from 23, level/feedback pairs from 31; MIDIOut channel 16, bank 17, program 18, CC number/value pairs from 22; HyperSynth swarm 27, width 28, filter type 30; WavSynth shape 18, scan 22, filter type 23 |

Empty note, velocity and instrument are `0xFF`. An empty FX command is `0xFF`.

## References

The layout was worked out from sample files and checked against two
independent parsers:

- [AlexCharlton/m8-files](https://github.com/AlexCharlton/m8-files) (Rust):
  offsets, version decoding, step layout, 215-byte instruments.
- [whitlockjc/m8-js](https://github.com/whitlockjc/m8-js) (JavaScript).

[laamaa/m8c](https://github.com/laamaa/m8c) is a client for the M8 hardware
over USB and does not read song files.
