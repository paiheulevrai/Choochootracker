#include "screen_color_theme.h"
#include "screen_audio_input.h"
#include "screen_keymapping.h"
#include "audio_manager.h"
#include "audio_input_manager.h"
#include "app.h"
#include "corelib_file.h"
#include "corelib_font.h"
#include "corelib_gfx.h"
#include "corelib_audio.h"
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
  gfxPrintf(23, 5, "%03d%%", load);
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
  else if (row == 2) gfxCursor(14, 4, 16);
  else if (row == 4) gfxCursor(23, 6, 3); // "OFF"/"ON" value width
}
static void audioField(int, int row, CellState state) {
  const ColorScheme cs = appSettings.colorScheme;
  if (row == 0) {
    gfxSetFgColor(cs.textLabel); gfxPrint(0, 2, "Sample rate");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxPrintf(23, 2, "%d kHz", appSettings.audioSampleRate / 1000);
  } else if (row == 1) {
    gfxSetFgColor(cs.textLabel); gfxPrint(0, 3, "Buffer size");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxClearRect(23, 3, 4, 1);
    gfxPrintf(23, 3, "%d", appSettings.audioBufferSize);
  } else if (row == 2) {
    gfxSetFgColor(cs.textLabel); gfxPrint(0, 4, "Audio Output");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxClearRect(14, 4, 16, 1);
    gfxPrintf(14, 4, "%.16s", appSettings.audioOutputDeviceName[0] ?
      appSettings.audioOutputDeviceName : "Default");
  } else if (row == 4) {
    gfxSetFgColor(cs.textLabel); gfxPrint(0, 6, "Audio Input");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxClearRect(23, 6, 3, 1);
    gfxPrint(23, 6, audioInputArmedCount() > 0 ? "ON" : "OFF");
  } else {
    gfxSetFgColor(cs.textLabel); gfxPrint(0, 5, "CPU load");
    drawAudioCpuLoad();
  }
}
static int audioBufferIndex(int value) {
  for (int i = 0; i < (int)(sizeof(audioBufferSizes) / sizeof(audioBufferSizes[0])); ++i)
    if (audioBufferSizes[i] == value) return i;
  return 0;
}
static int audioEdit(int, int row, CellEditAction action) {
  if (row == 4) {
    if (action == CellEditAction::clear || action == CellEditAction::decrease ||
        action == CellEditAction::decreaseBig || action == CellEditAction::multiDecrease ||
        action == CellEditAction::multiDecreaseBig) {
      audioInputManager.disarmAll();
      for (int i = 0; i < AUDIO_INPUT_MAX_ARMED; ++i) appSettings.audioInputArmed[i] = -1;
      if (settingsSave()) screenMessage(MESSAGE_TIME_ERROR, "Could not save audio setting");
      return 1;
    }
    if (action != CellEditAction::tap) return 0;
    screenSetup(&screenAudioInput, 0);
    return 1;
  }
  if (row == 2) {
    int count = audioOutputDeviceCount();
    if (count < 0) count = 0;
    int current = 0;
    char name[AUDIO_DEVICE_NAME_LENGTH + 1];
    for (int i = 0; i < count; ++i)
      if (!audioOutputDeviceName(i, name, sizeof(name)) &&
          strcmp(name, appSettings.audioOutputDeviceName) == 0) { current = i + 1; break; }
    int next = current;
    if (action == CellEditAction::clear) next = 0;
    else if (action == CellEditAction::tap || action == CellEditAction::increase ||
             action == CellEditAction::increaseBig || action == CellEditAction::multiIncrease ||
             action == CellEditAction::multiIncreaseBig) next = current < count ? current + 1 : 0;
    else if (action == CellEditAction::decrease || action == CellEditAction::decreaseBig ||
             action == CellEditAction::multiDecrease || action == CellEditAction::multiDecreaseBig)
      next = current > 0 ? current - 1 : count;
    else return 0;
    char previous[AUDIO_DEVICE_NAME_LENGTH + 1];
    strcpy(previous, appSettings.audioOutputDeviceName);
    name[0] = 0;
    if (next > 0 && audioOutputDeviceName(next - 1, name, sizeof(name))) return 1;
    if (strcmp(previous, name) == 0) return 1;
    audioOutputSelect(name);
    if (!audioManager.restart || audioManager.restart()) {
      audioOutputSelect(previous);
      if (audioManager.restart) audioManager.restart();
      screenMessage(MESSAGE_TIME_ERROR, "Audio output failed");
      return 1;
    }
    strcpy(appSettings.audioOutputDeviceName, name);
    if (settingsSave()) screenMessage(MESSAGE_TIME_ERROR, "Could not save audio setting");
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
static int audioCellValid(int, int row) { return row == 0 || row == 1 || row == 2 || row == 4; }
static ScreenData audioData = {5, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::none, columnCount, audioStatic, audioCursor, NULL, noHeader, noHeader, audioField, audioEdit, NULL, NULL, audioCellValid, NULL};

static void controlsStatic(void) { gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0, 0, "CONTROLS"); gfxPrint(0, 2, "Keys repeat"); }
#ifdef TOUCH_INPUT
static const int controlsHapticsRow = 3;
static const int controlsHidePadRow = 4;
static const int controlsKeyMappingRow = 5;
#else
static const int controlsKeyMappingRow = 3;
#endif
static const int controlsRows = controlsKeyMappingRow + 1;
static int controlsRowY(int row) { return row < 2 ? 3 + row : row == 2 ? 6 : 5 + row; }
static void controlsCursor(int, int row) {
  if (row < 3) gfxCursor(23, controlsRowY(row), row < 2 ? 3 : 6);
  else if (row == controlsKeyMappingRow) gfxCursor(0, controlsRowY(row), 11);
#ifdef TOUCH_INPUT
  else gfxCursor(23, controlsRowY(row), 3);
#endif
}
static void controlsField(int, int row, CellState state) {
  const ColorScheme cs = appSettings.colorScheme;
  const int y = controlsRowY(row);
  gfxSetFgColor(cs.textLabel);
  if (row == 0) { gfxPrint(0, y, " - Repeat delay"); gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrintf(23, y, "%03d", appSettings.keyRepeatDelay); }
  else if (row == 1) { gfxPrint(0, y, " - Repeat speed"); gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrintf(23, y, "%03d", appSettings.keyRepeatSpeed); }
  else if (row == 2) { gfxPrint(0, y, "Stick live mode"); gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(23, y, appSettings.stickLiveMode == StickLiveMode::free ? "FREE  " : appSettings.stickLiveMode == StickLiveMode::toggle ? "TOGGLE" : "HOLD  "); }
#ifdef TOUCH_INPUT
  else if (row == controlsHapticsRow) { gfxPrint(0, y, "Touch haptics"); gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(23, y, appSettings.touchHaptics ? "ON " : "OFF"); }
  else if (row == controlsHidePadRow) { gfxPrint(0, y, "Hide pad w/ gamepad"); gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(23, y, appSettings.hideTouchPadWithGamepad ? "ON " : "OFF"); }
#endif
  else if (row == controlsKeyMappingRow) { gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(0, y, "Key mapping"); }
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
  gfxSetFgColor(cs.textLabel);
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
  gfxSetFgColor(cs.textLabel); gfxPrint(0, 2 + row, row == 0 ? "Mix volume" : "Tilt pivot");
  gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
  if (row == 0) gfxPrintf(23, 2, "%03d%%", (int)(appSettings.mixVolume * 100.0f + .5f)); else gfxPrintf(23, 3, "%u Hz", chipnomadState->project.tiltPivotHz);
}
static int mixerEdit(int, int row, CellEditAction action) {
  if (row == 0) { uint8_t value = (uint8_t)(appSettings.mixVolume * 100.0f + .5f); int handled = edit8noLast(action, &value, 10, 1, 100); if (handled) { appSettings.mixVolume = (float)value / 100.0f; if (chipnomadState) chipnomadState->mixVolume = appSettings.mixVolume; } return handled; }
  int handled = edit16withMinMax(action, &chipnomadState->project.tiltPivotHz, 250, 250, 4000); if (handled) projectModified = 1; return handled;
}
static ScreenData mixerData = {2, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::none, columnCount, mixerStatic, mixerCursor, NULL, noHeader, noHeader, mixerField, mixerEdit, NULL, NULL, NULL, NULL};

static void graphicsStatic(void) { gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0, 0, "GRAPHICS"); }
static int graphicsRowY(int row) { return 2 + row + (row >= 2) + (row >= 6) + (row >= 7); }
static void graphicsCursor(int, int row) { gfxCursor(row == 2 || row >= 4 ? 23 : 0, graphicsRowY(row), row == 0 ? 16 : row == 1 ? 9 : row == 2 ? 3 : row == 3 ? 13 : row == 4 ? 6 : row == 7 ? 4 : row == 8 ? 2 : 3); }
static void graphicsField(int, int row, CellState state) {
  const ColorScheme cs = appSettings.colorScheme;
  const int y = graphicsRowY(row);
  if (row == 0) {
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(0, y, "Edit color theme");
  } else if (row == 1) {
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(0, y, "Load font");
  } else if (row == 2) {
    gfxSetFgColor(cs.textLabel); gfxPrint(0, y, "Song waveform");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(23, y, appSettings.persistentWaveform ? "ON " : "OFF");
  } else if (row == 3) {
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrint(0, y, "Track visuals");
  } else if (row == 4) {
    gfxSetFgColor(cs.textLabel); gfxPrint(0, y, "Waveform FPS");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault); gfxPrintf(23, y, "%02d FPS", appSettings.waveformRefreshHz);
  } else if (row == 5) {
    gfxSetFgColor(cs.textLabel); gfxPrint(0, y, "Renderer");
    gfxSetFgColor(cs.textInfo); gfxPrint(23, y, gfxGetRendererType());
  } else if (row == 6) {
    gfxSetFgColor(cs.textLabel); gfxPrint(0, y, "Boot screen");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxPrint(23, y, appSettings.bootScreenNight ? "Night   " : "Original");
  } else if (row == 7) {
    gfxSetFgColor(cs.textLabel); gfxPrint(0, y, "Playback counter");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxPrint(23, y, appSettings.playbackCounterMode ? "Time" : "Bars");
  } else if (row == 8) {
    gfxSetFgColor(cs.textLabel); gfxPrint(0, y, " - Rows per bar");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxPrintf(23, y, "%02u", appSettings.playbackBarRows);
  } else if (row == 9) {
    gfxSetFgColor(cs.textLabel); gfxPrint(0, y, " - Ticks per row");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxPrintf(23, y, "%03u", appSettings.playbackBarTicks);
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
  if (row >= 7 && row <= 9) {
    uint8_t value = row == 7 ? appSettings.playbackCounterMode :
      row == 8 ? appSettings.playbackBarRows : appSettings.playbackBarTicks;
    if (row == 7 && (action == CellEditAction::tap || action == CellEditAction::doubleTap)) value ^= 1;
    else if (!edit8noLast(action, &value, 1, row == 7 ? 0 : 1,
                          row == 7 ? 1 : row == 8 ? 16 : 254)) return 0;
    if (row == 7) appSettings.playbackCounterMode = value;
    else if (row == 8) appSettings.playbackBarRows = value;
    else appSettings.playbackBarTicks = value;
    if (settingsSave() != 0) screenMessage(MESSAGE_TIME, "Could not save counter setting");
    return 1;
  }
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
static ScreenData graphicsData = {10, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::none, columnCount, graphicsStatic, graphicsCursor, NULL, noHeader, noHeader, graphicsField, graphicsEdit, NULL, NULL, graphicsCellValid, NULL};

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
