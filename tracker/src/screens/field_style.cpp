#include "field_style.h"

#include "common.h"
#include "corelib_gfx.h"
#include "utils.h"

#include <math.h>

namespace {

// One bitmap per bar width, reused for every bar of that width.
Bitmap* bars[16];

Bitmap* barBitmap(int width) {
  if (width < 1 || width >= 16) return nullptr;
  Bitmap*& bitmap = bars[width];
  if (bitmap && (bitmap->widthPixels != width * gfxGetCharWidth() || bitmap->heightPixels != gfxGetCharHeight())) {
    gfxBitmapFree(bitmap);
    bitmap = nullptr;
  }
  if (!bitmap) bitmap = gfxBitmapCreate(width, 1);
  return bitmap;
}

void fill(Bitmap* b, int x0, int x1, int top, int bottom, uint8_t alpha) {
  if (x0 < 0) x0 = 0;
  if (x1 > b->widthPixels) x1 = b->widthPixels;
  for (int y = top; y < bottom; ++y)
    for (int x = x0; x < x1; ++x) b->data[y * b->widthPixels + x] = alpha;
}

}  // namespace

void fieldDrawLabel(int x, int y, const char* label, CellState state) {
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textLabel);
  gfxPrint(x, y, label);
}

void fieldDrawText(int x, int y, const char* text, int width, CellState state) {
  gfxClearRect(x, y, width, 1);
  if (text[0]) {
    gfxSetFgColor(fieldValueColor(state));
    gfxPrintf(x, y, "%.*s", width, text);
    return;
  }
  gfxSetFgColor(appSettings.colorScheme.textEmpty);
  for (int i = 0; i < width; ++i) gfxPrint(x + i, y, "-");
}

int fieldValueColor(CellState state) {
  return state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault;
}

int fieldNameColor(CellState state) {
  return state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textNames;
}

void fieldDrawNumber(int x, int y, uint8_t value, CellState state, float position, bool bipolar,
                     float livePosition) {
  gfxSetFgColor(fieldValueColor(state));
  gfxPrint(x, y, byteToHex(value));
  if (position < 0.0f) position = value / 255.0f;
  // The engine's rounding can land a step away from the set value; only a
  // value that has really moved gets a mark.
  if (livePosition < 0.0f || fabsf(livePosition - position) < 1.5f / 255.0f)
    fieldDrawBar(x + 2, y, kFieldBarWidth, position, state, bipolar);
  else fieldDrawBar(x + 2, y, kFieldBarWidth, livePosition, state, bipolar, position);
}

void fieldDrawBar(int x, int y, int width, float position, CellState state, bool bipolar, float mark) {
  gfxClearRect(x, y, width, 1);
  Bitmap* b = barBitmap(width);
  if (!b) return;
  gfxBitmapClear(b);
  // A band through the middle of the row, a little apart from the text on
  // either side.
  const int left = gfxGetCharWidth() / 4, right = b->widthPixels - gfxGetCharWidth() / 8;
  const int top = (int)lroundf(b->heightPixels * 0.3f), bottom = (int)lroundf(b->heightPixels * 0.72f);
  const int span = right - left;
  if (position < 0.0f) position = 0.0f;
  if (position > 1.0f) position = 1.0f;
  if (state == CellState::focus) fill(b, left, right, top, bottom, 40);
  const int at = left + (int)lroundf(position * span);
  if (bipolar) {
    // A faint mark shows where the middle is; the bar grows from it.
    const int middle = left + span / 2;
    fill(b, middle - 1, middle + 1, top, bottom, 110);
    if (abs(at - middle) < 2) fill(b, middle - 1, middle + 1, top, bottom, 255);
    else fill(b, at < middle ? at : middle, at < middle ? middle : at, top, bottom, 255);
  } else {
    // At zero, a sliver still shows where the bar starts.
    fill(b, left, at > left + 2 ? at : left + 2, top, bottom, 255);
  }
  if (mark >= 0.0f) {
    // A notch in the filled part, or a tick beyond it, a little taller than
    // the bar.
    const int m = left + (int)lroundf((mark > 1.0f ? 1.0f : mark) * span);
    for (int y = top - 2; y < bottom + 2; ++y)
      for (int x = m - 1; x < m + 1; ++x) {
        if (x < 0 || x >= b->widthPixels || y < 0 || y >= b->heightPixels) continue;
        uint8_t& pixel = b->data[y * b->widthPixels + x];
        pixel = pixel == 255 ? 60 : 200;
      }
  }
  gfxSetFgColor(appSettings.colorScheme.valueBar);
  gfxDrawBitmap(b, x, y);
}
