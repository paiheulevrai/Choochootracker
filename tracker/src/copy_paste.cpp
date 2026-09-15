#include "copy_paste.h"
#include "screens.h"
#include "project_utils.h"
#include <string.h>

uint16_t cpBufSong[PROJECT_MAX_LENGTH][PROJECT_MAX_TRACKS];
Chain cpBufChain;
Phrase cpBufPhrase;
Table cpBufTable;
Groove cpBufGroove;
Instrument cpBufInstrument;
Table cpBufInstrumentTable;
uint8_t cpBufWavetable[32];  // 32 4-bit values

static int cpBufGrooveLength = 0;
static int cpBufSongRows = 0;
static int cpBufSongCols = 0;
static int cpBufChainRows = 0;
static int cpBufChainStartCol;
static int cpBufChainEndCol;
static int cpBufPhraseRows = 0;
static int cpBufPhraseStartCol;
static int cpBufPhraseEndCol;
static int cpBufTableRows = 0;
static int cpBufTableStartCol;
static int cpBufTableEndCol;
static uint8_t cpBufFX[16][8];
static int cpBufFXRows = 0;
static int cpBufFXCols = 0;
static int cpBufFXStartCol;
static int cpBufInstrumentValid = 0;
static int cpBufWavetableValid = 0;

void resetCopyBuffers(void) {
  cpBufGrooveLength = 0;
  cpBufSongRows = 0;
  cpBufSongCols = 0;
  cpBufChainRows = 0;
  cpBufPhraseRows = 0;
  cpBufTableRows = 0;
  cpBufFXRows = 0;
  cpBufInstrumentValid = 0;
  cpBufWavetableValid = 0;
}

static int isFXSelection(int startCol, int endCol, int lastFXCol) {
  return startCol >= 3 && endCol <= lastFXCol;
}

static void copyFXCells(const uint8_t fx[][2], int row, int startCol, int endCol) {
  for (int col = startCol; col <= endCol; ++col)
    cpBufFX[row][col - startCol] = fx[(col - 3) / 2][(col - 3) % 2];
}

static void clearFXCells(uint8_t fx[][2], int startCol, int endCol) {
  for (int col = startCol; col <= endCol; ++col)
    fx[(col - 3) / 2][(col - 3) % 2] = (col - 3) % 2 ? 0 : EMPTY_VALUE_8;
}

static int canPasteFX(int lastFXCol, int startCol) {
  return cpBufFXRows && startCol >= 3 && startCol <= lastFXCol &&
    !((startCol - cpBufFXStartCol) & 1);
}

static void pasteFXCells(uint8_t fx[][2], int row, int lastFXCol, int startCol) {
  for (int col = 0; col < cpBufFXCols && startCol + col <= lastFXCol; ++col)
    fx[(startCol + col - 3) / 2][(startCol + col - 3) % 2] = cpBufFX[row][col];
}

void copyGroove(int grooveIdx, int startRow, int endRow, int isCut) {
  cpBufGrooveLength = endRow - startRow + 1;
  for (int row = startRow; row <= endRow; row++) {
    cpBufGroove.speed[row - startRow] = chipnomadState->project.grooves[grooveIdx].speed[row];
    if (isCut) {
      chipnomadState->project.grooves[grooveIdx].speed[row] = 0;
    }
  }
}

int pasteGroove(int grooveIdx, int startRow) {
  int rowsPasted = 0;
  for (int row = 0; row < cpBufGrooveLength && startRow + row < 16; row++) {
    rowsPasted++;
    chipnomadState->project.grooves[grooveIdx].speed[startRow + row] = cpBufGroove.speed[row];
  }
  return rowsPasted;
}

void copySong(int startCol, int startRow, int endCol, int endRow, int isCut) {
  cpBufSongRows = endRow - startRow + 1;
  cpBufSongCols = endCol - startCol + 1;

  for (int row = 0; row < cpBufSongRows; row++) {
    for (int col = 0; col < cpBufSongCols; col++) {
      cpBufSong[row][col] = chipnomadState->project.song[startRow + row][startCol + col];
      if (isCut) {
        chipnomadState->project.song[startRow + row][startCol + col] = EMPTY_VALUE_16;
      }
    }
  }
}

int pasteSong(int startCol, int startRow) {
  // Push down existing data in the paste area
  for (int col = startCol; col < startCol + cpBufSongCols && col < PROJECT_MAX_TRACKS; col++) {
    for (int row = PROJECT_MAX_LENGTH - 1; row >= startRow + cpBufSongRows; row--) {
      chipnomadState->project.song[row][col] = chipnomadState->project.song[row - cpBufSongRows][col];
    }
  }

  // Paste the buffer data
  int rowsPasted = 0;
  for (int row = 0; row < cpBufSongRows && startRow + row < PROJECT_MAX_LENGTH; row++) {
    rowsPasted++;
    for (int col = 0; col < cpBufSongCols && startCol + col < PROJECT_MAX_TRACKS; col++) {
      chipnomadState->project.song[startRow + row][startCol + col] = cpBufSong[row][col];
    }
  }
  return rowsPasted;
}

void shiftSongColumnUp(int col, int startRow) {
  for (int row = startRow; row < PROJECT_MAX_LENGTH - 1; row++) {
    chipnomadState->project.song[row][col] = chipnomadState->project.song[row + 1][col];
  }
  chipnomadState->project.song[PROJECT_MAX_LENGTH - 1][col] = EMPTY_VALUE_16;
}

void copyChain(int chainIdx, int startCol, int startRow, int endCol, int endRow, int isCut) {
  cpBufChainRows = endRow - startRow + 1;
  cpBufChainStartCol = startCol;
  cpBufChainEndCol = endCol;

  for (int row = 0; row < cpBufChainRows; row++) {
    cpBufChain.rows[row] = chipnomadState->project.chains[chainIdx].rows[startRow + row];

    if (isCut) {
      if (startCol == 0) chipnomadState->project.chains[chainIdx].rows[startRow + row].phrase = EMPTY_VALUE_16;
      if (endCol == 1) chipnomadState->project.chains[chainIdx].rows[startRow + row].transpose = 0;
    }
  }
}

int pasteChain(int chainIdx, int startCol, int startRow) {
  // Only paste if column types match
  if (startCol != cpBufChainStartCol) return 0;

  int rowsPasted = 0;
  for (int row = 0; row < cpBufChainRows && startRow + row < 16; row++) {
    rowsPasted++;
    if (cpBufChainStartCol == 0) {
      chipnomadState->project.chains[chainIdx].rows[startRow + row].phrase = cpBufChain.rows[row].phrase;
      // If we copied both columns (startCol=0, endCol=1), also paste transpose
      if (cpBufChainEndCol == 1) {
        chipnomadState->project.chains[chainIdx].rows[startRow + row].transpose = cpBufChain.rows[row].transpose;
      }
    } else {
      chipnomadState->project.chains[chainIdx].rows[startRow + row].transpose = cpBufChain.rows[row].transpose;
    }
  }
  return rowsPasted;
}

void copyPhrase(int phraseIdx, int startCol, int startRow, int endCol, int endRow, int isCut) {
  cpBufPhraseRows = endRow - startRow + 1;
  cpBufPhraseStartCol = startCol;
  cpBufPhraseEndCol = endCol;
  int fxOnly = isFXSelection(startCol, endCol, 8);
  cpBufFXRows = fxOnly ? cpBufPhraseRows : 0;
  if (fxOnly) { cpBufFXCols = endCol - startCol + 1; cpBufFXStartCol = startCol; }

  for (int row = 0; row < cpBufPhraseRows; row++) {
    cpBufPhrase.rows[row] = chipnomadState->project.phrases[phraseIdx].rows[startRow + row];
    if (fxOnly) copyFXCells(cpBufPhrase.rows[row].fx, row, startCol, endCol);

    if (isCut) {
      if (startCol <= 0 && endCol >= 0) chipnomadState->project.phrases[phraseIdx].rows[startRow + row].note = EMPTY_VALUE_8;
      if (startCol <= 1 && endCol >= 1) chipnomadState->project.phrases[phraseIdx].rows[startRow + row].instrument = EMPTY_VALUE_8;
      if (startCol <= 2 && endCol >= 2) chipnomadState->project.phrases[phraseIdx].rows[startRow + row].volume = EMPTY_VALUE_8;
      int fxStart = startCol > 3 ? startCol : 3, fxEnd = endCol < 8 ? endCol : 8;
      if (fxStart <= fxEnd) clearFXCells(chipnomadState->project.phrases[phraseIdx].rows[startRow + row].fx, fxStart, fxEnd);
    }
  }
}

int pastePhrase(int phraseIdx, int startCol, int startRow) {
  if (canPasteFX(8, startCol)) {
    int rowsPasted = 0;
    for (int row = 0; row < cpBufFXRows && startRow + row < 16; ++row) {
      pasteFXCells(chipnomadState->project.phrases[phraseIdx].rows[startRow + row].fx, row, 8, startCol);
      ++rowsPasted;
    }
    return rowsPasted;
  }
  // Check if we can paste - same column or FX to FX
  int canPaste = (startCol == cpBufPhraseStartCol) ||
  (startCol >= 3 && startCol <= 8 && cpBufPhraseStartCol >= 3 && cpBufPhraseStartCol <= 8);

  if (!canPaste) return 0;

  int rowsPasted = 0;
  for (int row = 0; row < cpBufPhraseRows && startRow + row < 16; row++) {
    rowsPasted++;
    // Handle FX-only paste (can paste to different FX columns)
    if (cpBufPhraseStartCol >= 3 && cpBufPhraseStartCol <= 8) {
      int colOffset = startCol - cpBufPhraseStartCol;
      for (int col = cpBufPhraseStartCol; col <= cpBufPhraseEndCol; col++) {
        int dstCol = col + colOffset;
        if (dstCol >= 3 && dstCol <= 8) {
          // Map UI columns to FX array indices
          int srcFx = (col - 3) / 2;
          int dstFx = (dstCol - 3) / 2;
          int srcPart = (col - 3) % 2;  // 0=name, 1=value
          int dstPart = (dstCol - 3) % 2;
          if (srcPart == dstPart) {  // Only paste name to name, value to value
            if (srcPart == 0) {
              chipnomadState->project.phrases[phraseIdx].rows[startRow + row].fx[dstFx][0] = cpBufPhrase.rows[row].fx[srcFx][0];
            } else {
              chipnomadState->project.phrases[phraseIdx].rows[startRow + row].fx[dstFx][1] = cpBufPhrase.rows[row].fx[srcFx][1];
            }
          }
        }
      }
    } else {
      // Handle regular multi-column paste
      for (int col = cpBufPhraseStartCol; col <= cpBufPhraseEndCol; col++) {
        if (col == 0) {
          chipnomadState->project.phrases[phraseIdx].rows[startRow + row].note = cpBufPhrase.rows[row].note;
        } else if (col == 1) {
          chipnomadState->project.phrases[phraseIdx].rows[startRow + row].instrument = cpBufPhrase.rows[row].instrument;
        } else if (col == 2) {
          chipnomadState->project.phrases[phraseIdx].rows[startRow + row].volume = cpBufPhrase.rows[row].volume;
        } else if (col >= 3 && col <= 8) {
          int fxIdx = (col - 3) / 2;
          int part = (col - 3) % 2;
          if (part == 0) {
            chipnomadState->project.phrases[phraseIdx].rows[startRow + row].fx[fxIdx][0] = cpBufPhrase.rows[row].fx[fxIdx][0];
          } else {
            chipnomadState->project.phrases[phraseIdx].rows[startRow + row].fx[fxIdx][1] = cpBufPhrase.rows[row].fx[fxIdx][1];
          }
        }
      }
    }
  }
  return rowsPasted;
}

void copyTable(int tableIdx, int startCol, int startRow, int endCol, int endRow, int isCut) {
  cpBufTableRows = endRow - startRow + 1;
  cpBufTableStartCol = startCol;
  cpBufTableEndCol = endCol;
  int fxOnly = isFXSelection(startCol, endCol, 10);
  cpBufFXRows = fxOnly ? cpBufTableRows : 0;
  if (fxOnly) { cpBufFXCols = endCol - startCol + 1; cpBufFXStartCol = startCol; }

  for (int row = 0; row < cpBufTableRows; row++) {
    cpBufTable.rows[row] = chipnomadState->project.tables[tableIdx].rows[startRow + row];
    if (fxOnly) copyFXCells(cpBufTable.rows[row].fx, row, startCol, endCol);

    if (isCut) {
      if (startCol <= 0 && endCol >= 0) chipnomadState->project.tables[tableIdx].rows[startRow + row].pitchFlag = 0;
      if (startCol <= 1 && endCol >= 1) chipnomadState->project.tables[tableIdx].rows[startRow + row].pitchOffset = 0;
      if (startCol <= 2 && endCol >= 2) chipnomadState->project.tables[tableIdx].rows[startRow + row].volume = EMPTY_VALUE_8;
      int fxStart = startCol > 3 ? startCol : 3, fxEnd = endCol < 10 ? endCol : 10;
      if (fxStart <= fxEnd) clearFXCells(chipnomadState->project.tables[tableIdx].rows[startRow + row].fx, fxStart, fxEnd);
    }
  }
}

int pasteTable(int tableIdx, int startCol, int startRow) {
  if (canPasteFX(10, startCol)) {
    int rowsPasted = 0;
    for (int row = 0; row < cpBufFXRows && startRow + row < 16; ++row) {
      pasteFXCells(chipnomadState->project.tables[tableIdx].rows[startRow + row].fx, row, 10, startCol);
      ++rowsPasted;
    }
    return rowsPasted;
  }
  // Check if we can paste - same column or FX to FX
  int canPaste = (startCol == cpBufTableStartCol) ||
  (startCol >= 3 && startCol <= 10 && cpBufTableStartCol >= 3 && cpBufTableStartCol <= 10);

  if (!canPaste) return 0;

  int rowsPasted = 0;
  for (int row = 0; row < cpBufTableRows && startRow + row < 16; row++) {
    rowsPasted++;
    // Handle FX-only paste (can paste to different FX columns)
    if (cpBufTableStartCol >= 3 && cpBufTableStartCol <= 10) {
      int colOffset = startCol - cpBufTableStartCol;
      for (int col = cpBufTableStartCol; col <= cpBufTableEndCol; col++) {
        int dstCol = col + colOffset;
        if (dstCol >= 3 && dstCol <= 10) {
          // Map UI columns to FX array indices
          int srcFx = (col - 3) / 2;
          int dstFx = (dstCol - 3) / 2;
          int srcPart = (col - 3) % 2;  // 0=name, 1=value
          int dstPart = (dstCol - 3) % 2;
          if (srcPart == dstPart) {  // Only paste name to name, value to value
            if (srcPart == 0) {
              chipnomadState->project.tables[tableIdx].rows[startRow + row].fx[dstFx][0] = cpBufTable.rows[row].fx[srcFx][0];
            } else {
              chipnomadState->project.tables[tableIdx].rows[startRow + row].fx[dstFx][1] = cpBufTable.rows[row].fx[srcFx][1];
            }
          }
        }
      }
    } else {
      // Handle regular multi-column paste
      for (int col = cpBufTableStartCol; col <= cpBufTableEndCol; col++) {
        if (col == 0) {
          chipnomadState->project.tables[tableIdx].rows[startRow + row].pitchFlag = cpBufTable.rows[row].pitchFlag;
        } else if (col == 1) {
          chipnomadState->project.tables[tableIdx].rows[startRow + row].pitchOffset = cpBufTable.rows[row].pitchOffset;
        } else if (col == 2) {
          chipnomadState->project.tables[tableIdx].rows[startRow + row].volume = cpBufTable.rows[row].volume;
        } else if (col >= 3 && col <= 10) {
          int fxIdx = (col - 3) / 2;
          int part = (col - 3) % 2;
          if (part == 0) {
            chipnomadState->project.tables[tableIdx].rows[startRow + row].fx[fxIdx][0] = cpBufTable.rows[row].fx[fxIdx][0];
          } else {
            chipnomadState->project.tables[tableIdx].rows[startRow + row].fx[fxIdx][1] = cpBufTable.rows[row].fx[fxIdx][1];
          }
        }
      }
    }
  }
  return rowsPasted;
}

// Selection mode switching helpers
static int isSingleCell(int startCol, int startRow, int endCol, int endRow) {
  return startCol == endCol && startRow == endRow;
}

static int isFullColumn(int startCol, int startRow, int endCol, int endRow) {
  return startCol == endCol && startRow == 0 && endRow == 15;
}

static int isFullRow(int startRow, int endRow, int startCol, int endCol, int maxCol) {
  return startRow == endRow && startCol == 0 && endCol == maxCol;
}

static void selectColumn(ScreenData* screen) {
  screen->selectStartRow = 0;
  screen->selectStartCol = screen->selectAnchorCol;
  screen->cursorRow = 15;
  screen->cursorCol = screen->selectAnchorCol;
}

static void selectRow(ScreenData* screen, int maxCol) {
  screen->selectStartRow = screen->selectAnchorRow;
  screen->selectStartCol = 0;
  screen->cursorRow = screen->selectAnchorRow;
  screen->cursorCol = maxCol;
}

static void selectAll(ScreenData* screen, int maxCol) {
  screen->selectStartRow = 0;
  screen->selectStartCol = 0;
  screen->cursorRow = 15;
  screen->cursorCol = maxCol;
}

// Selection mode switching
int switchPhraseSelectionMode(ScreenData* screen) {
  int startCol, startRow, endCol, endRow;
  getSelectionBounds(screen, &startCol, &startRow, &endCol, &endRow);

  if (isSingleCell(startCol, startRow, endCol, endRow)) {
    selectColumn(screen);
    return 0;
  }
  if (isFullColumn(startCol, startRow, endCol, endRow)) {
    selectRow(screen, 8);
    return 0;
  }
  if (isFullRow(startRow, endRow, startCol, endCol, 8)) {
    selectAll(screen, 8);
    return 0;
  }
  return 1;
}

int switchTableSelectionMode(ScreenData* screen) {
  int startCol, startRow, endCol, endRow;
  getSelectionBounds(screen, &startCol, &startRow, &endCol, &endRow);

  if (isSingleCell(startCol, startRow, endCol, endRow)) {
    selectColumn(screen);
    return 0;
  }
  if (isFullColumn(startCol, startRow, endCol, endRow)) {
    selectRow(screen, 10);
    return 0;
  }
  if (isFullRow(startRow, endRow, startCol, endCol, 10)) {
    selectAll(screen, 10);
    return 0;
  }
  return 1;
}

int switchChainSelectionMode(ScreenData* screen) {
  int startCol, startRow, endCol, endRow;
  getSelectionBounds(screen, &startCol, &startRow, &endCol, &endRow);

  if (isSingleCell(startCol, startRow, endCol, endRow)) {
    selectColumn(screen);
    return 0;
  }
  if (isFullColumn(startCol, startRow, endCol, endRow)) {
    selectRow(screen, 1);
    return 0;
  }
  if (isFullRow(startRow, endRow, startCol, endCol, 1)) {
    selectAll(screen, 1);
    return 0;
  }
  return 1;
}

int switchGrooveSelectionMode(ScreenData* screen) {
  int startCol, startRow, endCol, endRow;
  getSelectionBounds(screen, &startCol, &startRow, &endCol, &endRow);

  if (isSingleCell(startCol, startRow, endCol, endRow)) {
    selectAll(screen, 0);
    return 0;
  }
  return 1;
}

int switchSongSelectionMode(ScreenData* screen) {
  int startCol, startRow, endCol, endRow;
  getSelectionBounds(screen, &startCol, &startRow, &endCol, &endRow);

  if (isSingleCell(startCol, startRow, endCol, endRow)) {
    selectRow(screen, screen->getColumnCount(screen->selectAnchorRow) - 1);
    return 0;
  }
  return 1;
}

// Clone chain (shallow)
int cloneChain(int srcIdx, int dstIdx) {
  if (srcIdx >= PROJECT_MAX_CHAINS || dstIdx >= PROJECT_MAX_CHAINS) return 0;
  chipnomadState->project.chains[dstIdx] = chipnomadState->project.chains[srcIdx];
  return 1;
}

// Clone phrase
int clonePhrase(int srcIdx, int dstIdx) {
  if (srcIdx >= PROJECT_MAX_PHRASES || dstIdx >= PROJECT_MAX_PHRASES) return 0;
  chipnomadState->project.phrases[dstIdx] = chipnomadState->project.phrases[srcIdx];
  return 1;
}

// Clone instrument
int cloneInstrument(int srcIdx, int dstIdx) {
  if (srcIdx >= PROJECT_MAX_INSTRUMENTS || dstIdx >= PROJECT_MAX_INSTRUMENTS) return 0;
  chipnomadState->project.instruments[dstIdx] = chipnomadState->project.instruments[srcIdx];
  chipnomadState->project.tables[dstIdx] = chipnomadState->project.tables[srcIdx];
  return 1;
}

// Deep clone phrases within an existing chain
int deepCloneChain(int chainIdx) {
  if (chainIdx >= PROJECT_MAX_CHAINS) return 0;

  // Find all distinct phrases used in the chain
  uint16_t usedPhrases[16];
  uint16_t phraseMapping[16];
  int distinctCount = 0;

  for (int row = 0; row < 16; row++) {
    uint16_t phraseIdx = chipnomadState->project.chains[chainIdx].rows[row].phrase;
    if (phraseIdx == EMPTY_VALUE_16) {
      phraseMapping[row] = EMPTY_VALUE_16;
      continue;
    }

    // Check if phrase already processed
    int found = -1;
    for (int i = 0; i < distinctCount; i++) {
      if (usedPhrases[i] == phraseIdx) {
        found = i;
        break;
      }
    }

    if (found == -1) {
      // New phrase, find empty slot
      int newPhraseIdx = findEmptyPhrase(&chipnomadState->project, 0);
      if (newPhraseIdx == EMPTY_VALUE_16) return 0;

      // Clone the phrase
      clonePhrase(phraseIdx, newPhraseIdx);

      usedPhrases[distinctCount] = phraseIdx;
      phraseMapping[row] = newPhraseIdx;
      distinctCount++;
    } else {
      // Reuse existing mapping
      for (int i = 0; i < row; i++) {
        if (chipnomadState->project.chains[chainIdx].rows[i].phrase == phraseIdx) {
          phraseMapping[row] = phraseMapping[i];
          break;
        }
      }
    }
  }

  // Update chain with new phrase references
  for (int row = 0; row < 16; row++) {
    chipnomadState->project.chains[chainIdx].rows[row].phrase = phraseMapping[row];
  }

  return 1;
}

// Consolidated cloning functions
int cloneChainToNext(int srcIdx) {
  int nextEmpty = findEmptyChain(&chipnomadState->project, srcIdx + 1);
  if (nextEmpty == EMPTY_VALUE_16) return EMPTY_VALUE_16;
  cloneChain(srcIdx, nextEmpty);
  return nextEmpty;
}

int clonePhraseToNext(int srcIdx) {
  int nextEmpty = findEmptyPhrase(&chipnomadState->project, srcIdx + 1);
  if (nextEmpty == EMPTY_VALUE_16) return EMPTY_VALUE_16;
  clonePhrase(srcIdx, nextEmpty);
  return nextEmpty;
}

int cloneInstrumentToNext(int srcIdx) {
  int nextEmpty = findEmptyInstrument(&chipnomadState->project, srcIdx + 1);
  if (nextEmpty == EMPTY_VALUE_8) return EMPTY_VALUE_8;
  cloneInstrument(srcIdx, nextEmpty);
  return nextEmpty;
}

int deepCloneChainToNext(int srcIdx) {
  int nextEmpty = findEmptyChain(&chipnomadState->project, srcIdx + 1);
  if (nextEmpty == EMPTY_VALUE_16) return EMPTY_VALUE_16;
  cloneChain(srcIdx, nextEmpty);
  if (!deepCloneChain(nextEmpty)) return EMPTY_VALUE_16;
  return nextEmpty;
}

int cloneInstrumentsInPhrase(int phraseIdx, int startRow, int endRow) {
  uint8_t usedInstruments[16];
  uint8_t instrumentMapping[16];
  int distinctCount = 0;

  // Find all distinct instruments in selection
  for (int r = startRow; r <= endRow; r++) {
    uint8_t instIdx = chipnomadState->project.phrases[phraseIdx].rows[r].instrument;
    if (instIdx == EMPTY_VALUE_8) {
      instrumentMapping[r - startRow] = EMPTY_VALUE_8;
      continue;
    }

    // Check if instrument already processed
    int found = -1;
    for (int i = 0; i < distinctCount; i++) {
      if (usedInstruments[i] == instIdx) {
        found = i;
        break;
      }
    }

    if (found == -1) {
      // New instrument, clone it
      int newInstIdx = cloneInstrumentToNext(instIdx);
      if (newInstIdx == EMPTY_VALUE_8) return 0;

      usedInstruments[distinctCount] = instIdx;
      instrumentMapping[r - startRow] = newInstIdx;
      distinctCount++;
    } else {
      // Reuse existing mapping
      for (int i = 0; i < r - startRow; i++) {
        if (chipnomadState->project.phrases[phraseIdx].rows[startRow + i].instrument == instIdx) {
          instrumentMapping[r - startRow] = instrumentMapping[i];
          break;
        }
      }
    }
  }

  // Update phrase with new instrument references
  for (int r = startRow; r <= endRow; r++) {
    chipnomadState->project.phrases[phraseIdx].rows[r].instrument = instrumentMapping[r - startRow];
  }

  return distinctCount;
}

void copyInstrument(int instrumentIdx) {
  cpBufInstrument = chipnomadState->project.instruments[instrumentIdx];
  cpBufInstrumentTable = chipnomadState->project.tables[instrumentIdx];
  cpBufInstrumentValid = 1;
}

void pasteInstrument(int instrumentIdx) {
  if (!cpBufInstrumentValid) return;
  chipnomadState->project.instruments[instrumentIdx] = cpBufInstrument;
  chipnomadState->project.tables[instrumentIdx] = cpBufInstrumentTable;
}

void copyWavetable(int wavetableIdx) {
  memcpy(cpBufWavetable, chipnomadState->project.ayWavetables[wavetableIdx], 32);
  cpBufWavetableValid = 1;
}

void pasteWavetable(int wavetableIdx) {
  if (!cpBufWavetableValid) return;
  memcpy(chipnomadState->project.ayWavetables[wavetableIdx], cpBufWavetable, 32);
}
