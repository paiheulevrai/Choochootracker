#include "screen_view_convert.h"
#include "screens.h"
#include "screen_project.h"
#include "pattern_utils.h"
#include "project_utils.h"
#include "corelib_gfx.h"
#include "corelib_file.h"
#include "audio_manager.h"
#include "file_browser.h"
#include "export_path.h"
#include "common.h"
#include <string.h>

// View mode conversion screen: warns the user, then converts the whole song
// structure between classic and simplified view. Heavy lifting happens in
// pattern_utils (convertChainsToPatterns / convertPatternsToChains); this
// screen only drives the UI flow:
//
//   option 0 "Save and proceed"      — save first (file browser when the
//                                      project has no filename yet), then
//                                      convert
//   option 1 "Proceed without saving" — convert immediately
//   option 2 "Cancel"                — back to the View window
//
// While the conversion runs the helper row shows "Converting...", after
// completion "Converted!". Errors (slots exhausted) show an error message
// and leave the project untouched — the engine only flips viewMode on
// success, so the user can re-save or cancel safely.

#define CONVERT_OPTION_COUNT (3)

static const AppScreen* returnScreen = &screenViewSettings;
static ViewMode targetMode = ViewMode::simplified;
static int selected = 0;
static int converting = 0;  // 1 while the conversion runs (helper row text)
static int converted = 0;   // 1 after a successful conversion

static const char* optionNames[CONVERT_OPTION_COUNT] = {
  "Save and proceed",
  "Proceed without saving",
  "Cancel",
};

// Forward declarations (definitions live further down; the conversion flow
// and the save flow reference each other).
static void convertNow(void);
static void fullRedraw(void);

static void convertNow(void) {
  Project* p = chipnomadState ? &chipnomadState->project : NULL;
  if (!p) {
    screenSetup(returnScreen, 0);
    return;
  }
  if (p->viewMode == targetMode) {
    // Nothing to do (target already active).
    screenSetup(returnScreen, 0);
    return;
  }

  converting = 1;
  converted = 0;
  // Redraw with the "Converting..." helper text before the blocking work.
  fullRedraw();
  gfxUpdateScreen();

  audioManager.pause();
  int result = (targetMode == ViewMode::simplified)
    ? convertChainsToPatterns(p)
    : convertPatternsToChains(p);
  audioManager.resume();

  converting = 0;
  if (result == 0) {
    converted = 1;
    projectModified = 1;
    chipnomadQueueProjectRefresh(chipnomadState);
    // Show "Converted!" briefly, then return to the View window.
    fullRedraw();
    gfxUpdateScreen();
    screenSetup(returnScreen, 0);
  } else {
    // Slots exhausted: the project is in an indeterminate state per the
    // engine contract. Tell the user plainly; viewMode was not flipped.
    screenMessage(MESSAGE_TIME_ERROR, "Conversion failed: slots full");
    screenSetup(returnScreen, 0);
  }
}

// ── Save-and-proceed path ───────────────────────────────────────────────────

// File-browser callbacks for the save-first flow.
static void onSaveProceedSaved(const char* folderPath) {
  char fullPath[2048];
  snprintf(fullPath, sizeof(fullPath), "%s%s%s.cct", folderPath,
    PATH_SEPARATOR_STR, appSettings.projectFilename);

  if (projectSave(&chipnomadState->project, fullPath) == 0) {
    projectModified = 0;
    strncpy(appSettings.projectPath, folderPath, PATH_LENGTH);
    appSettings.projectPath[PATH_LENGTH] = 0;
    exportSyncFolderWithProjectName();
    settingsSave();
    convertNow();
  } else {
    screenMessage(MESSAGE_TIME_ERROR, "%s", projectFileError);
    screenSetup(returnScreen, 0);
  }
}

static void onSaveProceedCancelled(void) {
  // Save cancelled → no conversion (user decision: cancel means cancel).
  screenSetup(returnScreen, 0);
}

static void saveAndProceed(void) {
  if (strlen(appSettings.projectFilename) == 0) {
    // Never saved: force save-as first (user decision).
    fileBrowserSetupFolderMode("SAVE PROJECT", appSettings.projectPath,
      appSettings.projectFilename, ".cct", onSaveProceedSaved,
      onSaveProceedCancelled);
    screenSetup(&screenFileBrowser, 0);
  } else {
    // Known location: save silently, then convert.
    char fullPath[2048];
    snprintf(fullPath, sizeof(fullPath), "%s%s%s.cct",
      appSettings.projectPath, PATH_SEPARATOR_STR, appSettings.projectFilename);
    if (projectSave(&chipnomadState->project, fullPath) == 0) {
      projectModified = 0;
      exportSyncFolderWithProjectName();
      settingsSave();
      convertNow();
    } else {
      screenMessage(MESSAGE_TIME_ERROR, "%s", projectFileError);
      screenSetup(returnScreen, 0);
    }
  }
}

// ── Screen implementation ───────────────────────────────────────────────────

static void fullRedraw(void) {
  gfxSetBgColor(appSettings.colorScheme.background);
  gfxClear();

  const ColorScheme cs = appSettings.colorScheme;

  gfxSetFgColor(cs.textTitles);
  gfxPrint(0, 1, "CONVERT VIEW");

  gfxSetFgColor(cs.warning);
  gfxPrint(0, 3, "Warning: converting your project view");
  gfxPrint(0, 4, "might damage your song structure!");

  for (int i = 0; i < CONVERT_OPTION_COUNT; i++) {
    gfxSetFgColor(i == selected ? cs.textValue : cs.textDefault);
    gfxPrintf(2, 7 + i, "%c %s", i == selected ? '>' : ' ', optionNames[i]);
  }

  // Helper row (message line).
  gfxSetFgColor(cs.textInfo);
  if (converting) {
    gfxPrint(0, 19, "Converting...");
  } else if (converted) {
    gfxPrint(0, 19, "Converted!");
  }
}

static void setup(int input) {
  (void)input;
  selected = 0;
  fullRedraw();
}

static void draw(void) {
  // Static screen: the "Converting..." text is drawn before the blocking
  // conversion and "Converted!" right after; nothing animates.
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  (void)tapCount;
  if (!isKeyDown) return 1;

  if (keys == keyUp && selected > 0) {
    selected--;
    fullRedraw();
    return 1;
  } else if (keys == keyDown && selected < CONVERT_OPTION_COUNT - 1) {
    selected++;
    fullRedraw();
    return 1;
  } else if (keys == keyEdit || keys == keyPlay) {
    if (selected == 0) saveAndProceed();
    else if (selected == 1) convertNow();
    else screenSetup(returnScreen, 0);
    return 1;
  } else if (keys == keyOpt) {
    screenSetup(returnScreen, 0);
    return 1;
  }
  return 1;  // Swallow everything else: no playback shortcuts here.
}

const AppScreen screenViewConvert = {
  .init = NULL,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = NULL,
};

void screenViewConvertSetup(const AppScreen* fromScreen, ViewMode target) {
  returnScreen = fromScreen ? fromScreen : &screenViewSettings;
  targetMode = target;
  selected = 0;
  converting = 0;
  converted = 0;
  screenSetup(&screenViewConvert, 0);
}
