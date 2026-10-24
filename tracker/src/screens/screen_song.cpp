#include "screens.h"
#include "screen_settings.h"
#include "common.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "chipnomad_lib.h"
#include "project_utils.h"
#include "audio_manager.h"
#include "app.h"

#include "copy_paste.h"
#include <string.h>

// Screen state variables
static uint16_t lastChainValue = 0;

// Track mute/solo state machine
typedef enum {
  MUTE_SOLO_EMPTY,
  MUTE_SOLO_OPT_PRESSED,
  MUTE_SOLO_MUTE_STATE,
  MUTE_SOLO_SOLO_STATE,
  MUTE_SOLO_TEMP_LEFT,
  MUTE_SOLO_TEMP_RIGHT
} MuteSoloState;

static MuteSoloState muteSoloState = MUTE_SOLO_EMPTY;
static int liveMode = 0;
// A double tap on OPT waits before switching Live Mode, so a triple tap
// (highlight) never flashes the other screen title. The wait follows the
// player's own tapping speed: half again the gap between the first two taps.
static constexpr int LIVE_TOGGLE_MIN_WAIT_FRAMES = 6;
static int liveTogglePending = 0;
static int liveToggledEarly = 0;   // Switched while a third tap could still come
static uint32_t songFrame = 0;     // Counts Song screen frames
static uint32_t firstOptTapFrame = 0;
static uint32_t liveToggleFrame = 0;

static int getColumnCount(int row);
static void drawStatic(void);
static void drawField(int col, int row, CellState state);
static void drawRowHeader(int row, CellState state);
static void drawColHeader(int col, CellState state);
static void drawCursor(int col, int row);
static void drawSelection(int col1, int row1, int col2, int row2);
static int onEdit(int col, int row, CellEditAction action);
static LoopRange getLoopRange(void);

static ScreenData screen = {
  .rows = PROJECT_MAX_LENGTH,
  .cursorRow = 0,
  .cursorCol = 0,
  .topRow = 0,
  .selectMode = 0,
  .selectStartRow = 0,
  .selectStartCol = 0,
  .selectAnchorRow = 0,
  .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = getColumnCount,
  .drawStatic = drawStatic,
  .drawCursor = drawCursor,
  .drawSelection = drawSelection,
  .drawRowHeader = drawRowHeader,
  .drawColHeader = drawColHeader,
  .drawField = drawField,
  .onEdit = onEdit,
  .onInput = NULL,
  .onRawInput = NULL,
  .isCellValid = NULL,
  .getLoopRange = getLoopRange,
};

static void selectedTrackBounds(int* first, int* last) {
  *first = screen.cursorCol;
  *last = screen.cursorCol;
  if (screen.selectMode) {
    int startRow, endRow;
    getSelectionBounds(&screen, first, &startRow, last, &endRow);
  }
}

static void toggleSelectedMute(void) {
  int first, last, allMuted = 1;
  selectedTrackBounds(&first, &last);
  for (int i = first; i <= last; ++i) if (audioManager.trackStates[i] != TRACK_MUTED) allMuted = 0;
  for (int i = 0; i < PROJECT_MAX_TRACKS; ++i)
    if (audioManager.trackStates[i] == TRACK_SOLO) audioManager.trackStates[i] = TRACK_NORMAL;
  for (int i = first; i <= last; ++i) audioManager.trackStates[i] = allMuted ? TRACK_NORMAL : TRACK_MUTED;
  audioManager.toggleTrackMute(-1);
}

static void toggleSelectedSolo(void) {
  int first, last, onlySelectionSolo = 1;
  selectedTrackBounds(&first, &last);
  for (int i = 0; i < chipnomadState->project.tracksCount; ++i) {
    int selected = i >= first && i <= last;
    if (audioManager.trackStates[i] != (selected ? TRACK_SOLO : TRACK_NORMAL)) onlySelectionSolo = 0;
  }
  for (int i = 0; i < PROJECT_MAX_TRACKS; ++i)
    audioManager.trackStates[i] = (!onlySelectionSolo && i >= first && i <= last) ? TRACK_SOLO : TRACK_NORMAL;
  audioManager.toggleTrackSolo(-1);
}

static int livePlay(int tapCount) {
  int first, last, startRow, endRow;
  selectedTrackBounds(&first, &last);
  if (screen.selectMode) getSelectionBounds(&screen, &first, &startRow, &last, &endRow);
  else startRow = endRow = screen.cursorRow;
  if (screen.selectMode && startRow != endRow) {
    screenMessage(MESSAGE_TIME, "Live: select one row");
    return 1;
  }
  int row = screen.selectMode ? startRow : screen.cursorRow;
  const PlaybackStatus* status = chipnomadGetPlaybackStatus(chipnomadState);
  for (int track = first; track <= last; ++track) {
    int chain = chipnomadState->project.song[row][track];
    if (status->tracks[track].mode == PlaybackMode::stopped) {
      if (chain != EMPTY_VALUE_16) chipnomadQueuePlaybackStartLiveChain(chipnomadState, track, row);
    } else {
      // A Play press that starts a live chain must not turn the next, separate
      // chain selection into an urgent double-tap. A second press only upgrades
      // an already queued live action to urgent.
      int urgent = tapCount >= 2 && status->tracks[track].queue.liveAction != LiveQueueAction::none;
      chipnomadQueuePlaybackQueueLiveChain(chipnomadState, track,
        chain == EMPTY_VALUE_16 ? -1 : row, urgent);
    }
  }
  return 1;
}

static void init(void) {
  lastChainValue = 0;
  screen.cursorRow = 0;
  screen.cursorCol = 0;
  screen.topRow = 0;
  screen.selectMode = 0;
  screen.selectStartRow = 0;
  screen.selectStartCol = 0;
  screen.selectAnchorRow = 0;
  screen.selectAnchorCol = 0;
  liveMode = 0;
  liveTogglePending = 0;
  liveToggledEarly = 0;
  pSongRow = &screen.cursorRow;
  pSongTrack = &screen.cursorCol;
}

static void setup(int input) {
  screen.selectMode = 0;
  liveTogglePending = 0;
  liveToggledEarly = 0;
}

static void applyPendingLiveToggle(void) {
  if (!liveTogglePending) return;
  liveTogglePending = 0;
  liveToggledEarly = appTapSequenceOpen(keyOpt);
  liveMode ^= 1;
  drawStatic();
  screenMessage(MESSAGE_TIME, liveMode ? "Live: PLAY queue, double urgent" : "Song mode");
}

///////////////////////////////////////////////////////////////////////////////
//
// Drawing functions
//

static int getColumnCount(int row) {
  return chipnomadState->project.tracksCount;
}

static void drawStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 0, liveMode ? "LIVE" : "SONG");
}

static void drawField(int col, int row, CellState state) {
  if (row < screen.topRow || row >= (screen.topRow + screenVisibleRows())) return; // Don't draw outside of the viewing area

  int chain = chipnomadState->project.song[row][col];
  int isHighlighted = chipnomadState->project.songHighlight[row][col];

  if (isHighlighted) {
    gfxSetFgColor(appSettings.colorScheme.textTitles);
  } else {
    setCellColor(state, chain == EMPTY_VALUE_16, chain != EMPTY_VALUE_16 && chainHasNotes(&chipnomadState->project, chain));
  }
  gfxPrint(3 + col * 3, 3 + row - screen.topRow, chain == EMPTY_VALUE_16 ? "--" : byteToHex(chain));
}

static void drawRowHeader(int row, CellState state) {
  const ColorScheme cs = appSettings.colorScheme;

  gfxSetFgColor((state == CellState::focus) ? cs.textDefault : cs.textInfo);
  gfxPrint(0, 3 + row - screen.topRow, byteToHex(row));
}

static void drawColHeader(int col, CellState state) {
  static char digit[2] = "0";
  const ColorScheme cs = appSettings.colorScheme;

  gfxSetFgColor((state == CellState::focus) ? cs.textDefault : cs.textInfo);
  digit[0] = col + 49;
  gfxPrint(3 + col * 3, 2, digit);
}

static void drawCursor(int col, int row) {
  gfxCursor(3 + col * 3, 3 + row - screen.topRow, 2);
}

static void drawSelection(int col1, int row1, int col2, int row2) {
  int x = 3 + col1 * 3;
  int y = 3 + row1 - screen.topRow;
  int w = 3 * (col2 - col1 + 1) - 1;
  int y2 = y + (row2 - row1);

  if (y < 3) y = 3; // Top row of selection is above the screen
  if (y > (3 + 15)) return; // Top row of selection is below the screen
  if (y2 < 3) return;
  if (y2 > (3 + 15)) y2 = (3 + 15);

  gfxRect(x, y, w, (y2 - y) + 1);
}

static void fullRedraw(void) {
  screenFullRedraw(&screen);
}

static void draw(void) {
  ++songFrame;
  if (liveTogglePending && ((int32_t)(songFrame - liveToggleFrame) >= 0 || !appTapSequenceOpen(keyOpt)))
    applyPendingLiveToggle();

  for (int c = 0; c < chipnomadState->project.tracksCount; c++) {
    gfxClearRect(2 + c * 3, 3, 1, screenVisibleRows());
    const PlaybackTrackState* track = &chipnomadGetPlaybackStatus(chipnomadState)->tracks[c];
    if (track->songRow != EMPTY_VALUE_16) {
      int row = track->songRow - screen.topRow;
      if (row >= 0 && row < screenVisibleRows()) {
        gfxSetFgColor(appSettings.colorScheme.playMarkers);
        gfxPrint(2 + c * 3, 3 + row, ">");
      }
    }
    if (track->queue.liveAction != LiveQueueAction::none) {
      int stop = track->queue.liveAction == LiveQueueAction::stopNormal || track->queue.liveAction == LiveQueueAction::stopUrgent;
      int urgent = track->queue.liveAction == LiveQueueAction::urgent || track->queue.liveAction == LiveQueueAction::stopUrgent;
      int row = (stop ? track->songRow : track->queue.songRow) - screen.topRow;
      if (row >= 0 && row < screenVisibleRows()) {
        gfxSetFgColor(appSettings.colorScheme.playMarkers);
        gfxPrint(2 + c * 3, 3 + row, stop ? "-" : (urgent ? "!" : "+"));
      }
    }

    // Draw mute/solo indicator above track number
    gfxSetFgColor(appSettings.colorScheme.textTitles);
    if (audioManager.trackStates[c] == TRACK_MUTED) {
      gfxPrint(3 + c * 3, 1, "M");
    } else if (audioManager.trackStates[c] == TRACK_SOLO) {
      gfxPrint(3 + c * 3, 1, "S");
    } else {
      gfxPrint(3 + c * 3, 1, " "); // Clear indicator
    }
  }

  screenDrawOverlays(&screen);
}

///////////////////////////////////////////////////////////////////////////////
//
// Input handling
//

static int inputScreenNavigation(int keys, int tapCount) {
  if (keys == (keyLeft | keyShift)) {
    screenSetup(&screenMixer, 0);
    return 1;
  }

  // Go to Chain screen
  if (keys == (keyRight | keyShift)) {
    int chain = chipnomadState->project.song[screen.cursorRow][screen.cursorCol];

    if (chain == EMPTY_VALUE_16) {
      screenMessage(0, "Enter a chain");
    } else {
      screenSetup(&screenChain, -1);
    }
    return 1;
  }

  // Go to Project screen
  if (keys == (keyUp | keyShift)) {
    screenSetup(&screenProject, 0);
    return 1;
  }

  // Go to Settings screen
  if (keys == (keyDown | keyShift)) {
    screenSetup(&screenSettings, 0);
    return 1;
  }

  return 0;
}

static int editCell(int col, int row, CellEditAction action) {
  if (action == CellEditAction::doubleTap) {
    int current = chipnomadState->project.song[row][col];
    if (current != EMPTY_VALUE_16) {
      int nextEmpty = findEmptyChain(&chipnomadState->project, current + 1);
      if (nextEmpty != EMPTY_VALUE_16) {
        chipnomadState->project.song[row][col] = nextEmpty;
        lastChainValue = nextEmpty;
      } else {
        screenMessage(MESSAGE_TIME, "No free chains");
      }
    }
    return 1;
  } else if (action == CellEditAction::clear) {
    if (chipnomadState->project.song[row][col] != EMPTY_VALUE_16) {
      // Clear the value
      chipnomadState->project.song[row][col] = EMPTY_VALUE_16;
      return 1;
    } else {
      // Value is already empty, shift column up
      shiftSongColumnUp(col, row);
      fullRedraw();
      return 1;
    }
  } else if (action == CellEditAction::shallowClone) {
    int current = chipnomadState->project.song[row][col];
    if (current != EMPTY_VALUE_16) {
      int cloned = cloneChainToNext(current);
      if (cloned != EMPTY_VALUE_16) {
        chipnomadState->project.song[row][col] = cloned;
        lastChainValue = cloned;
        return 1;
      }
    }
    return 0;
  } else if (action == CellEditAction::deepClone) {
    int current = chipnomadState->project.song[row][col];
    if (current != EMPTY_VALUE_16) {
      // Deep clone the phrases in the existing chain (don't create a new chain)
      if (deepCloneChain(current)) {
        return 1;
      }
    }
    return 0;
  }
  return edit16withLimit(action, &chipnomadState->project.song[row][col], &lastChainValue, 16, PROJECT_MAX_CHAINS - 1);
}

static int onEdit(int col, int row, CellEditAction action) {
  int handled = 0;
  int startCol, startRow, endCol, endRow;
  getSelectionBounds(&screen, &startCol, &startRow, &endCol, &endRow);

  if (action == CellEditAction::switchSelection) {
    return switchSongSelectionMode(&screen);
  } else if (action == CellEditAction::multiIncreaseBig || action == CellEditAction::multiDecreaseBig) {
    if (screen.selectMode != 1) return 0;

    if (action == CellEditAction::multiDecreaseBig) {
      handled = applySongMoveDown(startCol, startRow, endCol, endRow);
      if (handled) {
        screen.selectStartRow++;
        screen.cursorRow++;
        // Scroll down if selection moved below visible area
        if (screen.cursorRow >= screen.topRow + screenVisibleRows()) {
          screen.topRow++;
        }
      }
    } else {
      handled = applySongMoveUp(startCol, startRow, endCol, endRow);
      if (handled) {
        screen.selectStartRow--;
        screen.cursorRow--;
        // Scroll up if selection moved above visible area
        if (screen.cursorRow < screen.topRow) {
          screen.topRow--;
        }
      }
    }

    if (handled) {
      fullRedraw();
    }
  } else if (action == CellEditAction::shallowClone || action == CellEditAction::deepClone) {
    int clonedCount = 0;
    for (int r = startRow; r <= endRow; r++) {
      for (int c = startCol; c <= endCol; c++) {
        if (editCell(c, r, action)) clonedCount++;
      }
    }
    if (clonedCount > 0) {
      const char* msg = (action == CellEditAction::shallowClone) ? "Shallow-cloned" : "Deep-cloned";
      handled = 1;
      screenMessage(MESSAGE_TIME, "%s %d chain%s", msg, clonedCount, clonedCount == 1 ? "" : "s");
    } else {
      screenMessage(MESSAGE_TIME, "No chains to clone");
    }
  } else if (applyMultiEdit(startCol, startRow, endCol, endRow, action, editCell)) {
    handled = 1;
  } else if (action == CellEditAction::copy) {
    copySong(startCol, startRow, endCol, endRow, 0);
    handled = 1;
  } else if (action == CellEditAction::cut) {
    copySong(startCol, startRow, endCol, endRow, 1);
    handled = 1;
  } else if (action == CellEditAction::paste) {
    const int rowsPasted = pasteSong(col, row);
    if (rowsPasted > 0) {
      // Move cursor below pasted data, or to last row if paste extends to end
      int newRow = row + rowsPasted;
      if (newRow >= screen.rows) newRow = screen.rows - 1;
      screen.cursorRow = newRow;
    }
    fullRedraw();
    handled = 1;
  } else {
    handled = editCell(col, row, action);
  }

  if (handled) projectModified = 1;
  return handled;
  return 0;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  int handled = 0;

  if (isKeyDown) {
    int thirdOptTap = keys == keyOpt && tapCount == 3;
    if (thirdOptTap && liveTogglePending) {
      liveTogglePending = 0; // The triple tap highlights instead
    } else if (thirdOptTap && liveToggledEarly) {
      liveMode ^= 1; // A slower third tap undoes the early switch
      drawStatic();
    } else if (liveTogglePending) {
      applyPendingLiveToggle(); // Any other press switches Live Mode first
    }
    liveToggledEarly = 0;
  }

  if (isKeyDown && keys == keyOpt && tapCount == 1) firstOptTapFrame = songFrame;
  if (isKeyDown && keys == keyOpt && tapCount == 2) {
    screenClearOptPressed();
    int gap = (int)(songFrame - firstOptTapFrame);
    int wait = gap + gap / 2;
    if (wait < LIVE_TOGGLE_MIN_WAIT_FRAMES) wait = LIVE_TOGGLE_MIN_WAIT_FRAMES;
    liveToggleFrame = songFrame + wait;
    liveTogglePending = 1;
    return 1;
  }
  if (isKeyDown && liveMode && keys == keyPlay) return livePlay(tapCount);

  // Mute/solo accepts a selected group of Song columns.
  {
    switch (muteSoloState) {
      case MUTE_SOLO_EMPTY:
        if (isKeyDown && keys == keyOpt) {
          if (tapCount == 3) {
            // Toggle highlight on triple-tap Opt
            uint8_t* highlight = &chipnomadState->project.songHighlight[screen.cursorRow][screen.cursorCol];
            *highlight ^= 1;
            projectModified = 1;
            drawField(screen.cursorCol, screen.cursorRow, CellState::focus);
            drawCursor(screen.cursorCol, screen.cursorRow);
            screenMessage(MESSAGE_TIME, *highlight ? "Highlight on" : "Highlight off");
          } else {
            muteSoloState = MUTE_SOLO_OPT_PRESSED;
          }
          // A plain Opt tap copies a selection on release. Let common input
          // arm that copy while still watching for a mute/solo chord.
          handled = tapCount == 3 || screen.selectMode == 0;
        }
        break;

      case MUTE_SOLO_OPT_PRESSED:
        if (isKeyDown && keys == (keyOpt | keyShift)) {
          toggleSelectedMute();
          muteSoloState = MUTE_SOLO_MUTE_STATE;
          handled = 1;
        } else if (isKeyDown && keys == (keyOpt | keyPlay)) {
          toggleSelectedSolo();
          muteSoloState = MUTE_SOLO_SOLO_STATE;
          handled = 1;
        } else if (isKeyDown && keys == (keyOpt | keyLeft)) {
          // Solo tracks to the left (including cursor)
          for (int i = 0; i < PROJECT_MAX_TRACKS; i++) {
            audioManager.trackStates[i] = (i <= screen.cursorCol) ? TRACK_SOLO : TRACK_NORMAL;
          }
          audioManager.toggleTrackSolo(-1);
          muteSoloState = MUTE_SOLO_TEMP_LEFT;
          handled = 1;
        } else if (isKeyDown && keys == (keyOpt | keyRight)) {
          // Solo tracks to the right (including cursor)
          for (int i = 0; i < PROJECT_MAX_TRACKS; i++) {
            audioManager.trackStates[i] = (i >= screen.cursorCol) ? TRACK_SOLO : TRACK_NORMAL;
          }
          audioManager.toggleTrackSolo(-1);
          muteSoloState = MUTE_SOLO_TEMP_RIGHT;
          handled = 1;
        } else if (isKeyDown && keys == keyOpt) {
          handled = 1; // Stay in OPT_PRESSED state
        } else if (!isKeyDown && keys == 0) {
          muteSoloState = MUTE_SOLO_EMPTY;
        }
        break;

      case MUTE_SOLO_MUTE_STATE:
        if (!isKeyDown && keys == keyOpt) {
          // SHIFT released first - momentary unmute
          toggleSelectedMute();
          muteSoloState = MUTE_SOLO_OPT_PRESSED;
          handled = 1;
        } else if (!isKeyDown && keys == keyShift) {
          // OPT released first - mute sticks
          muteSoloState = MUTE_SOLO_EMPTY;
          handled = 1;
        } else if (keys == (keyOpt | keyShift)) {
          handled = 1; // Stay in mute state
        } else if (keys == 0) {
          muteSoloState = MUTE_SOLO_EMPTY;
        }
        break;

      case MUTE_SOLO_SOLO_STATE:
        if (!isKeyDown && keys == keyOpt) {
          // PLAY released first - momentary unsolo
          toggleSelectedSolo();
          muteSoloState = MUTE_SOLO_OPT_PRESSED;
          handled = 1;
        } else if (!isKeyDown && keys == keyPlay) {
          // OPT released first - solo sticks
          muteSoloState = MUTE_SOLO_EMPTY;
          handled = 1;
        } else if (keys == (keyOpt | keyPlay)) {
          handled = 1; // Stay in solo state
        } else if (keys == 0) {
          muteSoloState = MUTE_SOLO_EMPTY;
        }
        break;

      case MUTE_SOLO_TEMP_LEFT:
      case MUTE_SOLO_TEMP_RIGHT:
        if (!isKeyDown && (keys == keyOpt || keys == 0)) {
          // Reset all tracks to normal
          for (int i = 0; i < PROJECT_MAX_TRACKS; i++) {
            audioManager.trackStates[i] = TRACK_NORMAL;
          }
          audioManager.toggleTrackSolo(-1);
          muteSoloState = (keys == keyOpt) ? MUTE_SOLO_OPT_PRESSED : MUTE_SOLO_EMPTY;
          handled = 1;
        } else if (keys == (keyOpt | keyLeft) || keys == (keyOpt | keyRight)) {
          handled = 1;
        }
        break;
    }
  }

  // Reset state when all keys are released.
  if (keys == 0) {
    muteSoloState = MUTE_SOLO_EMPTY;
  }

  // A Song-specific chord must not also copy and dismiss the selection.
  if (handled && (keys != keyOpt || tapCount == 3)) screenClearOptPressed();

  // Only call common input handling if we didn't handle Song-specific input
  if (!handled) {
    if (isKeyDown && screen.selectMode == 0 && inputScreenNavigation(keys, tapCount)) return 1;
    return screenInput(&screen, isKeyDown, keys, tapCount);
  }

  return handled;
}

static LoopRange getLoopRange(void) {
  LoopRange range = {0};
  if (screen.selectMode == 1) {
    int startCol, startRow, endCol, endRow;
    getSelectionBounds(&screen, &startCol, &startRow, &endCol, &endRow);
    range.enabled = 1;
    range.level = 0;
    range.startSongRow = startRow;
    range.startChainRow = 0;
    range.startPhraseRow = 0;
    range.endSongRow = endRow;
    range.endChainRow = 15;
    range.endPhraseRow = 15;
  }
  return range;
}

///////////////////////////////////////////////////////////////////////////////
//
// Key jazz (desktop only): type a chain's hex index directly instead of
// incrementing with Up/Down. Toggled with Esc, independent of the Phrase
// screen's key jazz (see screen_phrase.cpp for the note-entry version of
// this same pattern). Also brings Phrase's structure-editing shortcuts
// here: Shift+arrows select rows/columns, Delete/Backspace/Insert edit the
// song structure the same way. While key jazz is active this takes over
// Shift (so Shift+Right/Up no longer navigate to Chain/Project - Esc to
// get those back), same tradeoff as Phrase already makes.
//

#ifdef DESKTOP_BUILD

static int keyJazzEnabled = 0;
static int keyJazzEditRow = -1;
static int keyJazzEditCol = -1;

// The selection's row/column bounds if one is active, else just the
// cursor's single cell.
static void keyJazzGetActiveRange(int* startCol, int* startRow, int* endCol, int* endRow) {
  if (screen.selectMode) {
    getSelectionBounds(&screen, startCol, startRow, endCol, endRow);
  } else {
    *startCol = *endCol = screen.cursorCol;
    *startRow = *endRow = screen.cursorRow;
  }
}

int songKeyJazzHandleRawKey(InputCode input, int isDown) {
  if (input.deviceType != InputDeviceType::keyboard) return 0;

  if (inputIsKeyJazzToggle(input)) {
    if (isDown) {
      keyJazzEnabled = !keyJazzEnabled;
      if (!keyJazzEnabled) screen.selectMode = 0;
      screenMessage(MESSAGE_TIME, keyJazzEnabled ? "KEY JAZZ ON (Esc to exit)" : "KEY JAZZ OFF");
      fullRedraw();
    }
    return 1;
  }

  if (!keyJazzEnabled) return 0;
  if (inputIsShiftKey(input)) return 1; // Swallow: see screen_phrase.cpp's inputIsShiftKey comment

  int arrowDir = inputArrowKeyDirection(input);
  if (arrowDir != 0) {
    if (inputIsShiftHeld()) {
      if (isDown && !screen.selectMode) {
        screen.selectStartRow = screen.cursorRow;
        screen.selectStartCol = screen.cursorCol;
        screen.selectAnchorRow = screen.cursorRow;
        screen.selectAnchorCol = screen.cursorCol;
        screen.selectMode = 1;
      }
    } else if (screen.selectMode) {
      if (isDown) {
        screen.selectMode = 0;
        fullRedraw();
      }
    }
    return 0; // Let normal cursor movement happen (and extend/render the selection)
  }

  if (inputIsCtrlHeld()) {
    if (inputIsCopyKey(input) || inputIsCutKey(input)) {
      if (isDown) {
        int startCol, startRow, endCol, endRow;
        keyJazzGetActiveRange(&startCol, &startRow, &endCol, &endRow);
        int isCut = inputIsCutKey(input);
        copySong(startCol, startRow, endCol, endRow, isCut);
        int count = endRow - startRow + 1;
        screenMessage(MESSAGE_TIME, "KEY JAZZ: %s %d row%s", isCut ? "cut" : "copied", count, count == 1 ? "" : "s");
        if (isCut) {
          screen.selectMode = 0;
          fullRedraw();
        }
      }
      return 1;
    }
    if (inputIsPasteKey(input)) {
      if (isDown) {
        int rowsPasted = pasteSong(screen.cursorCol, screen.cursorRow);
        if (rowsPasted > 0) {
          screenMessage(MESSAGE_TIME, "KEY JAZZ: pasted %d row%s", rowsPasted, rowsPasted == 1 ? "" : "s");
          fullRedraw();
        }
      }
      return 1;
    }
    return 0; // Other Ctrl+key combos: not our concern
  }

  if (inputIsDeleteKey(input)) {
    // Whole row(s), every track column - the song-row equivalent of
    // Phrase's Delete.
    if (isDown) {
      int startCol, startRow, endCol, endRow;
      keyJazzGetActiveRange(&startCol, &startRow, &endCol, &endRow);
      int count = endRow - startRow + 1;
      int tracksCount = chipnomadState->project.tracksCount;
      for (int c = 0; c < tracksCount; c++)
        for (int i = 0; i < count; i++) shiftSongColumnUp(c, startRow);
      screen.cursorRow = startRow;
      screen.selectMode = 0;
      fullRedraw();
    }
    return 1;
  }

  if (inputIsBackspaceKey(input)) {
    // Just the current/selected column(s), like Phrase's Backspace.
    if (isDown) {
      int startCol, startRow, endCol, endRow;
      keyJazzGetActiveRange(&startCol, &startRow, &endCol, &endRow);
      int count = endRow - startRow + 1;
      for (int c = startCol; c <= endCol; c++)
        for (int i = 0; i < count; i++) shiftSongColumnUp(c, startRow);
      screen.cursorRow = startRow > 0 ? startRow - 1 : 0;
      screen.selectMode = 0;
      fullRedraw();
    }
    return 1;
  }

  if (inputIsInsertKey(input)) {
    if (isDown) {
      int row = screen.cursorRow;
      int lastRow = screen.rows - 1;
      int tracksCount = chipnomadState->project.tracksCount;
      for (int r = lastRow; r > row; r--)
        for (int c = 0; c < tracksCount; c++)
          chipnomadState->project.song[r][c] = chipnomadState->project.song[r - 1][c];
      for (int c = 0; c < tracksCount; c++) chipnomadState->project.song[row][c] = EMPTY_VALUE_16;
      fullRedraw();
    }
    return 1;
  }

  int digit = inputHexDigitValue(input);
  if (digit < 0) return 0;

  if (isDown) {
    int row = screen.cursorRow;
    int col = screen.cursorCol;
    uint16_t current = chipnomadState->project.song[row][col];
    // A cursor move since the last digit starts a fresh value; consecutive
    // digits on the same cell shift into the existing one (typing "3F").
    uint16_t base = (row == keyJazzEditRow && col == keyJazzEditCol && current != EMPTY_VALUE_16) ? current : 0;
    int value = base * 16 + digit;
    if (value > PROJECT_MAX_CHAINS - 1) value = PROJECT_MAX_CHAINS - 1;
    chipnomadState->project.song[row][col] = (uint16_t)value;
    lastChainValue = (uint16_t)value;
    keyJazzEditRow = row;
    keyJazzEditCol = col;
    drawField(col, row, CellState::normal);
  }
  return 1;
}

#endif // DESKTOP_BUILD

static ScreenPlaybackLevel getPlaybackLevel(void) {
  return ScreenPlaybackLevel::song;
}

const AppScreen screenSong = {
  .init = init,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = getPlaybackLevel
};

LoopRange songScreenGetLoopRange(void) {
  return getLoopRange();
}
