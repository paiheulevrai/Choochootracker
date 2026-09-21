#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "model_catalog.h"

static int modelButtonDown;
static void selectModel(int value) {
  chipnomadState->project.instruments[cInstrument].chip.mme.model = (MMEModel)value;
  projectModified = 1; screenSetup(&screenInstrument, cInstrument);
}
static void cancelModelSelection() { screenSetup(&screenInstrument, cInstrument); }
static void openModelSelection() {
  InstrumentMME* m = &chipnomadState->project.instruments[cInstrument].chip.mme;
  selectionPopupSetup("MME MODEL", mmeCategories, mmeCategoryCount, (int)m->model,
    selectModel, cancelModelSelection);
  screenSetup(&screenSelectionPopup, 0);
}

static const char* modelName(MMEModel model) {
  static const char* names[] = {"RING", "FOLD", "CROSS", "VPM", "SYNC", "LOGIC", "VOCODE"};
  int i = (int)model; return i >= 0 && i < 7 ? names[i] : "RING";
}
static const char* macroName(MMEModel model, int macro) {
  static const char* generic[] = {"Waves", "Interval", "Amount", "Flow", "Feedback", "Shaper"};
  static const char* ring[] = {"Waves", "Interval", "Amount", "RingType", "Feedback", "Shaper"};
  static const char* sync[] = {"Waves", "Interval", "SyncAmt", "Reset", "Feedback", "Shaper"};
  static const char* logic[] = {"Waves", "Interval", "Amount", "Logic", "Feedback", "Shaper"};
  static const char* vocode[] = {"Waves", "Interval", "Analyze", "Formant", "Feedback", "Shaper"};
  const char* const* names = model == MMEModel::ring ? ring : model == MMEModel::sync ? sync :
    model == MMEModel::logic ? logic : model == MMEModel::vocode ? vocode : generic;
  return names[macro];
}
static uint8_t* macro(InstrumentMME* m, int index) {
  uint8_t* values[] = {&m->waves, &m->interval, &m->amount, &m->flow, &m->feedback, &m->shaper};
  return index >= 0 && index < 6 ? values[index] : NULL;
}
static int columns(int row) { return row < 3 ? instrumentCommonColumnCount(row) : row == 3 ? 1 : row <= 8 ? 2 : 1; }
static void drawStatic(void) {
  instrumentCommonDrawStatic(); InstrumentMME* m = &chipnomadState->project.instruments[cInstrument].chip.mme;
  gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0, 6, "Model"); gfxPrint(0, 7, "MME");
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  for (int i = 0; i < 6; ++i) gfxPrint(0, 8 + i, macroName(m->model, i));
  instrumentCommonDrawVoicePostStatic(0);
}
static void drawCursor(int col, int row) {
  if (row < 3) { instrumentCommonDrawCursor(col, row); return; }
  if (col && row >= 4 && row <= 8) { instrumentCommonDrawVoicePostCursor(col, row); return; }
  gfxCursor(11, row == 3 ? 6 : row + 4, row == 3 ? 28 : 7);
}
static void drawField(int col, int row, CellState state) {
  if (row < 3) { instrumentCommonDrawField(col, row, state); return; }
  InstrumentMME* m = &chipnomadState->project.instruments[cInstrument].chip.mme;
  if (col && row >= 4 && row <= 8) { instrumentCommonDrawVoicePostField(col, row, state, m); return; }
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  gfxClearRect(11, row == 3 ? 6 : row + 4, row == 3 ? 20 : 7, 1);
  if (row == 3) gfxPrintf(11, 6, "%02d %s", (int)m->model, modelName(m->model));
  else if (uint8_t* value = macro(m, row - 4)) gfxPrint(11, row + 4, byteToHex(*value));
}
static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentMME* m = &chipnomadState->project.instruments[cInstrument].chip.mme;
  if (col && row >= 4 && row <= 8) { int ok = instrumentCommonOnEditVoicePost(col, row, action, m); if (ok) projectModified = 1; return ok; }
  int ok = row == 3 ? edit8noLast(action, (uint8_t*)&m->model, 1, 0, 6) :
    (macro(m, row - 4) ? edit8noLast(action, macro(m, row - 4), 16, 0, 255) : 0);
  if (ok) { projectModified = 1; screenFullRedraw(&screenInstrumentMME); } return ok;
}
static int onInput(int isKeyDown, int keys, int) {
  if (screenInstrumentMME.cursorRow != 3) { modelButtonDown = 0; return 0; }
  PopupEditInput input = popupEditInput(isKeyDown, keys, &modelButtonDown);
  if (input == PopupEditInput::cycle) {
    cycle8((uint8_t*)&chipnomadState->project.instruments[cInstrument].chip.mme.model,
      keys == (keyEdit | keyRight) ? 1 : -1, 0, 6, 0);
    projectModified = 1; screenFullRedraw(&screenInstrumentMME); return 1;
  }
  if (input == PopupEditInput::hold) return 1;
  if (input == PopupEditInput::open) { openModelSelection(); return 1; }
  return 0;
}
ScreenData screenInstrumentMME = {
  .rows = 10, .cursorRow = 0, .cursorCol = 0, .topRow = 0, .selectMode = -1,
  .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none, .getColumnCount = columns, .drawStatic = drawStatic,
  .drawCursor = drawCursor, .drawSelection = NULL, .drawRowHeader = NULL, .drawColHeader = NULL,
  .drawField = drawField, .onEdit = onEdit, .onInput = onInput, .onRawInput = NULL,
  .isCellValid = NULL, .getLoopRange = NULL,
};
