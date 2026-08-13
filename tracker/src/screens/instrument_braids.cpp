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

static const char* modelNames[] = {
  "CSAW", "MORPH", "SAW-SQUARE", "SINE-TRI", "BUZZ", "SQUARE-SUB",
  "SAW-SUB", "SQUARE-SYNC", "SAW-SYNC", "TRIPLE-SAW", "TRIPLE-SQR",
  "TRIPLE-TRI", "TRIPLE-SINE", "TRIPLE-RING", "SAW-SWARM", "SAW-COMB",
  "TOY", "FILTER-LP", "FILTER-PEAK", "FILTER-BP", "FILTER-HP", "VOSIM",
  "VOWEL", "VOWEL-FOF", "HARMONICS", "FM", "FEEDBACK-FM", "CHAOTIC-FM",
  "PLUCKED", "BOWED", "BLOWN", "FLUTED", "STRUCK-BELL", "STRUCK-DRUM",
  "KICK", "CYMBAL", "SNARE", "WAVETABLES", "WAVE-MAP", "WAVE-LINE",
  "WAVE-PARA", "FILTER-NOISE", "TWIN-PEAKS", "CLOCK-NOISE", "GRAN-CLOUD",
  "PARTICLE", "DIGI-MOD"
};

static int getColumnCount(int row) {
  if (row < 3) return instrumentCommonColumnCount(row);
  if (row == 3) return 1;
  return row == 9 ? 5 : 2;
}

static void drawStatic(void) {
  instrumentCommonDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(0,6,"Model"); gfxPrint(0,7,"Timbre"); gfxPrint(21,7,"Filter");
  gfxPrint(0,8,"Color"); gfxPrint(21,8,"Mode"); gfxPrint(21,9,"Slope"); gfxPrint(21,10,"Cutoff"); gfxPrint(21,11,"Reso");
  gfxPrint(0,13,"ADSR"); gfxPrint(6,13,"A"); gfxPrint(11,13,"D"); gfxPrint(16,13,"S"); gfxPrint(21,13,"R"); gfxPrint(27,13,"Shape");
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  if (row == 9) gfxCursor(col == 4 ? 35 : 7 + col * 5, 13, 2);
  else if (row == 3) gfxCursor(11, 6, 28);
  else gfxCursor(col ? 31 : 11, row + 3, col ? 8 : 9);
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) return instrumentCommonDrawField(col, row, state);
  InstrumentBraids* b = &chipnomadState->project.instruments[cInstrument].chip.braids;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  if (row == 9) {
    uint8_t values[] = {b->attack, b->decay, b->sustain, b->release, b->envelopeShape};
    gfxPrint(col == 4 ? 35 : 7 + col * 5, 13, byteToHex(values[col]));
    return;
  }
  if (row == 3) gfxClearRect(11, 6, 29, 1); else gfxClearRect(col ? 31 : 11, row + 3, col ? 9 : 10, 1);
  switch (row) {
    case 3: gfxPrintf(12, 6, "%02d %s", b->model, modelNames[b->model <= 46 ? b->model : 0]); break;
    case 4: if (!col) gfxPrintf(11,7,"%04u",(unsigned)((uint32_t)b->timbre*1023/32767)); else gfxPrint(31,7,b->filterEnabled?"On":"Off"); break;
    case 5: if (!col) gfxPrintf(11,8,"%04u",(unsigned)((uint32_t)b->color*1023/32767)); else { static const char* m[]={"LP","BP","HP"}; gfxPrint(31,8,m[b->filterMode<=2?b->filterMode:0]); } break;
    case 6: if (col) gfxPrint(31,9,b->filterSlope24dB?"24 dB":"12 dB"); break;
    case 7: if (col) gfxPrintf(31,10,"%u Hz",b->filterCutoffHz); break;
    case 8: if (col) gfxPrint(31,11,byteToHex(b->filterResonance)); break;
  }
}

static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentBraids* b = &chipnomadState->project.instruments[cInstrument].chip.braids;
  int handled = 0;
  switch (row) {
    case 3:
      handled = edit8noLast(action, &b->model, 1, 0, 46);
      break;
    case 4: handled = !col ? editOscillatorParameter(action,&b->timbre) : edit8noLast(action,&b->filterEnabled,1,0,1); break;
    case 5: handled = !col ? editOscillatorParameter(action,&b->color) : edit8noLast(action,&b->filterMode,1,0,2); break;
    case 6: handled = col ? edit8noLast(action,&b->filterSlope24dB,1,0,1) : 0; break;
    case 7: handled = col ? editFilterCutoff(action,&b->filterCutoffHz) : 0; break;
    case 8: handled = col ? edit8noLast(action,&b->filterResonance,16,0,255) : 0; break;
    case 9: {
      uint8_t* values[] = {&b->attack, &b->decay, &b->sustain, &b->release, &b->envelopeShape};
      handled = edit8noLast(action, values[col], 16, 0, 255);
      break;
    }
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
    modelButtonDown = 0;
    return 0;
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
