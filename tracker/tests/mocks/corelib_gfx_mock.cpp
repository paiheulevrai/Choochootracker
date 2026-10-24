#include "corelib_gfx.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "app_ui_mock.h"
#include <algorithm>

char mockGfxCells[20][40];
int mockTextRight, mockTextBottom;
int mockCursorX, mockCursorY, mockCursorWidth;
int mockBitmapDrawCount, mockBitmapDrawRow;

int gfxSetup(int *screenWidth, int* screenHeight) { return 0; }
void gfxCleanup(void) {}
void gfxSetFgColor(int rgb) {}
void gfxSetCursorColor(int rgb) {}
void gfxSetBgColor(int rgb) {}
void gfxClear(void) {
  memset(mockGfxCells, ' ', sizeof(mockGfxCells));
  mockTextRight = mockTextBottom = -1;
}
void gfxUpdateScreen(void) {}
void gfxClearRect(int x, int y, int w, int h) {
  for (int row = y; row < y + h && row < 20; ++row)
    for (int col = x; col < x + w && col < 40; ++col)
      if (row >= 0 && col >= 0) mockGfxCells[row][col] = ' ';
}
void gfxCursor(int x, int y, int w) { mockCursorX = x; mockCursorY = y; mockCursorWidth = w; }
void gfxRect(int x, int y, int w, int h) {}
void gfxPrint(int x, int y, const char* text) {
  if (*text) {
    mockTextRight = std::max(mockTextRight, x + int(strlen(text)) - 1);
    mockTextBottom = std::max(mockTextBottom, y);
  }
  if (y < 0 || y >= 20) return;
  for (; *text && x < 40; ++text, ++x) if (x >= 0) mockGfxCells[y][x] = *text;
}
void gfxPrintf(int x, int y, const char* format, ...) {
  char text[256];
  va_list args;
  va_start(args, format);
  vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  gfxPrint(x, y, text);
}
void gfxDrawCharBitmap(uint8_t* bitmap, int col, int row) {}
Bitmap* gfxBitmapCreate(int widthChars, int heightChars) {
  Bitmap* bitmap = (Bitmap*)calloc(1, sizeof(Bitmap));
  bitmap->widthChars = widthChars; bitmap->heightChars = heightChars;
  bitmap->widthPixels = widthChars * gfxGetCharWidth(); bitmap->heightPixels = heightChars * gfxGetCharHeight();
  bitmap->data = (uint8_t*)calloc(bitmap->widthPixels * bitmap->heightPixels, 1);
  return bitmap;
}
void gfxBitmapClear(Bitmap* bitmap) { if (bitmap) memset(bitmap->data, 0, bitmap->widthPixels * bitmap->heightPixels); }
void gfxBitmapFree(Bitmap* bitmap) { if (bitmap) { free(bitmap->data); free(bitmap); } }
void gfxDrawBitmap(Bitmap* bitmap, int col, int row) {
  ++mockBitmapDrawCount;
  mockBitmapDrawRow = row;
}
int gfxGetCharWidth(void) { return 8; }
int gfxGetCharHeight(void) { return 16; }
void gfxReloadFont(void) {}
const char* gfxGetRendererType(void) { return "Software"; }
void gfxDrawHUD(void) {}
void gfxSetButtonPressed(int buttonIndex, int pressed) {}
