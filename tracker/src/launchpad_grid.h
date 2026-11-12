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
static constexpr uint8_t kUrgentAmber = 9;

enum class LedState : uint8_t { off, populated, queued, urgent, playing };

inline LedState ledState(bool populated, bool queued, bool urgent, bool playing) {
  if (playing) return LedState::playing;
  if (urgent) return LedState::urgent;
  if (queued) return LedState::queued;
  if (populated) return LedState::populated;
  return LedState::off;
}

inline uint8_t colourForState(LedState state) {
  switch (state) {
    case LedState::populated: return kBlue;
    case LedState::queued: return kAmber;
    case LedState::urgent: return kUrgentAmber;
    case LedState::playing: return kGreen;
    default: return kOff;
  }
}

inline bool isScreenActive(bool enabled, bool songScreen) { return enabled && songScreen; }

// One identity space for 64 grid pads plus eight scene buttons. The first
// press acts immediately; a released repeat inside the app's tap interval
// upgrades that same target to urgent. Held duplicate MIDI events are ignored.
class PressTracker {
 public:
  static constexpr int kSceneTargetStart = kWidth * kHeight;
  static constexpr int kNavigationTargetStart = kSceneTargetStart + kHeight;
  static constexpr int kTargetCount = kNavigationTargetStart + 4;
  void reset() {
    held_ = 0;
    lastTarget_ = -1;
    remainingFrames_ = 0;
  }
  void tick() {
    if (remainingFrames_ > 0 && --remainingFrames_ == 0) lastTarget_ = -1;
  }
  int press(int target, int intervalFrames) {
    if (target < 0 || target >= kTargetCount) return 0;
    const uint64_t mask = UINT64_C(1) << target;
    if (held_ & mask) return 0;
    held_ |= mask;
    const int taps = target == lastTarget_ && remainingFrames_ > 0 ? 2 : 1;
    lastTarget_ = target;
    remainingFrames_ = intervalFrames > 0 ? intervalFrames : 1;
    return taps;
  }
  void release(int target) {
    if (target >= 0 && target < kTargetCount) held_ &= ~(UINT64_C(1) << target);
  }
 private:
  uint64_t held_ = 0;
  int lastTarget_ = -1;
  int remainingFrames_ = 0;
};

enum class SongNavigation : uint8_t { none, up, down, left, right };

// Mini MK3 Programmer Mode top-row CCs 91-94 are Up, Down, Left, Right.
// Session is CC 95 and is deliberately ignored. Keep this decoder in the
// Song layout so future screen layouts can choose their own controls.
inline SongNavigation songNavigationForEvent(uint8_t status, uint8_t control) {
  if ((status & 0xf0) != 0xb0) return SongNavigation::none;
  switch (control) {
    case 91: return SongNavigation::up;
    case 92: return SongNavigation::down;
    case 93: return SongNavigation::left;
    case 94: return SongNavigation::right;
    default: return SongNavigation::none;
  }
}

inline int targetForNavigation(SongNavigation navigation) {
  return navigation == SongNavigation::none ? -1 :
    PressTracker::kNavigationTargetStart + (int)navigation - 1;
}

inline bool songRowForSceneControl(uint8_t control, int topRow, int* songRow) {
  if (control < 19 || control > 89 || (control - 19) % 10 != 0) return false;
  const int visibleRow = kHeight - 1 - (control - 19) / 10;
  *songRow = topRow + visibleRow;
  return true;
}

inline int sceneControlForVisibleRow(int visibleRow) {
  return visibleRow < 0 || visibleRow >= kHeight ? -1 : 89 - visibleRow * 10;
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
