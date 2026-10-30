#include "common.h"
#include "screens.h"
#include "screen_project.h"
#include "project_utils.h"
#include "corelib_gfx.h"
#include <string.h>
#include <chrono>

// View settings window: opened from the Project screen's "View" row.
// Shows the CURRENT project's view mode (read-only — the mode is stored per
// project and chosen at creation; conversion between modes is deferred) plus
// the simplified-view options:
//   row 0: View mode        "Classic" / "Simplified" (read-only)
//   row 1: Default length   1..16 (app default for new patterns;
//                           settings.txt; shown in simplified mode)
//   row 2: Playhead         "Loop active bar" / "Loop pattern"
//                           (per-project playheadMode; shown in simplified
//                           mode; affects ONLY the pattern view playback —
//                           the Song screen plays the same in classic and
//                           simplified modes)
//
// The playhead value texts are longer than the space next to their label, so
// they scroll horizontally (same marquee feel as the mode chooser helper: 1 s
// hold, 180 ms per character, 1 s hold at the end, restart).
//
// Rows 1/2 are only valid (the cursor can land on them) in simplified mode;
// viewCellValid enforces this. In classic mode drawField wipes the stale
// option rows so nothing survives a mode change.

#define VIEW_SETTINGS_ROWS (3)

static ScreenData viewData;  // defined below (forward decl for viewEdit)

static const char* playheadNames[2] = {
  "Loop active bar", // PlayheadMode::loopActiveBar — mimics classic phrase-view
                     // playback on the actively viewed bar
  "Loop pattern",    // PlayheadMode::followPattern — loops the whole pattern
};

static const size_t kMarqueeWidth = 8;   // visible characters for the value
static const size_t kMarqueeValueX = 25; // value column on the Playhead row

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
static void viewCursor(int, int row) {
  if (row == 0) gfxCursor(23, 2, 10);       // "Classic"/"Simplified"
  else if (row == 1) gfxCursor(23, 3, 2);   // default length: 1..16
  else if (row == 2) gfxCursor(23, 4, 8);   // playhead marquee window
}
static void viewField(int, int row, CellState state) {
  const ColorScheme cs = appSettings.colorScheme;
  Project* p = chipnomadState ? &chipnomadState->project : NULL;

  if (row == 0) {
    gfxSetFgColor(cs.textDefault);
    gfxPrint(0, 2, "View mode");
    // Clear the value area first — "Simplified" is 10 chars, "Classic" is 7,
    // so without clearing, an earlier "Simplified" leaves stale "ied".
    gfxClearRect(23, 2, 10, 1);
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxPrint(23, 2, p && p->viewMode == ViewMode::simplified ? "Simplified" : "Classic");
  } else if (row == 1) {
    if (!simplifiedActive()) {
      // Hidden in classic mode: wipe any stale text from a previous
      // simplified session (drawField still runs for visible rows).
      gfxClearRect(0, 3, 40, 1);
      return;
    }
    gfxSetFgColor(cs.textDefault);
    gfxPrint(0, 3, "Default length");
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxPrintf(23, 3, "%02d", appSettings.defaultPatternLength);
  } else if (row == 2) {
    if (!simplifiedActive()) {
      gfxClearRect(0, 4, 40, 1);
      return;
    }
    gfxSetFgColor(cs.textDefault);
    gfxPrint(0, 4, "Playhead");
    // Value scrolls when it does not fit the 8-char window.
    const char* text = playheadNames[p ? (int)p->playheadMode : 1];
    const size_t length = strlen(text);
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
    gfxClearRect(kMarqueeValueX, 4, kMarqueeWidth, 1);
    if (length > kMarqueeWidth) {
      const size_t remaining = length - marqueeOffset;
      const size_t visible = remaining < kMarqueeWidth ? remaining : kMarqueeWidth;
      gfxPrintf(kMarqueeValueX, 4, "%.*s", (int)visible, text + marqueeOffset);
    } else {
      gfxPrint(kMarqueeValueX, 4, text);
    }
  }
}

static int viewEdit(int, int row, CellEditAction action) {
  Project* p = chipnomadState ? &chipnomadState->project : NULL;
  if (!p) return 0;

  if (row == 0) {
    // View mode is read-only (stored per project, chosen at creation).
    if (action == CellEditAction::tap || action == CellEditAction::doubleTap ||
        action == CellEditAction::increase || action == CellEditAction::decrease) {
      screenMessage(MESSAGE_TIME, "Set at project creation");
    }
    return 1; // Always consume: nothing here may change the mode.
  }

  if (row == 1) {
    // Default length (app setting, simplified mode only).
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

  if (row == 2) {
    // Playhead (project setting, simplified mode only).
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
  if (row == 0) return 1;
  // Rows 1/2 (Default length / Playhead) only in simplified mode.
  return simplifiedActive() ? 1 : 0;
}

static void initData() {
  viewData = (ScreenData){VIEW_SETTINGS_ROWS, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::none, getColumnCount, viewStatic, viewCursor, NULL, noHeader, noHeader, viewField, viewEdit, NULL, NULL, viewCellValid, NULL};
}

static void viewRedraw(void) { screenFullRedraw(&viewData); }

static void draw(void) {
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

const AppScreen screenViewSettings = {NULL, setup, viewRedraw, draw, viewInputFn, playbackLevel};
