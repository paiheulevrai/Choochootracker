#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "model_catalog.h"

static int modelButtonDown;
static void selectModel(int value) { chipnomadState->project.instruments[cInstrument].chip.sintered.model = (SinteredModel)value; projectModified = 1; screenSetup(&screenInstrument, cInstrument); }
static void cancelModelSelection() { screenSetup(&screenInstrument, cInstrument); }
static void openModelSelection() {
  InstrumentSintered* s = &chipnomadState->project.instruments[cInstrument].chip.sintered;
  selectionPopupSetup("SINTERED MODEL", sinteredCategories, sinteredCategoryCount, (int)s->model, selectModel, cancelModelSelection);
  screenSetup(&screenSelectionPopup, 0);
}
static const char* modelName(SinteredModel m) { static const char* names[] = {"KNOT", "SHARD", "BURST", "COMB", "LOGIC", "MELT"}; int i = (int)m; return i >= 0 && i < 6 ? names[i] : "KNOT"; }
static const char* macroName(SinteredModel m, int i) {
  static const char* names[][6] = {
    {"Decay","Mod","Ratio","Spread","Motion","Fold"}, {"Decay","Mod","Ratio","Feedback","Motion","Bite"},
    {"Decay","Mod","Noise","Color","Motion","Feedback"}, {"Decay","Mod","Time","Damping","Motion","Regen"},
    {"Decay","Mod","Rate","Pattern","Motion","Crush"}, {"Decay","Mod","Ratio","Chaos","Motion","Drive"}
  }; return names[(int)m < 6 ? (int)m : 0][i];
}
static uint8_t* macro(InstrumentSintered* s, int i) { uint8_t* values[] = {&s->decay, &s->mod, &s->a, &s->b, &s->motion, &s->c}; return i >= 0 && i < 6 ? values[i] : NULL; }
static int columns(int row) { return row < 3 ? instrumentCommonColumnCount(row) : row == 3 ? 1 : row <= 8 ? 2 : 1; }
static void drawStatic(void) {
  instrumentCommonDrawStatic(); InstrumentSintered* s = &chipnomadState->project.instruments[cInstrument].chip.sintered;
  gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0, 6, "Model"); gfxPrint(0, 7, "SINTERED"); gfxSetFgColor(appSettings.colorScheme.textDefault);
  for (int i = 0; i < 6; ++i) gfxPrint(0, 8 + i, macroName(s->model, i)); instrumentCommonDrawVoicePostStatic(0);
}
static void drawCursor(int col, int row) { if (row < 3) { instrumentCommonDrawCursor(col, row); return; } if (col && row >= 4 && row <= 8) { instrumentCommonDrawVoicePostCursor(col, row); return; } gfxCursor(11, row == 3 ? 6 : row + 4, 7); }
static void drawField(int col, int row, CellState state) {
  if (row < 3) { instrumentCommonDrawField(col, row, state); return; } InstrumentSintered* s = &chipnomadState->project.instruments[cInstrument].chip.sintered;
  if (col && row >= 4 && row <= 8) { instrumentCommonDrawVoicePostField(col, row, state, s); return; }
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault); gfxClearRect(11, row == 3 ? 6 : row + 4, row == 3 ? 20 : 7, 1);
  if (row == 3) gfxPrintf(11, 6, "%02d %s", (int)s->model, modelName(s->model)); else gfxPrint(11, row + 4, byteToHex(*macro(s, row - 4)));
}
static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action); InstrumentSintered* s = &chipnomadState->project.instruments[cInstrument].chip.sintered;
  if (col && row >= 4 && row <= 8) { int ok = instrumentCommonOnEditVoicePost(col, row, action, s); if (ok) projectModified = 1; return ok; }
  int ok = row == 3 ? edit8noLast(action, (uint8_t*)&s->model, 1, 0, 5) : edit8noLast(action, macro(s, row - 4), 16, 0, 255);
  if (ok) { projectModified = 1; screenFullRedraw(&screenInstrumentSintered); } return ok;
}
static int onInput(int down, int keys, int) { if (screenInstrumentSintered.cursorRow != 3) { modelButtonDown = 0; return 0; } PopupEditInput input = popupEditInput(down, keys, &modelButtonDown); if (input == PopupEditInput::cycle) { cycle8((uint8_t*)&chipnomadState->project.instruments[cInstrument].chip.sintered.model, keys == (keyEdit | keyRight) ? 1 : -1, 0, 5, 0); projectModified = 1; screenFullRedraw(&screenInstrumentSintered); return 1; } if (input == PopupEditInput::hold) return 1; if (input == PopupEditInput::open) { openModelSelection(); return 1; } return 0; }
ScreenData screenInstrumentSintered = {.rows=10,.cursorRow=0,.cursorCol=0,.topRow=0,.selectMode=-1,.selectStartRow=0,.selectStartCol=0,.selectAnchorRow=0,.selectAnchorCol=0,.playbackLevel=ScreenPlaybackLevel::none,.getColumnCount=columns,.drawStatic=drawStatic,.drawCursor=drawCursor,.drawSelection=NULL,.drawRowHeader=NULL,.drawColHeader=NULL,.drawField=drawField,.onEdit=onEdit,.onInput=onInput,.onRawInput=NULL,.isCellValid=NULL,.getLoopRange=NULL};
