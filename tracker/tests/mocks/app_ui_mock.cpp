#include <cstdarg>
#include <cstdio>
// UI/audio boundaries for exercising the real app and Settings event handlers.
#include "app_ui_mock.h"
#include "audio_manager.h"
#include "file_browser.h"
#include "screen_quick_help.h"

int mockLastInputKeys;
static int ignoreInput(int, int keys, int) { mockLastInputKeys = keys; return 1; }
static void noOp(void) {}
static int startAudio(int, int) { return 0; }
int mockAudioReconfigureResult, mockAudioReconfigureCalls, mockAudioReconfigureRate, mockAudioReconfigureSize, mockAudioCpuLoad;
int mockAudioRestartResult, mockAudioRestartCalls;
static int reconfigureAudio(int rate, int size) { ++mockAudioReconfigureCalls; mockAudioReconfigureRate = rate; mockAudioReconfigureSize = size; return mockAudioReconfigureResult; }
static int restartAudio(void) { ++mockAudioRestartCalls; return mockAudioRestartResult; }
static int cpuLoad() { return mockAudioCpuLoad; }
static int mutateProject(ProjectMutation* mutation, void* userdata) { return mutation ? mutation(userdata) : 1; }
AudioManager audioManager = {startAudio, reconfigureAudio, restartAudio, noOp, noOp, nullptr, nullptr, nullptr, mutateProject, noOp,
                             nullptr, nullptr, cpuLoad};
const AppScreen screenSong = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenTitle = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenPhrase = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenInstrument = {nullptr, nullptr, noOp, noOp, ignoreInput};
int cInstrument = 0;
// Instrument screen boundaries for exercising the real native preview helper.
int mockEnvelopePreviewCount;
void instrumentCommonDrawEnvelopePreview(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) {
  ++mockEnvelopePreviewCount;
}
int instrumentCommonDrawVoicePostCursor(int, int) { return 0; }
int instrumentCommonDrawVoicePostField(int, int, CellState, const InstrumentVoicePostSettings*) { return 0; }
int instrumentCommonOnEditVoicePost(int, int, CellEditAction, InstrumentVoicePostSettings*) { return 0; }
const AppScreen screenKeyMapping = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenFileBrowser = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenColorTheme = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenChain = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenTable = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenInstrumentPool = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenModulation = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenMixer = {nullptr, nullptr, noOp, noOp, ignoreInput};
int screenMixerGetPage(void) { return 0; }
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
void mainLoopSetTargetFps(int) {}
int mainLoopGetTargetFps(void) { return 60; }
int mainLoopGetFrameTimeUs(void) { return 0; }
void screenDraw(void) {}
static char activeMessage[42]{};
void screenMessage(int, const char* format, ...) {
  va_list args; va_start(args, format);
  vsnprintf(activeMessage, sizeof(activeMessage), format, args);
  va_end(args);
}
const char* screenGetActiveMessage(void) { return activeMessage; }
void screenClearMessage(void) { activeMessage[0] = '\0'; }
int mockAudioRestartNotificationFrames = 0;
void screenNotifyAudioRestart(void) { mockAudioRestartNotificationFrames = 180; }
int screenAudioRestartActive(void) { return mockAudioRestartNotificationFrames > 0; }
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

int instrumentCommonColumnCount(int) { return 1; }
void instrumentCommonDrawStatic() {}
void instrumentCommonDrawCursor(int, int) {}
void instrumentCommonDrawField(int, int, CellState) {}
int instrumentCommonOnEdit(int, int, CellEditAction) { return 0; }
