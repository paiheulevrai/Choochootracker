#include "screen_project.h"
#include "corelib_gfx.h"
#include <string.h>
#include "chipnomad_lib.h"
#include "common.h"

static int getColumnCount(int row) {
  if (row < SCR_PROJECT_ROWS) return projectCommonColumnCount(row);
  return 1;
}

static void drawStatic(void) {
  projectCommonDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(0, 12, "Pitch table");
  gfxPrint(0, 13, "Scale");
}

static void drawCursor(int col, int row) {
  if (row < SCR_PROJECT_ROWS) return projectCommonDrawCursor(col, row);
  if (row == SCR_PROJECT_ROWS) {
    gfxCursor(13, 12, strlen(chipnomadState->project.pitchTable.name));
  } else if (row == SCR_PROJECT_ROWS + 1) {
    gfxCursor(0, 13, 5);
  }
}

static void drawField(int col, int row, CellState state) {
  if (row < SCR_PROJECT_ROWS) return projectCommonDrawField(col, row, state);

  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);

  if (row == SCR_PROJECT_ROWS) {
    gfxClearRect(13, 12, PROJECT_PITCH_TABLE_TITLE_LENGTH, 1);
    gfxPrint(13, 12, chipnomadState->project.pitchTable.name);
  } else if (row == SCR_PROJECT_ROWS + 1) {
    gfxPrint(0, 13, "Scale");
  }
}

static int onEdit(int col, int row, enum CellEditAction action) {
  if (row < SCR_PROJECT_ROWS) return projectCommonOnEdit(col, row, action);

  if (row == SCR_PROJECT_ROWS) {
    screenSetup(&screenPitchTable, 0);
  } else if (row == SCR_PROJECT_ROWS + 1) {
    screenSetup(&screenScale, 0);
  }
  return 0;
}

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
};
