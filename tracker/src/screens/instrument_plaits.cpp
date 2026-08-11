#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "model_catalog.h"

static void selectEngine(int value) {
  chipnomadState->project.instruments[cInstrument].chip.plaits.engine = (uint8_t)value;
  projectModified = 1;
  screenSetup(&screenInstrument, cInstrument);
}

static void cancelEngineSelection() { screenSetup(&screenInstrument, cInstrument); }

static const char* engineNames[] = {
  "VA VCF", "PHASE DIST", "6-OP FM 1", "6-OP FM 2", "6-OP FM 3", "WAVE TERRAIN",
  "STRING MACH", "CHIPTUNE", "VIRTUAL ANALOG", "WAVESHAPING", "2-OP FM", "FORMANT",
  "HARMONIC", "WAVETABLE", "CHORD", "SPEECH", "SWARM", "NOISE", "PARTICLE",
  "STRING", "MODAL", "BASS DRUM", "SNARE DRUM", "HI-HAT"
};

static int getColumnCount(int row) {
  if (row < 3) return instrumentCommonColumnCount(row);
  if (row == 14) {
    InstrumentPlaits* p = &chipnomadState->project.instruments[cInstrument].chip.plaits;
    return p->envelopeMode == 0 ? 2 : 4;
  }
  return 1;
}

static void drawStatic(void) {
  instrumentCommonDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  static const char* labels[] = {
    "Engine", "Harmonic", "Timbre", "Morph", "Main/Aux", "Env Mode",
    "Filter", "Mode", "Slope", "Cutoff", "Reso"
  };
  for (int i = 0; i < 11; ++i) gfxPrint(0, 6 + i, labels[i]);
  InstrumentPlaits* p = &chipnomadState->project.instruments[cInstrument].chip.plaits;
  if (p->envelopeMode == 0) {
    gfxPrint(0, 17, "LPG"); gfxPrint(6, 17, "D"); gfxPrint(11, 17, "C");
  } else {
    gfxPrint(0, 17, "ADSR");
    gfxPrint(6, 17, "A"); gfxPrint(11, 17, "D");
    gfxPrint(16, 17, "S"); gfxPrint(21, 17, "R");
  }
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  if (row == 14) gfxCursor(7 + col * 5, 17, 2);
  else gfxCursor(12, row + 3, row == 3 ? 24 : 5);
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) return instrumentCommonDrawField(col, row, state);
  InstrumentPlaits* p = &chipnomadState->project.instruments[cInstrument].chip.plaits;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  if (row == 14) {
    uint8_t values[] = {p->attack, p->decay, p->sustain, p->release};
    uint8_t value = p->envelopeMode == 0 ? values[col + 1] : values[col];
    gfxPrint(7 + col * 5, 17, byteToHex(value));
    return;
  }
  gfxClearRect(12, row + 3, 27, 1);
  switch (row) {
    case 3: gfxPrintf(12, 6, "%02d %s", p->engine, engineNames[p->engine < 24 ? p->engine : 0]); break;
    case 4: gfxPrintf(12, 7, "%04u", (unsigned)((uint32_t)p->harmonics * 1023 / 32767)); break;
    case 5: gfxPrintf(12, 8, "%04u", (unsigned)((uint32_t)p->timbre * 1023 / 32767)); break;
    case 6: gfxPrintf(12, 9, "%04u", (unsigned)((uint32_t)p->morph * 1023 / 32767)); break;
    case 7: gfxPrint(12, 10, byteToHex(p->auxMix)); break;
    case 8: { static const char* modes[] = {"TRIG", "LEVEL", "VCA"}; gfxPrint(12, 11, modes[p->envelopeMode <= 2 ? p->envelopeMode : 0]); break; }
    case 9: gfxPrint(12, 12, p->filterEnabled ? "On" : "Off"); break;
    case 10: { static const char* modes[] = {"LP", "BP", "HP"}; gfxPrint(12, 13, modes[p->filterMode <= 2 ? p->filterMode : 0]); break; }
    case 11: gfxPrint(12, 14, p->filterSlope24dB ? "24 dB" : "12 dB"); break;
    case 12: gfxPrintf(12, 15, "%u Hz", p->filterCutoffHz); break;
    case 13: gfxPrint(12, 16, byteToHex(p->filterResonance)); break;
  }
}

static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentPlaits* p = &chipnomadState->project.instruments[cInstrument].chip.plaits;
  int handled = 0;
  switch (row) {
    case 3:
      if (action == CellEditAction::tap) {
        selectionPopupSetup("PLAITS ENGINE", plaitsCategories, plaitsCategoryCount,
          p->engine, selectEngine, cancelEngineSelection);
        screenSetup(&screenSelectionPopup, 0);
        return 0;
      }
      handled = edit8noLast(action, &p->engine, 1, 0, 23);
      break;
    case 4: handled = editOscillatorParameter(action, &p->harmonics); break;
    case 5: handled = editOscillatorParameter(action, &p->timbre); break;
    case 6: handled = editOscillatorParameter(action, &p->morph); break;
    case 7: handled = edit8noLast(action, &p->auxMix, 16, 0, 255); break;
    case 8:
      handled = edit8noLast(action, &p->envelopeMode, 1, 0, 2);
      if (handled) screenSetup(&screenInstrument, cInstrument);
      break;
    case 9: handled = edit8noLast(action, &p->filterEnabled, 1, 0, 1); break;
    case 10: handled = edit8noLast(action, &p->filterMode, 1, 0, 2); break;
    case 11: handled = edit8noLast(action, &p->filterSlope24dB, 1, 0, 1); break;
    case 12: handled = editFilterCutoff(action, &p->filterCutoffHz); break;
    case 13: handled = edit8noLast(action, &p->filterResonance, 16, 0, 255); break;
    case 14: {
      uint8_t* values[] = {&p->attack, &p->decay, &p->sustain, &p->release};
      handled = edit8noLast(action, values[p->envelopeMode == 0 ? col + 1 : col], 16, 0, 255);
      break;
    }
  }
  if (handled) projectModified = 1;
  return handled;
}

ScreenData screenInstrumentPlaits = {
  .rows = 15, .cursorRow = 0, .cursorCol = 0, .topRow = 0, .selectMode = -1,
  .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none, .getColumnCount = getColumnCount,
  .drawStatic = drawStatic, .drawCursor = drawCursor, .drawSelection = NULL,
  .drawRowHeader = NULL, .drawColHeader = NULL, .drawField = drawField, .onEdit = onEdit,
  .onInput = NULL, .onRawInput = NULL, .isCellValid = NULL, .getLoopRange = NULL,
};
