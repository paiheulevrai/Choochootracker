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

enum class SongNavigation : uint8_t { none, up, down, left, right };

// Mini MK3 Programmer Mode top-row CCs 92-95 are the four navigation keys
// (the first top key, CC 91, is Session). Keep this decoder in the Song
// layout so future screen layouts can choose their own controls.
inline SongNavigation songNavigationForEvent(uint8_t status, uint8_t control, uint8_t value) {
  if ((status & 0xf0) != 0xb0 || value == 0) return SongNavigation::none;
  switch (control) {
    case 92: return SongNavigation::up;
    case 93: return SongNavigation::down;
    case 94: return SongNavigation::left;
    case 95: return SongNavigation::right;
    default: return SongNavigation::none;
  }
}

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
