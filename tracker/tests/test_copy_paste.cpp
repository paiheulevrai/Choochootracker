#include "doctest.h"
#include "copy_paste.h"
#include "common.h"
#include "screens.h"

#include <cstring>

// copy_paste also owns selection-mode helpers; the clipboard tests do not link
// the full screen implementation.
void getSelectionBounds(ScreenData* screen, int* startCol, int* startRow, int* endCol, int* endRow) {
  *startCol = screen->selectStartCol < screen->cursorCol ? screen->selectStartCol : screen->cursorCol;
  *endCol = screen->selectStartCol > screen->cursorCol ? screen->selectStartCol : screen->cursorCol;
  *startRow = screen->selectStartRow < screen->cursorRow ? screen->selectStartRow : screen->cursorRow;
  *endRow = screen->selectStartRow > screen->cursorRow ? screen->selectStartRow : screen->cursorRow;
}

TEST_CASE("FX clipboard moves Phrase FX into Table lanes and truncates") {
  ChipNomadState state = {};
  chipnomadState = &state;
  projectInit(&state.project);
  PhraseRow& source0 = state.project.phrases[0].rows[0];
  PhraseRow& source1 = state.project.phrases[0].rows[1];
  source0.fx[0][0] = fxDMD; source0.fx[0][1] = 8;
  source1.fx[0][0] = fxDFM; source1.fx[0][1] = 255;
  copyPhrase(0, 3, 0, 4, 1, 1);
  CHECK(source0.fx[0][0] == EMPTY_VALUE_8); CHECK(source0.fx[0][1] == 0);
  CHECK(pasteTable(0, 9, 14) == 2);
  CHECK(state.project.tables[0].rows[14].fx[3][0] == fxDMD);
  CHECK(state.project.tables[0].rows[14].fx[3][1] == 8);
  CHECK(state.project.tables[0].rows[15].fx[3][0] == fxDFM);
  CHECK(state.project.tables[0].rows[15].fx[3][1] == 255);
}

TEST_CASE("FX clipboard moves Table FX into Phrase lanes and preserves partial cells") {
  ChipNomadState state = {};
  chipnomadState = &state;
  projectInit(&state.project);
  TableRow& source = state.project.tables[0].rows[0];
  source.fx[1][0] = fxDDC; source.fx[1][1] = 99;
  copyTable(0, 5, 0, 6, 0, 0);
  CHECK(pastePhrase(0, 3, 0) == 1);
  CHECK(state.project.phrases[0].rows[0].fx[0][0] == fxDDC);
  CHECK(state.project.phrases[0].rows[0].fx[0][1] == 99);
  copyTable(0, 6, 0, 6, 0, 1);
  CHECK(source.fx[1][0] == fxDDC); CHECK(source.fx[1][1] == 0);
  state.project.phrases[0].rows[1].fx[0][0] = fxDMD;
  CHECK(pastePhrase(0, 4, 1) == 1);
  CHECK(state.project.phrases[0].rows[1].fx[0][0] == fxDMD);
  CHECK(state.project.phrases[0].rows[1].fx[0][1] == 99);
}

TEST_CASE("Phrase row selection copies note and FX command with its value") {
  ChipNomadState state = {};
  chipnomadState = &state;
  projectInit(&state.project);
  PhraseRow& source = state.project.phrases[0].rows[0];
  source.note = 48;
  source.instrument = 2;
  source.volume = 80;
  source.fx[0].command = fxDMD;
  source.fx[0].value = 37;
  source.fx[2].command = fxDFM;
  source.fx[2].value = 42;

  ScreenData selection = {};
  selection.selectStartRow = selection.selectAnchorRow = selection.cursorRow = 0;
  selection.selectStartCol = selection.selectAnchorCol = selection.cursorCol = 0;
  CHECK(switchPhraseSelectionMode(&selection) == 0);
  CHECK(switchPhraseSelectionMode(&selection) == 0);
  int startCol, startRow, endCol, endRow;
  getSelectionBounds(&selection, &startCol, &startRow, &endCol, &endRow);
  CHECK(startCol == 0);
  CHECK(endCol == 8);
  copyPhrase(0, startCol, startRow, endCol, endRow, 0);
  CHECK(pastePhrase(0, 0, 1) == 1);
  const PhraseRow& pasted = state.project.phrases[0].rows[1];
  CHECK(pasted.note == 48);
  CHECK(pasted.instrument == 2);
  CHECK(pasted.volume == 80);
  CHECK(pasted.fx[0].command == fxDMD);
  CHECK(pasted.fx[0].value == 37);
  CHECK(pasted.fx[2].command == fxDFM);
  CHECK(pasted.fx[2].value == 42);
}

TEST_CASE("Table full-row selection and paste include the last row and FX value") {
  ChipNomadState state = {};
  chipnomadState = &state;
  projectInit(&state.project);
  ScreenData selection = {};
  selection.selectStartRow = selection.selectAnchorRow = selection.cursorRow = 16;
  selection.selectStartCol = selection.selectAnchorCol = selection.cursorCol = 0;
  CHECK(switchTableSelectionMode(&selection) == 0);
  CHECK(selection.selectStartRow == 1);
  CHECK(selection.cursorRow == 16);
  CHECK(switchTableSelectionMode(&selection) == 0);
  CHECK(selection.selectStartRow == 16);
  CHECK(selection.cursorRow == 16);
  CHECK(selection.selectStartCol == 0);
  CHECK(selection.cursorCol == 10);

  TableRow& source = state.project.tables[0].rows[15];
  source.pitchFlag = 1;
  source.pitchOffset = 12;
  source.volume = 64;
  source.fx[0].command = fxDDC;
  source.fx[0].value = 99;
  source.fx[3].command = fxDMD;
  source.fx[3].value = 24;
  int startCol, startRow, endCol, endRow;
  getSelectionBounds(&selection, &startCol, &startRow, &endCol, &endRow);
  CHECK(startCol == 0);
  CHECK(endCol == 10);
  copyTable(0, startCol, startRow - 1, endCol, endRow - 1, 0);
  CHECK(pasteTable(0, 0, 0) == 1);
  const TableRow& pasted = state.project.tables[0].rows[0];
  CHECK(pasted.pitchFlag == 1);
  CHECK(pasted.pitchOffset == 12);
  CHECK(pasted.volume == 64);
  CHECK(pasted.fx[0].command == fxDDC);
  CHECK(pasted.fx[0].value == 99);
  CHECK(pasted.fx[3].command == fxDMD);
  CHECK(pasted.fx[3].value == 24);
}
