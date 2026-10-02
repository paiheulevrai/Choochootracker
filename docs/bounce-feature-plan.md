# Bounce Selection to Audio — Implementation Plan

**Status:** Implementation complete 2026-10-01. All 292 tests pass (277 baseline + 15 new bounce tests); UI screens syntax-check clean.
**Baseline:** 277/277 tests pass (`cd tracker && make -f Makefile.test -j4`).
**Related research docs:** `docs/` + agent memory: `audio-export-architecture.md`, `selection-architecture.md`.

---

## 1. Feature summary (original request, verbatim)

> "I want to introduce the feature: bounce selection to audio. You can select multiple notes, chains, phrases, by clicking SHIFT + B. I want to utilize already existing feature to export project audio to .wav file, name it (use the existing scheme for naming the track but duplicate it for the purpose of this feature - we will add some other options later) and save it in the Samples/Exports directory. and make it export only the selected: chains, phrases, note patterns (it should also work with multiple selected tracks in a song view) I also want to be able to choose whether i want the export to include audio tail beyond the selected area, or cut it right at the end of it. [...] The button combination would be double-tap A when the selection is active and no other buttons are held"

**Simplification (supersedes tail option):** "Or maybe - skip the tail feature for now, just kill notes at the end of the selected sequence"

## 2. Confirmed decisions (user answers, 2026-10-01)

| # | Question | Answer |
|---|----------|--------|
| 1 | WAV format for bounce | **Reuse last settings used on the Export screen** (sample rate + bit depth indexes) |
| 2 | Song view: unselected tracks muted (solo the selection)? | **Confirmed** |
| 3 | Chain/Phrase view: bounce only currently viewed chain/phrase on current track? | **Confirmed** |
| 4 | Song rows bounce FULL chains (no mid-chain cut from song view)? | **OK** |
| 5 | Save location | **`Samples/Export`** (folder named `Export`, singular) for now |

Trigger: **double-tap A** (`keys == keyEdit && tapCount == 2`, exactly keyEdit — no other buttons held) while selection is active.

---

## 3. Architecture (4 pieces)

### 3.1 Engine: stop-at-position ("kill notes at end")

`PlaybackState` gets a `stopRange` field, same shape as existing `LoopRange`:

```c
// playback.h (next to LoopRange)
typedef struct StopRange {
  int enabled;
  int level; // 0 = song, 1 = chain, 2 = phrase
  int startSongRow, endSongRow;
  int startChainRow, endChainRow;
  int startPhraseRow, endPhraseRow;
} StopRange;
```

In `moveToNextPhraseRow` (playback.cpp:909) there are THREE loop-range checkpoints. At the same three places, when `stopRange.enabled` and the track is AT the end boundary, instead of advancing/wrapping: `resetTrack(state, trackIdx); stopped = 1;`

- **Phrase level (level 2)** — before `track->phraseRow++` (~line 916): if `phraseRow == endPhraseRow` → resetTrack + stop.
- **Chain level (level 1)** — after phrase overflow, before next chain row (~line 940): if `chainRow == endChainRow` → resetTrack + stop.
- **Song level (level 0)** — before advancing song row (~line 972): if `songRow == endSongRow` → resetTrack + stop.

Key semantics:
- `resetTrack` (playback.cpp:71) sets `songRow = EMPTY_VALUE_16` → `nextFrame` returns early → all `update*Voices` kill voices → **instant silence**. This is the SAME mechanism as a track reaching song end — exactly "kill notes at end of selection".
- Apply stopRange **unconditionally** (do NOT require `track->loop == 1` like LoopRange does — LoopRange semantics don't fit; stopRange is exporter-only so no live-playback impact).
- `playbackInit` must set `stopRange.enabled = 0`.
- **Edge cases:** SNG positive jump and HOP can jump PAST the end boundary. Decision: a jump landing past the end stops the track (consistent with "cut at end"). Check after jump in `readPhraseRow` SNG handler (~line 600) and HOP handler (~line 630), or re-check boundary after any position change. Refine during implementation; add tests.
- Do NOT reuse `LoopRange` itself: it requires `track->loop==1`, it LOOPS instead of stops, and `playbackQueuePhrase` refuses to queue when `loopRange.enabled`.

### 3.2 Start-at-position support

- **Song bounce:** `playbackStartSong(state, startRow, 0, 0)` — exactly what base `Exporter` ctor already does. No change needed (full chains from their start, per decision #4).
- **Chain bounce:** `playbackStartChain(state, trackIdx, songRow, chainRow, loop)` (playback.cpp:1147) already takes chainRow — start at selection start row. Note: it refuses if `playbackIsPlaying()` — fine in fresh exporter state.
- **Phrase bounce:** `playbackStartPhrase` (playback.cpp:1162) always queues `phraseRow = 0`. `PlaybackTrackQueue.phraseRow` field EXISTS and is consumed in `playbackNextFrame` (`track->phraseRow = track->queue.phraseRow`). **Add a startPhraseRow parameter** (or new `playbackStartPhraseAt`) that sets `queue.phraseRow = startPhraseRow`. Groove starts at row 0 — acceptable.

### 3.3 `ExporterSelectionWAV` (chipnomad_lib/export/)

Subclass of `ExporterWAV`. The base `Exporter` already renders into an isolated `ChipNomadState` with a shallow-copied project (`ownsProjectResources = 0`) — bounce never touches the live project.

**Constructor trick (avoids refactoring ctors):** call `ExporterWAV(path, project, 0, sampleRate, bitDepth, mixVolume, false)` normally (it starts song playback on the private state, but nothing has rendered yet), then in the subclass ctor body:
1. `playbackStop(&chipnomadState->playbackState)` — resets all tracks/queues.
2. Set `trackEnabled[]` per selection mask (song bounce: mutes unselected tracks — same mechanism as stems export; mixer-only gate, sequencer still advances).
3. Set `stopRange` on the private playback state.
4. Start only the selected track(s): `playbackStartSong` per selected track (song level), or `playbackStartChain`/`playbackStartPhraseAt` for the single current track (chain/phrase level).

Spec passed from UI — define in `export.h`:

```c
struct ExportSelection {
  int level; // 0 song, 1 chain, 2 phrase
  int startSongRow, endSongRow;
  int startChainRow, endChainRow;
  int startPhraseRow, endPhraseRow;
  uint8_t trackMask; // bit per track (song level)
};
```

`next()` / `finish()` / `cancel()` inherited UNCHANGED:
- `next()` renders 1s chunks via `chipnomadRender`; when all tracks stop, `chipnomadRender` zero-fills the remainder and returns < sampleRate → `next()` writes the partial final chunk FIRST (writeSamples happens before the -1 check) then returns -1. `finish()` rewrites the WAV header with real `totalSamples`. So exact-length output with hard cut falls out naturally.
- Empty selection / no track starts → first `next()` returns -1 → valid 0-length WAV. Acceptable.

### 3.4 UI trigger (tracker/src/screens/screens.cpp)

In `inputSelectMode` (~line 548) there is currently **NO handler for keyEdit alone** — `keys == keyEdit && tapCount == 2` is a free slot.

- Gate to bounceable screens: `screen->getLoopRange != NULL` (song/chain/phrase have it; table/groove/export have NULL). This also hands us the bounds for free.
- Bounce spec = the selection's own loop range: `screen->getLoopRange(screen)` returns `LoopRange{level, start/end SongRow/ChainRow/PhraseRow}` — start corner = min, end corner = max. Same values needed for start position AND stop boundary.
- Song-level track mask: from selection columns = tracks (`getSelectionBounds` cols, or `selectedTrackBounds()` used by mute/solo).
- Chain/phrase current track index: resolve from the screen's context pointers (see §6 open details).
- After triggering: **switch to the Export screen** (`screenSetup(&screenExport, 0)`) so the existing `draw()` loop drives `currentExporter->next()` once per frame, reusing OPT-cancel, progress message, and Android `fileExportDocument` / web download push. Do NOT duplicate that machinery on song/chain/phrase screens.
- Add a `currentExportIsBounce` flag in screen_export.cpp to show "Bouncing... %ds. OPT to cancel" instead of "Exporting...".
- Decide during implementation whether to exit select mode on trigger (recommended: exit, like copy does).

### 3.5 Naming & directory (screen_export.cpp)

`generateBouncePath(char* outputPath, int maxLen)` — duplicate of `generateExportPath` collision logic (`_001`..`_999`), but:
- Base dir: `appSettings.samplePath + PATH_SEPARATOR_STR + "Export"` (desktop: `samples/Export`; Android: `<workspace>/samples/Export`; WEB: `/user/samples/Export/<name>.wav`).
- Filename: `appSettings.projectFilename` + `.wav`.
- Create the directory before starting: `fileCreateDirectory()` (corelib_file.cpp:83, single-level mkdir 0755) + `fileDirectoryExists()` check. **Verify whether `samplePath` itself is auto-created at startup** (common.cpp:55-90 sets defaults; if not auto-created, create both levels).

### 3.6 WAV settings reuse (decision #1)

`currentSampleRateIndex` / `currentBitDepthIndex` are `static` in screen_export.cpp. Add accessors in screen_export.cpp, declared in screen_export.h:

```c
int exportGetLastSampleRate(); // sampleRates[currentSampleRateIndex]
int exportGetLastBitDepth();   // bitDepths[currentBitDepthIndex]
```

Bounce uses these + `appSettings.mixVolume`. Quality forced to `ChipNomadQuality::best` (existing ExporterWAV behavior).

---

## 4. File-by-file change list

| File | Change |
|------|--------|
| `chipnomad_lib/playback.h` | `StopRange` struct; `stopRange` field in `PlaybackState`; `playbackStartPhrase` phraseRow param (or new fn); optional `playbackSetStopRange` |
| `chipnomad_lib/playback.cpp` | `playbackInit` clears stopRange; 3 stop checkpoints in `moveToNextPhraseRow`; SNG/HOP jump-past-end handling; phraseRow start support |
| `chipnomad_lib/export/export.h` | `ExportSelection` struct; `ExporterSelectionWAV` class decl |
| `chipnomad_lib/export/export_wav.cpp` (or new `export_selection.cpp`) | `ExporterSelectionWAV` ctor: playbackStop → trackEnabled → stopRange → per-track starts |
| `tracker/src/screens/screen_export.{h,cpp}` | `generateBouncePath`; `exportGetLastSampleRate/BitDepth`; `currentExportIsBounce` flag + message; dir creation |
| `tracker/src/screens/screens.cpp` | `inputSelectMode`: double-tap-A handler → build `ExportSelection` from getLoopRange + track bounds → create `ExporterSelectionWAV` into `currentExporter` → `screenSetup(&screenExport, 0)` |
| `tracker/src/screens/screen_song.cpp` / `screen_chain.cpp` / `screen_phrase.cpp` | Only if needed: expose current track index / selected track bounds helper |
| `tracker/tests/test_bounce*.cpp` (new) | See §6 |

## 5. Per-screen bounce semantics

| Screen | Region | Tracks | Start | Stop |
|--------|--------|--------|-------|------|
| Song | selected song rows (full chains) | selected track columns only (others muted via trackEnabled) | `playbackStartSong(startRow, 0, 0)` per selected track | stop when songRow would advance past endSongRow |
| Chain | selected rows of viewed chain | current track only | `playbackStartChain(songRow, chainRow=startRow)` | stop when chainRow would advance past endChainRow |
| Phrase | selected rows of viewed phrase | current track only | `playbackStartPhraseAt(..., phraseRow=startRow)` | stop when phraseRow would advance past endPhraseRow |

Known inherent behaviors (accepted):
- Song bounce: a selected track whose cell AT startRow is empty never starts (song-mode semantics).
- Phrase rows have no empty marker; starting mid-phrase is handled by the phraseRow start param, not by data truncation. Do NOT truncate the project copy (breaks HOP/THO absolute row references; phrase-level truncation leaves silent rows).

## 6. Open details to resolve during implementation

1. How chain/phrase screens expose the current track index (check `screen_chain.cpp` / `screen_phrase.cpp` context pointers — likely shared `*pSongTrack` or equivalent).
2. Whether `samplePath` is auto-created at startup (affects Export dir creation; `fileCreateDirectory` is single-level).
3. Confirm table/groove screens have `getLoopRange == NULL` (bounce gate).
4. Exact placement of stopRange checks relative to live-queue actions in `moveToNextPhraseRow` (put stop checks before live-action handling at each level, or after — pick and test).
5. Exit select mode after trigger: recommended yes.
6. SNG/HOP past-end semantics: stop the track (chosen), verify with tests.

## 7. Test plan (tracker/tests/, auto-wildcarded by Makefile.test)

- **stopRange song level:** track stops (songRow==EMPTY_VALUE_16, mode stopped) after endSongRow; earlier rows render; multi-track: only past-boundary behavior per track.
- **stopRange chain level / phrase level:** same at respective levels.
- **Notes killed at end:** voice killed → rendered audio silent after boundary (render via chipnomadRender into buffer, assert trailing zeros).
- **Start-at phraseRow:** `playbackStartPhraseAt` starts at correct row (use `readPhraseRowDirect`-style assertions like existing tests).
- **SNG/HOP past end:** track stops instead of playing past boundary.
- **ExporterSelectionWAV:** output duration ≈ expected (rows × groove ticks × samples/tick; 6 ticks/row default groove, 24 ticks = 1 beat); unselected tracks silent in mix; valid WAV header (dataSize == totalSamples × 2 × bytes/sample).
- Baseline: 276/276 must stay green.

## 8. Verification commands

```bash
# Tests (from workspace root):
cd tracker && make -f Makefile.test -j4
# Filter: ./build/tests/run_tests -tc="*bounce*"

# UI syntax check (UI screens NOT in test build — ALWAYS do this after UI edits; from tracker/):
c++ -std=c++17 -fsyntax-only -Isrc -Isrc/screens -Isrc/corelib -I../chipnomad_lib \
  -I../chipnomad_lib/chips -I../chipnomad_lib/synth -I../chipnomad_lib/external \
  src/screens/screens.cpp src/screens/screen_export.cpp
```

## 9. Key codebase facts (research digest)

**Input:** `corelib_input.h:23-26` keyEdit=0x10 (A), keyOpt=0x20 (B), keyPlay=0x40, keyShift=0x80. "SHIFT+B" = `keyShift|keyOpt`. Double-tap: app.cpp:479-492 `doubleTapMask = keyEdit|keyOpt|keyUnmapped|keyPlay`; same button within `appSettings.doubleTapFrames` → tapCount++. `appInput` (app.cpp:216) passes FULL keys bitmask → "no other buttons held" = `keys == keyEdit` exactly. Existing double-taps: screens.cpp:472 (keyEdit×2 → CellEditAction::doubleTap in normal mode), screen_song.cpp:407 (keyOpt×2), screen_ay_wavetable.cpp:451.

**Selection:** enter `keyShift|keyOpt` in `inputNormalMode` (screens.cpp:426+); `inputSelectMode` (548+) handles copy (OPT), cut (OPT+EDIT), clones (SHIFT+EDIT), multi-edit (EDIT+dir) — keyEdit alone unhandled. `getSelectionBounds` (screens.cpp:704) = min/max anchor vs cursor. `screenInput` (669) dispatches. Song selection columns = TRACKS. Per-screen `getLoopRange`: song level 0 / chain level 1 / phrase level 2, start=min corner, end=max corner.

**Playback:** `PlaybackTrackQueue{mode,songRow,chainRow,phraseRow,loop,liveAction}` — phraseRow field exists, always set 0 by start fns. `moveToNextPhraseRow` (playback.cpp:909-1054): 3 loop checkpoints (phrase ~916 pre-increment, chain ~940 post-overflow, song ~972 pre-songRow-advance), all require `loopRange.enabled && track->loop`, they LOOP not stop. Song end: chainRow overflow → songRow+1; empty cell / >= MAX_LENGTH → loop? scan back : songRow=-1 → resetTrack + stopped. `resetTrack` (71) → songRow=EMPTY_VALUE_16, note.instrument=EMPTY_VALUE_8, killed flags 0 → voices killed instantly. `handleNoteOff` (281) = envelope RELEASE (not instant); `fxKIL` = instant kill. `playbackNextFrame` (1290+) returns `!hasActiveTracks`; queue consumed at top (1306-1325): `track->phraseRow = track->queue.phraseRow`. `trackEnabled[]` gates ONLY mixer render (chipnomad_lib.cpp:771,791,814); sequencer always advances. `playbackStop` (1271) resets all tracks + queues + chip envShape.

**Render:** `chipnomadRender(state, buffer, samples)` (chipnomad_lib.cpp:844) accepts any chunk size; renders tick-sized chunks (`frameSampleCounter += sampleRate/tickRate`); zero-fills + returns early when `advancePlaybackFrame` reports all stopped; returns `samples - samplesLeft`.

**Export:** `Exporter` base (export.h) ctor `(Project*, int startRow)` → private ChipNomadState, shallow project copy, `playbackInit` + `playbackStartSong(startRow,0,0)`. `ExporterWAV(path, project, startRow, sampleRate, bitDepth, mixVolume, stems=false)`: 44-byte header (audioFormat 3 = float32 when bitDepth 32), 1s renderBuffer, `chipnomadInitChips` + quality best. `next()`: render 1s → writeSamples → if rendered < sampleRate: stems? next track : return -1. `finish()` rewrites header with `totalSamples`. `cancel()` deletes files. `writeSamples` quantizes 16/24-bit int (clamped) / 32-bit raw float; `totalSamples += samples`.

**Export UI:** screen_export.cpp — `currentExporter` global driven from `draw()` (one `next()` per frame); OPT cancels; all other input blocked during export; Android pushes via `fileExportDocument(path, "audio/wav")`; web via `webDownloadExportFile`. `generateExportPath` (line 265): `<projectPath>/<projectFilename>.wav`, collisions `_001`..`_999`; WEB `/user/exports/`. Settings: sampleRates {44100,48000,88200,96000}, bitDepths {16,24,32} as statics.

**Paths:** `appSettings.samplePath` (common.h:51) — desktop `"samples"`, Android `<workspace>/samples`, WEB `/user/samples` (common.cpp:55-90). `fileCreateDirectory` (corelib_file.cpp:83) single-level mkdir; `fileDirectoryExists` (stat+S_ISDIR).

**Data model:** `song[256][8]` of chain idx (EMPTY_VALUE_16=32767); `ChainRow{phrase,transpose}`; `PhraseRow{note,instrument,volume,fx[3][2]}` (no empty marker; empty = note==EMPTY_VALUE_8); groove 255 = sentinel; default groove {6,6,255,...} → 6 ticks/row; 24 ticks = 1 beat; tickRate default 50 Hz. NOTE_OFF=254.
