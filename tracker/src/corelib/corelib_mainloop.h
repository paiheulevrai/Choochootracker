#ifndef __CORELIB_MAINLOOP_H__
#define __CORELIB_MAINLOOP_H__

#include "corelib_input.h"

enum class MainLoopEvent {
  tick,
  keyDown,
  keyUp,
  gamepadAxes,
  exit,
  sleep,
  wake,
  fullRedraw,
  touchTap,
  touchAdjust,
  touchNavigate,
  // SDL_AUDIODEVICEREMOVED reached the platform loop. data.value is the
  // event's iscapture flag (1 = a capture device was removed/failed).
  audioDeviceRemoved,
};

struct MainLoopEventData {
  MainLoopEvent type;
  union {
    int value;
    InputCode input;
    float axes[4];
    struct { int x, y, direction; } touch;
  } data;
};

void mainLoopRun(void (*draw)(void), void (*onEvent)(MainLoopEventData eventData));
void mainLoopDelay(int ms);
void mainLoopQuit(void);
void mainLoopTriggerQuit(void);
// UI-only pacing. Audio remains owned by SDL's callback thread.
void mainLoopSetTargetFps(int fps);
int mainLoopGetTargetFps(void);
int mainLoopGetFrameTimeUs(void);

#endif
