// Minimal input stubs for tests
#include "corelib/corelib_input.h"

const ConsoleInputAlias* inputConsoleAliases(int* count) {
  static const ConsoleInputAlias aliases[] = {
    {1073741906, 11, "Up"}, {1073741905, 12, "Down"},
    {1073741904, 13, "Left"}, {1073741903, 14, "Right"},
    {'x', 0, "A"}, {'z', 1, "B"}, {'s', 2, "X"}, {'a', 3, "Y"},
    {1073742049, 4, "Select"}, {' ', 6, "Start"},
    {'q', 9, "L1"}, {'r', 10, "R1"},
    {'w', gamepadTriggerLeft, "L2"}, {'e', gamepadTriggerRight, "R2"},
  };
  *count = sizeof(aliases) / sizeof(aliases[0]);
  return aliases;
}

const char* inputGetKeyName(InputCode input) {
  return "?";
}

void inputInitDefaultKeyMapping(void) {
  // Do nothing in tests
}
