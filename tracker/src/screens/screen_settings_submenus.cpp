#include "screen_color_theme.h"
#include "screen_audio_input.h"
#include "screen_keymapping.h"
#include "audio_manager.h"
#include "audio_input_manager.h"
#include "app.h"
#include "corelib_file.h"
#include "corelib_font.h"
#include "corelib_gfx.h"
#include "file_browser.h"
#include "screens.h"
#include "waveform_display.h"
#include <string.h>

static int columnCount(int) { return 1; }
static void noHeader(int, CellState) {}
static void setup(int) {}
static void draw(void) {}

static const int audioBufferSizes[] = {32, 64, 128, 256, 512, 1024};
static const int audioSampleRates[] = {32000, 48000};
static int displayedCpuLoad = -1;

static void drawAudioCpuLoad(void) {
  int load = audioManager.getCpuLoadPercent ? audioManager.getCpuLoadPercent() : 0;
  if (load < 0) load = 0;
  if (load > 999) load = 999;
  if (load == displayedCpuLoad) return;
  displayedCpuLoad = load;
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrintf(23, 4, "%03d%%", load);
}

static void audioStatic(void) { gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0, 0, "AUDIO"); }

static int audioInputArmedCount(void) {
  int count = 0;
  for (int i = 0; i < AUDIO_INPUT_MAX_ARMED; i++)
    if (appSettings.audioInputArmed[i] >= 0) count++;
  return count;
}

static void audioCursor(int, int row) {
  if (row == 0) gfxCursor(23, 2, 6);
  else if (row == 1) gfxCursor(23, 3, appSettings.audioBufferSize == 1024 ? 4 : 3);
  else if (row == 3) gfxCursor(23, 5, 3); // "OFF"/"ON" value width
}
static void audioField(int, int row, CellState state) {
  const ColorScheme cs = appSettings.colorScheme;
  if (row == 0) {
    gfxSetFgColor(cs.textDefault); gfxPrint(0, 2, "Sample rate");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxPrintf(23, 2, "%d kHz", appSettings.audioSampleRate / 1000);
  } else if (row == 1) {
    gfxSetFgColor(cs.textDefault); gfxPrint(0, 3, "Buffer size");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxClearRect(23, 3, 4, 1);
    gfxPrintf(23, 3, "%d", appSettings.audioBufferSize);
  } else if (row == 3) {
    gfxSetFgColor(cs.textDefault); gfxPrint(0, 5, "Audio Input");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxClearRect(23, 5, 3, 1);
    gfxPrint(23, 5, audioInputArmedCount() > 0 ? "ON" : "OFF");
  } else {
    gfxSetFgColor(cs.textDefault); gfxPrint(0, 4, "CPU load");
    drawAudioCpuLoad();
  }
}
static int audioBufferIndex(int value) {
  for (int i = 0; i < (int)(sizeof(audioBufferSizes) / sizeof(audioBufferSizes[0])); ++i)
    if (audioBufferSizes[i] == value) return i;
  return 0;
}
static int audioEdit(int, int row, CellEditAction action) {
  if (row == 3) {
    if (action != CellEditAction::tap) return 0;
    screenSetup(&screenAudioInput, 0);
    return 1;
  }
  if (row != 0 && row != 1) return 0;
  int direction = 0;
  if (action == CellEditAction::clear) direction = -99;
  else if (action == CellEditAction::increase || action == CellEditAction::increaseBig ||
           action == CellEditAction::multiIncrease || action == CellEditAction::multiIncreaseBig) direction = 1;
  else if (action == CellEditAction::decrease || action == CellEditAction::decreaseBig ||
           action == CellEditAction::multiDecrease || action == CellEditAction::multiDecreaseBig) direction = -1;
  else return action == CellEditAction::tap;

  int index = row == 0 ? (appSettings.audioSampleRate == 32000 ? 0 : 1)
                       : audioBufferIndex(appSettings.audioBufferSize);
  int count = row == 0 ? 2 : (int)(sizeof(audioBufferSizes) / sizeof(audioBufferSizes[0]));
  if (direction < -1) index = row == 0 ? 1 : 0;
  else if (direction > 0 && index + 1 < count) ++index;
  else if (direction < 0 && index > 0) --index;
  const int rate = row == 0 ? audioSampleRates[index] : appSettings.audioSampleRate;
  const int size = row == 1 ? audioBufferSizes[index] : appSettings.audioBufferSize;
  if (rate == appSettings.audioSampleRate && size == appSettings.audioBufferSize) return 1;
  if (row == 0 && audioInputManagerRecordingActive()) {
    screenMessage(MESSAGE_TIME_ERROR, "Stop recording first");
    return 1;
  }
  if (!audioManager.reconfigure || audioManager.reconfigure(rate, size) != 0) {
    screenMessage(MESSAGE_TIME_ERROR, row == 0 ? "Audio rate failed" : "Audio buffer failed");
    return 1;
  }
  appSettings.audioSampleRate = rate;
  appSettings.audioBufferSize = size;
  if (settingsSave() != 0) screenMessage(MESSAGE_TIME_ERROR, "Could not save audio setting");
  return 1;
}
static int audioCellValid(int, int row) { return row == 0 || row == 1 || row == 3; }
static ScreenData audioData = {4, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::none, columnCount, audioStatic, audioCursor, NULL, noHeader, noHeader, audioField, audioEdit, NULL, NULL, audioCellValid, NULL};

static void controlsStatic(void) { gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0, 0, "CONTROLS"); }
#ifdef TOUCH_INPUT
static const int controlsHapticsRow = 3;
static const int controlsHidePadRow = 4;
static const int controlsKeyMappingRow = 5;
#else
static const int controlsKeyMappingRow = 3;
#endif
static const int controlsRows = controlsKeyMappingRow + 1;
static void controlsCursor(int, int row) {
  if (row < 3) gfxCursor(23, 2 + row, row < 2 ? 3 : 6);
  else if (row == controlsKeyMappingRow) gfxCursor(0, 2 + row, 11);
#ifdef TOUCH_INPUT
  else gfxCursor(23, 2 + row, 3);
#endif
}
static void controlsField(int, int row, CellState state) {
  const ColorScheme cs = appSettings.colorScheme;
  gfxSetFgColor(cs.textDefault);
  if (row == 0) { gfxPrint(0, 2, "Repeat delay"); gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrintf(23, 2, "%03d", appSettings.keyRepeatDelay); }
  else if (row == 1) { gfxPrint(0, 3, "Repeat speed"); gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrintf(23, 3, "%03d", appSettings.keyRepeatSpeed); }
  else if (row == 2) { gfxPrint(0, 4, "Stick live mode"); gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(23, 4, appSettings.stickLiveMode == StickLiveMode::free ? "FREE  " : appSettings.stickLiveMode == StickLiveMode::toggle ? "TOGGLE" : "HOLD  "); }
#ifdef TOUCH_INPUT
  else if (row == controlsHapticsRow) { gfxPrint(0, 5, "Touch haptics"); gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(23, 5, appSettings.touchHaptics ? "ON " : "OFF"); }
  else if (row == controlsHidePadRow) { gfxPrint(0, 6, "Hide pad w/ gamepad"); gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(23, 6, appSettings.hideTouchPadWithGamepad ? "ON " : "OFF"); }
#endif
  else if (row == controlsKeyMappingRow) { gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(0, 2 + row, "Key mapping"); }
}
static int controlsEdit(int, int row, CellEditAction action) {
  if (row == 0) { uint8_t value = appSettings.keyRepeatDelay; int handled = edit8noLast(action, &value, 4, 4, 30); if (handled) appSettings.keyRepeatDelay = value; return handled; }
  if (row == 1) { uint8_t value = appSettings.keyRepeatSpeed; int handled = edit8noLast(action, &value, 2, 1, 12); if (handled) appSettings.keyRepeatSpeed = value; return handled; }
  if (row == 2) { uint8_t value = (uint8_t)appSettings.stickLiveMode; int handled = edit8noLast(action, &value, 1, 0, 2); if (handled) appSetStickLiveMode((StickLiveMode)value); return handled; }
#ifdef TOUCH_INPUT
  if (row == controlsHapticsRow) {
    uint8_t value = appSettings.touchHaptics;
    if (action == CellEditAction::tap || action == CellEditAction::doubleTap) value ^= 1;
    else if (!edit8noLast(action, &value, 1, 0, 1)) return 0;
    appSettings.touchHaptics = value;
#ifdef ANDROID_BUILD
    extern void androidSetTouchHaptics(int enabled);
    androidSetTouchHaptics(value);
#endif
    if (settingsSave() != 0) screenMessage(MESSAGE_TIME, "Could not save setting");
    return 1;
  }
#endif
  if (row == controlsKeyMappingRow) { if (action == CellEditAction::tap) { screenSetup(&screenKeyMapping, 0); return 1; } return 0; }
  uint8_t value = appSettings.hideTouchPadWithGamepad;
  if (action == CellEditAction::tap || action == CellEditAction::doubleTap) value ^= 1;
  else if (!edit8noLast(action, &value, 1, 0, 1)) return 0;
  appSettings.hideTouchPadWithGamepad = value;
  if (settingsSave() != 0) screenMessage(MESSAGE_TIME, "Could not save setting");
  return 1;
}
static ScreenData controlsData = {controlsRows, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::none, columnCount, controlsStatic, controlsCursor, NULL, noHeader, noHeader, controlsField, controlsEdit, NULL, NULL, NULL, NULL};

static void synthStatic(void) { gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0, 0, "SYNTHS"); }
static void synthCursor(int, int row) { gfxCursor(23, 2 + row, row == 0 ? 6 : row == 1 ? 3 : row == 2 ? 6 : 4); }
static void synthField(int, int row, CellState state) {
  const ColorScheme cs = appSettings.colorScheme;
  gfxSetFgColor(cs.textDefault);
  if (row == 0) {
    static const char* names[] = {"LOW   ", "MEDIUM", "HIGH  ", "BEST  "};
    gfxPrint(0, 2, "AY Quality"); gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(23, 2, names[appSettings.quality]);
  } else if (row == 1) {
    gfxPrint(0, 3, "AY Sample dithering"); gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(23, 3, appSettings.aySampleDithering ? "ON " : "OFF");
  } else {
    static const char* labels[] = {"Braids BITS", "Braids DRFT", "Braids SIGN"};
    static const char* bits[] = {"2 BIT ", "3 BIT ", "4 BIT ", "6 BIT ", "8 BIT ", "12 BIT", "16 BIT"};
    static const char* levels[] = {"OFF ", "LOW ", "MED ", "HIGH", "MAX "};
    gfxPrint(0, 2 + row, labels[row - 2]); gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxPrint(23, 2 + row, row == 2 ? bits[appSettings.braidsBits] : levels[row == 3 ? appSettings.braidsDrift : appSettings.braidsSignature]);
  }
}
static int synthEdit(int, int row, CellEditAction action) {
  if (row == 0) { int handled = edit8noLast(action, (uint8_t*)&appSettings.quality, 1, 0, 3); if (handled) chipnomadSetQuality(chipnomadState, (ChipNomadQuality)appSettings.quality); return handled; }
  if (row == 1) { int handled = edit8noLast(action, (uint8_t*)&appSettings.aySampleDithering, 1, 0, 1); if (handled && chipnomadState) chipnomadState->aySampleDithering = appSettings.aySampleDithering; return handled; }
  uint8_t value = (uint8_t)(row == 2 ? appSettings.braidsBits : row == 3 ? appSettings.braidsDrift : appSettings.braidsSignature);
  int handled = edit8noLast(action, &value, 1, 0, row == 2 ? 6 : 4);
  if (handled) {
    if (row == 2) appSettings.braidsBits = value;
    else if (row == 3) appSettings.braidsDrift = value;
    else appSettings.braidsSignature = value;
  }
  if (handled) chipnomadSetBraidsSettings(chipnomadState, appSettings.braidsBits, appSettings.braidsDrift, appSettings.braidsSignature, appSettings.braidsSignatureSeed);
  return handled;
}
static ScreenData synthData = {5, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::none, columnCount, synthStatic, synthCursor, NULL, noHeader, noHeader, synthField, synthEdit, NULL, NULL, NULL, NULL};

static void mixerStatic(void) { gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0, 0, "MIXER"); }
static void mixerCursor(int, int row) { gfxCursor(23, 2 + row, 4); }
static void mixerField(int, int row, CellState state) {
  const ColorScheme cs = appSettings.colorScheme;
  gfxSetFgColor(cs.textDefault); gfxPrint(0, 2 + row, row == 0 ? "Mix volume" : "Tilt pivot");
  gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
  if (row == 0) gfxPrintf(23, 2, "%03d%%", (int)(appSettings.mixVolume * 100.0f + .5f)); else gfxPrintf(23, 3, "%u Hz", chipnomadState->project.tiltPivotHz);
}
static int mixerEdit(int, int row, CellEditAction action) {
  if (row == 0) { uint8_t value = (uint8_t)(appSettings.mixVolume * 100.0f + .5f); int handled = edit8noLast(action, &value, 10, 1, 100); if (handled) { appSettings.mixVolume = (float)value / 100.0f; if (chipnomadState) chipnomadState->mixVolume = appSettings.mixVolume; } return handled; }
  int handled = edit16withMinMax(action, &chipnomadState->project.tiltPivotHz, 250, 250, 4000); if (handled) projectModified = 1; return handled;
}
static ScreenData mixerData = {2, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::none, columnCount, mixerStatic, mixerCursor, NULL, noHeader, noHeader, mixerField, mixerEdit, NULL, NULL, NULL, NULL};

static void graphicsStatic(void) { gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0, 0, "GRAPHICS"); }
static void graphicsCursor(int, int row) { gfxCursor(row == 2 || row >= 4 ? 23 : 0, 2 + row, row == 0 ? 16 : row == 1 ? 9 : row == 2 ? 3 : row == 3 ? 13 : row == 4 ? 6 : 8); }
static void graphicsField(int, int row, CellState state) {
  const ColorScheme cs = appSettings.colorScheme;
  if (row == 0) {
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(0, 2, "Edit color theme");
  } else if (row == 1) {
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(0, 3, "Load font");
  } else if (row == 2) {
    gfxSetFgColor(cs.textDefault); gfxPrint(0, 4, "Song waveform");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(23, 4, appSettings.persistentWaveform ? "ON " : "OFF");
  } else if (row == 3) {
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(0, 5, "Track visuals");
  } else if (row == 4) {
    gfxSetFgColor(cs.textDefault); gfxPrint(0, 6, "Waveform FPS");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrintf(23, 6, "%02d FPS", appSettings.waveformRefreshHz);
  } else if (row == 5) {
    gfxSetFgColor(cs.textDefault); gfxPrint(0, 7, "Renderer");
    gfxSetFgColor(cs.textInfo); gfxPrint(23, 7, gfxGetRendererType());
  } else if (row == 6) {
    gfxSetFgColor(cs.textDefault); gfxPrint(0, 8, "Boot screen");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxPrint(23, 8, appSettings.bootScreenNight ? "Night   " : "Original");
  }
}
static void fontLoaded(const char* path) {
  Font* font = fontLoad(path);
  if (font) {
    fontSetCurrent(font); gfxReloadFont(); strncpy(appSettings.fontPath, path, PATH_LENGTH); appSettings.fontPath[PATH_LENGTH] = 0;
    const char* separator = strrchr(path, PATH_SEPARATOR);
    if (separator) { int len = separator - path; if (len > 0 && len < PATH_LENGTH) { strncpy(appSettings.fontFolderPath, path, len); appSettings.fontFolderPath[len] = 0; } }
    screenMessage(MESSAGE_TIME, "Loaded: %s", font->name);
  } else screenMessage(MESSAGE_TIME, "Failed to load font");
  screenSetup(&screenGraphicsSettings, 0);
}
static void fontCancelled(void) { screenSetup(&screenGraphicsSettings, 0); }
static int graphicsEdit(int, int row, CellEditAction action) {
  if (row == 6) {
    uint8_t value = appSettings.bootScreenNight;
    if (action == CellEditAction::tap || action == CellEditAction::doubleTap) value ^= 1;
    else if (!edit8noLast(action, &value, 1, 0, 1)) return 0;
    appSettings.bootScreenNight = value;
    if (settingsSave() != 0) screenMessage(MESSAGE_TIME, "Could not save boot screen");
    return 1;
  }
  if (row == 2) {
    uint8_t value = appSettings.persistentWaveform;
    if (action == CellEditAction::tap || action == CellEditAction::doubleTap) value ^= 1;
    else if (!edit8noLast(action, &value, 1, 0, 1)) return 0;
    appSettings.persistentWaveform = value;
    if (settingsSave() != 0) screenMessage(MESSAGE_TIME, "Could not save waveform setting");
    return 1;
  }
  if (row == 4) {
    uint8_t value = (uint8_t)appSettings.waveformRefreshHz;
    if (!edit8noLast(action, &value, 5, 1, 60)) return 0;
    appSettings.waveformRefreshHz = value;
    if (settingsSave() != 0) screenMessage(MESSAGE_TIME, "Could not save waveform setting");
    return 1;
  }
  if (row == 5 || action != CellEditAction::tap) return 0;
  if (row == 0) screenSetup(&screenColorTheme, 0);
  else if (row == 1) { fileBrowserSetup("LOAD FONT", ".cnfont", appSettings.fontFolderPath, fontLoaded, fontCancelled); screenSetup(&screenFileBrowser, 0); }
  else if (row == 3) screenSetup(&screenTrackVisuals, 0);
  return 0;
}
static int graphicsCellValid(int, int) { return 1; }
static ScreenData graphicsData = {7, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::none, columnCount, graphicsStatic, graphicsCursor, NULL, noHeader, noHeader, graphicsField, graphicsEdit, NULL, NULL, graphicsCellValid, NULL};

static int menuInput(ScreenData* data, int isKeyDown, int keys, int taps) { if (keys == keyOpt) { screenSetup(&screenSettings, 0); return 1; } return screenInput(data, isKeyDown, keys, taps); }
static void audioRedraw(void) { displayedCpuLoad = -1; screenFullRedraw(&audioData); }
static void controlsRedraw(void) { screenFullRedraw(&controlsData); }
static void synthRedraw(void) { screenFullRedraw(&synthData); }
static void mixerRedraw(void) { screenFullRedraw(&mixerData); }
static void graphicsRedraw(void) { screenFullRedraw(&graphicsData); }
static int synthInput(int down, int keys, int taps) { return menuInput(&synthData, down, keys, taps); }
static int controlsInput(int down, int keys, int taps) { return menuInput(&controlsData, down, keys, taps); }
static int audioInput(int down, int keys, int taps) { return menuInput(&audioData, down, keys, taps); }
static int mixerInput(int down, int keys, int taps) { return menuInput(&mixerData, down, keys, taps); }
static int graphicsInput(int down, int keys, int taps) { return menuInput(&graphicsData, down, keys, taps); }
static ScreenPlaybackLevel playbackLevel(void) { return ScreenPlaybackLevel::song; }
const AppScreen screenControlsSettings = {NULL, setup, controlsRedraw, draw, controlsInput, playbackLevel};
const AppScreen screenAudioSettings = {NULL, setup, audioRedraw, drawAudioCpuLoad, audioInput, playbackLevel};
const AppScreen screenSynthSettings = {NULL, setup, synthRedraw, draw, synthInput, playbackLevel};
const AppScreen screenMixerSettings = {NULL, setup, mixerRedraw, draw, mixerInput, playbackLevel};
const AppScreen screenGraphicsSettings = {NULL, setup, graphicsRedraw, draw, graphicsInput, playbackLevel};
