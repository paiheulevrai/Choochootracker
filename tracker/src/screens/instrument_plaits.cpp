#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "utils.h"

static const char* engineNames[] = {
  "VA VCF", "PHASE DIST", "6-OP FM 1", "6-OP FM 2", "6-OP FM 3", "WAVE TERRAIN",
  "STRING MACH", "CHIPTUNE", "VIRTUAL ANALOG", "WAVESHAPING", "2-OP FM", "FORMANT",
  "HARMONIC", "WAVETABLE", "CHORD", "SPEECH", "SWARM", "NOISE", "PARTICLE",
  "STRING", "MODAL", "BASS DRUM", "SNARE DRUM", "HI-HAT"
};

static int getColumnCount(int row) { return row < 3 ? instrumentCommonColumnCount(row) : 1; }

static void drawStatic(void) {
  instrumentCommonDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  static const char* labels[] = {
    "Engine", "Harmonic", "Timbre", "Morph", "Main/Aux", "Filter", "Mode", "Slope",
    "Cutoff", "Reso", "Attack", "Decay", "Sustain", "Release"
  };
  for (int i = 0; i < 14; ++i) gfxPrint(0, 6 + i, labels[i]);
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  gfxCursor(12, row + 3, row == 3 ? 18 : 5);
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) return instrumentCommonDrawField(col, row, state);
  InstrumentPlaits* p = &chipnomadState->project.instruments[cInstrument].chip.plaits;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  gfxClearRect(12, row + 3, 24, 1);
  switch (row) {
    case 3: gfxPrintf(12, 6, "%02d %s", p->engine, engineNames[p->engine < 24 ? p->engine : 0]); break;
    case 4: gfxPrintf(12, 7, "%04X", p->harmonics); break;
    case 5: gfxPrintf(12, 8, "%04X", p->timbre); break;
    case 6: gfxPrintf(12, 9, "%04X", p->morph); break;
    case 7: gfxPrint(12, 10, byteToHex(p->auxMix)); break;
    case 8: gfxPrint(12, 11, p->filterEnabled ? "On" : "Off"); break;
    case 9: { static const char* modes[] = {"LP", "BP", "HP"}; gfxPrint(12, 12, modes[p->filterMode <= 2 ? p->filterMode : 0]); break; }
    case 10: gfxPrint(12, 13, p->filterSlope24dB ? "24 dB" : "12 dB"); break;
    case 11: gfxPrintf(12, 14, "%u Hz", p->filterCutoffHz); break;
    case 12: gfxPrint(12, 15, byteToHex(p->filterResonance)); break;
    case 13: gfxPrint(12, 16, byteToHex(p->attack)); break;
    case 14: gfxPrint(12, 17, byteToHex(p->decay)); break;
    case 15: gfxPrint(12, 18, byteToHex(p->sustain)); break;
    case 16: gfxPrint(12, 19, byteToHex(p->release)); break;
  }
}

static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentPlaits* p = &chipnomadState->project.instruments[cInstrument].chip.plaits;
  int handled = 0;
  switch (row) {
    case 3: handled = edit8noLast(action, &p->engine, 1, 0, 23); break;
    case 4: handled = edit16withMinMax(action, &p->harmonics, 256, 0, 32767); break;
    case 5: handled = edit16withMinMax(action, &p->timbre, 256, 0, 32767); break;
    case 6: handled = edit16withMinMax(action, &p->morph, 256, 0, 32767); break;
    case 7: handled = edit8noLast(action, &p->auxMix, 16, 0, 255); break;
    case 8: handled = edit8noLast(action, &p->filterEnabled, 1, 0, 1); break;
    case 9: handled = edit8noLast(action, &p->filterMode, 1, 0, 2); break;
    case 10: handled = edit8noLast(action, &p->filterSlope24dB, 1, 0, 1); break;
    case 11: handled = edit16withMinMax(action, &p->filterCutoffHz, 1000, 20, 43200); break;
    case 12: handled = edit8noLast(action, &p->filterResonance, 16, 0, 255); break;
    case 13: handled = edit8noLast(action, &p->attack, 16, 0, 255); break;
    case 14: handled = edit8noLast(action, &p->decay, 16, 0, 255); break;
    case 15: handled = edit8noLast(action, &p->sustain, 16, 0, 255); break;
    case 16: handled = edit8noLast(action, &p->release, 16, 0, 255); break;
  }
  if (handled) projectModified = 1;
  return handled;
}

ScreenData screenInstrumentPlaits = {
  .rows = 17, .cursorRow = 0, .cursorCol = 0, .topRow = 0, .selectMode = -1,
  .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none, .getColumnCount = getColumnCount,
  .drawStatic = drawStatic, .drawCursor = drawCursor, .drawSelection = NULL,
  .drawRowHeader = NULL, .drawColHeader = NULL, .drawField = drawField, .onEdit = onEdit,
  .onInput = NULL, .onRawInput = NULL, .isCellValid = NULL, .getLoopRange = NULL,
};
