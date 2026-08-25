#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "corelib/corelib_file.h"
#include "file_browser.h"
#include "synth/sample_voice.h"
#include "synth/scwf_voice.h"
#include "audio_manager.h"
#include "utils.h"

#include <string.h>

static int loadSlot;
static int buttonDown;
static constexpr int sourceValueX = 9;
static constexpr int sourceValueWidth = 7;

static const char* filename(const char* path) {
  const char* separator = strrchr(path, PATH_SEPARATOR);
  return separator ? separator + 1 : path;
}

static const char* shortFilename(const char* path, char* output, size_t maxLength) {
  const char* name = filename(path);
  const char* extension = strrchr(name, '.');
  size_t length = extension ? (size_t)(extension - name) : strlen(name);
  if (length > maxLength) {
    name += length - maxLength;
    length = maxLength;
  }
  memcpy(output, name, length);
  output[length] = 0;
  return output;
}

static void cancelled(void) { screenSetup(&screenInstrument, cInstrument); }

static void loaded(const char* path) {
  InstrumentSCWF* scwf = &chipnomadState->project.instruments[cInstrument].chip.scwf;
  char error[64];
  audioManager.pause();
  if (sampleLoadWav16(path, &scwf->oscillator[loadSlot], error, sizeof(error))) {
    screenMessage(MESSAGE_TIME * 3, "%s", error);
  } else if (scwf->oscillator[loadSlot].channels != 1) {
    free(scwf->oscillator[loadSlot].data);
    scwf->oscillator[loadSlot].data = NULL;
    scwf->oscillator[loadSlot].path[0] = 0;
    screenMessage(MESSAGE_TIME * 3, "2xSCWF needs mono WAV");
  } else {
    updatePathFromFile(appSettings.scwfPath, path);
    projectModified = 1;
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
  gfxPrint(0,7,"SOURCE");
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(0,8,"Osc A"); gfxPrint(0,9,"Osc B");
  gfxPrint(0,10,"Detune"); gfxPrint(0,11,"Mix");
  instrumentCommonDrawVoicePostStatic(1);
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  if (instrumentCommonDrawVoicePostCursor(col, row)) return;
  if (!col && row >= 3 && row <= 6) gfxCursor(sourceValueX,row + 5,sourceValueWidth);
  else gfxCursor(col ? 26 : sourceValueX,row + 4,col ? 8 : sourceValueWidth);
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) return instrumentCommonDrawField(col, row, state);
  InstrumentSCWF* scwf = &chipnomadState->project.instruments[cInstrument].chip.scwf;
  if (instrumentCommonDrawVoicePostField(col, row, state, scwf)) return;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  if (!col && row >= 3 && row <= 6) gfxClearRect(sourceValueX,row + 5,sourceValueWidth,1);
  char name[PROJECT_INSTRUMENT_NAME_LENGTH + 1];
  if (row == 3 && !col) gfxPrint(sourceValueX,8,scwf->oscillator[0].path[0] ? shortFilename(scwf->oscillator[0].path, name, 7) : "Load");
  else if (row == 4 && !col) gfxPrint(sourceValueX,9,scwf->oscillator[1].path[0] ? shortFilename(scwf->oscillator[1].path, name, 7) : "Load");
  else if (row == 5 && !col) {
    int cents = scwfDetuneCents(scwf->detune);
    if (cents <= 200) gfxPrintf(sourceValueX,10,"+%03d ct", cents);
    else gfxPrintf(sourceValueX,10,"+%02d st", cents / 100);
  } else if (row == 6 && !col) gfxPrint(sourceValueX,11,byteToHex(scwf->mix));
}

static int edit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentSCWF* scwf = &chipnomadState->project.instruments[cInstrument].chip.scwf;
  if ((row == 9) || (col && row >= 4 && row <= 8)) {
    int handled = instrumentCommonOnEditVoicePost(col, row, action, scwf);
    if (handled) projectModified = 1;
    return handled;
  }
  int handled = 0;
  if (row == 5 && !col) handled = edit8noLast(action, &scwf->detune, 1, 0, SCWF_DETUNE_MAX);
  if (row == 6 && !col) handled = edit8noLast(action, &scwf->mix, 8, 0, 255);
  if (handled) projectModified = 1;
  return handled;
}

static int input(int isKeyDown, int keys, int) {
  int row = screenInstrumentSCWF.cursorRow;
  if ((row != 3 && row != 4) || screenInstrumentSCWF.cursorCol != 0) { buttonDown = 0; return 0; }
  if (isKeyDown && (keys == (keyEdit | keyLeft) || keys == (keyEdit | keyRight))) {
    InstrumentSCWF* scwf = &chipnomadState->project.instruments[cInstrument].chip.scwf;
    InstrumentSample* oscillator = &scwf->oscillator[row - 3];
    int direction = keys == (keyEdit | keyRight) ? 1 : -1;
    char path[PROJECT_SAMPLE_PATH_LENGTH + 1];
    if (fileBrowserGetAdjacentPath(oscillator->path, ".wav", direction, path, sizeof(path))) { loadSlot = row - 3; loaded(path); }
    return 1;
  }
  if (isKeyDown && keys == keyEdit) { buttonDown = 1; return 1; }
  if (!isKeyDown && keys == 0 && buttonDown) {
    buttonDown = 0;
    loadSlot = row - 3;
    fileBrowserSetup("LOAD SCWF OSC", ".wav", appSettings.scwfPath, loaded, cancelled);
    screenSetup(&screenFileBrowser, 0);
    return 1;
  }
  return 0;
}

static int isCellValid(int col, int row) {
  return !(col == 0 && (row == 7 || row == 8));
}

ScreenData screenInstrumentSCWF = {
  .rows = 10, .cursorRow = 0, .cursorCol = 0, .topRow = 0,
  .selectMode = -1, .selectStartRow = 0, .selectStartCol = 0,
  .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = columns, .drawStatic = drawStatic, .drawCursor = drawCursor,
  .drawSelection = NULL, .drawRowHeader = NULL, .drawColHeader = NULL,
  .drawField = drawField, .onEdit = edit, .onInput = input, .onRawInput = NULL,
  .isCellValid = isCellValid, .getLoopRange = NULL,
};
