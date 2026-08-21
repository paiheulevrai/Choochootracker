#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "model_catalog.h"

static int modelButtonDown;

static void selectModel(int value) {
  chipnomadState->project.instruments[cInstrument].chip.braids.model = (uint8_t)value;
  projectModified = 1;
  screenSetup(&screenInstrument, cInstrument);
}

static void cancelModelSelection() { screenSetup(&screenInstrument, cInstrument); }

static void openModelSelection() {
  InstrumentBraids* b = &chipnomadState->project.instruments[cInstrument].chip.braids;
  selectionPopupSetup("BRAIDS MODEL", braidsCategories, braidsCategoryCount,
    b->model, selectModel, cancelModelSelection);
  screenSetup(&screenSelectionPopup, 0);
}

static int getColumnCount(int row) {
  if (row < 3) return instrumentCommonColumnCount(row);
  if (row == 3) return 1;
  return row == 9 ? 5 : 2;
}

static void drawStatic(void) {
  instrumentCommonDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0,6,"Model");
  gfxPrint(0,7,"SOURCE");
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(0,8,"Timbre");
  gfxPrint(0,9,"Color");
  instrumentCommonDrawVoicePostStatic(1);
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  if (instrumentCommonDrawVoicePostCursor(col, row)) return;
  else if (row == 3) gfxCursor(11, 6, 28);
  else gfxCursor(col ? 26 : 11, row + 4, col ? 8 : 7);
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) return instrumentCommonDrawField(col, row, state);
  InstrumentBraids* b = &chipnomadState->project.instruments[cInstrument].chip.braids;
  if (instrumentCommonDrawVoicePostField(col, row, state, b)) return;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  if (row == 3) gfxClearRect(11, 6, 29, 1); else gfxClearRect(col ? 26 : 11, row + 4, col ? 8 : 7, 1);
  switch (row) {
    case 3: gfxPrintf(12, 6, "%02d %s", b->model, modelCatalogName(InstrumentType::Braids, b->model)); break;
    case 4: if (!col) gfxPrintf(11,8,"%04u",(unsigned)((uint32_t)b->timbre*1023/32767)); break;
    case 5: if (!col) gfxPrintf(11,9,"%04u",(unsigned)((uint32_t)b->color*1023/32767)); break;
  }
}

static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentBraids* b = &chipnomadState->project.instruments[cInstrument].chip.braids;
  if ((row == 9) || (col && row >= 4 && row <= 8)) {
    int handled = instrumentCommonOnEditVoicePost(col, row, action, b);
    if (handled) projectModified = 1;
    return handled;
  }
  int handled = 0;
  switch (row) {
    case 3:
      handled = edit8noLast(action, &b->model, 1, 0, 46);
      break;
    case 4: handled = !col ? editOscillatorParameter(action,&b->timbre) : 0; break;
    case 5: handled = !col ? editOscillatorParameter(action,&b->color) : 0; break;
  }
  if (handled) projectModified = 1;
  return handled;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (screenInstrumentBraids.cursorRow != 3) {
    modelButtonDown = 0;
    return 0;
  }
  if (isKeyDown && keys == keyEdit) {
    modelButtonDown = 1;
    return 1;
  }
  if (isKeyDown && (keys == (keyEdit | keyLeft) ||
                    keys == (keyEdit | keyRight))) {
    cycle8(&chipnomadState->project.instruments[cInstrument].chip.braids.model,
      keys == (keyEdit | keyRight) ? 1 : -1, 0, 46, 0);
    projectModified = 1;
    screenFullRedraw(&screenInstrumentBraids);
    modelButtonDown = 0;
    return 1;
  }
  if (!isKeyDown && keys == 0 && modelButtonDown) {
    modelButtonDown = 0;
    openModelSelection();
    return 1;
  }
  return 0;
}

ScreenData screenInstrumentBraids = {
  .rows = 10,
  .cursorRow = 0,
  .cursorCol = 0,
  .topRow = 0,
  .selectMode = -1,
  .selectStartRow = 0,
  .selectStartCol = 0,
  .selectAnchorRow = 0,
  .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = getColumnCount,
  .drawStatic = drawStatic,
  .drawCursor = drawCursor,
  .drawSelection = NULL,
  .drawRowHeader = NULL,
  .drawColHeader = NULL,
  .drawField = drawField,
  .onEdit = onEdit,
  .onInput = onInput,
  .onRawInput = NULL,
  .isCellValid = NULL,
  .getLoopRange = NULL,
};
