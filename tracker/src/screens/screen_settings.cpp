#include "screen_settings.h"
#include "screen_midi.h"
#include "common.h"
#include "app.h"
#include "corelib_gfx.h"
#include "corelib_mainloop.h"
#include "screens.h"
#include "support_report.h"
#include "corelib_file.h"
#include <string.h>
#include <stdio.h>

// Android adds a "Working folder" row (row 7) before Quit.
#ifdef ANDROID_BUILD
#define SETTINGS_WORKING_FOLDER_ROW 7
#define SETTINGS_QUIT_ROW 8
#define SETTINGS_TOTAL_ROWS 9
static char workingFolderStatus[256];
static void refreshWorkingFolderStatus(void) {
  if (fileGetWorkingFolderStatus(workingFolderStatus, sizeof(workingFolderStatus)) != 0)
    snprintf(workingFolderStatus, sizeof(workingFolderStatus), "Unavailable");
}
#else
#define SETTINGS_QUIT_ROW 7
#define SETTINGS_TOTAL_ROWS 8
#endif

static int columnCount(int) { return 1; }
static void setup(int) {
#ifdef ANDROID_BUILD
  refreshWorkingFolderStatus();
  screenMessage(MESSAGE_TIME, "%s", workingFolderStatus);
#endif
}
static void draw(void) {}
static void drawStatic(void) { gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0, 0, "SETTINGS"); }
static void drawCursor(int, int row) {
  if (row < 6) { static const int widths[] = {8, 5, 4, 6, 5, 8}; gfxCursor(0, 2 + row, widths[row]); }
  else if (row == 6) gfxCursor(0, 10, 14);
#ifdef ANDROID_BUILD
  else if (row == SETTINGS_WORKING_FOLDER_ROW) gfxCursor(0, 12, 14);
#endif
  else if (row == SETTINGS_QUIT_ROW) gfxCursor(0, 18, 19);
}
static void noHeader(int, CellState) {}
static void drawField(int, int row, CellState state) {
  const ColorScheme cs = appSettings.colorScheme;
  gfxSetFgColor(cs.textDefault);
  if (row < 6) { static const char* labels[] = {"Controls", "Audio", "MIDI", "Synths", "Mixer", "Graphics"}; gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(0, 2 + row, labels[row]); }
  else if (row == 6) { gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(0, 10, "Support report"); }
#ifdef ANDROID_BUILD
  else if (row == SETTINGS_WORKING_FOLDER_ROW) {
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxPrint(0, 12, "Working folder");
    gfxPrint(23, 12, strncmp(workingFolderStatus, "Folder:", 7) == 0 ? "FOLDER " :
      strncmp(workingFolderStatus, "Shared", 6) == 0 ? "SHARED " :
      strncmp(workingFolderStatus, "Permission", 10) == 0 ? "ALLOW  " :
      strcmp(workingFolderStatus, "Unavailable") == 0 ? "N/A    " : "PRIVATE");
  }
#endif
  else if (row == SETTINGS_QUIT_ROW) { gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(0, 18, "Quit ChooChooTracker"); }
}
static int onEdit(int, int row, CellEditAction action) {
#ifdef ANDROID_BUILD
  if (row == SETTINGS_WORKING_FOLDER_ROW && action == CellEditAction::clear) {
    if (strcmp(workingFolderStatus, "Unavailable") == 0) { screenMessage(MESSAGE_TIME, "Not available in this build"); return 1; }
    fileClearWorkingFolder();
    return 1;
  }
#endif
  if (action != CellEditAction::tap) return 0;
  if (row == 0) screenSetup(&screenControlsSettings, 0);
  else if (row == 1) screenSetup(&screenAudioSettings, 0);
  else if (row == 2) screenSetup(&screenMidi, 0);
  else if (row == 3) screenSetup(&screenSynthSettings, 0);
  else if (row == 4) screenSetup(&screenMixerSettings, 0);
  else if (row == 5) screenSetup(&screenGraphicsSettings, 0);
  else if (row == 6) {
    char path[PATH_LENGTH + 32];
    if (supportReportSaveDefault("Settings", path, sizeof(path)) == 0)
      screenMessage(MESSAGE_TIME, "Support report saved");
    else
      screenMessage(MESSAGE_TIME_ERROR, "Support report failed");
    return 1;
  }
#ifdef ANDROID_BUILD
  else if (row == SETTINGS_WORKING_FOLDER_ROW) {
    if (strcmp(workingFolderStatus, "Unavailable") == 0) screenMessage(MESSAGE_TIME, "Not available in this build");
    else filePickWorkingFolder();
    return 0;
  }
#endif
  else if (row == SETTINGS_QUIT_ROW) { mainLoopTriggerQuit(); return 1; }
  return 0;
}
static ScreenData data = {
  .rows = SETTINGS_TOTAL_ROWS, .cursorRow = 0, .cursorCol = 0, .topRow = 0, .selectMode = -1,
  .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none, .getColumnCount = columnCount,
  .drawStatic = drawStatic, .drawCursor = drawCursor, .drawSelection = NULL,
  .drawRowHeader = noHeader, .drawColHeader = noHeader, .drawField = drawField,
  .onEdit = onEdit, .onInput = NULL, .onRawInput = NULL, .isCellValid = NULL, .getLoopRange = NULL,
};
static void fullRedraw(void) { screenFullRedraw(&data); }
static int onInput(int isKeyDown, int keys, int taps) { if (keys == keyOpt || keys == (keyUp | keyShift)) { screenSetup(&screenSong, 0); return 1; } return screenInput(&data, isKeyDown, keys, taps); }
static ScreenPlaybackLevel playbackLevel(void) { return ScreenPlaybackLevel::song; }
const AppScreen screenSettings = {NULL, setup, fullRedraw, draw, onInput, playbackLevel};
