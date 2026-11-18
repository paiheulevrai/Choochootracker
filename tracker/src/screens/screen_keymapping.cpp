#include "screen_keymapping.h"
#include "screen_settings.h"
#include "corelib_gfx.h"
#include "corelib_file.h"
#include "common.h"
#include "screens.h"
#include "corelib_input.h"
#include "corelib_keymap.h"
#include "app.h"
#include "console_input.h"
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
static constexpr int doneRow = 14;
static KeyMapping candidateMapping;
static KeyMapping editorMapping;
static bool editorVerified = true;
static bool saveAfterVerification = false;
static InputCode heldInputs[16];
static int heldCount = 0;
static uint32_t lastInputTime, stepStartTime;
static uint32_t pressStartTime;
static constexpr uint32_t releaseQuietMs = 150;
static constexpr uint32_t stepTimeoutMs = 15000;
static constexpr uint32_t restartHoldMs = 1000;
static bool calibrating = false;
static bool startupRecovery = false;
static bool migrationRecovery = false;
static bool recoveryFile = false;
struct VerificationStep {
  int row;
  InputCode required;
};
static VerificationStep verificationSteps[8 + 13 * 3];
static int verificationCount = 0;
static int verifyStep = 0;
static bool verifyConflict = false;
static char recoveryPath[PATH_LENGTH + 32];
static InputCode resetInput = {InputDeviceType::none, 0};
static bool resetHeld = false;
static int resetTaps = 0;
static uint32_t resetTime;
#else
static constexpr int doneRow = 13;
static InputCode captureTrigger = {InputDeviceType::none, 0};
static InputCode captureIgnore = {InputDeviceType::none, 0};
#endif
static void fullRedraw(void);
static void applyPendingCapture(void);
#ifdef CCT_CONSOLE_CONTROLS
static void startCapture(bool calibration, bool keepHeld = false);
#endif

static const char* buttonNames[] = {
  "Up", "Down", "Left", "Right", "Edit (A)", "Opt (B)", "Play", "Shift", "Stick live", "Motion rec", "Motion del", "Undo", "Redo"
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
    case 11: return &mapping.keyUndo[col];
    case 12: return &mapping.keyRedo[col];
  }
  return NULL;
}

static InputCode* getKeySlot(int row, int col) {
#ifdef CCT_CONSOLE_CONTROLS
  return getKeySlot(captureState == STATE_NAVIGATION ? editorMapping : candidateMapping, row, col);
#else
  return getKeySlot(appSettings.keyMapping, row, col);
#endif
}

static bool sameInput(InputCode first, InputCode second) {
  return first.deviceType == second.deviceType && first.code == second.code;
}

#ifdef CCT_CONSOLE_CONTROLS
void screenKeyMappingCancel(void) {
  captureState = STATE_NAVIGATION;
  heldCount = 0;
  pendingCapture = {InputDeviceType::none, 0};
  resetInput = {InputDeviceType::none, 0};
  resetHeld = false;
  resetTaps = 0;
  editorMapping = appSettings.keyMapping;
  editorVerified = true;
  saveAfterVerification = false;
  inputRawCallback = NULL;
  appResetInputState();
}

static void cancelCapture(const char* message) {
  KeyMapping staged = editorMapping;
  bool verified = editorVerified;
  screenKeyMappingCancel();
  editorMapping = staged;
  editorVerified = verified;
  if (migrationRecovery) startCapture(true);
  else if (startupRecovery) screenSetup(&screenTitle, 0);
  else fullRedraw();
  screenMessage(MESSAGE_TIME_ERROR, "%s", message);
}

static int mappedRow(InputCode input) {
  // Match the app's priority: motion controls are handled before navigation.
  const int motionRows[] = {11, 12, 9, 8, 10};
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
  verificationCount = 8;
  for (int row = 0; row < 8; ++row)
    verificationSteps[row] = {row, {InputDeviceType::none, 0}};
  if (!calibrating || !startupRecovery) {
    for (int row = calibrating ? 8 : 0; row < 13; ++row) {
      for (int col = 0; col < 3; ++col) {
        InputCode input = *getKeySlot(candidateMapping, row, col);
        if (input.deviceType == InputDeviceType::none) continue;
        bool previouslyMapped = false;
        for (int oldCol = 0; oldCol < 3; ++oldCol) {
          if (sameInput(input, consoleInput::normalize(*getKeySlot(appSettings.keyMapping, row, oldCol))))
            previouslyMapped = true;
        }
        if (previouslyMapped) continue;
        if (row < 8 && verificationSteps[row].required.deviceType == InputDeviceType::none)
          verificationSteps[row].required = input;
        else
          verificationSteps[verificationCount++] = {row, input};
      }
    }
  }
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
    pendingCapture = input;
    captureState = STATE_CAPTURE_DONE;
  } else if (captureState == STATE_VERIFYING || captureState == STATE_VERIFY_DONE) {
    const VerificationStep& expected = verificationSteps[verifyStep];
    int row = mappedRow(input);
    if (row >= 0 && row != expected.row) {
      verifyConflict = true;
      captureState = STATE_VERIFYING;
      screenMessage(MESSAGE_TIME_ERROR, "Wrong control; release and try again");
    } else if (row == expected.row && !verifyConflict) {
      if (expected.required.deviceType != InputDeviceType::none &&
          !sameInput(input, expected.required))
        screenMessage(MESSAGE_TIME_ERROR, "Test the newly assigned key or button");
      else captureState = STATE_VERIFY_DONE;
    }
  }
}

static void startCapture(bool calibration, bool keepHeld) {
  calibrating = calibration;
  candidateMapping = editorMapping;
  saveAfterVerification = false;
  verifyConflict = false;
  if (calibration) {
    for (int row = 0; row < 13; ++row)
      for (int col = 0; col < 3; ++col)
        *getKeySlot(candidateMapping, row, col) =
          consoleInput::normalize(*getKeySlot(candidateMapping, row, col));
    captureRow = captureCol = 0;
    for (int row = 0; row < 8; ++row)
      for (int col = 0; col < 3; ++col)
        *getKeySlot(candidateMapping, row, col) = {InputDeviceType::none, 0};
    bool cleared = false;
    for (int row = 8; row < 13; ++row) {
      for (int col = 0; col < 3; ++col) {
        const InputCode input = *getKeySlot(candidateMapping, row, col);
        if (consoleInput::aliasIndex(input) < 0) continue;
        bool conflict = false;
        for (int other = row + 1; other < 13; ++other)
          for (int slot = 0; slot < 3; ++slot)
            if (sameInput(input, *getKeySlot(candidateMapping, other, slot))) conflict = true;
        if (!conflict) continue;
        for (int other = row; other < 13; ++other)
          for (int slot = 0; slot < 3; ++slot)
            if (sameInput(input, *getKeySlot(candidateMapping, other, slot)))
              *getKeySlot(candidateMapping, other, slot) = {InputDeviceType::none, 0};
        cleared = true;
      }
    }
    if (cleared) screenMessage(MESSAGE_TIME_ERROR, "Conflicting Motion/Undo bindings cleared");
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
  if (row < 13) return 3;
  return 1;
}

static void drawStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 0, "Key Mapping");

  gfxSetFgColor(appSettings.colorScheme.textLabel);
  for (int i = 0; i < 13; i++) {
    gfxPrint(0, i + 2, buttonNames[i]);
  }

  if (captureState != STATE_NAVIGATION) {
    gfxSetFgColor(appSettings.colorScheme.textValue);
#ifdef CCT_CONSOLE_CONTROLS
    if (captureState == STATE_WAIT_RELEASE || captureState == STATE_WAIT_VERIFY)
      gfxPrint(0, 17, "Release all buttons... (15s)");
    else if (captureState == STATE_VERIFYING || captureState == STATE_VERIFY_DONE) {
      const VerificationStep& step = verificationSteps[verifyStep];
      if (step.required.deviceType == InputDeviceType::none)
        gfxPrintf(0, 17, "Test: press %s (15s)", buttonNames[step.row]);
      else
        gfxPrintf(0, 17, "Test: %s (%s)", buttonNames[step.row], inputGetKeyName(step.required));
    }
    else if (calibrating) gfxPrintf(0, 17, "Press physical %s (15s)", buttonNames[captureRow]);
    else gfxPrint(0, 17, "Press new key...");
    gfxPrint(0, 18, calibrating ? "Hold any button 1s: restart" : "15s per step or test cancels");
#else
    gfxPrint(0, 17, "Press any key...");
#endif
  } else {
    gfxSetFgColor(appSettings.colorScheme.textInfo);
    gfxPrint(0, 17, "Tap unmapped key 5x");
    gfxPrint(0, 18, "to reset defaults");
  }
}

static void drawCursor(int col, int row) {
  if (row < 13) {
    InputCode* slot = getKeySlot(row, col);
    int width = slot ? strlen(inputGetKeyName(*slot)) : 3;
    gfxCursor(11 + col * 8, row + 2, width);
  } else {
    gfxCursor(0, row + 2, row == doneRow ? 4 : 18);
  }
}

static void drawField(int col, int row, CellState state) {
  if (row < 13) {
    gfxClearRect(11 + col * 8, row + 2, 7, 1);
    InputCode* slot = getKeySlot(row, col);
    if (slot) {
      int isEmpty = (slot->deviceType == InputDeviceType::none);
      if (state == CellState::focus) {
        gfxSetFgColor(appSettings.colorScheme.cursor);
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
    gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.cursor : appSettings.colorScheme.textDefault);
    gfxPrint(0, row + 2, row == doneRow ? "Done" : "Calibrate controls");
  }
}

static void drawRowHeader(int row, CellState state) {
}

static void drawColHeader(int col, CellState state) {
}

#ifdef CCT_CONSOLE_CONTROLS
static int saveEditorMapping(void) {
  KeyMapping previous = appSettings.keyMapping;
  appSettings.keyMapping = editorMapping;
  if (settingsSave() != 0) {
    appSettings.keyMapping = previous;
    fullRedraw();
    screenMessage(MESSAGE_TIME_ERROR, "Could not save controls");
    return 1;
  }
  appResetInputState();
  screenSetup(&screenControlsSettings, 0);
  return 1;
}
#endif

static int onEdit(int col, int row, CellEditAction action) {
  if (captureState != STATE_NAVIGATION) return 1;
  if (row < 13) {
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
    } else if (action == CellEditAction::clear && (col > 0 || row >= 8)) {
#ifdef CCT_CONSOLE_CONTROLS
      InputCode* slot = getKeySlot(editorMapping, row, col);
      if (slot) {
        slot->deviceType = InputDeviceType::none;
        slot->code = 0;
      }
      editorVerified = false;
      fullRedraw();
#else
      *getKeySlot(row, col) = {InputDeviceType::none, 0};
#endif
      return 1;
    }
  } else if (row == doneRow && action == CellEditAction::tap) {
#ifdef CCT_CONSOLE_CONTROLS
    if (!editorVerified) {
      candidateMapping = editorMapping;
      if (!consoleInput::migrate(candidateMapping)) {
        screenMessage(MESSAGE_TIME_ERROR, "Conflicting controls; change them first");
        return 1;
      }
      calibrating = false;
      saveAfterVerification = true;
      heldCount = 0;
      inputRawCallback = onRawInput;
      beginVerification();
      return 1;
    }
    return saveEditorMapping();
#else
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
#endif
#ifdef CCT_CONSOLE_CONTROLS
  } else if (row == 13 && action == CellEditAction::tap) {
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
  startupRecovery = input != 0;
  migrationRecovery = (input & 2) != 0;
  recoveryFile = (input & 1) != 0;
  if (migrationRecovery)
    screenMessage(MESSAGE_TIME_ERROR, "Conflicting controls; calibrate to keep changes");
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
    // Find conflicting slot and swap
    for (int row = 0; row < 13; row++) {
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
#ifdef CCT_CONSOLE_CONTROLS
  if (calibrating && ++captureRow < 8) {
    captureState = STATE_CAPTURING;
    stepStartTime = mainLoopGetTicks();
    fullRedraw();
  } else {
    if (calibrating) {
      beginVerification();
    } else {
      if (!consoleInput::migrate(candidateMapping)) {
        cancelCapture("Conflicting controls; change cancelled");
        return;
      }
      KeyMapping staged = candidateMapping;
      screenKeyMappingCancel();
      editorMapping = staged;
      editorVerified = false;
      fullRedraw();
      screenMessage(MESSAGE_TIME, "Change staged; Done to test and save");
    }
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
  if (captureState == STATE_NAVIGATION) {
    if (migrationRecovery) startCapture(true);
    return;
  }
  uint32_t now = mainLoopGetTicks();
  if (calibrating && captureState != STATE_WAIT_RELEASE && heldCount &&
      now - pressStartTime >= restartHoldMs) {
    startCapture(true, true);
    return;
  }
  if (now - stepStartTime >= stepTimeoutMs) {
    cancelCapture("Timed out; active controls unchanged");
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
    if (++verifyStep < verificationCount) {
      captureState = STATE_VERIFYING;
      stepStartTime = now;
      fullRedraw();
    } else {
      KeyMapping previous = appSettings.keyMapping;
      if (!consoleInput::migrate(candidateMapping)) {
        cancelCapture("Conflicting controls; calibrate again");
        return;
      }
      if (startupRecovery) appSettings.keyMapping = candidateMapping;
      if (startupRecovery && settingsSave() != 0) {
        appSettings.keyMapping = previous;
        cancelCapture("Could not save controls; try again");
        return;
      }
      migrationRecovery = false;
      bool shouldSave = saveAfterVerification;
      KeyMapping staged = candidateMapping;
      screenKeyMappingCancel();
      if (startupRecovery) {
        int deleted = recoveryFile ? fileDelete(recoveryPath) : 0;
        screenSetup(&screenTitle, 0);
        screenMessage(deleted ? MESSAGE_TIME_ERROR : MESSAGE_TIME,
                      deleted ? "Controls saved; remove configure-controls" : "Controls calibrated and saved");
      } else {
        editorMapping = staged;
        editorVerified = true;
        if (shouldSave) {
          saveEditorMapping();
          return;
        }
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
    editorMapping = appSettings.keyMapping;
    appSettings.keyMapping = previous;
    editorVerified = false;
    fullRedraw();
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
