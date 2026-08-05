#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "utils.h"

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
  return row < 3 ? instrumentCommonColumnCount(row) : 1;
}

static void drawStatic(void) {
  instrumentCommonDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  static const char* labels[] = {
    "Model", "Timbre", "Color", "Filter", "Mode", "Slope", "Cutoff",
    "Reso", "Attack", "Decay", "Sustain", "Release"
  };
  for (int i = 0; i < 12; i++) gfxPrint(0, 6 + i, labels[i]);
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  gfxCursor(12, row + 3, row == 3 ? 17 : 5);
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) return instrumentCommonDrawField(col, row, state);
  InstrumentBraids* b = &chipnomadState->project.instruments[cInstrument].chip.braids;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  gfxClearRect(12, row + 3, 20, 1);
  switch (row) {
    case 3: gfxPrintf(12, 6, "%02d %s", b->model, modelNames[b->model <= 46 ? b->model : 0]); break;
    case 4: gfxPrintf(12, 7, "%04X", b->timbre); break;
    case 5: gfxPrintf(12, 8, "%04X", b->color); break;
    case 6: gfxPrint(12, 9, b->filterEnabled ? "On" : "Off"); break;
    case 7: { static const char* modes[] = {"LP", "BP", "HP"}; gfxPrint(12, 10, modes[b->filterMode <= 2 ? b->filterMode : 0]); break; }
    case 8: gfxPrint(12, 11, b->filterSlope24dB ? "24 dB" : "12 dB"); break;
    case 9: gfxPrintf(12, 12, "%u Hz", b->filterCutoffHz); break;
    case 10: gfxPrint(12, 13, byteToHex(b->filterResonance)); break;
    case 11: gfxPrint(12, 14, byteToHex(b->attack)); break;
    case 12: gfxPrint(12, 15, byteToHex(b->decay)); break;
    case 13: gfxPrint(12, 16, byteToHex(b->sustain)); break;
    case 14: gfxPrint(12, 17, byteToHex(b->release)); break;
  }
}

static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentBraids* b = &chipnomadState->project.instruments[cInstrument].chip.braids;
  int handled = 0;
  switch (row) {
    case 3: handled = edit8noLast(action, &b->model, 1, 0, 46); break;
    case 4: handled = edit16withMinMax(action, &b->timbre, 256, 0, 32767); break;
    case 5: handled = edit16withMinMax(action, &b->color, 256, 0, 32767); break;
    case 6: handled = edit8noLast(action, &b->filterEnabled, 1, 0, 1); break;
    case 7: handled = edit8noLast(action, &b->filterMode, 1, 0, 2); break;
    case 8: handled = edit8noLast(action, &b->filterSlope24dB, 1, 0, 1); break;
    case 9: handled = edit16withMinMax(action, &b->filterCutoffHz, 1000, 20, 43200); break;
    case 10: handled = edit8noLast(action, &b->filterResonance, 16, 0, 255); break;
    case 11: handled = edit8noLast(action, &b->attack, 16, 0, 255); break;
    case 12: handled = edit8noLast(action, &b->decay, 16, 0, 255); break;
    case 13: handled = edit8noLast(action, &b->sustain, 16, 0, 255); break;
    case 14: handled = edit8noLast(action, &b->release, 16, 0, 255); break;
  }
  if (handled) projectModified = 1;
  return handled;
}

ScreenData screenInstrumentBraids = {
  .rows = 15,
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
