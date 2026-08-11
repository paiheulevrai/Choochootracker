#include <SDL2/SDL.h>
#include <emscripten.h>

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
