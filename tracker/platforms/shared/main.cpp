#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "corelib_gfx.h"
#include "corelib_font.h"
#include "corelib_mainloop.h"
#include "app.h"
#include "common.h"
#include "startup_timing.h"
#if defined(MACOS_BUILD) || defined(VITA_BUILD)
#include "corelib_assets.h"
#endif
#ifdef VITA_BUILD
void vitaPlatformInit(void);
#endif

int main(int argv, char** args) {
#ifdef VITA_BUILD
  vitaPlatformInit();
#endif
#if defined(MACOS_BUILD) || defined(VITA_BUILD)
  if (assetsInit() != 0) return 1;
#endif
  { StartupTiming timing("settings"); settingsLoad(); }

  // Load custom font before gfxSetup so it uses the correct font
  if (appSettings.fontPath[0] != '\0') {
    Font* font = fontLoad(appSettings.fontPath);
    if (font) {
      fontSetCurrent(font);
    } else {
      appSettings.fontPath[0] = '\0';
      fontSetCurrent(NULL);
    }
  }

  { StartupTiming timing("SDL/window/renderer/font atlas");
    if (gfxSetup(&appSettings.screenWidth, &appSettings.screenHeight) != 0) return 1;
  }

  appSetup();
  mainLoopRun(appDraw, appOnEvent);
#ifndef WEB_BUILD
  appCleanup();
  gfxCleanup();
  mainLoopQuit();
#endif

  return 0;
}

extern "C" int SDL_main(int argv, char** args) {
  return main(argv, args);
}
