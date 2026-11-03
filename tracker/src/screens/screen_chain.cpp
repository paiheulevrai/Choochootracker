#include "screens.h"
#include "common.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "chipnomad_lib.h"
#include "project_utils.h"
#include "copy_paste.h"
#include <string.h>

static int chain = 0;
static uint16_t lastPhraseValue = 0;
static uint8_t lastTransposeValue = 0;

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
  .rows = 16,
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

static void init(void) {
  lastPhraseValue = 0;
  lastTransposeValue = 0;
  screen.cursorRow = 0;
  screen.cursorCol = 0;
  screen.topRow = 0;
  screen.selectMode = 0;
  screen.selectStartRow = 0;
  screen.selectStartCol = 0;
  screen.selectAnchorRow = 0;
  screen.selectAnchorCol = 0;
  pChainRow = &screen.cursorRow;
}

static void setup(int input) {
  chain = chipnomadState->project.song[*pSongRow][*pSongTrack];
  // Defensive: an out-of-range or EMPTY cell must never index chains[].
  // (Navigation guards route empty cells to the Song screen; this clamp
  // keeps any other entry path safe.)
  if (chain >= PROJECT_MAX_CHAINS) chain = 0;
  screen.selectMode = 0;
}

///////////////////////////////////////////////////////////////////////////////
//
// Drawing functions
//

static int getColumnCount(int row) {
  return 2;
}

static void drawStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrintf(0, 0, "CHAIN %02X%c", chain, isChainUsedElsewhere(&chipnomadState->project, chain, *pSongTrack, *pSongRow) ? '*' : ' ');
}

static void drawField(int col, int row, CellState state) {
  if (row < screen.topRow || row >= screen.topRow + screenVisibleRows()) return;
  uint16_t phrase = chipnomadState->project.chains[chain].rows[row].phrase;
  int hasContent = phrase != EMPTY_VALUE_16 && phraseHasNotes(&chipnomadState->project, phrase);

  if (col == 0) {
    // Phrase
    setCellColor(state, phrase == EMPTY_VALUE_16, hasContent);
    if (phrase == EMPTY_VALUE_16) {
      gfxPrint(3, 3 + row - screen.topRow, "---");
    } else {
      gfxPrintf(3, 3 + row - screen.topRow, "%03X", phrase);
    }
    // Also draw transpose to keep colors synchronized (only if not in selection mode)
    if (screen.selectMode == 0) {
      setCellColor(CellState::normal, 0, hasContent);
      gfxPrint(7, 3 + row - screen.topRow, byteToHex(chipnomadState->project.chains[chain].rows[row].transpose));
    }
  } else {
    // Transpose
    setCellColor(state, 0, hasContent);
    gfxPrint(7, 3 + row - screen.topRow, byteToHex(chipnomadState->project.chains[chain].rows[row].transpose));
  }
}

static void drawRowHeader(int row, CellState state) {
  if (row < screen.topRow || row >= screen.topRow + screenVisibleRows()) return;
  const ColorScheme cs = appSettings.colorScheme;
  gfxSetFgColor((state == CellState::focus) ? cs.textDefault : cs.textInfo);
  gfxPrintf(1, 3 + row - screen.topRow, "%X", row);
}

static void drawColHeader(int col, CellState state) {
  const ColorScheme cs = appSettings.colorScheme;
  gfxSetFgColor((state == CellState::focus) ? cs.textDefault : cs.textInfo);

  if (col == 0) {
    // Phrase
    gfxPrint(3, 2, "P");
  } else {
    // Transpose
    gfxPrint(7, 2, "T");
  }
}

static void drawCursor(int col, int row) {
  if (row < screen.topRow || row >= screen.topRow + screenVisibleRows()) return;
  if (col == 0) {
    // Phrase
    gfxCursor(3, 3 + row - screen.topRow, 3);
  } else {
    // Transpose
    gfxCursor(7, 3 + row - screen.topRow, 2);
  }
}

static void drawSelection(int col1, int row1, int col2, int row2) {
  int x = (col1 == 0) ? 3 : 7;
  int w = (col2 - col1 == 1) ? 6 : (col1 == 0 ? 3 : 2);
  int y = 3 + row1 - screen.topRow;
  int h = row2 - row1 + 1;
  gfxRect(x, y, w, h);
}

static void fullRedraw(void) {
  screenFullRedraw(&screen);
}

static void draw(void) {
  gfxClearRect(2, 3, 1, screenVisibleRows());
  if (chipnomadState && chipnomadGetPlaybackStatus(chipnomadState)->tracks[*pSongTrack].songRow == *pSongRow) {
    int chainRow = chipnomadGetPlaybackStatus(chipnomadState)->tracks[*pSongTrack].chainRow;
    if (chainRow >= screen.topRow && chainRow < screen.topRow + screenVisibleRows()) {
      gfxSetFgColor(appSettings.colorScheme.playMarkers);
      gfxPrint(2, 3 + chainRow - screen.topRow, ">");
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
//
// Input handling
//

static int editCell(int col, int row, enum CellEditAction action) {
  if (col == 0) {
    if (action == CellEditAction::doubleTap) {
      uint16_t current = chipnomadState->project.chains[chain].rows[row].phrase;
      if (current != EMPTY_VALUE_16) {
        int nextEmpty = findEmptyPhrase(&chipnomadState->project, current + 1);
        if (nextEmpty != EMPTY_VALUE_16) {
          chipnomadState->project.chains[chain].rows[row].phrase = nextEmpty;
          lastPhraseValue = nextEmpty;
        } else {
          screenMessage(MESSAGE_TIME, "No empty phrases");
        }
      }
      return 1;
    } else if (action == CellEditAction::shallowClone || action == CellEditAction::deepClone) {
      uint16_t current = chipnomadState->project.chains[chain].rows[row].phrase;
      if (current != EMPTY_VALUE_16) {
        int cloned = clonePhraseToNext(current);
        if (cloned != EMPTY_VALUE_16) {
          chipnomadState->project.chains[chain].rows[row].phrase = cloned;
          lastPhraseValue = cloned;
          return 1;
        }
      }
      return 0;
    }
    return edit16withLimit(action, &chipnomadState->project.chains[chain].rows[row].phrase, &lastPhraseValue, 16, PROJECT_MAX_PHRASES - 1);
  } else {
    return edit8noLimit(action, &chipnomadState->project.chains[chain].rows[row].transpose, &lastTransposeValue, chipnomadState->project.pitchTable.octaveSize);
  }
}

static int onEdit(int col, int row, enum CellEditAction action) {
  int handled = 0;

  int startCol, startRow, endCol, endRow;
  getSelectionBounds(&screen, &startCol, &startRow, &endCol, &endRow);

  if (action == CellEditAction::switchSelection) {
    return switchChainSelectionMode(&screen);
  } else if (action == CellEditAction::multiIncrease || action == CellEditAction::multiDecrease ||
             action == CellEditAction::multiIncreaseBig || action == CellEditAction::multiDecreaseBig) {
    if (!isSingleColumnSelection(&screen)) return 0;
    handled = applyMultiEdit(startCol, startRow, endCol, endRow, action, editCell);
  } else if (action == CellEditAction::shallowClone || action == CellEditAction::deepClone) {
    int clonedCount = 0;
    for (int r = startRow; r <= endRow; r++) {
      for (int c = startCol; c <= endCol; c++) {
        if (editCell(c, r, action)) clonedCount++;
      }
    }
    if (clonedCount > 0) {
      screenMessage(MESSAGE_TIME, "Cloned %d phrase%s", clonedCount, clonedCount == 1 ? "" : "s");
      handled = 1;
    } else {
      screenMessage(MESSAGE_TIME, "No phrases to clone");
    }
  } else if (action == CellEditAction::copy) {
    int startCol, startRow, endCol, endRow;
    getSelectionBounds(&screen, &startCol, &startRow, &endCol, &endRow);
    copyChain(chain, startCol, startRow, endCol, endRow, 0);
    handled = 1;
  } else if (action == CellEditAction::cut) {
    int startCol, startRow, endCol, endRow;
    getSelectionBounds(&screen, &startCol, &startRow, &endCol, &endRow);
    copyChain(chain, startCol, startRow, endCol, endRow, 1);
    handled = 1;
  } else if (action == CellEditAction::paste) {
    const int rowsPasted = pasteChain(chain, col, row);
    if (rowsPasted > 0) {
      // Move cursor below pasted data, or to last row if paste extends to end
      int newRow = row + rowsPasted;
      if (newRow > 15) newRow = 15;
      screen.cursorRow = newRow;
    }
    fullRedraw();
    handled = 1;
  } else {
    handled = editCell(col, row, action);
  }

  if (handled) projectModified = 1;
  return handled;
}

static int inputScreenNavigation(int keys, int tapCount) {
  if (keys == (keyRight | keyShift)) {
    // To Phrase screen
    int phrase = chipnomadState->project.chains[chain].rows[screen.cursorRow].phrase;
    if (phrase == EMPTY_VALUE_16) {
      screenMessage(0, "Enter a phrase");
    } else {
      screenSetup(&screenPhrase, -1);
    }
    return 1;
  } else if (keys == (keyLeft | keyShift)) {
    // To Song screen
    screenSetup(&screenSong, 0);
    return 1;
  } else if (keys == (keyLeft | keyOpt)) {
    // Previous track
    if (*pSongTrack == 0) return 1;
    if (chipnomadState->project.song[*pSongRow][*pSongTrack - 1] != EMPTY_VALUE_16) {
      *pSongTrack -= 1;
      setup(-1);
      fullRedraw();
    }
    return 1;
  } else if (keys == (keyRight | keyOpt)) {
    // Next track
    if (*pSongTrack == chipnomadState->project.tracksCount - 1) return 1;
    if (chipnomadState->project.song[*pSongRow][*pSongTrack + 1] != EMPTY_VALUE_16) {
      *pSongTrack += 1;
      setup(-1);
      fullRedraw();
    }
    return 1;
  } else if (keys == (keyUp | keyOpt)) {
    // Previous song row
    if (*pSongRow == 0) return 1;
    if (chipnomadState->project.song[*pSongRow - 1][*pSongTrack] != EMPTY_VALUE_16) {
      *pSongRow -= 1;
      setup(-1);
      fullRedraw();
    }
    return 1;
  } else if (keys == (keyDown | keyOpt)) {
    // Next song row
    if (*pSongRow == PROJECT_MAX_LENGTH - 1) return 1;
    if (chipnomadState->project.song[*pSongRow + 1][*pSongTrack] != EMPTY_VALUE_16) {
      *pSongRow += 1;
      setup(-1);
      fullRedraw();
    }
  }
  return 0;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (screen.selectMode == 0 && inputScreenNavigation(keys, tapCount)) return 1;
  return screenInput(&screen, isKeyDown, keys, tapCount);
}

static LoopRange getLoopRange(void) {
  LoopRange range = {0};
  if (screen.selectMode == 1) {
    int startCol, startRow, endCol, endRow;
    getSelectionBounds(&screen, &startCol, &startRow, &endCol, &endRow);
    range.enabled = 1;
    range.level = 1;
    range.startSongRow = *pSongRow;
    range.startChainRow = startRow;
    range.startPhraseRow = 0;
    range.endSongRow = *pSongRow;
    range.endChainRow = endRow;
    range.endPhraseRow = 15;
  }
  return range;
}

///////////////////////////////////////////////////////////////////////////////
//
// Key jazz (desktop only): type a phrase's hex index directly instead of
// incrementing with Up/Down (column 0 only - the transpose column is
// untouched). Same pattern as screen_song.cpp's chain index entry.
//

#ifdef DESKTOP_BUILD

static int keyJazzEnabled = 0;
static int keyJazzEditRow = -1;

int chainKeyJazzHandleRawKey(InputCode input, int isDown) {
  if (input.deviceType != InputDeviceType::keyboard) return 0;

  if (inputIsKeyJazzToggle(input)) {
    if (isDown) {
      keyJazzEnabled = !keyJazzEnabled;
      screenMessage(MESSAGE_TIME, keyJazzEnabled ? "KEY JAZZ ON (Esc to exit)" : "KEY JAZZ OFF");
    }
    return 1;
  }

  // Unlike Phrase/Project, hex digits aren't affected by Shift, and Chain
  // has no Shift-modified key jazz behavior - Shift must keep reaching the
  // normal pipeline so Shift+Right/Left (screen navigation) still works.
  if (!keyJazzEnabled || screen.cursorCol != 0) return 0;

  int digit = inputHexDigitValue(input);
  if (digit < 0) return 0;

  if (isDown) {
    int row = screen.cursorRow;
    uint16_t current = chipnomadState->project.chains[chain].rows[row].phrase;
    uint16_t base = (row == keyJazzEditRow && current != EMPTY_VALUE_16) ? current : 0;
    int value = base * 16 + digit;
    if (value > PROJECT_MAX_PHRASES - 1) value = PROJECT_MAX_PHRASES - 1;
    chipnomadState->project.chains[chain].rows[row].phrase = (uint16_t)value;
    lastPhraseValue = (uint16_t)value;
    keyJazzEditRow = row;
    drawField(0, row, CellState::normal);
  }
  return 1;
}

#endif // DESKTOP_BUILD

static ScreenPlaybackLevel getPlaybackLevel(void) {
  return ScreenPlaybackLevel::chain;
}

const AppScreen screenChain = {
  .init = init,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = getPlaybackLevel
};

LoopRange chainScreenGetLoopRange(void) {
  return getLoopRange();
}
