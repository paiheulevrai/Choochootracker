#include "screens.h"
#include "common.h"
#include "corelib_gfx.h"
#include "corelib/corelib_file.h"
#include "utils.h"
#include "chipnomad_lib.h"
#include "project_utils.h"
#include "screen_instrument.h"
#include "user_presets.h"
#include "user_preset_browser.h"
#include "waveform_display.h"
#include "copy_paste.h"
#include "file_browser.h"
#include "import_vts.h"
#include "selection_popup.h"
#include "simple_chip_presets.h"
#include "audio_manager.h"
#include <string.h>
#include <stdio.h>
#include <strings.h>
#include <math.h>
#include <chrono>
#include <initializer_list>

extern const AppScreen screenInstrumentPool;

int cInstrument = 0;
static int isCharEdit = 0;
static int typeButtonDown = 0;
static Bitmap* envelopePreviewBitmap = NULL;
static Bitmap* livePreviewBitmap = NULL;
static int livePreviewWasActive = 0;
static std::chrono::steady_clock::time_point livePreviewRefresh;
static std::chrono::steady_clock::time_point livePreviewLastActivity;
static int livePreviewInstrument = -1;
static InstrumentType livePreviewType = InstrumentType::none;
static int livePreviewTrack = -1;

static void resetLivePreview() {
  livePreviewWasActive = 0;
  livePreviewRefresh = {};
  livePreviewLastActivity = {};
  livePreviewInstrument = -1;
  livePreviewTrack = -1;
}

static SelectionItem instrumentTypeChip[] = {
  {"SID / C64", (int)InstrumentType::SID, NULL, 0},
  {NULL, (int)InstrumentType::AY1, NULL, 0},
  {NULL, (int)InstrumentType::AY2, NULL, 0},
  {NULL, (int)InstrumentType::AYSample, NULL, 0},
  {NULL, (int)InstrumentType::SegaPSG, NULL, 0},
  {NULL, (int)InstrumentType::GBPulse, NULL, 0},
  {NULL, (int)InstrumentType::GBNoise, NULL, 0},
};
static SelectionItem instrumentTypeSynth[] = {
  {NULL, (int)InstrumentType::Monk, NULL, 0},
  {NULL, (int)InstrumentType::AChChid, NULL, 0},
  {NULL, (int)InstrumentType::Braids, NULL, 0},
  {NULL, (int)InstrumentType::Plaits, NULL, 0},
  {NULL, (int)InstrumentType::PlaitsAlt, NULL, 0},
  {NULL, (int)InstrumentType::MME, NULL, 0},
};
static SelectionItem instrumentTypeDrums[] = {
  {NULL, (int)InstrumentType::DrumSynth, NULL, 0},
  {NULL, (int)InstrumentType::Sintered, NULL, 0},
};
static SelectionItem instrumentTypeSample[] = {
  {NULL, (int)InstrumentType::Sample, NULL, 0},
  {NULL, (int)InstrumentType::SCWF, NULL, 0},
  {NULL, (int)InstrumentType::BYOWTBL, NULL, 0},
  {NULL, (int)InstrumentType::SoundFont, NULL, 0},
};
static SelectionItem instrumentTypeFM[] = {
  {"OPLL / YM2413", (int)InstrumentType::OPLL, NULL, 0},
  {"VRC7 / DS1001", (int)InstrumentType::VRC7, NULL, 0},
  {"AdLib / YM3812", (int)InstrumentType::OPL2, NULL, 0},
  {"OPL3 / YMF262", (int)InstrumentType::OPL3, NULL, 0},
  {"Genesis / YM2612", (int)InstrumentType::GenesisFM, NULL, 0},
  {"Arcade / YM2151", (int)InstrumentType::ArcadeFM, NULL, 0},
  {"DX7 FM", (int)InstrumentType::DX7, NULL, 0},
};
static SelectionItem instrumentTypeMidi[] = {
  {NULL, (int)InstrumentType::Midi, NULL, 0},
};
static const SelectionItem instrumentTypeCategories[] = {
  {"CHIP", -1, instrumentTypeChip, 7},
  {"DRUMS", -1, instrumentTypeDrums, 2},
  {"SAMPLE", -1, instrumentTypeSample, 4},
  {"SYNTH", -1, instrumentTypeSynth, 5},
  {"MIDI", -1, instrumentTypeMidi, 1},
  {"FM", -1, instrumentTypeFM, sizeof(instrumentTypeFM) / sizeof(instrumentTypeFM[0])},
};

static const InstrumentType instrumentTypesQuickCycle[] = {
  InstrumentType::none, InstrumentType::DrumSynth, InstrumentType::Sintered,
  InstrumentType::AY1, InstrumentType::AY2, InstrumentType::AYSample,
  InstrumentType::SCWF, InstrumentType::BYOWTBL, InstrumentType::Sample, InstrumentType::SoundFont,
  InstrumentType::AChChid, InstrumentType::Braids,
  InstrumentType::Plaits, InstrumentType::PlaitsAlt,
  InstrumentType::MME, InstrumentType::Monk,
  InstrumentType::SegaPSG, InstrumentType::GBPulse, InstrumentType::GBNoise,
  InstrumentType::GenesisFM, InstrumentType::ArcadeFM, InstrumentType::DX7, InstrumentType::Midi, InstrumentType::OPLL, InstrumentType::VRC7, InstrumentType::OPL2, InstrumentType::OPL3,
  InstrumentType::SID,
};

static int editInstrumentType(CellEditAction action, InstrumentType* type) {
  action = convertMultiAction(action);
  if (action == CellEditAction::clear) { *type = InstrumentType::none; return 1; }
  if (action == CellEditAction::tap) return 1;
  int direction = (action == CellEditAction::increase || action == CellEditAction::increaseBig) ? 1 :
                  (action == CellEditAction::decrease || action == CellEditAction::decreaseBig) ? -1 : 0;
  if (!direction) return 0;
  int count = sizeof(instrumentTypesQuickCycle) / sizeof(instrumentTypesQuickCycle[0]);
  for (int i = 0; i < count; ++i) if (instrumentTypesQuickCycle[i] == *type) {
    *type = instrumentTypesQuickCycle[(i + direction + count) % count];
    return 1;
  }
  return 0;
}

static void getInstrumentFilename(char* filename, int size) {
  if (strlen(chipnomadState->project.instruments[cInstrument].name) > 0) {
    snprintf(filename, size, "%s", chipnomadState->project.instruments[cInstrument].name);
  } else {
    snprintf(filename, size, "instrument_%02X", cInstrument);
  }
}

typedef int (*ImportHandler)(const char* path, int instrumentIdx);

struct InstrumentFormat {
  const char* extension;
  ImportHandler handler;
  const char* successMsg;
  const char* errorMsg;
};

static const InstrumentFormat importFormats[] = {
  { ".vts", instrumentLoadVTS, "VTS sample imported", "Failed to import VTS" },
};

struct InstrumentLoad {
  const char* path;
  int instrument;
  ImportHandler handler;
};

static int loadInstrument(void* userdata) {
  auto* load = static_cast<InstrumentLoad*>(userdata);
  return load->handler ? load->handler(load->path, load->instrument) :
    instrumentLoad(&chipnomadState->project, load->path, load->instrument);
}

static void onInstrumentLoaded(const char* path) {
  int result = 1;
  const char* ext = strrchr(path, '.');
  int formatHandled = 0;
  if(ext&&!strcasecmp(ext,".syx")){instrumentFMImportSysEx(path);return;}

  if (ext != NULL) {
    for (size_t i = 0; i < sizeof(importFormats) / sizeof(importFormats[0]); i++) {
      if (strcasecmp(ext, importFormats[i].extension) == 0) {
        InstrumentLoad load{path, cInstrument, importFormats[i].handler};
        result = audioManager.mutateProject(loadInstrument, &load);
        if (result == 0) {
          screenMessage(MESSAGE_TIME, importFormats[i].successMsg);
        } else {
          screenMessage(MESSAGE_TIME_ERROR, importFormats[i].errorMsg);
        }
        formatHandled = 1;
        break;
      }
    }
  }

  if (!formatHandled) {
    InstrumentLoad load{path, cInstrument, nullptr};
    result = audioManager.mutateProject(loadInstrument, &load);
    if (result == 0) {
      screenMessage(MESSAGE_TIME, "Instrument loaded");
    } else {
      screenMessage(MESSAGE_TIME_ERROR, "%s", projectFileError);
    }
  }

  if (result == 0) {
    // Save the directory path
    const char* lastSeparator = strrchr(path, PATH_SEPARATOR);
    if (lastSeparator) {
      int pathLen = lastSeparator - path;
      if (pathLen < PATH_LENGTH) {
        strncpy(appSettings.instrumentPath, path, pathLen);
        appSettings.instrumentPath[pathLen] = 0;
      }
    }
  }
  screenSetup(&screenInstrument, cInstrument);
}

static void onInstrumentSaved(const char* folderPath) {
  // The file browser constructs the full path internally, but we need to recreate it
  // since the callback only gives us the folder path
  char fullPath[2048];
  char filename[32];
  getInstrumentFilename(filename, sizeof(filename));
  snprintf(fullPath, sizeof(fullPath), "%s%s%s.cni", folderPath, PATH_SEPARATOR_STR, filename);

  auto& instrument = chipnomadState->project.instruments[cInstrument];
  const bool monk = instrument.type == InstrumentType::Monk;
  const uint8_t previousBank = monk ? instrument.chip.monk.factory : 0;
  if (monk) instrument.chip.monk.factory = 0;
  if (instrumentSave(&chipnomadState->project, fullPath, cInstrument) == 0) {
    if (monk) projectModified = 1;
    strncpy(appSettings.instrumentPath, folderPath, PATH_LENGTH);
    appSettings.instrumentPath[PATH_LENGTH - 1] = 0;
  } else if (monk) instrument.chip.monk.factory = previousBank;
  screenSetup(&screenInstrument, cInstrument);
}

static void onInstrumentCancelled(void) {
  screenSetup(&screenInstrument, cInstrument);
}

static void drawRowHeader(int row, CellState state);
static void drawColHeader(int col, CellState state);
static ScreenData* instrumentScreen(void);

static InstrumentVoicePostSettings* voicePostSettings(Instrument* instrument, InstrumentType type) {
  (void)type;
  return instrumentVoicePostSettings(instrument);
}

static void setInstrumentType(InstrumentType newType) {
  Instrument* instrument = &chipnomadState->project.instruments[cInstrument];
  InstrumentType oldType = instrument->type;
  if (oldType == newType) {
    screenSetup(&screenInstrument, cInstrument);
    return;
  }
  InstrumentVoicePostSettings savedEnvelope = {};
  InstrumentVoicePostSettings* oldPost = voicePostSettings(instrument, oldType);
  int preserveEnvelope = oldPost && (oldPost->attack || oldPost->decay || oldPost->sustain || oldPost->release);
  if (preserveEnvelope) savedEnvelope = *oldPost;
  getInstrumentFunctions(oldType).free(instrument);
  instrument->type = newType;
  getInstrumentFunctions(newType).init(instrument);
  if (preserveEnvelope && !isSimpleChip(newType)) {
    InstrumentVoicePostSettings* post = voicePostSettings(instrument, newType);
    if (post) {
      post->attack = savedEnvelope.attack; post->decay = savedEnvelope.decay;
      post->sustain = savedEnvelope.sustain; post->release = savedEnvelope.release;
      post->envelopeShape = savedEnvelope.envelopeShape;
    }
  }
  ScreenData* screen = instrumentScreen();
  screen->cursorRow = screen->cursorCol = screen->topRow = 0;
  projectModified = 1;
  screenSetup(&screenInstrument, cInstrument);
}

static void selectInstrumentType(int value) {
  setInstrumentType((InstrumentType)value);
}

static void cancelInstrumentTypeSelection(void) {
  screenSetup(&screenInstrument, cInstrument);
}

static int instrumentTypePopupInput(int isKeyDown, int keys, ScreenData* screen) {
  if (screen->cursorRow != 0 || screen->cursorCol != 0) {
    typeButtonDown = 0;
    return 0;
  }
  PopupEditInput input = popupEditInput(isKeyDown, keys, &typeButtonDown);
  if (input == PopupEditInput::hold) return 1;
  if (input == PopupEditInput::cycle) {
    return 0;
  }
  if (input == PopupEditInput::open) {
    selectionPopupSetup("INSTRUMENT TYPE", instrumentTypeCategories,
      sizeof(instrumentTypeCategories) / sizeof(instrumentTypeCategories[0]),
      (int)chipnomadState->project.instruments[cInstrument].type,
      selectInstrumentType, cancelInstrumentTypeSelection);
    screenSetup(&screenSelectionPopup, 0);
    return 1;
  }
  return 0;
}



static ScreenData screenInstrumentNone = {
  .rows = 1,
  .cursorRow = 0,
  .cursorCol = 0,
  .topRow = 0,
  .selectMode = -1,
  .selectStartRow = 0,
  .selectStartCol = 0,
  .selectAnchorRow = 0,
  .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = instrumentCommonColumnCount,
  .drawStatic = instrumentCommonDrawStatic,
  .drawCursor = instrumentCommonDrawCursor,
  .drawSelection = NULL,
  .drawRowHeader = drawRowHeader,
  .drawColHeader = drawColHeader,
  .drawField = instrumentCommonDrawField,
  .onEdit = instrumentCommonOnEdit,
  .onInput = NULL,
  .onRawInput = NULL,
  .isCellValid = NULL,
  .getLoopRange = NULL,
};

static ScreenData* instrumentScreen(void) {
  // Slot navigation can redraw directly without running setup().
  instrumentFMSetContext(cInstrument, chipnomadState->project.instruments[cInstrument].type);
  static ScreenData* const screens[] = {
    &screenInstrumentNone, &screenInstrumentAY, &screenInstrumentAY2,
    &screenInstrumentAYSample, &screenInstrumentBraids, &screenInstrumentSample,
    &screenInstrumentSCWF, &screenInstrumentBYOWTBL, &screenInstrumentPlaits, &screenInstrumentAChChid,
    &screenInstrumentDrumSynth, &screenInstrumentMME,
    &screenInstrumentSintered,
    &screenInstrumentMidi, &screenInstrumentOPLL, &screenInstrumentOPL, &screenInstrumentSimpleChip, &screenInstrumentOPL, &screenInstrumentSoundFont, &screenInstrumentMonk,
  };
  InstrumentScreenKind kind = getInstrumentDefinition(chipnomadState->project.instruments[cInstrument].type)->screen;
  ScreenData* data = screens[(int)kind];
  data->drawRowHeader = drawRowHeader;
  data->drawColHeader = drawColHeader;
  return data;
}

static void init(void) {
  isCharEdit = 0;
  typeButtonDown = 0;
  screenInstrumentNone.cursorRow = 0;
  screenInstrumentNone.cursorCol = 0;
  SelectionItem* groups[] = {instrumentTypeChip, instrumentTypeSample, instrumentTypeSynth, instrumentTypeDrums, instrumentTypeMidi};
  const int counts[] = {7, 4, 5, 2, 1};
  for (int group = 0; group < 5; ++group)
    for (int item = 0; item < counts[group]; ++item)
      groups[group][item].label = getInstrumentDefinition((InstrumentType)groups[group][item].value)->uiName;
}

#ifdef DESKTOP_BUILD
static void keyJazzReleaseAll(void);
#endif

static void setup(int input) {
  isCharEdit = 0;
  resetLivePreview();
#ifdef DESKTOP_BUILD
  keyJazzReleaseAll(); // a key held while leaving the screen never sends its release here
#endif
  if (input != -1) {
    cInstrument = input;
  }
}

static void fullRedraw(void) {
  ScreenData* screen = instrumentScreen();
  screenFullRedraw(screen);
}

static void draw(void) {
  if (chipnomadState->project.instruments[cInstrument].type==InstrumentType::SoundFont) {
    auto& d=screenInstrumentSoundFont;
    for (int row : {3,4}) for (int col=0; col<d.getColumnCount(row); ++col)
      d.drawField(col,row,d.cursorRow==row && d.cursorCol==col ? CellState::focus : CellState::normal);
    d.drawCursor(d.cursorCol,d.cursorRow);
  }
  instrumentSIDDrawHints();
  // Field redraws may paint their static preview. The live scope owns this
  // region for the frame, including frames between waveform refreshes.
  instrumentCommonDrawLivePreview();
}

///////////////////////////////////////////////////////////////////////////////
//
// Common part of the form
//

static void drawRowHeader(int row, CellState state) {}
static void drawColHeader(int col, CellState state) {}

int instrumentCommonColumnCount(int row) {
  if (row == 0) {
    return 3; // Instrument type, load, save
  } else if (row == 1) {
    return 15; // Instrument name
  } else if (row == 2) {
    return 4; // Transpose, table speed, volume, pan
  }
  return 1; // Default value
}

void instrumentCommonDrawStatic(void) {
  const ColorScheme cs = appSettings.colorScheme;

  gfxSetFgColor(cs.textTitles);
  gfxPrintf(0, 0, "INSTRUMENT %02X", cInstrument);

  gfxSetFgColor(cs.textDefault);
  gfxPrint(0, 2, "Type");

  if (chipnomadState->project.instruments[cInstrument].type == InstrumentType::none) return;

  gfxPrint(0, 3, "Name");
  gfxPrint(0, 4, "Transp.");
  gfxPrint(14, 4, "TIC");
  gfxPrint(28, 3, "PAN");
  gfxPrint(28, 4, "VOL");
}

void instrumentCommonDrawCursor(int col, int row) {
  if (row == 0 && col == 0) {
    // Type
    gfxCursor(8, 2, strlen(instrumentTypeName(chipnomadState->project.instruments[cInstrument].type)));
  } else if (row == 0 && col == 1) {
    // Load
    gfxCursor(23, 2, 4);
  } else if (row == 0 && col == 2) {
    // Save
    gfxCursor(29, 2, 4);
  } else if (row == 1) {
    // Instrument name
    gfxCursor(8 + col, 3, 1);
  } else if (row == 2 && col == 0) {
    // Transpose on/off
    gfxCursor(8, 4, 3);
  } else if (row == 2 && col == 1) {
    // Table tic speed
    gfxCursor(18, 4, 2);
  } else if (row == 2 && col == 2) {
    gfxCursor(32, 4, 2);
  } else if (row == 2 && col == 3) {
    gfxCursor(32, 3, 2);
  }
}

void instrumentCommonDrawField(int col, int row, CellState state) {
  gfxSetFgColor(appSettings.colorScheme.textDefault);

  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);

  if (row == 0 && col == 0) {
    // Instrument type
    gfxClearRect(8, 2, 8, 1);
    gfxPrint(8, 2, instrumentTypeName(chipnomadState->project.instruments[cInstrument].type));
  } else if (row == 0 && col == 1) {
    // Load
    gfxPrint(23, 2, "Load");
  } else if (row == 0 && col == 2) {
    // Save
    gfxPrint(29, 2, "Save");
  } else if (row == 0 && col == 3) {
    // Save
    gfxPrint(29, 2, "Save");
  } else if (row == 1) {
    // Instrument name
    gfxClearRect(8, 3, PROJECT_INSTRUMENT_NAME_LENGTH, 1);
    gfxPrintf(8, 3, "%s", chipnomadState->project.instruments[cInstrument].name);
  } else if (row == 2 && col == 0) {
    // Transpose
    gfxPrintf(8, 4, chipnomadState->project.instruments[cInstrument].transposeEnabled ? "On " : "Off");
  } else if (row == 2 && col == 1) {
    // Table tic speed
    gfxPrint(18, 4, byteToHex(chipnomadState->project.instruments[cInstrument].tableSpeed));
  } else if (row == 2 && col == 2) {
    gfxPrint(32, 4, byteToHex(chipnomadState->project.instruments[cInstrument].volume));
  } else if (row == 2 && col == 3) {
    gfxPrint(32, 3, byteToHex(chipnomadState->project.instruments[cInstrument].pan));
  }
}

void instrumentCommonDrawVoicePostStatic(int drawEnvelope) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(18, 7, "FILTER");
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(18, 8, "Type");
  gfxPrint(18, 9, "Mode");
  gfxPrint(18, 10, "Slope");
  gfxPrint(18, 11, "Cutoff");
  gfxPrint(18, 12, "Reso");
  if (drawEnvelope) {
    gfxSetFgColor(appSettings.colorScheme.textTitles);
    gfxPrint(0, 14, "ADSR");
    gfxSetFgColor(appSettings.colorScheme.textDefault);
    gfxPrint(6, 14, "A");
    gfxPrint(11, 14, "D");
    gfxPrint(16, 14, "S");
    gfxPrint(21, 14, "R");
    gfxPrint(27, 14, "SHP");
  }
}

int instrumentCommonDrawVoicePostCursor(int col, int row) {
  if (row == 9) {
    gfxCursor(col == 4 ? 31 : 7 + col * 5, 14, 2);
    return 1;
  }
  if (col && row >= 4 && row <= 8) {
    InstrumentVoicePostSettings* post = instrumentVoicePostSettings(&chipnomadState->project.instruments[cInstrument]);
    int width = 2;
    if (row == 4) {
      static const char* types[] = {"Off", "Clean", "Classic", "Aggro", "Acid"};
      width = (int)strlen(types[post && post->filterCharacter <= 4 ? post->filterCharacter : 1]);
    } else if (row == 6) {
      width = 5;
    } else if (row == 7) {
      char value[12];
      snprintf(value, sizeof(value), "%u Hz", post ? post->filterCutoffHz : 0);
      width = (int)strlen(value);
    }
    gfxCursor(26, row + 4, width);
    return 1;
  }
  return 0;
}

int instrumentCommonDrawVoicePostField(int col, int row, CellState state,
                                        const InstrumentVoicePostSettings* post) {
  if (row == 9) {
    const uint8_t values[] = {post->attack, post->decay, post->sustain, post->release, post->envelopeShape};
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    gfxPrint(col == 4 ? 31 : 7 + col * 5, 14, byteToHex(values[col]));
    instrumentCommonDrawEnvelopePreview(post->attack, post->decay, post->sustain, post->release, post->envelopeShape);
    return 1;
  }
  if (!col || row < 4 || row > 8) return 0;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  gfxClearRect(26, row + 4, 8, 1);
  switch (row) {
    case 4: { static const char* types[] = {"Off", "Clean", "Classic", "Aggro", "Acid"}; gfxPrint(26, 8, types[post->filterCharacter <= 4 ? post->filterCharacter : 1]); break; }
    case 5: { static const char* modes[] = {"LP", "BP", "HP"}; gfxPrint(26, 9, modes[post->filterMode <= 2 ? post->filterMode : 0]); break; }
    case 6: gfxPrint(26, 10, post->filterSlope24dB ? "24 dB" : "12 dB"); break;
    case 7: gfxPrintf(26, 11, "%u Hz", post->filterCutoffHz); break;
    case 8: gfxPrint(26, 12, byteToHex(post->filterResonance)); break;
  }
  return 1;
}

int instrumentCommonOnEditVoicePost(int col, int row, CellEditAction action,
                                     InstrumentVoicePostSettings* post) {
  if (row == 9) {
    uint8_t* values[] = {&post->attack, &post->decay, &post->sustain, &post->release, &post->envelopeShape};
    int handled = edit8noLast(action, values[col], 16, 0, 255);
    if (handled) currentScreen->fullRedraw();
    return handled;
  }
  if (!col) return 0;
  switch (row) {
    case 4: {
      int handled = edit8noLast(action, &post->filterCharacter, 1, 0, 4);
      post->filterEnabled = post->filterCharacter != 0;
      return handled;
    }
    case 5: return edit8noLast(action, &post->filterMode, 1, 0, 2);
    case 6: return edit8noLast(action, &post->filterSlope24dB, 1, 0, 1);
    case 7: return editFilterCutoff(action, &post->filterCutoffHz);
    case 8: return edit8noLast(action, &post->filterResonance, 16, 0, 255);
    default: return 0;
  }
}

static float envelopePreviewCurve(float x, uint8_t shape) {
  if (shape <= 0x80) {
    float root = sqrtf(x);
    return root + (x - root) * ((float)shape / 128.0f);
  }
  return x + (x * x - x) * ((float)(shape - 0x80) / 127.0f);
}

static void drawEnvelopePreviewSegment(Bitmap* bitmap, int x0, float start, int x1, float target, uint8_t shape) {
  int width = x1 - x0;
  if (width < 1) width = 1;
  for (int x = 0; x <= width; x++) {
    float progress = envelopePreviewCurve((float)x / width, shape);
    float level = start + (target - start) * progress;
    int y = bitmap->heightPixels - 1 - (int)(level * (bitmap->heightPixels - 2));
    if (x0 + x >= 0 && x0 + x < bitmap->widthPixels && y >= 0 && y < bitmap->heightPixels) {
      bitmap->data[y * bitmap->widthPixels + x0 + x] = 255;
    }
  }
}

void instrumentCommonDrawEnvelopePreview(uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release, uint8_t shape) {
  const int columns = 17;
  const int rows = 3;
  if (envelopePreviewBitmap && (envelopePreviewBitmap->widthPixels != columns * gfxGetCharWidth() ||
      envelopePreviewBitmap->heightPixels != rows * gfxGetCharHeight())) {
    gfxBitmapFree(envelopePreviewBitmap);
    envelopePreviewBitmap = nullptr;
  }
  if (!envelopePreviewBitmap) envelopePreviewBitmap = gfxBitmapCreate(columns, rows);
  if (!envelopePreviewBitmap) return;

  Bitmap* bitmap = envelopePreviewBitmap;
  memset(bitmap->data, 0, bitmap->widthPixels * bitmap->heightPixels);
  const int width = bitmap->widthPixels - 1;
  const int attackEnd = 3 + ((int)attack * width) / (255 * 7);
  const int decayEnd = attackEnd + 3 + ((int)decay * width) / (255 * 6);
  const int releaseStart = width - 3 - ((int)release * width) / (255 * 7);

  float sustainLevel = (float)sustain / 255.0f;
  drawEnvelopePreviewSegment(bitmap, 0, 0.0f, attackEnd, 1.0f, shape);
  drawEnvelopePreviewSegment(bitmap, attackEnd, 1.0f, decayEnd, sustainLevel, shape);
  drawEnvelopePreviewSegment(bitmap, decayEnd, sustainLevel, releaseStart, sustainLevel, shape);
  drawEnvelopePreviewSegment(bitmap, releaseStart, sustainLevel, width, 0.0f, shape);
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  const int column = 6;
  gfxDrawBitmap(bitmap, column, 15);
}

void instrumentCommonDrawLivePreview(void) {
  const auto type = chipnomadState->project.instruments[cInstrument].type;
  if (livePreviewInstrument != cInstrument || livePreviewType != type) {
    resetLivePreview();
    livePreviewInstrument = cInstrument;
    livePreviewType = type;
  }
  const PlaybackStatus* playback = chipnomadGetPlaybackStatus(chipnomadState);
  const auto now = std::chrono::steady_clock::now();
  int track = -1;
  for (int i = 0; i < chipnomadState->project.tracksCount; ++i) {
    if (playback->tracks[i].note.instrument == cInstrument &&
        chipnomadState->voiceMonitors[i].active &&
        (track < 0 || i == *pSongTrack)) track = i;
  }
  if (track < 0) {
    if (!livePreviewWasActive) return;
    // Audio monitors are briefly inactive while a render chunk is assembled.
    // Hold through those gaps and short note gaps instead of flashing static.
    const bool quiet = now - livePreviewLastActivity >= std::chrono::milliseconds(250);
    if (quiet && !playback->isPlaying) {
      resetLivePreview();
      currentScreen->fullRedraw();
      return;
    }
    if (quiet) {
      // Stay in live mode across song rests, showing silence rather than an
      // indefinitely frozen note. Resume immediately when a voice returns.
      const float silence = 0.0f;
      renderFloatPreview(livePreviewBitmap, &silence, 1);
      livePreviewRefresh = {};
    }
  } else {
    if (!livePreviewBitmap) livePreviewBitmap = gfxBitmapCreate(32, 3);
    if (!livePreviewBitmap) return;
    livePreviewLastActivity = now;
    int hz = appSettings.waveformRefreshHz;
    if (hz < 1 || hz > 60) hz = 30;
    if (track != livePreviewTrack || livePreviewRefresh == std::chrono::steady_clock::time_point() ||
        std::chrono::duration<float>(now - livePreviewRefresh).count() >= 1.0f / hz) {
      renderFloatPreview(livePreviewBitmap, chipnomadState->voiceMonitors[track].samples,
                         VOICE_MONITOR_SAMPLES);
      livePreviewRefresh = now;
      livePreviewTrack = track;
    }
  }
  // Cached bitmap pixels are transparent; clear on every draw, not only when
  // resampling, so a control's static preview cannot show through underneath.
  gfxClearRect(0, 16, 32, 3);
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxDrawBitmap(livePreviewBitmap, 0, 16);
  Instrument* instrument = &chipnomadState->project.instruments[cInstrument];
  InstrumentVoicePostSettings* post = voicePostSettings(instrument, instrument->type);
  if (post && instrument->type != InstrumentType::DrumSynth && instrument->type != InstrumentType::Sintered &&
      ((instrument->type != InstrumentType::Plaits &&
                instrument->type != InstrumentType::PlaitsAlt) ||
               instrument->chip.plaits.envelopeMode != 0)) {
    instrumentCommonDrawEnvelopePreview(post->attack, post->decay, post->sustain,
                                        post->release, post->envelopeShape);
  }
  livePreviewWasActive = 1;
}

int instrumentCommonOnEdit(int col, int row, enum CellEditAction action) {
  int handled = 0;
  if (row == 0 && col == 0) {
    // Instrument type
    InstrumentType oldType = chipnomadState->project.instruments[cInstrument].type;
    handled = editInstrumentType(action, &chipnomadState->project.instruments[cInstrument].type);
    InstrumentType newType = chipnomadState->project.instruments[cInstrument].type;

    if (oldType != newType) {
      // edit8noLast updates the displayed enum in place. Restore the old
      // value so setInstrumentType can release it and run the new type's
      // initializer instead of treating this as a no-op.
      chipnomadState->project.instruments[cInstrument].type = oldType;
      setInstrumentType(newType);
    }
  } else if (row == 0 && col == 1) {
    // Load instrument (supports .cni and .vts formats)
    fileBrowserSetup("LOAD INSTRUMENT", ".cni,.vts,.syx", appSettings.instrumentPath, onInstrumentLoaded, onInstrumentCancelled);
    screenSetup(&screenFileBrowser, 0);
  } else if (row == 0 && col == 2) {
    // Save instrument
    if (instrumentIsEmpty(&chipnomadState->project, cInstrument)) {
      screenMessage(MESSAGE_TIME, "Cannot save empty instrument");
    } else if (strlen(chipnomadState->project.instruments[cInstrument].name) == 0) {
      screenMessage(MESSAGE_TIME, "Enter instrument name");
    } else {
      char filename[32];
      getInstrumentFilename(filename, sizeof(filename));
      UserPresets destination;
      const char* folder = appSettings.instrumentPath;
      if (chipnomadState->project.instruments[cInstrument].type == InstrumentType::Monk &&
          setupUserPresetLibrary(destination, InstrumentType::Monk, false)) {
        fileCreateDirectoryRecursive(destination.rootPath().c_str());
        folder = destination.rootPath().c_str();
      }
      fileBrowserSetupFolderMode("SAVE INSTRUMENT", folder, filename, ".cni", onInstrumentSaved, onInstrumentCancelled);
      screenSetup(&screenFileBrowser, 0);
    }
  } else if (row == 1) {
    // Instrument name
    int res = editCharacter(action, chipnomadState->project.instruments[cInstrument].name, col, PROJECT_INSTRUMENT_NAME_LENGTH);
    if (res == 1) {
      isCharEdit = 1;
    } else if (res > 1) {
      handled = 1;
    }
  } else if (row == 2 && col == 0) {
    // Transpose
    handled = edit8noLast(action, &chipnomadState->project.instruments[cInstrument].transposeEnabled, 1, 0, 1);
  } else if (row == 2 && col == 1) {
    // Table tic speed
    handled = edit8noLast(action, &chipnomadState->project.instruments[cInstrument].tableSpeed, 16, 1, 255);
  } else if (row == 2 && col == 2) {
    handled = edit8noLast(action, &chipnomadState->project.instruments[cInstrument].volume, 16, 0, 255);
  } else if (row == 2 && col == 3) {
    if (action == CellEditAction::clear) {
      chipnomadState->project.instruments[cInstrument].pan = 0x80;
      handled = 1;
    } else handled = edit8noLast(action, &chipnomadState->project.instruments[cInstrument].pan, 16, 0, 255);
  }

  if (handled) projectModified = 1;
  return handled;
}

///////////////////////////////////////////////////////////////////////////////
//
// Input handling
//

static int inputScreenNavigation(int keys, int tapCount) {
  if (keys == (keyRight | keyShift)) {
    // To Table screen with the default instrument table
    screenSetup(&screenTable, cInstrument);
    return 1;
  } else if (keys == (keyLeft | keyShift)) {
    // To Phrase screen
    screenSetup(&screenPhrase, -1);
    return 1;
  } else if (keys == (keyDown | keyShift)) {
    // To Instrument Pool screen
    screenSetup(&screenInstrumentPool, cInstrument);
    return 1;
  } else if (keys == (keyUp | keyShift)) {
    // To Modulation screen
    screenSetup(&screenModulation, cInstrument);
    return 1;
  } else if (keys == (keyOpt | keyLeft)) {
    // To the previous instrument
    if (cInstrument != 0) {
      cInstrument--;
      chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
      fullRedraw();
    }
    return 1;
  } else if (keys == (keyOpt | keyRight)) {
    // To the next instrument
    if (cInstrument != PROJECT_MAX_INSTRUMENTS - 1) {
      cInstrument++;
      chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
      fullRedraw();
    }
    return 1;
  } else if (keys == (keyOpt | keyUp)) {
    // +16 instruments
    cInstrument += 16;
    if (cInstrument >= PROJECT_MAX_INSTRUMENTS) cInstrument = PROJECT_MAX_INSTRUMENTS - 1;
    chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
    fullRedraw();
    return 1;
  } else if (keys == (keyOpt | keyDown)) {
    // -16 instruments
    cInstrument -= 16;
    if (cInstrument < 0) cInstrument = 0;
    chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
    fullRedraw();
    return 1;
  } else if (keys == (keyEdit | keyPlay)) {
    // Preview instrument
    if (!instrumentIsEmpty(&chipnomadState->project, cInstrument) && !chipnomadGetPlaybackStatus(chipnomadState)->isPlaying) {
      uint8_t note = instrumentFirstNote(&chipnomadState->project, cInstrument);
      chipnomadQueuePlaybackPreviewNote(chipnomadState, *pSongTrack, note, cInstrument);
    }
    return 1;
  } else if (keys == (keyShift | keyOpt)) {
    // Copy instrument
    copyInstrument(cInstrument);
    screenMessage(MESSAGE_TIME, "Copied instrument");
    return 1;
  } else if (keys == (keyShift | keyEdit)) {
    // Paste instrument
    pasteInstrument(cInstrument);
    fullRedraw();
    return 1;
  }
  return 0;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  // Stop preview when keys are released
  if (keys == 0) {
    chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
  }

  if (isCharEdit) {
    ScreenData* screen = instrumentScreen();
    char result = charEditInput(keys, tapCount, chipnomadState->project.instruments[cInstrument].name, screen->cursorCol, PROJECT_INSTRUMENT_NAME_LENGTH);
    if (result) {
      isCharEdit = 0;
      if (screen->cursorCol < 15) screen->cursorCol++;
      fullRedraw();
    }
  } else {
    // Allow instrument-specific input handling (e.g., loop preview for AY Sample)
    ScreenData* screen = instrumentScreen();
    if (screen->onInput && screen->onInput(isKeyDown, keys, tapCount)) {
      return 1;
    }

    if (instrumentTypePopupInput(isKeyDown, keys, screen)) return 1;

    if (inputScreenNavigation(keys, tapCount)) return 1;

    if (screenInput(screen, isKeyDown, keys, tapCount)) return 1;
  }
  return 0;
}

static ScreenPlaybackLevel getPlaybackLevel(void) {
  return ScreenPlaybackLevel::phrase;
}

#ifdef DESKTOP_BUILD
int instrumentKeyJazzTextField(KeyJazzTextField* field) {
  ScreenData* screen = instrumentScreen();
  field->screen = screen;
  field->row = 1;
  field->popupOpen = isCharEdit;
  field->marksProjectModified = 1;
  field->str = NULL;
  if (screen->cursorRow == 1 && chipnomadState->project.instruments[cInstrument].type != InstrumentType::Monk) { field->str = chipnomadState->project.instruments[cInstrument].name; field->maxLen = PROJECT_INSTRUMENT_NAME_LENGTH; }
  return 1;
}

// Key jazz on the Instrument screen: with key jazz on (Esc, the same toggle as
// the Name field's text entry), the note keys audition the instrument on the
// current track, like an external MIDI keyboard does, without entering any
// note. On the Name field the keys type text instead. Last-note priority:
// releasing a key falls back to the previous key still held, and the sound is
// released when none is left.
static int keyJazzBaseNote = 48;
static int keyJazzHeld[32];
static int keyJazzHeldCount = 0;

static int keyJazzClamp(int note) {
  int maxNote = chipnomadState->project.pitchTable.length - 1;
  return note < 0 ? 0 : note > maxNote ? maxNote : note;
}

static void keyJazzSound(int offset) {
  PlaybackMode mode = chipnomadGetPlaybackStatus(chipnomadState)->tracks[*pSongTrack].mode;
  if (mode != PlaybackMode::stopped && mode != PlaybackMode::phraseRow) return; // the song is playing this track
  if (instrumentIsEmpty(&chipnomadState->project, cInstrument)) return;
  chipnomadQueuePlaybackPreviewNote(chipnomadState, *pSongTrack, (uint8_t)keyJazzClamp(keyJazzBaseNote + offset), cInstrument);
}

static void keyJazzReleaseAll(void) {
  if (keyJazzHeldCount) chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
  keyJazzHeldCount = 0;
}

int instrumentKeyJazzHandleRawKey(InputCode input, int isDown) {
  if (input.deviceType != InputDeviceType::keyboard) return 0;

  if (inputIsKeyJazzToggle(input)) {
    // The text handler flips the state right after this: start on the
    // octave the instrument's first note is in, and let go of anything held.
    if (isDown) {
      if (!keyJazzTextIsEnabled()) {
        uint16_t octaveSize = chipnomadState->project.pitchTable.octaveSize;
        int first = instrumentFirstNote(&chipnomadState->project, cInstrument);
        keyJazzBaseNote = octaveSize > 0 ? (first / octaveSize) * octaveSize : first;
      }
      keyJazzReleaseAll();
    }
    return 0;
  }

  if (!keyJazzTextIsEnabled()) return 0;

  KeyJazzTextField field;
  instrumentKeyJazzTextField(&field);
  if (field.popupOpen || field.str) {   // popup open, or typing in the Name field
    keyJazzReleaseAll();
    return 0;
  }
  // Ctrl shortcuts only bypass key-down: a note held before Ctrl was pressed
  // must still be released by its key-up, or the preview would stay on.
  if (isDown && inputIsCtrlHeld()) return 0;

  int octaveDelta = inputKeyJazzOctaveDelta(input);
  if (octaveDelta != 0) {
    if (isDown) {
      uint16_t octaveSize = chipnomadState->project.pitchTable.octaveSize;
      keyJazzBaseNote = keyJazzClamp(keyJazzBaseNote + octaveDelta * (int)octaveSize);
      screenMessage(MESSAGE_TIME, "KEY JAZZ octave: %s", chipnomadState->project.pitchTable.noteNames[keyJazzBaseNote]);
    }
    return 1;
  }

  int offset = inputKeyJazzNoteOffset(input);
  if (offset < 0) return 0;   // not a note key: normal input (arrows, Play...)

  int held = -1;
  for (int i = 0; i < keyJazzHeldCount; i++) if (keyJazzHeld[i] == offset) held = i;
  if (isDown) {
    if (held >= 0) return 1;  // key repeat
    if (instrumentIsEmpty(&chipnomadState->project, cInstrument)) {
      screenMessage(MESSAGE_TIME, "Instrument is empty");
      return 1;
    }
    if (keyJazzHeldCount < 32) keyJazzHeld[keyJazzHeldCount++] = offset;
    keyJazzSound(offset);
  } else if (held >= 0) {
    for (int i = held; i < keyJazzHeldCount - 1; i++) keyJazzHeld[i] = keyJazzHeld[i + 1];
    keyJazzHeldCount--;
    if (keyJazzHeldCount) keyJazzSound(keyJazzHeld[keyJazzHeldCount - 1]);
    else chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
  }
  return 1;
}
#endif

const AppScreen screenInstrument = {
  .init = init,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = getPlaybackLevel
};
