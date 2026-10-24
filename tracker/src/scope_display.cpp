#include "scope_display.h"
#include "monitor_display.h"
#include "audio_monitor.h"
#include "common.h"
#include "corelib_gfx.h"
#include "screens/screens.h"
#include <algorithm>
#include <cmath>

static Bitmap* scope;

static Bitmap* sized(Bitmap*& bitmap, int columns, int rows) {
  if (bitmap && (bitmap->widthPixels != columns * gfxGetCharWidth() ||
                 bitmap->heightPixels != rows * gfxGetCharHeight())) {
    gfxBitmapFree(bitmap);
    bitmap = nullptr;
  }
  if (!bitmap) bitmap = gfxBitmapCreate(columns, rows);
  if (bitmap) gfxBitmapClear(bitmap);
  return bitmap;
}

void scopeDisplayDraw(void) {
  if (!chipnomadState || currentScreen == &screenTitle) return;
  ScreenOverlayCoordinates overlay;
  if (screenScopeRows(currentScreen) && sized(scope, 40, 2)) {
    const float* samples = monitorDisplayMixSamples();
    int w = scope->widthPixels, h = scope->heightPixels;
    float range = 0.25f;
    if (samples) for (int i = 0; i < AUDIO_MONITOR_SAMPLES; ++i) range = std::max(range, fabsf(samples[i]));
    int previous = h / 2;
    for (int x = 0; x < w; ++x) {
      int index = x * (AUDIO_MONITOR_SAMPLES - 1) / std::max(1, w - 1);
      float sample = samples && std::isfinite(samples[index]) ? samples[index] : 0;
      int y = std::max(1, std::min(h - 2, h / 2 - (int)(sample * (h / 2 - 2) / range)));
      if (!x) previous = y;
      for (int py = std::min(previous, y); py <= std::max(previous, y); ++py) scope->data[py * w + x] = 255;
      previous = y;
    }
    gfxSetBgColor(appSettings.colorScheme.background);
    gfxClearRect(0, 0, 40, 2);
    gfxSetFgColor(appSettings.colorScheme.textInfo);
    gfxDrawBitmap(scope, 0, 0);
  }
}
