#include "screens.h"
#include "common.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "chipnomad_lib.h"
#include "project_utils.h"
#include "screen_instrument.h"
#include "selection_popup.h"
#include <math.h>
#include <string.h>

// Screen layout (20 rows):
// y 0: "MODULATION 00"
// y 1: (empty)
// y 2:  Mod1  [type]       Mod3  [type]
// y 3:  Dest  Off          Dest  Off
// y 4:  Amt   [00]         Amt   [00]
// y 5:  p1 label [val]     p1 label [val]
// y 6:  p2 label [val]     p2 label [val]
// y 7:  p3 label [val]     p3 label [val]
// y 8:  p4 label [val]     p4 label [val]
// y 9:  (spacing)
// y 10: Mod2  [type]       Mod4  [type]
// y 11: Dest  Off          Dest  Off
// y 12: Amt   [00]         Amt   [00]
// y 13: p1 label [val]     p1 label [val]
// y 14: p2 label [val]     p2 label [val]
// y 15: p3 label [val]     p3 label [val]
// y 16: p4 label [val]     p4 label [val]
//
// Logical rows (per modulator, 7 rows each):
// Top block (Mod1 left, Mod3 right): rows 0-6
// Bottom block (Mod2 left, Mod4 right): rows 7-13
//
// Column mapping:
// col 0 = left modulator (Mod1 top, Mod2 bottom)
// col 1 = right modulator (Mod3 top, Mod4 bottom)

#define COL_LEFT_X    0
#define COL_LEFT_VAL  7
#define COL_RIGHT_X   17
#define COL_RIGHT_VAL 24

#define ROW_TOTAL 14
#define ROWS_PER_MOD 7

static SelectionItem destinationCategories[5];
static SelectionItem engineDestinations[32];
static SelectionItem sendDestinations[2];
static SelectionItem parameterDestinations[16];
static SelectionItem envelopeDestinations[5];
static SelectionItem triggerDestinations[2];
static char parameterLabels[16][20];
static char parameterHelpers[16][40];
static int editedModIndex;
static int destinationButtonDown;

static void destinationSelected(int value) {
  chipnomadState->project.instruments[cInstrument].modulation[editedModIndex].destination = value;
  projectModified = 1;
  screenSetup(&screenModulation, cInstrument);
}

static void destinationCancelled() { screenSetup(&screenModulation, cInstrument); }

static const char* modulationParameterName(ModulationType type, int parameter) {
  static const char* adsr[] = {"Attack", "Decay", "Sustain", "Release"};
  static const char* ahd[] = {"Attack", "Hold", "Decay", ""};
  static const char* lfo[] = {"Shape", "Trigger", "Rate", ""};
  static const char* slfo[] = {"Shape", "Trigger", "Ticks", "Multiplier"};
  static const char* flfo[] = {"Shape", "Trigger", "Frequency", ""};
  static const char* stick[] = {"Axis", "", "", ""};
  static const char* stickRate[] = {"Axis", "Ticks", "", ""};
  const char* const* names = lfo;
  if (type == ModulationType::ADSR) names = adsr;
  else if (type == ModulationType::AHD) names = ahd;
  else if (type == ModulationType::SLFO) names = slfo;
  else if (type == ModulationType::FLFO) names = flfo;
  else if (type == ModulationType::StickLinear) names = stick;
  else if (type == ModulationType::StickRate) names = stickRate;
  return names[parameter];
}

static const char* stickAxisName(uint8_t axis) {
  static const char* names[] = {"LVert ", "LHoriz", "RVert ", "RHoriz"};
  return names[axis < static_cast<uint8_t>(StickAxis::totalCount) ? axis : 0];
}

static void openDestinationPopup(int modIndex) {
  Instrument* instrument = &chipnomadState->project.instruments[cInstrument];
  InstrumentFunctions functions = getInstrumentFunctions(instrument->type);
  editedModIndex = modIndex;
  for (int i = 0; i <= functions.modDestinationsCount; ++i) {
    engineDestinations[i] = {instrumentModDestinationName(instrument->type, i), i, NULL, 0};
  }
  int firstGeneric = functions.modDestinationsCount + 1;
  sendDestinations[0] = {"REVERB SEND", firstGeneric + genericModReverbSend, NULL, 0};
  sendDestinations[1] = {"DELAY SEND", firstGeneric + genericModDelaySend, NULL, 0};
  for (int i = 0; i < 16; ++i) {
    int destination = firstGeneric + genericModFirstParameter + i;
    int mod = i / 4;
    const char* parameter = modulationParameterName(instrument->modulation[mod].type, i % 4);
    snprintf(parameterLabels[i], sizeof(parameterLabels[i]), "M%d %s", mod + 1, parameter);
    snprintf(parameterHelpers[i], sizeof(parameterHelpers[i]), "Mod %d target: %s", mod + 1, parameter);
    parameterDestinations[i] = {parameterLabels[i], destination, NULL, 0, parameterHelpers[i]};
  }
  static const char* envelopeNames[] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE", "SHAPE"};
  static const char* triggerNames[] = {"DECAY", "COLOR"};
  for (int i = 0; i < 5; ++i)
    envelopeDestinations[i] = {envelopeNames[i], firstGeneric + genericModEnvelopeAttack + i, NULL, 0};
  for (int i = 0; i < 2; ++i)
    triggerDestinations[i] = {triggerNames[i], firstGeneric + genericModTriggerDecay + i, NULL, 0};

  int categoryCount = 0;
  destinationCategories[categoryCount++] = {"ENGINE", -1, engineDestinations, functions.modDestinationsCount + 1};
  destinationCategories[categoryCount++] = {"FX SENDS", -1, sendDestinations, 2};
  destinationCategories[categoryCount++] = {"MODULATORS", -1, parameterDestinations, 16};
  if (functions.supportsVoicePost) {
    destinationCategories[categoryCount++] = {"ADSR", -1, envelopeDestinations, 5};
    if (functions.supportsTrigger)
      destinationCategories[categoryCount++] = {"TRIGGER", -1, triggerDestinations, 2};
  }
  selectionPopupSetup("DESTINATION", destinationCategories, categoryCount,
    instrument->modulation[modIndex].destination, destinationSelected,
    destinationCancelled);
  screenSetup(&screenSelectionPopup, 0);
}

// Forward declarations for screenData
static int getColumnCount(int row);
static void drawStatic(void);
static void drawCursor(int col, int row);
static void drawRowHeader(int row, CellState state);
static void drawColHeader(int col, CellState state);
static void drawField(int col, int row, CellState state);
static int onEdit(int col, int row, CellEditAction action);
static int isCellValid(int col, int row);
static int onInput(int isKeyDown, int keys, int tapCount);

// ScreenData definition
static ScreenData screenData = {
  .rows = ROW_TOTAL,
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
  .drawRowHeader = drawRowHeader,
  .drawColHeader = drawColHeader,
  .drawField = drawField,
  .onEdit = onEdit,
  .onInput = onInput,
  .onRawInput = NULL,
  .isCellValid = isCellValid,
  .getLoopRange = NULL,
};

// Map logical row to screen Y
static int rowToY(int row) {
  if (row < 7) return row + 2;   // Top block: y 2-8
  return row + 3;                 // Bottom block: y 10-16
}

// Map (col, row) to modulator index (0-3)
static int getModIndex(int col, int row) {
  if (row < 7) return col == 0 ? 0 : 2;  // Top: Mod1 (left), Mod3 (right)
  return col == 0 ? 1 : 3;                // Bottom: Mod2 (left), Mod4 (right)
}

// Row within a modulator block (0-6)
static int getModRow(int row) {
  return row < 7 ? row : row - 7;
}

static const char* modTypeName(ModulationType type) {
  switch (type) {
    case ModulationType::ADSR: return "ADSR";
    case ModulationType::AHD:  return "AHD";
    case ModulationType::LFO:  return "LFO";
    case ModulationType::SLFO: return "SLFO";
    case ModulationType::FLFO: return "FLFO";
    case ModulationType::StickLinear: return "STKLIN";
    case ModulationType::StickRate: return "STKRAT";
    default:      return "?   ";
  }
}

static const char* lfoShapeName(LFOShape shape) {
  switch (shape) {
    case LFOShape::tri:      return "Tri   ";
    case LFOShape::sin:      return "Sin   ";
    case LFOShape::uniTri:   return "UniTri";
    case LFOShape::uniSin:   return "UniSin";
    case LFOShape::rampDown: return "RampDn";
    case LFOShape::rampUp:   return "RampUp";
    case LFOShape::expDown:  return "ExpDn ";
    case LFOShape::expUp:    return "ExpUp ";
    case LFOShape::square:   return "Square";
    case LFOShape::random:   return "Random";
    default:               return "?     ";
  }
}

static const char* lfoTrigName(LFOTrigger trig) {
  switch (trig) {
    case LFOTrigger::free:   return "Free  ";
    case LFOTrigger::retrig: return "Retrig";
    case LFOTrigger::hold:   return "Hold  ";
    case LFOTrigger::once:   return "Once  ";
    default:            return "?     ";
  }
}

// Get the parameter label for a given modulator type and parameter index (0-3)
static const char* paramLabel(ModulationType type, int paramIdx) {
  switch (type) {
    case ModulationType::ADSR:
      if (paramIdx == 0) return "Atk";
      if (paramIdx == 1) return "Dec";
      if (paramIdx == 2) return "Sus";
      if (paramIdx == 3) return "Rel";
      break;
    case ModulationType::AHD:
      if (paramIdx == 0) return "Atk";
      if (paramIdx == 1) return "Hold";
      if (paramIdx == 2) return "Dec";
      break;
    case ModulationType::LFO:
      if (paramIdx == 0) return "Shape";
      if (paramIdx == 1) return "Trig";
      if (paramIdx == 2) return "Period";
      break;
    case ModulationType::SLFO:
      if (paramIdx == 0) return "Shape";
      if (paramIdx == 1) return "Trig";
      if (paramIdx == 2) return "Ticks";
      if (paramIdx == 3) return "Mult";
      break;
    case ModulationType::FLFO:
      if (paramIdx == 0) return "Shape";
      if (paramIdx == 1) return "Trig";
      if (paramIdx == 2) return "Freq";
      break;
    case ModulationType::StickLinear:
      if (paramIdx == 0) return "Axis";
      break;
    case ModulationType::StickRate:
      if (paramIdx == 0) return "Axis";
      if (paramIdx == 1) return "Ticks";
      break;
    default:
      break;
  }
  return NULL;
}

// How many parameter rows does this modulation type have?
static int paramCount(ModulationType type) {
  switch (type) {
    case ModulationType::ADSR: return 4;
    case ModulationType::AHD:  return 3;
    case ModulationType::LFO:  return 3;
    case ModulationType::SLFO: return 4;
    case ModulationType::FLFO: return 3;
    case ModulationType::StickLinear: return 1;
    case ModulationType::StickRate: return 2;
    default:      return 0;
  }
}

static int isCellValid(int col, int row) {
  if (chipnomadState->project.instruments[cInstrument].type == InstrumentType::none) return 0;

  int modRow = getModRow(row);
  if (modRow <= 2) return 1; // Type, Dest, Amt always valid

  int modIdx = getModIndex(col, row);
  Modulation* mod = &chipnomadState->project.instruments[cInstrument].modulation[modIdx];
  int paramIdx = modRow - 3;
  return paramIdx < paramCount(mod->type);
}

static int getColumnCount(int row) {
  return 2;
}

static void drawStatic(void) {
  const ColorScheme cs = appSettings.colorScheme;

  gfxSetFgColor(cs.textTitles);
  gfxPrintf(0, 0, "MODULATION %02X", cInstrument);

  if (chipnomadState->project.instruments[cInstrument].type == InstrumentType::none) return;

  for (int block = 0; block < 2; block++) {
    int baseY = block == 0 ? 2 : 10;

    gfxSetFgColor(cs.textTitles);
    gfxPrintf(COL_LEFT_X, baseY, "Mod%d", block == 0 ? 1 : 2);
    gfxPrintf(COL_RIGHT_X, baseY, "Mod%d", block == 0 ? 3 : 4);

    gfxSetFgColor(cs.textDefault);
    gfxPrint(COL_LEFT_X, baseY + 1, "Dest");
    gfxPrint(COL_LEFT_X, baseY + 2, "Amt");
    gfxPrint(COL_RIGHT_X, baseY + 1, "Dest");
    gfxPrint(COL_RIGHT_X, baseY + 2, "Amt");
  }
}

static void drawCursor(int col, int row) {
  int y = rowToY(row);
  int valX = col == 0 ? COL_LEFT_VAL : COL_RIGHT_VAL;
  int modRow = getModRow(row);
  int modIdx = getModIndex(col, row);
  Modulation* mod = &chipnomadState->project.instruments[cInstrument].modulation[modIdx];

  switch (modRow) {
    case 0: gfxCursor(valX, y, strlen(modTypeName(mod->type))); break;
    case 1: {
      Instrument* inst = &chipnomadState->project.instruments[cInstrument];
      gfxCursor(valX, y, strlen(instrumentModDestinationName(inst->type, mod->destination)));
      break;
    }
    case 2: gfxCursor(valX, y, 2); break; // Amt
    default:
      if ((mod->type == ModulationType::LFO && (modRow - 3) <= 1) ||
          (mod->type == ModulationType::FLFO && modRow == 5) ||
          ((mod->type == ModulationType::StickLinear || mod->type == ModulationType::StickRate) &&
           modRow == 3)) {
        gfxCursor(valX, y, 6);
      } else if (mod->type == ModulationType::StickRate && modRow == 4) {
        gfxCursor(valX, y, 3);
      } else {
        gfxCursor(valX, y, 2); // Hex values
      }
      break;
  }
}

static void drawField(int col, int row, CellState state) {
  if (chipnomadState->project.instruments[cInstrument].type == InstrumentType::none) return;

  int y = rowToY(row);
  int valX = col == 0 ? COL_LEFT_VAL : COL_RIGHT_VAL;
  int labelX = col == 0 ? COL_LEFT_X : COL_RIGHT_X;
  int modRow = getModRow(row);
  int modIdx = getModIndex(col, row);
  Modulation* mod = &chipnomadState->project.instruments[cInstrument].modulation[modIdx];

  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);

  switch (modRow) {
    case 0: // Type (on header row)
      gfxPrint(valX, y, modTypeName(mod->type));
      break;
    case 1: // Destination
      gfxClearRect(valX, y, 8, 1);
      {
        Instrument* inst = &chipnomadState->project.instruments[cInstrument];
        gfxPrint(valX, y, instrumentModDestinationName(inst->type, mod->destination));
      }
      break;
    case 2: // Amount
      gfxClearRect(valX, y, 2, 1);
      gfxPrint(valX, y, byteToHex(mod->amount));
      break;
    default: {
      int paramIdx = modRow - 3;
      if (paramIdx >= paramCount(mod->type)) break;

      // Draw the label
      gfxSetFgColor(appSettings.colorScheme.textDefault);
      gfxClearRect(labelX, y, 6, 1);
      const char* label = paramLabel(mod->type, paramIdx);
      if (label) gfxPrint(labelX, y, label);

      // Draw the value
      gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
      gfxClearRect(valX, y, 6, 1);

      if (mod->type == ModulationType::AHD || mod->type == ModulationType::ADSR) {
        uint8_t* params = &mod->p1;
        gfxPrint(valX, y, byteToHex(params[paramIdx]));
      } else if (mod->type == ModulationType::LFO || mod->type == ModulationType::SLFO || mod->type == ModulationType::FLFO) {
        if (paramIdx == 0) {
          gfxPrint(valX, y, lfoShapeName(static_cast<LFOShape>(mod->p1)));
        } else if (paramIdx == 1) {
          gfxPrint(valX, y, lfoTrigName(static_cast<LFOTrigger>(mod->p2)));
        } else if (paramIdx == 2) {
          if (mod->type == ModulationType::FLFO) gfxPrintf(valX, y, "%5.0f", powf(8000.0f, mod->p3 / 255.0f));
          else gfxPrint(valX, y, byteToHex(mod->p3));
        } else if (paramIdx == 3) {
          gfxPrint(valX, y, byteToHex(mod->p4));
        }
      } else if (mod->type == ModulationType::StickLinear || mod->type == ModulationType::StickRate) {
        if (paramIdx == 0) gfxPrint(valX, y, stickAxisName(mod->p1));
        else if (paramIdx == 1 && mod->type == ModulationType::StickRate) gfxPrintf(valX, y, "%3u", mod->p2);
      }
      break;
    }
  }
}

static int onEdit(int col, int row, enum CellEditAction action) {
  if (chipnomadState->project.instruments[cInstrument].type == InstrumentType::none) return 0;

  int handled = 0;
  int modRow = getModRow(row);
  int modIdx = getModIndex(col, row);
  Modulation* mod = &chipnomadState->project.instruments[cInstrument].modulation[modIdx];

  switch (modRow) {
    case 0: { // Type
      uint8_t type = static_cast<uint8_t>(mod->type);
      uint8_t oldType = type;
      handled = edit8noLast(action, &type, 1, 0, static_cast<uint8_t>(ModulationType::totalCount) - 1);
      if (type == static_cast<uint8_t>(ModulationType::StickVelocity)) {
        type = oldType < type ? static_cast<uint8_t>(ModulationType::StickRate)
                              : static_cast<uint8_t>(ModulationType::StickLinear);
      }
      mod->type = static_cast<ModulationType>(type);
      if (oldType != type && !(modulationIsLiveStick(static_cast<ModulationType>(oldType)) &&
                               modulationIsLiveStick(mod->type))) {
        mod->p1 = 0;
        mod->p2 = mod->type == ModulationType::StickRate ? 24 : 0;
        mod->p3 = (mod->type == ModulationType::ADSR) ? 255 :
                  (mod->type == ModulationType::FLFO ? 0 : (mod->type == ModulationType::SLFO ? 24 : 6));
        mod->p4 = mod->type == ModulationType::SLFO ? 4 : 0;
      }
      if (oldType != type) screenFullRedraw(&screenData);
      break;
    }
    case 1: { // Destination
      Instrument* inst = &chipnomadState->project.instruments[cInstrument];
      if (action == CellEditAction::tap) {
        openDestinationPopup(modIdx);
        return 0;
      }
      handled = edit8noLast(action, &mod->destination, 1, 0,
        instrumentModDestinationMax(inst->type));
      break;
    }
    case 2: // Amount
      handled = editSigned8(action, &mod->amount, 16, -128, 127);
      if (handled) {
        screenMessage(0, "Modulation amount %hhd", mod->amount);
      }
      break;
    default: {
      int paramIdx = modRow - 3;
      if (paramIdx >= paramCount(mod->type)) break;

      if (mod->type == ModulationType::AHD || mod->type == ModulationType::ADSR) {
        uint8_t* params = &mod->p1;
        handled = edit8noLast(action, &params[paramIdx], 16, 0, 255);
        if (handled) {
          // Full field names for AHD and ADSR
          const char* fullName = NULL;
          if (mod->type == ModulationType::ADSR) {
            const char* adsrNames[] = {"Attack", "Decay", "Sustain", "Release"};
            if (paramIdx < 4) fullName = adsrNames[paramIdx];
          } else if (mod->type == ModulationType::AHD) {
            const char* ahdNames[] = {"Attack", "Hold", "Decay"};
            if (paramIdx < 3) fullName = ahdNames[paramIdx];
          }
          if (fullName) {
            screenMessage(0, "%s %hhu ticks", fullName, params[paramIdx]);
          }
        }
      } else if (mod->type == ModulationType::LFO || mod->type == ModulationType::SLFO || mod->type == ModulationType::FLFO) {
        if (paramIdx == 0) {
          handled = edit8noLast(action, &mod->p1, 1, 0, static_cast<uint8_t>(LFOShape::totalCount) - 1);
          // No hint for LFO shape - not adding value
        } else if (paramIdx == 1) {
          handled = edit8noLast(action, &mod->p2, 1, 0, static_cast<uint8_t>(LFOTrigger::totalCount) - 1);
          // No hint for LFO trigger - not adding value
        } else if (paramIdx == 2) {
          handled = edit8noLast(action, &mod->p3, 16, 0, 255);
          if (handled) {
            if (mod->type == ModulationType::FLFO)
              screenMessage(0, "Frequency %.0f Hz", powf(8000.0f, mod->p3 / 255.0f));
            else
              screenMessage(0, "Period %hhu ticks", mod->p3);
          }
        } else if (paramIdx == 3 && mod->type == ModulationType::SLFO) {
          handled = edit8noLast(action, &mod->p4, 1, 1, 64);
          if (handled) screenMessage(0, "%hhu x %hhu ticks", mod->p4, mod->p3);
        }
      } else if (mod->type == ModulationType::StickLinear || mod->type == ModulationType::StickRate) {
        if (paramIdx == 0) {
          handled = edit8noLast(action, &mod->p1, 1, 0, static_cast<uint8_t>(StickAxis::totalCount) - 1);
        } else if (paramIdx == 1 && mod->type == ModulationType::StickRate) {
          handled = edit8noLast(action, &mod->p2, 16, 1, 255);
          if (handled) screenMessage(0, "Rate sweep %u ticks", mod->p2);
        }
      }
      break;
    }
  }

  if (handled) projectModified = 1;
  return handled;
}

static void fullRedraw(void) {
  screenFullRedraw(&screenData);
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (getModRow(screenData.cursorRow) == 1) {
    Instrument* instrument = &chipnomadState->project.instruments[cInstrument];
    Modulation* mod = &instrument->modulation[getModIndex(screenData.cursorCol, screenData.cursorRow)];
    PopupEditInput input = popupEditInput(isKeyDown, keys, &destinationButtonDown);
    if (input == PopupEditInput::cycle) {
      int maximum = instrumentModDestinationMax(instrument->type);
      cycle8(&mod->destination, keys == (keyEdit | keyRight) ? 1 : -1, 0, maximum, 1);
      projectModified = 1;
      drawField(screenData.cursorCol, screenData.cursorRow, CellState::focus);
      return 1;
    }
    if (input == PopupEditInput::hold) return 1;
    if (input == PopupEditInput::open) {
      openDestinationPopup(getModIndex(screenData.cursorCol, screenData.cursorRow));
      return 1;
    }
  } else {
    destinationButtonDown = 0;
  }
  if (keys == 0) {
    chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
  }

  if (keys == (keyDown | keyShift)) {
    // To Instrument screen
    screenSetup(&screenInstrument, cInstrument);
    return 1;
  } else if (keys == (keyOpt | keyLeft)) {
    if (cInstrument != 0) {
      cInstrument--;
      chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
      fullRedraw();
    }
    return 1;
  } else if (keys == (keyOpt | keyRight)) {
    if (cInstrument != PROJECT_MAX_INSTRUMENTS - 1) {
      cInstrument++;
      chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
      fullRedraw();
    }
    return 1;
  } else if (keys == (keyOpt | keyUp)) {
    cInstrument += 16;
    if (cInstrument >= PROJECT_MAX_INSTRUMENTS) cInstrument = PROJECT_MAX_INSTRUMENTS - 1;
    chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
    fullRedraw();
    return 1;
  } else if (keys == (keyOpt | keyDown)) {
    cInstrument -= 16;
    if (cInstrument < 0) cInstrument = 0;
    chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
    fullRedraw();
    return 1;
  } else if (keys == (keyEdit | keyPlay)) {
    if (!instrumentIsEmpty(&chipnomadState->project, cInstrument) && !chipnomadGetPlaybackStatus(chipnomadState)->isPlaying) {
      uint8_t note = instrumentFirstNote(&chipnomadState->project, cInstrument);
      chipnomadQueuePlaybackPreviewNote(chipnomadState, *pSongTrack, note, cInstrument);
    }
    return 1;
  }

  if (screenInput(&screenData, isKeyDown, keys, tapCount)) return 1;

  return 0;
}

static void init(void) {
}

static void setup(int input) {
  if (input != -1) {
    cInstrument = input;
  }
}

static void draw(void) {
}

static void drawRowHeader(int row, CellState state) {}
static void drawColHeader(int col, CellState state) {}

static ScreenPlaybackLevel getPlaybackLevel(void) {
  return ScreenPlaybackLevel::phrase;
}

const AppScreen screenModulation = {
  .init = init,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = getPlaybackLevel
};
