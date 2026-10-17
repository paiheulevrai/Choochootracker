#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "corelib_file.h"
#include "user_preset_browser.h"
#include "selection_popup.h"
#include "project_utils.h"
#include "monk_patch.h"
#include "utils.h"
#include <algorithm>
#include <cstring>

namespace {
Instrument& current() { return chipnomadState->project.instruments[cInstrument]; }
int held = 0;
const int homeY[] = {2, 3, 6, 7, 9, 10, 11, 12, 14, 15};
const int homeParameters[][2] = {{monkVowel, -1}, {monkVoice, monkBreath},
  {monkGlide, monkVibrato}, {monkVibRate, -1}, {monkUnison, monkDetune}, {monkSpread, -1}};
const int commonX[] = {3, 12, 22, 31};
void returnHome() { screenSetup(&screenInstrument, cInstrument); }
void chooseBank(int value) {
  if (value >= monkBankUser && value <= monkBankAll) { current().chip.monk.factory = value; projectModified = 1; }
  returnHome();
}
void banks() {
  static const SelectionItem choices[] = {{"ALL", monkBankAll, nullptr, 0},
    {"FACTORY", monkBankFactory, nullptr, 0}, {"USER", monkBankUser, nullptr, 0}};
  selectionPopupSetup("MONK BANK", choices, 3, current().chip.monk.factory, chooseBank, returnHome);
  screenSetup(&screenSelectionPopup, 0);
}
int homeColumns(int row) {
  return row == 0 ? 3 : row == 1 ? 4 : row == 3 ? 2 :
    row >= 4 && homeParameters[row - 4][1] >= 0 ? 2 : 1;
}
int homeX(int col, int row) { return row == 1 ? commonX[col] : row == 3 && col ? 29 : row < 4 ? 8 : col ? 31 : 11; }
int homeWidth(int col, int row) { return row == 1 ? (col ? 2 : 3) : row == 2 ? 7 : row == 3 ? (col ? 4 : 19) : 2; }
void homeStatic() {
  gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrintf(0, 0, "INSTRUMENT %02X", cInstrument);
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(0, 2, "Type"); gfxPrint(0, 3, "TR"); gfxPrint(8, 3, "TIC");
  gfxPrint(18, 3, "VOL"); gfxPrint(27, 3, "PAN");
  gfxPrint(0, homeY[2], "Bank"); gfxPrint(0, homeY[3], "Preset");
  for (int row = 4; row < 10; ++row) for (int col = 0; col < homeColumns(row); ++col)
    gfxPrint(col ? 20 : 0, homeY[row], monkControls[homeParameters[row-4][col]].name);
  gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0, 8, "VOICE"); gfxPrint(0, 13, "UNISON");
}
void homeCursor(int col, int row) {
  if (row == 0) instrumentCommonDrawCursor(col, 0);
  else gfxCursor(homeX(col, row), homeY[row], homeWidth(col, row));
}
void homeField(int col, int row, CellState state) {
  if (row == 0) { instrumentCommonDrawField(col, 0, state); return; }
  int x = homeX(col, row), y = homeY[row];
  gfxClearRect(x, y, homeWidth(col, row), 1);
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  const auto& m = current().chip.monk;
  if (row == 1) {
    if (!col) gfxPrint(x, y, current().transposeEnabled ? "On " : "Off");
    else gfxPrint(x, y, byteToHex(col == 1 ? current().tableSpeed : col == 2 ? current().volume : current().pan));
  } else if (row == 2) gfxPrint(x, y, m.factory == monkBankAll ? "ALL" : m.factory == monkBankFactory ? "FACTORY" : "USER");
  else if (row == 3) {
    if (col) gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textInfo);
    gfxPrintf(x, y, col ? "EDIT" : "%.19s", current().name);
  }
  else {
    gfxPrint(x, y, byteToHex(monkValue(m, homeParameters[row-4][col])));
    if (row == 4) {
      const char* vowels[] = {"A", "E", "I", "O", "U"};
      int nearest = (m.vowel * 4 + 127) / 255;
      for (int i = 0; i < 5; ++i) {
        gfxSetFgColor(i == nearest ? appSettings.colorScheme.textValue : appSettings.colorScheme.textInfo);
        gfxPrint(16 + i * 4, y, vowels[i]);
      }
    }
  }
}
int editParameter(int parameter, CellEditAction action) {
  const auto& control = monkControls[parameter];
  int changed = edit8noLast(action, monkParameter(&current().chip.monk, parameter),
    parameter == monkUnison ? 1 : 16, control.minimum, control.maximum);
  if (changed) projectModified = 1;
  return changed;
}
int homeEdit(int col, int row, CellEditAction action) {
  if (row == 0) return instrumentCommonOnEdit(col, 0, action);
  if (row == 1) return instrumentCommonOnEdit(col, 2, action);
  if (row < 4) {
    if (convertMultiAction(action) != CellEditAction::tap) return 0;
    if (row == 2) banks();
    else if (col) screenSetup(&screenMonkEdit, cInstrument);
    else openUserPresetBrowser();
    return 1;
  }
  return editParameter(homeParameters[row-4][col], action);
}
int homeInput(int down, int keys, int) {
  int row = screenInstrumentMonk.cursorRow;
  if ((row != 2 && row != 3) || screenInstrumentMonk.cursorCol) { held = 0; return 0; }
  auto action = popupEditInput(down, keys, &held);
  if (action == PopupEditInput::cycle) {
    if (row == 2) {
      // Display order is ALL, FACTORY, USER.
      int delta = keys == (keyEdit | keyRight) ? -1 : 1;
      chooseBank((current().chip.monk.factory + delta + 3) % 3);
    }
    else cycleUserPreset(keys == (keyEdit | keyRight) ? 1 : -1);
    return 1;
  }
  if (action == PopupEditInput::hold) return 1;
  if (action == PopupEditInput::open) return homeEdit(0, row, CellEditAction::tap);
  return 0;
}

namespace editor {
ScreenData data;
bool characterPopup = false;
const int rowY[] = {2, 5, 6, 7, 10, 11, 14, 15, 17};
const int parameters[][2] = {{monkVowel, monkVoice}, {monkBreath, monkGlide},
  {monkVibrato, monkVibRate}, {monkAttack, monkDecay}, {monkSustain, monkRelease},
  {monkUnison, monkDetune}, {monkSpread, monkLevel}, {monkDelayMix, monkDelayRate}};
int columns(int row) { return row ? 2 : PROJECT_INSTRUMENT_NAME_LENGTH; }
void noHeader(int, CellState) {}
void drawStatic() {
  gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrintf(0, 0, "MONK EDIT %02X", cInstrument);
  gfxPrint(0, 4, "VOICE"); gfxPrint(0, 9, "ENVELOPE"); gfxPrint(0, 13, "UNISON / LEVEL");
  gfxSetFgColor(appSettings.colorScheme.textInfo); gfxPrint(0, 2, "Name");
  for (int row = 1; row < 9; ++row) for (int col = 0; col < 2; ++col)
    gfxPrint(col ? 20 : 0, rowY[row], monkControls[parameters[row-1][col]].name);
}
void cursor(int col, int row) { gfxCursor(row ? (col ? 31 : 11) : 8 + col, rowY[row], row ? 2 : 1); }
void field(int col, int row, CellState state) {
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  if (!row) {
    char text[2] = {col < int(std::strlen(current().name)) ? current().name[col] : ' ', 0};
    gfxPrint(8 + col, rowY[0], text);
  } else gfxPrint(col ? 31 : 11, rowY[row], byteToHex(monkValue(current().chip.monk, parameters[row-1][col])));
}
int edit(int col, int row, CellEditAction action) {
  if (row) return editParameter(parameters[row-1][col], action);
  int result = editCharacter(action, current().name, col, PROJECT_INSTRUMENT_NAME_LENGTH);
  characterPopup = result == 1;
  if (result > 1) projectModified = 1;
  return result;
}
void setup(int) { data.cursorCol = data.cursorRow = data.topRow = 0; characterPopup = false; }
void redraw() { screenFullRedraw(&data); }
void draw() {}
int input(int down, int keys, int taps) {
  if (characterPopup) {
    if (charEditInput(keys, taps, current().name, data.cursorCol, PROJECT_INSTRUMENT_NAME_LENGTH)) {
      characterPopup = false; projectModified = 1;
      data.cursorCol = std::min(data.cursorCol + 1, PROJECT_INSTRUMENT_NAME_LENGTH - 1); redraw();
    }
    return 1;
  }
  if (!keys) chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
  if (keys == keyOpt || keys == (keyShift | keyLeft)) {
    chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack); returnHome(); return 1;
  }
  if (keys == (keyEdit | keyPlay)) {
    if (down && !chipnomadGetPlaybackStatus(chipnomadState)->isPlaying)
      chipnomadQueuePlaybackPreviewNote(chipnomadState, *pSongTrack,
        instrumentFirstNote(&chipnomadState->project, cInstrument), cInstrument);
    return 1;
  }
  return screenInput(&data, down, keys, taps);
}
ScreenPlaybackLevel level() { return ScreenPlaybackLevel::phrase; }
struct Initializer {
  Initializer() {
    data = {}; data.rows = 9; data.selectMode = -1; data.getColumnCount = columns;
    data.drawStatic = drawStatic; data.drawField = field; data.drawCursor = cursor;
    data.drawRowHeader = noHeader; data.drawColHeader = noHeader; data.onEdit = edit;
  }
} initializer;
}
}
ScreenData screenInstrumentMonk = {
  .rows = 10, .cursorRow = 0, .cursorCol = 0, .topRow = 0, .selectMode = -1,
  .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none, .getColumnCount = homeColumns, .drawStatic = homeStatic,
  .drawCursor = homeCursor, .drawSelection = nullptr, .drawRowHeader = nullptr, .drawColHeader = nullptr,
  .drawField = homeField, .onEdit = homeEdit, .onInput = homeInput, .onRawInput = nullptr,
  .isCellValid = nullptr, .getLoopRange = nullptr,
};
const AppScreen screenMonkEdit = {nullptr, editor::setup, editor::redraw, editor::draw, editor::input, editor::level};

bool instrumentMonkTouchCell(int col, int row, int* targetCol, int* targetRow) {
  if (currentScreen == &screenMonkEdit) {
    for (int field = 0; field < 9; ++field) if (row == editor::rowY[field]) {
      if (field == 0) {
        if (col < 8 || col >= 8 + PROJECT_INSTRUMENT_NAME_LENGTH) return false;
        *targetCol = col - 8;
      } else {
        if (col < 0 || col > 32 || (col > 12 && col < 20)) return false;
        *targetCol = col >= 20;
      }
      *targetRow = field; return true;
    }
    return false;
  }
  for (int field = 1; field < 10; ++field) if (row == homeY[field]) {
    int column = 0;
    if (field == 1) {
      if (col < 0 || col > 32) return false;
      column = col >= 27 ? 3 : col >= 18 ? 2 : col >= 8 ? 1 : 0;
    } else if (field == 2) {
      if (col < 8 || col > 14) return false;
    } else if (field == 3) {
      if (col < 8 || col > 32 || col == 27 || col == 28) return false;
      column = col >= 29;
    } else if (field == 4) {
      if (col < 0 || col > 32) return false;
    } else {
      if (col < 0 || col > 32 || (col > 12 && col < 20)) return false;
      column = col >= 20;
      if (column >= homeColumns(field)) return false;
    }
    *targetCol = column; *targetRow = field; return true;
  }
  return false;
}
