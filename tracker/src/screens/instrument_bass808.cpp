// Bass808 instrument editor screen.
//
// Layout follows the DrumSynth / Sintered pattern (see instrument_drum_synth.cpp):
//   rows 0-2  : common instrument header (name / type / volume / pan / transpose / EQ / modulations)
//   row 3     : Waveform picker (popup: SINE / TRI / SAW / SQR)
//   rows 4-8  : six Bass808 parameters + shared voice-post filter/ADSR block in the right column
//   row 9     : Sub-oscillator enable + waveform picker (OFF / SINE / TRI / SAW / SQR)
//
// The six macro slots on rows 4-8 are deliberately stable in position so the
// player's muscle memory transfers from one preset to another even when the
// sub oscillator is disabled.  When sub is OFF, the SubWave/SubLevel rows
// display '--' and ignore edits, matching how DrumSynth renders macros that
// aren't relevant to the current engine.

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

// Returns a pointer to the macro byte at column 0 / (row-4), matching the
// screen row order below.  Returns nullptr when the cell isn't editable.
static uint8_t* macro(InstrumentBass808* b, int row) {
  switch (row) {
    case 4: return &b->drive;       // row 4: Drive
    case 5: return &b->glide;       // row 5: Glide
    case 6: return &b->subWaveform; // row 6: SubWave (0=off, 1..4 picker)
    case 7: return &b->subLevel;     // row 7: SubLvl
    default: return nullptr;
  }
}
// The label that appears in column 0 for each macro row.  Kept in sync with
// macro() above so the field the cursor highlights always matches what the
// value means.
static const char* macroLabel(int row) {
  switch (row) {
    case 4: return "Drive";
    case 5: return "Glide";
    case 6: return "SubWave";
    case 7: return "SubLvl";
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
  selectionPopupSetup("808 WAVE", items, 4, (int)b->waveform, selectWave, cancelWaveSelection);
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
  selectionPopupSetup("808 SUB", items, 5, (int)b->subWaveform, selectSubWave, cancelSubWaveSelection);
  screenSetup(&screenSelectionPopup, 0);
}

static int columns(int row) {
  // Mirrors DrumSynth: 3 header rows, then 1 column for the picker row,
  // 2 columns for the macro + voice-post rows, 1 elsewhere.
  return row < 3 ? instrumentCommonColumnCount(row) : row == 3 ? 1 : row <= 8 ? 2 : 1;
}

static void drawStatic(void) {
  instrumentCommonDrawStatic();
  InstrumentBass808* b = &chipnomadState->project.instruments[cInstrument].chip.bass808;
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 6, "Wave");
  gfxPrint(0, 7, "BASS");
  gfxSetFgColor(appSettings.colorScheme.textLabel);
  gfxPrint(0, 8, macroLabel(4));
  gfxPrint(0, 9, macroLabel(5));
  gfxPrint(0, 10, macroLabel(6));
  gfxPrint(0, 11, macroLabel(7));
  // The shared filter+ADSR block in the right column is rendered by this
  // helper; passing drawEnvelope=0 keeps the envelope preview hidden (the
  // Sintered / DrumSynth screens do the same).
  instrumentCommonDrawVoicePostStatic(0);
}

static void drawCursor(int col, int row) {
  if (row < 3) { instrumentCommonDrawCursor(col, row); return; }
  if (col && row >= 4 && row <= 8) { instrumentCommonDrawVoicePostCursor(col, row); return; }
  // The picker row spans its full label width so the popup affordance reads.
  if (row == 3) gfxCursor(kColumnValueX[0], 6, 3 + strlen(waveName(chipnomadState->project.instruments[cInstrument].chip.bass808.waveform)));
  else if (!col) gfxCursor(kColumnValueX[0], row + 4, kFieldNumberWidth);
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) { instrumentCommonDrawField(col, row, state); return; }
  InstrumentBass808* b = &chipnomadState->project.instruments[cInstrument].chip.bass808;
  if (col && row >= 4 && row <= 8) { instrumentCommonDrawVoicePostField(col, row, state, b); return; }

  gfxSetFgColor(fieldValueColor(state));
  const int x = kColumnValueX[0];

  if (row == 3) {
    // Main waveform picker: shows the two-digit index + a 3-letter label,
    // matching DrumSynth's Model row.
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textTitles);
    gfxPrint(0, 6, "Wave");
    gfxClearRect(x, 6, 34 - x, 1);
    gfxPrintf(x, 6, "%02d", (int)b->waveform);
    gfxSetFgColor(fieldNameColor(state));
    gfxPrint(x + 3, 6, waveName(b->waveform));
    return;
  }

  // Macro rows (4=Drive, 5=Glide, 6=SubWave, 7=SubLvl).
  fieldDrawLabel(0, row + 4, macroLabel(row), state);
  gfxSetFgColor(fieldValueColor(state));
  gfxClearRect(x, row + 4, kFieldNumberWidth, 1);
  if (uint8_t* value = macro(b, row)) {
    if (row == 6) {
      // SubWave is a picker (OFF/SINE/TRI/SAW/SQR) so the byte value 0..4
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
  // The five Bass808 macros (Wave is a picker, so it doesn't get a live bar).
  // 0=Drive, 1=Glide, 2=SubWave, 3=SubLvl, 4=unused.
  if (control < liveSource1 || control >= liveSource1 + 5) return false;
  const int macroIndex = control - liveSource1;
  if (macroIndex == 4) return false;  // unused slot
  *col = 0;
  *row = 4 + macroIndex + 1;  // skip row 3 (Wave) -> row 4 is Drive
  return true;
}

static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentBass808* b = &chipnomadState->project.instruments[cInstrument].chip.bass808;
  if (col && row >= 4 && row <= 8) {
    int ok = instrumentCommonOnEditVoicePost(col, row, action, b);
    if (ok) projectModified = 1;
    return ok;
  }
  int ok = 0;
  if (row == 3) {
    // Main waveform: cycle 0..3, hold for popup.
    ok = edit8noLast(action, &b->waveform, 1, 0, (int)Bass808Waveform::totalCount - 1);
  } else if (uint8_t* value = macro(b, row)) {
    if (row == 6) {
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
  // Hold-to-popup on the Wave (row 3) and SubWave (row 7) pickers, mirroring
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
  if (row == 7) {
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
  .rows = 11, .cursorRow = 0, .cursorCol = 0, .topRow = 0, .selectMode = -1,
  .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none, .getColumnCount = columns,
  .drawStatic = drawStatic, .drawCursor = drawCursor, .drawSelection = NULL,
  .drawRowHeader = NULL, .drawColHeader = NULL, .drawField = drawField, .onEdit = onEdit,
  .onInput = onInput, .onRawInput = NULL, .isCellValid = NULL, .getLoopRange = NULL,
};
