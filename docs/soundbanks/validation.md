# SoundFont contribution validation

Prepared against upstream `583702d68ee6badf7ad37f7264a1ba9f2b87ab46` on October 14, 2026.
This branch contains the SoundFont feature only; personal ESQ, Beads, MonkSynth,
launcher artwork and deployment configuration are excluded. SoundFont takes the
next upstream instrument ID, 28. Existing upstream instrument/FX IDs are unchanged.
Personal-fork song numbering is different and is not an upstream interchange format.

## Standalone contribution

- Native suite on macOS: 686 cases, 685 passed, one pre-existing M8 import path-case
  assertion failed (113,826,438 assertions, one failure). The same failure was
  reproduced previously against untouched upstream `583702d`: the test expects
  lowercase `samples`, while a case-insensitive filesystem can resolve `Samples`.
  The M8 importer and its test are unchanged here. This is not a clean full-suite pass.
- Focused SoundFont suite: 40 cases and 707,081 assertions passed.
- Stretch allocation lifetime check: 32 cycles, zero retained allocation bytes.
- Parser/voice address and undefined-behavior sanitizer suite: 23 cases and
  3,773 assertions passed without sanitizer findings.
- Fresh Emscripten WebAssembly build passed using the stock Web Makefile.
- Headless Chrome 155: USER ZIP import, keeping existing files, traversal rejection,
  persistence after reload, nonzero finite audio, collection browser, direct editor
  and instrument/song roundtrip passed with no JavaScript runtime errors.
- Browser storage queue tests: four passed. Source and checked-in wrapper HTML match.

The browser test uses generated fixtures and an isolated profile, not the user's
live browser storage. Automated audio sampling is not a speaker listening test.

## Integration evidence from the personal daily

The same dedicated SoundFont parser, source, voice, browser and library code is
used by personal daily `7720a36`; shared tracker integration is adapted here to
stock upstream types and layout. The personal build contains other experiments,
so its results are supporting evidence rather than tests of this exact branch.

- R36H: startup and SoundFont UI/load/failure/editor/save checks passed.
- 54 note-transition/KILL playback cases passed across nine installed samples.
- All 411 installed loose WAV/AIFF files loaded; seven compatibility warnings,
  zero failures. The larger local reference scan loaded 1,670 files with zero
  failures. These counts do not establish every preset's sonic fidelity.
- Owner reported an hour of R36H playtesting before requesting review.
- Personal web `7720a36`: production build and the same browser regression passed;
  all 11 published runtime file hashes matched its build manifest.

## Remaining limits

This is a supported subset of SF2/SF3, SFZ and legacy EXS24, not a complete
replacement for their original players. See [format coverage](README.md#format-coverage)
and the user manual. Large instruments can exceed the 64 MiB decoded PCM budget;
there is no streaming. Custom modulation and some articulation/envelope/routing
features remain unsupported. Physical Safari/iOS, Windows and Android behavior
have not been established by this review. Private libraries are not included.
