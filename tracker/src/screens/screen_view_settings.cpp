#include "common.h"
#include "screens.h"
#include "screen_project.h"
#include "screen_view_convert.h"
#include "project_utils.h"
#include "corelib_gfx.h"
#include <string.h>
#include <chrono>

// View settings window: opened from the Project screen's "View" row.
//   row 0: Mode            active project view mode. Editing it (left/right
//                          or tap) opens the CONVERSION screen - switching
//                          modes converts the whole song structure.
//   row 1: Default mode    global app setting (settings.txt): the view mode
//                          NEW projects start in.
//   row 2: Default length  1..16 - app default for new patterns (settings.txt;
//                          visible only while the ACTIVE mode is simplified)
//   row 3: Playhead        "Loop active bar" / "Loop pattern" (per-project
//                          playheadMode; simplified only; affects ONLY the
//                          pattern view playback - the Song screen plays the
//                          same in classic and simplified modes)
//
// The playhead value texts are longer than the space next to their label, so
// they scroll horizontally (marquee: 1 s hold, 180 ms per character, 1 s hold
// at the end, restart).
//
// Rows 2/3 are only valid (the cursor can land on them) in simplified mode;
// viewCellValid enforces this. In classic mode drawField wipes the stale
// option rows so nothing survives a mode change.

#define VIEW_SETTINGS_ROWS (4)

static ScreenData viewData;  // defined below (forward decl for viewEdit)

static const char* modeNames[2] = {
  "Classic",
  "Simplified",
};

static const char* playheadNames[2] = {
  "Loop active bar", // PlayheadMode::loopActiveBar - mimics classic phrase-view
                     // playback on the actively viewed bar
  "Loop pattern",    // PlayheadMode::followPattern - loops the whole pattern
};

static const size_t kMarqueeWidth = 10;  // visible characters for the value
static const size_t kMarqueeValueX = 23; // value column on the Playhead row
                                         // (same column as the other
                                         // toggleboxes)

// Marquee: the position is recomputed per frame from the clock (no tickers
// to reset); a full redraw happens only when the offset changes.
static std::chrono::steady_clock::time_point marqueeSince;
static size_t marqueeOffset;
static int marqueeText;  // which text the marquee is showing

static PlayheadMode currentPlayheadMode(void) {
  return chipnomadState
    ? chipnomadState->project.playheadMode
    : PlayheadMode::followPattern;
}

static size_t marqueePositionFor(const char* text) {
  const size_t length = strlen(text);
  if (length <= kMarqueeWidth) return 0;
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - marqueeSince).count();
  const auto travel = (long long)(length - kMarqueeWidth) * 180;
  const auto phase = elapsed % (1000 + travel + 1000);
  if (phase < 1000) return 0;
  long long pos = (phase - 1000) / 180 + 1;
  if (pos > (long long)(length - kMarqueeWidth)) pos = length - kMarqueeWidth;
  return (size_t)pos;
}

static void resetMarquee(void) {
  marqueeSince = std::chrono::steady_clock::now();
  marqueeOffset = 0;
  marqueeText = (int)currentPlayheadMode();
}

static int simplifiedActive(void) {
  return chipnomadState &&
    chipnomadState->project.viewMode == ViewMode::simplified;
}

static int getColumnCount(int) { return 1; }
static void noHeader(int, CellState) {}
static void viewStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 0, "VIEW");
}

// Fixed description shown under the cursor while it rests on the Mode row
// (wrapped over the three bottom canvas rows 17..19):
//   Simplified: patterns with adjustable length instead of chains/phrases
//   Classic:    the LSDJ-style song -> chain -> phrase structure
static const char* modeDescription(ViewMode mode) {
  return mode == ViewMode::simplified
    ? "Uses patterns with adjustable length instead of chains and phrases"
    : "Follows LSDJ-style song - chain - phrase structure";
}

#define VIEW_DESC_WIDTH (40)
#define VIEW_DESC_ROWS (3)
#define VIEW_DESC_TOP_Y (17)

static int lastDescVisible = 0;  // session: were the rows just drawn?

// Wrap `text` into lines of at most `width` chars; breaks at spaces.
// Returns the line count; lines[] hold up to VIEW_DESC_ROWS entries.
static int wrapDescription(const char* text, int width,
  char lines[][VIEW_DESC_WIDTH + 1]) {
  int lineCount = 0;
  const char* p = text;
  while (*p && lineCount < VIEW_DESC_ROWS) {
    while (*p == ' ') p++;
    if (!*p) break;
    int take = 0;
    int lastSpace = -1;
    while (p[take] && take < width) {
      if (p[take] == ' ') lastSpace = take;
      take++;
    }
    if (p[take] && lastSpace > 0) take = lastSpace;  // break at the space
    if (take == 0) take = 1;                         // defensive
    memcpy(lines[lineCount], p, take);
    lines[lineCount][take] = 0;
    lineCount++;
    p += take;
  }
  return lineCount;
}

static void drawDescriptionRow(void) {
  const int onModeRow = viewData.cursorRow == 0;
  if (!onModeRow) {
    if (lastDescVisible) {
      // Cursor left the Mode row: wipe the description.
      gfxSetBgColor(appSettings.colorScheme.background);
      gfxClearRect(0, VIEW_DESC_TOP_Y, 40, VIEW_DESC_ROWS);
      lastDescVisible = 0;
    }
    return;
  }

  char lines[VIEW_DESC_ROWS][VIEW_DESC_WIDTH + 1];
  const int count = wrapDescription(
    modeDescription(chipnomadState ? chipnomadState->project.viewMode
                                   : ViewMode::classic),
    VIEW_DESC_WIDTH, lines);
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  for (int i = 0; i < count; i++) {
    gfxPrint(0, VIEW_DESC_TOP_Y + i, lines[i]);
  }
  if (count < VIEW_DESC_ROWS) {
    gfxSetBgColor(appSettings.colorScheme.background);
    gfxClearRect(0, VIEW_DESC_TOP_Y + count, 40,
      VIEW_DESC_ROWS - count);
  }
  lastDescVisible = 1;
}
static void viewCursor(int, int row) {
  if (row == 0) gfxCursor(23, 2, 10);       // "Classic"/"Simplified" (Mode)
  else if (row == 1) gfxCursor(23, 3, 10);  // "Classic"/"Simplified" (Default)
  else if (row == 2) gfxCursor(23, 4, 2);   // default length: 1..16
  else if (row == 3) gfxCursor(23, 5, 10);  // playhead marquee window
}
static void viewField(int, int row, CellState state) {
  const ColorScheme cs = appSettings.colorScheme;
  Project* p = chipnomadState ? &chipnomadState->project : NULL;

  if (row == 0) {
    gfxSetFgColor(cs.textDefault);
    gfxPrint(0, 2, "Mode");
    // Clear the value area first - "Simplified" is 10 chars, "Classic" is 7,
    // so without clearing, an earlier "Simplified" leaves stale "ied".
    gfxClearRect(23, 2, 10, 1);
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxPrint(23, 2, p && p->viewMode == ViewMode::simplified ? "Simplified" : "Classic");
  } else if (row == 1) {
    gfxSetFgColor(cs.textDefault);
    gfxPrint(0, 3, "Default mode");
    gfxClearRect(23, 3, 10, 1);
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxPrint(23, 3, modeNames[appSettings.defaultViewMode ? 1 : 0]);
  } else if (row == 2) {
    if (!simplifiedActive()) {
      // Hidden in classic mode: wipe any stale text from a previous
      // simplified session (drawField still runs for visible rows).
      gfxClearRect(0, 4, 40, 1);
      return;
    }
    gfxSetFgColor(cs.textDefault);
    gfxPrint(0, 4, "Default length");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxPrintf(23, 4, "%02d", appSettings.defaultPatternLength);
  } else if (row == 3) {
    if (!simplifiedActive()) {
      gfxClearRect(0, 5, 40, 1);
      return;
    }
    gfxSetFgColor(cs.textDefault);
    gfxPrint(0, 5, "Playhead");
    // Value scrolls when it does not fit the 8-char window.
    const char* text = playheadNames[p ? (int)p->playheadMode : 1];
    const size_t length = strlen(text);
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxClearRect(kMarqueeValueX, 5, kMarqueeWidth, 1);
    if (length > kMarqueeWidth) {
      const size_t remaining = length - marqueeOffset;
      const size_t visible = remaining < kMarqueeWidth ? remaining : kMarqueeWidth;
      gfxPrintf(kMarqueeValueX, 5, "%.*s", (int)visible, text + marqueeOffset);
    } else {
      gfxPrint(kMarqueeValueX, 5, text);
    }
  }
}

static int viewEdit(int, int row, CellEditAction action) {
  Project* p = chipnomadState ? &chipnomadState->project : NULL;
  if (!p) return 0;

  if (row == 0) {
    // Active mode: ONLY EDIT + LEFT/RIGHT (increase/decrease) changes the
    // toggle and opens the conversion screen for the OTHER mode. A plain
    // EDIT tap changes nothing (the warning prompt belongs to an actual
    // mode switch, not to merely visiting the row).
    if (action == CellEditAction::increase || action == CellEditAction::decrease) {
      ViewMode target = (p->viewMode == ViewMode::simplified)
        ? ViewMode::classic : ViewMode::simplified;
      screenViewConvertSetup(&screenViewSettings, target);
      return 1;
    }
    if (action == CellEditAction::tap || action == CellEditAction::doubleTap) {
      screenMessage(MESSAGE_TIME, "EDIT + LEFT/RIGHT to switch");
    }
    return 1;  // Always consume: nothing here may change the mode directly.
  }

  if (row == 1) {
    // Default mode (app setting): ONLY EDIT + LEFT/RIGHT toggles it, same
    // clicking logic as the Mode row. A plain EDIT tap changes nothing.
    if (action == CellEditAction::increase || action == CellEditAction::decrease) {
      appSettings.defaultViewMode = appSettings.defaultViewMode ? 0 : 1;
      if (settingsSave() != 0) screenMessage(MESSAGE_TIME_ERROR, "Could not save setting");
      screenFullRedraw(&viewData);
      return 1;
    }
    if (action == CellEditAction::tap || action == CellEditAction::doubleTap) {
      screenMessage(MESSAGE_TIME, "EDIT + LEFT/RIGHT to switch");
    }
    return 1;  // Always consume: nothing here may change the default directly.
  }

  if (row == 2) {
    // Default length (app setting, simplified active mode only).
    if (!simplifiedActive()) return 0;
    if (action != CellEditAction::tap && action != CellEditAction::doubleTap &&
        action != CellEditAction::increase && action != CellEditAction::decrease &&
        action != CellEditAction::increaseBig && action != CellEditAction::decreaseBig &&
        action != CellEditAction::clear) return 0;
    uint8_t value = appSettings.defaultPatternLength;
    int handled = edit8noLast(action, &value, 4, 1, PROJECT_MAX_PATTERN_LENGTH);
    if (handled) {
      appSettings.defaultPatternLength = value;
      if (settingsSave() != 0) screenMessage(MESSAGE_TIME_ERROR, "Could not save setting");
    }
    return handled;
  }

  if (row == 3) {
    // Playhead (project setting, simplified active mode only).
    if (!simplifiedActive()) return 0;
    if (action == CellEditAction::tap || action == CellEditAction::doubleTap ||
        action == CellEditAction::increase || action == CellEditAction::decrease) {
      p->playheadMode = (p->playheadMode == PlayheadMode::loopActiveBar)
        ? PlayheadMode::followPattern : PlayheadMode::loopActiveBar;
      projectModified = 1;
      chipnomadQueueProjectRefresh(chipnomadState);
      resetMarquee();
      screenFullRedraw(&viewData);
      return 1;
    }
    if (action == CellEditAction::clear) {
      p->playheadMode = PlayheadMode::followPattern;
      projectModified = 1;
      chipnomadQueueProjectRefresh(chipnomadState);
      resetMarquee();
      screenFullRedraw(&viewData);
      return 1;
    }
    return 0;
  }

  return 0;
}

static int viewCellValid(int, int row) {
  if (row == 0 || row == 1) return 1;
  // Rows 2/3 (Default length / Playhead) only in simplified mode.
  return simplifiedActive() ? 1 : 0;
}

static void initData() {
  viewData = (ScreenData){VIEW_SETTINGS_ROWS, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::none, getColumnCount, viewStatic, viewCursor, NULL, noHeader, noHeader, viewField, viewEdit, NULL, NULL, viewCellValid, NULL};
}

static void viewRedraw(void) { screenFullRedraw(&viewData); }

static void draw(void) {
  // Mode-row description lives in the bottom canvas rows; it must be
  // repainted whenever the cursor changes rows (cheap: at most 3 prints).
  drawDescriptionRow();

  if (!simplifiedActive()) return;
  if (marqueeText != (int)chipnomadState->project.playheadMode) {
    // Value changed: restart the marquee for the new text.
    resetMarquee();
    viewRedraw();
    return;
  }
  const char* text = playheadNames[marqueeText];
  size_t offset = marqueePositionFor(text);
  if (offset != marqueeOffset) {
    marqueeOffset = offset;
    viewRedraw();
  }
}

static void setup(int input) {
  (void)input;
  initData();
  resetMarquee();
  lastDescVisible = 0;
  viewRedraw();
}

static int viewInputFn(int isKeyDown, int keys, int taps) {
  if (isKeyDown && keys == keyOpt) {
    screenSetup(&screenProject, 0);
    return 1;
  }
  return screenInput(&viewData, isKeyDown, keys, taps);
}

static ScreenPlaybackLevel playbackLevel(void) { return ScreenPlaybackLevel::song; }

const AppScreen screenViewSettings = {NULL, setup, viewRedraw, draw, viewInputFn
, playbackLevel};
