#include "screen_instrument.h"
#include "selection_popup.h"
#include "corelib_gfx.h"
#include "opll_presets.h"
#include "chipnomad_lib.h"
#include "utils.h"
#include <cstring>

static int presetButtonDown;
static SelectionItem programs[15];
static Instrument* current() { return &chipnomadState->project.instruments[cInstrument]; }
static void previewPreset(int program, bool held) {
  Instrument temporary = *current();
  if (held && opllApplyPreset(&temporary, program)) chipnomadQueueOPLLPreview(chipnomadState, *pSongTrack, &temporary.chip.opll);
  else chipnomadQueueOPLLPreview(chipnomadState, *pSongTrack, nullptr);
}
static void selectPreset(int program) {
  previewPreset(0, false);
  if (opllApplyPreset(current(), program)) projectModified = 1;
  screenSetup(&screenInstrument, cInstrument);
}
static void cancelPreset() { previewPreset(0, false); screenSetup(&screenInstrument, cInstrument); }
static void openPresets() {
  for (int i = 0; i < 15; ++i) programs[i] = {opllPresetName(current()->type, i + 1), i + 1, nullptr, 0};
  selectionPopupSetup(current()->type == InstrumentType::VRC7 ? "VRC7 PROGRAMS" : "YM2413 PROGRAMS",
    programs, 15, current()->chip.opll.program, selectPreset, cancelPreset, true, previewPreset);
  screenSetup(&screenSelectionPopup, 0);
}
static int columns(int row) { return row < 3 ? instrumentCommonColumnCount(row) : row == 4 || row == 5 ? 2 : row == 7 ? 5 : 1; }
static void drawStatic() {
  instrumentCommonDrawStatic();
  instrumentFMAmpDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(0, 6, "Bank"); gfxPrint(9, 6, current()->type == InstrumentType::VRC7 ? "VRC7 / DS1001" : "OPLL / YM2413");
  gfxPrint(0, 7, "Preset"); gfxPrint(0, 9, "Fine ct");
}
static void drawCursor(int col, int row) {
  if (row == 5) { instrumentFMToneDrawCursor(col); return; }
  if (row == 4 && col == 1) { gfxCursor(16, 9, 4); return; }
  if (row >= 6) { instrumentFMAmpDrawCursor(col, row - 6); return; }
  if (row < 3) instrumentCommonDrawCursor(col, row);
  else gfxCursor(9, row == 3 ? 7 : 9, row == 3 ? 27 : 4);
}
static void drawField(int col, int row, CellState state) {
  if (row == 5) { instrumentFMToneDrawField(col, state); return; }
  if (row >= 6) { instrumentFMAmpDrawField(col, row - 6, state); return; }
  if (row < 3) { instrumentCommonDrawField(col, row, state); return; }
  if (row == 4 && col == 1) {
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textInfo);
    gfxClearRect(16, 9, 4, 1); gfxPrint(16, 9, "EDIT"); return;
  }
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  gfxClearRect(9, row == 3 ? 7 : 9, row == 3 ? 30 : 6, 1);
  if (row == 3) gfxPrintf(9, 7, "%02d %.27s", current()->chip.opll.program, current()->chip.opll.program ? opllPresetName(current()->type, current()->chip.opll.program) : "Custom");
  else gfxPrintf(9, 9, "%+04d", current()->chip.opll.fineTune);
  instrumentFMRefreshStaticWaveform();
}
static int onEdit(int col, int row, CellEditAction action) {
  if (row == 5) return instrumentFMToneEdit(col, action);
  if (row >= 6) return instrumentFMAmpEdit(col, row - 6, action);
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  if (row == 3) {
    uint8_t program = current()->chip.opll.program;
    if (!edit8noLast(action, &program, 1, 1, 15)) return 0;
    selectPreset(program); return 1;
  }
  if (row == 4 && col == 1) { screenSetup(&screenFMEdit, cInstrument); return 1; }
  int value = current()->chip.opll.fineTune;
  action = convertMultiAction(action);
  if (action == CellEditAction::clear) value = 0;
  else if (action == CellEditAction::increase) ++value;
  else if (action == CellEditAction::decrease) --value;
  else if (action == CellEditAction::increaseBig) value += 10;
  else if (action == CellEditAction::decreaseBig) value -= 10;
  else return 0;
  current()->chip.opll.fineTune = value < -100 ? -100 : value > 100 ? 100 : value;
  projectModified = 1; return 1;
}
static int onInput(int down, int keys, int) {
  if (screenInstrumentOPLL.cursorRow != 3) { presetButtonDown = 0; return 0; }
  PopupEditInput input = popupEditInput(down, keys, &presetButtonDown);
  if (input == PopupEditInput::cycle) {
    int value = current()->chip.opll.program + (keys == (keyEdit | keyRight) ? 1 : -1);
    selectPreset(value < 1 ? 15 : value > 15 ? 1 : value); return 1;
  }
  if (input == PopupEditInput::hold) return 1;
  if (input == PopupEditInput::open) { openPresets(); return 1; }
  return 0;
}
ScreenData screenInstrumentOPLL = {
  .rows = 8, .cursorRow = 0, .cursorCol = 0, .topRow = 0, .selectMode = -1,
  .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none, .getColumnCount = columns, .drawStatic = drawStatic,
  .drawCursor = drawCursor, .drawSelection = nullptr, .drawRowHeader = nullptr, .drawColHeader = nullptr,
  .drawField = drawField, .onEdit = onEdit, .onInput = onInput, .onRawInput = nullptr,
  .isCellValid = nullptr, .getLoopRange = nullptr,
};
