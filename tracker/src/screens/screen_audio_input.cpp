// Audio Input settings sub-screen (Phase 4 of the audio input recording
// feature). Lists capture devices with live level meters and per-device ARM
// checkboxes. Modeled on screen_midi.cpp.
#include "screen_audio_input.h"
#include "../common.h"
#include "../audio_input_manager.h"
#include "../corelib/corelib_audio.h"
#include "../monitor_display.h"
#include "corelib_gfx.h"
#include "corelib_input.h"
#include "screens.h"
#include <string.h>
#include <stdio.h>

#define NAME_FIELD_X (0)
#define NAME_FIELD_WIDTH (20)
// Level meter: a small square (inherits the row height) placed between the
// name and the ARM checkbox. Display-only: the cursor and touch taps skip
// it (see isCellValid and the touch mapping in screens.cpp).
#define METER_FIELD_X (26)
#define METER_FIELD_WIDTH (2)
#define ARM_FIELD_X (28)
#define ARM_FIELD_WIDTH (4)

// Marquee state for long device names: while the cursor rests on a name
// that does not fit the field, the full name scrolls through it.
static int marqueeDevice = -1;
static int marqueeOffset = 0;
static int marqueeFrameCounter = 0;
static constexpr int MARQUEE_INTERVAL = 10; // ~6 chars/sec at 60 fps

// Truncates src to at most maxLen visible characters, replacing the tail
// with "..." when it doesn't fit (same approach as midiDeviceLabel).
static void truncateWithEllipsis(const char* src, char* dst, int maxLen) {
  int len = (int)strlen(src);
  if (len <= maxLen) {
    strcpy(dst, src);
    return;
  }
  if (maxLen <= 3) {
    strncpy(dst, src, maxLen);
    dst[maxLen] = '\0';
    return;
  }
  strncpy(dst, src, maxLen - 3);
  strcpy(dst + (maxLen - 3), "...");
}

// Resolves a device index to a display label clipped to NAME_FIELD_WIDTH.
static void audioInputDeviceLabel(int deviceIndex, char* buffer, int bufferSize) {
  char fullName[128];
  if (deviceIndex < 0 || deviceIndex >= audioInputManager.deviceCount() ||
      audioInputManager.deviceName(deviceIndex, fullName, sizeof(fullName)) != 0) {
    snprintf(buffer, bufferSize, "OFF");
    return;
  }
  truncateWithEllipsis(fullName, buffer, NAME_FIELD_WIDTH);
}

// Returns the armed slot for a device index, or -1.
static int armedSlotForDevice(int deviceIndex) {
  for (int slot = 0; slot < AUDIO_INPUT_MAX_ARMED; ++slot)
    if (audioInputManager.armedDeviceIndex(slot) == deviceIndex) return slot;
  return -1;
}

static int columnCount(int row) {
  // Keep counts stable across rows: cursor navigation clamps
  // cursorCol = getColumnCount(row) - 1, so returning 0 on empty rows would
  // produce cursorCol = -1. Empty rows are excluded via isCellValid instead.
  return row == 0 ? 2 : 3;
}

static void drawStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 0, "AUDIO INPUT");
  // Column header for the ARM checkboxes (device rows start at y=4).
  gfxPrint(ARM_FIELD_X, 3, "ARM");
}

static void drawCursor(int col, int row) {
  if (row == 0) {
    // Top row buttons: cursor spans the tapped cell.
    if (col == 0) gfxCursor(0, 2, 6);
    else gfxCursor(10, 2, 10);
  } else {
    int deviceIndex = row - 1;
    if (col == 0) {
      char name[NAME_FIELD_WIDTH + 1];
      audioInputDeviceLabel(deviceIndex, name, sizeof(name));
      gfxCursor(NAME_FIELD_X, 4 + deviceIndex, (int)strlen(name));
    } else if (col == 2) {
      gfxCursor(ARM_FIELD_X, 4 + deviceIndex, ARM_FIELD_WIDTH);
    }
  }
}

static void drawRowHeader(int row, CellState state) {
}

static void drawColHeader(int col, CellState state) {
}

static void drawField(int col, int row, CellState state) {
  const ColorScheme cs = appSettings.colorScheme;
  if (row == 0) {
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    if (col == 0) gfxPrint(0, 2, "[List]");
    else if (col == 1) gfxPrint(10, 2, "[Refresh]");
    return;
  }
  int deviceIndex = row - 1;
  if (deviceIndex < 0 || deviceIndex >= audioInputManager.deviceCount()) return;
  int y = 4 + deviceIndex;
  if (col == 0) {
    char name[NAME_FIELD_WIDTH + 1];
    audioInputDeviceLabel(deviceIndex, name, sizeof(name));
    gfxSetFgColor(cs.textDefault);
    gfxClearRect(NAME_FIELD_X, y, NAME_FIELD_WIDTH, 1);
    gfxPrint(NAME_FIELD_X, y, name);
  } else if (col == 1) {
    int slot = armedSlotForDevice(deviceIndex);
    gfxSetBgColor(cs.background);
    gfxClearRect(METER_FIELD_X, y, METER_FIELD_WIDTH, 1);
    if (slot >= 0) monitorDisplayDrawHorizontalMeter(slot, METER_FIELD_X, y, METER_FIELD_WIDTH);
  } else if (col == 2) {
    int slot = armedSlotForDevice(deviceIndex);
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxClearRect(ARM_FIELD_X, y, ARM_FIELD_WIDTH, 1);
    gfxPrint(ARM_FIELD_X, y, slot >= 0 ? "[x]" : "[ ]");
  }
}

// Persists armed device indices into appSettings (settingsSave() writes them
// as names; see common.cpp's audioInputArmed.%d branch). The live device
// list must be synced into appSettings first: settingsSave() resolves armed
// slots to names via appSettings.audioInputDeviceNames, and nothing else
// populates that list.
static void persistArmedDevices(void) {
  int deviceCount = audioInputManager.deviceCount();
  if (deviceCount > AUDIO_INPUT_MAX_DEVICES) deviceCount = AUDIO_INPUT_MAX_DEVICES;
  appSettings.audioInputDeviceCount = deviceCount;
  for (int i = 0; i < deviceCount; ++i) {
    audioInputManager.deviceName(i, appSettings.audioInputDeviceNames[i],
      AUDIO_DEVICE_NAME_LENGTH + 1);
  }
  for (int slot = 0; slot < AUDIO_INPUT_MAX_ARMED; ++slot) {
    int deviceIndex = audioInputManager.armedDeviceIndex(slot);
    appSettings.audioInputArmed[slot] = (int8_t)(deviceIndex >= 0 ? deviceIndex : -1);
  }
  settingsSave();
}

// Match saved names against the current enumeration, whose indices can change
// when an interface is plugged in or removed.
void syncArmedDevicesFromSettings(void) {
  int wanted[AUDIO_INPUT_MAX_ARMED];
  int wantedCount = 0;
  for (int slot = 0; slot < AUDIO_INPUT_MAX_ARMED; ++slot) {
    int saved = appSettings.audioInputArmed[slot];
    if (saved < 0 || saved >= appSettings.audioInputDeviceCount) continue;
    for (int live = 0; live < audioInputManager.deviceCount(); ++live) {
      char name[AUDIO_DEVICE_NAME_LENGTH + 1];
      if (audioInputManager.deviceName(live, name, sizeof(name)) == 0 &&
          strcmp(name, appSettings.audioInputDeviceNames[saved]) == 0) {
        wanted[wantedCount++] = live;
        break;
      }
    }
  }
  bool unchanged = audioInputManager.armedCount() == wantedCount;
  for (int i = 0; unchanged && i < wantedCount; ++i)
    unchanged = audioInputManager.armedDeviceIndex(i) == wanted[i];
  if (unchanged) return;
  audioInputManager.disarmAll();
  for (int i = 0; i < wantedCount; ++i) audioInputManager.arm(wanted[i]);
}

static int onEdit(int col, int row, CellEditAction action);

static int isCellValid(int col, int row) {
  if (row == 0) return col < 2;
  if (col == 1) return 0; // meter column is display-only
  int deviceIndex = row - 1;
  return deviceIndex >= 0 && deviceIndex < audioInputManager.deviceCount();
}

static ScreenData screenAudioInputData = {
  .rows = 1 + AUDIO_INPUT_MAX_DEVICES,
  .cursorRow = 0,
  .cursorCol = 0,
  .topRow = 0,
  .selectMode = -1,
  .selectStartRow = 0,
  .selectStartCol = 0,
  .selectAnchorRow = 0,
  .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::song,
  .getColumnCount = columnCount,
  .drawStatic = drawStatic,
  .drawCursor = drawCursor,
  .drawSelection = NULL,
  .drawRowHeader = drawRowHeader,
  .drawColHeader = drawColHeader,
  .drawField = drawField,
  .onEdit = onEdit,
  .onInput = NULL,
  .onRawInput = NULL,
  .isCellValid = isCellValid,
  .getLoopRange = NULL,
};
static int onEdit(int col, int row, CellEditAction action) {
  if (row == 0 && action == CellEditAction::tap) {
    if (col == 0) {
      // List: re-read the device enumeration without restarting SDL audio.
      audioInputManager.enumerate();
      syncArmedDevicesFromSettings();
      screenFullRedraw(&screenAudioInputData);
      return 1;
    } else if (col == 1) {
      // Refresh: restart the SDL audio subsystem for hot-plugged devices.
      audioInputManager.disarmAll();
      audioInputCleanup();
      audioInputRefreshDevices();
      audioInputManager.enumerate();
      syncArmedDevicesFromSettings();
      screenFullRedraw(&screenAudioInputData);
      return 1;
    }
    return 0;
  }
  if (row > 0 && col == 2 && action == CellEditAction::tap) {
    int deviceIndex = row - 1;
    int slot = armedSlotForDevice(deviceIndex);
    if (slot >= 0) {
      audioInputManager.disarm(slot);
    } else {
      slot = audioInputManager.arm(deviceIndex);
      if (slot < 0) {
        screenMessage(MESSAGE_TIME, "Cannot arm audio input");
        return 1;
      }
    }
    persistArmedDevices();
    screenFullRedraw(&screenAudioInputData);
    return 1;
  }
  return 0;
}



static void setup(int input) {
  screenAudioInputData.cursorRow = 0;
  screenAudioInputData.cursorCol = 0;
  audioInputManager.enumerate();
  syncArmedDevicesFromSettings();
}

static void fullRedraw(void) {
  screenFullRedraw(&screenAudioInputData);
}

static void draw(void) {
  // Live meters: redraw meter cells each frame for armed devices.
  for (int deviceIndex = 0; deviceIndex < audioInputManager.deviceCount(); ++deviceIndex) {
    int slot = armedSlotForDevice(deviceIndex);
    if (slot >= 0) {
      monitorDisplayDrawHorizontalMeter(slot, METER_FIELD_X, 4 + deviceIndex, METER_FIELD_WIDTH);
    }
  }
  // Marquee: when the cursor rests on a device name longer than the
  // field, scroll the full name through the field so it can be read.
  const int cursorRow = screenAudioInputData.cursorRow;
  const int cursorCol = screenAudioInputData.cursorCol;
  int deviceIndex = -1;
  char fullName[128];
  if (cursorRow > 0 && cursorCol == 0) {
    deviceIndex = cursorRow - 1;
    if (deviceIndex >= audioInputManager.deviceCount() ||
        audioInputManager.deviceName(deviceIndex, fullName, sizeof(fullName)) != 0) {
      deviceIndex = -1;
    }
  }
  if (deviceIndex >= 0 && (int)strlen(fullName) > NAME_FIELD_WIDTH) {
    const int len = (int)strlen(fullName);
    if (marqueeDevice != deviceIndex) {
      marqueeDevice = deviceIndex;
      marqueeOffset = 0;
      marqueeFrameCounter = 0;
    }
    if (++marqueeFrameCounter >= MARQUEE_INTERVAL) {
      marqueeFrameCounter = 0;
      marqueeOffset = (marqueeOffset + 1) % (len + 1);
    }
    // Wrap-around window over the full name with one blank at the seam.
    char window[NAME_FIELD_WIDTH + 1];
    for (int i = 0; i < NAME_FIELD_WIDTH; ++i) {
      const int pos = (marqueeOffset + i) % (len + 1);
      window[i] = pos < len ? fullName[pos] : ' ';
    }
    window[NAME_FIELD_WIDTH] = '\0';
    const int y = 4 + deviceIndex;
    gfxSetFgColor(appSettings.colorScheme.textDefault);
    gfxClearRect(NAME_FIELD_X, y, NAME_FIELD_WIDTH, 1);
    gfxPrint(NAME_FIELD_X, y, window);
    // The clear wipes the cursor underline (bottom pixel row of the
    // cell), so it must be redrawn every frame.
    gfxCursor(NAME_FIELD_X, y, NAME_FIELD_WIDTH);
  } else if (marqueeDevice >= 0) {
    // Focus left the name (or the name now fits): restore the truncated
    // label. drawField's clear wipes the cursor underline, so redraw it
    // when the cursor is still on this cell.
    const int row = marqueeDevice + 1;
    drawField(0, row, CellState::normal);
    if (screenAudioInputData.cursorRow == row && screenAudioInputData.cursorCol == 0) {
      drawCursor(0, row);
    }
    marqueeDevice = -1;
  }
  // Helper text: shown whenever no message owns the bottom bar (the
  // message bar is printed by screenDraw() after screen->draw()). Stay
  // quiet while the audio-restart notice is showing so it stays visible.
  if (screenGetActiveMessage()[0] == '\0' && !screenAudioRestartActive()) {
    gfxSetFgColor(appSettings.colorScheme.textEmpty);
    gfxPrint(0, 19, "Check to arm input");
  }
}

static int audioInputScreenInput(int isKeyDown, int keys, int taps) {
  if (keys == keyOpt) {
    screenSetup(&screenAudioSettings, 0);
    return 1;
  }
  // The meter column is display-only: horizontal moves step over it
  // directly. The intercept runs before the framework navigation - its
  // dead-cell recovery would move the cursor up a row instead of aside -
  // and repaints the affected cells itself (row/col headers are no-ops).
  if (isKeyDown && screenAudioInputData.cursorRow > 0) {
    const int fromCol = screenAudioInputData.cursorCol;
    int toCol = -1;
    if (fromCol == 0 && keys == keyRight) toCol = 2;
    else if (fromCol == 2 && keys == keyLeft) toCol = 0;
    if (toCol >= 0) {
      const int row = screenAudioInputData.cursorRow;
      screenAudioInputData.cursorCol = toCol;
      drawField(fromCol, row, CellState::normal);
      drawField(toCol, row, CellState::focus);
      drawCursor(toCol, row);
      return 1;
    }
  }
  return screenInput(&screenAudioInputData, isKeyDown, keys, taps);
}

static ScreenPlaybackLevel audioInputPlaybackLevel(void) { return ScreenPlaybackLevel::song; }

const AppScreen screenAudioInput = {
  .init = NULL,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = audioInputScreenInput,
  .getPlaybackLevel = audioInputPlaybackLevel,
};
