#include <SDL2/SDL.h>
#include <emscripten.h>
#include <emscripten/html5.h>

#include "corelib_keymap.h"
#include "corelib_mainloop.h"
#include "corelib_assets.h"
#include "corelib_gfx.h"
#include "../../src/app.h"

extern "C" EMSCRIPTEN_KEEPALIVE void webQueueButton(int button, int isDown) {
  static const int keyCodes[] = {
    BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
    BTN_A, BTN_B, BTN_START, BTN_SELECT
  };
  if (button < 0 || button >= (int)(sizeof(keyCodes) / sizeof(keyCodes[0]))) return;

  SDL_Event event = {};
  event.type = isDown ? SDL_KEYDOWN : SDL_KEYUP;
  event.key.keysym.sym = keyCodes[button];
  event.key.repeat = 0;
  SDL_PushEvent(&event);
}

struct WebLoopContext {
  void (*draw)(void);
  void (*onEvent)(MainLoopEventData eventData);
  int menu;
  bool stopped;
};

static void queueGamepadButton(WebLoopContext* context, int button, int down) {
  MainLoopEventData eventData;
  eventData.type = down ? MainLoopEvent::keyDown : MainLoopEvent::keyUp;
  eventData.data.input = (InputCode){InputDeviceType::gamepad, button};
  context->onEvent(eventData);
}

static void pollGamepad(WebLoopContext* context, float axes[4]) {
  static bool previousButtons[16] = {};
  bool buttons[16] = {};
  static const int buttonMap[] = {
    SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    gamepadTriggerLeft, gamepadTriggerRight,
    SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK,
    SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT
  };

  if (emscripten_sample_gamepad_data() == EMSCRIPTEN_RESULT_SUCCESS) {
    int count = emscripten_get_num_gamepads();
    for (int i = 0; i < count; ++i) {
      EmscriptenGamepadEvent gamepad = {};
      if (emscripten_get_gamepad_status(i, &gamepad) != EMSCRIPTEN_RESULT_SUCCESS || !gamepad.connected) continue;
      for (int button = 0; button < gamepad.numButtons && button < (int)(sizeof(buttonMap) / sizeof(buttonMap[0])); ++button)
        buttons[button] = gamepad.digitalButton[button];
      if (gamepad.numAxes > 0) axes[1] = (float)gamepad.axis[0];
      if (gamepad.numAxes > 1) axes[0] = (float)-gamepad.axis[1];
      if (gamepad.numAxes > 2) axes[3] = (float)gamepad.axis[2];
      if (gamepad.numAxes > 3) axes[2] = (float)-gamepad.axis[3];
      break;
    }
  }

  for (int button = 0; button < (int)(sizeof(buttonMap) / sizeof(buttonMap[0])); ++button) {
    if (buttons[button] != previousButtons[button]) queueGamepadButton(context, buttonMap[button], buttons[button]);
    previousButtons[button] = buttons[button];
  }
}

static void webLoopFrame(void* userdata) {
  WebLoopContext* context = static_cast<WebLoopContext*>(userdata);
  SDL_Event event;
  MainLoopEventData eventData;

  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN &&
        context->menu && event.key.keysym.sym == BTN_X)) {
      eventData.type = MainLoopEvent::exit;
      eventData.data.value = 0;
      context->onEvent(eventData);
      emscripten_cancel_main_loop();
      context->stopped = true;
      appCleanup();
      gfxCleanup();
      mainLoopQuit();
      return;
    }

    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
      if (event.type == SDL_KEYDOWN && event.key.repeat) continue;
      if (event.key.keysym.sym == BTN_MENU) {
        context->menu = event.type == SDL_KEYDOWN;
      } else {
        eventData.type = event.type == SDL_KEYDOWN
          ? MainLoopEvent::keyDown : MainLoopEvent::keyUp;
        eventData.data.input = (InputCode){InputDeviceType::keyboard,
          event.key.keysym.sym};
        context->onEvent(eventData);
      }
    }
  }

  float axes[4] = {};
  pollGamepad(context, axes);
  eventData.type = MainLoopEvent::gamepadAxes;
  for (int axis = 0; axis < 4; ++axis) eventData.data.axes[axis] = axes[axis];
  context->onEvent(eventData);

  eventData.type = MainLoopEvent::tick;
  eventData.data.value = 0;
  context->onEvent(eventData);
  context->draw();
  gfxUpdateScreen();
}

void mainLoopRun(void (*draw)(void), void (*onEvent)(MainLoopEventData eventData)) {
  assetsInit();
  static WebLoopContext context;
  context = {draw, onEvent, 0, false};
  emscripten_set_main_loop_arg(webLoopFrame, &context, 0, 1);
}

void mainLoopDelay(int ms) {
  emscripten_sleep(ms);
}

void mainLoopQuit(void) {
  SDL_Quit();
}

void mainLoopTriggerQuit(void) {
  SDL_Event event;
  event.type = SDL_QUIT;
  SDL_PushEvent(&event);
}
