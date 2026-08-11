#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "model_catalog.h"

static void selectModel(int value) {
  chipnomadState->project.instruments[cInstrument].chip.braids.model = (uint8_t)value;
  projectModified = 1;
  screenSetup(&screenInstrument, cInstrument);
}

static void cancelModelSelection() { screenSetup(&screenInstrument, cInstrument); }

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
  return row == 11 ? 4 : 1;
}

static void drawStatic(void) {
  instrumentCommonDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  static const char* labels[] = {
    "Model", "Timbre", "Color", "Filter", "Mode", "Slope", "Cutoff",
    "Reso"
  };
  for (int i = 0; i < 8; i++) gfxPrint(0, 6 + i, labels[i]);
  gfxPrint(0, 14, "ADSR");
  gfxPrint(6, 14, "A"); gfxPrint(11, 14, "D");
  gfxPrint(16, 14, "S"); gfxPrint(21, 14, "R");
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  if (row == 11) gfxCursor(7 + col * 5, 14, 2);
  else gfxCursor(12, row + 3, row == 3 ? 24 : 5);
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) return instrumentCommonDrawField(col, row, state);
  InstrumentBraids* b = &chipnomadState->project.instruments[cInstrument].chip.braids;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  if (row == 11) {
    uint8_t values[] = {b->attack, b->decay, b->sustain, b->release};
    gfxPrint(7 + col * 5, 14, byteToHex(values[col]));
    return;
  }
  gfxClearRect(12, row + 3, 27, 1);
  switch (row) {
    case 3: gfxPrintf(12, 6, "%02d %s", b->model, modelNames[b->model <= 46 ? b->model : 0]); break;
    case 4: gfxPrintf(12, 7, "%04u", (unsigned)((uint32_t)b->timbre * 1023 / 32767)); break;
    case 5: gfxPrintf(12, 8, "%04u", (unsigned)((uint32_t)b->color * 1023 / 32767)); break;
    case 6: gfxPrint(12, 9, b->filterEnabled ? "On" : "Off"); break;
    case 7: { static const char* modes[] = {"LP", "BP", "HP"}; gfxPrint(12, 10, modes[b->filterMode <= 2 ? b->filterMode : 0]); break; }
    case 8: gfxPrint(12, 11, b->filterSlope24dB ? "24 dB" : "12 dB"); break;
    case 9: gfxPrintf(12, 12, "%u Hz", b->filterCutoffHz); break;
    case 10: gfxPrint(12, 13, byteToHex(b->filterResonance)); break;
  }
}

static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentBraids* b = &chipnomadState->project.instruments[cInstrument].chip.braids;
  int handled = 0;
  switch (row) {
    case 3:
      if (action == CellEditAction::tap) {
        selectionPopupSetup("BRAIDS MODEL", braidsCategories, braidsCategoryCount,
          b->model, selectModel, cancelModelSelection);
        screenSetup(&screenSelectionPopup, 0);
        return 0;
      }
      handled = edit8noLast(action, &b->model, 1, 0, 46);
      break;
    case 4: handled = editOscillatorParameter(action, &b->timbre); break;
    case 5: handled = editOscillatorParameter(action, &b->color); break;
    case 6: handled = edit8noLast(action, &b->filterEnabled, 1, 0, 1); break;
    case 7: handled = edit8noLast(action, &b->filterMode, 1, 0, 2); break;
    case 8: handled = edit8noLast(action, &b->filterSlope24dB, 1, 0, 1); break;
    case 9: handled = editFilterCutoff(action, &b->filterCutoffHz); break;
    case 10: handled = edit8noLast(action, &b->filterResonance, 16, 0, 255); break;
    case 11: {
      uint8_t* values[] = {&b->attack, &b->decay, &b->sustain, &b->release};
      handled = edit8noLast(action, values[col], 16, 0, 255);
      break;
    }
  }
  if (handled) projectModified = 1;
  return handled;
}

ScreenData screenInstrumentBraids = {
  .rows = 12,
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
  .onInput = NULL,
  .onRawInput = NULL,
  .isCellValid = NULL,
  .getLoopRange = NULL,
};
