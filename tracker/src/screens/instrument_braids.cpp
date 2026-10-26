#include "screen_instrument.h"
#include "field_style.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "model_catalog.h"
#include "waveform_display.h"
#include <string.h>

static int modelButtonDown;
static Bitmap* previewBitmap;

static const char* paraphonicChordName(uint16_t color) {
  static const char* names[] = {"DETUNE", "OCT", "2/5/OCT", "MIN7", "MIN", "MIN9", "MIN11", "5TH", "SPREAD", "MAJ11", "MAJ9", "MAJ", "MAJ7", "SUS4", "DETUNE", "OCT+", "OCT++"};
  int index = (int)((uint32_t)color * 17 / 32768);
  return names[index > 16 ? 16 : index];
}

static void drawPreview(const InstrumentBraids* braids) {
  if (!previewBitmap) previewBitmap = gfxBitmapCreate(32, 3);
  renderBraidsPreview(previewBitmap, braids);
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxDrawBitmap(previewBitmap, 0, 16);
}

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
  gfxSetFgColor(appSettings.colorScheme.textLabel);
  gfxPrint(0,8,"Timbre");
  gfxPrint(0,9,"Color");
  instrumentCommonDrawVoicePostStatic(1);
  drawPreview(&chipnomadState->project.instruments[cInstrument].chip.braids);
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  if (instrumentCommonDrawVoicePostCursor(col, row)) return;
  else if (row == 3) gfxCursor(kColumnValueX[0], 6, 3 + strlen(modelCatalogName(InstrumentType::Braids, chipnomadState->project.instruments[cInstrument].chip.braids.model)));
  else if (!col) gfxCursor(kColumnValueX[0], row + 4, row == 5 && chipnomadState->project.instruments[cInstrument].chip.braids.model == 40 ? strlen(paraphonicChordName(chipnomadState->project.instruments[cInstrument].chip.braids.color)) : kFieldNumberWidth);
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) return instrumentCommonDrawField(col, row, state);
  InstrumentBraids* b = &chipnomadState->project.instruments[cInstrument].chip.braids;
  if (instrumentCommonDrawVoicePostField(col, row, state, b)) return;
  // The selected field's label lights up with its value; Model is a title.
  if (row == 3) {
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textTitles);
    gfxPrint(0, 6, "Model");
  } else if (!col && row <= 5) {
    fieldDrawLabel(0, row + 4, row == 4 ? "Timbre" : "Color", state);
  }
  gfxSetFgColor(fieldValueColor(state));
  const int x = kColumnValueX[0];
  if (row == 3) gfxClearRect(x, 6, 34 - x, 1); else if (!col) gfxClearRect(x, row + 4, kFieldNumberWidth, 1);
  switch (row) {
    case 3:
      gfxPrintf(x, 6, "%02d", b->model);
      gfxSetFgColor(fieldNameColor(state));
      gfxPrint(x + 3, 6, modelCatalogName(InstrumentType::Braids, b->model));
      break;
    case 4: if (!col) fieldDrawNumber(x, 8, controlFromRange(b->timbre, 32767), state, -1.0f, false, instrumentLivePosition(liveSource1)); break;
    case 5: if (!col) {
      if (b->model == 40) {
        gfxSetFgColor(fieldNameColor(state));
        gfxPrint(x, 9, paraphonicChordName(b->color));
      } else {
        fieldDrawNumber(x, 9, controlFromRange(b->color, 32767), state, -1.0f, false, instrumentLivePosition(liveSource2));
      }
      break;
    }
  }
}

bool instrumentBraidsLiveCell(int control, int* col, int* row) {
  // Timbre, and Color unless it names a chord.
  if (control == liveSource2 && chipnomadState->project.instruments[cInstrument].chip.braids.model == 40) return false;
  if (control != liveSource1 && control != liveSource2) return false;
  *col = 0;
  *row = control == liveSource1 ? 4 : 5;
  return true;
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
  if (handled) { projectModified = 1; screenFullRedraw(&screenInstrumentBraids); }
  return handled;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (screenInstrumentBraids.cursorRow != 3) {
    modelButtonDown = 0;
    return 0;
  }
  PopupEditInput input = popupEditInput(isKeyDown, keys, &modelButtonDown);
  if (input == PopupEditInput::cycle) {
    cycle8(&chipnomadState->project.instruments[cInstrument].chip.braids.model,
      keys == (keyEdit | keyRight) ? 1 : -1, 0, 46, 0);
    projectModified = 1;
    screenFullRedraw(&screenInstrumentBraids);
    return 1;
  }
  if (input == PopupEditInput::hold) return 1;
  if (input == PopupEditInput::open) {
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
