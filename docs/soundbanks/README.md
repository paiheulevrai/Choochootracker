# SoundFont instruments

Choose SAMPLE → SOUNDFONT and place banks in `instruments/USER/soundfont`.
The engine reads SF2/SF3, SFZ and legacy EXS24 instruments, standalone WAV/AIFF
samples, and ZIP collections containing those formats. Sample libraries are
external user files; none are included in this contribution.

Rescan indexes metadata. Preset opens the current sound in a two-column
collection/preset browser. Physical folders stay navigable: Right or EDIT/A
enters a folder, Left returns through the panels and parent folders, and OPT
goes back. In the preset panel, Right pages by 15 entries without loading;
EDIT/A loads the highlighted preset. PLAY controls song playback while browsing.
EDIT+Left/Right cycles in matching nested folder order; EDIT+PLAY auditions.

Collections use their full panel height. The selected preset stays below the
eight track previews, and long names scroll after one second of idle selection.
The third row reports the current preparation stage. Supporting sample folders
stay with their mapped instruments without hiding unrelated loose samples.
Empty or `(null)` EXS header names fall back to the instrument filename.
See [the manual](../USER_MANUAL.md#soundfont-collection-browsing) for controls.

Only the selected preset is decoded. Failed presets remain selected and silent,
showing Error and its reason at the bottom of Instrument; cycling can continue.
Changing source files requires Rescan. The source audio and mappings are never
rewritten. Renaming bank paths requires reselecting the preset in saved songs.

## Storage and limits

Portable builds use `instruments/USER/soundfont` relative to their data directory.
Other platforms use the existing application data-directory helper.
`CHOOCHOO_SOUNDBANKS` overrides that root. The default decoded PCM cache budget
is 64 MiB; `CHOOCHOO_SOUNDBANK_CACHE_MB` accepts 8–1024 MiB. Budget headroom is
also needed for the tracker and temporary decode data. There is no disk streaming.
Metadata indexing can succeed for an instrument too large to prepare for playback.

Native preparation uses one background worker. Web preparation yields through
Asyncify; imported files persist through the existing browser storage mechanism.
Import sound banks accepts files/ZIPs; Import bank folder preserves relative sample
paths. Import USER ZIP accepts `instruments/USER/` or `USER/`, preserves existing
files, and limits new uncompressed imports to 1 GiB total and 512 MiB per file.
Browser quota and memory limits still apply. Keep independent song/library backups.

The disposable `.choochoo-index-v3` metadata cache tracks source and dependency
changes. ZIP members are read individually; compressed members use temporary
files deleted after use. Limits: 2 GiB uncompressed member, 64 MiB central
directory, 200,000 members. ZIP64 is supported; encrypted archives, invalid CRCs,
escaping paths and nested ZIP traversal are rejected or excluded.

Relative sample lookup stays within the configured root/archive, handles case and
legacy EXS path hints, and rejects equally good ambiguous matches. A missing bank
or sample produces an error instead of substituting a different instrument.

## Format coverage

| Format | Implemented | Explicit limits |
| --- | --- | --- |
| SF2 | Bank/program IDs, zones, key/velocity ranges, root/tuning, offsets, gain/pan, stereo sides, loops, amp ADSR/delay/hold, low-pass filter, exclusive classes | Custom preset/instrument modulation routes, modulation envelopes/LFOs, key-scaled envelope times, fixed key/velocity and chorus/reverb sends are not implemented; diagnostics retain unsupported-feature warnings. 24-bit extensions play at 16-bit precision. ROM samples fail. |
| SF3 | SoundFont version 3 mono Ogg Vorbis sample streams, including tested inclusive/exclusive sample-end conventions | Compressed stereo-per-record and Opus fail. No assumption that arbitrary SF2-compatible compression is supported. |
| SFZ | control/global/master/group/region inheritance, bounded includes/macros, default_path, key/velocity mappings, pitch/gain/pan, offsets, loops/one-shot/reverse, amp envelope, low-pass filter, choke, round-robin, random selection, velocity curves/fades, attack/release regions | Default channel/articulation only. Live keyswitches, CC-triggered regions, sustain-pedal logic, dynamic envelopes, non-low-pass filters and additional opcodes warn. Unsupported trigger/loop types fail. Not full SFZ. |
| EXS24 | Legacy endian variants, instrument name, zone/sample records (336/592/600), key/velocity/root/tuning, gain/pan, offsets, loops/reverse, sample ordinal references | Group synthesis/envelopes, instrument parameter blocks, fades, loop crossfades and routing warn. Missing referenced samples fail explicitly. |
| WAV | PCM 8/16/24/32, float32, supported extensible PCM/float, mono/stereo, smpl root/fraction/forward loop | Standard ping-pong/backward loops fail. Exactly one-past-end forward loops and a narrowly recognized zero byte counted as PCM padding are accepted with warnings; other invalid boundaries fail. Unknown sampler-specific loop types (32+) warn and leave the embedded loop disabled; an explicit SFZ/EXS zone loop remains authoritative. Diagnoses a narrowly recognized omitted odd-chunk pad. |
| AIFF/AIF | PCM and AIFC NONE/twos/sowt, rate/channels, INST root/detune and MARK sustain loop | Other compression and ping-pong loops fail. Release-loop and advanced instrument metadata are not mapped. |

Referenced Ogg Vorbis samples are also decoded for SFZ. Standalone OGG, FLAC and
MP3 are not instrument types. SFPack is excluded: research did not establish a
permissively licensed, independently usable decoder/specification. SBK, sfArk,
Kontakt, GigaStudio, HALion and Reason formats are outside this feature.

The renderer uses existing envelopes, filter, tracker mixer, effects and note
routing. Voices are fixed per tracker/chord slot, with eight sample layers per
voice. Retriggering steals that slot deterministically. Preparation rejects
instruments that can exceed the layer bound; there are no note-time allocations,
file operations, archive decoding or format parsing. Pitch/volume MOD sources and
instrument/track pan use existing routes. **Edit** beside the active Preset opens
the direct instrument editor for tuning, cutoff, resonance and envelope
delay/attack/hold/decay/sustain/release. **Bank** retains each zone's original value;
explicit overrides apply to all layers and save with the instrument/project.
Tone and tuning apply live; delay/attack/hold apply at note-on. Decay, sustain and
release can update active voices; note OFF starts release. Clear restores
the selected default. Source banks are unchanged. Eight contextual Lane FX are described in the user manual.

### Choosing and translating SoundFonts

An SF2/SF3 file can contain a complete bank of presets. A custom modulation route
can affect one preset or a shared instrument layer, rather than every preset.
The current `pmod`/`imod` detection is file-wide; it cannot establish that the
selected preset is affected. The generic warning is therefore explained in the
manual instead of repeated on every Instrument page. Unsupported routes remain
unimplemented and detailed diagnostics retain them.

See the [SoundFont compatibility table in the user manual](../USER_MANUAL.md#soundfont-compatibility-banks-presets-and-modulation)
for supported mappings, omitted programming, practical selection/conversion
advice and the distinction between loading successfully and reproducing the
original sound. This applies to both SF2 and SF3. Renaming or recompressing a bank
does not translate unsupported modulation into supported behavior.


## Build and verification

The normal native and Web build recipes discover the new sources automatically.
Run `make -C tracker -f Makefile.test -j4` for the native suite.
`make -C tracker -f Makefile.soundbanks soundbanks-test` runs the parser/voice
fixtures under address and undefined-behavior sanitizers.
`tools/test_web_soundbanks.py` uses Playwright against a rebuilt `web/dist` or
`WEB_TEST_URL` to check import preservation, playback, controls and save/reload.
See [validation](validation.md) for the exact evidence and remaining limitations.

## Dependencies and fixtures

The renderer uses existing tracker envelopes, filters and mixer code. ZIP handling
reuses miniz. `stb_vorbis.c` is pinned to `nothings/stb` commit
`f58f558c120e9b32c217290b80bad1a0729fbb2c` under its MIT option; its notice is
packaged in `licenses/stb_vorbis.txt`. EXS offset research used Matt Allan's MIT
renoise-exs24 parser; its notice is in [EXS-parser-LICENSE.md](EXS-parser-LICENSE.md)
and the packaged licenses. No external SoundFont synth is required.

Fixtures are generated by `tools/soundbanks/make_fixtures.py`. SF3 fixtures include
a small MIT-licensed FluidR3Mono sample with provenance and full license alongside
them. These are test inputs, not a bundled preset collection.
