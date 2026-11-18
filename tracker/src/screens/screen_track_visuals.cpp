#include "screens.h"
#include "common.h"
#include "corelib_gfx.h"

static constexpr int displayX = 4;
static constexpr int displayWidth = 14;
static constexpr int allRow = PROJECT_MAX_TRACKS;
static constexpr int doneRow = allRow + 1;
static void fullRedraw();

static int columnCount(int row) { return row == allRow ? 2 : 1; }
static void drawStatic() {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 0, "TRACK VISUALS");
  gfxPrint(0, 2, "TRK");
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrint(0, 16, "DETAIL: waveform + overlays");
  gfxPrint(0, 17, "AUDIO: summed track output");
  gfxPrint(0, 18, "EDIT toggles; SHIFT+LEFT back");
}
static void drawCursor(int col, int row) {
  if (row < allRow) gfxCursor(displayX, 3 + row, displayWidth);
  else if (row == allRow) gfxCursor(col ? 15 : 0, 12, col ? 9 : 12);
  else gfxCursor(0, 14, 4);
}
static void drawRowHeader(int row, CellState state) {
  if (row >= allRow) return;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.cursor : appSettings.colorScheme.textInfo);
  gfxPrintf(0, 3 + row, "%d", row + 1);
}
static void drawColHeader(int col, CellState state) {
  if (col != 0) return;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.cursor : appSettings.colorScheme.textTitles);
  gfxPrint(displayX, 2, "DISPLAY");
}
static void drawField(int col, int row, CellState state) {
  const auto cs = appSettings.colorScheme;
  gfxSetFgColor(state == CellState::focus ? cs.cursor : cs.textValue);
  if (row < allRow) {
    gfxPrint(displayX, 3 + row, appSettings.trackVisuals[row].mode == TrackVisualMode::audio ?
      "Audio waveform" : "Detailed      ");
  } else if (row == allRow) gfxPrint(col ? 15 : 0, 12, col ? "All audio" : "All detailed");
  else gfxPrint(0, 14, "Done");
}
static void done() {
  if (settingsSave() == 0) screenSetup(&screenSettings, 0);
  else screenMessage(MESSAGE_TIME, "Could not save track visuals");
}
static int onEdit(int col, int row, CellEditAction action) {
  if (row == doneRow) {
    if (action != CellEditAction::tap) return 0;
    done();
    return 1;
  }
  if (row == allRow) {
    if (action != CellEditAction::tap) return 0;
    for (auto& visual : appSettings.trackVisuals) {
      visual.mode = col ? TrackVisualMode::audio : TrackVisualMode::detailed;
    }
    fullRedraw();
    return 1;
  }
  auto& visual = appSettings.trackVisuals[row];
  uint8_t value = (uint8_t)visual.mode;
  if (action == CellEditAction::tap || action == CellEditAction::doubleTap) value ^= 1;
  else if (!edit8noLast(action, &value, 1, 0, 1)) return 0;
  visual.mode = (TrackVisualMode)value;
  return 1;
}
static ScreenData screen = {
  .rows = doneRow + 1,
  .cursorRow = 0, .cursorCol = 0, .topRow = 0, .selectMode = -1,
  .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = columnCount, .drawStatic = drawStatic, .drawCursor = drawCursor,
  .drawSelection = nullptr, .drawRowHeader = drawRowHeader, .drawColHeader = drawColHeader,
  .drawField = drawField, .onEdit = onEdit, .onInput = nullptr, .onRawInput = nullptr,
  .isCellValid = nullptr, .getLoopRange = nullptr,
};
static void setup(int) {}
static void fullRedraw() { screenFullRedraw(&screen); }
static void draw() {}
static int onInput(int isKeyDown, int keys, int tapCount) {
  if (keys == (keyShift | keyLeft)) {
    if (isKeyDown) done();
    return 1;
  }
  return screenInput(&screen, isKeyDown, keys, tapCount);
}
static ScreenPlaybackLevel playbackLevel() { return ScreenPlaybackLevel::song; }
const AppScreen screenTrackVisuals = {nullptr, setup, fullRedraw, draw, onInput, playbackLevel};
