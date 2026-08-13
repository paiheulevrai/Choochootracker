#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "model_catalog.h"

static int engineButtonDown;

static bool isAlt(void) {
  return chipnomadState->project.instruments[cInstrument].type == InstrumentType::PlaitsAlt;
}

static void selectEngine(int value) {
  chipnomadState->project.instruments[cInstrument].chip.plaits.engine = (uint8_t)value;
  projectModified = 1;
  screenSetup(&screenInstrument, cInstrument);
}

static void cancelEngineSelection() { screenSetup(&screenInstrument, cInstrument); }

static void openEngineSelection() {
  InstrumentPlaits* p = &chipnomadState->project.instruments[cInstrument].chip.plaits;
  selectionPopupSetup(isAlt() ? "PLAITS-ALT ENGINE" : "PLAITS ENGINE",
    isAlt() ? plaitsAltCategories : plaitsCategories,
    isAlt() ? plaitsAltCategoryCount : plaitsCategoryCount,
    p->engine, selectEngine, cancelEngineSelection);
  screenSetup(&screenSelectionPopup, 0);
}

static const char* engineNames[] = {
  "VA VCF", "PHASE DIST", "6-OP FM 1", "6-OP FM 2", "6-OP FM 3", "WAVE TERRAIN",
  "STRING MACH", "CHIPTUNE", "VIRTUAL ANALOG", "WAVESHAPING", "2-OP FM", "FORMANT",
  "HARMONIC", "WAVETABLE", "CHORD", "SPEECH", "SWARM", "NOISE", "PARTICLE",
  "STRING", "MODAL", "BASS DRUM", "SNARE DRUM", "HI-HAT"
};
static const char* altEngineNames[] = {
  "GLISSON", "PULSAR", "GENDY", "SCANNED", "LOOPBACK", "PHASE WEAVE",
  "SIDEBAND BANK", "UNDERTOW", "ATTRACTOR", "LOCKSTEP", "REED PIPE", "BRASS",
  "SHAKERS", "CLAPS", "FRESHETS FORMANT", "DIATONIC CHORD", "SCALE STACK",
  "WT DIATONIC CHORD", "WT SCALE STACK", "HELIX", "BYTEBEAT", "RULEFIELD",
  "SPECTRAL SPIRAL", "PHASE FLOCK"
};

static int getColumnCount(int row) {
  if (row < 3) return instrumentCommonColumnCount(row);
  if (row == 3) return 1;
  if (row == 9) {
    InstrumentPlaits* p = &chipnomadState->project.instruments[cInstrument].chip.plaits;
    return p->envelopeMode == 0 ? 2 : 5;
  }
  return 2;
}

static void drawStatic(void) {
  instrumentCommonDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(0, 6, "Engine");
  gfxPrint(0, 7, "Harmonic"); gfxPrint(21, 7, "Filter");
  gfxPrint(0, 8, "Timbre");   gfxPrint(21, 8, "Mode");
  gfxPrint(0, 9, "Morph");    gfxPrint(21, 9, "Slope");
  gfxPrint(0, 10, "Main/Aux");gfxPrint(21, 10, "Cutoff");
  gfxPrint(0, 11, "Env Mode");gfxPrint(21, 11, "Reso");
  InstrumentPlaits* p = &chipnomadState->project.instruments[cInstrument].chip.plaits;
  if (p->envelopeMode == 0) {
    gfxPrint(0, 13, "LPG"); gfxPrint(6, 13, "D"); gfxPrint(11, 13, "C");
  } else {
    gfxPrint(0, 13, "ADSR");
    gfxPrint(6, 13, "A"); gfxPrint(11, 13, "D");
    gfxPrint(16, 13, "S"); gfxPrint(21, 13, "R"); gfxPrint(27, 13, "Shape");
  }
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  if (row == 9) gfxCursor(col == 4 ? 35 : 7 + col * 5, 13, 2);
  else if (row == 3) gfxCursor(11, 6, 28);
  else gfxCursor(col ? 31 : 11, row + 3, col ? 8 : 9);
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) return instrumentCommonDrawField(col, row, state);
  InstrumentPlaits* p = &chipnomadState->project.instruments[cInstrument].chip.plaits;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  if (row == 9) {
    uint8_t values[] = {p->attack, p->decay, p->sustain, p->release, p->envelopeShape};
    uint8_t value = p->envelopeMode == 0 ? values[col + 1] : values[col];
    gfxPrint(col == 4 ? 35 : 7 + col * 5, 13, byteToHex(value));
    return;
  }
  if (row == 3) gfxClearRect(11, 6, 29, 1);
  else gfxClearRect(col ? 31 : 11, row + 3, col ? 9 : 10, 1);
  switch (row) {
    case 3: gfxPrintf(12, 6, "%02d %s", p->engine,
      (isAlt() ? altEngineNames : engineNames)[p->engine < 24 ? p->engine : 0]); break;
    case 4: if (!col) gfxPrintf(11, 7, "%04u", (unsigned)((uint32_t)p->harmonics * 1023 / 32767)); else gfxPrint(31, 7, p->filterEnabled ? "On" : "Off"); break;
    case 5: if (!col) gfxPrintf(11, 8, "%04u", (unsigned)((uint32_t)p->timbre * 1023 / 32767)); else { static const char* modes[] = {"LP", "BP", "HP"}; gfxPrint(31, 8, modes[p->filterMode <= 2 ? p->filterMode : 0]); } break;
    case 6: if (!col) gfxPrintf(11, 9, "%04u", (unsigned)((uint32_t)p->morph * 1023 / 32767)); else gfxPrint(31, 9, p->filterSlope24dB ? "24 dB" : "12 dB"); break;
    case 7: if (!col) gfxPrint(11, 10, byteToHex(p->auxMix)); else gfxPrintf(31, 10, "%u Hz", p->filterCutoffHz); break;
    case 8: if (!col) gfxPrint(11, 11, p->envelopeMode == 0 ? "TRIG" : "VCA"); else gfxPrint(31, 11, byteToHex(p->filterResonance)); break;
  }
}

static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentPlaits* p = &chipnomadState->project.instruments[cInstrument].chip.plaits;
  int handled = 0;
  switch (row) {
    case 3:
      handled = edit8noLast(action, &p->engine, 1, 0, 23);
      break;
    case 4: handled = !col ? editOscillatorParameter(action, &p->harmonics) : edit8noLast(action, &p->filterEnabled, 1, 0, 1); break;
    case 5: handled = !col ? editOscillatorParameter(action, &p->timbre) : edit8noLast(action, &p->filterMode, 1, 0, 2); break;
    case 6: handled = !col ? editOscillatorParameter(action, &p->morph) : edit8noLast(action, &p->filterSlope24dB, 1, 0, 1); break;
    case 7: handled = !col ? edit8noLast(action, &p->auxMix, 16, 0, 255) : editFilterCutoff(action, &p->filterCutoffHz); break;
    case 8: if (!col) {
      uint8_t mode = p->envelopeMode == 0 ? 0 : 1;
      handled = edit8noLast(action, &mode, 1, 0, 1);
      if (handled) { p->envelopeMode = mode == 0 ? 0 : 2; screenSetup(&screenInstrument, cInstrument); }
    } else handled = edit8noLast(action, &p->filterResonance, 16, 0, 255); break;
    case 9: {
      uint8_t* values[] = {&p->attack, &p->decay, &p->sustain, &p->release, &p->envelopeShape};
      handled = edit8noLast(action, values[p->envelopeMode == 0 ? col + 1 : col], 16, 0, 255);
      break;
    }
  }
  if (handled) projectModified = 1;
  return handled;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (screenInstrumentPlaits.cursorRow != 3) {
    engineButtonDown = 0;
    return 0;
  }
  if (isKeyDown && keys == keyEdit) {
    engineButtonDown = 1;
    return 1;
  }
  if (isKeyDown && (keys == (keyEdit | keyLeft) ||
                    keys == (keyEdit | keyRight))) {
    engineButtonDown = 0;
    return 0;
  }
  if (!isKeyDown && keys == 0 && engineButtonDown) {
    engineButtonDown = 0;
    openEngineSelection();
    return 1;
  }
  return 0;
}

ScreenData screenInstrumentPlaits = {
  .rows = 10, .cursorRow = 0, .cursorCol = 0, .topRow = 0, .selectMode = -1,
  .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none, .getColumnCount = getColumnCount,
  .drawStatic = drawStatic, .drawCursor = drawCursor, .drawSelection = NULL,
  .drawRowHeader = NULL, .drawColHeader = NULL, .drawField = drawField, .onEdit = onEdit,
  .onInput = onInput, .onRawInput = NULL, .isCellValid = NULL, .getLoopRange = NULL,
};
