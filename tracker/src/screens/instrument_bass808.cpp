// Fatty (Bass808) instrument editor screen.
//
// Layout follows the DrumSynth / Sintered pattern (see instrument_drum_synth.cpp):
//   rows 0-2  : common instrument header (name / type / volume / pan / transpose / EQ / modulations)
//   row 3     : OSC waveform picker (popup: SINE / TRI / SAW / SQR)
//   rows 4-7  : Sub / SubLvl / Drive / Glide + shared voice-post filter block in the right column
//   row 8     : EnvMode (TRIG / VCA)
//   row 9     : shared ADSR row (VCA mode) — rendered by the common helper
//
// The macro rows keep stable positions so muscle memory transfers between
// presets.  When sub is OFF, the Sub/SubLvl rows still edit their bytes (the
// sub simply contributes nothing).

#include "screen_instrument.h"
#include "field_style.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "selection_popup.h"
#include <string.h>

// Pop-up picker state for the two waveform selectors.  Reuses the same
// popupEditInput helper as DrumSynth / Sintered so the key bindings stay
// consistent (hold = open popup, hold+right = cycle).
static int waveButtonDown;
static int subButtonDown;

// Single source of truth for the 5-option sub-oscillator menu (OFF + 4 waves)
// and the 4-option main-oscillator menu.  Used both for the popup and for
// the value-text rendering so the labels stay in sync.
static const char* waveName(int w) {
  static const char* names[] = {"SINE", "TRI", "SAW", "SQR"};
  return w >= 0 && w < 4 ? names[w] : "SINE";
}
static const char* subWaveName(int w) {
  static const char* names[] = {"OFF", "SINE", "TRI", "SAW", "SQR"};
  return w >= 0 && w < 5 ? names[w] : "OFF";
}

// Returns a pointer to the macro byte at column 0 for rows 4-7, matching
// the screen row order: 4=Sub, 5=SubLvl, 6=Drive, 7=Glide.  Returns nullptr
// for other rows.
static uint8_t* macro(InstrumentBass808* b, int row) {
  switch (row) {
    case 4: return &b->subWaveform; // Sub (0=off, 1..4 picker)
    case 5: return &b->subLevel;    // SubLvl
    case 6: return &b->drive;       // Drive
    case 7: return &b->glide;       // Glide
    default: return nullptr;
  }
}
// The label that appears in column 0 for each macro row.  Kept in sync with
// macro() above so the field the cursor highlights always matches what the
// value means.
static const char* macroLabel(int row) {
  switch (row) {
    case 4: return "Sub";
    case 5: return "SubLvl";
    case 6: return "Drive";
    case 7: return "Glide";
    default: return "---";
  }
}

// Pop-up callbacks (mirror DrumSynth's selectEngine / cancelEngineSelection).
static void selectWave(int value) {
  chipnomadState->project.instruments[cInstrument].chip.bass808.waveform = (uint8_t)value;
  projectModified = 1;
  screenSetup(&screenInstrument, cInstrument);
}
static void cancelWaveSelection() { screenSetup(&screenInstrument, cInstrument); }
static void openWaveSelection() {
  InstrumentBass808* b = &chipnomadState->project.instruments[cInstrument].chip.bass808;
  static const char* labels[] = {"SINE", "TRI", "SAW", "SQR"};
  static SelectionItem items[4];
  for (int i = 0; i < 4; ++i) {
    items[i].label = labels[i];
    items[i].value = i;
    items[i].children = nullptr;
    items[i].childCount = 0;
  }
  selectionPopupSetup("FATTY OSC", items, 4, (int)b->waveform, selectWave, cancelWaveSelection);
  screenSetup(&screenSelectionPopup, 0);
}
static void selectSubWave(int value) {
  chipnomadState->project.instruments[cInstrument].chip.bass808.subWaveform = (uint8_t)value;
  projectModified = 1;
  screenSetup(&screenInstrument, cInstrument);
}
static void cancelSubWaveSelection() { screenSetup(&screenInstrument, cInstrument); }
static void openSubWaveSelection() {
  InstrumentBass808* b = &chipnomadState->project.instruments[cInstrument].chip.bass808;
  static const char* labels[] = {"OFF", "SINE", "TRI", "SAW", "SQR"};
  static SelectionItem items[5];
  for (int i = 0; i < 5; ++i) {
    items[i].label = labels[i];
    items[i].value = i;
    items[i].children = nullptr;
    items[i].childCount = 0;
  }
  selectionPopupSetup("FATTY SUB", items, 5, (int)b->subWaveform, selectSubWave, cancelSubWaveSelection);
  screenSetup(&screenSelectionPopup, 0);
}

static int columns(int row) {
  // Mirrors DrumSynth/Plaits: 3 header rows, 1 column for the OSC picker,
  // 2 columns for the macro + voice-post rows, 1 for EnvMode (row 8).
  // The bottom row is the ADSR (5 columns) in VCA mode or the LPG D/C
  // pair (2 columns) in TRIG mode.
  return row < 3 ? instrumentCommonColumnCount(row)
       : row == 3 ? 1
       : row <= 7 ? 2
       : row == 8 ? 1
       : chipnomadState->project.instruments[cInstrument].chip.bass808.envelopeMode == 0 ? 2 : 5;
}

static void drawStatic(void) {
  instrumentCommonDrawStatic();
  InstrumentBass808* b = &chipnomadState->project.instruments[cInstrument].chip.bass808;
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 6, "OSC");
  gfxPrint(0, 12, "EnvMode");
  gfxSetFgColor(appSettings.colorScheme.textLabel);
  gfxPrint(0, 8, macroLabel(4));
  gfxPrint(0, 9, macroLabel(5));
  gfxPrint(0, 10, macroLabel(6));
  gfxPrint(0, 11, macroLabel(7));
  // The shared filter block lives in the right column.  The bottom row
  // mirrors plaits: VCA mode shows the ADSR, TRIG mode shows the LPG-style
  // D (decay) / C (colour) controls driven by the same helper.
  instrumentCommonDrawVoicePostStatic(b->envelopeMode != 0);
  if (b->envelopeMode == 0) {
    gfxSetFgColor(appSettings.colorScheme.textTitles);
    gfxPrint(0, 14, "LPG");
    gfxSetFgColor(appSettings.colorScheme.textLabel);
    gfxPrint(6, 14, "D");
    gfxPrint(11, 14, "C");
  }
}

static void drawCursor(int col, int row) {
  if (row < 3) { instrumentCommonDrawCursor(col, row); return; }
  InstrumentBass808* b = &chipnomadState->project.instruments[cInstrument].chip.bass808;
  // VCA mode: the common helpers own the filter columns AND the ADSR row.
  // TRIG mode: the common helpers own only the filter columns; the bottom
  // row is the LPG D/C pair handled below (plaits-exact).
  if (b->envelopeMode != 0 && instrumentCommonDrawVoicePostCursor(col, row)) return;
  if (row == 9) {
    if (b->envelopeMode == 0) gfxCursor(7 + col * 5, 14, 2);
    return;
  }
  if (col && row >= 4 && row <= 8) { instrumentCommonDrawVoicePostCursor(col, row); return; }
  // The picker row spans its full label width so the popup affordance reads.
  if (row == 3) gfxCursor(kColumnValueX[0], 6, 3 + strlen(waveName(b->waveform)));
  else if (row == 8) gfxCursor(kColumnValueX[0], 12, b->envelopeMode == 0 ? 4 : 3);
  else if (!col) gfxCursor(kColumnValueX[0], row + 4, kFieldNumberWidth);
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) { instrumentCommonDrawField(col, row, state); return; }
  InstrumentBass808* b = &chipnomadState->project.instruments[cInstrument].chip.bass808;
  // VCA mode: common helpers own the filter columns AND the ADSR row.
  if (b->envelopeMode != 0 && instrumentCommonDrawVoicePostField(col, row, state, b)) return;

  gfxSetFgColor(fieldValueColor(state));
  const int x = kColumnValueX[0];

  if (row == 3) {
    // OSC waveform picker: shows the two-digit index + a 3-letter label,
    // matching DrumSynth's Model row.
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textTitles);
    gfxPrint(0, 6, "OSC");
    gfxClearRect(x, 6, 34 - x, 1);
    gfxPrintf(x, 6, "%02d", (int)b->waveform);
    gfxSetFgColor(fieldNameColor(state));
    gfxPrint(x + 3, 6, waveName(b->waveform));
    return;
  }

  if (row == 8) {
    // EnvMode: TRIG (one-shot pluck) / VCA (ADSR drives the gain).
    fieldDrawLabel(0, 12, "EnvMode", state);
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textInfo);
    gfxClearRect(x, 12, 6, 1);
    gfxPrint(x, 12, b->envelopeMode == 0 ? "TRIG" : "VCA");
    return;
  }

  if (row == 9) {
    // TRIG mode bottom row: LPG-style D (decay) / C (colour) — the exact
    // layout plaits uses (2 columns only). No ADSR preview in this mode.
    if (b->envelopeMode != 0 || col >= 2) return;
    uint8_t values[2] = {b->decay, b->sustain};
    fieldDrawLabel(col ? 11 : 6, 14, col ? "C" : "D", state);
    gfxSetFgColor(fieldValueColor(state));
    gfxPrint(7 + col * 5, 14, byteToHex(values[col]));
    return;
  }

  // Macro rows (4=Sub, 5=SubLvl, 6=Drive, 7=Glide).
  fieldDrawLabel(0, row + 4, macroLabel(row), state);
  gfxSetFgColor(fieldValueColor(state));
  gfxClearRect(x, row + 4, kFieldNumberWidth, 1);
  if (uint8_t* value = macro(b, row)) {
    if (row == 4) {
      // Sub is a picker (OFF/SINE/TRI/SAW/SQR) so the byte value 0..4
      // is rendered as its short label instead of a number.
      gfxPrint(x, row + 4, subWaveName(*value));
    } else {
      fieldDrawNumber(x, row + 4, *value, state, -1.0f, false,
                       instrumentLivePosition(liveSource1 + row - 4));
    }
  } else {
    gfxPrint(x, row + 4, "--");
  }
}

bool instrumentBass808LiveCell(int control, int* col, int* row) {
  // Live bars map in page order: Sub, SubLvl, Drive, Glide (OSC is a picker,
  // so it doesn't get a bar).
  if (control < liveSource1 || control >= liveSource1 + 4) return false;
  const int macroIndex = control - liveSource1;
  *col = 0;
  *row = 4 + macroIndex;  // rows 4..7
  return true;
}

static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentBass808* b = &chipnomadState->project.instruments[cInstrument].chip.bass808;
  // VCA mode: the common helpers own the filter columns AND the ADSR row.
  if (b->envelopeMode != 0 && ((row == 9) || (col && row >= 4 && row <= 8))) {
    int ok = instrumentCommonOnEditVoicePost(col, row, action, b);
    if (ok) projectModified = 1;
    return ok;
  }
  int ok = 0;
  if (row == 3) {
    // OSC waveform: cycle 0..3, hold for popup.
    ok = edit8noLast(action, &b->waveform, 1, 0, (int)Bass808Waveform::totalCount - 1);
  } else if (row == 8) {
    // EnvMode cycles ONLY on EDIT+direction (tap does nothing) and WRAPS
    // around (TRIG -> VCA -> TRIG) so the user never has to travel back.
    if (action == CellEditAction::increase || action == CellEditAction::decrease) {
      b->envelopeMode = b->envelopeMode == 0 ? 2 : 0;
      ok = 1;
    }
    if (ok) {
      projectModified = 1;
      screenSetup(&screenInstrument, cInstrument);
    }
    return ok;
  } else if (row == 9) {
    // TRIG mode bottom row: LPG-style D (decay) / C (colour), the same
    // byte slots plaits' TRIG mode drives (decay / sustain).
    uint8_t* values[2] = {&b->decay, &b->sustain};
    if (b->envelopeMode == 0 && col < 2) ok = edit8noLast(action, values[col], 16, 0, 255);
  } else if (uint8_t* value = macro(b, row)) {
    if (row == 4) {
      // Sub waveform: cycle 0..4.
      ok = edit8noLast(action, value, 1, 0, 4);
    } else {
      ok = edit8noLast(action, value, 16, 0, 255);
    }
  }
  if (ok) { projectModified = 1; screenFullRedraw(&screenInstrumentBass808); }
  return ok;
}

static int onInput(int isKeyDown, int keys, int) {
  // Hold-to-popup on the OSC (row 3) and Sub (row 4) pickers, mirroring
  // the DrumSynth Model picker so the muscle memory transfers.
  const int row = screenInstrumentBass808.cursorRow;
  if (row == 3) {
    PopupEditInput input = popupEditInput(isKeyDown, keys, &waveButtonDown);
    if (input == PopupEditInput::cycle) {
      cycle8(&chipnomadState->project.instruments[cInstrument].chip.bass808.waveform,
             keys == (keyEdit | keyRight) ? 1 : -1, 0,
             (int)Bass808Waveform::totalCount - 1, 0);
      projectModified = 1;
      screenFullRedraw(&screenInstrumentBass808);
      return 1;
    }
    if (input == PopupEditInput::hold) return 1;
    if (input == PopupEditInput::open) { openWaveSelection(); return 1; }
    return 0;
  }
  if (row == 4) {
    PopupEditInput input = popupEditInput(isKeyDown, keys, &subButtonDown);
    if (input == PopupEditInput::cycle) {
      cycle8(&chipnomadState->project.instruments[cInstrument].chip.bass808.subWaveform,
             keys == (keyEdit | keyRight) ? 1 : -1, 0, 4, 0);
      projectModified = 1;
      screenFullRedraw(&screenInstrumentBass808);
      return 1;
    }
    if (input == PopupEditInput::hold) return 1;
    if (input == PopupEditInput::open) { openSubWaveSelection(); return 1; }
    return 0;
  }
  waveButtonDown = 0;
  subButtonDown = 0;
  return 0;
}

ScreenData screenInstrumentBass808 = {
  .rows = 10, .cursorRow = 0, .cursorCol = 0, .topRow = 0, .selectMode = -1,
  .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none, .getColumnCount = columns,
  .drawStatic = drawStatic, .drawCursor = drawCursor, .drawSelection = NULL,
  .drawRowHeader = NULL, .drawColHeader = NULL, .drawField = drawField, .onEdit = onEdit,
  .onInput = onInput, .onRawInput = NULL, .isCellValid = NULL, .getLoopRange = NULL,
};
