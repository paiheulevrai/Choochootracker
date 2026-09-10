#include "screens.h"
#include "common.h"
#include "corelib_gfx.h"
#include "corelib_file.h"
#include "utils.h"
#include "chipnomad_lib.h"
#include "project_utils.h"

static void drawRowHeader(int row, CellState state) {}
static void drawColHeader(int col, CellState state) {}

static ScreenData screenManageData = {
  .rows = 6,
  .cursorRow = 0,
  .cursorCol = 0,
  .topRow = 0,
  .selectMode = -1,
  .selectStartRow = 0,
  .selectStartCol = 0,
  .selectAnchorRow = 0,
  .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = manageColumnCount,
  .drawStatic = manageDrawStatic,
  .drawCursor = manageDrawCursor,
  .drawSelection = NULL,
  .drawRowHeader = drawRowHeader,
  .drawColHeader = drawColHeader,
  .drawField = manageDrawField,
  .onEdit = manageOnEdit,
  .onInput = NULL,
  .onRawInput = NULL,
  .isCellValid = NULL,
  .getLoopRange = NULL,
};

static void init(void) {
  screenManageData.cursorRow = 1;
  screenManageData.cursorCol = 0;
}

static void setup(int input) {
}

static void fullRedraw(void) {
  screenFullRedraw(&screenManageData);
}

static void draw(void) {
}

int manageColumnCount(int row) {
  return 1;
}

void manageDrawStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 0, "MANAGE PROJECT");

  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(0, 2, "Clean unused/duplicate:");
  gfxPrint(0, 7, "Android files:");
  gfxPrint(0, 11, "Privacy: no data collected");
}

void manageDrawCursor(int col, int row) {
  if (row == 1) {
    gfxCursor(2, 3, 18); // "Phrases and Chains"
  } else if (row == 2) {
    gfxCursor(2, 4, 22); // "Instruments and Tables"
  } else if (row == 3) {
    gfxCursor(2, 8, 14);
  } else if (row == 4) {
    gfxCursor(2, 9, 10);
  }
}

void manageDrawField(int col, int row, CellState state) {
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);

  if (row == 1) {
    gfxPrint(2, 3, "Phrases and Chains");
  } else if (row == 2) {
    gfxPrint(2, 4, "Instruments and Tables");
  } else if (row == 3) {
    gfxPrint(2, 8, "Import project");
  } else if (row == 4) {
    gfxPrint(2, 9, "Import WAV");
  } else if (row == 5) {
    gfxPrint(0, 11, "Privacy: no data collected");
  }
}

int manageOnEdit(int col, int row, CellEditAction action) {
  if (action != CellEditAction::tap && action != CellEditAction::doubleTap) return 0;

  if (row == 1) {
    // Cleanup phrases and chains
    int phrasesFreed, chainsFreed;
    cleanupPhrasesAndChains(&chipnomadState->project, &phrasesFreed, &chainsFreed);
    if (phrasesFreed > 0 || chainsFreed > 0) {
      screenMessage(MESSAGE_TIME, "Freed %d phrases, %d chains", phrasesFreed, chainsFreed);
      projectModified = 1;
    } else {
      screenMessage(MESSAGE_TIME, "Nothing to clean");
    }
  } else if (row == 2) {
    // Cleanup instruments and tables
    int instrumentsFreed, tablesFreed;
    cleanupInstrumentsAndTables(&chipnomadState->project, &instrumentsFreed, &tablesFreed);
    if (instrumentsFreed > 0 || tablesFreed > 0) {
      screenMessage(MESSAGE_TIME, "Freed %d instruments, %d tables", instrumentsFreed, tablesFreed);
      projectModified = 1;
    } else {
      screenMessage(MESSAGE_TIME, "Nothing to clean");
    }
  } else if (row == 3) {
    fileImportDocument("application/octet-stream", "projects");
    screenMessage(MESSAGE_TIME, "Choose a project to import");
  } else if (row == 4) {
    fileImportDocument("audio/wav", "samples");
    screenMessage(MESSAGE_TIME, "Choose a WAV to import");
  }

  return 1;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (keys == keyOpt) {
    screenSetup(&screenProject, 0);
    return 1;
  }

  return screenInput(&screenManageData, isKeyDown, keys, tapCount);
}

const AppScreen screenManage = {
  .init = init,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = NULL,
};
