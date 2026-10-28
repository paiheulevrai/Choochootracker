#pragma once
#include "screens.h"
#include <string>
#include <vector>

extern char mockGfxCells[20][40];
extern int mockTextRight, mockTextBottom;
extern int mockCursorX, mockCursorY, mockCursorWidth;
extern int mockQuitTriggered;
extern ScreenData* mockScreenData;
extern const char* mockBrowserTitle;
extern const char* mockBrowserExtension;
extern int mockLastInputKeys;
extern int mockEnvelopePreviewCount;
extern int mockBitmapDrawCount, mockBitmapDrawRow;
extern int mockAudioReconfigureResult, mockAudioReconfigureCalls, mockAudioReconfigureRate, mockAudioReconfigureSize, mockAudioCpuLoad;
extern int mockAudioRestartResult, mockAudioRestartCalls;
extern std::vector<std::string> mockAudioOutputNames, mockAudioOutputAttempts;
extern std::vector<int> mockAudioRestartResults;
extern std::string mockOpenedAudioOutput;
extern bool mockAudioOutputOpen;
extern int mockMessageFrames;
extern int mockAudioRefreshResult, mockAudioRefreshCalls;
extern int mockAudioRestartNotificationFrames;
