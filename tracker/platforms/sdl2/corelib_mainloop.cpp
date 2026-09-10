#include "corelib_mainloop.h"
#include <stdint.h>
#include <SDL2/SDL.h>
#include "corelib_gfx.h"
#include "corelib_keymap.h"
#include "corelib_input.h"
#include "../../src/corelib/corelib_assets.h"

#define FPS 60

#ifdef GAMEPAD_SUPPORT
// Gamepad support
static SDL_GameController* gameController = NULL;
static int gamepadTriggerDown[2] = {};

static void initGamepad(void) {
#ifdef TOUCH_INPUT
  extern int vpadEnabled;
  vpadEnabled = 1;
#endif
  for (int i = 0; i < SDL_NumJoysticks(); i++) {
    if (SDL_IsGameController(i)) {
      gameController = SDL_GameControllerOpen(i);
      if (gameController) {
#ifdef TOUCH_INPUT
        vpadEnabled = 0;
#endif
        break;
      }
    }
  }
}
#endif

#ifdef TOUCH_INPUT
// Virtual gamepad state
int vpadEnabled = 0;
SDL_Rect dpadRect, aButtonRect, bButtonRect, startButtonRect, selectButtonRect;
SDL_Rect dpadUpRect, dpadDownRect, dpadLeftRect, dpadRightRect;

// Button definitions
struct Button {
  SDL_Rect* rect;
  int key;
};

static Button buttons[] = {
  {&dpadUpRect, keyUp},
  {&dpadDownRect, keyDown},
  {&dpadLeftRect, keyLeft},
  {&dpadRightRect, keyRight},
  {&aButtonRect, keyEdit},
  {&bButtonRect, keyOpt},
  {&startButtonRect, keyPlay},
  {&selectButtonRect, keyShift}
};

static int isPointInRect(int x, int y, SDL_Rect* rect) {
  // Phone taps are imprecise at the edge of a button. Keep the visual gap but
  // accept a small invisible halo; it never overlaps the next control.
  const int halo = 10;
  return (x >= rect->x - halo && x < rect->x + rect->w + halo &&
    y >= rect->y - halo && y < rect->y + rect->h + halo);
}

static int getTouchButton(int x, int y) {
  if (!vpadEnabled) return -1;

  for (int i = 0; i < 8; i++) {
    if (isPointInRect(x, y, buttons[i].rect)) {
      return i;
    }
  }
  return -1;
}

#ifdef ANDROID_BUILD
// SDL's renderer watcher remaps finger x/y through the current 4:3 viewport
// before queued events reach us, clamping touches in the control side bands.
// The event filter runs before renderer watchers, so preserve raw normalized
// window coordinates in dx/dy (unused by our button handling).
static int SDLCALL preserveRawTouchCoordinates(void*, SDL_Event* event) {
  if (event->type == SDL_FINGERDOWN || event->type == SDL_FINGERUP ||
      event->type == SDL_FINGERMOTION) {
    event->tfinger.dx = event->tfinger.x;
    event->tfinger.dy = event->tfinger.y;
  }
  return 1;
}
#endif
#endif

void mainLoopRun(void (*draw)(void), void (*onEvent)(MainLoopEventData eventData)) {
  uint32_t delay = 1000 / FPS;
  uint32_t start;
  uint32_t busytime = 0;
  SDL_Event event;
  int menu = 0;
  MainLoopEventData eventData;

#ifdef MOBILE_LIFECYCLE
  int wakeRedrawFrames = 0;
#endif

  // Initialize bundled assets
  assetsInit();

#ifdef TOUCH_INPUT
  struct FingerButton {
    SDL_FingerID fingerId;
    int buttonIndex;
  };

  FingerButton activeFingers[10] = {0};
  int numActiveFingers = 0;
  int buttonTouches[8] = {0};
#ifndef ANDROID_BUILD
  int mouseTouchButton = -1;
#endif

  extern void gfxSetButtonPressed(int buttonIndex, int pressed);
  auto releaseFingers = [&]() {
    for (int i = 0; i < 8; ++i) {
      if (!buttonTouches[i]) continue;
      buttonTouches[i] = 0;
      gfxSetButtonPressed(i, 0);
      eventData.type = MainLoopEvent::keyUp;
      eventData.data.input = (InputCode){InputDeviceType::logical, buttons[i].key};
      onEvent(eventData);
    }
    numActiveFingers = 0;
  };
#ifdef ANDROID_BUILD
  SDL_SetEventFilter(preserveRawTouchCoordinates, NULL);
#endif
#endif

#ifdef GAMEPAD_SUPPORT
  // Initialize gamepad support
  initGamepad();
#endif

  while (1) {
    start = SDL_GetTicks();

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && (
        (menu && event.key.keysym.sym == BTN_X)))) {
        eventData.type = MainLoopEvent::exit;
        eventData.data.value = 0;
        onEvent(eventData);
#ifdef GAMEPAD_SUPPORT
        if (gameController) {
          SDL_GameControllerClose(gameController);
          gameController = NULL;
        }
#endif
        return;
      }
#ifdef MOBILE_LIFECYCLE
      else if (event.type == SDL_APP_TERMINATING) {
#ifdef GAMEPAD_SUPPORT
        if (gameController) {
          SDL_GameControllerClose(gameController);
          gameController = NULL;
        }
#endif
      }
      else if (event.type == SDL_APP_WILLENTERBACKGROUND) {
#ifdef TOUCH_INPUT
        releaseFingers();
#endif
        eventData.type = MainLoopEvent::sleep;
        eventData.data.value = 0;
        onEvent(eventData);
      }
      else if (event.type == SDL_APP_DIDENTERFOREGROUND) {
        eventData.type = MainLoopEvent::wake;
        eventData.data.value = 0;
        onEvent(eventData);
        wakeRedrawFrames = FPS;
#ifdef GAMEPAD_SUPPORT
        if (gameController && !SDL_GameControllerGetAttached(gameController)) {
          SDL_GameControllerClose(gameController);
          gameController = NULL;
        }
        if (!gameController) {
          initGamepad();
        }
#endif
      }
      else if (event.type == SDL_RENDER_TARGETS_RESET || event.type == SDL_RENDER_DEVICE_RESET) {
        wakeRedrawFrames = FPS;
      }
      else if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
            event.window.event == SDL_WINDOWEVENT_RESIZED) {
          gfxHandleResize();
#ifdef TOUCH_INPUT
          releaseFingers();
#endif
          // A resize changes the explicit tracker viewport. Redraw the
          // current screen immediately instead of waiting for an input frame.
          eventData.type = MainLoopEvent::fullRedraw;
          eventData.data.value = 0;
          onEvent(eventData);
          wakeRedrawFrames = FPS;
        }
        if (event.window.event == SDL_WINDOWEVENT_RESTORED ||
            event.window.event == SDL_WINDOWEVENT_EXPOSED ||
            event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
          wakeRedrawFrames = FPS;
        }
      }
#endif
      else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        // SDL/OS keyboard repeat competes with the app's deterministic repeat timer.
        if (event.type == SDL_KEYDOWN && event.key.repeat) continue;
#ifdef DESKTOP_BUILD
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RETURN &&
            (event.key.keysym.mod & KMOD_ALT)) {
          gfxToggleFullscreen();
          continue;
        }
#endif
        if (event.key.keysym.sym == BTN_MENU) {
          menu = event.type == SDL_KEYDOWN;
        } else {
          eventData.type = event.type == SDL_KEYDOWN ? MainLoopEvent::keyDown : MainLoopEvent::keyUp;
          eventData.data.input = (InputCode){InputDeviceType::keyboard, event.key.keysym.sym};
          onEvent(eventData);
        }
      }
#ifdef GAMEPAD_SUPPORT
      if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) {
        eventData.type = event.type == SDL_CONTROLLERBUTTONDOWN ? MainLoopEvent::keyDown : MainLoopEvent::keyUp;
        eventData.data.input = (InputCode){InputDeviceType::gamepad, event.cbutton.button};
        onEvent(eventData);
      }
      else if (event.type == SDL_CONTROLLERDEVICEADDED) {
        if (!gameController) {
          for (int i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_IsGameController(i)) {
              gameController = SDL_GameControllerOpen(i);
              if (gameController) {
#ifdef TOUCH_INPUT
                vpadEnabled = 0;
#endif
                break;
              }
            }
          }
        }
      }
      else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
        if (gameController && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gameController))) {
          SDL_GameControllerClose(gameController);
          gameController = NULL;
#ifdef TOUCH_INPUT
          vpadEnabled = 1;
#endif
        }
      }
#endif
#ifdef TOUCH_INPUT
      if (event.type == SDL_FINGERDOWN) {
        int dx, dy;
        gfxGetPhysicalSize(&dx, &dy);
#ifdef ANDROID_BUILD
        int x = (int)(event.tfinger.dx * dx);
        int y = (int)(event.tfinger.dy * dy);
#else
        int x = (int)(event.tfinger.x * dx);
        int y = (int)(event.tfinger.y * dy);
#endif

        int buttonIndex = getTouchButton(x, y);
        if (buttonIndex >= 0 && numActiveFingers < 10) {
          activeFingers[numActiveFingers].fingerId = event.tfinger.fingerId;
          activeFingers[numActiveFingers].buttonIndex = buttonIndex;
          numActiveFingers++;
          if (buttonTouches[buttonIndex]++ == 0) {
            gfxSetButtonPressed(buttonIndex, 1);
            eventData.type = MainLoopEvent::keyDown;
            eventData.data.input = (InputCode){InputDeviceType::logical, buttons[buttonIndex].key};
            onEvent(eventData);
          }
        }
      }
      else if (event.type == SDL_FINGERUP) {
        for (int i = 0; i < numActiveFingers; i++) {
          if (activeFingers[i].fingerId == event.tfinger.fingerId) {
            int buttonIndex = activeFingers[i].buttonIndex;
            if (--buttonTouches[buttonIndex] == 0) {
              gfxSetButtonPressed(buttonIndex, 0);
              eventData.type = MainLoopEvent::keyUp;
              eventData.data.input = (InputCode){InputDeviceType::logical, buttons[buttonIndex].key};
              onEvent(eventData);
            }
            for (int j = i; j < numActiveFingers - 1; j++) {
              activeFingers[j] = activeFingers[j + 1];
            }
            numActiveFingers--;
            break;
          }
        }
      }
      // Non-Android touch backends may expose taps as mouse events. Android
      // always uses SDL finger IDs here: do not mix its synthetic mouse events
      // with fingers, or a two-finger shortcut can lose one held button.
#ifndef ANDROID_BUILD
      else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int buttonIndex = getTouchButton(event.button.x, event.button.y);
        if (buttonIndex >= 0 && mouseTouchButton < 0) {
          mouseTouchButton = buttonIndex;
          if (buttonTouches[buttonIndex]++ == 0) {
            gfxSetButtonPressed(buttonIndex, 1);
            eventData.type = MainLoopEvent::keyDown;
            eventData.data.input = (InputCode){InputDeviceType::logical, buttons[buttonIndex].key};
            onEvent(eventData);
          }
        }
      }
      else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT && mouseTouchButton >= 0) {
        int buttonIndex = mouseTouchButton;
        mouseTouchButton = -1;
        if (--buttonTouches[buttonIndex] == 0) {
          gfxSetButtonPressed(buttonIndex, 0);
          eventData.type = MainLoopEvent::keyUp;
          eventData.data.input = (InputCode){InputDeviceType::logical, buttons[buttonIndex].key};
          onEvent(eventData);
        }
      }
#endif
#endif
    }

#ifdef GAMEPAD_SUPPORT
    eventData.type = MainLoopEvent::gamepadAxes;
    for (int axis = 0; axis < 4; ++axis) eventData.data.axes[axis] = 0.0f;
    if (gameController && SDL_GameControllerGetAttached(gameController)) {
      int triggers[] = {
        SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16384,
        SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16384,
      };
      for (int trigger = 0; trigger < 2; ++trigger) {
        if (triggers[trigger] != gamepadTriggerDown[trigger]) {
          eventData.type = triggers[trigger] ? MainLoopEvent::keyDown : MainLoopEvent::keyUp;
          eventData.data.input = (InputCode){InputDeviceType::gamepad, gamepadTriggerLeft + trigger};
          onEvent(eventData);
          gamepadTriggerDown[trigger] = triggers[trigger];
        }
      }
      eventData.type = MainLoopEvent::gamepadAxes;
      eventData.data.axes[0] = -SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
      eventData.data.axes[1] = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
      eventData.data.axes[2] = -SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_RIGHTY) / 32767.0f;
      eventData.data.axes[3] = SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_RIGHTX) / 32767.0f;
    }
    else {
      for (int trigger = 0; trigger < 2; ++trigger) {
        if (!gamepadTriggerDown[trigger]) continue;
        eventData.type = MainLoopEvent::keyUp;
        eventData.data.input = (InputCode){InputDeviceType::gamepad, gamepadTriggerLeft + trigger};
        onEvent(eventData);
        gamepadTriggerDown[trigger] = 0;
      }
      eventData.type = MainLoopEvent::gamepadAxes;
    }
    onEvent(eventData);
#endif

#ifdef MOBILE_LIFECYCLE
    if (wakeRedrawFrames > 0) {
      eventData.type = MainLoopEvent::fullRedraw;
      eventData.data.value = 0;
      onEvent(eventData);
      wakeRedrawFrames--;
    }
#endif

    eventData.type = MainLoopEvent::tick;
    eventData.data.value = 0;
    onEvent(eventData);

    draw();
    gfxUpdateScreen();

    busytime = SDL_GetTicks() - start;
    if (delay > busytime) {
      SDL_Delay(delay - busytime);
    }
  }
}

void mainLoopDelay(int ms) {
  SDL_Delay(ms);
}

void mainLoopQuit(void) {
  SDL_Quit();
}

void mainLoopTriggerQuit(void) {
  SDL_Event quitEvent;
  quitEvent.type = SDL_QUIT;
  SDL_PushEvent(&quitEvent);
}
