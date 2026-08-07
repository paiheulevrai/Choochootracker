#include "screens.h"
#include "audio_manager.h"
#include "common.h"
#include "corelib_gfx.h"

static void fullRedraw(void);

static int columnCount(int row) { return 3; }

static void drawStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 0, "MIXER");
  gfxPrint(8, 2, "LEVEL");
  gfxPrint(18, 2, "MUTE");
  gfxPrint(26, 2, "SOLO");
}

static void drawCursor(int col, int row) {
  static const int x[] = {8, 18, 26};
  static const int width[] = {4, 3, 3};
  gfxCursor(x[col], 3 + row, width[col]);
}

static void drawField(int col, int row, CellState state) {
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  if (col == 0) {
    gfxPrintf(0, 3 + row, "Track %d %03d%%", row + 1, chipnomadState->project.trackVolume[row]);
  } else if (col == 1) {
    gfxPrint(18, 3 + row, audioManager.trackStates[row] == TRACK_MUTED ? "ON " : "OFF");
  } else {
    gfxPrint(26, 3 + row, audioManager.trackStates[row] == TRACK_SOLO ? "ON " : "OFF");
  }
}

static int onEdit(int col, int row, CellEditAction action) {
  if (col == 0) {
    int handled = edit8noLast(action, &chipnomadState->project.trackVolume[row], 10, 0, 100);
    if (handled) projectModified = 1;
    return handled;
  }
  if (action != CellEditAction::tap) return 0;
  if (col == 1) audioManager.toggleTrackMute(row);
  else audioManager.toggleTrackSolo(row);
  fullRedraw();
  return 1;
}

static ScreenData screen = {
  .rows = PROJECT_MAX_TRACKS,
  .cursorRow = 0,
  .cursorCol = 0,
  .topRow = 0,
  .selectMode = -1,
  .selectStartRow = 0,
  .selectStartCol = 0,
  .selectAnchorRow = 0,
  .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::song,
  .getColumnCount = columnCount,
  .drawStatic = drawStatic,
  .drawCursor = drawCursor,
  .drawSelection = NULL,
  .drawRowHeader = NULL,
  .drawColHeader = NULL,
  .drawField = drawField,
  .onEdit = onEdit,
  .onInput = NULL,
  .onRawInput = NULL,
  .isCellValid = NULL,
  .getLoopRange = NULL,
};

static void setup(int input) {}
static void fullRedraw(void) { screenFullRedraw(&screen); }
static void draw(void) {}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (isKeyDown && keys == (keyUp | keyShift)) {
    screenSetup(&screenProject, 0);
    return 1;
  }
  return screenInput(&screen, isKeyDown, keys, tapCount);
}

static ScreenPlaybackLevel getPlaybackLevel(void) { return ScreenPlaybackLevel::song; }

const AppScreen screenMixer = {
  .init = NULL,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = getPlaybackLevel,
};
