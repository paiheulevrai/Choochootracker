# Feature 5 — Slice Row Split Boxes (Count + Slice Browser)

Status: planned (not started)
Branch: `sampler`
Scope: `tracker/src/screens/screen_sample_settings.cpp`, `docs/USER_MANUAL.md`. **No engine
(`chipnomad_lib`) changes, no save-format changes, no new dependencies.**

---

## 1. Request (verbatim from the maintainer)

> ok, i want to simplify the sample slice editing row. I want separate two digit boxes in a
> slice mode row First one should be displaying the number of slices total (slice count),
> second one should be a slice browser. Both should be adjusted with edit + dir. The
> small-step-big-step logic should stay. Same with deleting logic - adding or removing slices
> in slice count box recalculates the division. Deleting slice in a slice browses merges
> slices with the existing logic. You can get out of the edit screen with b. When the pointer
> is on a slice count, display: "Adjust the number of slices" in a helper row, when the box is
> on a slice browser, display "Browse slices". Adding slices manually in a lazy mode should
> always refresh the slice counter value.

## 2. Current state (what exists today)

The Slice row (row 2 of the SAMPLE EDIT screen) has three cells:

| Cell | Col | Geometry | Content today |
|---|---|---|---|
| Mode | 0 | `sliceModeX=6`, `sliceModeW=5` | `OFF/EQUAL/AUTO/LAZY`, cycled with EDIT+dir / tap, EDIT+OPT = off |
| Number | 1 | `sliceNumX=12`, `sliceNumW=2` | **Dual display**: slice count by default (`sliceNumShowsCount=1`), 1-indexed current slice in "edit view". EDIT+dir browses slices; EDIT+OPT deletes the current slice; tap previews it |
| Frame | 2 | `sliceFrameX=15`, `sliceFrameW=6` | Current slice's start frame in hex; EDIT+dir nudges; EDIT+OPT deletes |

Count editing lives in a **B+direction intercept** in `onInput()` (before the standard
dispatch): B+LEFT/RIGHT = count ±1, B+UP/DOWN = cycle through `2,4,8,16,32,64` with
wrap-around. EQUAL re-divides evenly, AUTO re-runs detection with the new count, LAZY is
inert. Because of that combo, **bare B is consumed on the Number cell** (an intercept in
`inputScreenNavigation`) so it does not navigate back to the instrument screen.

Hints (`settingsUpdateHint(row, col, ownsBar)` + `hintOwnedText` ownership tracking): the
Mode cell shows a persistent hint bound to the active mode; the Number cell **alternates**
two combo hints every 2.5 s (`hintPhase`); the Frame cell shows a timed static hint.

Session state involved: `sliceNumShowsCount` (dual-display toggle), `currentSlice`
(0-based browsed slice), `hintPhase` / `hintCursorRow` / `hintCursorCol` / `hintOwnedText`.

## 3. Target design

### 3.1 Row layout — four cells

```
Slice   OFF    04  02  000000
        Mode Count Brws Frame
```

| Cell | Col | Geometry | Content |
|---|---|---|---|
| Mode | 0 | `sliceModeX=6`, w=5 | unchanged |
| **Count** | 1 | `sliceNumX=12`, w=2 | total number of slices, `%02d` (1..64) |
| **Slice** (browser) | 2 | `sliceBrowseX=15`, w=2 | 1-indexed current slice, `%02d` |
| Frame | 3 | `sliceFrameX=18`, w=6 | unchanged content, x moves 15 → 18 |

`settingsColumnCount(row)` returns 4 for row 2. Every `col < 3` / `col > 2` / `col == 2`
assumption in the file is updated (repaint loops, cursor geometry, dim logic, cursor clamp in
`fullRedraw`, LAZY playback-drop dim moves from col 2 to col 3).

### 3.2 Count box (col 1) — "recalculates the division"

- **EDIT + LEFT/RIGHT** = count −1/+1, clamped to 1..64 (the old B+LEFT/RIGHT).
- **EDIT + UP/DOWN** = power-of-two cycle `2,4,8,16,32,64` with wrap-around; count 0/1
  enters at 2 going up, wraps to 64 going down (the old B+UP/DOWN, logic reused verbatim).
- Every count change **recalculates the division**, exactly like the old B+direction:
  EQUAL → `sampleSliceInitEven()` re-divides the Start/End window; AUTO →
  `settingsRunAutoDetect()` re-runs detection (sensitivity derived from the count,
  `DETECTING...` feedback preserved). `currentSlice` is clamped afterwards
  (`settingsClampCurrentSlice`).
- **LAZY: the Count box is inert and dimmed** — hand-placed slices have no division to
  recalculate, and re-initializing would destroy them. (Same semantics as the old
  "B+direction is inert in LAZY".)
- **OFF: shows `-`** dimmed (like the old Number cell).
- **EDIT + OPT on Count is inert** (turning slicing off stays on the Mode cell; deleting a
  slice stays on the browser). Plain EDIT tap on Count is inert too.
- The count box is **always live**: it repaints on every count change, mode change, AUTO
  detection, slice delete and LAZY playback-drop (see 3.5).

### 3.3 Slice browser box (col 2) — "browse + delete/merge"

- **EDIT + LEFT/RIGHT** = current slice −1/+1, clamped to `0..count-1` (no wrap; same as
  today's browsing).
- **EDIT + UP/DOWN** = big step, **±4 slices**, clamped the same way (new; follows the
  screen's small-step/big-step convention — the exact step is a tunable constant,
  `kSliceBrowseBigStep = 4`).
- Browsing recenters the zoomed view on the slice start (existing `zoomToMarker` logic),
  brightens the slice's marker and draws the black band on the waveform (all driven by
  `currentSlice` — unchanged).
- **EDIT + OPT deletes the current slice** with the existing merge logic
  (`settingsSliceDeleteCurrent` → `sampleSliceDelete`): the slice joins the previous one,
  the first slice cannot be deleted (`Cannot delete first slice`), deleting the last
  remaining slice turns the mode off. Unchanged.
- **EDIT tap / double-tap previews** the current slice (existing
  `settingsSlicePreviewCurrent`, works in every mode).
- OFF: shows `-` dimmed.

### 3.4 B exits the screen from everywhere

- The **bare-B intercept on the Number cell is removed** from `inputScreenNavigation`:
  pressing B (OPT) on any Slice cell now navigates back to the instrument screen like
  everywhere else on the screen.
- The **whole B+direction count intercept is removed** from `onInput()` — count editing
  moved to EDIT+direction on the Count box. `screenClearOptPressed()` in that block goes
  with it.
- Leaving via B still stops a running LAZY full-sample preview and A-tap slice preview
  (the existing keyOpt stop logic in `inputScreenNavigation` already covers this — it now
  simply also applies to the Slice row).

### 3.5 LAZY playback-drop refreshes the count box

`settingsDropSliceAtPlayback()` keeps inserting via `sampleSliceInsertAtFrameGapped()` and
setting `currentSlice = index`; `settingsRepaintSlice()` repaints **all four** cells, so the
Count box shows the incremented count immediately and the browser follows the dropped slice.
The `sliceNumShowsCount = 0` write disappears (the variable is deleted, see 3.6). This is
the explicit acceptance point: **every manual slice add in LAZY refreshes the counter
value on the next frame.**

### 3.6 State removals (simplification)

| Removed | Why |
|---|---|
| `sliceNumShowsCount` (+ all 9 write sites) | the dual display is replaced by two always-visible boxes |
| `hintPhase` (+ the Number-cell rotation) | no cell rotates hints anymore |
| B+direction count intercept in `onInput()` | moved to EDIT+direction on Count |
| bare-B intercept on the Number cell in `inputScreenNavigation()` | B exits everywhere now |

`settingsSliceNumberEdit()` is split into `settingsSliceCountEdit()` (col 1) and
`settingsSliceBrowseEdit()` (col 2). The "mode change returns the Number box to the count
display" write in `settingsSliceModeEdit()` disappears.

### 3.7 Hints — static, one per box, no rotation

`settingsUpdateHint()` keeps the ownership-tracking pattern (`hintOwnedText`, foreign
messages pause, `screenClearMessage()` on drop) but every Slice cell now has a **single
persistent hint**, re-issued every frame while the bar is ours or empty (the Mode-cell
pattern; timer `2` survives the decrement-after-print):

| Cell | Hint text |
|---|---|
| Mode (col 0) | unchanged — bound to the active mode, OFF clears |
| Count (col 1) | `Adjust the number of slices` |
| Slice (col 2) | `Browse slices` |
| Frame (col 3) | `Adjust slice start` (switches from timed-150 to persistent for consistency) |

Deleted: the alternating `OPT + DIR = change slice count` / `EDIT + DIR = browse slices`
pair (both combos no longer exist in that form).

## 4. Implementation steps

All in `tracker/src/screens/screen_sample_settings.cpp` unless noted.

1. **Geometry**: add `sliceBrowseX = 15`, `sliceBrowseW = 2`; move `sliceFrameX` to 18.
   Update `settingsDrawCursor()` and the row-2 branch of `settingsDrawField()`.
2. **Column count / navigation**: `settingsColumnCount()` row 2 → 4; `fullRedraw()` cursor
   clamp `> 2` → `> 3`; `settingsIsCellValid()` unchanged (whole row invalid under Stretch).
3. **Draw**: row-2 branch draws four cells. Dim rules: Stretch → all four; OFF → Count,
   Slice, Frame dimmed (`-` / `------`); LAZY → Count dimmed; LAZY playback-drop armed →
   Frame (col 3) dimmed. Count always prints `%02d` of `sampleDecodeSliceCount()`; Slice
   always prints `%02d` of `currentSlice + 1`.
4. **Edit handlers**: split `settingsSliceNumberEdit()` into `settingsSliceCountEdit()`
   (small ±1 / big power-of-two cycle + re-init EQUAL/AUTO, inert in LAZY and OFF) and
   `settingsSliceBrowseEdit()` (small ±1 / big ±4, clamp, recenter, repaint). Move the
   count-cycle table (`kSliceSteps`) from the `onInput()` intercept into the Count handler.
5. **Dispatch** `settingsOnEditSlice()`: col 0 → mode; col 1 → count; col 2 → browse;
   col 3 → frame. `CellEditAction::clear` deletes the current slice from cols 2 and 3 only
   (inert on 0 and 1). Tap/double-tap previews on cols 2 and 3.
6. **Delete the intercepts**: the B+direction block in `onInput()` and the bare-B-on-Number
   check in `inputScreenNavigation()`.
7. **Delete dead state**: `sliceNumShowsCount` (all sites), `hintPhase`, the
   `sliceNumShowsCount` write in `settingsSliceModeEdit()` / `settingsDropSliceAtPlayback()` /
   `settingsSliceDeleteCurrent()` / `settingsSliceFrameEdit()` / preview tap; the
   `sliceNumShowsCount = 1` reset in `setup()`.
8. **Hints**: rewrite `settingsUpdateHint()` to the four-static-persistent-hints table
   (3.7); keep `hintOwnedText` ownership, the draw() hook and the `setup()` resets; delete
   `hintPhase` and the rotation comment block; refresh the section comment.
9. **Repaint loops**: `settingsRepaintSlice()` and the LAZY-dim repaint in `draw()` loop
   `col < 4`; the playback-drop dim condition moves to col 3.
10. **Docs**: `docs/USER_MANUAL.md` — rewrite the Slice bullets (627-712 region): four
    cells; Count edited with EDIT+direction (small/big semantics, EQUAL re-divides, AUTO
    re-detects, inert+dimmed in LAZY); Slice browser with browse/delete/preview; B exits
    from the Slice row too; hint texts; LAZY drop refreshes the count box. Remove the
    dual-display and B+direction descriptions.

## 5. Decisions & defaults (deviate only with reason)

| # | Question | Default |
|---|---|---|
| D1 | EDIT+OPT on the Count box | Inert (off = Mode cell, delete = browser) |
| D2 | EDIT tap on the Count box | Inert (direction keys do the work) |
| D3 | Browser big step | ±4 slices, clamped, no wrap |
| D4 | Frame hint timer | Becomes persistent like the others (consistency; no visible gap on re-arm) |
| D5 | Count box in LAZY | Dimmed + inert (hand-placed slices only change by drop/delete) |

## 6. Manual QA checklist

- [ ] EQUAL: EDIT+RIGHT on Count 4→5 re-divides evenly; box shows `05`; browser clamps to
      the new last slice; waveform markers repaint.
- [ ] EDIT+UP on Count cycles 4→8→16; EDIT+DOWN 4→2; at 64 UP wraps to 2, at 2 DOWN wraps
      to 64; count 1 via LEFT stays valid.
- [ ] AUTO: count change shows `DETECTING...` and re-runs detection; sensitivity message
      unchanged.
- [ ] LAZY: Count box dimmed and inert (EDIT+dir does nothing); dropping slices with EDIT
      during playback increments the Count box immediately and moves the browser to the
      dropped slice; deleting in the browser merges into the previous slice and both boxes
      refresh.
- [ ] Browser: EDIT+RIGHT/LEFT steps one slice (brighter marker + black band follow; zoomed
      view recenters on the slice start); EDIT+UP/DOWN jumps 4; EDIT+OPT deletes (first
      slice rejected with `Cannot delete first slice`; last slice turns the mode off).
- [ ] Frame: nudging works as before at its new position; dims while the LAZY playback-drop
      is armed.
- [ ] B alone exits to the instrument screen from every Slice cell (and stops the LAZY
      preview / A-tap preview); SHIFT+LEFT still exits from anywhere.
- [ ] Hints: Count shows `Adjust the number of slices`, Slice shows `Browse slices`, Frame
      shows `Adjust slice start`, Mode shows the active mode's description and OFF clears
      the bar; action messages (`Detected N slices`, `Too close to slice`, errors) override
      and hints resume after they expire.
- [ ] Stretch active: the whole row is dimmed and inert; the cursor never rests on it.
- [ ] OFF: Count and Slice show `-` dimmed; mode round-trip OFF → EQUAL restores bounds.
- [ ] `cd tracker && timeout 900 make -f Makefile.test -j4` — 506/506 green (no engine
      changes; no new tests required).
- [ ] `cd tracker && timeout 900 make -f Makefile.linux linux` — EXIT=0, no new warnings
      (baseline 106).

## 7. Commit

Single scoped commit, local only (the repo is never pushed from the agent session):

```
Split the Slice Number box into Count and Slice browser boxes

- The dual-display Number cell becomes two two-digit boxes: Count (total
  slices) and Slice (1-indexed browser). Both edit with EDIT+direction
  (small = left/right, big = up/down); the Frame cell moves right.
- Count edits recalculate the division (EQUAL re-divides, AUTO re-runs
  detection) and are inert+dimmed in LAZY; the browser keeps the existing
  delete/merge (EDIT+OPT) and tap-preview behavior and gains a ±4 big step.
- B+direction count editing and the bare-B intercept on the Number cell
  are removed: B now exits the screen from the Slice row like everywhere
  else. sliceNumShowsCount and the hint rotation are deleted; every Slice
  cell shows one persistent hint (Count: "Adjust the number of slices",
  Slice: "Browse slices").
- LAZY playback-drop refreshes the Count box immediately (repaint covers
  all four cells). USER_MANUAL updated. Test suite unchanged (506/506).
```

---

## 8. AGENT PROMPT (copy-paste everything in the fence)

```text
You are working on ChooChooTracker (branch "sampler"), a portable C++17 music tracker.
Repo rules live in AGENTS.md at the root; architecture rules in docs/ARCHITECTURE.md and
docs/realtime-architecture.md. You are implementing ONE self-contained UI feature:
splitting the Slice row's dual-purpose Number box into two separate two-digit boxes.

MANDATORY READING before writing code:
- AGENTS.md
- docs/sampler-edit-plan/08_SLICE_ROW_SPLIT_BOXES.md  (this task's contract - read it fully)
- tracker/src/screens/screen_sample_settings.cpp  (the only code file you touch)
- docs/USER_MANUAL.md  (the Slice section you must update)

NON-NEGOTIABLE CONSTRAINTS:
1. UI is a 40x20 character grid driven by ScreenData/CellEditAction - no popups, no mouse.
   Follow the existing edit-action conventions in the file (EDIT+dir = adjust, EDIT+OPT =
   clear/delete, tap = preview/cycle).
2. NO changes outside tracker/src/screens/screen_sample_settings.cpp and
   docs/USER_MANUAL.md. In particular: no chipnomad_lib changes, no save-format changes,
   no new dependencies. The slice sentinel encoding, sliceBounds[] and all
   sampleSlice*() engine helpers are used as-is.
3. C++17, repo code style: static module state, snprintf, C-strings, no exceptions.
4. Delete dead state completely - do not leave unused variables behind:
   sliceNumShowsCount, hintPhase, the B+direction count intercept in onInput(), and the
   bare-B intercept on the Number cell in inputScreenNavigation() must all be removed.
5. Tests must pass: cd tracker && timeout 900 make -f Makefile.test -j4
   (expect 506/506; no new tests required since this is UI-only).
6. Build must be clean: cd tracker && timeout 900 make -f Makefile.linux linux
   (EXIT=0; warning baseline is 106 - no new warnings allowed).
7. Update docs/USER_MANUAL.md before committing (never the in-app help).
8. Commit locally with the message from section 7 of the plan doc. NEVER push - the
   remote must stay untouched.

TASK (full details, geometry, semantics, decisions and edge cases are in section 3 of
docs/sampler-edit-plan/08_SLICE_ROW_SPLIT_BOXES.md - that section is binding):
- Slice row becomes four cells: Mode (col 0, unchanged) / Count (col 1, new box at
  x=12 w=2, shows the total slice count %02d) / Slice (col 2, new browser box at x=15
  w=2, shows the 1-indexed current slice %02d) / Frame (col 3, unchanged content,
  moves to x=18).
- Count box: EDIT+LEFT/RIGHT = count -+1 clamped 1..64; EDIT+UP/DOWN = the existing
  power-of-two cycle 2,4,8,16,32,64 with wrap-around (move the kSliceSteps logic from
  the onInput() B+direction intercept into this handler). Every change recalculates the
  division: EQUAL -> sampleSliceInitEven(), AUTO -> settingsRunAutoDetect() (keep the
  DETECTING... feedback), then settingsClampCurrentSlice(). Inert AND dimmed in LAZY;
  shows "-" dimmed in OFF; EDIT+OPT and plain tap are inert.
- Slice browser box: EDIT+LEFT/RIGHT = current slice -+1 clamped (existing browse logic
  incl. zoom recenter on the slice start); EDIT+UP/DOWN = -+4 slices (new big step,
  clamped, no wrap); EDIT+OPT deletes the current slice via the existing
  settingsSliceDeleteCurrent() merge logic; tap/double-tap previews the current slice
  via settingsSlicePreviewCurrent(); shows "-" dimmed in OFF.
- B (OPT) alone now exits to the instrument screen from every Slice cell: delete the
  bare-B intercept on the Number cell. The existing preview-stop logic on keyOpt in
  inputScreenNavigation() must keep working (it now also covers the Slice row).
- Hints: settingsUpdateHint() keeps the hintOwnedText ownership pattern exactly (bar is
  ours when empty or strcmp-equal; foreign messages pause; screenClearMessage() drops),
  but every Slice cell now has ONE persistent hint re-issued every frame with
  screenMessage(2, ...) (the current Mode-cell pattern - timer 2 survives the
  decrement-after-print): Mode keeps the per-mode texts, Count shows "Adjust the number
  of slices", Slice shows "Browse slices", Frame shows "Adjust slice start" (switch it
  from timed 150 to persistent). Delete hintPhase and the alternating Number hints.
- LAZY playback-drop: settingsDropSliceAtPlayback() must leave both new boxes correct
  on the next frame (settingsRepaintSlice repaints all four cells; the count box always
  shows the live count). The Frame dim during the playback-drop moves from col 2 to
  col 3.
- Update every col<3 / col>2 / col==2 assumption: settingsColumnCount (row 2 -> 4),
  settingsDrawCursor, settingsDrawField row-2 branch, settingsRepaintSlice loop, the
  LAZY-dim repaint loop in draw(), the fullRedraw() cursor clamp (> 2 -> > 3), and the
  settingsOnEditSlice() dispatch.
- setup() drops the sliceNumShowsCount reset; settingsSliceModeEdit() drops its
  "return to count display" write.

WORKFLOW:
1. Read the files listed above, then implement.
2. Run the test suite; fix until 506/506 green.
3. Run the Linux build; verify EXIT=0 and no new warnings vs the 106 baseline.
4. Walk through the QA checklist in section 6 of the plan doc; report pass/fail per item
   for everything automatable.
5. Update docs/USER_MANUAL.md (rewrite the Slice bullets per section 4 step 10).
6. Make ONE scoped commit (files: screen_sample_settings.cpp, USER_MANUAL.md) with the
   message from section 7 of the plan doc. Do not push.

Report back: files changed, test output summary, build result, QA checklist results,
and any deviation from the plan's section 5 decisions (with rationale).
```
