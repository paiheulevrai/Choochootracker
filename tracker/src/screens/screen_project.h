#ifndef __SCREEN_PROJECT_H__
#define __SCREEN_PROJECT_H__

#include "screens.h"

// Common rows on the project screen:
// 0 Load/Save/New/Export/Manage, 1 File, 2 Title, 3 Author, 4 Linear pitch,
// 5 Clock source, 6 Tempo (BPM). Pitch table and Scale follow.
#define SCR_PROJECT_ROWS (7)

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
