#include <cstdarg>
#include <cstdio>
// UI/audio boundaries for exercising the real app and Settings event handlers.
#include "app_ui_mock.h"
#include "audio_manager.h"
#include "app.h"
#include "corelib_gfx.h"
#include "file_browser.h"
#include "screen_quick_help.h"

int mockLastInputKeys;
static int ignoreInput(int, int keys, int) { mockLastInputKeys = keys; return 1; }
static void noOp(void) {}
static int startAudio(int, int) { return 0; }
int mockAudioReconfigureResult, mockAudioReconfigureCalls, mockAudioReconfigureRate, mockAudioReconfigureSize, mockAudioCpuLoad;
int mockAudioRestartResult, mockAudioRestartCalls;
std::vector<std::string> mockAudioOutputAttempts;
std::vector<int> mockAudioRestartResults;
std::string mockOpenedAudioOutput;
bool mockAudioOutputOpen = true;
extern char mockSelectedAudioOutput[128];
int mockAudioRefreshResult, mockAudioRefreshCalls;
static int reconfigureAudio(int rate, int size) { ++mockAudioReconfigureCalls; mockAudioReconfigureRate = rate; mockAudioReconfigureSize = size; return mockAudioReconfigureResult; }
static int restartAudio(void) {
  const int call = mockAudioRestartCalls++;
  mockAudioOutputAttempts.emplace_back(mockSelectedAudioOutput);
  const int result = call < (int)mockAudioRestartResults.size() ? mockAudioRestartResults[call]
                                                              : mockAudioRestartResult;
  mockAudioOutputOpen = result == 0;
  mockOpenedAudioOutput = mockAudioOutputOpen ? mockSelectedAudioOutput : "";
  return result;
}
static int refreshAudio(void) { ++mockAudioRefreshCalls; return mockAudioRefreshResult; }
static int cpuLoad() { return mockAudioCpuLoad; }
static int mutateProject(ProjectMutation* mutation, void* userdata) { return mutation ? mutation(userdata) : 1; }
static void replaceProject(Project* replacement) {
  projectFree(&chipnomadState->project);
  chipnomadState->project = *replacement;
  *replacement = {};
  appActivateProject();
}
AudioManager audioManager = {startAudio, reconfigureAudio, restartAudio, refreshAudio, noOp, noOp, replaceProject, nullptr, nullptr, mutateProject, noOp,
                             nullptr, nullptr, cpuLoad, nullptr, nullptr, nullptr, nullptr, noOp};
const AppScreen screenSong = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenTitle = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenPhrase = {nullptr, nullptr, noOp, noOp, ignoreInput};
int phraseScreenRecordMidi(uint8_t, uint8_t, uint8_t) { return 1; }
uint8_t mockPhraseMidiInstrument = EMPTY_VALUE_8;
uint8_t phraseScreenMidiInstrument(void) { return mockPhraseMidiInstrument; }
uint8_t mockTableMidiInstrument = EMPTY_VALUE_8;
uint8_t mockPoolMidiInstrument = EMPTY_VALUE_8;
uint8_t tableScreenMidiInstrument(void) { return mockTableMidiInstrument; }
uint8_t instrumentPoolMidiInstrument(void) { return mockPoolMidiInstrument; }
const AppScreen screenInstrument = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenSampleSettings = {nullptr, nullptr, noOp, noOp, ignoreInput};
int cInstrument = 0;
// Instrument screen boundaries for exercising the real native preview helper.
int mockEnvelopePreviewCount;
void instrumentCommonDrawEnvelopePreview(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) {
  ++mockEnvelopePreviewCount;
}
int instrumentCommonDrawVoicePostCursor(int, int) { return 0; }
int instrumentCommonDrawVoicePostField(int, int, CellState, const InstrumentVoicePostSettings*) { return 0; }
int instrumentCommonOnEditVoicePost(int, int, CellEditAction, InstrumentVoicePostSettings*) { return 0; }
const AppScreen screenFileBrowser = {nullptr, nullptr, noOp, noOp, ignoreInput};
const AppScreen screenColorTheme = {nullptr, nullptr, noOp, noOp, ignoreInput};
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
void screenApplyPending(void) {}
bool fileBrowserHasMidiPreview(void) { return false; }
void fileBrowserMidiInput(const MidiPreviewIntent&) {}
static char activeMessage[42]{};
int mockMessageFrames = 0;
void screenMessage(int frames, const char* format, ...) {
  mockMessageFrames = frames;
  va_list args; va_start(args, format);
  vsnprintf(activeMessage, sizeof(activeMessage), format, args);
  va_end(args);
}
const char* screenGetActiveMessage(void) { return activeMessage; }
void screenClearMessage(void) { activeMessage[0] = '\0'; mockMessageFrames = 0; }
int mockAudioRestartNotificationFrames = 0;
void screenNotifyAudioRestart(void) { mockAudioRestartNotificationFrames = 180; }
int screenAudioRestartActive(void) { return mockAudioRestartNotificationFrames > 0; }
void drawScreenMap(void) {}
ScreenPlaybackLevel screenGetPlaybackLevel(const AppScreen*) { return ScreenPlaybackLevel::none; }
LoopRange screenGetLoopRange(const AppScreen*) { return {}; }
void screenFullRedraw(ScreenData* screen) { mockScreenData = screen; gfxClear(); }
int screenInput(ScreenData* screen, int, int, int) { mockScreenData = screen; return 1; }
void setCellColor(CellState state, int empty, int content) {
  const auto& cs = appSettings.colorScheme;
  gfxSetFgColor(state == CellState::selected ? (empty ? cs.textEmpty : cs.selection) :
    state == CellState::focus ? cs.textDefault :
    empty ? cs.textEmpty : content ? cs.textValue : cs.textInfo);
}
void getSelectionBounds(ScreenData* screen, int* startCol, int* startRow, int* endCol, int* endRow) {
  *startCol = screen->selectStartCol < screen->cursorCol ? screen->selectStartCol : screen->cursorCol;
  *endCol = screen->selectStartCol > screen->cursorCol ? screen->selectStartCol : screen->cursorCol;
  *startRow = screen->selectStartRow < screen->cursorRow ? screen->selectStartRow : screen->cursorRow;
  *endRow = screen->selectStartRow > screen->cursorRow ? screen->selectStartRow : screen->cursorRow;
}
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

void fileBrowserSetupFolderMode(const char* title, const char*, const char*,
                                const char* extension, void (*)(const char*),
                                void (*)(void)) {
  mockBrowserTitle = title;
  mockBrowserExtension = extension;
}

int instrumentCommonColumnCount(int) { return 1; }
int instrumentLiveValue(int) { return -1; }
float instrumentLivePosition(int) { return -1.0f; }
void instrumentCommonDrawStatic() {}
void instrumentCommonDrawCursor(int, int) {}
void instrumentCommonDrawField(int, int, CellState) {}
int instrumentCommonOnEdit(int, int, CellEditAction) { return 0; }
