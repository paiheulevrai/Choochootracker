// UI/audio boundaries for exercising the real app and Settings event handlers.
#include "app_ui_mock.h"
#include "audio_manager.h"
#include "corelib_gfx.h"
#include "file_browser.h"
#include "screen_quick_help.h"

int mockLastInputKeys;
static int ignoreInput(int, int keys, int) { mockLastInputKeys = keys; return 1; }
static void noOp(void) {}
static int startAudio(int, int) { return 0; }
AudioManager audioManager = {startAudio, noOp, noOp, nullptr, nullptr, noOp};
const AppScreen screenSong = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenTitle = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenPhrase = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenInstrument = {nullptr, nullptr, noOp, noOp, ignoreInput};
int cInstrument = 0;
const AppScreen screenKeyMapping = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenFileBrowser = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenColorTheme = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenTable = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenInstrumentPool = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenModulation = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenMixer = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenGroove = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenProject = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenAYWavetable = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen* currentScreen = &screenSong;
ScreenData* mockScreenData;
const char* mockBrowserTitle;
const char* mockBrowserExtension;

void screensInitAll(void) {
  static int row = 0, track = 0, chainRow = 0;
  pSongRow = &row;
  pSongTrack = &track;
  pChainRow = &chainRow;
}
void screenSetup(const AppScreen* screen, int) { currentScreen = screen; }
void screenDraw(void) {}
void screenMessage(int, const char*, ...) {}
void drawScreenMap(void) {}
ScreenPlaybackLevel screenGetPlaybackLevel(const AppScreen*) { return ScreenPlaybackLevel::none; }
LoopRange screenGetLoopRange(const AppScreen*) { return {}; }
void screenFullRedraw(ScreenData* screen) { mockScreenData = screen; gfxClear(); }
int screenInput(ScreenData* screen, int, int, int) { mockScreenData = screen; return 1; }
void setCellColor(CellState, int, int) {}
int isSingleColumnSelection(ScreenData* screen) {
  int startCol, startRow, endCol, endRow;
  getSelectionBounds(screen, &startCol, &startRow, &endCol, &endRow);
  return startCol == endCol;
}
int screenTouchTap(int, int) { return 0; }
TouchAdjustResult screenTouchAdjust(int, int) { return touchAdjustNone; }
void screenQuickHelpOpen(const AppScreen*) {}
void fileBrowserSetup(const char* title, const char* extension, const char*,
                      void (*)(const char*), void (*)(void)) {
  mockBrowserTitle = title;
  mockBrowserExtension = extension;
}
