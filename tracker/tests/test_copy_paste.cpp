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
