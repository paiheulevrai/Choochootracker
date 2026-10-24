#include "meter_display.h"
#include "monitor_display.h"
#include "audio_input_manager.h"
#include "common.h"
#include "corelib_gfx.h"
#include <algorithm>
#include <cmath>
#include <string.h>

static Bitmap* meter;

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

int monitorMeterHeight(float peak, int height) {
  if (!std::isfinite(peak) || peak <= 0 || height <= 0) return 0;
  float db = 20.0f * log10f(peak);
  return std::max(0, std::min(height, (int)ceilf((db + 48.0f) * height / 48.0f)));
}

void monitorDisplayDrawMeter(int track, int col, int row) {
  if (track < 0 || track >= PROJECT_MAX_TRACKS || !sized(meter, 2, 1)) return;
  const auto cs = appSettings.colorScheme;
  gfxSetBgColor(cs.background);
  gfxClearRect(col, row, 2, 1);
  const int w = meter->widthPixels, h = meter->heightPixels;
  for (int channel = 0; channel < 2; ++channel) for (int band = 0; band < 3; ++band) {
    gfxBitmapClear(meter);
    const int pixels = monitorMeterHeight(monitorDisplayTrackPeak(track, channel), h - 2);
    for (int level = 0; level < pixels; ++level) {
      int zone = level >= (h - 2) * 15 / 16 ? 2 : level >= (h - 2) * 3 / 4 ? 1 : 0;
      if (zone != band) continue;
      int left = channel ? w / 2 + 1 : w / 2 - w / 5;
      int right = channel ? w / 2 + w / 5 : w / 2 - 1;
      for (int x = left; x < right; ++x)
        meter->data[(h - 2 - level) * w + x] = 255;
    }
    gfxSetFgColor(band == 2 ? 0xff0000 : band == 1 ? cs.textTitles : cs.textInfo);
    gfxDrawBitmap(meter, col, row);
  }
}

// Horizontal 1-row meter for the audio input settings screen. Same 48 dB /
// 3-zone logic as monitorDisplayDrawMeter but rotated 90 degrees, reading
// from the AudioInputManager levels snapshot instead of track monitors.
// Shared per-frame snapshot for input meters: the first caller each frame
// consumes the published snapshot; later callers reuse the cached copy.
static AudioInputLevels inputLevelsCache;
static bool inputLevelsValid = false;

bool monitorDisplayInputLevels(AudioInputLevels* out, int* fresh) {
  AudioInputLevels received;
  if (audioInputManager.receiveLevels(received)) {
    inputLevelsCache = received;
    inputLevelsValid = true;
    if (fresh) *fresh = 1;
  } else if (fresh) {
    *fresh = 0;
  }
  if (!inputLevelsValid || !out) return inputLevelsValid;
  *out = inputLevelsCache;
  return true;
}

void monitorDisplayDrawHorizontalMeter(int armedSlot, int col, int row, int width) {
  if (armedSlot < 0 || armedSlot >= AUDIO_INPUT_MAX_ARMED || width <= 0) return;
  const auto cs = appSettings.colorScheme;
  gfxSetBgColor(cs.background);
  gfxClearRect(col, row, width, 1);
  AudioInputLevels levels;
  if (!monitorDisplayInputLevels(&levels, nullptr)) return;
  // Peak across both channels, mapped through the same 48 dB window.
  float peak = levels.peaks[armedSlot][0];
  float right = levels.peaks[armedSlot][1];
  if (right > peak) peak = right;
  if (!std::isfinite(peak) || peak <= 0) return;
  float db = 20.0f * log10f(peak);
  int fill = std::max(0, std::min(width, (int)ceilf((db + 48.0f) * width / 48.0f)));
  if (fill <= 0) return;
  // Solid cells: draw a 1x1 bitmap per zone run (a printed space glyph has
  // no pixels, so text-based fills render nothing).
  static Bitmap* cell = nullptr;
  if (!sized(cell, 1, 1)) return;
  int x = 0;
  while (x < fill) {
    int zone = x >= width - width / 16 ? 2 : x >= width * 3 / 4 ? 1 : 0;
    int runEnd = x + 1;
    while (runEnd < fill &&
           (runEnd >= width - width / 16 ? 2 : runEnd >= width * 3 / 4 ? 1 : 0) == zone)
      ++runEnd;
    gfxBitmapClear(cell);
    memset(cell->data, 255, (size_t)cell->widthPixels * cell->heightPixels);
    gfxSetFgColor(zone == 2 ? 0xff0000 : zone == 1 ? cs.textTitles : cs.textInfo);
    for (int cx = x; cx < runEnd; ++cx) gfxDrawBitmap(cell, col + cx, row);
    x = runEnd;
  }
}
