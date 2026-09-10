#include "corelib_mainloop.h"
#include <stdint.h>
#include <stdlib.h>
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
SDL_Rect recButtonRect, delButtonRect, leftStickRect, rightStickRect;
float vpadStickAxes[4] = {};

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
  {&selectButtonRect, keyShift},
  {&recButtonRect, keyMotionRecord},
  {&delButtonRect, keyMotionErase}
};

static int isPointInRect(int x, int y, SDL_Rect* rect) {
  if (rect->w <= 0 || rect->h <= 0) return 0;
  // Phone taps are imprecise at the edge of a button. Keep the visual gap but
  // accept a small invisible halo; it never overlaps the next control.
  const int halo = 10;
  return (x >= rect->x - halo && x < rect->x + rect->w + halo &&
    y >= rect->y - halo && y < rect->y + rect->h + halo);
}

static int getTouchButton(int x, int y) {
  if (!vpadEnabled) return -1;

  for (int i = 0; i < (int)(sizeof(buttons) / sizeof(buttons[0])); i++) {
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

  FingerButton activeFingers[12] = {0};
  int numActiveFingers = 0;
  int buttonTouches[10] = {0};
  struct Gesture {
    SDL_FingerID fingerId;
    int active;
    int startX, startY;
    int startCol, startRow;
    int lastAdjustX;
    int navigation;
    int moved;
  } gesture = {};
#ifdef ANDROID_BUILD
  struct StickFinger { SDL_FingerID fingerId; } stickFingers[2] = {{-1}, {-1}};
#endif
#ifndef ANDROID_BUILD
  int mouseTouchButton = -1;
#endif

  extern void gfxSetButtonPressed(int buttonIndex, int pressed);
  auto releaseFingers = [&]() {
    for (int i = 0; i < (int)(sizeof(buttons) / sizeof(buttons[0])); ++i) {
      if (!buttonTouches[i]) continue;
      buttonTouches[i] = 0;
      gfxSetButtonPressed(i, 0);
      eventData.type = MainLoopEvent::keyUp;
      eventData.data.input = (InputCode){InputDeviceType::logical, buttons[i].key};
      onEvent(eventData);
    }
    numActiveFingers = 0;
    gesture.active = 0;
#ifdef ANDROID_BUILD
    int hadStick = stickFingers[0].fingerId != -1 || stickFingers[1].fingerId != -1;
    stickFingers[0].fingerId = stickFingers[1].fingerId = -1;
    for (int i = 0; i < 4; ++i) vpadStickAxes[i] = 0.0f;
    if (hadStick) {
      eventData.type = MainLoopEvent::keyUp;
      eventData.data.input = (InputCode){InputDeviceType::logical, keyMotionLive};
      onEvent(eventData);
    }
#endif
  };
  auto touchPosition = [&](const SDL_TouchFingerEvent& finger, int* x, int* y) {
    int width, height;
    gfxGetPhysicalSize(&width, &height);
#ifdef ANDROID_BUILD
    *x = (int)(finger.dx * width);
    *y = (int)(finger.dy * height);
#else
    *x = (int)(finger.x * width);
    *y = (int)(finger.y * height);
#endif
  };
  auto sendTouch = [&](MainLoopEvent type, int col, int row, int direction) {
    eventData.type = type;
    eventData.data.touch = {col, row, direction};
    onEvent(eventData);
  };
#ifdef ANDROID_BUILD
  auto stickAt = [&](int x, int y) {
    SDL_Rect* pads[] = {&leftStickRect, &rightStickRect};
    for (int i = 0; i < 2; ++i) {
      SDL_Rect* pad = pads[i];
      int cx = pad->x + pad->w / 2, cy = pad->y + pad->h / 2, r = pad->w / 2;
      int dx = x - cx, dy = y - cy;
      if (pad->w > 0 && dx * dx + dy * dy <= r * r) return i;
    }
    return -1;
  };
  auto updateStick = [&](int stick, int x, int y) {
    SDL_Rect* pad = stick == 0 ? &leftStickRect : &rightStickRect;
    int cx = pad->x + pad->w / 2, cy = pad->y + pad->h / 2, r = pad->w / 2;
    float horizontal = (float)(x - cx) / r;
    float vertical = (float)(cy - y) / r;
    if (horizontal < -1.0f) horizontal = -1.0f;
    if (horizontal > 1.0f) horizontal = 1.0f;
    if (vertical < -1.0f) vertical = -1.0f;
    if (vertical > 1.0f) vertical = 1.0f;
    vpadStickAxes[stick * 2] = vertical;
    vpadStickAxes[stick * 2 + 1] = horizontal;
  };
#endif
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
        if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
#ifdef TOUCH_INPUT
          releaseFingers();
#endif
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
                releaseFingers();
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
        touchPosition(event.tfinger, &dx, &dy);
#ifdef ANDROID_BUILD
        int stick = stickAt(dx, dy);
        if (stick >= 0 && stickFingers[stick].fingerId == -1) {
          int hadStick = stickFingers[0].fingerId != -1 || stickFingers[1].fingerId != -1;
          stickFingers[stick].fingerId = event.tfinger.fingerId;
          updateStick(stick, dx, dy);
          if (!hadStick) {
            eventData.type = MainLoopEvent::keyDown;
            eventData.data.input = (InputCode){InputDeviceType::logical, keyMotionLive};
            onEvent(eventData);
          }
        } else
#endif
        {
        int buttonIndex = getTouchButton(dx, dy);
        if (buttonIndex >= 0 && numActiveFingers < 12) {
          activeFingers[numActiveFingers].fingerId = event.tfinger.fingerId;
          activeFingers[numActiveFingers].buttonIndex = buttonIndex;
          numActiveFingers++;
          if (buttonTouches[buttonIndex]++ == 0) {
            gfxSetButtonPressed(buttonIndex, 1);
            eventData.type = MainLoopEvent::keyDown;
            eventData.data.input = (InputCode){InputDeviceType::logical, buttons[buttonIndex].key};
            onEvent(eventData);
          }
        } else if (!gesture.active) {
          int col, row;
          if (gfxGetTouchGridPosition(dx, dy, &col, &row)) {
            gesture = {event.tfinger.fingerId, 1, dx, dy, col, row, dx,
              col >= 34 && row >= 15, 0};
          }
        }
        }
      }
      else if (event.type == SDL_FINGERMOTION) {
        int x, y;
        touchPosition(event.tfinger, &x, &y);
#ifdef ANDROID_BUILD
        int stick = -1;
        for (int i = 0; i < 2; ++i) if (stickFingers[i].fingerId == event.tfinger.fingerId) stick = i;
        if (stick >= 0) {
          updateStick(stick, x, y);
        } else
#endif
        if (gesture.active && gesture.fingerId == event.tfinger.fingerId) {
        if (gesture.navigation) {
          int dx = x - gesture.startX;
          int dy = y - gesture.startY;
          int width, height;
          gfxGetPhysicalSize(&width, &height);
          int threshold = (width < height ? width : height) / 24;
          if (!gesture.moved && (abs(x - gesture.startX) >= threshold || abs(y - gesture.startY) >= threshold)) {
            int direction = abs(dx) >= abs(dy) ? (dx < 0 ? keyLeft : keyRight) :
              (dy < 0 ? keyUp : keyDown);
            sendTouch(MainLoopEvent::touchNavigate, gesture.startCol, gesture.startRow, direction);
            gesture.moved = 1;
          }
        } else {
          // Once a value drag has started, capture it until finger-up.  The
          // user may cross labels or leave the original cell while adjusting.
          int width, height;
          gfxGetPhysicalSize(&width, &height);
          int stepPixels = (width < height ? width : height) / 20;
          if (stepPixels < 24) stepPixels = 24;
          int delta = x - gesture.lastAdjustX;
          if (abs(delta) >= stepPixels) {
            // Horizontal drags use the existing coarse Edit+Up/Down actions.
            int direction = delta < 0 ? keyDown : keyUp;
            int steps = abs(delta) / stepPixels;
            for (int i = 0; i < steps; ++i)
              sendTouch(MainLoopEvent::touchAdjust, gesture.startCol, gesture.startRow, direction);
            gesture.lastAdjustX += delta < 0 ? -steps * stepPixels : steps * stepPixels;
            gesture.moved = 1;
          } else {
            int width, height;
            gfxGetPhysicalSize(&width, &height);
            int threshold = (width < height ? width : height) / 24;
            if (abs(x - gesture.startX) >= threshold || abs(y - gesture.startY) >= threshold)
              gesture.moved = 1;
          }
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
#ifdef ANDROID_BUILD
        for (int i = 0; i < 2; ++i) {
          if (stickFingers[i].fingerId != event.tfinger.fingerId) continue;
          stickFingers[i].fingerId = -1;
          vpadStickAxes[i * 2] = vpadStickAxes[i * 2 + 1] = 0.0f;
          if (stickFingers[0].fingerId == -1 && stickFingers[1].fingerId == -1) {
            eventData.type = MainLoopEvent::keyUp;
            eventData.data.input = (InputCode){InputDeviceType::logical, keyMotionLive};
            onEvent(eventData);
          }
          break;
        }
#endif
        if (gesture.active && gesture.fingerId == event.tfinger.fingerId) {
          if (!gesture.moved) sendTouch(MainLoopEvent::touchTap, gesture.startCol, gesture.startRow, 0);
          gesture.active = 0;
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
#ifdef TOUCH_INPUT
      for (int axis = 0; axis < 4; ++axis) eventData.data.axes[axis] = vpadStickAxes[axis];
#else
      for (int axis = 0; axis < 4; ++axis) eventData.data.axes[axis] = 0.0f;
#endif
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
