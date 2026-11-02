// Standalone regression check using the production SDL input backend.
/* From tracker:
g++ -std=c++17 -DPORTMASTER_BUILD -I../chipnomad_lib -Isrc -Isrc/corelib \
  -Iplatforms/shared tests/native_nextui_input.cpp platforms/sdl2/corelib_input.cpp \
  $(pkg-config --cflags --libs sdl2) -o build/native-nextui-input
*/
#include "common.h"
#include "corelib_keymap.h"
#include <cassert>
#include <cstdlib>
#include <cstring>

AppSettings appSettings = {};

int main() {
  unsetenv("CCT_NEXTUI");
  inputInitDefaultKeyMapping();
  assert(appSettings.keyMapping.keyEdit[0].code == BTN_A);
  assert(appSettings.keyMapping.keyUp[1].deviceType == InputDeviceType::none);
  const KeyMapping portmaster = appSettings.keyMapping;
  inputInitNativeGamepadMapping();
  assert(memcmp(&portmaster, &appSettings.keyMapping, sizeof(portmaster)) == 0);

  // Upgrade an existing keyboard-only installation without losing its keys.
  appSettings.keyMapping.keyEdit[0].code = SDLK_c;
  setenv("CCT_NEXTUI", "1", 1);
  inputInitNativeGamepadMapping();
  assert(appSettings.keyMapping.keyEdit[0].code == SDLK_c);
  assert(appSettings.keyMapping.keyUp[1].deviceType == InputDeviceType::gamepad);
  assert(appSettings.keyMapping.keyUp[1].code == SDL_CONTROLLER_BUTTON_DPAD_UP);
  assert(appSettings.keyMapping.keyEdit[1].code == SDL_CONTROLLER_BUTTON_A);
  assert(appSettings.keyMapping.keyOpt[1].code == SDL_CONTROLLER_BUTTON_B);
  assert(appSettings.keyMapping.keyPlay[1].code == SDL_CONTROLLER_BUTTON_START);
  assert(appSettings.keyMapping.keyShift[1].code == SDL_CONTROLLER_BUTTON_BACK);
  assert(appSettings.keyMapping.keyMotionRecord[1].code == gamepadTriggerLeft);
  assert(appSettings.keyMapping.keyMotionErase[1].code == gamepadTriggerRight);

  // Preserve controller customizations and deliberately empty bindings.
  appSettings.keyMapping.keyEdit[1].code = SDL_CONTROLLER_BUTTON_X;
  appSettings.keyMapping.keyOpt[1] = {InputDeviceType::none, 0};
  const KeyMapping custom = appSettings.keyMapping;
  inputInitNativeGamepadMapping();
  assert(memcmp(&custom, &appSettings.keyMapping, sizeof(custom)) == 0);

  // Reset and first launch both include native controls on NextUI.
  inputInitDefaultKeyMapping();
  assert(appSettings.keyMapping.keyEdit[0].code == BTN_A);
  assert(appSettings.keyMapping.keyEdit[1].code == SDL_CONTROLLER_BUTTON_A);
  assert(appSettings.keyMapping.keyOpt[1].deviceType == InputDeviceType::gamepad);
}
