#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "corelib/corelib_file.h"
#include "file_browser.h"
#include "synth/scwf_voice.h"
#include "synth/sr_wavetable_loader.h"
#include "audio_manager.h"
#include "utils.h"

#include <string.h>

static int loadSlot, buttonDown;
static constexpr int sourceValueX = 9;
static constexpr int sourceValueWidth = 7;

static const char* shortFilename(const char* path, char* output) {
  const char* name = strrchr(path, PATH_SEPARATOR);
  name = name ? name + 1 : path;
  const char* extension = strrchr(name, '.');
  size_t length = extension ? (size_t)(extension - name) : strlen(name);
  if (length > 7) {
    name += length - 7;
    length = 7;
  }
  memcpy(output, name, length);
  output[length] = 0;
  return output;
}

static void cancelled(void) { screenSetup(&screenInstrument, cInstrument); }

static void loaded(const char* path) {
  InstrumentBYOWTBL* table = &chipnomadState->project.instruments[cInstrument].chip.byowtbl;
  char error[64];
  audioManager.pause();
  if (srWavetableLoadWav(path, &table->oscillator[loadSlot], &table->frameSize[loadSlot],
                       &table->tableFrames[loadSlot], error, sizeof(error))) {
    screenMessage(MESSAGE_TIME * 3, "%s", error);
  } else {
    updatePathFromFile(appSettings.srWavetablePath, path);
    projectModified = 1;
    screenMessage(MESSAGE_TIME, "%u x %u frames", table->tableFrames[loadSlot], table->frameSize[loadSlot]);
  }
  audioManager.resume();
  screenSetup(&screenInstrument, cInstrument);
}

static int columns(int row) {
  if (row < 3) return instrumentCommonColumnCount(row);
  if (row == 3) return 1;
  return row == 9 ? 5 : 2;
}

static void drawStatic(void) {
  instrumentCommonDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 7, "SOURCE");
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(0, 8, "Table A"); gfxPrint(0, 9, "Table B");
  gfxPrint(0, 10, "Pos A"); gfxPrint(0, 11, "Pos B");
  gfxPrint(0, 12, "Detune"); gfxPrint(0, 13, "Mix");
  instrumentCommonDrawVoicePostStatic(1);
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  if (instrumentCommonDrawVoicePostCursor(col, row)) return;
  if (!col && row >= 3 && row <= 8) gfxCursor(sourceValueX, row + 5, sourceValueWidth);
  else gfxCursor(26, row + 4, 8);
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) return instrumentCommonDrawField(col, row, state);
  InstrumentBYOWTBL* table = &chipnomadState->project.instruments[cInstrument].chip.byowtbl;
  if (instrumentCommonDrawVoicePostField(col, row, state, table)) return;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  if (!col && row >= 3 && row <= 8) gfxClearRect(sourceValueX, row + 5, sourceValueWidth, 1);
  char name[PROJECT_INSTRUMENT_NAME_LENGTH + 1];
  if (row == 3 && !col) gfxPrint(sourceValueX, 8, table->oscillator[0].path[0] ? shortFilename(table->oscillator[0].path, name) : "Load");
  else if (row == 4 && !col) gfxPrint(sourceValueX, 9, table->oscillator[1].path[0] ? shortFilename(table->oscillator[1].path, name) : "Load");
  else if (row == 5 && !col) gfxPrint(sourceValueX, 10, byteToHex(table->frameIndex[0]));
  else if (row == 6 && !col) gfxPrint(sourceValueX, 11, byteToHex(table->frameIndex[1]));
  else if (row == 7 && !col) gfxPrintf(sourceValueX, 12, "+%03d ct", scwfDetuneCents(table->detune));
  else if (row == 8 && !col) gfxPrint(sourceValueX, 13, byteToHex(table->mix));
}

static int edit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentBYOWTBL* table = &chipnomadState->project.instruments[cInstrument].chip.byowtbl;
  int handled = instrumentCommonOnEditVoicePost(col, row, action, table);
  if (!handled && !col) {
    if (row == 5) handled = edit8noLast(action, &table->frameIndex[0], 8, 0, 255);
    else if (row == 6) handled = edit8noLast(action, &table->frameIndex[1], 8, 0, 255);
    else if (row == 7) handled = edit8noLast(action, &table->detune, 1, 0, SCWF_DETUNE_MAX);
    else if (row == 8) handled = edit8noLast(action, &table->mix, 8, 0, 255);
  }
  if (handled) projectModified = 1;
  return handled;
}

static int input(int isKeyDown, int keys, int) {
  int row = screenInstrumentBYOWTBL.cursorRow;
  if ((row != 3 && row != 4) || screenInstrumentBYOWTBL.cursorCol) { buttonDown = 0; return 0; }
  if (isKeyDown && (keys == (keyEdit | keyLeft) || keys == (keyEdit | keyRight))) {
    InstrumentBYOWTBL* table = &chipnomadState->project.instruments[cInstrument].chip.byowtbl;
    int direction = keys == (keyEdit | keyRight) ? 1 : -1;
    char path[PROJECT_SAMPLE_PATH_LENGTH + 1];
    if (fileBrowserGetAdjacentPath(table->oscillator[row - 3].path, ".wav", direction, path, sizeof(path))) { loadSlot = row - 3; loaded(path); }
    return 1;
  }
  if (isKeyDown && keys == keyEdit) { buttonDown = 1; return 1; }
  if (!isKeyDown && !keys && buttonDown) {
    buttonDown = 0; loadSlot = row - 3;
    fileBrowserSetup("LOAD SR WAVETABLE", ".wav", appSettings.srWavetablePath, loaded, cancelled);
    screenSetup(&screenFileBrowser, 0); return 1;
  }
  return 0;
}

ScreenData screenInstrumentBYOWTBL = {
  .rows = 10, .cursorRow = 0, .cursorCol = 0, .topRow = 0, .selectMode = -1,
  .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none, .getColumnCount = columns,
  .drawStatic = drawStatic, .drawCursor = drawCursor, .drawSelection = NULL,
  .drawRowHeader = NULL, .drawColHeader = NULL, .drawField = drawField, .onEdit = edit,
  .onInput = input, .onRawInput = NULL, .isCellValid = NULL, .getLoopRange = NULL,
};
