#include "screen_project.h"
#include "corelib_gfx.h"
#include <string.h>
#include "chipnomad_lib.h"
#include "common.h"

static const char viewLabel[] = "View";

static int getColumnCount(int row) {
  if (row < SCR_PROJECT_ROWS) return projectCommonColumnCount(row);
  return 1;
}

static void drawStatic(void) {
  projectCommonDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textLabel);
  gfxPrint(0, 16, "Scale");
  gfxPrint(0, 17, viewLabel);
}

static void drawCursor(int col, int row) {
  if (row < SCR_PROJECT_ROWS) return projectCommonDrawCursor(col, row);
  if (row == SCR_PROJECT_ROWS) {
    gfxCursor(0, 16, 5);
  } else if (row == SCR_PROJECT_ROWS + 1) {
    gfxCursor(0, 17, sizeof(viewLabel) - 1);
  }
}

static void drawField(int col, int row, CellState state) {
  if (row < SCR_PROJECT_ROWS) return projectCommonDrawField(col, row, state);

  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);

  if (row == SCR_PROJECT_ROWS) {
    gfxClearRect(0, 16, 5, 1);
    gfxPrint(0, 16, "Scale");
  } else if (row == SCR_PROJECT_ROWS + 1) {
    gfxClearRect(0, 17, sizeof(viewLabel) - 1, 1);
    gfxPrint(0, 17, viewLabel);
  }
}

static int onEdit(int col, int row, enum CellEditAction action) {
  if (row < SCR_PROJECT_ROWS) return projectCommonOnEdit(col, row, action);
  if (action != CellEditAction::tap && action != CellEditAction::doubleTap) return 0;

  if (row == SCR_PROJECT_ROWS) {
    screenSetup(&screenScale, 0);
  } else if (row == SCR_PROJECT_ROWS + 1) {
    // View window: this project's view mode (read-only), default pattern
    // length and the playhead mode for the simplified view.
    screenSetup(&screenViewSettings, 0);
  }
  return 0;
}

static int isButtonCell(int, int row) { return row >= 7; }

ScreenData screenProjectExtra = {
  .rows = SCR_PROJECT_ROWS + 2,
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
  .drawRowHeader = NULL,
  .drawColHeader = NULL,
  .drawField = drawField,
  .onEdit = onEdit,
  .onInput = NULL,
  .onRawInput = NULL,
  .isCellValid = NULL,
  .getLoopRange = NULL,
  .isButtonCell = isButtonCell,
};
