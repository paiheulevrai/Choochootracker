#include <SDL2/SDL.h>
#include <stdint.h>
#include "version.h"
#include "corelib_gfx.h"
#include "corelib_font.h"
#include "../../src/common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#ifdef TOUCH_INPUT
#include "button_icons.h"
#endif

#define PRINT_BUFFER_SIZE (256)
#define TEXT_COLS (40)
#define TEXT_ROWS (20)

#define CHAR_X(x) ((x) * charW + offsetX)
#define CHAR_Y(y) ((y) * charH + offsetY)

#ifdef TOUCH_INPUT
#define VPAD_BUTTON_SIZE 110
#define VPAD_MARGIN 30
#endif

extern const uint8_t font12x16[];
extern const uint8_t font16x24[];
extern const uint8_t font24x36[];
extern const uint8_t font32x48[];
extern const uint8_t font48x54[];

SDL_Window* window = NULL;
SDL_Renderer * renderer = NULL;

static uint32_t fgColor = 0;
static uint32_t bgColor = 0;
static uint32_t cursorColor = 0;
static char printBuffer[PRINT_BUFFER_SIZE];
static int screenW;
static int screenH;
static int logicalW;
static int logicalH;
#ifdef ANDROID_BUILD
// The tracker was designed for a 640x480 canvas. Keep that canvas intact on
// phones; physical pixels are used only by the touch-control overlay.
static int physicalW;
static int physicalH;
static int resizePresentDelay;

static SDL_Rect getTrackerViewport(void) {
  if (physicalW < physicalH) {
    const int button = physicalW / 8;
    const int gap = button / 4;
    const int controlsH = button * 7 + button / 3 + gap * 6;
    // Keep the complete canvas + controls composition on-screen, including
    // unusually short portrait displays.
    int canvasH = physicalW * 3 / 4;
    const int availableCanvasH = physicalH - controlsH - gap;
    if (canvasH > availableCanvasH) canvasH = availableCanvasH;
    if (canvasH < 1) canvasH = 1;
    const int canvasW = canvasH * 4 / 3;
    const int groupH = canvasH + gap + controlsH;
    return (SDL_Rect){(physicalW - canvasW) / 2, (physicalH - groupH) / 2,
      canvasW, canvasH};
  }
  const int canvasW = physicalH * 4 / 3;
  return (SDL_Rect){(physicalW - canvasW) / 2, 0, canvasW, physicalH};
}

static void useTrackerCanvas(void) {
  // SDL resets its viewport when changing render targets. The viewport is
  // explicit so portrait can centre the canvas + controls as one composition.
  SDL_Rect viewport = getTrackerViewport();
  SDL_RenderSetLogicalSize(renderer, 0, 0);
  SDL_RenderSetViewport(renderer, &viewport);
  SDL_RenderSetScale(renderer, (float)viewport.w / logicalW,
    (float)viewport.h / logicalH);
}
#endif
static int charW;
static int charH;
static int offsetX;
static int offsetY;
static int isDirty;
static const FontResolution* currentResolution = NULL;
static SDL_Texture* fontTexture = NULL;
static SDL_Rect charRects[95];

struct GfxImage {
  SDL_Texture* texture;
  int width;
  int height;
};

static int titleLogicalSizeActive = 0;
static SDL_Texture* titleTexture = NULL;

#ifdef TOUCH_INPUT
static void layoutVirtualPad(void) {
  extern SDL_Rect dpadUpRect, dpadDownRect, dpadLeftRect, dpadRightRect, dpadRect;
  extern SDL_Rect aButtonRect, bButtonRect, startButtonRect, selectButtonRect;
  extern SDL_Rect recButtonRect, delButtonRect, leftStickRect, rightStickRect;
  int layoutW = screenW;
  int layoutH = screenH;
#ifdef ANDROID_BUILD
  layoutW = physicalW;
  layoutH = physicalH;
#endif
  int btnSize = layoutW < layoutH ? layoutW / 8 : layoutH / 9;
  if (btnSize < 40) btnSize = 40;
  int margin = btnSize / 3;
  int gap = btnSize / 4;
  if (layoutW < layoutH) {
#ifdef ANDROID_BUILD
    SDL_Rect canvas = getTrackerViewport();
#else
    SDL_Rect canvas = {0, 0, layoutW, layoutH};
#endif
    const int y = canvas.y + canvas.h + gap;
    const int dpadSize = (layoutW - margin * 3) / 2;
    const int actionSize = btnSize * 7 / 4;
    const int actionX = layoutW - margin - actionSize;
    const int leftActionX = margin + dpadSize + gap;
    const int smallH = btnSize + btnSize / 3;
    dpadRect = (SDL_Rect){margin, y, dpadSize, dpadSize};
    dpadUpRect = (SDL_Rect){margin + dpadSize / 3, y, dpadSize / 3, dpadSize / 3};
    dpadDownRect = (SDL_Rect){margin + dpadSize / 3, y + dpadSize * 2 / 3, dpadSize / 3, dpadSize / 3};
    dpadLeftRect = (SDL_Rect){margin, y + dpadSize / 3, dpadSize / 3, dpadSize / 3};
    dpadRightRect = (SDL_Rect){margin + dpadSize * 2 / 3, y + dpadSize / 3, dpadSize / 3, dpadSize / 3};
    aButtonRect = (SDL_Rect){actionX, y, actionSize, actionSize};
    bButtonRect = (SDL_Rect){leftActionX, y + actionSize + gap, actionSize, actionSize};
    startButtonRect = (SDL_Rect){actionX, y + dpadSize - smallH, actionSize, smallH};
    selectButtonRect = (SDL_Rect){leftActionX, y + actionSize * 2 + gap * 2, actionSize, smallH};
    const int separatorY = selectButtonRect.y + selectButtonRect.h + gap * 2;
    const int stickSize = btnSize * 2;
    const int stickY = separatorY + gap * 2;
    leftStickRect = (SDL_Rect){margin, stickY, stickSize, stickSize};
    rightStickRect = (SDL_Rect){layoutW - margin - stickSize, stickY, stickSize, stickSize};
    const int motionW = btnSize;
    const int motionH = btnSize;
    const int motionY = stickY + (stickSize - motionH) / 2;
    recButtonRect = (SDL_Rect){layoutW / 2 - gap - motionW, motionY, motionW, motionH};
    delButtonRect = (SDL_Rect){layoutW / 2 + gap, motionY, motionW, motionH};
  } else {
    int sideBand = (layoutW - layoutH * 4 / 3) / 2;
    if (sideBand < margin * 3 + btnSize * 2) sideBand = margin * 3 + btnSize * 2;
    int dpadSize = sideBand - margin * 2;
    if (dpadSize > layoutH - margin * 2) dpadSize = layoutH - margin * 2;
    const int dpadY = (layoutH - dpadSize) / 2;
    const int actionSize = btnSize + btnSize / 2;
    const int smallH = btnSize;
    const int actionRight = layoutW - margin - actionSize;
    const int actionLeft = actionRight - actionSize - gap;
    const int y = (layoutH - (actionSize * 2 + gap * 3 + smallH * 2)) / 2;
    dpadRect = (SDL_Rect){margin, dpadY, dpadSize, dpadSize};
    dpadUpRect = (SDL_Rect){margin + dpadSize / 3, dpadY, dpadSize / 3, dpadSize / 3};
    dpadDownRect = (SDL_Rect){margin + dpadSize / 3, dpadY + dpadSize * 2 / 3, dpadSize / 3, dpadSize / 3};
    dpadLeftRect = (SDL_Rect){margin, dpadY + dpadSize / 3, dpadSize / 3, dpadSize / 3};
    dpadRightRect = (SDL_Rect){margin + dpadSize * 2 / 3, dpadY + dpadSize / 3, dpadSize / 3, dpadSize / 3};
    aButtonRect = (SDL_Rect){actionRight, y, actionSize, actionSize};
    bButtonRect = (SDL_Rect){actionLeft, y + actionSize + gap, actionSize, actionSize};
    startButtonRect = (SDL_Rect){actionRight, y + actionSize * 2 + gap * 2, actionSize, smallH};
    selectButtonRect = (SDL_Rect){actionLeft, y + actionSize * 2 + gap * 3 + smallH, actionSize, smallH};
    recButtonRect = delButtonRect = leftStickRect = rightStickRect = (SDL_Rect){0, 0, 0, 0};
  }
}
#endif

GfxImage* gfxImageLoadBMP(const char* path) {
  SDL_Surface* surface = SDL_LoadBMP(path);
  if (!surface) return NULL;
  const int width = surface->w;
  const int height = surface->h;
  SDL_Surface* rgba = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(surface);
  if (!rgba) return NULL;
  uint8_t* pixels = (uint8_t*)rgba->pixels;
  for (int y = 0; y < rgba->h; y++) {
    for (int x = 0; x < rgba->w; x++) {
      uint8_t* pixel = pixels + y * rgba->pitch + x * 4;
      // Image generation can shift the chroma key a few RGB values.
      if (pixel[0] > 100 && pixel[2] > 100 &&
          pixel[0] > pixel[1] * 1.6f && pixel[2] > pixel[1] * 1.6f) pixel[3] = 0;
    }
  }
  SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, rgba);
  GfxImage* image = texture ? (GfxImage*)malloc(sizeof(GfxImage)) : NULL;
  if (image) {
    image->texture = texture;
    image->width = width;
    image->height = height;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  } else if (texture) {
    SDL_DestroyTexture(texture);
  }
  SDL_FreeSurface(rgba);
  return image;
}

void gfxImageFree(GfxImage* image) {
  if (!image) return;
  SDL_DestroyTexture(image->texture);
  free(image);
}

int gfxImageWidth(const GfxImage* image) { return image ? image->width : 0; }
int gfxImageHeight(const GfxImage* image) { return image ? image->height : 0; }

void gfxImageDrawCrop(const GfxImage* image, int sourceX, int sourceY,
  int sourceW, int sourceH, int destinationX, int destinationY) {
  if (!image || !image->texture || sourceW <= 0 || sourceH <= 0) return;
  SDL_Rect source = {sourceX, sourceY, sourceW, sourceH};
  SDL_Rect destination = {destinationX, destinationY, sourceW, sourceH};
  SDL_RenderCopy(renderer, image->texture, &source, &destination);
  isDirty = 1;
}

void gfxTitleBegin(void) {
  if (!titleLogicalSizeActive) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    titleTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
      SDL_TEXTUREACCESS_TARGET, 256, 224);
    if (!titleTexture) return;
    SDL_SetTextureBlendMode(titleTexture, SDL_BLENDMODE_NONE);
    SDL_RenderSetLogicalSize(renderer, 640, 480);
    titleLogicalSizeActive = 1;
  }
  SDL_SetRenderTarget(renderer, titleTexture);
  SDL_RenderSetLogicalSize(renderer, 256, 224);
  SDL_RenderSetScale(renderer, 1.0f, 1.0f);
  SDL_SetRenderDrawColor(renderer, 5, 12, 31, 255);
  SDL_RenderClear(renderer);
  isDirty = 1;
}

void gfxTitleFadeBlack(uint8_t alpha) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
  SDL_Rect rect = {0, 0, 256, 224};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  isDirty = 1;
}

void gfxTitlePresent(void) {
  if (!titleTexture) return;
  SDL_SetRenderTarget(renderer, NULL);
#ifdef ANDROID_BUILD
  // A title frame only occupies the centered 4:3 canvas. Clear the physical
  // display first, otherwise pixels from the preceding tracker frame remain
  // visible in the side bands.
  SDL_RenderSetLogicalSize(renderer, physicalW, physicalH);
  SDL_SetRenderDrawColor(renderer, 5, 12, 31, 255);
  SDL_RenderClear(renderer);
  useTrackerCanvas();
#endif
  // Switching away from a render target resets the renderer viewport.
  // Restore the 4:3 logical canvas before presenting to a resized window.
#ifndef ANDROID_BUILD
  SDL_RenderSetLogicalSize(renderer, 640, 480);
#endif
  SDL_Rect destination = {0, 0, 640, 480};
  SDL_RenderCopy(renderer, titleTexture, NULL, &destination);
  // HUD hit boxes use the full mobile renderer coordinates.
#ifndef ANDROID_BUILD
  SDL_RenderSetLogicalSize(renderer, logicalW, logicalH);
#endif
  isDirty = 1;
}

void gfxTitleEnd(void) {
  SDL_SetRenderTarget(renderer, NULL);
  SDL_RenderSetScale(renderer, 1.0f, 1.0f);
  if (titleTexture) SDL_DestroyTexture(titleTexture);
  titleTexture = NULL;
  if (titleLogicalSizeActive) {
#ifdef ANDROID_BUILD
    useTrackerCanvas();
#else
    SDL_RenderSetLogicalSize(renderer, logicalW, logicalH);
#endif
  }
  titleLogicalSizeActive = 0;
}

void gfxTitlePrint(int x, int y, const char* text) {
  if (!text || !fontTexture) return;
  int cx = x * 8;
  int cy = y * 12;
  SDL_SetTextureColorMod(fontTexture, (fgColor >> 16) & 0xFF,
    (fgColor >> 8) & 0xFF, fgColor & 0xFF);
  for (int i = 0; text[i]; i++) {
    uint8_t c = text[i];
    if (c >= 32 && c <= 126) {
      SDL_Rect dst = {cx, cy, 8, 12};
      SDL_RenderCopy(renderer, fontTexture, &charRects[c - 32], &dst);
    }
    cx += 8;
  }
  isDirty = 1;
}

void gfxToggleFullscreen(void) {
#ifdef DESKTOP_BUILD
  const Uint32 flags = SDL_GetWindowFlags(window);
  SDL_SetWindowFullscreen(window, flags & SDL_WINDOW_FULLSCREEN_DESKTOP
    ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
#endif
}

#ifdef TOUCH_INPUT
static int buttonPressed[10] = {0};
#endif

static void createFontTexture(void) {
  if (!currentResolution || !currentResolution->data) return;

  int fontW = (currentResolution->charWidth + 7) / 8;  // Bytes per row
  const uint8_t* fontData = currentResolution->data;

  fontTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, charW * 95, charH);
  SDL_SetTextureBlendMode(fontTexture, SDL_BLENDMODE_BLEND);

  SDL_SetRenderTarget(renderer, fontTexture);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
  SDL_RenderClear(renderer);

  for (int ch = 0; ch < 95; ch++) {
    int charX = ch * charW;
    charRects[ch] = (SDL_Rect){charX, 0, charW, charH};

    for (int l = 0; l < charH; l++) {
      for (int c = 0; c < fontW; c++) {
        uint8_t fontByte = fontData[ch * fontW * charH + l * fontW + c];
        int bitsToDraw = (c == fontW - 1 && charW % 8 != 0) ? charW % 8 : 8;
        uint8_t mask = 0x80;

        for (int b = 0; b < bitsToDraw; b++) {
          if (fontByte & mask) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawPoint(renderer, charX + c * 8 + b, l);
          }
          mask >>= 1;
        }
      }
    }
  }

  SDL_SetRenderTarget(renderer, NULL);
}

// Some PortMaster SDL2 builds omit haptic and sensor support (including
// TrimUI Brick's NextUI build), so SDL_INIT_EVERYTHING would fail.
#if defined(PORTMASTER_BUILD)
#define SDL_INIT_FLAGS (SDL_INIT_EVERYTHING & ~SDL_INIT_HAPTIC & ~SDL_INIT_SENSOR)
#elif defined(WEB_BUILD)
#define SDL_INIT_FLAGS (SDL_INIT_EVERYTHING & ~SDL_INIT_HAPTIC)
#else
#define SDL_INIT_FLAGS (SDL_INIT_EVERYTHING)
#endif

int gfxSetup(int *screenWidth, int *screenHeight) {
  #ifdef ANDROID_BUILD
  SDL_SetHint(SDL_HINT_ORIENTATIONS,
    "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown");
  #endif
  if (SDL_Init(SDL_INIT_FLAGS) != 0) {
    fprintf(stderr, "SDL2 Initialization Error: %s\n", SDL_GetError());
    return 1;
  }

  snprintf(printBuffer, PRINT_BUFFER_SIZE, "%s v%s (%s)", appTitle, appVersion, appBuild);

  // Detect screen resolution if not provided or zero
  if (screenWidth == NULL || screenHeight == NULL || *screenWidth == 0 || *screenHeight == 0) {
    #if defined(DESKTOP_BUILD) || defined(WEB_BUILD)
      // Desktop and the HTML canvas use the tracker's native size.
      screenW = 640;
      screenH = 480;
    #else
      // For other platforms, try to get display mode
      SDL_DisplayMode dm;
      if (SDL_GetDesktopDisplayMode(0, &dm) == 0) {
        screenW = dm.w;
        screenH = dm.h;
      } else {
        // Fallback to 640x480 if detection fails
        screenW = 640;
        screenH = 480;
      }
    #endif
    // Update the pointers if they're not NULL
    if (screenWidth != NULL) *screenWidth = screenW;
    if (screenHeight != NULL) *screenHeight = screenH;
  } else {
    screenW = *screenWidth;
    screenH = *screenHeight;
  }

  window = SDL_CreateWindow(printBuffer,
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    screenW, screenH,
    SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
#ifdef ANDROID_BUILD
    | SDL_WINDOW_RESIZABLE
#endif
#ifdef DESKTOP_BUILD
    | SDL_WINDOW_RESIZABLE
#endif
  );

  if (!window) {
    fprintf(stderr, "SDL2 Create Window Error: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

  // Check for high-DPI display and get actual drawable size. HTML5 uses a
  // software canvas, so SDL_GL_GetDrawableSize is not meaningful there.
  int drawableW, drawableH;
#ifdef WEB_BUILD
  // Keep the tracker grid at its native 40x20 character layout. CSS scales
  // the 640x480 canvas for portrait phones without changing the font size.
  drawableW = screenW;
  drawableH = screenH;
#else
  SDL_GL_GetDrawableSize(window, &drawableW, &drawableH);
#endif
  if (drawableW <= 0 || drawableH <= 0) {
    SDL_GetWindowSize(window, &drawableW, &drawableH);
  }
  if (drawableW != screenW || drawableH != screenH) {
    screenW = drawableW;
    screenH = drawableH;
  }

#ifdef ANDROID_BUILD
  physicalW = screenW;
  physicalH = screenH;
  screenW = 640;
  screenH = 480;
#endif
  logicalW = screenW;
  logicalH = screenH;
#ifdef ANDROID_BUILD
  useTrackerCanvas();
#else
  SDL_RenderSetLogicalSize(renderer, logicalW, logicalH);
#endif

#ifdef WEB_BUILD
  // SDL's browser backend can report the physical screen size while the canvas
  // remains 640x480. Select the font against the canvas's fixed tracker grid.
  currentResolution = fontSelectResolution(fontGetCurrent(), 640, 480);
#else
  currentResolution = fontSelectResolution(fontGetCurrent(), screenW, screenH);
#endif
  if (currentResolution) {
    charW = currentResolution->charWidth;
    charH = currentResolution->charHeight;
  } else {
    // Fallback
    charW = 12;
    charH = 16;
  }

  // Center the text window
  int textWindowW = TEXT_COLS * charW;
  int textWindowH = TEXT_ROWS * charH;
  offsetX = (screenW - textWindowW) / 2;
#ifdef MOBILE_BUILD
  offsetY = 0;
#else
  offsetY = (screenH - textWindowH) / 2;
#endif

  createFontTexture();
  isDirty = 1;

#ifdef TOUCH_INPUT
  // Setup virtual gamepad layout using window coordinates
  extern int vpadEnabled;
  extern SDL_Rect dpadUpRect, dpadDownRect, dpadLeftRect, dpadRightRect, dpadRect;
  extern SDL_Rect aButtonRect, bButtonRect, startButtonRect, selectButtonRect;
  extern SDL_Rect dpadRect;

  layoutVirtualPad();
#endif

  return 0;
}

void gfxCleanup(void) {
  if (fontTexture) SDL_DestroyTexture(fontTexture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
}

void gfxSetFgColor(int rgb) {
  fgColor = rgb;
}

void gfxSetBgColor(int rgb) {
  bgColor = rgb;
}

void gfxSetCursorColor(int rgb) {
  cursorColor = rgb;
}

static void setColor(int rgb) {
  SDL_SetRenderDrawColor(renderer, (rgb & 0xff0000) >> 16, (rgb & 0xff00) >> 8, rgb & 0xff, 255);
}

void gfxClear(void) {
  setColor(bgColor);
  SDL_RenderClear(renderer);
  isDirty = 1;
}

void gfxPoint(int x, int y, uint32_t color) {
  setColor(color);
  SDL_RenderDrawPoint(renderer, x, y);
  isDirty = 1;
}

void gfxClearRect(int x, int y, int w, int h) {
  SDL_Rect rect = { CHAR_X(x), CHAR_Y(y), w * charW, h * charH };
  setColor(bgColor);
  SDL_RenderFillRect(renderer, &rect);
  isDirty = 1;
}

void gfxPrint(int x, int y, const char* text) {
  if (text == NULL) return;

  int cx = CHAR_X(x);
  int cy = CHAR_Y(y);
  int len = (int)strlen(text);

  // Draw background rectangles first
  setColor(bgColor);
  for (int i = 0; i < len; i++) {
    if (text[i] == '\r' && text[i + 1] == '\n') {
      i++;
      cx = CHAR_X(x);
      cy += charH;
      continue;
    }
    SDL_Rect bgRect = {cx, cy, charW, charH};
    SDL_RenderFillRect(renderer, &bgRect);
    cx += charW;
    if (cx >= offsetX + TEXT_COLS * charW) {
      cx = CHAR_X(x);
      cy += charH;
    }
  }

  // Draw characters
  cx = CHAR_X(x);
  cy = CHAR_Y(y);
  SDL_SetTextureColorMod(fontTexture, (fgColor >> 16) & 0xFF, (fgColor >> 8) & 0xFF, fgColor & 0xFF);

  for (int i = 0; i < len; i++) {
    uint8_t C = text[i];
    if (C == '\r' && text[i + 1] == '\n') {
      i++;
      cx = CHAR_X(x);
      cy += charH;
      if (cy >= offsetY + TEXT_ROWS * charH) {
        cy = CHAR_Y(y);
      }
      continue;
    }

    if (C >= 32 && C <= 126) {
      SDL_Rect dstRect = {cx, cy, charW, charH};
      SDL_RenderCopy(renderer, fontTexture, &charRects[C - 32], &dstRect);
    }

    cx += charW;
    if (cx >= offsetX + TEXT_COLS * charW) {
      cx = CHAR_X(x);
      cy += charH;
    }
  }
  isDirty = 1;
}

void gfxPrintf(int x, int y, const char* format, ...) {
  va_list args;
  va_start(args, format);
  vsnprintf(printBuffer, PRINT_BUFFER_SIZE, format, args);
  va_end(args);
  gfxPrint(x, y, printBuffer);
}

void gfxCursor(int x, int y, int w) {
  SDL_Rect rect = { CHAR_X(x), CHAR_Y(y) + charH - 1, w * charW, 1 };
  setColor(cursorColor);
  SDL_RenderFillRect(renderer, &rect);
  isDirty = 1;
}

void gfxRect(int x, int y, int w, int h) {
  int cx = CHAR_X(x);
  int cy = CHAR_Y(y);
  int cw = w * charW;
  int ch = h * charH;
  setColor(fgColor);

  SDL_Rect rects[4] = {
    {cx, cy, cw, 1},           // top
    {cx, cy + ch - 1, cw, 1}, // bottom
    {cx, cy, 1, ch},          // left
    {cx + cw - 1, cy, 1, ch}  // right
  };
  SDL_RenderFillRects(renderer, rects, 4);
  isDirty = 1;
}

void gfxUpdateScreen(void) {
#ifdef ANDROID_BUILD
  // SDL can report the new dimensions before Android has attached the new
  // surface. Presenting in that short window crashes inside SDL_RenderPresent.
  if (resizePresentDelay > 0) {
    resizePresentDelay--;
    isDirty = 1;
    return;
  }
#endif
  if (isDirty) {
    gfxDrawHUD();
    SDL_RenderPresent(renderer);
  }
  isDirty = 0;
}

void gfxDrawCharBitmap(uint8_t* bitmap, int col, int row) {
  int cx = CHAR_X(col);
  int cy = CHAR_Y(row);

  uint8_t fgR = (fgColor >> 16) & 0xFF;
  uint8_t fgG = (fgColor >> 8) & 0xFF;
  uint8_t fgB = fgColor & 0xFF;
  uint8_t bgR = (bgColor >> 16) & 0xFF;
  uint8_t bgG = (bgColor >> 8) & 0xFF;
  uint8_t bgB = bgColor & 0xFF;

  for (int y = 0; y < charH; y++) {
    for (int x = 0; x < charW; x++) {
      uint8_t alpha = bitmap[y * charW + x];
      uint8_t r = bgR + ((fgR - bgR) * alpha) / 255;
      uint8_t g = bgG + ((fgG - bgG) * alpha) / 255;
      uint8_t b = bgB + ((fgB - bgB) * alpha) / 255;
      SDL_SetRenderDrawColor(renderer, r, g, b, 255);
      SDL_RenderDrawPoint(renderer, cx + x, cy + y);
    }
  }
  isDirty = 1;
}

Bitmap* gfxBitmapCreate(int widthChars, int heightChars) {
  Bitmap* bitmap = (Bitmap*)malloc(sizeof(Bitmap));
  if (!bitmap) return NULL;

  bitmap->widthChars = widthChars;
  bitmap->heightChars = heightChars;
  bitmap->widthPixels = widthChars * charW;
  bitmap->heightPixels = heightChars * charH;

  // Allocate and zero the pixel data
  int dataSize = bitmap->widthPixels * bitmap->heightPixels;
  bitmap->data = (uint8_t*)malloc(dataSize);
  if (!bitmap->data) {
    free(bitmap);
    return NULL;
  }
  memset(bitmap->data, 0, dataSize);

  // Create SDL texture for hardware-accelerated rendering
  SDL_Texture* texture = SDL_CreateTexture(
    renderer,
    SDL_PIXELFORMAT_RGBA8888,
    SDL_TEXTUREACCESS_STREAMING,
    bitmap->widthPixels,
    bitmap->heightPixels
  );
  if (texture) {
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  }
  bitmap->userdata = texture;

  return bitmap;
}

void gfxBitmapClear(Bitmap* bitmap) {
  memset(bitmap->data, 0, bitmap->widthPixels * bitmap->heightPixels);
}

void gfxBitmapFree(Bitmap* bitmap) {
  if (!bitmap) return;

  if (bitmap->userdata) {
    SDL_DestroyTexture((SDL_Texture*)bitmap->userdata);
  }
  if (bitmap->data) {
    free(bitmap->data);
  }
  free(bitmap);
}

void gfxDrawBitmap(Bitmap* bitmap, int col, int row) {
  if (!bitmap || !bitmap->data) return;

  SDL_Texture* texture = (SDL_Texture*)bitmap->userdata;
  if (!texture) return;

  // Extract color components
  uint8_t fgR = (fgColor >> 16) & 0xFF;
  uint8_t fgG = (fgColor >> 8) & 0xFF;
  uint8_t fgB = fgColor & 0xFF;

  // Lock texture and update pixels
  void* pixels;
  int pitch;
  if (SDL_LockTexture(texture, NULL, &pixels, &pitch) == 0) {
    uint32_t* pixelData = (uint32_t*)pixels;

    for (int y = 0; y < bitmap->heightPixels; y++) {
      for (int x = 0; x < bitmap->widthPixels; x++) {
        uint8_t alpha = bitmap->data[y * bitmap->widthPixels + x];
        pixelData[y * (pitch / 4) + x] = (fgR << 24) | (fgG << 16) | (fgB << 8) | alpha;
      }
    }

    SDL_UnlockTexture(texture);
  }

  // Draw the texture at the specified character position
  SDL_Rect destRect = {
    CHAR_X(col),
    CHAR_Y(row),
    bitmap->widthPixels,
    bitmap->heightPixels
  };

  SDL_RenderCopy(renderer, texture, NULL, &destRect);
  isDirty = 1;
}

int gfxGetCharWidth(void) {
  return charW;
}

int gfxGetCharHeight(void) {
  return charH;
}

void gfxReloadFont(void) {
  if (fontTexture) {
    SDL_DestroyTexture(fontTexture);
    fontTexture = NULL;
  }

  currentResolution = fontSelectResolution(fontGetCurrent(), screenW, screenH);
  if (currentResolution) {
    charW = currentResolution->charWidth;
    charH = currentResolution->charHeight;
  }

  int textWindowW = TEXT_COLS * charW;
  int textWindowH = TEXT_ROWS * charH;
  offsetX = (screenW - textWindowW) / 2;
#ifdef MOBILE_BUILD
  offsetY = 0;
#else
  offsetY = (screenH - textWindowH) / 2;
#endif

  createFontTexture();
  isDirty = 1;
}

void gfxHandleResize(void) {
  if (!window || !renderer) return;
  int width, height;
  if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0 || width <= 0 || height <= 0) return;
#ifdef ANDROID_BUILD
  if (width == physicalW && height == physicalH) return;
  physicalW = width;
  physicalH = height;
  resizePresentDelay = 2;
  screenW = 640;
  screenH = 480;
#else
  if (width == screenW && height == screenH) return;
  screenW = width;
  screenH = height;
#endif
  logicalW = screenW;
  logicalH = screenH;
#ifdef ANDROID_BUILD
  useTrackerCanvas();
#else
  SDL_RenderSetLogicalSize(renderer, logicalW, logicalH);
#endif
  gfxReloadFont();
#ifdef TOUCH_INPUT
  layoutVirtualPad();
#endif
  isDirty = 1;
}

void gfxGetPhysicalSize(int* width, int* height) {
  int outputW = 0;
  int outputH = 0;
  if (renderer) SDL_GetRendererOutputSize(renderer, &outputW, &outputH);
#ifdef ANDROID_BUILD
  if (outputW <= 0) outputW = physicalW;
  if (outputH <= 0) outputH = physicalH;
#else
  if (outputW <= 0) outputW = screenW;
  if (outputH <= 0) outputH = screenH;
#endif
  if (width) *width = outputW;
  if (height) *height = outputH;
}

int gfxGetTouchGridPosition(int physicalX, int physicalY, int* col, int* row) {
  int logicalX = physicalX;
  int logicalY = physicalY;
#ifdef ANDROID_BUILD
  SDL_Rect viewport = getTrackerViewport();
  if (physicalX < viewport.x || physicalY < viewport.y ||
      physicalX >= viewport.x + viewport.w || physicalY >= viewport.y + viewport.h) return 0;
  logicalX = (physicalX - viewport.x) * logicalW / viewport.w;
  logicalY = (physicalY - viewport.y) * logicalH / viewport.h;
#endif
  if (!charW || !charH || logicalX < offsetX || logicalY < offsetY) return 0;
  int gridX = (logicalX - offsetX) / charW;
  int gridY = (logicalY - offsetY) / charH;
  if (gridX < 0 || gridX >= TEXT_COLS || gridY < 0 || gridY >= TEXT_ROWS) return 0;
  if (col) *col = gridX;
  if (row) *row = gridY;
  return 1;
}

#ifdef TOUCH_INPUT
static void drawFilledCircle(int cx, int cy, int radius, int color, int alpha) {
  constexpr int segments = 48;
  SDL_Vertex vertices[segments + 2];
  int indices[segments * 3];
  const SDL_Color drawColor = {(Uint8)(color >> 16), (Uint8)(color >> 8), (Uint8)color, (Uint8)alpha};
  vertices[0] = {{(float)cx, (float)cy}, drawColor, {0.0f, 0.0f}};
  for (int i = 0; i <= segments; ++i) {
    const float angle = 6.283185307f * i / segments;
    vertices[i + 1] = {{cx + cosf(angle) * radius, cy + sinf(angle) * radius}, drawColor, {0.0f, 0.0f}};
    if (i < segments) {
      indices[i * 3] = 0;
      indices[i * 3 + 1] = i + 1;
      indices[i * 3 + 2] = i + 2;
    }
  }
  SDL_RenderGeometry(renderer, NULL, vertices, segments + 2, indices, segments * 3);
}

static void drawCircleOutline(int cx, int cy, int radius, int color) {
  SDL_SetRenderDrawColor(renderer, (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff, 255);
  constexpr int segments = 48;
  int previousX = cx + radius;
  int previousY = cy;
  for (int i = 1; i <= segments; ++i) {
    const float angle = 6.283185307f * i / segments;
    const int x = cx + (int)(cosf(angle) * radius);
    const int y = cy + (int)(sinf(angle) * radius);
    SDL_RenderDrawLine(renderer, previousX, previousY, x, y);
    previousX = x;
    previousY = y;
  }
}

static void drawFilledTriangle(SDL_Point a, SDL_Point b, SDL_Point c, int color) {
  const SDL_Color drawColor = {(Uint8)(color >> 16), (Uint8)(color >> 8), (Uint8)color, 255};
  const SDL_Vertex vertices[3] = {
    {{(float)a.x, (float)a.y}, drawColor, {0.0f, 0.0f}},
    {{(float)b.x, (float)b.y}, drawColor, {0.0f, 0.0f}},
    {{(float)c.x, (float)c.y}, drawColor, {0.0f, 0.0f}},
  };
  SDL_RenderGeometry(renderer, NULL, vertices, 3, NULL, 0);
}

static int tintColor(int background, int foreground) {
  return (((((background >> 16) & 0xff) * 3 + ((foreground >> 16) & 0xff)) / 4) << 16) |
    (((((background >> 8) & 0xff) * 3 + ((foreground >> 8) & 0xff)) / 4) << 8) |
    ((((background & 0xff) * 3 + (foreground & 0xff)) / 4));
}

static void drawIcon(const uint8_t* iconData, int centerX, int y, int scale, int color) {
  if (!iconData) return;
  SDL_SetRenderDrawColor(renderer, (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff, 255);
  const int iconW = ICON_WIDTH * scale;
  const int iconX = centerX - iconW / 2;
  for (int py = 0; py < ICON_HEIGHT; ++py) for (int px = 0; px < ICON_WIDTH; ++px) {
    if (!(iconData[py * ICON_BYTES_PER_ROW + px / 8] & (1 << (7 - px % 8)))) continue;
    SDL_Rect pixel = {iconX + px * scale, y + py * scale, scale, scale};
    SDL_RenderFillRect(renderer, &pixel);
  }
}

static void drawTrackerLabel(const char* text, int centerX, int y, int color) {
  if (!text || !fontTexture || !currentResolution) return;
  const int width = (int)strlen(text) * charW;
  SDL_SetTextureColorMod(fontTexture, (color >> 16) & 0xff,
    (color >> 8) & 0xff, color & 0xff);
  for (int i = 0; text[i]; ++i) {
    const uint8_t c = text[i];
    if (c < 32 || c > 126) continue;
    SDL_Rect dst = {centerX - width / 2 + i * charW, y, charW, charH};
    SDL_RenderCopy(renderer, fontTexture, &charRects[c - 32], &dst);
  }
}

static void drawButton(SDL_Rect* rect, const uint8_t* iconData, int btnIndex) {
  const ColorScheme& colors = appSettings.colorScheme;
  int color = colors.textInfo;       // D-pad
  if (btnIndex == 4) color = colors.textValue; // A / EDIT
  if (btnIndex == 5) color = colors.textTitles; // B / OPT
  if (btnIndex >= 6) color = colors.selection; // SELECT / START
  if (btnIndex == 8) color = colors.textValue; // REC
  if (btnIndex == 9) color = colors.textTitles; // DEL
#ifdef ANDROID_BUILD
  // Use three theme accents: edit, modifier, then transport/navigation.
  if (btnIndex == 4) color = colors.warning; // A / EDIT
  if (btnIndex == 5) color = colors.cursor; // B / OPT
  if (btnIndex == 6 || btnIndex == 7) color = colors.textInfo;
  const int fill = tintColor(colors.background, color);
  const int labelColor = buttonPressed[btnIndex] ? colors.background : color;
  const int cx = rect->x + rect->w / 2;
  const int cy = rect->y + rect->h / 2;
  if (btnIndex == 4 || btnIndex == 5) {
    const int radius = rect->w / 2;
    drawFilledCircle(cx, cy, radius, buttonPressed[btnIndex] ? color : fill, 255);
    drawCircleOutline(cx, cy, radius, color);
  } else {
    const int face = buttonPressed[btnIndex] ? color : fill;
    SDL_SetRenderDrawColor(renderer, (face >> 16) & 0xff, (face >> 8) & 0xff, face & 0xff, 255);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff, 255);
    SDL_RenderDrawRect(renderer, rect);
  }
  if (btnIndex == 4) {
    drawTrackerLabel("A", cx, cy - charH - 2, labelColor);
    drawTrackerLabel("EDIT", cx, cy + 2, labelColor);
  } else if (btnIndex == 5) {
    drawTrackerLabel("B", cx, cy - charH - 2, labelColor);
    drawTrackerLabel("OPT", cx, cy + 2, labelColor);
  } else if (btnIndex == 6) {
    drawTrackerLabel("START", cx, cy - charH - 2, labelColor);
    drawTrackerLabel("PLAY", cx, cy + 2, labelColor);
  } else if (btnIndex == 7) {
    drawTrackerLabel("SELECT", cx, cy - charH - 2, labelColor);
    drawTrackerLabel("SHIFT", cx, cy + 2, labelColor);
  } else {
    const int scale = 3;
    drawIcon(iconData, cx, cy - ICON_HEIGHT * scale / 2, scale, labelColor);
  }
  return;
#endif
  uint8_t r = (color >> 16) & 0xff;
  uint8_t g = (color >> 8) & 0xff;
  uint8_t b = color & 0xff;
  if (buttonPressed[btnIndex]) { r = (uint8_t)(r * 0.68f); g = (uint8_t)(g * 0.68f); b = (uint8_t)(b * 0.68f); }
  SDL_SetRenderDrawColor(renderer, r, g, b, 245);
  SDL_RenderFillRect(renderer, rect);
  SDL_SetRenderDrawColor(renderer, (colors.background >> 16) & 0xff,
    (colors.background >> 8) & 0xff, colors.background & 0xff, 255);
  SDL_RenderDrawRect(renderer, rect);

  if (iconData) {
    int iconScale = rect->w / (ICON_WIDTH * 3);
    if (iconScale < 2) iconScale = 2;
    if (iconScale > 4) iconScale = 4;
    int iconW = ICON_WIDTH * iconScale;
    int iconH = ICON_HEIGHT * iconScale;
    int iconX = rect->x + (rect->w - iconW) / 2;
    int iconY = rect->y + (rect->h - iconH) / 2;
    const int labelColor = btnIndex >= 4 ? colors.background : colors.textDefault;
    SDL_SetRenderDrawColor(renderer, (labelColor >> 16) & 0xff,
      (labelColor >> 8) & 0xff, labelColor & 0xff, 255);
    for (int y = 0; y < ICON_HEIGHT; y++) {
      for (int x = 0; x < ICON_WIDTH; x++) {
        int byteIndex = y * ICON_BYTES_PER_ROW + x / 8;
        int bitIndex = 7 - (x % 8);
        if (iconData[byteIndex] & (1 << bitIndex)) {
          SDL_Rect pixel = { iconX + x * iconScale, iconY + y * iconScale,
            iconScale, iconScale };
          SDL_RenderFillRect(renderer, &pixel);
        }
      }
    }
  }
}

static void drawDpad(void) {
  extern SDL_Rect dpadUpRect, dpadDownRect, dpadLeftRect, dpadRightRect;
  const ColorScheme& colors = appSettings.colorScheme;
  const int base = colors.textInfo;
#ifdef ANDROID_BUILD
  extern SDL_Rect dpadRect;
  const int fill = tintColor(colors.background, base);
  SDL_SetRenderDrawColor(renderer, (fill >> 16) & 0xff, (fill >> 8) & 0xff, fill & 0xff, 255);
  SDL_RenderFillRect(renderer, &dpadRect);
  SDL_SetRenderDrawColor(renderer, (base >> 16) & 0xff, (base >> 8) & 0xff, base & 0xff, 255);
  SDL_RenderDrawRect(renderer, &dpadRect);
  SDL_RenderDrawLine(renderer, dpadRect.x, dpadRect.y, dpadRect.x + dpadRect.w - 1, dpadRect.y + dpadRect.h - 1);
  SDL_RenderDrawLine(renderer, dpadRect.x + dpadRect.w - 1, dpadRect.y, dpadRect.x, dpadRect.y + dpadRect.h - 1);
  SDL_Rect* androidParts[] = {&dpadUpRect, &dpadDownRect, &dpadLeftRect, &dpadRightRect};
  const uint8_t* androidIcons[] = {icon_arrow_up, icon_arrow_down, icon_arrow_left, icon_arrow_right};
  for (int i = 0; i < 4; ++i) {
    const int cx = androidParts[i]->x + androidParts[i]->w / 2;
    const int cy = androidParts[i]->y + androidParts[i]->h / 2;
    if (buttonPressed[i]) {
      const SDL_Point center = {dpadRect.x + dpadRect.w / 2, dpadRect.y + dpadRect.h / 2};
      if (i == 0) drawFilledTriangle({dpadRect.x, dpadRect.y}, {dpadRect.x + dpadRect.w, dpadRect.y}, center, base);
      if (i == 1) drawFilledTriangle({dpadRect.x, dpadRect.y + dpadRect.h}, {dpadRect.x + dpadRect.w, dpadRect.y + dpadRect.h}, center, base);
      if (i == 2) drawFilledTriangle({dpadRect.x, dpadRect.y}, {dpadRect.x, dpadRect.y + dpadRect.h}, center, base);
      if (i == 3) drawFilledTriangle({dpadRect.x + dpadRect.w, dpadRect.y}, {dpadRect.x + dpadRect.w, dpadRect.y + dpadRect.h}, center, base);
    }
    drawIcon(androidIcons[i], cx, cy - 8, 2, buttonPressed[i] ? colors.background : colors.textDefault);
  }
  return;
#endif
  const int shadow = ((base >> 17) & 0x7f) << 16 | ((base >> 9) & 0x7f) << 8 | ((base >> 1) & 0x7f);
  extern SDL_Rect dpadRect;
  int radius = dpadRect.w / 2;
  int cx = dpadRect.x + radius, cy = dpadRect.y + radius;
  drawFilledCircle(cx + 4, cy + 5, radius, shadow, 255);
  drawFilledCircle(cx, cy, radius, base, 255);
  drawFilledCircle(cx, cy, radius / 4, shadow, 255);
  SDL_Rect* parts[] = {&dpadUpRect, &dpadDownRect, &dpadLeftRect, &dpadRightRect};
  const uint8_t* icons[] = {icon_arrow_up, icon_arrow_down, icon_arrow_left, icon_arrow_right};
  for (int i = 0; i < 4; ++i) {
    if (buttonPressed[i]) {
      drawFilledCircle(parts[i]->x + parts[i]->w / 2, parts[i]->y + parts[i]->h / 2,
        parts[i]->w / 3, shadow, 255);
    }
    int scale = parts[i]->w / (ICON_WIDTH * 3);
    if (scale < 2) scale = 2;
    if (scale > 4) scale = 4;
    int x = parts[i]->x + (parts[i]->w - ICON_WIDTH * scale) / 2;
    int y = parts[i]->y + (parts[i]->h - ICON_HEIGHT * scale) / 2;
    SDL_SetRenderDrawColor(renderer, (colors.textDefault >> 16) & 0xff,
      (colors.textDefault >> 8) & 0xff, colors.textDefault & 0xff, 255);
    for (int py = 0; py < ICON_HEIGHT; ++py) for (int px = 0; px < ICON_WIDTH; ++px) {
      if (!(icons[i][py * ICON_BYTES_PER_ROW + px / 8] & (1 << (7 - px % 8)))) continue;
      SDL_Rect pixel = {x + px * scale, y + py * scale, scale, scale};
      SDL_RenderFillRect(renderer, &pixel);
    }
  }
}

#ifdef ANDROID_BUILD
static void drawStick(SDL_Rect* rect, int axis) {
  const ColorScheme& colors = appSettings.colorScheme;
  int r = rect->w / 2;
  int cx = rect->x + r, cy = rect->y + r;
  // Opaque redraw clears the previous knob position during playback redraws.
  drawFilledCircle(cx, cy, r, tintColor(colors.background, colors.selection), 255);
  drawCircleOutline(cx, cy, r, colors.selection);
  extern float vpadStickAxes[4];
  int knob = r / 3;
  int knobX = cx + (int)(vpadStickAxes[axis + 1] * (r - knob));
  int knobY = cy - (int)(vpadStickAxes[axis] * (r - knob));
  drawFilledCircle(knobX, knobY, knob, tintColor(colors.background, colors.textDefault), 255);
  drawCircleOutline(knobX, knobY, knob, colors.textDefault);
}

static void clearHUDBackground(void) {
  const SDL_Rect canvas = getTrackerViewport();
  const SDL_Rect areas[] = {
    {0, 0, physicalW, canvas.y},
    {0, canvas.y + canvas.h, physicalW, physicalH - canvas.y - canvas.h},
    {0, canvas.y, canvas.x, canvas.h},
    {canvas.x + canvas.w, canvas.y, physicalW - canvas.x - canvas.w, canvas.h},
  };
  setColor(appSettings.colorScheme.background);
  SDL_RenderFillRects(renderer, areas, sizeof(areas) / sizeof(*areas));
}
#endif
#endif

void gfxDrawHUD(void) {
#ifdef TOUCH_INPUT
  extern int vpadEnabled;
  extern SDL_Rect dpadUpRect, dpadDownRect, dpadLeftRect, dpadRightRect, dpadRect;
  extern SDL_Rect aButtonRect, bButtonRect, startButtonRect, selectButtonRect;
#ifdef ANDROID_BUILD
  extern SDL_Rect recButtonRect, delButtonRect, leftStickRect, rightStickRect;
#endif

  if (!vpadEnabled) return;

#ifdef ANDROID_BUILD
  // The tracker is a centered 640x480 canvas; controls are a physical overlay,
  // matching the Web controls placed outside that canvas.
  SDL_RenderSetLogicalSize(renderer, physicalW, physicalH);
  clearHUDBackground();
#endif

  drawDpad();
  #ifdef ANDROID_BUILD
  drawButton(&aButtonRect, icon_a, 4);
  drawButton(&bButtonRect, icon_b, 5);
  drawButton(&startButtonRect, icon_start, 6);
  drawButton(&selectButtonRect, icon_select, 7);
  #else
  drawButton(&aButtonRect, icon_edit, 4);
  drawButton(&bButtonRect, icon_opt, 5);
  drawButton(&startButtonRect, icon_play, 6);
  drawButton(&selectButtonRect, icon_shift, 7);
  #endif
#ifdef ANDROID_BUILD
  if (physicalH > physicalW) {
    drawButton(&recButtonRect, icon_rec, 8);
    drawButton(&delButtonRect, icon_del, 9);
    const int separatorY = (selectButtonRect.y + selectButtonRect.h + leftStickRect.y) / 2;
    SDL_SetRenderDrawColor(renderer, (appSettings.colorScheme.textInfo >> 16) & 0xff,
      (appSettings.colorScheme.textInfo >> 8) & 0xff, appSettings.colorScheme.textInfo & 0xff, 255);
    SDL_RenderDrawLine(renderer, dpadRect.x, separatorY,
      physicalW - dpadRect.x - 1, separatorY);
    drawStick(&leftStickRect, 0);
    drawStick(&rightStickRect, 2);
  }
#endif
#ifdef ANDROID_BUILD
  useTrackerCanvas();
#endif
#endif
}

void gfxSetButtonPressed(int buttonIndex, int pressed) {
#ifdef TOUCH_INPUT
  if (buttonIndex >= 0 && buttonIndex < 10) {
    buttonPressed[buttonIndex] = pressed;
    isDirty = 1;
  }
#endif
}
