// UI/audio boundaries for exercising the real app and Settings event handlers.
#include "app_ui_mock.h"
#include "audio_manager.h"
#include "file_browser.h"
#include "screen_quick_help.h"

static int ignoreInput(int, int, int) { return 1; }
static void noOp(void) {}
static int startAudio(int, int) { return 0; }
AudioManager audioManager = {startAudio, noOp, noOp, nullptr, nullptr, noOp};
const AppScreen screenSong = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenTitle = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenPhrase = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenKeyMapping = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenFileBrowser = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenColorTheme = {nullptr, nullptr, noOp, noOp, ignoreInput};
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
void screenFullRedraw(ScreenData* screen) { mockScreenData = screen; }
int screenInput(ScreenData* screen, int, int, int) { mockScreenData = screen; return 1; }
int screenTouchTap(int, int) { return 0; }
TouchAdjustResult screenTouchAdjust(int, int) { return touchAdjustNone; }
void screenQuickHelpOpen(const AppScreen*) {}
void fileBrowserSetup(const char* title, const char* extension, const char*,
                      void (*)(const char*), void (*)(void)) {
  mockBrowserTitle = title;
  mockBrowserExtension = extension;
}
