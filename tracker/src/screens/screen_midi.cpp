#include "screen_midi.h"
#include "screen_settings.h"
#include "screen_midi_channel_map.h"
#include "common.h"
#include "corelib_gfx.h"
#include "corelib_input.h"
#include "screens.h"
#include "midi/midi_router.h"
#include <string.h>

static int columnCount(int row) {
  return 1;
}

// Keep the device value close to its short row label so the fixed 40-column
// screen has room for real Android names such as Novation/Focusrite ports.
#define DEVICE_FIELD_X (10)
#define DEVICE_FIELD_WIDTH (40 - DEVICE_FIELD_X)

// Truncates src to at most maxLen visible characters, replacing the tail
// with "..." when it doesn't fit, rather than letting a long device name
// overwrite neighboring rows or wrap past the field.
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

// isInput selects which port list to look up (input ports for MIDI In,
// output ports for MIDI Out). deviceIndex < 0 with no savedName shows "OFF";
// deviceIndex < 0 with a savedName (a configured device that couldn't be
// resolved to a live port at startup, or was unplugged since) shows "NOT
// FOUND" instead, rather than silently leaving no indication anything is
// misconfigured. The result is already clipped to DEVICE_FIELD_WIDTH.
static void midiDeviceLabel(int isInput, int deviceIndex, const char* savedName, char* buffer, int bufferSize) {
  int count = isInput ? midiRouterInputPortCount() : midiRouterOutputPortCount();
  char fullName[128];
  if (deviceIndex < 0 || deviceIndex >= count ||
      (isInput ? midiRouterInputPortName(deviceIndex, fullName, sizeof(fullName)) : midiRouterOutputPortName(deviceIndex, fullName, sizeof(fullName))) != 0) {
    snprintf(buffer, bufferSize, savedName && savedName[0] ? "NOT FOUND" : "OFF");
    return;
  }
  truncateWithEllipsis(fullName, buffer, bufferSize - 1 < DEVICE_FIELD_WIDTH ? bufferSize - 1 : DEVICE_FIELD_WIDTH);
}

static void drawStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 0, "MIDI");
}

static void drawCursor(int col, int row) {
  if ((row == 0 || row == 1) && col == 0) {
    char name[DEVICE_FIELD_WIDTH + 1];
    midiDeviceLabel(row == 0, row == 0 ? appSettings.midiInputDevice : appSettings.midiOutputDevice,
      row == 0 ? appSettings.midiInputDeviceName : appSettings.midiOutputDeviceName, name, sizeof(name));
    gfxCursor(DEVICE_FIELD_X, 2 + row, (int)strlen(name));
  } else if ((row == 2 || row == 3) && col == 0) {
    gfxCursor(0, 5 + row - 2, (int)strlen(row == 2 ? "Channel mapping" : "MIDI CC mapping"));
  }
}

static void drawRowHeader(int row, CellState state) {
}

static void drawColHeader(int col, CellState state) {
}

static void drawField(int col, int row, CellState state) {
  if (row == 0 && col == 0) {
    gfxSetFgColor(appSettings.colorScheme.textDefault);
    gfxPrint(0, 2, "MIDI In");
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    char name[DEVICE_FIELD_WIDTH + 1];
    midiDeviceLabel(1, appSettings.midiInputDevice, appSettings.midiInputDeviceName, name, sizeof(name));
    gfxClearRect(DEVICE_FIELD_X, 2, DEVICE_FIELD_WIDTH, 1);
    gfxPrint(DEVICE_FIELD_X, 2, name);
  } else if (row == 1 && col == 0) {
    gfxSetFgColor(appSettings.colorScheme.textDefault);
    gfxPrint(0, 3, "MIDI Out");
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    char name[DEVICE_FIELD_WIDTH + 1];
    midiDeviceLabel(0, appSettings.midiOutputDevice, appSettings.midiOutputDeviceName, name, sizeof(name));
    gfxClearRect(DEVICE_FIELD_X, 3, DEVICE_FIELD_WIDTH, 1);
    gfxPrint(DEVICE_FIELD_X, 3, name);
  } else if (row == 2 && col == 0) {
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    gfxPrint(0, 5, "Channel mapping");
  } else if (row == 3 && col == 0) {
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    gfxPrint(0, 6, "MIDI CC mapping");
  }
}

static int onEdit(int col, int row, CellEditAction action) {
  if ((row == 0 || row == 1) && col == 0) {
    action = convertMultiAction(action);
    int direction = (action == CellEditAction::increase || action == CellEditAction::increaseBig) ? 1 :
                    (action == CellEditAction::decrease || action == CellEditAction::decreaseBig) ? -1 : 0;
    if (!direction) return 0;
    int isInput = row == 0;
    int count = isInput ? midiRouterInputPortCount() : midiRouterOutputPortCount();
    int* device = isInput ? &appSettings.midiInputDevice : &appSettings.midiOutputDevice;
    *device += direction;
    if (*device < -1) *device = count - 1;
    if (*device >= count) *device = -1;
    if (isInput) {
      // A note held across the switch must not leave a phantom entry in the
      // legato held-note stack once the (possibly different) device resumes.
      midiRouterResetHeldNotes(chipnomadState->midiRouter);
      if (*device < 0) {
        midiRouterCloseInput();
        appSettings.midiInputDeviceName[0] = '\0';
      } else if (midiRouterOpenInput(*device) != 0) {
        *device = -1; // Transient failure to open - leave any saved name alone.
      } else {
        midiRouterInputPortName(*device, appSettings.midiInputDeviceName, sizeof(appSettings.midiInputDeviceName));
      }
    } else {
      // Flush any still-sounding notes on the port we're about to leave -
      // once it's closed/switched, a Note Off can no longer reach it.
      chipnomadMidiPanic(chipnomadState);
      if (*device < 0) {
        midiRouterCloseOutput();
        appSettings.midiOutputDeviceName[0] = '\0';
      } else if (midiRouterOpenOutput(*device) != 0) {
        *device = -1;
      } else {
        midiRouterOutputPortName(*device, appSettings.midiOutputDeviceName, sizeof(appSettings.midiOutputDeviceName));
      }
    }
    return 1;
  } else if (row == 2 && col == 0 && action == CellEditAction::tap) {
    screenSetup(&screenMidiChannelMap, 0);
    return 0;
  } else if (row == 3 && col == 0 && action == CellEditAction::tap) {
    extern const AppScreen screenMidiCC;
    screenSetup(&screenMidiCC, 0);
    return 0;
  }
  return 0;
}

static ScreenData screenMidiData = {
  .rows = 4,
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
  .isCellValid = NULL,
  .getLoopRange = NULL,
};

static int displayedDroppedCount = -1;
static char displayedMidiStatus[41] = {};

static void setup(int input) {
  screenMidiData.cursorRow = 0;
  screenMidiData.cursorCol = 0;
  displayedDroppedCount = -1;
  displayedMidiStatus[0] = '\0';
  midiRouterRequestAccess();
}

static void fullRedraw(void) {
  screenFullRedraw(&screenMidiData);
}

static void draw(void) {
  const char* status = midiRouterStatusMessage();
  if (!status) status = "";
  if (strcmp(status, displayedMidiStatus) != 0) {
    strncpy(displayedMidiStatus, status, 40);
    displayedMidiStatus[40] = '\0';
    gfxSetFgColor(appSettings.colorScheme.warning);
    gfxClearRect(0, 7, 40, 1);
    if (displayedMidiStatus[0]) gfxPrint(0, 7, displayedMidiStatus);
  }

  // Dropped count is monotonic (see midiRouterGetDroppedCount) and only
  // means something went wrong at some point this session - once shown it
  // stays shown, rather than disappearing again on its own.
  int dropped = (int)midiRouterGetDroppedCount();
  if (displayedMidiStatus[0] || dropped == displayedDroppedCount || dropped == 0) return;
  displayedDroppedCount = dropped;
  gfxSetFgColor(appSettings.colorScheme.warning);
  gfxClearRect(0, 7, 40, 1);
  gfxPrintf(0, 7, "MIDI out queue dropped %d msg%s", dropped, dropped == 1 ? "" : "s");
}

static int inputScreenNavigation(int keys, int tapCount) {
  if (keys == keyOpt) {
    screenSetup(&screenSettings, 0);
    return 1;
  }
  return 0;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (inputScreenNavigation(keys, tapCount)) return 1;
  return screenInput(&screenMidiData, isKeyDown, keys, tapCount);
}

static ScreenPlaybackLevel getPlaybackLevel(void) { return ScreenPlaybackLevel::song; }

const AppScreen screenMidi = {
  .init = NULL,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = getPlaybackLevel,
};
