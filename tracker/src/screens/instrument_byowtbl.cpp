#include "screen_instrument.h"
#include "field_style.h"
#include "corelib_gfx.h"
#include "corelib/corelib_file.h"
#include "file_browser.h"
#include "synth/scwf_voice.h"
#include "synth/sr_wavetable_loader.h"
#include "audio_manager.h"
#include "utils.h"
#include "waveform_display.h"

#include <string.h>

static int loadSlot, buttonDown;
static constexpr int sourceValueX = kColumnValueX[0];
static constexpr int sourceValueWidth = kFieldNumberWidth;
static Bitmap* previewBitmap;

static void drawPreview(const InstrumentBYOWTBL* table) {
  if (!previewBitmap) previewBitmap = gfxBitmapCreate(32, 3);
  renderSCWFPreview(previewBitmap, table, table->frameSize, table->frameIndex);
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxDrawBitmap(previewBitmap, 0, 16);
}

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

static void cancelled(void) { audioManager.stopSamplePreview(); screenSetup(&screenInstrument, cInstrument); }

static void loaded(const char* path) {
  audioManager.stopSamplePreview();
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
  chipnomadQueueProjectRefresh(chipnomadState);
  audioManager.resume();
  screenSetup(&screenInstrument, cInstrument);
}

static void preview(const char* path) {
  const InstrumentBYOWTBL* table = &chipnomadState->project.instruments[cInstrument].chip.byowtbl;
  if (audioManager.previewBYOWTBL(path, table, loadSlot)) screenMessage(MESSAGE_TIME, "Cannot preview WAV");
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
  gfxSetFgColor(appSettings.colorScheme.textLabel);
  gfxPrint(0, 8, "Table A"); gfxPrint(0, 9, "Table B");
  gfxPrint(0, 10, "Pos A"); gfxPrint(0, 11, "Pos B");
  gfxPrint(0, 12, "Detune"); gfxPrint(0, 13, "Mix");
  instrumentCommonDrawVoicePostStatic(1);
  drawPreview(&chipnomadState->project.instruments[cInstrument].chip.byowtbl);
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  if (instrumentCommonDrawVoicePostCursor(col, row)) return;
  if (!col && row >= 3 && row <= 8) gfxCursor(sourceValueX, row + 5, sourceValueWidth);
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) return instrumentCommonDrawField(col, row, state);
  InstrumentBYOWTBL* table = &chipnomadState->project.instruments[cInstrument].chip.byowtbl;
  if (instrumentCommonDrawVoicePostField(col, row, state, table)) return;
  if (col || row < 3 || row > 8) return;
  // The selected field's label lights up with its value.
  static const char* const labels[] = {"Table A", "Table B", "Pos A", "Pos B", "Detune", "Mix"};
  fieldDrawLabel(0, row + 5, labels[row - 3], state);
  gfxSetFgColor(fieldValueColor(state));
  gfxClearRect(sourceValueX, row + 5, sourceValueWidth, 1);
  char name[PROJECT_INSTRUMENT_NAME_LENGTH + 1];
  if (row == 3) gfxPrint(sourceValueX, 8, table->oscillator[0].path[0] ? shortFilename(table->oscillator[0].path, name) : "Load");
  else if (row == 4) gfxPrint(sourceValueX, 9, table->oscillator[1].path[0] ? shortFilename(table->oscillator[1].path, name) : "Load");
  else if (row == 5) fieldDrawNumber(sourceValueX, 10, table->frameIndex[0], state, -1.0f, false, instrumentLivePosition(liveSource1));
  else if (row == 6) fieldDrawNumber(sourceValueX, 11, table->frameIndex[1], state, -1.0f, false, instrumentLivePosition(liveSource2));
  else if (row == 7) fieldDrawNumber(sourceValueX, 12, table->detune, state, table->detune / float(SCWF_DETUNE_MAX), false,
                                     instrumentLivePosition(liveSource3));
  else fieldDrawNumber(sourceValueX, 13, table->mix, state, -1.0f, false, instrumentLivePosition(liveSource4));
}

bool instrumentBYOWTBLLiveCell(int control, int* col, int* row) {
  if (control < liveSource1 || control > liveSource4) return false;  // Pos A, Pos B, Detune, Mix
  *col = 0;
  *row = 5 + control - liveSource1;
  return true;
}

static int edit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentBYOWTBL* table = &chipnomadState->project.instruments[cInstrument].chip.byowtbl;
  int handled = instrumentCommonOnEditVoicePost(col, row, action, table);
  if (!handled && !col) {
    if (row == 5) handled = edit8noLast(action, &table->frameIndex[0], 8, 0, 255);
    else if (row == 6) handled = edit8noLast(action, &table->frameIndex[1], 8, 0, 255);
    else if (row == 7) handled = edit8noLast(action, &table->detune, 16, 0, SCWF_DETUNE_MAX);
    else if (row == 8) handled = edit8noLast(action, &table->mix, 8, 0, 255);
  }
  if (handled) { projectModified = 1; screenFullRedraw(&screenInstrumentBYOWTBL); }
  return handled;
}

static int input(int isKeyDown, int keys, int) {
  int row = screenInstrumentBYOWTBL.cursorRow;
  if ((row != 3 && row != 4) || screenInstrumentBYOWTBL.cursorCol) { buttonDown = 0; return 0; }
  PopupEditInput input = popupEditInput(isKeyDown, keys, &buttonDown);
  if (input == PopupEditInput::cycle) {
    InstrumentBYOWTBL* table = &chipnomadState->project.instruments[cInstrument].chip.byowtbl;
    int direction = keys == (keyEdit | keyRight) ? 1 : -1;
    char path[PROJECT_SAMPLE_PATH_LENGTH + 1];
    if (fileBrowserGetAdjacentPath(table->oscillator[row - 3].path, ".wav", direction, path, sizeof(path))) { loadSlot = row - 3; loaded(path); }
    return 1;
  }
  if (input == PopupEditInput::hold) return 1;
  if (input == PopupEditInput::open) {
    loadSlot = row - 3;
    fileBrowserSetupWithPreview("LOAD SR WAVETABLE", ".wav", appSettings.srWavetablePath, loaded, cancelled, preview);
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
