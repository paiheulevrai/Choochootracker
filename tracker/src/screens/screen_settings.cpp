#include "screen_settings.h"
#include "screen_color_theme.h"
#include "screen_keymapping.h"
#include "file_browser.h"
#include "common.h"
#include "corelib_gfx.h"
#include "corelib_mainloop.h"
#include "corelib_font.h"
#include "corelib_file.h"
#include "screens.h"
#include <string.h>

// Forward declarations
static int settingsColumnCount(int row);
static void settingsDrawStatic(void);
static void settingsDrawCursor(int col, int row);
static void settingsDrawRowHeader(int row, CellState state);
static void settingsDrawColHeader(int col, CellState state);
static void settingsDrawField(int col, int row, CellState state);
static int settingsOnEdit(int col, int row, CellEditAction action);

static ScreenData screenSettingsData = {
  .rows = 9,
  .cursorRow = 0,
  .cursorCol = 0,
  .topRow = 0,
  .selectMode = -1,
  .selectStartRow = 0,
  .selectStartCol = 0,
  .selectAnchorRow = 0,
  .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = settingsColumnCount,
  .drawStatic = settingsDrawStatic,
  .drawCursor = settingsDrawCursor,
  .drawSelection = NULL,
  .drawRowHeader = settingsDrawRowHeader,
  .drawColHeader = settingsDrawColHeader,
  .drawField = settingsDrawField,
  .onEdit = settingsOnEdit,
  .onInput = NULL,
  .onRawInput = NULL,
  .isCellValid = NULL,
  .getLoopRange = NULL,
};

static void setup(int input) {
}

static void fontLoadCallback(const char* path) {
  Font* font = fontLoad(path);
  if (font) {
    fontSetCurrent(font);
    gfxReloadFont();
    strncpy(appSettings.fontPath, path, PATH_LENGTH);
    appSettings.fontPath[PATH_LENGTH] = 0;

    // Extract folder path
    const char* lastSeparator = strrchr(path, PATH_SEPARATOR);
    if (lastSeparator) {
      int pathLen = lastSeparator - path;
      if (pathLen > 0 && pathLen < PATH_LENGTH) {
        strncpy(appSettings.fontFolderPath, path, pathLen);
        appSettings.fontFolderPath[pathLen] = '\0';
      }
    }

    screenMessage(MESSAGE_TIME, "Loaded: %s", font->name);
  } else {
    screenMessage(MESSAGE_TIME, "Failed to load font");
  }
  screenSetup(&screenSettings, 0);
}

static void fontCancelCallback(void) {
  screenSetup(&screenSettings, 0);
}

static void fullRedraw(void) {
  screenFullRedraw(&screenSettingsData);
}

static void draw(void) {
}

int settingsColumnCount(int row) {
  return 1;
}

void settingsDrawStatic(void) {
  const ColorScheme cs = appSettings.colorScheme;

  gfxSetFgColor(cs.textTitles);
  gfxPrint(0, 0, "SETTINGS");
}

void settingsDrawCursor(int col, int row) {
  if ((row == 0 || row == 1) && col == 0) {
    gfxCursor(23, 2 + row, 3);
  } else if (row == 2 && col == 0) {
    gfxCursor(23, 4, 4);
  } else if (row == 3 && col == 0) {
    gfxCursor(23, 5, 6);
  } else if (row == 4 && col == 0) {
    gfxCursor(23, 6, 3);
  } else if (row == 5 && col == 0) {
    gfxCursor(0, 7, 11);
  } else if (row == 6 && col == 0) {
    gfxCursor(0, 8, 9);
  } else if (row == 7 && col == 0) {
    gfxCursor(0, 9, 16);
  } else if (row == 8 && col == 0) {
    gfxCursor(0, 17, 14);
  }
}

void settingsDrawRowHeader(int row, CellState state) {
}

void settingsDrawColHeader(int col, CellState state) {
}

void settingsDrawField(int col, int row, CellState state) {
  if (row == 0 && col == 0) {
    gfxSetFgColor(appSettings.colorScheme.textDefault);
    gfxPrint(0, 2, "Repeat delay");
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    gfxPrintf(23, 2, "%03d", appSettings.keyRepeatDelay);
  } else if (row == 1 && col == 0) {
    gfxSetFgColor(appSettings.colorScheme.textDefault);
    gfxPrint(0, 3, "Repeat speed");
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    gfxPrintf(23, 3, "%03d", appSettings.keyRepeatSpeed);
  } else if (row == 2 && col == 0) {
    gfxSetFgColor(appSettings.colorScheme.textDefault);
    gfxPrint(0, 4, "Mix volume");
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    int mixVolumePercent = (int)(appSettings.mixVolume * 100.0f + 0.5f);
    gfxPrintf(23, 4, "%03d%%", mixVolumePercent);
  } else if (row == 3 && col == 0) {
    gfxSetFgColor(appSettings.colorScheme.textDefault);
    gfxPrint(0, 5, "AY Quality");
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    const char* qualityNames[] = {"LOW   ", "MEDIUM", "HIGH  ", "BEST  "};
    gfxPrint(23, 5, qualityNames[appSettings.quality]);
  } else if (row == 4 && col == 0) {
    gfxSetFgColor(appSettings.colorScheme.textDefault);
    gfxPrint(0, 6, "AY Sample dithering");
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    gfxPrint(23, 6, appSettings.aySampleDithering ? "ON " : "OFF");
  } else if (row == 5 && col == 0) {
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    gfxPrint(0, 7, "Key mapping");
  } else if (row == 6 && col == 0) {
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    gfxPrint(0, 8, "Load font");
  } else if (row == 7 && col == 0) {
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    gfxPrint(0, 9, "Edit color theme");
  } else if (row == 8 && col == 0) {
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    gfxPrint(0, 17, "Quit ChooChooTracker");
  }
}

int settingsOnEdit(int col, int row, CellEditAction action) {
  if (row == 0 && col == 0) {
    uint8_t value = appSettings.keyRepeatDelay;
    int handled = edit8noLast(action, &value, 4, 4, 30);
    if (handled) appSettings.keyRepeatDelay = value;
    return handled;
  } else if (row == 1 && col == 0) {
    uint8_t value = appSettings.keyRepeatSpeed;
    int handled = edit8noLast(action, &value, 2, 1, 12);
    if (handled) appSettings.keyRepeatSpeed = value;
    return handled;
  } else if (row == 2 && col == 0) {
    // Mix volume (1-100%)
    int mixVolumePercent = (int)(appSettings.mixVolume * 100.0f + 0.5f);
    uint8_t mixVolumePercentU8 = (uint8_t)mixVolumePercent;
    int handled = edit8noLast(action, &mixVolumePercentU8, 10, 1, 100);
    if (handled) {
      appSettings.mixVolume = (float)mixVolumePercentU8 / 100.0f;
      if (chipnomadState) {
        chipnomadState->mixVolume = appSettings.mixVolume;
      }
    }
    return handled;
  } else if (row == 3 && col == 0) {
    // Quality (0-3)
    int handled = edit8noLast(action, (uint8_t*)&appSettings.quality, 1, 0, 3);
    if (handled) {
      chipnomadSetQuality(chipnomadState, (ChipNomadQuality)appSettings.quality);
    }
    return handled;
  } else if (row == 4 && col == 0) {
    // Sample dithering (0/1)
    int handled = edit8noLast(action, (uint8_t*)&appSettings.aySampleDithering, 1, 0, 1);
    if (handled && chipnomadState) {
      chipnomadState->aySampleDithering = appSettings.aySampleDithering;
    }
    return handled;
  } else if (row == 5 && col == 0 && action == CellEditAction::tap) {
    screenSetup(&screenKeyMapping, 0);
    return 0;
  } else if (row == 6 && col == 0 && action == CellEditAction::tap) {
    fileBrowserSetup("LOAD FONT", ".cnfont", appSettings.fontFolderPath,
      (void (*)(const char*))fontLoadCallback,
      (void (*)(void))fontCancelCallback);
    screenSetup(&screenFileBrowser, 0);
    return 0;
  } else if (row == 7 && col == 0 && action == CellEditAction::tap) {
    screenSetup(&screenColorTheme, 0);
    return 0;
  } else if (row == 8 && col == 0 && action == CellEditAction::tap) {
    // Trigger exit event
    mainLoopTriggerQuit();
    return 1;
  }
  return 0;
}

static int inputScreenNavigation(int keys, int tapCount) {
  if (keys == (keyUp | keyShift)) {
    screenSetup(&screenSong, 0);
    return 1;
  }
  return 0;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (inputScreenNavigation(keys, tapCount)) return 1;
  return screenInput(&screenSettingsData, isKeyDown, keys, tapCount);
}

static ScreenPlaybackLevel getPlaybackLevel(void) {
  return ScreenPlaybackLevel::song;
}

const AppScreen screenSettings = {
  .init = NULL,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = getPlaybackLevel,
};
