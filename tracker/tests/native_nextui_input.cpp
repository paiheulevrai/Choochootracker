// Standalone regression check using the production SDL input backend.
/* From tracker:
g++ -std=c++17 -DPORTMASTER_BUILD -I../chipnomad_lib -Isrc -Isrc/corelib \
  -Iplatforms/shared tests/native_nextui_input.cpp platforms/sdl2/corelib_input.cpp \
  $(pkg-config --cflags --libs sdl2) -o build/native-nextui-input
*/
#define SDL_MAIN_HANDLED
#include "common.h"
#include "corelib_keymap.h"
#include "console_input.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

AppSettings appSettings = {};

#ifdef main
#undef main
#endif
int main(int, char**) {
#ifdef _WIN32
  _putenv_s("CCT_NEXTUI", "");
#else
  unsetenv("CCT_NEXTUI");
#endif
  inputInitDefaultKeyMapping();
  assert(appSettings.keyMapping.keyEdit[0].deviceType == InputDeviceType::gamepad);
  assert(appSettings.keyMapping.keyEdit[0].code == SDL_CONTROLLER_BUTTON_A);
  assert(appSettings.keyMapping.keyUp[1].deviceType == InputDeviceType::none);
  const KeyMapping portmaster = appSettings.keyMapping;

#ifdef _WIN32
  _putenv_s("CCT_NEXTUI", "1");
#else
  setenv("CCT_NEXTUI", "1", 1);
#endif
  inputInitDefaultKeyMapping();
  assert(memcmp(&portmaster, &appSettings.keyMapping, sizeof(portmaster)) == 0);

  int count;
  const auto* aliases = inputConsoleAliases(&count);
  const struct { const InputCode* slots; int code; } defaults[] = {
    {appSettings.keyMapping.keyUp, SDL_CONTROLLER_BUTTON_DPAD_UP},
    {appSettings.keyMapping.keyDown, SDL_CONTROLLER_BUTTON_DPAD_DOWN},
    {appSettings.keyMapping.keyLeft, SDL_CONTROLLER_BUTTON_DPAD_LEFT},
    {appSettings.keyMapping.keyRight, SDL_CONTROLLER_BUTTON_DPAD_RIGHT},
    {appSettings.keyMapping.keyEdit, SDL_CONTROLLER_BUTTON_A},
    {appSettings.keyMapping.keyOpt, SDL_CONTROLLER_BUTTON_B},
    {appSettings.keyMapping.keyPlay, SDL_CONTROLLER_BUTTON_START},
    {appSettings.keyMapping.keyShift, SDL_CONTROLLER_BUTTON_BACK},
    {appSettings.keyMapping.keyMotionLive, SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
    {appSettings.keyMapping.keyMotionRecord, gamepadTriggerLeft},
    {appSettings.keyMapping.keyMotionErase, gamepadTriggerRight},
  };
  for (const auto& binding : defaults) {
    assert(binding.slots[0].deviceType == InputDeviceType::gamepad);
    assert(binding.slots[0].code == binding.code);
    assert(binding.slots[1].deviceType == InputDeviceType::none);
    assert(binding.slots[2].deviceType == InputDeviceType::none);
  }
  for (int i = 0; i < count; ++i) {
    const InputCode key{InputDeviceType::keyboard, aliases[i].keyboard};
    const InputCode pad{InputDeviceType::gamepad, aliases[i].gamepad};
    assert(consoleInput::same(consoleInput::normalize(key), pad));
    assert(strcmp(inputGetKeyName(key), inputGetKeyName(pad)) == 0);
    assert(strcmp(inputGetKeyName(key), aliases[i].name) == 0);
    for (bool keyFirst : {false, true}) {
      for (bool keyUpFirst : {false, true}) {
        consoleInput::Buttons buttons;
        auto first = keyFirst ? key : pad;
        auto second = keyFirst ? pad : key;
        assert(buttons.update(first, true));
        assert(!buttons.update(second, true));
        first = keyUpFirst ? key : pad;
        second = keyUpFirst ? pad : key;
        assert(!buttons.update(first, false));
        assert(buttons.update(second, false));
      }
    }
  }

  appSettings.keyMapping = {};
  appSettings.keyMapping.keyEdit[0] = {InputDeviceType::keyboard, BTN_A};
  appSettings.keyMapping.keyEdit[1] = {InputDeviceType::gamepad, SDL_CONTROLLER_BUTTON_A};
  appSettings.keyMapping.keyUndo[0] = {InputDeviceType::keyboard, SDLK_c};
  assert(consoleInput::migrate(appSettings.keyMapping));
  assert(appSettings.keyMapping.keyEdit[1].deviceType == InputDeviceType::none);
  assert(appSettings.keyMapping.keyUndo[0].code == SDLK_c);
  appSettings.keyMapping.keyOpt[0] = {InputDeviceType::keyboard, BTN_A};
  const KeyMapping conflict = appSettings.keyMapping;
  assert(!consoleInput::migrate(appSettings.keyMapping));
  assert(memcmp(&conflict, &appSettings.keyMapping, sizeof(conflict)) == 0);
  return 0;
}
