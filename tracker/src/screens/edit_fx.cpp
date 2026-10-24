#include "soundbank_fx.h"
#include "screens.h"
#include "corelib_gfx.h"
#include "help.h"
#include "synth/sample_voice.h"
#include <algorithm>
#include <string>
#include <cstring>

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

static FXId availableInsertChoice(FXId fx) {
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
  if (getCurrentInstrumentType() == InstrumentType::Monk) {
    switch (fx) {
      case fxMWV: return "A 00  E 40  I 80  O BF  U FF";
      case fxMIN: return "Low voice <------> High voice";
      case fxMAM: return "Amount of breath in the voice";
      case fxMFL: return "Smooth pitch and vowel changes";
      case fxMFB: return "Depth of the voice's pitch movement";
      case fxMSH: return "Speed of the voice's pitch movement";
      default: return "";
    }
  }

  switch(fx) {
    case fxFPT:return "00=-24st 18=0 30=+24st";
    case fxFVL:return "Instrument volume: 00 silent, FF full";
    case fxFPN:return "00 left, 80 center, FF right";
    case fxFCF:return "20Hz-20kHz; FF uses Edit/Bank";
    case fxFRS:return "0-40dB; FF uses Edit/Bank";
    case fxFAT:case fxFHD:return "Next note: 0-20s; FF uses Edit/Bank";
    case fxFDC:return "Decay to Sustain; FF uses Edit/Bank";
    case fxFRL:return "After note OFF; FF uses Edit/Bank";
    case fxFSU:return "Held level 0-100%; FF uses Edit/Bank";
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
void selectInstrumentFX(FXCell* fx, FXId selected, uint8_t instrumentIdx) {
  if (!insertFXAvailable(selected)) return;
  NativeFXInfo info{};
  bool native=nativeInfo(instrumentIdx,selected,info);
  if(((selected>=fxFBR&&selected<=fxLEN)||(selected>=fxFPT&&selected<=fxFSU))&&!native)return;
  if(fx->command==selected) {
    if(native)fx->value=std::clamp(int(fx->value),info.minimum,info.maximum);
    return; // Preserve legal edits; repair values from a different engine.
  }
  fx->command=selected;
  if(native)fx->value=info.preset;
}

static const char* contextualFXHint(const FXCell* fx,int table,uint8_t instrument) {
  if (fx->command==fxPBN && instrument!=EMPTY_VALUE_8 &&
      chipnomadState->project.instruments[instrument].type==InstrumentType::SoundFont) {
    static char bend[80];
    int step=chipnomadState->project.instruments[instrument].chip.soundfont.controls.bendStepCents;
    if (!step) step=chipnomadState->project.linearPitch ? 25 : 1;
    snprintf(bend,sizeof(bend),"Bend: %+d cents/row (%d ct step)",int(int8_t(fx->value))*step,step);
    return bend;
  }
  NativeFXInfo native{};
  if(nativeInfo(instrument,fx->command,native) && native.label) {
    static char text[80];
    if (fx->command >= fxFPT && fx->command <= fxFSU) {
      int cmd=fx->command, value=fx->value;
      if (value==255 && cmd>=fxFCF) snprintf(text,sizeof(text),"%s: Edit/Bank",native.label);
      else if (cmd==fxFPT) snprintf(text,sizeof(text),"Pitch: %+d semitones",std::min(value,48)-24);
      else if (cmd==fxFCF) snprintf(text,sizeof(text),"Cutoff: %d Hz",soundbankFXCutoff(value));
      else if (cmd==fxFRS) snprintf(text,sizeof(text),"Resonance: %.1f dB",value*40.0/254);
      else if (cmd==fxFSU) snprintf(text,sizeof(text),"Sustain: %d%%",(value*100+127)/254);
      else if (cmd>=fxFAT) snprintf(text,sizeof(text),"%s: %d ms",native.label,soundbankFXTime(value));
      else snprintf(text,sizeof(text),"%s: %02X",native.label,value);
      return text;
    }
    if(fx->command>=fxOAR&&fx->command<=fxOE4)snprintf(text,sizeof(text),"OP1 %s: %02X",native.label,fx->value);
    else snprintf(text,sizeof(text),"%s: %02X",native.label,fx->value);
    return text;
  }
  // RNS/RNN carry full 16-bit commands that the legacy byte-based
  // helpFXHint cannot represent, so format their value hints here.
  if (fx->command == fxRNS) {
    static const char* rnsModes[] = {"always random", "random per PLAY",
      "pick and keep"};
    static char rns[41];
    uint8_t mode = fx->value >> 4;
    snprintf(rns, sizeof(rns), "Random slice: %s",
      mode <= 2 ? rnsModes[mode] : "?");
    return rns;
  }
  if (fx->command == fxRNN) {
    static const char* rnnModes[] = {"always random", "random per PLAY",
      "pick and keep"};
    static const char* rnnRanges[] = {"any note", "+3 st", "+6 st", "+12 st",
      "-3 st", "-6 st", "-12 st", "+/-3 st", "+/-6 st", "+/-12 st"};
    static char rnn[41];
    uint8_t mode = fx->value >> 4;
    uint8_t range = fx->value & 0x0f;
    snprintf(rnn, sizeof(rnn), "%s:%s",
      mode <= 2 ? rnnModes[mode] : "?",
      range <= 9 ? rnnRanges[range] : "?");
    return rnn;
  }
  if(fx->command<fxF11 || fx->command>fxF28) {
    uint8_t legacy[] = {(uint8_t)fx->command, fx->value};
    return helpFXHint(legacy, table, instrument);
  }
  static char text[80];int a=fx->command-fxF11;
  const auto& c=chipnomadState->project.trackInserts[*pSongTrack][a/8];
  const auto& d=insertDescriptor(c.module);
  char decoded[32];
  insertDescribe(decoded,sizeof(decoded),c.module,a%8,fx->value);
  snprintf(text,sizeof(text),"%s %s: %s",d.name,a%8<d.count?d.parameters[a%8].name:"Unused",decoded);
  return text;
}
static bool isFXAvailable(FXId fx, uint8_t instrumentIdx, int isTable) {
  if (!insertFXAvailable(fx)) return false;
  if(fx==fxFBR||(fx>=fxFET&&fx<=fxFLD))return false;
  if((fx>=fxFO1&&fx<=fxFO6)||fx==fxFFB)return false; // Retired personal commands.
  if (isTable && (fx == fxSCL || fx == fxCRD || fx == fxRNS || fx == fxRNN)) return false;
  // Note Lock pins note entry to the project scale, so SCL is not offered.
  if (fx == fxSCL && chipnomadState->project.scaleMode != 0) return false;
  InstrumentType instrumentType = getInstrumentType(instrumentIdx);
  const Instrument* instrument = instrumentIdx != EMPTY_VALUE_8 && instrumentIdx < PROJECT_MAX_INSTRUMENTS
    ? &chipnomadState->project.instruments[instrumentIdx] : NULL;
  if((fx>=fxFBR&&fx<=fxLEN)||(fx>=fxFPT&&fx<=fxFSU)||(fx>=fxMUV&&fx<=fxMLV))return instrument && instrumentFXAvailableForInstrument(instrument,fx);
  if (instrumentType == InstrumentType::Monk && fx == fxESH) return false;
  if (instrument && instrumentFXAvailableForInstrument(instrument, fx)) return true;
  for (int groupIdx = 0; groupIdx < fxGroupCount; groupIdx++) {
    FXGroup* group = &fxGroups[groupIdx];
    if (group->instType != InstrumentType::none && group->instType != instrumentType) continue;
    if (group->instType == InstrumentType::DrumSynth && instrument &&
        !instrumentFXAvailableForInstrument(instrument, fx)) continue;
    for (int i = 0; i < group->count; i++) {
      if (group->fxList[i].fx == fx) return true;
    }
  }
  return false;
}

static void stepFX(FXCell* fx, int direction, uint8_t instrumentIdx, int isTable) {
  for (int candidate = (int)fx->command + direction;
       candidate >= 0 && candidate < fxTotalCount; candidate += direction) {
    if (isFXAvailable((FXId)candidate, instrumentIdx, isTable)) {
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
    // MONK exposes its envelope in its own complete sixteen-control group.
    if (instrument && instrument->type == InstrumentType::Monk &&
        group->instType == InstrumentType::none &&
        group->fxList[i].fx >= fxEAT && group->fxList[i].fx <= fxESH) continue;

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
    // MONK exposes its envelope in its own complete sixteen-control group.
    if (instrument && instrument->type == InstrumentType::Monk &&
        group->instType == InstrumentType::none &&
        group->fxList[i].fx >= fxEAT && group->fxList[i].fx <= fxESH) continue;

    if (currentIsTable && (group->fxList[i].fx == fxSCL || group->fxList[i].fx == fxCRD)) continue;
    if (hideSCL && group->fxList[i].fx == fxSCL) continue;
    if(group->fxList[i].fx>=fxFBR && group->fxList[i].fx<=fxLEN && (!instrument || !instrumentFXAvailableForInstrument(instrument,group->fxList[i].fx)))continue;
    if (instrument && group->instType == InstrumentType::DrumSynth && !instrumentFXAvailableForInstrument(instrument, group->fxList[i].fx)) continue;
    if (visibleIndex-- == 0) return &group->fxList[i];
  }
  return NULL;
}

void fxEditFullDraw(FXId currentFX, uint8_t instrumentIdx, int isTable);

int editFX(CellEditAction action, FXCell* fx, FXCell* lastValue, int isTable, uint8_t instrumentIdx) {
  int result = 0;
  action = convertMultiAction(action);

  if (action == CellEditAction::clear) {
    // Clear FX
    if (fx->command != FX_ID_EMPTY) *lastValue = *fx;
    fx->command = FX_ID_EMPTY;
    fx->value = 0;
    result = 2;
  } else if (action == CellEditAction::tap) {
    // Insert last FX
    if (fx->command == FX_ID_EMPTY) {
      fx->value = lastValue->value;
      selectInstrumentFX(fx,availableInsertChoice(lastValue->command),instrumentIdx);
    }
    *lastValue = *fx;
    result = 2;
  } else if (action == CellEditAction::increase && fx->command != FX_ID_EMPTY) {
    stepFX(fx, 1, instrumentIdx, isTable);
    *lastValue = *fx;
    result = 2;
  } else if (action == CellEditAction::decrease && fx->command != FX_ID_EMPTY) {
    stepFX(fx, -1, instrumentIdx, isTable);
    *lastValue = *fx;
    result = 2;
  } else if (action == CellEditAction::increaseBig || action == CellEditAction::decreaseBig) {
    // Show FX select screen with instrument context
    fxEditFullDraw(fx->command, instrumentIdx, isTable);
    result = 1;
  }
  if (result != 1) screenMessage(0, "%s", contextualFXHint(fx, isTable, instrumentIdx));
  return result;
}

int editFXValue(CellEditAction action, FXCell* fx, FXCell* lastFX, int isTable, uint8_t instrumentIdx) {
  NativeFXInfo native{};
  if(nativeInfo(instrumentIdx,fx->command,native)) {
    bool multi=action==CellEditAction::multiIncrease || action==CellEditAction::multiDecrease || action==CellEditAction::multiIncreaseBig || action==CellEditAction::multiDecreaseBig;
    fx->value=std::clamp(int(fx->value),native.minimum,native.maximum);
    int handled=edit8noLast(action,&fx->value,native.maximum<16?1:16,native.minimum,native.maximum);
    if(handled&&!multi)lastFX->value=fx->value;
    screenMessage(0,"%s",contextualFXHint(fx,isTable,instrumentIdx));
    return handled;
  }
  if (fx->command == fxCRD) {
    int isNotMultiAction = action != CellEditAction::multiIncrease && action != CellEditAction::multiDecrease &&
      action != CellEditAction::multiIncreaseBig && action != CellEditAction::multiDecreaseBig;
    action = convertMultiAction(action);
    uint8_t chord = fx->value & 0x0f;
    uint8_t inversion = fx->value >> 4;
    int handled = 1;
    if (action == CellEditAction::clear) { chord = 0; inversion = 0; }
    else if (action == CellEditAction::tap) { if (fx->value == 0) { chord = lastFX->value & 0x0f; inversion = lastFX->value >> 4; } }
    else if (action == CellEditAction::increase) chord = (chord + 1) & 0x0f;
    else if (action == CellEditAction::decrease) chord = (chord + 15) & 0x0f;
    else if (action == CellEditAction::increaseBig && inversion < 0x0f) ++inversion;
    else if (action == CellEditAction::decreaseBig && inversion > 0) --inversion;
    else handled = 0;
    if (handled) {
      fx->value = (inversion << 4) | chord;
      if (isNotMultiAction) lastFX->value = fx->value;
    }
    screenMessage(0, "%s", contextualFXHint(fx, isTable, instrumentIdx));
    return handled;
  }

  action = convertMultiAction(action);

  if (fx->command == fxSPD && !chipnomadState->project.signedTrackSpeed) {
    int handled = edit8noLast(action, &fx->value, 1, 0, 0x10);
    screenMessage(0, "%s", contextualFXHint(fx, isTable, instrumentIdx));
    return handled;
  }

  // SLI cycles through the instrument's live slice count (1..count), so a
  // value can never point at an empty slice. 00 keeps the normal note
  // mapping.
  if (fx->command == fxSLI) {
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
          fx->value = 0;
          break;
        case CellEditAction::tap:
          if (fx->value == 0) fx->value = lastFX->value && lastFX->value <= sliceCount ? lastFX->value : 1;
          break;
        case CellEditAction::increase:
          fx->value = fx->value >= sliceCount ? 1 : fx->value + 1;
          break;
        case CellEditAction::decrease:
          fx->value = fx->value <= 1 ? sliceCount : fx->value - 1;
          break;
        case CellEditAction::increaseBig:
        case CellEditAction::decreaseBig:
          // Same cycle: the slice list is short, big steps add nothing.
          fx->value = action == CellEditAction::increaseBig
            ? (fx->value >= sliceCount ? 1 : fx->value + 1)
            : (fx->value <= 1 ? sliceCount : fx->value - 1);
          break;
        default:
          handled = 0;
          break;
      }
      if (handled && isNotMultiAction) lastFX->value = fx->value;
      screenMessage(0, "%s", contextualFXHint(fx, isTable, instrumentIdx));
      return handled;
    }
    // No slicing on this instrument: fall through to the generic editor so
    // the value stays inert but still editable.
  }

  // RNS cycles through its modes (0 always random, 1 random per PLAY,
  // 2 pick and keep). The low nibble is unused and stays 0.
  if (fx->command == fxRNS) {
    int isNotMultiAction = action != CellEditAction::multiIncrease && action != CellEditAction::multiDecrease &&
      action != CellEditAction::multiIncreaseBig && action != CellEditAction::multiDecreaseBig;
    action = convertMultiAction(action);
    uint8_t mode = fx->value >> 4;
    int handled = 1;
    switch (action) {
      case CellEditAction::clear:
        mode = 0;
        break;
      case CellEditAction::tap:
        if (fx->value == 0) mode = lastFX->value >> 4 ? lastFX->value >> 4 : 1;
        break;
      case CellEditAction::increase:
      case CellEditAction::increaseBig:
        mode = mode >= 2 ? 0 : mode + 1;
        break;
      case CellEditAction::decrease:
      case CellEditAction::decreaseBig:
        mode = mode == 0 ? 2 : mode - 1;
        break;
      default:
        handled = 0;
        break;
    }
    if (handled) {
      fx->value = (uint8_t)(mode << 4);
      if (isNotMultiAction) lastFX->value = fx->value;
    }
    screenMessage(0, "%s", contextualFXHint(fx, isTable, instrumentIdx));
    return handled;
  }

  // RNN edits its two nibbles independently: EDIT+UP/DOWN changes the
  // mode (high nibble), EDIT+LEFT/RIGHT changes the range (low nibble).
  if (fx->command == fxRNN) {
    int isNotMultiAction = action != CellEditAction::multiIncrease && action != CellEditAction::multiDecrease &&
      action != CellEditAction::multiIncreaseBig && action != CellEditAction::multiDecreaseBig;
    action = convertMultiAction(action);
    uint8_t mode = fx->value >> 4;
    uint8_t range = fx->value & 0x0f;
    int handled = 1;
    switch (action) {
      case CellEditAction::clear:
        mode = 0;
        range = 0;
        break;
      case CellEditAction::tap:
        if (fx->value == 0) { mode = lastFX->value >> 4; range = lastFX->value & 0x0f; }
        break;
      case CellEditAction::increase:
        range = (range + 1) & 0x0f;
        break;
      case CellEditAction::decrease:
        range = (range + 15) & 0x0f;
        break;
      case CellEditAction::increaseBig:
        mode = mode >= 2 ? 0 : mode + 1;
        break;
      case CellEditAction::decreaseBig:
        mode = mode == 0 ? 2 : mode - 1;
        break;
      default:
        handled = 0;
        break;
    }
    if (handled) {
      fx->value = (uint8_t)((mode << 4) | range);
      if (isNotMultiAction) lastFX->value = fx->value;
    }
    screenMessage(0, "%s", contextualFXHint(fx, isTable, instrumentIdx));
    return handled;
  }

  uint8_t bigStep = 16;
  if (fx->command == fxENT || fx->command == fxTNN || fx->command == fxENN || fx->command == fxSFN) {
    // Note-setting FX: use octave size for big step
    bigStep = chipnomadState->project.pitchTable.octaveSize;
  }

  int handled = edit8noLimit(action, &fx->value, &lastFX->value, bigStep);
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

// Keep the picker clear of the track sidebar and bottom message row. Help
// has a fixed footprint, so changing commands never changes group spacing.
static constexpr int fxHelpWidth = 33;
// One blank row separates the fixed help block from the groups.
static constexpr int fxListTop = 6;
static int fxListBottom() { return 18 - gfxGetContentRowOffset(); }
static int fxColumns(const FXGroup* group) {
  return group ? std::clamp(group->columns, 1, 8) : 1;
}

static void printFXLine(int y, const std::string& text) {
  std::string line = text;
  if (line.size() > fxHelpWidth) line = line.substr(0, fxHelpWidth - 3) + "...";
  gfxPrint(1, y, line.c_str());
}

// Keep the title separate and reflow existing help at word boundaries.
// The compact summary never draws into the list or adjacent track sidebar.
static void drawPickerHelp(const char* text, int firstRow, int lastRow) {
  std::string remaining = text ? text : "";
  for (int row = firstRow; row <= lastRow && !remaining.empty(); ++row) {
    size_t end = remaining.find('\n');
    if (end == std::string::npos) end = remaining.size();
    if (end > fxHelpWidth) {
      end = remaining.rfind(' ', fxHelpWidth);
      if (end == std::string::npos || end == 0) end = fxHelpWidth;
    }
    std::string line = remaining.substr(0, end);
    remaining.erase(0, end);
    if (row == 1) {
      // Body line breaks are formatting, not extra rows in the picker.
      std::replace(remaining.begin(), remaining.end(), '\n', ' ');
    }
    if (!remaining.empty() && (remaining[0] == ' ' || remaining[0] == '\n')) remaining.erase(0, 1);
    if (row == lastRow && !remaining.empty()) line = line.substr(0, fxHelpWidth - 3) + "...";
    gfxSetFgColor(row == 1 ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    printFXLine(row, line);
  }
}

// Draw a group header at specified Y position
// Returns the Y position after the header (for next element)
int drawGroupHeader(int visibleGroupIdx, int y, int isCurrent) {
  FXGroup* group = getVisibleGroup(visibleGroupIdx, getCurrentInstrumentType());
  if (!group) return y;
  if (y < fxListTop || y > fxListBottom()) return y + 1;

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
  } else if (getCurrentInstrumentType() == InstrumentType::Monk && group->instType == InstrumentType::none && group->fxList[0].fx == fxEAT) {
    gfxPrint(1, y, "Trigger FX");
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

  int cols = fxColumns(group);

  // Draw FX in grid with group-specific column count
  int count = visibleFXCount(group);
  for (int idx = 0; idx < count; idx++) {
    const FXName* item = visibleFXAt(group, idx);
    if (!item) continue;
    int row = idx / cols;
    int col = idx % cols;
    int fxY = y + row;
    if (fxY < fxListTop || fxY > fxListBottom()) continue;

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

void fxEditFullDraw(FXId currentFX, uint8_t instrumentIdx, int isTable) {
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

  NativeFXInfo info{};
  if (nativeInfo(instrumentIdx, currentFX, info) && info.label) {
    gfxSetFgColor(appSettings.colorScheme.textValue);
    std::string title = info.label;
    if (currentFX >= fxOAR && currentFX <= fxOE4) title += " (OP1)";
    printFXLine(1, title);
    drawPickerHelp(nativeControlDescription(currentFX), 2, 3);
    if (!nativeControlDescription(currentFX)[0]) {
      gfxSetFgColor(appSettings.colorScheme.textDefault);
      printFXLine(2, info.relative ? "Relative to instrument setting" : "Absolute native value");
    }
    gfxSetFgColor(appSettings.colorScheme.textInfo);
    gfxPrintf(1, 4, "%s %02X   Range %02X-%02X",
      info.relative ? "Preset FX" : "Preset", info.preset, info.minimum, info.maximum);
  } else if (currentFX >= fxF11 && currentFX <= fxF28) {
    const int address = currentFX - fxF11, parameter = address % 8;
    const auto& config = chipnomadState->project.trackInserts[*pSongTrack][address / 8];
    const auto& descriptor = insertDescriptor(config.module);
    char line[128], decoded[32];
    gfxSetFgColor(appSettings.colorScheme.textValue);
    snprintf(line, sizeof(line), "TF%d %s: %s", address / 8 + 1,
      descriptor.name, descriptor.parameters[parameter].name);
    printFXLine(1, line);
    gfxSetFgColor(appSettings.colorScheme.textDefault);
    snprintf(line, sizeof(line), "%s Absolute; holds until reset", fxNames[currentFX].name);
    printFXLine(2, line);
    char low[32], high[32];
    int maximum = insertClamp(config.module, parameter, 255);
    insertDescribe(low, sizeof(low), config.module, parameter, 0);
    insertDescribe(high, sizeof(high), config.module, parameter, maximum);
    snprintf(line, sizeof(line), "00: %s  %02X: %s", low, maximum, high);
    // Keep both numeric bounds when decoded endpoints cannot fit together.
    if (strlen(line) > fxHelpWidth) snprintf(line, sizeof(line), "Range 00-%02X", maximum);
    printFXLine(3, line);
    insertDescribe(decoded, sizeof(decoded), config.module, parameter, config.values[parameter]);
    snprintf(line, sizeof(line), "Saved %02X: %s", config.values[parameter], decoded);
    gfxSetFgColor(appSettings.colorScheme.textInfo);
    printFXLine(4, line);
  } else {
    drawPickerHelp(helpFXDescription((FX)currentFX, instrumentIdx), 1, 4);
  }

  FXGroup* expanded = getVisibleGroup(expandedGroup, instType);
  int expandedRows = expanded ? (visibleFXCount(expanded) + fxColumns(expanded) - 1) / fxColumns(expanded) : 0;
  const int totalRows = visibleGroupCount + expandedRows;
  const int availableRows = fxListBottom() - fxListTop + 1;
  const int selectedRow = currentGroup + 1 + currentIdx / fxColumns(expanded);
  // Scroll only when necessary, always keeping the selected command visible.
  const int scroll = std::clamp(selectedRow - availableRows + 1, 0,
                                std::max(0, totalRows - availableRows));
  int y = fxListTop - scroll;
  for (int g = 0; g < visibleGroupCount; ++g) {
    y = drawGroupHeader(g, y, g == currentGroup);
    if (g == expandedGroup) y = drawFXList(g, y);
  }
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  if (scroll) gfxPrint(34, fxListTop, "^");
  if (totalRows - scroll > availableRows) gfxPrint(34, fxListBottom(), "v");
}


int fxEditInput(int keys, int tapCount, FXCell* fx, FXCell* lastFX) {
  if (keys == 0) {
    // Selection complete - update the FX value
    FXGroup* group = getVisibleGroup(currentGroup, getCurrentInstrumentType());
    const FXName* item = group ? visibleFXAt(group, currentIdx) : NULL;
    if (item) {
      selectInstrumentFX(fx,item->fx,currentInstrumentIdx);
      *lastFX = *fx;
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
      currentIdx -= fxColumns(group);
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
      currentIdx += fxColumns(group);
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
