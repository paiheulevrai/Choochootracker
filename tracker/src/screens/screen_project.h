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

struct ProjectTapTempo {
  uint64_t last = 0;
  unsigned int intervals[4] = {};
  unsigned int count = 0, next = 0, sum = 0;
  bool started = false;

  void reset() { *this = {}; }
  int tap(uint64_t now) {
    if (!started || now <= last || now - last < 120 || now - last > 4000) {
      reset();
      last = now;
      started = true;
      return 0;
    }
    const unsigned int interval = (unsigned int)(now - last);
    last = now;
    if (count == 4) sum -= intervals[next];
    else ++count;
    intervals[next] = interval;
    sum += interval;
    next = (next + 1) % 4;
    return (60000 * count + sum / 2) / sum;
  }
};

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
