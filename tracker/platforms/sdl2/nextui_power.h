#ifndef NEXTUI_POWER_H
#define NEXTUI_POWER_H

#include <stdint.h>

// NextUI uses a short Power press for sleep. Leave long presses untouched.
struct NextUIPowerButton {
  bool held = false;
  bool resumed = false;
  uint32_t pressedAt = 0;
  uint32_t resumedAt = 0;

  bool update(bool down, uint32_t now, bool repeat = false) {
    if (down && repeat) return false;
    // The wake press may still be queued when the suspend helper returns.
    if (resumed && now - resumedAt < 1000) return false;
    resumed = false;
    if (down) {
      if (!held) pressedAt = now;
      held = true;
      return false;
    }
    const bool sleep = held && now - pressedAt < 1000;
    held = false;
    return sleep;
  }

  void onResume(uint32_t now) {
    held = false;
    resumed = true;
    resumedAt = now;
  }
};

#endif
