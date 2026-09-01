#include <string.h>
#include "corelib_gfx.h"
#include "corelib_font.h"
#include "corelib_file.h"
#include "common.h"
#include "audio_manager.h"
#include "app.h"
#include "screens.h"
#include "chipnomad_lib.h"
#include "project_utils.h"
#include "waveform_display.h"
#include "corelib_input.h"
#include "corelib_keymap.h"
#include "screens/screen_quick_help.h"

#ifdef WEB_BUILD
#include <emscripten/emscripten.h>
#endif

// Raw input callback for key mapping screen
void (*inputRawCallback)(InputCode input, int isDown) = NULL;

// Input handling vars:

/** Currently pressed buttons */
static int pressedButtons;
/** Frame counter for tap detection */
static int tapTimerCount;
/** Button that triggered tap timer */
static int tapButton;
/** Number of taps detected */
static int tapCount;
/** Frame counter for key repeats */
static int keyRepeatCount;
static int motionRecordHeld;
static int motionEraseHeld;
static int motionLiveHeld;
static int quickHelpSelectHeld;
static int quickHelpSelectAlone;
static int audioProjectDirty;

static int applyMotionRecordEvent(const MotionRecordEvent& event) {
  if (event.phrase >= PROJECT_MAX_PHRASES || event.row >= 16 || event.fx >= fxTotalCount) return 0;
  PhraseRow* row = &chipnomadState->project.phrases[event.phrase].rows[event.row];
  int column = -1;
  for (int i = 2; i >= 0; --i) if (row->fx[i][0] == event.fx) { column = i; break; }
  if (event.erase) {
    if (column < 0) return 0;
    row->fx[column][0] = EMPTY_VALUE_8;
    row->fx[column][1] = 0;
    return 1;
  }
  if (column < 0)
    for (int i = 2; i >= 0; --i) if (row->fx[i][0] == EMPTY_VALUE_8) { column = i; break; }
  if (column < 0) {
    chipnomadSetMotionRecordOverflow();
    return 0;
  }
  if (row->fx[column][0] == event.fx && row->fx[column][1] == event.value) return 0;
  row->fx[column][0] = event.fx;
  row->fx[column][1] = event.value;
  return 1;
}

static int isMotionRecordTrigger(InputCode input) {
  for (int i = 0; i < 3; i++)
    if (appSettings.keyMapping.keyMotionRecord[i].deviceType == input.deviceType && appSettings.keyMapping.keyMotionRecord[i].code == input.code) return 1;
  return 0;
}

static int isMotionLiveTrigger(InputCode input) {
  for (int i = 0; i < 3; i++)
    if (appSettings.keyMapping.keyMotionLive[i].deviceType == input.deviceType && appSettings.keyMapping.keyMotionLive[i].code == input.code) return 1;
  return 0;
}

static int isMotionEraseTrigger(InputCode input) {
  for (int i = 0; i < 3; i++)
    if (appSettings.keyMapping.keyMotionErase[i].deviceType == input.deviceType && appSettings.keyMapping.keyMotionErase[i].code == input.code) return 1;
  return 0;
}

static void updateMotionRecordMode(void) {
  chipnomadSetMotionRecordMode(motionRecordHeld, motionEraseHeld);
  chipnomadSetLiveStickEnabled(motionLiveHeld || motionRecordHeld || motionEraseHeld);
}

/**
* @brief Convert InputCode to Key enum value
*
* @param input Input code
* @return Key value or 0 if not recognized
*/
static int inputCodeToKey(InputCode input) {
  // Logical buttons are not remappable
  if (input.deviceType == InputDeviceType::logical) {
    return input.code;
  }

  // Check key mapping for keyboard and gamepad inputs
  for (int i = 0; i < 3; i++) {
    if (appSettings.keyMapping.keyUp[i].deviceType == input.deviceType && appSettings.keyMapping.keyUp[i].code == input.code) return keyUp;
    if (appSettings.keyMapping.keyDown[i].deviceType == input.deviceType && appSettings.keyMapping.keyDown[i].code == input.code) return keyDown;
    if (appSettings.keyMapping.keyLeft[i].deviceType == input.deviceType && appSettings.keyMapping.keyLeft[i].code == input.code) return keyLeft;
    if (appSettings.keyMapping.keyRight[i].deviceType == input.deviceType && appSettings.keyMapping.keyRight[i].code == input.code) return keyRight;
    if (appSettings.keyMapping.keyEdit[i].deviceType == input.deviceType && appSettings.keyMapping.keyEdit[i].code == input.code) return keyEdit;
    if (appSettings.keyMapping.keyOpt[i].deviceType == input.deviceType && appSettings.keyMapping.keyOpt[i].code == input.code) return keyOpt;
    if (appSettings.keyMapping.keyPlay[i].deviceType == input.deviceType && appSettings.keyMapping.keyPlay[i].code == input.code) return keyPlay;
    if (appSettings.keyMapping.keyShift[i].deviceType == input.deviceType && appSettings.keyMapping.keyShift[i].code == input.code) return keyShift;
  }

  // Return keyUnmapped for any input that doesn't match a mapping
  return keyUnmapped;
}

static void applyLoopRange(void) {
  LoopRange range = screenGetLoopRange(currentScreen);
  if (range.enabled) {
    chipnomadQueueLoopRange(chipnomadState, range);
  } else {
    chipnomadQueueClearLoopRange(chipnomadState);
  }
}

/**
* @brief Handle play/stop key commands
*
* @param keys Pressed keys
* @param tapCount number of taps
* @return int 0 - input not handled, 1 - input handled
*/
static int inputPlayback(int keys, int tapCount) {
  if (!chipnomadState) return 0;

  const PlaybackStatus* playback = chipnomadGetPlaybackStatus(chipnomadState);
  int isPlaying = playback->isPlaying;
  ScreenPlaybackLevel playbackLevel = screenGetPlaybackLevel(currentScreen);

  // Play song/chain/phrase depending on the screen's playback level
  if (!isPlaying && keys == keyPlay) {
    if (playbackLevel == ScreenPlaybackLevel::none) {
      return 0; // This screen doesn't support playback
    }

    chipnomadQueuePlaybackStop(chipnomadState);
    LoopRange range = screenGetLoopRange(currentScreen);

    if (playbackLevel == ScreenPlaybackLevel::song) {
      int startRow = range.enabled ? range.startSongRow : *pSongRow;
      chipnomadQueuePlaybackStartSong(chipnomadState, startRow, 0, 1);
      applyLoopRange();
    } else if (playbackLevel == ScreenPlaybackLevel::chain) {
      int startRow = range.enabled ? range.startChainRow : *pChainRow;
      chipnomadQueuePlaybackStartChain(chipnomadState, *pSongTrack, *pSongRow, startRow, 1);
      applyLoopRange();
    } else if (playbackLevel == ScreenPlaybackLevel::phrase) {
      chipnomadQueuePlaybackStartPhrase(chipnomadState, *pSongTrack, *pSongRow, *pChainRow, 1);
      applyLoopRange();
    }
    return 1;
  }
  // Play song from music screens (Shift+Play)
  else if (!isPlaying && keys == (keyPlay | keyShift)) {
    if (playbackLevel == ScreenPlaybackLevel::none) {
      return 0; // This screen doesn't support playback
    }

    chipnomadQueuePlaybackStop(chipnomadState);
    LoopRange range = screenGetLoopRange(currentScreen);

    if (playbackLevel == ScreenPlaybackLevel::song) {
      int startRow = range.enabled ? range.startSongRow : *pSongRow;
      chipnomadQueuePlaybackStartSong(chipnomadState, startRow, 0, 1);
      applyLoopRange();
    } else if (playbackLevel == ScreenPlaybackLevel::chain || playbackLevel == ScreenPlaybackLevel::phrase) {
      int startChainRow = range.enabled ? range.startChainRow : *pChainRow;
      chipnomadQueuePlaybackStartSong(chipnomadState, *pSongRow, startChainRow, 1);
      applyLoopRange();
    }
    return 1;
  }
  // Stop playback
  else if (isPlaying && keys == keyPlay) {
    chipnomadQueuePlaybackStop(chipnomadState);
    return 1;
  }
  return 0;
}

/**
* @brief App input handler. Handles app-wide commands and then forwards the call to the current screen
*
* @param isKeyDown whether this is a key press (1) or key release (0)
* @param keys Pressed buttons
* @param tapCount number of taps
*/
static void appInput(int isKeyDown, int keys, int tapCount) {
  // Stop phrase row and preview
  if (chipnomadGetPlaybackStatus(chipnomadState)->tracks[*pSongTrack].mode == PlaybackMode::phraseRow && keys == 0) {
    chipnomadQueuePlaybackStop(chipnomadState);
  }
  // Let screen handle input first, then try global playback if not handled
  if (!currentScreen->onInput(isKeyDown, keys, tapCount)) {
    if (isKeyDown) {
      inputPlayback(keys, tapCount);
    }
  }
  // The UI owns Project. Coalesce edits into one snapshot for the next audio tick.
  if (isKeyDown) audioProjectDirty = 1;
}


#define AUTOSAVE_INTERVAL_FRAMES (60 * 60) // 1 minute at 60 FPS

static int autosaveCounter = 0;

///////////////////////////////////////////////////////////////////////////////
//

/**
* @brief Initialize the application: setup audio system, load auto-saved project, show the first screen
*/
void appSetup(void) {
  // LOGD("--- ChipNomad started ---");
  // Initialize default key mappings if not loaded from settings
  if (appSettings.keyMapping.keyUp[0].deviceType == InputDeviceType::none) {
    inputInitDefaultKeyMapping();
  }
  if (appSettings.keyMapping.keyMotionLive[0].deviceType == InputDeviceType::none) {
    appSettings.keyMapping.keyMotionLive[0] = (InputCode){InputDeviceType::keyboard, BTN_L1};
    if (appSettings.keyMapping.keyMotionRecord[0].deviceType == InputDeviceType::none ||
        (appSettings.keyMapping.keyMotionRecord[0].code == BTN_R2 && appSettings.keyMapping.keyMotionErase[0].code == BTN_L2)) {
      appSettings.keyMapping.keyMotionRecord[0] = (InputCode){InputDeviceType::keyboard, BTN_L2};
      appSettings.keyMapping.keyMotionErase[0] = (InputCode){InputDeviceType::keyboard, BTN_R2};
    }
  }

  // Keyboard input reset
  pressedButtons = 0;
  tapTimerCount = 0;
  tapButton = 0;
  tapCount = 0;
  keyRepeatCount = 0;
  motionRecordHeld = 0;
  motionEraseHeld = 0;
  motionLiveHeld = 0;
  quickHelpSelectHeld = 0;
  quickHelpSelectAlone = 0;
  updateMotionRecordMode();

  // Clear screen
  gfxSetBgColor(appSettings.colorScheme.background);
  gfxClear();

  // Initialize waveform display
  waveformDisplayInit();

  // Create ChipNomad state
  chipnomadState = chipnomadCreate();
  if (!chipnomadState) {
    // Handle error - for now just exit
    return;
  }

#ifdef WEB_BUILD
  // Restore the browser's IndexedDB-backed autosave, with the demo as fallback.
  int projectLoaded = 0;
  if (projectLoad(&chipnomadState->project, getAutosavePath()) == 0) {
    projectLoaded = 1;
  } else if (projectLoad(&chipnomadState->project, "/projects/TECNODEMO.cct") == 0) {
    projectLoaded = 1;
    extractFilenameWithoutExtension("/projects/TECNODEMO.cct", appSettings.projectFilename, FILENAME_LENGTH + 1);
  }
  if (!projectLoaded) projectInitAY(&chipnomadState->project);
#else
  // Native builds restore the user's auto-saved project.
  if (projectLoad(&chipnomadState->project, getAutosavePath()) != 0)
    projectInitAY(&chipnomadState->project);
#endif

  // Initialize all screen states
  screensInitAll();

  playbackInit(&chipnomadState->playbackState, &chipnomadState->project);

  // Set mix volume from settings
  chipnomadState->mixVolume = appSettings.mixVolume;
  chipnomadState->aySampleDithering = appSettings.aySampleDithering;

  // Initialize audio system
  chipnomadInitChips(chipnomadState, appSettings.audioSampleRate, NULL);
  chipnomadSetQuality(chipnomadState, (ChipNomadQuality)appSettings.quality);
  chipnomadSetBraidsSettings(chipnomadState, appSettings.braidsBits,
    appSettings.braidsDrift, appSettings.braidsSignature,
    appSettings.braidsSignatureSeed);
  audioManager.start(appSettings.audioSampleRate, appSettings.audioBufferSize);
  audioManager.resume();

  screenSetup(&screenTitle, 0);
}

#ifdef WEB_BUILD
extern "C" EMSCRIPTEN_KEEPALIVE int webSaveProject(const char* path) {
  if (!chipnomadState || !path || !path[0]) return 1;
  return projectSave(&chipnomadState->project, path);
}
#endif

/**
* @brief Release all resources before closing the application
*/
void appCleanup(void) {
  audioManager.stop();
  chipnomadDestroy(chipnomadState);
  chipnomadState = NULL;
}

/**
* @brief Main draw function. Draws playback status
*/
void appDraw(void) {
  const ColorScheme cs = appSettings.colorScheme;

  screenDraw();

  if (currentScreen == &screenTitle) return;

  if (!chipnomadState) return;

  // Tracks
  char digit[2] = "0";
  for (int c = 0; c < chipnomadState->project.tracksCount; c++) {
    // Draw mute/solo indicator to the left of track number
    gfxSetFgColor(cs.textTitles);
    if (audioManager.trackStates[c] == TRACK_MUTED) {
      gfxPrint(34, 3 + c, "M");
    } else if (audioManager.trackStates[c] == TRACK_SOLO) {
      gfxPrint(34, 3 + c, "S");
    } else {
      gfxPrint(34, 3 + c, " "); // Clear indicator
    }

    // Keep the clipping source visible on every screen, not only in the mixer.
    int useOverloadColor = (chipnomadState->trackClipping[c] > 0);
    gfxSetFgColor(useOverloadColor ? cs.warning :
      (*pSongTrack == c ? cs.textDefault : cs.textInfo));
    digit[0] = c + 49;
    gfxPrint(35, 3 + c, digit);

    // Draw waveform between track number and note
    gfxSetFgColor(cs.textInfo);
    Bitmap* waveformBitmap = waveformDisplayGetBitmap(c);
    gfxClearRect(36, 3 + c, 1, 1);
    if (waveformBitmap) {
      gfxDrawBitmap(waveformBitmap, 36, 3 + c);
    }

    uint8_t note = chipnomadGetPlaybackStatus(chipnomadState)->tracks[c].note.pitchFinal;
    const char* noteStr = noteName(&chipnomadState->project, note);

    // Use warning color if track warning is active
    int useWarningColor = (appSettings.pitchConflictWarning && chipnomadState->trackWarnings[c] > 0);

    gfxSetFgColor(useWarningColor ? cs.warning :
      (noteStr[0] == '-' ? cs.textEmpty : cs.textValue));
      gfxPrint(37, 3 + c, noteStr);
  }

  int realtimeOverflow = chipnomadGetMotionRecordOverflow() ||
    chipnomadGetCommandOverflow(chipnomadState) || chipnomadGetRenderBufferOverflow(chipnomadState);
  if (motionEraseHeld) {
    gfxSetFgColor(cs.warning);
    gfxPrint(39, 19, "x");
  } else if (motionRecordHeld) {
    gfxSetFgColor(realtimeOverflow ? cs.warning : cs.textTitles);
    gfxPrint(39, 19, realtimeOverflow ? "!" : "*");
  } else if (motionLiveHeld) {
    gfxSetFgColor(cs.textTitles);
    gfxPrint(39, 19, "~");
  } else {
    gfxPrint(39, 19, " ");
  }
}

/**
* @brief Main event handler
*
* @param event Event
* @param value Event value
* @param userdata Arbitraty event data
*/
void appOnEvent(MainLoopEventData eventData) {
  static int dPadMask = keyLeft | keyRight | keyUp | keyDown;
  static int doubleTapMask = keyEdit | keyOpt | keyUnmapped;

  switch (eventData.type) {
  case MainLoopEvent::keyDown: {
    int value = inputCodeToKey(eventData.data.input);
    int rawInputActive = inputRawCallback != NULL;

    // Call raw input callback if set (for key mapping screen)
    if (inputRawCallback) {
      inputRawCallback(eventData.data.input, 1);
    }

    if (quickHelpSelectHeld && !rawInputActive && value != keyShift) quickHelpSelectAlone = 0;

    if (!rawInputActive && isMotionRecordTrigger(eventData.data.input)) {
      motionRecordHeld = 1;
      updateMotionRecordMode();
      break;
    }
    if (!rawInputActive && isMotionLiveTrigger(eventData.data.input)) {
      motionLiveHeld = 1;
      updateMotionRecordMode();
      break;
    }
    if (!rawInputActive && isMotionEraseTrigger(eventData.data.input)) {
      motionEraseHeld = 1;
      updateMotionRecordMode();
      break;
    }

    // PortMaster may expose the same physical control through gptokeyb and
    // SDL's controller API. Unmapped controller duplicates must not replay
    // the currently held keyboard combination.
    if (value == keyUnmapped && currentScreen != &screenKeyMapping) break;

    // Ignore duplicate downs. SDL keyboard repeat is filtered by the platform
    // loop, but this also protects tap detection from duplicate device events.
    if (pressedButtons & value) break;

    if (!rawInputActive && value == keyShift && pressedButtons == 0) {
      quickHelpSelectHeld = 1;
      quickHelpSelectAlone = 1;
    } else if (quickHelpSelectHeld) {
      quickHelpSelectAlone = 0;
    }

    pressedButtons |= value;

    // Multi-tap detection
    if (value & doubleTapMask) {
      if (value == tapButton && tapTimerCount > 0) {
        // Same button pressed again within timer - increment tap count
        tapCount++;
      } else {
        // First tap or different button - start new tap sequence
        tapButton = value;
        tapCount = 1;
      }
      tapTimerCount = appSettings.doubleTapFrames;
    } else {
      // Non-multi-tap button pressed - reset tap state
      tapButton = 0;
      tapCount = 1;
      tapTimerCount = 0;
    }

    if (value & dPadMask) {
      // Key repeats are only applicable to d-pad
      keyRepeatCount = appSettings.keyRepeatDelay;
      // As we don't support multiple d-pad keys, keep only the last pressed one
      pressedButtons = (pressedButtons & ~dPadMask) | value;
    }
    appInput(1, pressedButtons, tapCount);

    break;
  }
  case MainLoopEvent::keyUp: {
    int value = inputCodeToKey(eventData.data.input);
    int rawInputActive = inputRawCallback != NULL;

    // Call raw input callback if set (for key mapping screen)
    if (inputRawCallback) {
      inputRawCallback(eventData.data.input, 0);
    }

    if (!rawInputActive && isMotionRecordTrigger(eventData.data.input)) {
      motionRecordHeld = 0;
      updateMotionRecordMode();
      break;
    }
    if (!rawInputActive && isMotionLiveTrigger(eventData.data.input)) {
      motionLiveHeld = 0;
      updateMotionRecordMode();
      break;
    }
    if (!rawInputActive && isMotionEraseTrigger(eventData.data.input)) {
      motionEraseHeld = 0;
      updateMotionRecordMode();
      break;
    }

    if (value == keyUnmapped && currentScreen != &screenKeyMapping) break;

    pressedButtons &= ~value;

    appInput(0, pressedButtons, 0);

    if (!rawInputActive && value == keyShift && quickHelpSelectHeld &&
        quickHelpSelectAlone && pressedButtons == 0 && appSettings.quickHelpReleaseSeen < 5) {
      appSettings.quickHelpReleaseSeen++;
      screenQuickHelpOpen(currentScreen);
    }
    if (value == keyShift) quickHelpSelectHeld = 0;

    if (pressedButtons == 0) {
      // Clean untimed screen message when all keys are released
      screenMessage(0, "");
    }

    break;
  }
  case MainLoopEvent::gamepadAxes:
    chipnomadSetLiveStickAxes(eventData.data.axes[0], eventData.data.axes[1],
                              eventData.data.axes[2], eventData.data.axes[3]);
    break;
  case MainLoopEvent::tick: {
    int motionRecordChanged = 0;
    MotionRecordEvent motionRecordEvent;
    while (chipnomadConsumeMotionRecordEvent(&motionRecordEvent))
      motionRecordChanged |= applyMotionRecordEvent(motionRecordEvent);
    if (motionRecordChanged) {
      projectModified = 1;
      audioProjectDirty = 1;
      if (currentScreen == &screenPhrase) currentScreen->fullRedraw();
    }
    if (audioProjectDirty && chipnomadQueueProjectRefresh(chipnomadState)) audioProjectDirty = 0;
    // Autosave
    if (++autosaveCounter >= AUTOSAVE_INTERVAL_FRAMES) {
      autosaveCounter = 0;
      projectSave(&chipnomadState->project, getAutosavePath());
    }

    // Multi-tap timer handling
    if (tapTimerCount > 0) {
      tapTimerCount--;
      if (tapTimerCount == 0) {
        // Timer expired, reset tap count
        tapCount = 0;
        tapButton = 0;
      }
    }

    // Key repeat handling
    if (keyRepeatCount > 0) {
      int maskedButtons = pressedButtons & dPadMask;
      // Only one d-pad button can be pressed for key repeats
      if (maskedButtons == keyLeft || maskedButtons == keyRight || maskedButtons == keyUp || maskedButtons == keyDown) {
        keyRepeatCount--;
        if (keyRepeatCount == 0) {
          keyRepeatCount = appSettings.keyRepeatSpeed;
          appInput(1, pressedButtons, 0);
        }
      } else {
        keyRepeatCount = 0;
      }
    }
    break;
  }
  case MainLoopEvent::exit:
    // Auto-save the current project and settings on exit
    projectSave(&chipnomadState->project, getAutosavePath());
    settingsSave();
    break;
  case MainLoopEvent::sleep:
    // Pause audio when app goes to background
    audioManager.pause();
    if (chipnomadState) {
      // Stop playback to avoid state issues
      chipnomadQueuePlaybackStop(chipnomadState);
      // Auto-save project
      projectSave(&chipnomadState->project, getAutosavePath());
    }
    // Save settings
    settingsSave();
    break;
  case MainLoopEvent::wake:
    // Resume audio when app comes back to foreground
    audioManager.resume();
    break;
  case MainLoopEvent::fullRedraw:
    // Force full screen redraw
    gfxSetBgColor(appSettings.colorScheme.background);
    gfxClear();
    if (currentScreen) {
      currentScreen->fullRedraw();
      if (currentScreen != &screenTitle)
        drawScreenMap();
    }
    break;
  }
}
