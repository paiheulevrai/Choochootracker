#include "screens.h"
#include "common.h"
#include "corelib_gfx.h"
#include "corelib/corelib_file.h"
#include "utils.h"
#include "chipnomad_lib.h"
#include "project_utils.h"
#include "screen_instrument.h"
#include "copy_paste.h"
#include "file_browser.h"
#include "import_vts.h"
#include "selection_popup.h"
#include <string.h>
#include <strings.h>

extern const AppScreen screenInstrumentPool;

int cInstrument = 0;
static int isCharEdit = 0;
static int typeButtonDown = 0;

static const SelectionItem instrumentTypeChip[] = {
  {"AY Classic", (int)InstrumentType::AY1, NULL, 0},
  {"AY Plus", (int)InstrumentType::AY2, NULL, 0},
  {"AY Sample", (int)InstrumentType::AYSample, NULL, 0},
};
static const SelectionItem instrumentTypeSynth[] = {
  {"Braids", (int)InstrumentType::Braids, NULL, 0},
  {"PCM Sample", (int)InstrumentType::Sample, NULL, 0},
  {"Plaits", (int)InstrumentType::Plaits, NULL, 0},
  {"Plaits-Alt", (int)InstrumentType::PlaitsAlt, NULL, 0},
};
static const SelectionItem instrumentTypeCategories[] = {
  {"CHIP", -1, instrumentTypeChip, 3},
  {"SYNTH", -1, instrumentTypeSynth, 4},
};

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

static void onInstrumentLoaded(const char* path) {
  int result = 1;
  const char* ext = strrchr(path, '.');
  int formatHandled = 0;

  if (ext != NULL) {
    for (size_t i = 0; i < sizeof(importFormats) / sizeof(importFormats[0]); i++) {
      if (strcasecmp(ext, importFormats[i].extension) == 0) {
        result = importFormats[i].handler(path, cInstrument);
        if (result == 0) {
          screenMessage(MESSAGE_TIME, importFormats[i].successMsg);
        } else {
          screenMessage(MESSAGE_TIME, importFormats[i].errorMsg);
        }
        formatHandled = 1;
        break;
      }
    }
  }

  if (!formatHandled) {
    result = instrumentLoad(&chipnomadState->project, path, cInstrument);
    if (result == 0) {
      screenMessage(MESSAGE_TIME, "Instrument loaded");
    } else {
      screenMessage(MESSAGE_TIME, "%s", projectFileError);
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

  if (instrumentSave(&chipnomadState->project, fullPath, cInstrument) == 0) {
    strncpy(appSettings.instrumentPath, folderPath, PATH_LENGTH);
    appSettings.instrumentPath[PATH_LENGTH - 1] = 0;
  }
  screenSetup(&screenInstrument, cInstrument);
}

static void onInstrumentCancelled(void) {
  screenSetup(&screenInstrument, cInstrument);
}

static void drawRowHeader(int row, CellState state);
static void drawColHeader(int col, CellState state);

static void setInstrumentType(InstrumentType newType) {
  Instrument* instrument = &chipnomadState->project.instruments[cInstrument];
  InstrumentType oldType = instrument->type;
  if (oldType == newType) {
    screenSetup(&screenInstrument, cInstrument);
    return;
  }
  getInstrumentFunctions(oldType).free(instrument);
  instrument->type = newType;
  getInstrumentFunctions(newType).init(instrument);
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
  if (isKeyDown && keys == keyEdit) {
    typeButtonDown = 1;
    return 1;
  }
  if (isKeyDown && (keys == (keyEdit | keyLeft) || keys == (keyEdit | keyRight))) {
    typeButtonDown = 0;
    return 0;
  }
  if (!isKeyDown && keys == 0 && typeButtonDown) {
    typeButtonDown = 0;
    selectionPopupSetup("INSTRUMENT TYPE", instrumentTypeCategories, 2,
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
  ScreenData* data = &screenInstrumentNone;
  switch (chipnomadState->project.instruments[cInstrument].type) {
    case InstrumentType::AY1:         data = &screenInstrumentAY; break;
    case InstrumentType::AY2:         data = &screenInstrumentAY2; break;
    case InstrumentType::AYSample:    data = &screenInstrumentAYSample; break;
    case InstrumentType::Braids:      data = &screenInstrumentBraids; break;
    case InstrumentType::Sample:      data = &screenInstrumentSample; break;
    case InstrumentType::Plaits:      data = &screenInstrumentPlaits; break;
    case InstrumentType::PlaitsAlt:   data = &screenInstrumentPlaits; break;
    default: break;
  }
  data->drawRowHeader = drawRowHeader;
  data->drawColHeader = drawColHeader;
  return data;
}

static void init(void) {
  isCharEdit = 0;
  typeButtonDown = 0;
  screenInstrumentNone.cursorRow = 0;
  screenInstrumentNone.cursorCol = 0;
}

static void setup(int input) {
  isCharEdit = 0;
  if (input != -1) {
    cInstrument = input;
  }
}

static void fullRedraw(void) {
  ScreenData* screen = instrumentScreen();
  screenFullRedraw(screen);
}

static void draw(void) {
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
    return 3; // Transpose, table speed, volume
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
  gfxPrint(15, 4, "Tbl.Tic");
  gfxPrint(28, 4, "Vol");
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
    gfxCursor(23, 4, 2);
  } else if (row == 2 && col == 2) {
    gfxCursor(32, 4, 2);
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
    gfxPrint(23, 4, byteToHex(chipnomadState->project.instruments[cInstrument].tableSpeed));
  } else if (row == 2 && col == 2) {
    gfxPrint(32, 4, byteToHex(chipnomadState->project.instruments[cInstrument].volume));
  }
}

int instrumentCommonOnEdit(int col, int row, enum CellEditAction action) {
  int handled = 0;
  if (row == 0 && col == 0) {
    // Instrument type
    InstrumentType oldType = chipnomadState->project.instruments[cInstrument].type;
    handled = edit8noLast(action,
      reinterpret_cast<uint8_t*>(&chipnomadState->project.instruments[cInstrument].type),
      1, 0, static_cast<uint8_t>(InstrumentType::totalCount) - 1);
    InstrumentType newType = chipnomadState->project.instruments[cInstrument].type;

    if (oldType != newType) setInstrumentType(newType);
  } else if (row == 0 && col == 1) {
    // Load instrument (supports .cni and .vts formats)
    fileBrowserSetup("LOAD INSTRUMENT", ".cni,.vts", appSettings.instrumentPath, onInstrumentLoaded, onInstrumentCancelled);
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
      fileBrowserSetupFolderMode("SAVE INSTRUMENT", appSettings.instrumentPath, filename, ".cni", onInstrumentSaved, onInstrumentCancelled);
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
      playbackStopPreview(&chipnomadState->playbackState, *pSongTrack);
      fullRedraw();
    }
    return 1;
  } else if (keys == (keyOpt | keyRight)) {
    // To the next instrument
    if (cInstrument != PROJECT_MAX_INSTRUMENTS - 1) {
      cInstrument++;
      playbackStopPreview(&chipnomadState->playbackState, *pSongTrack);
      fullRedraw();
    }
    return 1;
  } else if (keys == (keyOpt | keyUp)) {
    // +16 instruments
    cInstrument += 16;
    if (cInstrument >= PROJECT_MAX_INSTRUMENTS) cInstrument = PROJECT_MAX_INSTRUMENTS - 1;
    playbackStopPreview(&chipnomadState->playbackState, *pSongTrack);
    fullRedraw();
    return 1;
  } else if (keys == (keyOpt | keyDown)) {
    // -16 instruments
    cInstrument -= 16;
    if (cInstrument < 0) cInstrument = 0;
    playbackStopPreview(&chipnomadState->playbackState, *pSongTrack);
    fullRedraw();
    return 1;
  } else if (keys == (keyEdit | keyPlay)) {
    // Preview instrument
    if (!instrumentIsEmpty(&chipnomadState->project, cInstrument) && !playbackIsPlaying(&chipnomadState->playbackState)) {
      uint8_t note = instrumentFirstNote(&chipnomadState->project, cInstrument);
      playbackPreviewNote(&chipnomadState->playbackState, *pSongTrack, note, cInstrument);
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
    playbackStopPreview(&chipnomadState->playbackState, *pSongTrack);
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

const AppScreen screenInstrument = {
  .init = init,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = getPlaybackLevel
};
