#include "corelib_gfx.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "app_ui_mock.h"

char mockGfxCells[20][40];
int mockCursorX, mockCursorY, mockCursorWidth;

int gfxSetup(int *screenWidth, int* screenHeight) { return 0; }
void gfxCleanup(void) {}
void gfxSetFgColor(int rgb) {}
void gfxSetCursorColor(int rgb) {}
void gfxSetBgColor(int rgb) {}
void gfxClear(void) { memset(mockGfxCells, ' ', sizeof(mockGfxCells)); }
void gfxUpdateScreen(void) {}
void gfxClearRect(int x, int y, int w, int h) {}
void gfxCursor(int x, int y, int w) { mockCursorX = x; mockCursorY = y; mockCursorWidth = w; }
void gfxRect(int x, int y, int w, int h) {}
void gfxPrint(int x, int y, const char* text) {
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
Bitmap* gfxBitmapCreate(int widthChars, int heightChars) { return NULL; }
void gfxBitmapClear(Bitmap* bitmap) {}
void gfxBitmapFree(Bitmap* bitmap) {}
void gfxDrawBitmap(Bitmap* bitmap, int col, int row) {}
int gfxGetCharWidth(void) { return 8; }
int gfxGetCharHeight(void) { return 16; }
void gfxReloadFont(void) {}
void gfxDrawHUD(void) {}
void gfxSetButtonPressed(int buttonIndex, int pressed) {}
