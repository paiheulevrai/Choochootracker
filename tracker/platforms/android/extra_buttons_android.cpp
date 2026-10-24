// Pads in "extra buttons" mode (KEYCODE_BUTTON_1..16) reach us from
// ChooChooTrackerActivity.dispatchKeyEvent. They are queued as SDL user
// events and turned into gamepad key events by the SDL main loop.
#include <jni.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "extra_buttons_android.h"

extern "C" JNIEXPORT void JNICALL
Java_com_paiheulevrai_choochootracker_ChooChooTrackerActivity_nativeExtraButton(
    JNIEnv*, jclass, jint keyCode, jboolean down) {
  SDL_Event event = {};
  event.type = SDL_USEREVENT;
  event.user.code = EXTRA_BUTTON_EVENT_CODE;
  event.user.data1 = (void*)(intptr_t)keyCode;
  event.user.data2 = (void*)(intptr_t)(down ? 1 : 0);
  SDL_PushEvent(&event);
}
