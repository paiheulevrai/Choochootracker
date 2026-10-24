#include <SDL2/SDL.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <cstring>

#include "corelib_keymap.h"
#include "corelib_mainloop.h"
#include "corelib_assets.h"
#include "corelib_gfx.h"
#include "../../src/app.h"
#include "../../src/common.h"

static void syncBrowserTheme() {
  const auto& c = appSettings.colorScheme;
  const int colors[] = {c.background, c.textEmpty, c.textInfo, c.textDefault,
    c.textValue, c.textTitles, c.playMarkers, c.cursor, c.selection, c.warning};
  static int previous[10] = {};
  static char previousName[THEME_NAME_LENGTH + 1] = {};
  static bool initialized = false;
  if (initialized && !std::memcmp(previous, colors, sizeof(colors)) &&
      !std::strcmp(previousName, appSettings.themeName)) return;
  std::memcpy(previous, colors, sizeof(colors));
  std::memcpy(previousName, appSettings.themeName, sizeof(previousName));
  initialized = true;
  // Tab reloads do not follow the native app's quit/save path. Serialize the
  // selected theme immediately; the wrapper flushes it to its existing IDBFS.
  const int saveResult = settingsSave();
  EM_ASM({
    if (window.choochooApplyTheme)
      window.choochooApplyTheme([$0,$1,$2,$3,$4,$5,$6,$7,$8,$9], $10);
  }, colors[0], colors[1], colors[2], colors[3], colors[4], colors[5],
     colors[6], colors[7], colors[8], colors[9], saveResult);
}

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
  double lastDrawAt;
  bool hidden;
};

static WebLoopContext* activeWebLoop = nullptr;

extern "C" EMSCRIPTEN_KEEPALIVE void webSetPageHidden(int hidden) {
  if (!activeWebLoop || activeWebLoop->stopped || activeWebLoop->hidden == bool(hidden)) return;
  activeWebLoop->hidden = bool(hidden);
  // Key releases can be lost while a phone is locked or the tab is hidden.
  SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
  appResetInputState();
  activeWebLoop->menu = 0;
  MainLoopEventData event = {};
  event.type = hidden ? MainLoopEvent::sleep : MainLoopEvent::wake;
  activeWebLoop->onEvent(event);
}

static int frameTimeUs;
void mainLoopSetTargetFps(int) {}
int mainLoopGetTargetFps(void) { return 30; }
int mainLoopGetFrameTimeUs(void) { return frameTimeUs; }

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
  if (context->hidden) return;
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

  // SDL's browser audio callbacks share the main thread with drawing. Keep
  // input and app timers on requestAnimationFrame, but paint at about 30 Hz
  // to leave audio more scheduling headroom. Allow for timer rounding.
  const double now = emscripten_get_now();
  if (now - context->lastDrawAt < 1000.0 / 30.0 - 1.0) return;
  context->lastDrawAt = now;
  const double startedAt = emscripten_get_now();
  context->draw();
  syncBrowserTheme();
  gfxUpdateScreen();
  frameTimeUs = (int)((emscripten_get_now() - startedAt) * 1000.0);
}

void mainLoopRun(void (*draw)(void), void (*onEvent)(MainLoopEventData eventData)) {
  assetsInit();
  static WebLoopContext context;
  context = {draw, onEvent, 0, false, 0.0, false};
  activeWebLoop = &context;
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
