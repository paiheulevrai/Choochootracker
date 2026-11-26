#include "screen_fur_channels.h"
#include "common.h"
#include "corelib_gfx.h"
#include "screen_layout.h"
#include <string.h>

// Desktop and Android only, like the Furnace import itself.
#if defined(DESKTOP_BUILD) || defined(ANDROID_BUILD)

#define MAX_LISTED (256)
#define MAX_PICKED (PROJECT_MAX_TRACKS)

static FurChannelInfo listed[MAX_LISTED];
static uint8_t chosen[MAX_LISTED];
static int listedCount = 0;
static void (*onPicked)(const int*, int) = NULL;
static void (*onCancelled)(void) = NULL;

static int chosenCount(void) {
  int n = 0;
  for (int i = 0; i < listedCount; i++) n += chosen[i];
  return n;
}

static int columnCount(int row);
static void drawStatic(void);
static void drawCursor(int col, int row);
static void drawField(int col, int row, CellState state);
static int onEdit(int col, int row, CellEditAction action);
static void drawRowHeader(int row, CellState state);
static void drawColHeader(int col, CellState state);

static ScreenData screen = {
  .rows = 1,
  .cursorRow = 0, .cursorCol = 0, .topRow = 0, .selectMode = -1,
  .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = columnCount, .drawStatic = drawStatic, .drawCursor = drawCursor,
  .drawSelection = nullptr, .drawRowHeader = drawRowHeader, .drawColHeader = drawColHeader,
  .drawField = drawField, .onEdit = onEdit, .onInput = nullptr, .onRawInput = nullptr,
  .isCellValid = nullptr, .getLoopRange = nullptr,
};

static int columnCount(int row) { return 1; }

static void drawStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 0, "FURNACE CHANNELS");
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrintf(0, 1, "Pick up to %d (%d picked)", MAX_PICKED, chosenCount());
}

static void drawRowHeader(int row, CellState state) {}
static void drawColHeader(int col, CellState state) {}

static void drawCursor(int col, int row) {
  gfxCursor(0, 3 + row - screen.topRow, row < listedCount ? 38 : 4);
}

static void drawField(int col, int row, CellState state) {
  if (row < screen.topRow || row >= screen.topRow + screenVisibleRows()) return;
  const ColorScheme cs = appSettings.colorScheme;
  int y = 3 + row - screen.topRow;
  gfxClearRect(0, y, 38, 1);
  gfxSetFgColor(state == CellState::focus ? cs.cursor : cs.textDefault);
  if (row < listedCount) {
    gfxPrintf(0, y, "[%c] %02d %s", chosen[row] ? 'X' : ' ', listed[row].channel + 1, listed[row].name);
    gfxPrintf(30, y, "%5d", listed[row].notes);
  } else {
    gfxPrint(0, y, "Done");
  }
}

static void redrawHeader(void) {
  gfxClearRect(0, 1, 38, 1);
  drawStatic();
}

static int onEdit(int col, int row, CellEditAction action) {
  if (action != CellEditAction::tap && action != CellEditAction::doubleTap) return 0;
  if (row < listedCount) {
    if (!chosen[row] && chosenCount() >= MAX_PICKED) {
      screenMessage(MESSAGE_TIME, "At most %d channels", MAX_PICKED);
      return 1;
    }
    chosen[row] ^= 1;
    redrawHeader();
    return 1;
  }
  if (chosenCount() == 0) {
    screenMessage(MESSAGE_TIME, "Pick at least one channel");
    return 1;
  }
  int channels[MAX_PICKED];
  int n = 0;
  for (int i = 0; i < listedCount && n < MAX_PICKED; i++) if (chosen[i]) channels[n++] = listed[i].channel;
  if (onPicked) onPicked(channels, n);
  return 1;
}

void furChannelsPick(const FurChannelInfo* channels, int count,
                     void (*picked)(const int*, int), void (*cancelled)(void)) {
  listedCount = count < MAX_LISTED ? count : MAX_LISTED;
  memcpy(listed, channels, listedCount * sizeof(FurChannelInfo));
  for (int i = 0; i < listedCount; i++) chosen[i] = listed[i].picked ? 1 : 0;
  onPicked = picked;
  onCancelled = cancelled;
}

static void setup(int) {
  screen.rows = listedCount + 1;
  screen.cursorRow = 0;
  screen.cursorCol = 0;
  screen.topRow = 0;
}
static void fullRedraw() { screenFullRedraw(&screen); }
static void draw() {}
static int onInput(int isKeyDown, int keys, int tapCount) {
  if (keys == keyOpt || keys == (keyShift | keyLeft)) {
    if (isKeyDown && onCancelled) onCancelled();
    return 1;
  }
  return screenInput(&screen, isKeyDown, keys, tapCount);
}
static ScreenPlaybackLevel playbackLevel() { return ScreenPlaybackLevel::none; }
const AppScreen screenFurChannels = {nullptr, setup, fullRedraw, draw, onInput, playbackLevel};

#else

void furChannelsPick(const FurChannelInfo*, int, void (*)(const int*, int), void (*)(void)) {}
const AppScreen screenFurChannels = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

#endif
