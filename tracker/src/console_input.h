#ifndef CCT_CONSOLE_INPUT_H
#define CCT_CONSOLE_INPUT_H

#include "common.h"

#if defined(PORTMASTER_BUILD) || defined(TEST_PORTMASTER_INPUT)
namespace consoleInput {

inline bool same(InputCode a, InputCode b) {
  return a.deviceType == b.deviceType && a.code == b.code;
}

inline int aliasIndex(InputCode input) {
  int count;
  const auto* aliases = inputConsoleAliases(&count);
  for (int i = 0; i < count; ++i) {
    if ((input.deviceType == InputDeviceType::keyboard && input.code == aliases[i].keyboard) ||
        (input.deviceType == InputDeviceType::gamepad && input.code == aliases[i].gamepad))
      return i;
  }
  return -1;
}

inline InputCode normalize(InputCode input) {
  const int index = aliasIndex(input);
  if (index < 0) return input;
  int count;
  const auto* aliases = inputConsoleAliases(&count);
  return {InputDeviceType::gamepad, aliases[index].gamepad};
}

class Buttons {
  uint8_t held_[consoleInputButtonCount]{};

public:
  void clear() {
    for (auto& held : held_) held = 0;
  }

  bool update(InputCode& input, bool down) {
    const int index = aliasIndex(input);
    if (index < 0) return true;
    const uint8_t source = input.deviceType == InputDeviceType::keyboard ? 1 : 2;
    const bool wasDown = held_[index] != 0;
    if (down) held_[index] |= source;
    else held_[index] &= ~source;
    input = normalize(input);
    return wasDown != (held_[index] != 0);
  }

  template<class Release>
  void release(InputDeviceType source, Release emit) {
    int count;
    const auto* aliases = inputConsoleAliases(&count);
    const uint8_t mask = source == InputDeviceType::keyboard ? 1 : 2;
    for (int i = 0; i < count; ++i) {
      const bool wasDown = held_[i] != 0;
      held_[i] &= ~mask;
      if (wasDown && !held_[i]) emit(InputCode{InputDeviceType::gamepad, aliases[i].gamepad});
    }
  }
};

inline bool migrate(KeyMapping& mapping) {
  KeyMapping converted = mapping;
  InputCode* rows[] = {
    converted.keyUp, converted.keyDown, converted.keyLeft, converted.keyRight,
    converted.keyEdit, converted.keyOpt, converted.keyPlay, converted.keyShift,
    converted.keyMotionLive, converted.keyMotionRecord, converted.keyMotionErase,
    converted.keyUndo, converted.keyRedo
  };
  for (int row = 0; row < 13; ++row) {
    for (int col = 0; col < 3; ++col) {
      auto& input = rows[row][col];
      input = normalize(input);
      if (input.deviceType == InputDeviceType::none) continue;
      for (int previous = 0; previous <= row; ++previous) {
        const int end = previous == row ? col : 3;
        for (int slot = 0; slot < end; ++slot) {
          if (!same(input, rows[previous][slot])) continue;
          if (previous != row && aliasIndex(input) >= 0) return false;
          if (previous == row) input = {InputDeviceType::none, 0};
        }
      }
    }
  }
  mapping = converted;
  return true;
}

}
#endif
#endif
