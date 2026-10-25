#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "utils.h"

// A MIDI-out instrument has no synth voice of its own: triggering a note
// sends a MIDI Note On/Off on the chosen channel to whatever device is
// selected in Settings, instead of rendering audio (see
// chipnomad_lib/midi_io.h and chipnomad_lib.cpp's applyVoiceEvents()).
// Program/Bank/CC numbers are optional (EMPTY_VALUE_8 = don't send that
// message/leave that row FX slot inert); Clear toggles a field off and
// remembers its value, like Song/Chain's optional chain/phrase references.
// The CC1-CC4 rows only pick WHICH controller number each of the MC1-MC4
// row FX sends to - the FX itself carries the value (see
// playback_fx_midi.cpp), same split as Braids' BTM/BCL FX and their fixed
// Timbre/Color destinations.

static uint8_t lastProgram = 0, lastBankHigh = 0, lastBankLow = 0;
static uint8_t lastCCNumber[4] = {0, 0, 0, 0};

static int columns(int row) { return row < 3 ? instrumentCommonColumnCount(row) : 1; }

static void drawStatic(void) {
  instrumentCommonDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textLabel);
  gfxPrint(0, 6, "Channel");
  gfxPrint(0, 7, "Program");
  gfxPrint(0, 8, "Bank high (CC0)");
  gfxPrint(0, 9, "Bank low (CC32)");
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 11, "CC row FX");
  gfxSetFgColor(appSettings.colorScheme.textLabel);
  gfxPrint(0, 12, "MC1 number");
  gfxPrint(0, 13, "MC2 number");
  gfxPrint(0, 14, "MC3 number");
  gfxPrint(0, 15, "MC4 number");
}

static int fieldLine(int row) { return row <= 6 ? row + 3 : row + 5; }

static void drawCursor(int col, int row) {
  if (row < 3) { instrumentCommonDrawCursor(col, row); return; }
  gfxCursor(17, fieldLine(row), 2);
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) { instrumentCommonDrawField(col, row, state); return; }
  InstrumentMidi* m = &chipnomadState->project.instruments[cInstrument].chip.midi;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  int y = fieldLine(row);
  gfxClearRect(17, y, 3, 1);
  if (row == 3) gfxPrintf(17, y, "%02d", m->channel + 1);
  else if (row == 4) gfxPrint(17, y, byteToHexOrEmpty(m->program));
  else if (row == 5) gfxPrint(17, y, byteToHexOrEmpty(m->bankHigh));
  else if (row == 6) gfxPrint(17, y, byteToHexOrEmpty(m->bankLow));
  else gfxPrint(17, y, byteToHexOrEmpty(m->ccNumber[row - 7]));
}

static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentMidi* m = &chipnomadState->project.instruments[cInstrument].chip.midi;
  int ok;
  if (row == 3) {
    ok = edit8noLast(action, &m->channel, 1, 0, 15);
  } else if (row == 4) {
    ok = edit8withLimit(action, &m->program, &lastProgram, 16, 127);
  } else if (row == 5) {
    ok = edit8withLimit(action, &m->bankHigh, &lastBankHigh, 16, 127);
  } else if (row == 6) {
    ok = edit8withLimit(action, &m->bankLow, &lastBankLow, 16, 127);
  } else {
    int slot = row - 7;
    ok = edit8withLimit(action, &m->ccNumber[slot], &lastCCNumber[slot], 16, 127);
  }
  if (ok) { projectModified = 1; screenFullRedraw(&screenInstrumentMidi); }
  return ok;
}

ScreenData screenInstrumentMidi = {
  .rows = 11, .cursorRow = 0, .cursorCol = 0, .topRow = 0,
  .selectMode = -1, .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = columns,
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
