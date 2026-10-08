#include "screens.h"
#include "corelib_gfx.h"
#include "chipnomad_lib.h"
#include "common.h"
#include <cstring>

static const char* const noteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

static uint16_t activeMask() {
  Project* p = &chipnomadState->project;
  return p->scalePreset == scaleCustom ? p->scaleCustomMask : scalePresetMask(p->scalePreset);
}

static int noteBit(int note) {
  int bit = note - chipnomadState->project.scaleRoot;
  return bit < 0 ? bit + 12 : bit;
}

static void redraw(void);

static int columns(int row) {
  return row >= 3 && row <= 8 ? 3 : 1;
}

static void drawStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 0, "SCALE");
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(0, 2, "Mode");
  gfxPrint(0, 3, "Root");
  gfxPrint(0, 4, "Scale");
  gfxPrint(0, 6, "Tracks");
  gfxPrint(17, 6, "Notes");
}

static void drawCursor(int col, int row) {
  if (row == 0) gfxCursor(8, 2, 9);
  else if (row == 1) gfxCursor(8, 3, 2);
  else if (row == 2) gfxCursor(8, 4, strlen(scalePresetName(chipnomadState->project.scalePreset)));
  else if (col == 0) gfxCursor(8, row + 4, 3);
  else if (col == 1) gfxCursor(20, row + 4, 3);
  else gfxCursor(31, row + 4, 3);
}

static void drawField(int col, int row, CellState state) {
  Project* p = &chipnomadState->project;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  if (row == 0) {
    gfxPrint(8, 2, p->scaleMode ? "Note Lock" : "Quantizer");
  } else if (row == 1) {
    gfxPrint(8, 3, noteNames[p->scaleRoot]);
  } else if (row == 2) {
    gfxClearRect(8, 4, 16, 1);
    gfxPrint(8, 4, scalePresetName(p->scalePreset));
  } else if (col == 0) {
    int track = row - 3;
    gfxPrintf(0, row + 4, "Track %d [%c]", track + 1, (p->scaleTracksMask & (1u << track)) ? 'x' : ' ');
  } else if (row <= 8) {
    int note = row - 3 + (col == 2 ? 6 : 0);
    uint16_t mask = activeMask();
    gfxPrintf(col == 1 ? 17 : 28, row + 4, "%2s [%c]", noteNames[note], (mask & (1u << noteBit(note))) ? 'x' : ' ');
  }
}

static int edit(int col, int row, CellEditAction action) {
  Project* p = &chipnomadState->project;
  int handled = 0;
  if (row == 0) {
    if (action == CellEditAction::tap) {
      p->scaleMode = p->scaleMode ? 0 : 1;
      handled = 1;
    } else {
      handled = edit8noLast(action, &p->scaleMode, 1, 0, 1);
    }
    if (handled) redraw();
  } else if (row == 1) {
    handled = edit8noLast(action, &p->scaleRoot, 1, 0, 11);
    if (handled) redraw();
  } else if (row == 2) {
    uint8_t preset = (uint8_t)p->scalePreset;
    handled = edit8noLast(action, &preset, 1, 0, scalePresetCount - 1);
    if (handled) p->scalePreset = (ScalePreset)preset;
    if (handled) redraw();
  } else if (col == 0) {
    int track = row - 3;
    if (action == CellEditAction::tap) {
      p->scaleTracksMask ^= 1u << track;
      handled = 1;
    } else {
      uint8_t enabled = (p->scaleTracksMask & (1u << track)) ? 1 : 0;
      handled = edit8noLast(action, &enabled, 1, 0, 1);
      if (handled) {
        if (enabled) p->scaleTracksMask |= 1u << track;
        else p->scaleTracksMask &= ~(1u << track);
      }
    }
  } else if (row <= 8) {
    int note = row - 3 + (col == 2 ? 6 : 0);
    int bit = noteBit(note);
    uint8_t enabled = (activeMask() & (1u << bit)) != 0;
    if (action == CellEditAction::tap) { enabled ^= 1; handled = 1; }
    else handled = edit8noLast(action, &enabled, 1, 0, 1);
    if (handled) {
      if (p->scalePreset != scaleCustom) p->scaleCustomMask = activeMask();
      p->scalePreset = scaleCustom;
      if (enabled) p->scaleCustomMask |= 1u << bit;
      else p->scaleCustomMask &= ~(1u << bit);
      if (!p->scaleCustomMask) p->scaleCustomMask = 1;
      redraw();
    }
  }
  if (handled) {
    projectModified = 1;
    chipnomadQueuePlaybackScale(chipnomadState, p->scaleRoot, p->scalePreset);
  }
  return handled;
}

static void rowHeader(int row, CellState state) {}
static void colHeader(int col, CellState state) {}

static ScreenData data = {
  .rows = 11, .cursorRow = 0, .cursorCol = 0, .topRow = 0, .selectMode = -1,
  .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none, .getColumnCount = columns,
  .drawStatic = drawStatic, .drawCursor = drawCursor, .drawSelection = NULL,
  .drawRowHeader = rowHeader, .drawColHeader = colHeader, .drawField = drawField,
  .onEdit = edit, .onInput = NULL, .onRawInput = NULL, .isCellValid = NULL, .getLoopRange = NULL,
};

// Mode-cell helper (session-only): while the cursor rests on the Mode
// cell and no other message is showing, the status bar shows a fixed
// description of the active mode; toggling the mode swaps the text
// immediately. Same ownership pattern as the sample screen's slice hints:
// the bar is ours when it is empty or shows the text we set last.
static char scaleHintOwnedText[48];

static void scaleUpdateHint(int ownsBar) {
  const char* issued = NULL;
  if (data.cursorRow == 0 && data.cursorCol == 0) {
    issued = chipnomadState->project.scaleMode ?
      "Snaps sequencer to scale" : "Quantizes notes on playback";
  }
  if (issued) {
    if (!ownsBar || strcmp(issued, scaleHintOwnedText) != 0) {
      screenMessage(2, "%s", issued);
      snprintf(scaleHintOwnedText, sizeof(scaleHintOwnedText), "%s",
               screenGetActiveMessage());
    }
  } else if (scaleHintOwnedText[0]) {
    // Cursor left the Mode cell: drop our hint.
    screenClearMessage();
    scaleHintOwnedText[0] = '\0';
  }
}

static void setup(int input) {
  data.topRow = 0;
  if (data.cursorRow >= data.rows) {
    data.cursorRow = 0;
    data.cursorCol = 0;
  }
  scaleHintOwnedText[0] = '\0';
}
static void redraw(void) { screenFullRedraw(&data); }
static void fullRedraw(void) { screenFullRedraw(&data); }
static void draw(void) {
  const char* activeMessage = screenGetActiveMessage();
  if (activeMessage[0] == '\0') {
    scaleUpdateHint(0);
  } else if (strcmp(activeMessage, scaleHintOwnedText) == 0) {
    scaleUpdateHint(1);
  } else {
    // A foreign message (action feedback) pauses the hint until it expires.
    scaleHintOwnedText[0] = '\0';
  }
}
static int onInput(int isKeyDown, int keys, int tapCount) {
  if (keys == keyOpt) { screenSetup(&screenProject, 0); return 1; }
  return screenInput(&data, isKeyDown, keys, tapCount);
}

const AppScreen screenScale = {
  .init = NULL, .setup = setup, .fullRedraw = fullRedraw, .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = []() { return ScreenPlaybackLevel::song; },
};
