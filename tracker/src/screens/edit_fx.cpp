#include "screens.h"
#include "corelib_gfx.h"
#include "help.h"
#include "chord.h"
#include "synth/sample_voice.h"
#include <algorithm>

// State for FX selection screen
int currentGroup;      // Current group being navigated
int currentIdx;        // Current FX index within group
int expandedGroup;     // Currently expanded group (-1 = none)
uint8_t currentInstrumentIdx;  // Current instrument index for context-aware help
static int currentIsTable;

// Helper to get instrument type from stored instrument index
static InstrumentType getInstrumentType(uint8_t instrumentIdx) {
  if (instrumentIdx != EMPTY_VALUE_8 && instrumentIdx < PROJECT_MAX_INSTRUMENTS) {
    return chipnomadState->project.instruments[instrumentIdx].type;
  }
  return InstrumentType::none;
}

static InstrumentType getCurrentInstrumentType() {
  return getInstrumentType(currentInstrumentIdx);
}
static const Instrument* getCurrentInstrument() {
  return currentInstrumentIdx != EMPTY_VALUE_8 && currentInstrumentIdx < PROJECT_MAX_INSTRUMENTS
    ? &chipnomadState->project.instruments[currentInstrumentIdx] : NULL;
}

// Insert addresses remain stable in songs; only the picker is contextual.
static bool insertFXAvailable(int fx) {
  if (fx < fxF11 || fx > fxF28) return true;
  if (!pSongTrack || *pSongTrack >= PROJECT_MAX_TRACKS) return false;
  const int address = fx - fxF11;
  const auto& config = chipnomadState->project.trackInserts[*pSongTrack][address / 8];
  return address % 8 < insertDescriptor(config.module).count;
}

static uint8_t availableInsertChoice(uint8_t fx) {
  if (insertFXAvailable(fx)) return fx;
  // A stale command starts at the first control in its slot, or the other
  // configured slot. Opening the picker does not change the stored command.
  const int slot = (fx - fxF11) / 8;
  for (int candidate : {fxF11 + slot * 8, fxF11 + (1 - slot) * 8}) {
    if (insertFXAvailable(candidate)) return candidate;
  }
  return fxARP;
}

static bool nativeInfo(uint8_t instrumentIdx,int fx,NativeFXInfo& info) {
  return instrumentIdx!=EMPTY_VALUE_8 && instrumentIdx<PROJECT_MAX_INSTRUMENTS && instrumentFXAvailableForInstrument(&chipnomadState->project.instruments[instrumentIdx],fx) && instrumentNativeFXInfo(&chipnomadState->project.instruments[instrumentIdx],fx,&info);
}

static const char* nativeControlDescription(int fx) {
  switch(fx) {
    case fxOAR:return "How quickly the operator attacks";
    case fxODR:return "How quickly the operator decays";
    case fxORR:return "Decay rate after note release";
    case fxOSL:return "Held-note attenuation; more=softer";
    case fxOMU:return "Operator frequency multiplier";
    case fxLFR:return "How quickly the native LFO cycles";
    case fxLAD:return "Amount of native LFO tremolo";
    case fxLPD:return "Amount of native LFO vibrato";
    case fxLAS:return "Volume response to native LFO";
    case fxLPS:return "Pitch response to native LFO";
    case fxLEN:return "Turns the chip LFO on or off";
    case fxSMR:return "How fast SID macro steps advance";
    case fxSWV:return "Selects the SID oscillator wave";
    case fxSPR:return "Ring/sync partner frequency ratio";
    default:return "";
  }
}
void selectInstrumentFX(uint8_t* fx,uint8_t selected,uint8_t instrumentIdx) {
  if (!insertFXAvailable(selected)) return;
  NativeFXInfo info{};
  bool native=nativeInfo(instrumentIdx,selected,info);
  if(selected>=fxFBR&&selected<=fxLEN&&!native)return;
  if(fx[0]==selected) {
    if(native)fx[1]=std::clamp(int(fx[1]),info.minimum,info.maximum);
    return; // Preserve legal edits; repair values from a different engine.
  }
  fx[0]=selected;
  if(native)fx[1]=info.preset;
}

static const char* contextualFXHint(uint8_t* fx,int table,uint8_t instrument) {
  NativeFXInfo native{};
  if(nativeInfo(instrument,fx[0],native) && native.label) {
    static char text[80];
    if(fx[0]>=fxOAR&&fx[0]<=fxOE4)snprintf(text,sizeof(text),"OP1 %s: %02X",native.label,fx[1]);
    else snprintf(text,sizeof(text),"%s: %02X",native.label,fx[1]);
    return text;
  }
  if(fx[0]<fxF11 || fx[0]>fxF28)return helpFXHint(fx,table,instrument);
  static char text[80];int a=fx[0]-fxF11;
  const auto& c=chipnomadState->project.trackInserts[*pSongTrack][a/8];
  const auto& d=insertDescriptor(c.module);
  char decoded[32];
  insertDescribe(decoded,sizeof(decoded),c.module,a%8,fx[1]);
  snprintf(text,sizeof(text),"%s %s: %s",d.name,a%8<d.count?d.parameters[a%8].name:"Unused",decoded);
  return text;
}
static bool isFXAvailable(enum FX fx, uint8_t instrumentIdx, int isTable) {
  if (!insertFXAvailable(fx)) return false;
  if(fx==fxFBR||(fx>=fxFET&&fx<=fxFLD))return false;
  if((fx>=fxFO1&&fx<=fxFO6)||fx==fxFFB)return false; // Retired personal commands.
  if (isTable && (fx == fxSCL || fx == fxCRD)) return false;
  // Note Lock pins note entry to the project scale, so SCL is not offered.
  if (fx == fxSCL && chipnomadState->project.scaleMode != 0) return false;
  InstrumentType instrumentType = getInstrumentType(instrumentIdx);
  const Instrument* instrument = instrumentIdx != EMPTY_VALUE_8 && instrumentIdx < PROJECT_MAX_INSTRUMENTS
    ? &chipnomadState->project.instruments[instrumentIdx] : NULL;
  if(fx>=fxFBR&&fx<=fxLEN)return instrument && instrumentFXAvailableForInstrument(instrument,fx);
  if (instrument && instrumentFXAvailableForInstrument(instrument, (uint8_t)fx)) return true;
  for (int groupIdx = 0; groupIdx < fxGroupCount; groupIdx++) {
    FXGroup* group = &fxGroups[groupIdx];
    if (group->instType != InstrumentType::none && group->instType != instrumentType) continue;
    if (group->instType == InstrumentType::DrumSynth && instrument &&
        !instrumentFXAvailableForInstrument(instrument, (uint8_t)fx)) continue;
    for (int i = 0; i < group->count; i++) {
      if (group->fxList[i].fx == fx) return true;
    }
  }
  return false;
}

static void stepFX(uint8_t* fx, int direction, uint8_t instrumentIdx, int isTable) {
  for (int candidate = (int)fx[0] + direction;
       candidate >= 0 && candidate < fxTotalCount; candidate += direction) {
    if (isFXAvailable((enum FX)candidate, instrumentIdx, isTable)) {
      selectInstrumentFX(fx,candidate,instrumentIdx);
      return;
    }
  }
}

static int visibleFXCount(const FXGroup* group) {
  const Instrument* instrument = getCurrentInstrument();
  int hideSCL = !currentIsTable && chipnomadState->project.scaleMode != 0;
  int count = 0;
  for (int i = 0; i < group->count; ++i) {
    if (!insertFXAvailable(group->fxList[i].fx)) continue;
    if (currentIsTable && (group->fxList[i].fx == fxSCL || group->fxList[i].fx == fxCRD)) continue;
    if (hideSCL && group->fxList[i].fx == fxSCL) continue;
    if(group->fxList[i].fx>=fxFBR && group->fxList[i].fx<=fxLEN && (!instrument || !instrumentFXAvailableForInstrument(instrument,group->fxList[i].fx)))continue;
    if (!instrument || group->instType != InstrumentType::DrumSynth || instrumentFXAvailableForInstrument(instrument, group->fxList[i].fx)) ++count;
  }
  return count;
}

static const FXName* visibleFXAt(const FXGroup* group, int visibleIndex) {
  const Instrument* instrument = getCurrentInstrument();
  int hideSCL = !currentIsTable && chipnomadState->project.scaleMode != 0;
  for (int i = 0; i < group->count; ++i) {
    if (!insertFXAvailable(group->fxList[i].fx)) continue;
    if (currentIsTable && (group->fxList[i].fx == fxSCL || group->fxList[i].fx == fxCRD)) continue;
    if (hideSCL && group->fxList[i].fx == fxSCL) continue;
    if(group->fxList[i].fx>=fxFBR && group->fxList[i].fx<=fxLEN && (!instrument || !instrumentFXAvailableForInstrument(instrument,group->fxList[i].fx)))continue;
    if (instrument && group->instType == InstrumentType::DrumSynth && !instrumentFXAvailableForInstrument(instrument, group->fxList[i].fx)) continue;
    if (visibleIndex-- == 0) return &group->fxList[i];
  }
  return NULL;
}

void fxEditFullDraw(uint8_t currentFX, uint8_t instrumentIdx, int isTable);

int editFX(CellEditAction action, uint8_t* fx, uint8_t* lastValue, int isTable, uint8_t instrumentIdx) {
  int result = 0;
  action = convertMultiAction(action);

  if (action == CellEditAction::clear) {
    // Clear FX
    if (fx[0] != EMPTY_VALUE_8) {
      lastValue[0] = fx[0];
      lastValue[1] = fx[1];
    }
    fx[0] = EMPTY_VALUE_8;
    fx[1] = 0;
    result = 2;
  } else if (action == CellEditAction::tap) {
    // Insert last FX
    if (fx[0] == EMPTY_VALUE_8) {
      fx[1] = lastValue[1];
      selectInstrumentFX(fx,availableInsertChoice(lastValue[0]),instrumentIdx);
    }
    lastValue[0] = fx[0];
    lastValue[1] = fx[1];
    result = 2;
  } else if (action == CellEditAction::increase && fx[0] != EMPTY_VALUE_8) {
    stepFX(fx, 1, instrumentIdx, isTable);
    lastValue[0] = fx[0];
    result = 2;
  } else if (action == CellEditAction::decrease && fx[0] != EMPTY_VALUE_8) {
    stepFX(fx, -1, instrumentIdx, isTable);
    lastValue[0] = fx[0];
    result = 2;
  } else if (action == CellEditAction::increaseBig || action == CellEditAction::decreaseBig) {
    // Show FX select screen with instrument context
    fxEditFullDraw(fx[0], instrumentIdx, isTable);
    result = 1;
  }
  if (result != 1) screenMessage(0, "%s", contextualFXHint(fx, isTable, instrumentIdx));
  return result;
}

int editFXValue(CellEditAction action, uint8_t* fx, uint8_t* lastFX, int isTable, uint8_t instrumentIdx) {
  NativeFXInfo native{};
  if(nativeInfo(instrumentIdx,fx[0],native)) {
    bool multi=action==CellEditAction::multiIncrease || action==CellEditAction::multiDecrease || action==CellEditAction::multiIncreaseBig || action==CellEditAction::multiDecreaseBig;
    fx[1]=std::clamp(int(fx[1]),native.minimum,native.maximum);
    int handled=edit8noLast(action,&fx[1],native.maximum<16?1:16,native.minimum,native.maximum);
    if(handled&&!multi)lastFX[1]=fx[1];
    screenMessage(0,"%s",contextualFXHint(fx,isTable,instrumentIdx));
    return handled;
  }
  if (fx[0] == fxCRD) {
    int isNotMultiAction = action != CellEditAction::multiIncrease && action != CellEditAction::multiDecrease &&
      action != CellEditAction::multiIncreaseBig && action != CellEditAction::multiDecreaseBig;
    action = convertMultiAction(action);
    uint8_t chord = fx[1] & 0x0f;
    uint8_t inversion = fx[1] >> 4;
    int handled = 1;
    if (action == CellEditAction::clear) { chord = 0; inversion = 0; }
    else if (action == CellEditAction::tap) { if (fx[1] == 0) { chord = lastFX[1] & 0x0f; inversion = lastFX[1] >> 4; } }
    else if (action == CellEditAction::increase) chord = (chord + 1) & 0x0f;
    else if (action == CellEditAction::decrease) chord = (chord + 15) & 0x0f;
    else if (action == CellEditAction::increaseBig && inversion < chordMaxInversion(chord)) ++inversion;
    else if (action == CellEditAction::decreaseBig && inversion > 0) --inversion;
    else handled = 0;
    if (inversion > chordMaxInversion(chord)) inversion = chordMaxInversion(chord);
    if (handled) {
      fx[1] = (inversion << 4) | chord;
      if (isNotMultiAction) lastFX[1] = fx[1];
    }
    screenMessage(0, "%s", contextualFXHint(fx, isTable, instrumentIdx));
    return handled;
  }

  action = convertMultiAction(action);

  if (fx[0] == fxSPD && !chipnomadState->project.signedTrackSpeed) {
    int handled = edit8noLast(action, &fx[1], 1, 0, 0x10);
    screenMessage(0, "%s", contextualFXHint(fx, isTable, instrumentIdx));
    return handled;
  }

  // SLI cycles through the instrument's live slice count (1..count), so a
  // value can never point at an empty slice. 00 keeps the normal note
  // mapping.
  if (fx[0] == fxSLI) {
    uint8_t sliceCount = 0;
    if (instrumentIdx != EMPTY_VALUE_8 && instrumentIdx < PROJECT_MAX_INSTRUMENTS) {
      const Instrument* instrument = &chipnomadState->project.instruments[instrumentIdx];
      if (instrument->type == InstrumentType::Sample)
        sliceCount = sampleDecodeSliceCount(instrument->chip.sample.slice);
    }
    if (sliceCount) {
      int isNotMultiAction = action != CellEditAction::multiIncrease && action != CellEditAction::multiDecrease &&
        action != CellEditAction::multiIncreaseBig && action != CellEditAction::multiDecreaseBig;
      action = convertMultiAction(action);
      int handled = 1;
      switch (action) {
        case CellEditAction::clear:
          fx[1] = 0;
          break;
        case CellEditAction::tap:
          if (fx[1] == 0) fx[1] = lastFX[1] && lastFX[1] <= sliceCount ? lastFX[1] : 1;
          break;
        case CellEditAction::increase:
          fx[1] = fx[1] >= sliceCount ? 1 : fx[1] + 1;
          break;
        case CellEditAction::decrease:
          fx[1] = fx[1] <= 1 ? sliceCount : fx[1] - 1;
          break;
        case CellEditAction::increaseBig:
        case CellEditAction::decreaseBig:
          // Same cycle: the slice list is short, big steps add nothing.
          fx[1] = action == CellEditAction::increaseBig
            ? (fx[1] >= sliceCount ? 1 : fx[1] + 1)
            : (fx[1] <= 1 ? sliceCount : fx[1] - 1);
          break;
        default:
          handled = 0;
          break;
      }
      if (handled && isNotMultiAction) lastFX[1] = fx[1];
      screenMessage(0, "%s", contextualFXHint(fx, isTable, instrumentIdx));
      return handled;
    }
    // No slicing on this instrument: fall through to the generic editor so
    // the value stays inert but still editable.
  }

  uint8_t bigStep = 16;
  if (fx[0] == fxENT || fx[0] == fxTNN || fx[0] == fxENN || fx[0] == fxSFN) {
    // Note-setting FX: use octave size for big step
    bigStep = chipnomadState->project.pitchTable.octaveSize;
  }

  int handled = edit8noLimit(action, &fx[1], &lastFX[1], bigStep);
  screenMessage(0, "%s", contextualFXHint(fx, 0, instrumentIdx));
  return handled;
}

// Helper functions for FX group management

// Get number of visible groups based on current instrument type
int getVisibleGroupCount(InstrumentType instType) {
  extern FXGroup fxGroups[];
  extern int fxGroupCount;

  int count = 0;
  for (int i = 0; i < fxGroupCount; i++) {
    // Show group if it's non-instrument (InstrumentType::none) or matches current instrument type
    if ((fxGroups[i].instType == InstrumentType::none || fxGroups[i].instType == instType) &&
        visibleFXCount(&fxGroups[i]) > 0) {
      count++;
    }
  }
  return count;
}

// Get visible group by index (skips groups that don't match instrument type)
FXGroup* getVisibleGroup(int visibleIdx, InstrumentType instType) {
  extern FXGroup fxGroups[];
  extern int fxGroupCount;

  int visibleCount = 0;
  for (int i = 0; i < fxGroupCount; i++) {
    if ((fxGroups[i].instType == InstrumentType::none || fxGroups[i].instType == instType) &&
        visibleFXCount(&fxGroups[i]) > 0) {
      if (visibleCount == visibleIdx) {
        return &fxGroups[i];
      }
      visibleCount++;
    }
  }
  return NULL;
}

// Get actual group index from visible index
int getActualGroupIndex(int visibleIdx, InstrumentType instType) {
  extern FXGroup fxGroups[];
  extern int fxGroupCount;

  int visibleCount = 0;
  for (int i = 0; i < fxGroupCount; i++) {
    if ((fxGroups[i].instType == InstrumentType::none || fxGroups[i].instType == instType) &&
        visibleFXCount(&fxGroups[i]) > 0) {
      if (visibleCount == visibleIdx) {
        return i;
      }
      visibleCount++;
    }
  }
  return -1;
}

// Check if a group is expanded
int isGroupExpanded(int groupIdx) {
  return expandedGroup == groupIdx;
}

// Expand a group (collapses others)
void expandGroup(int groupIdx) {
  expandedGroup = groupIdx;
}

// Collapse a group
void collapseGroup(int groupIdx) {
  if (expandedGroup == groupIdx) {
    expandedGroup = -1;
  }
}

// Draw a group header at specified Y position
// Returns the Y position after the header (for next element)
int drawGroupHeader(int visibleGroupIdx, int y, int isCurrent) {
  FXGroup* group = getVisibleGroup(visibleGroupIdx, getCurrentInstrumentType());
  if (!group) return y;

  // Highlight current group header
  if (isCurrent) {
    gfxSetFgColor(appSettings.colorScheme.textValue);
  } else {
    gfxSetFgColor(appSettings.colorScheme.textTitles);
  }

  if (group->fxList && group->fxList[0].fx >= fxF11 && group->fxList[0].fx <= fxF28) {
    const int slot = (group->fxList[0].fx - fxF11) / 8;
    const auto& config = chipnomadState->project.trackInserts[*pSongTrack][slot];
    gfxPrintf(1, y, "TF%d: %s", slot + 1, insertDescriptor(config.module).name);
  } else {
    gfxPrint(1, y, group->name);
  }

  return y + 1;  // Next line after header
}

// Draw FX list for a group at specified Y position
// Returns the Y position after the FX list (for next element)
int drawFXList(int visibleGroupIdx, int y) {
  FXGroup* group = getVisibleGroup(visibleGroupIdx, getCurrentInstrumentType());
  if (!group) return y;

  int cols = group->columns;  // Use group-specific column count

  // Draw FX in grid with group-specific column count
  int count = visibleFXCount(group);
  for (int idx = 0; idx < count; idx++) {
    const FXName* item = visibleFXAt(group, idx);
    if (!item) continue;
    int row = idx / cols;
    int col = idx % cols;
    int fxY = y + row;

    // Highlight current FX if this is the current group
    int isCurrent = (visibleGroupIdx == currentGroup && idx == currentIdx);
    if (isCurrent) {
      gfxSetFgColor(appSettings.colorScheme.textValue);
    } else {
      gfxSetFgColor(appSettings.colorScheme.textDefault);
    }

    gfxPrint(1 + col * 4, fxY, item->name);

    if (isCurrent) gfxCursor(1 + col * 4, fxY, 3);
  }

  // Calculate how many rows the FX list takes
  int rows = (count + cols - 1) / cols;  // Ceiling division
  return y + rows;
}

void fxEditFullDraw(uint8_t currentFX, uint8_t instrumentIdx, int isTable) {
  gfxClearRect(0, 0, 35, 20);
  // The phrase screen reserves the bottom row for messages.  Do not leave a
  // stale insert hint there: it can overwrite the second row of Fxx entries.
  screenMessage(0, "");

  // Store instrument index for this session
  currentInstrumentIdx = instrumentIdx;
  currentIsTable = isTable;

  // Get instrument type for filtering groups
  InstrumentType instType = InstrumentType::none;
  if (instrumentIdx != EMPTY_VALUE_8 && instrumentIdx < PROJECT_MAX_INSTRUMENTS) {
    instType = chipnomadState->project.instruments[instrumentIdx].type;
  }

  currentFX = availableInsertChoice(currentFX);

  // Get visible groups
  int visibleGroupCount = getVisibleGroupCount(instType);

  // Find which group contains the current FX (if any)
  int foundGroup = -1;
  int foundIdx = -1;
  for (int g = 0; g < visibleGroupCount; g++) {
    FXGroup* group = getVisibleGroup(g, instType);
    if (group) {
      for (int i = 0; i < visibleFXCount(group); i++) {
        const FXName* item = visibleFXAt(group, i);
        if (item && item->fx == currentFX) {
          foundGroup = g;
          foundIdx = i;
          break;
        }
      }
      if (foundGroup >= 0) break;
    }
  }

  // Initialize state
  if (foundGroup >= 0) {
    // Current FX found in a visible group - expand that group and position cursor on it
    currentGroup = foundGroup;
    currentIdx = foundIdx;
    expandedGroup = foundGroup;
  } else {
    // Current FX not found - default to first group, first FX
    currentGroup = 0;
    currentIdx = 0;
    expandedGroup = 0;
    const FXName* first = visibleFXAt(getVisibleGroup(0, instType), 0);
    if (first) currentFX = first->fx;
  }

  // Draw help for current FX at top (with instrument context)
  NativeFXInfo direct{};
  int presetRow=6;
  if(nativeInfo(instrumentIdx,currentFX,direct) && direct.label) {
    gfxSetFgColor(appSettings.colorScheme.textValue);gfxPrint(1,1,direct.label);
    gfxSetFgColor(appSettings.colorScheme.textDefault);
    gfxPrint(1,2,nativeControlDescription(currentFX));
    int nextRow=3;
    if(currentFX>=fxOAR&&currentFX<=fxOE4)gfxPrint(1,nextRow++,"Operator 1");
    gfxPrint(1,nextRow++,"Absolute native value");
    presetRow=nextRow;
  } else drawFXHelp((enum FX)currentFX, instrumentIdx);
  NativeFXInfo info{};
  if(nativeInfo(instrumentIdx,currentFX,info)) {
    gfxSetFgColor(appSettings.colorScheme.textInfo);
    gfxPrintf(1,presetRow,"%s %02X   Range %02X-%02X",info.relative?"Preset FX":"Preset",info.preset,info.minimum,info.maximum);
  } else if(currentFX>=fxF11 && currentFX<=fxF28) {
    const int address=currentFX-fxF11, parameter=address%8;
    const auto& config=chipnomadState->project.trackInserts[*pSongTrack][address/8];
    if(parameter<insertDescriptor(config.module).count) {
      char decoded[32];
      insertDescribe(decoded,sizeof(decoded),config.module,parameter,config.values[parameter]);
      gfxSetFgColor(appSettings.colorScheme.textInfo);
      gfxPrintf(1,presetRow,"Saved %02X: %s",config.values[parameter],decoded);
    }
  }

  // Draw all visible groups (headers + expanded group's FX list)
  int y = presetRow+1;
  FXGroup* expanded = getVisibleGroup(expandedGroup, getCurrentInstrumentType());
  int expandedRows = expanded ? (visibleFXCount(expanded) + expanded->columns - 1) / expanded->columns : 0;
  bool separateGroups = !screenScopeRows(currentScreen) &&
    y + visibleGroupCount + expandedRows + visibleGroupCount - 1 <= 20 - gfxGetContentRowOffset();
  for (int g = 0; g < visibleGroupCount; g++) {
    // Draw group header
    int isCurrent = (g == currentGroup);
    y = drawGroupHeader(g, y, isCurrent);

    // Draw FX list only if this group is expanded
    if (g == expandedGroup) {
      y = drawFXList(g, y);
    }

    // Spend blank rows only when the entire expanded group fits on screen.
    if (separateGroups) y++;
  }
}


int fxEditInput(int keys, int tapCount, uint8_t* fx, uint8_t* lastFX) {
  if (keys == 0) {
    // Selection complete - update the FX value
    FXGroup* group = getVisibleGroup(currentGroup, getCurrentInstrumentType());
    const FXName* item = group ? visibleFXAt(group, currentIdx) : NULL;
    if (item) {
      selectInstrumentFX(fx,item->fx,currentInstrumentIdx);
      lastFX[0] = fx[0];
      lastFX[1] = fx[1];
    }
    return 1;
  }

  if (keys & keyEdit) {
    int oldGroup = currentGroup;
    int visibleGroupCount = getVisibleGroupCount(getCurrentInstrumentType());
    FXGroup* group = getVisibleGroup(currentGroup, getCurrentInstrumentType());
    if (!group) return 0;

    // Navigate based on d-pad input
    if (keys & keyRight) {
      // Move to next FX (linear navigation)
      currentIdx++;
      if (currentIdx >= visibleFXCount(group)) {
        // Reached end of current group - move to next group
        if (currentGroup < visibleGroupCount - 1) {
          currentGroup++;
          currentIdx = 0;
          expandedGroup = currentGroup;
        } else {
          // At last FX of last group - stay there
          currentIdx = visibleFXCount(group) - 1;
        }
      }
    } else if (keys & keyLeft) {
      // Move to previous FX (linear navigation)
      currentIdx--;
      if (currentIdx < 0) {
        // Reached beginning of current group - move to previous group
        if (currentGroup > 0) {
          currentGroup--;
          FXGroup* prevGroup = getVisibleGroup(currentGroup, getCurrentInstrumentType());
          currentIdx = prevGroup ? visibleFXCount(prevGroup) - 1 : 0;
          expandedGroup = currentGroup;
        } else {
          // At first FX of first group - stay there
          currentIdx = 0;
        }
      }
    } else if (keys & keyUp) {
      // Move up one row in grid (using group-specific column count)
      currentIdx -= group->columns;
      if (currentIdx < 0) {
        // Reached top of current group - move to previous group
        if (currentGroup > 0) {
          currentGroup--;
          FXGroup* prevGroup = getVisibleGroup(currentGroup, getCurrentInstrumentType());
          if (prevGroup) {
            // Position at last FX of previous group
            currentIdx = visibleFXCount(prevGroup) - 1;
          } else {
            currentIdx = 0;
          }
          expandedGroup = currentGroup;
        } else {
          // At top of first group - stay at first FX
          currentIdx = 0;
        }
      }
    } else if (keys & keyDown) {
      // Move down one row in grid (using group-specific column count)
      currentIdx += group->columns;
      if (currentIdx >= visibleFXCount(group)) {
        // Reached bottom of current group - move to next group
        if (currentGroup < visibleGroupCount - 1) {
          currentGroup++;
          currentIdx = 0;
          expandedGroup = currentGroup;
        } else {
          // At bottom of last group - stay at last FX
          currentIdx = visibleFXCount(group) - 1;
        }
      }
    }

    // If group changed, need full redraw
    if (oldGroup != currentGroup) {
      FXGroup* newGroup = getVisibleGroup(currentGroup, getCurrentInstrumentType());
      const FXName* item = newGroup ? visibleFXAt(newGroup, currentIdx) : NULL;
      if (item) {
        fxEditFullDraw(item->fx, currentInstrumentIdx, currentIsTable);
      }
    } else {
      // Same group, just update the FX selection
      // For now, do a simple redraw (can optimize later)
      FXGroup* currentGroupPtr = getVisibleGroup(currentGroup, getCurrentInstrumentType());
      const FXName* item = currentGroupPtr ? visibleFXAt(currentGroupPtr, currentIdx) : NULL;
      if (item) {
        fxEditFullDraw(item->fx, currentInstrumentIdx, currentIsTable);
      }
    }
  }

  return 0;
}
