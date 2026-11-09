#include "screen_keymapping.h"
#include "screen_settings.h"
#include "corelib_gfx.h"
#include "corelib_file.h"
#include "common.h"
#include "screens.h"
#include "corelib_input.h"
#include "corelib_keymap.h"
#include "app.h"
#include <errno.h>
#include <string.h>

typedef enum {
  STATE_NAVIGATION,
  STATE_WAIT_RELEASE,
  STATE_WAIT_VERIFY,
  STATE_CAPTURING,
  STATE_CAPTURE_DONE,
  STATE_VERIFYING,
  STATE_VERIFY_DONE
} CaptureState;

static int captureRow = 0;
static int captureCol = 0;
static CaptureState captureState = STATE_NAVIGATION;
static InputCode pendingCapture = {InputDeviceType::none, 0};
#ifdef CCT_CONSOLE_CONTROLS
static constexpr int doneRow = 12;
static InputCode requiredCapture = {InputDeviceType::none, 0};
static KeyMapping candidateMapping;
static InputCode heldInputs[16];
static int heldCount = 0;
static uint32_t lastInputTime, stepStartTime;
static uint32_t pressStartTime;
static constexpr uint32_t releaseQuietMs = 150;
static constexpr uint32_t stepTimeoutMs = 15000;
static constexpr uint32_t restartHoldMs = 1000;
static bool calibrating = false;
static bool startupRecovery = false;
static int verifyStep = 0;
static bool verifyConflict = false;
static int extraVerifyRow = -1;
static char recoveryPath[PATH_LENGTH + 32];
static InputCode resetInput = {InputDeviceType::none, 0};
static bool resetHeld = false;
static int resetTaps = 0;
static uint32_t resetTime;
#else
static constexpr int doneRow = 11;
static InputCode captureTrigger = {InputDeviceType::none, 0};
static InputCode captureIgnore = {InputDeviceType::none, 0};
#endif
static void fullRedraw(void);
static void applyPendingCapture(void);

static const char* buttonNames[] = {
  "Up", "Down", "Left", "Right", "Edit (A)", "Opt (B)", "Play", "Shift", "Stick live", "Motion rec", "Motion del"
};

static InputCode* getKeySlot(KeyMapping& mapping, int row, int col) {
  switch (row) {
    case 0: return &mapping.keyUp[col];
    case 1: return &mapping.keyDown[col];
    case 2: return &mapping.keyLeft[col];
    case 3: return &mapping.keyRight[col];
    case 4: return &mapping.keyEdit[col];
    case 5: return &mapping.keyOpt[col];
    case 6: return &mapping.keyPlay[col];
    case 7: return &mapping.keyShift[col];
    case 8: return &mapping.keyMotionLive[col];
    case 9: return &mapping.keyMotionRecord[col];
    case 10: return &mapping.keyMotionErase[col];
  }
  return NULL;
}

static InputCode* getKeySlot(int row, int col) {
#ifdef CCT_CONSOLE_CONTROLS
  return getKeySlot(captureState == STATE_NAVIGATION ? appSettings.keyMapping : candidateMapping, row, col);
#else
  return getKeySlot(appSettings.keyMapping, row, col);
#endif
}

#ifdef PORTMASTER_BUILD
// PortMaster normally delivers every physical control twice: once through
// SDL's controller API and once through gptokeyb as a keyboard key.  Keep
// saved mappings keyboard-based, but use the controller event as a reliable
// fallback when Knulli does not deliver the keyboard event to the capture UI.
static int portMasterGamepadToKeyboard(InputCode input, InputCode* output) {
  const char* nextui = getenv("CCT_NEXTUI");
  if (nextui && strcmp(nextui, "1") == 0) return 0;
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

static bool sameInput(InputCode first, InputCode second) {
  return first.deviceType == second.deviceType && first.code == second.code;
}

#ifdef CCT_CONSOLE_CONTROLS
void screenKeyMappingCancel(void) {
  captureState = STATE_NAVIGATION;
  heldCount = 0;
  pendingCapture = {InputDeviceType::none, 0};
  requiredCapture = {InputDeviceType::none, 0};
  resetInput = {InputDeviceType::none, 0};
  resetHeld = false;
  resetTaps = 0;
  inputRawCallback = NULL;
  appResetInputState();
}

static void cancelCapture(const char* message) {
  screenKeyMappingCancel();
  if (startupRecovery) screenSetup(&screenTitle, 0);
  else fullRedraw();
  screenMessage(MESSAGE_TIME_ERROR, "%s", message);
}

static int mappedRow(InputCode input) {
  // Match the app's priority: motion controls are handled before navigation.
  const int motionRows[] = {9, 8, 10};
  for (int row : motionRows) {
    for (int col = 0; col < 3; ++col)
      if (sameInput(*getKeySlot(candidateMapping, row, col), input)) return row;
  }
  for (int col = 0; col < 3; ++col)
    for (int row = 0; row < 8; ++row)
      if (sameInput(*getKeySlot(candidateMapping, row, col), input)) return row;
  return -1;
}

static void beginVerification(void) {
  verifyStep = 0;
  verifyConflict = false;
  captureState = STATE_WAIT_VERIFY;
  InputCode opener = appCurrentInput();
  if (opener.deviceType != InputDeviceType::none && !heldCount) heldInputs[heldCount++] = opener;
  lastInputTime = stepStartTime = mainLoopGetTicks();
  appResetInputState();
  fullRedraw();
}

static void onRawInput(InputCode input, int isDown) {
  lastInputTime = mainLoopGetTicks();
  int index = 0;
  while (index < heldCount && !sameInput(heldInputs[index], input)) ++index;
  if (isDown) {
    if (index < heldCount) return;
    if (heldCount == (int)(sizeof(heldInputs) / sizeof(heldInputs[0]))) {
      cancelCapture("Too many held inputs; try again");
      return;
    }
    if (!heldCount) pressStartTime = lastInputTime;
    heldInputs[heldCount++] = input;
  } else if (index < heldCount) {
    heldInputs[index] = heldInputs[--heldCount];
  }
  if (!isDown) {
    return;
  }
  if (input.deviceType == InputDeviceType::logical || input.deviceType == InputDeviceType::none) {
    screenMessage(MESSAGE_TIME_ERROR, "Use a physical key or pad button");
    return;
  }
  if (captureState == STATE_CAPTURING) {
#ifdef PORTMASTER_BUILD
    InputCode translatedInput;
    if (portMasterGamepadToKeyboard(input, &translatedInput)) input = translatedInput;
#endif
    pendingCapture = input;
    captureState = STATE_CAPTURE_DONE;
  } else if (captureState == STATE_VERIFYING || captureState == STATE_VERIFY_DONE) {
    int expectedRow = verifyStep < 8 ? verifyStep : extraVerifyRow;
    int row = mappedRow(input);
    if (row >= 0 && row != expectedRow) {
      verifyConflict = true;
      captureState = STATE_VERIFYING;
      screenMessage(MESSAGE_TIME_ERROR, "Wrong control; release and try again");
    } else if (row == expectedRow && !verifyConflict) {
      if (expectedRow == captureRow && requiredCapture.deviceType != InputDeviceType::none &&
          !sameInput(input, requiredCapture))
        screenMessage(MESSAGE_TIME_ERROR, "Test the newly assigned key or button");
      else captureState = STATE_VERIFY_DONE;
    }
  }
}

static void startCapture(bool calibration, bool keepHeld = false) {
  calibrating = calibration;
  candidateMapping = appSettings.keyMapping;
  extraVerifyRow = -1;
  requiredCapture = {InputDeviceType::none, 0};
  verifyConflict = false;
  if (calibration) {
    captureRow = captureCol = 0;
    for (int row = 0; row < 8; ++row)
      for (int col = 0; col < 3; ++col)
        *getKeySlot(candidateMapping, row, col) = {InputDeviceType::none, 0};
  }
  if (!keepHeld) {
    heldCount = 0;
    InputCode opener = appCurrentInput();
    if (opener.deviceType != InputDeviceType::none) heldInputs[heldCount++] = opener;
  }
  captureState = STATE_WAIT_RELEASE;
  lastInputTime = stepStartTime = mainLoopGetTicks();
  pendingCapture = {InputDeviceType::none, 0};
  inputRawCallback = onRawInput;
  appResetInputState();
  fullRedraw();
}
#else
static void onRawInput(InputCode input, int isDown) {
  if (!isDown) {
    if (captureState == STATE_CAPTURING && sameInput(input, captureIgnore)) {
      captureIgnore = {InputDeviceType::none, 0};
    } else if (captureState == STATE_CAPTURE_DONE && sameInput(input, captureTrigger)) {
      applyPendingCapture();
      captureState = STATE_NAVIGATION;
      inputRawCallback = NULL;
      fullRedraw();
    }
    return;
  }
  if (captureState != STATE_CAPTURING || sameInput(input, captureIgnore)) return;
  pendingCapture = captureTrigger = input;
  captureState = STATE_CAPTURE_DONE;
}
#endif

static int getColumnCount(int row) {
  if (row < 11) return 3;
  return 1;
}

static void drawStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 0, "Key Mapping");

  gfxSetFgColor(appSettings.colorScheme.textLabel);
  for (int i = 0; i < 11; i++) {
    gfxPrint(0, i + 2, buttonNames[i]);
  }

  if (captureState != STATE_NAVIGATION) {
    gfxSetFgColor(appSettings.colorScheme.textValue);
#ifdef CCT_CONSOLE_CONTROLS
    if (captureState == STATE_WAIT_RELEASE || captureState == STATE_WAIT_VERIFY)
      gfxPrint(0, 16, "Release all buttons... (15s)");
    else if (captureState == STATE_VERIFYING || captureState == STATE_VERIFY_DONE)
      gfxPrintf(0, 16, "Test: press %s (15s)", buttonNames[verifyStep < 8 ? verifyStep : extraVerifyRow]);
    else if (calibrating) gfxPrintf(0, 16, "Press physical %s (15s)", buttonNames[captureRow]);
    else gfxPrint(0, 16, "Press new key...");
    gfxPrint(0, 17, calibrating ? "Hold any button 1s: restart" : "15s per step or changes cancel");
#else
    gfxPrint(0, 16, "Press any key...");
#endif
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
  } else {
    gfxCursor(0, row + 2, row == doneRow ? 4 : 18);
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
  } else {
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
    gfxPrint(0, row + 2, row == doneRow ? "Done" : "Calibrate controls");
  }
}

static void drawRowHeader(int row, CellState state) {
}

static void drawColHeader(int col, CellState state) {
}

static int onEdit(int col, int row, CellEditAction action) {
  if (captureState != STATE_NAVIGATION) return 1;
  if (row < 11) {
    if (action == CellEditAction::tap) {
      captureRow = row;
      captureCol = col;
#ifdef CCT_CONSOLE_CONTROLS
      startCapture(false);
#else
      captureState = STATE_CAPTURING;
      captureIgnore = appCurrentInput();
      inputRawCallback = onRawInput;
      fullRedraw();
#endif
      return 1;
    } else if (action == CellEditAction::clear && col > 0) {
#ifdef CCT_CONSOLE_CONTROLS
      candidateMapping = appSettings.keyMapping;
      InputCode* slot = getKeySlot(candidateMapping, row, col);
      if (slot) {
        slot->deviceType = InputDeviceType::none;
        slot->code = 0;
      }
      heldCount = 0;
      extraVerifyRow = -1;
      inputRawCallback = onRawInput;
      lastInputTime = mainLoopGetTicks();
      beginVerification();
#else
      *getKeySlot(row, col) = {InputDeviceType::none, 0};
#endif
      return 1;
    }
  } else if (row == doneRow && action == CellEditAction::tap) {
    if (settingsSave() != 0) {
      screenMessage(MESSAGE_TIME_ERROR, "Could not save controls");
      return 1;
    }
    // The key that confirms Done can have just been remapped. Do not carry
    // its old logical state into Settings: on gptokeyb/PortMaster the release
    // may arrive after this mapping has changed.
    appResetInputState();
    screenSetup(&screenControlsSettings, 0);
    return 1;
#ifdef CCT_CONSOLE_CONTROLS
  } else if (row == 11 && action == CellEditAction::tap) {
    startCapture(true);
    return 1;
#endif
  }

  return 0;
}

static ScreenData screenKeyMappingData = {
  .rows = doneRow + 1,
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
#ifdef CCT_CONSOLE_CONTROLS
  screenKeyMappingCancel();
  startupRecovery = input == 1;
  if (startupRecovery) startCapture(true);
#else
  captureState = STATE_NAVIGATION;
  inputRawCallback = NULL;
  captureTrigger = captureIgnore = {InputDeviceType::none, 0};
#endif
}

static void fullRedraw(void) {
  screenFullRedraw(&screenKeyMappingData);
}

static void draw(void) {
}

static void applyPendingCapture(void) {
#ifdef CCT_CONSOLE_CONTROLS
  InputCode* slot = getKeySlot(candidateMapping, captureRow, captureCol);
#else
  InputCode* slot = getKeySlot(captureRow, captureCol);
#endif
  if (slot && pendingCapture.deviceType != InputDeviceType::none) {
#ifdef CCT_CONSOLE_CONTROLS
    if (calibrating) {
      for (int row = 0; row < captureRow; ++row)
        if (sameInput(*getKeySlot(candidateMapping, row, 0), pendingCapture)) {
          captureState = STATE_CAPTURING;
          stepStartTime = mainLoopGetTicks();
          screenMessage(MESSAGE_TIME_ERROR, "Already used; press another control");
          return;
        }
    }
#endif
    InputCode previous = *slot;
#ifdef CCT_CONSOLE_CONTROLS
    bool conflictsOtherRow = false;
    for (int row = 0; row < 11; ++row)
      for (int col = 0; col < 3; ++col)
        if (row != captureRow && sameInput(*getKeySlot(candidateMapping, row, col), pendingCapture))
          conflictsOtherRow = true;

    if (!calibrating && conflictsOtherRow && previous.deviceType != pendingCapture.deviceType) {
      // Swap within the captured device path, not a keyboard key into a pad slot.
      InputCode* deviceSlot = NULL;
      for (int col = 0; col < 3; ++col) {
        InputCode* other = getKeySlot(candidateMapping, captureRow, col);
        if (col != captureCol && other->deviceType == pendingCapture.deviceType &&
            !sameInput(*other, pendingCapture)) {
          deviceSlot = other;
          break;
        }
      }
      if (!deviceSlot) {
        cancelCapture("Use Calibrate controls for this swap");
        return;
      }
      InputCode displaced = *deviceSlot;
      *deviceSlot = previous;
      previous = displaced;
    }
#endif

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
#ifdef CCT_CONSOLE_CONTROLS
    if (!calibrating) requiredCapture = pendingCapture;
#endif
  }
  pendingCapture = (InputCode){InputDeviceType::none, 0};
#ifdef CCT_CONSOLE_CONTROLS
  if (calibrating && ++captureRow < 8) {
    captureState = STATE_CAPTURING;
    stepStartTime = mainLoopGetTicks();
    fullRedraw();
  } else {
    extraVerifyRow = !calibrating && captureRow >= 8 ? captureRow : -1;
    beginVerification();
  }
#else
  appResetInputState();
#endif
}

#ifdef CCT_CONSOLE_CONTROLS
void screenKeyMappingTick(void) {
  if (currentScreen != &screenKeyMapping) {
    if (inputRawCallback == onRawInput) screenKeyMappingCancel();
    return;
  }
  if (captureState == STATE_NAVIGATION) return;
  uint32_t now = mainLoopGetTicks();
  if (calibrating && captureState != STATE_WAIT_RELEASE && heldCount &&
      now - pressStartTime >= restartHoldMs) {
    startCapture(true, true);
    return;
  }
  if (now - stepStartTime >= stepTimeoutMs) {
    cancelCapture("Controls unchanged: test timed out");
    return;
  }
  if (heldCount || now - lastInputTime < releaseQuietMs) return;
  if (verifyConflict) {
    verifyConflict = false;
    return;
  }
  if (captureState == STATE_WAIT_RELEASE) {
    captureState = STATE_CAPTURING;
    stepStartTime = now;
    fullRedraw();
  } else if (captureState == STATE_WAIT_VERIFY) {
    captureState = STATE_VERIFYING;
    stepStartTime = now;
    fullRedraw();
  } else if (captureState == STATE_CAPTURE_DONE) {
    applyPendingCapture();
  } else if (captureState == STATE_VERIFY_DONE) {
    if (++verifyStep < (extraVerifyRow < 0 ? 8 : 9)) {
      captureState = STATE_VERIFYING;
      stepStartTime = now;
      fullRedraw();
    } else {
      KeyMapping previous = appSettings.keyMapping;
      appSettings.keyMapping = candidateMapping;
      if (startupRecovery && settingsSave() != 0) {
        appSettings.keyMapping = previous;
        cancelCapture("Could not save controls; try again");
        return;
      }
      screenKeyMappingCancel();
      if (startupRecovery) {
        int deleted = fileDelete(recoveryPath);
        screenSetup(&screenTitle, 0);
        screenMessage(deleted ? MESSAGE_TIME_ERROR : MESSAGE_TIME,
                      deleted ? "Controls saved; remove configure-controls" : "Controls calibrated and saved");
      } else {
        fullRedraw();
        screenMessage(MESSAGE_TIME, "Controls tested; Done to save");
      }
    }
  }
}

int screenKeyMappingRecoveryRequested(void) {
  char directory[PATH_LENGTH];
  if (fileGetDefaultDirectory(directory, sizeof(directory)) != 0) {
    fprintf(stderr, "Could not locate controls recovery file\n");
    return 0;
  }
  snprintf(recoveryPath, sizeof(recoveryPath), "%s%sconfigure-controls", directory, PATH_SEPARATOR_STR);
  FILE* file = fopen(recoveryPath, "r");
  if (!file) {
    if (errno != ENOENT) fprintf(stderr, "Could not read controls recovery file: %s\n", strerror(errno));
    return 0;
  }
  fclose(file);
  return 1;
}
#endif

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (captureState != STATE_NAVIGATION) return 1;

  // Keep a reliable exit after remapping Edit and Opt. This is especially
  // useful with PortMaster's keyboard bridge, where a just-swapped pair can
  // otherwise make the expected confirmation button ambiguous.
  if (isKeyDown && screenKeyMappingData.cursorRow == doneRow &&
      (keys == keyEdit || keys == keyOpt)) {
    return onEdit(0, doneRow, CellEditAction::tap);
  }

  // 5 taps on any unmapped key resets to defaults
  if (isKeyDown && keys == keyUnmapped && tapCount >= 5) {
#ifdef CCT_CONSOLE_CONTROLS
    KeyMapping previous = appSettings.keyMapping;
    inputInitDefaultKeyMapping();
    candidateMapping = appSettings.keyMapping;
    appSettings.keyMapping = previous;
    heldCount = 0;
    extraVerifyRow = -1;
    inputRawCallback = onRawInput;
    lastInputTime = mainLoopGetTicks();
    beginVerification();
#else
    inputInitDefaultKeyMapping();
    fullRedraw();
#endif
    return 1;
  }

  return screenInput(&screenKeyMappingData, isKeyDown, keys, tapCount);
}

#ifdef CCT_CONSOLE_CONTROLS
void screenKeyMappingUnmappedInput(InputCode input, int isDown) {
  if (captureState != STATE_NAVIGATION) return;
  uint32_t now = mainLoopGetTicks();
  if (isDown && (resetInput.deviceType == InputDeviceType::none || now - resetTime > 1000)) {
    resetInput = input;
    resetHeld = false;
    resetTaps = 0;
  }
  // Follow one raw input, so its keyboard/controller duplicate cannot add taps.
  if (!sameInput(resetInput, input)) return;
  if (!isDown) {
    resetHeld = false;
  } else if (!resetHeld) {
    resetHeld = true;
    resetTime = now;
    onInput(1, keyUnmapped, ++resetTaps);
  }
}
#endif

const AppScreen screenKeyMapping = {
  .init = NULL,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = NULL,
};
