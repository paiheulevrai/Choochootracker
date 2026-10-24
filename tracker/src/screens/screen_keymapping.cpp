#include "screen_keymapping.h"
#include "screen_settings.h"
#include "corelib_gfx.h"
#include "corelib_file.h"
#include "common.h"
#include "screens.h"
#include "corelib_input.h"
#include "corelib_keymap.h"
#include "app.h"
#include <string.h>

typedef enum {
  STATE_NAVIGATION,
  STATE_CAPTURING,
  STATE_CAPTURE_DONE
} CaptureState;

static int captureRow = 0;
static int captureCol = 0;
static CaptureState captureState = STATE_NAVIGATION;
static InputCode pendingCapture = {InputDeviceType::none, 0};
static InputCode captureTrigger = {InputDeviceType::none, 0};
static InputCode captureIgnore = {InputDeviceType::none, 0};
static void fullRedraw(void);
static void applyPendingCapture(void);

static const char* buttonNames[] = {
  "Up", "Down", "Left", "Right", "Edit (A)", "Opt (B)", "Play", "Shift", "Stick live", "Motion rec", "Motion del"
};

static InputCode* getKeySlot(int row, int col) {
  switch (row) {
    case 0: return &appSettings.keyMapping.keyUp[col];
    case 1: return &appSettings.keyMapping.keyDown[col];
    case 2: return &appSettings.keyMapping.keyLeft[col];
    case 3: return &appSettings.keyMapping.keyRight[col];
    case 4: return &appSettings.keyMapping.keyEdit[col];
    case 5: return &appSettings.keyMapping.keyOpt[col];
    case 6: return &appSettings.keyMapping.keyPlay[col];
    case 7: return &appSettings.keyMapping.keyShift[col];
    case 8: return &appSettings.keyMapping.keyMotionLive[col];
    case 9: return &appSettings.keyMapping.keyMotionRecord[col];
    case 10: return &appSettings.keyMapping.keyMotionErase[col];
  }
  return NULL;
}

#ifdef PORTMASTER_BUILD
// PortMaster normally delivers every physical control twice: once through
// SDL's controller API and once through gptokeyb as a keyboard key.  Keep
// saved mappings keyboard-based, but use the controller event as a reliable
// fallback when Knulli does not deliver the keyboard event to the capture UI.
static int portMasterGamepadToKeyboard(InputCode input, InputCode* output) {
  if (input.deviceType != InputDeviceType::gamepad) return 0;

  int key = 0;
  switch (input.code) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: key = BTN_UP; break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: key = BTN_DOWN; break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: key = BTN_LEFT; break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: key = BTN_RIGHT; break;
    case SDL_CONTROLLER_BUTTON_A: key = BTN_A; break;
    case SDL_CONTROLLER_BUTTON_B: key = BTN_B; break;
    case SDL_CONTROLLER_BUTTON_X: key = BTN_X; break;
    case SDL_CONTROLLER_BUTTON_Y: key = BTN_Y; break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: key = BTN_L1; break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: key = BTN_R1; break;
    case SDL_CONTROLLER_BUTTON_BACK: key = BTN_SELECT; break;
    case SDL_CONTROLLER_BUTTON_START: key = BTN_START; break;
    case gamepadTriggerLeft: key = BTN_L2; break;
    case gamepadTriggerRight: key = BTN_R2; break;
    default: return 0;
  }
  *output = (InputCode){InputDeviceType::keyboard, key};
  return 1;
}
#endif

static int samePhysicalButton(InputCode first, InputCode second) {
  if (first.deviceType == second.deviceType && first.code == second.code) return 1;
#ifdef PORTMASTER_BUILD
  InputCode translated;
  if (portMasterGamepadToKeyboard(first, &translated) &&
      translated.deviceType == second.deviceType && translated.code == second.code) return 1;
  if (portMasterGamepadToKeyboard(second, &translated) &&
      translated.deviceType == first.deviceType && translated.code == first.code) return 1;
#endif
  return 0;
}

static void onRawInput(InputCode input, int isDown) {
  if (!isDown) {
    if (captureState == STATE_CAPTURING && samePhysicalButton(input, captureIgnore)) {
      captureIgnore = (InputCode){InputDeviceType::none, 0};
    } else if (captureState == STATE_CAPTURE_DONE && samePhysicalButton(input, captureTrigger)) {
      applyPendingCapture();
      captureState = STATE_NAVIGATION;
      inputRawCallback = NULL;
      fullRedraw();
    }
    return;
  }
  if (captureState != STATE_CAPTURING) return;
  if (samePhysicalButton(input, captureIgnore)) return;

#ifdef PORTMASTER_BUILD
  InputCode translatedInput;
  if (portMasterGamepadToKeyboard(input, &translatedInput)) {
    pendingCapture = translatedInput;
    captureTrigger = input;
    captureState = STATE_CAPTURE_DONE;
    return;
  }
#endif

  // Logical buttons (e.g. touch vpad) are not remappable
  if (input.deviceType == InputDeviceType::logical) {
    pendingCapture = input;
    captureTrigger = input;
    captureState = STATE_CAPTURE_DONE;
    return;
  }

  pendingCapture = input;
  captureTrigger = input;
  captureState = STATE_CAPTURE_DONE;
}

static int getColumnCount(int row) {
  if (row < 11) return 3;
  return 1;
}

static void drawStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 0, "Key Mapping");

  gfxSetFgColor(appSettings.colorScheme.textDefault);
  for (int i = 0; i < 11; i++) {
    gfxPrint(0, i + 2, buttonNames[i]);
  }

  if (inputRawCallback) {
    gfxSetFgColor(appSettings.colorScheme.textValue);
    gfxPrint(0, 16, "Press any key...");
  } else {
    gfxSetFgColor(appSettings.colorScheme.textInfo);
    gfxPrint(0, 16, "Tap unmapped key 5x");
    gfxPrint(0, 17, "to reset defaults");
  }
}

static void drawCursor(int col, int row) {
  if (row < 11) {
    InputCode* slot = getKeySlot(row, col);
    int width = slot ? strlen(inputGetKeyName(*slot)) : 3;
    gfxCursor(11 + col * 8, row + 2, width);
  } else if (row == 11) {
    gfxCursor(0, 14, 4);
  }
}

static void drawField(int col, int row, CellState state) {
  if (row < 11) {
    gfxClearRect(11 + col * 8, row + 2, 7, 1);
    InputCode* slot = getKeySlot(row, col);
    if (slot) {
      int isEmpty = (slot->deviceType == InputDeviceType::none);
      if (state == CellState::focus) {
        gfxSetFgColor(appSettings.colorScheme.textValue);
      } else if (isEmpty) {
        gfxSetFgColor(appSettings.colorScheme.textEmpty);
      } else {
        gfxSetFgColor(appSettings.colorScheme.textDefault);
      }
      const char* keyName = inputGetKeyName(*slot);
      char trimmed[8];
      strncpy(trimmed, keyName, 7);
      trimmed[7] = '\0';
      gfxPrint(11 + col * 8, row + 2, trimmed);
    }
  } else if (row == 11) {
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    gfxPrint(0, 13, "Done");
  }
}

static void drawRowHeader(int row, CellState state) {
}

static void drawColHeader(int col, CellState state) {
}

static int onEdit(int col, int row, CellEditAction action) {
  if (row < 11) {
    if (action == CellEditAction::tap) {
      captureRow = row;
      captureCol = col;
      captureState = STATE_CAPTURING;
      // The button that opened capture can arrive again through PortMaster's
      // other input path.  Wait for it to be released before accepting a key.
      captureIgnore = appCurrentInput();
      inputRawCallback = onRawInput;
      fullRedraw();
      return 1;
    } else if (action == CellEditAction::clear && col > 0) {
      InputCode* slot = getKeySlot(row, col);
      if (slot) {
        slot->deviceType = InputDeviceType::none;
        slot->code = 0;
      }
      return 1;
    }
  } else if (row == 11 && action == CellEditAction::tap) {
    settingsSave();
    // The key that confirms Done can have just been remapped. Do not carry
    // its old logical state into Settings: on gptokeyb/PortMaster the release
    // may arrive after this mapping has changed.
    appResetInputState();
    screenSetup(&screenControlsSettings, 0);
    return 1;
  }

  return 0;
}

static ScreenData screenKeyMappingData = {
  .rows = 12,
  .cursorRow = 0,
  .cursorCol = 0,
  .topRow = 0,
  .selectMode = -1,
  .selectStartRow = 0,
  .selectStartCol = 0,
  .selectAnchorRow = 0,
  .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = getColumnCount,
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

static void setup(int input) {
  screenKeyMappingData.cursorRow = 0;
  screenKeyMappingData.cursorCol = 0;
  captureState = STATE_NAVIGATION;
  inputRawCallback = NULL;
  captureTrigger = (InputCode){InputDeviceType::none, 0};
  captureIgnore = (InputCode){InputDeviceType::none, 0};
}

static void fullRedraw(void) {
  screenFullRedraw(&screenKeyMappingData);
}

static void draw(void) {
}

static void applyPendingCapture(void) {
  InputCode* slot = getKeySlot(captureRow, captureCol);
  if (slot && pendingCapture.deviceType != InputDeviceType::none) {
    InputCode previous = *slot;

    // Find conflicting slot and swap
    for (int row = 0; row < 11; row++) {
      for (int col = 0; col < 3; col++) {
        if (row == captureRow && col == captureCol) continue;
        InputCode* other = getKeySlot(row, col);
        if (other && other->deviceType == pendingCapture.deviceType && other->code == pendingCapture.code) {
          *other = previous;
        }
      }
    }

    *slot = pendingCapture;
  }
  pendingCapture = (InputCode){InputDeviceType::none, 0};
  // The release event that completes the capture can otherwise be interpreted
  // with the newly changed mapping, leaving a stale logical key held.
  appResetInputState();
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (captureState == STATE_CAPTURING) return 1;
  if (captureState == STATE_CAPTURE_DONE) return 1;

  // Keep a reliable exit after remapping Edit and Opt. This is especially
  // useful with PortMaster's keyboard bridge, where a just-swapped pair can
  // otherwise make the expected confirmation button ambiguous.
  if (isKeyDown && screenKeyMappingData.cursorRow == 11 &&
      (keys == keyEdit || keys == keyOpt)) {
    return onEdit(0, 11, CellEditAction::tap);
  }

  // 5 taps on any unmapped key resets to defaults
  if (isKeyDown && keys == keyUnmapped && tapCount >= 5) {
    inputInitDefaultKeyMapping();
    fullRedraw();
    return 1;
  }

  return screenInput(&screenKeyMappingData, isKeyDown, keys, tapCount);
}

const AppScreen screenKeyMapping = {
  .init = NULL,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = NULL,
};
