#ifndef CHOOCHOO_LAUNCHPAD_GRID_H
#define CHOOCHOO_LAUNCHPAD_GRID_H

#include <stdint.h>

namespace launchpadGrid {
static constexpr int kWidth = 8;
static constexpr int kHeight = 8;
static constexpr uint8_t kOff = 0;
static constexpr uint8_t kAmber = 9;
static constexpr uint8_t kBlue = 45;
static constexpr uint8_t kGreen = 19;

// Programmer Mode note numbers increase left-to-right and bottom-to-top.
inline int noteFor(int column, int visibleRow) {
  return column < 0 || column >= kWidth || visibleRow < 0 || visibleRow >= kHeight
    ? -1 : 11 + column + 10 * (kHeight - 1 - visibleRow);
}

inline bool coordinatesForNote(int note, int* column, int* visibleRow) {
  const int offset = note - 11;
  if (offset < 0) return false;
  const int rowFromBottom = offset / 10;
  const int col = offset % 10;
  if (rowFromBottom >= kHeight || col >= kWidth) return false;
  *column = col;
  *visibleRow = kHeight - 1 - rowFromBottom;
  return true;
}

inline int songRowForVisibleRow(int topRow, int visibleRow) {
  return visibleRow < 0 || visibleRow >= kHeight ? -1 : topRow + visibleRow;
}

inline uint8_t colour(bool populated, bool queued, bool playing) {
  if (playing) return kGreen;
  if (queued) return kAmber;
  if (populated) return kBlue;
  return kOff;
}
}

#endif
