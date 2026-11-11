#ifndef __SCREEN_PROJECT_H__
#define __SCREEN_PROJECT_H__

#include "screens.h"

// 0 Actions, 1 File, 2 Title, 3 Author, 4 Tempo, 5 Sync,
// 6 Link connection, 7 Link options, 8 Sync out, 9 Tuning.
#define SCR_PROJECT_ROWS (10)

inline int projectRowY(int row) {
  constexpr int positions[] = {3, 5, 6, 7, 9, 10, 11, 12, 13, 15, 16, 17};
  return positions[row];
}

int projectLoadFromPath(const char* path);
void projectOpenFromScreen(const AppScreen* returnScreen);
void projectOpenFromScreenAtPath(const AppScreen* returnScreen, const char* path);
void projectCreateNewFromScreen(const AppScreen* returnScreen);

int projectCommonColumnCount(int row);
void projectCommonDrawStatic(void);
void projectCommonDrawCursor(int col, int row);
void projectCommonDrawField(int col, int row, CellState state);
int projectCommonOnEdit(int col, int row, CellEditAction action);

extern ScreenData screenProjectExtra;

#endif
