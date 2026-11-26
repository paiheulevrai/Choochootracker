#ifndef SNDSTATION_DISPLAY_H
#define SNDSTATION_DISPLAY_H

#include <SDL2/SDL.h>

#ifdef SNDSTATION_BUILD
extern "C" void gfxMapSNDStationInput(int* x, int* y);
#endif

struct SNDStationDisplay {
  SDL_Rect destination;
  SDL_Rect bounds;
  bool rotated;
};

inline SNDStationDisplay sndstationDisplay(int width, int height, int canvasW, int canvasH) {
  const bool rotated = height > width;
  const int availableW = rotated ? height : width;
  const int availableH = rotated ? width : height;
  int drawW = availableW;
  int drawH = drawW * canvasH / canvasW;
  if (drawH > availableH) {
    drawH = availableH;
    drawW = drawH * canvasW / canvasH;
  }
  const int boundsW = rotated ? drawH : drawW;
  const int boundsH = rotated ? drawW : drawH;
  return {{(width - drawW) / 2, (height - drawH) / 2, drawW, drawH},
          {(width - boundsW) / 2, (height - boundsH) / 2, boundsW, boundsH},
          rotated};
}

inline bool sndstationMapInput(const SNDStationDisplay& display, int canvasW, int canvasH,
                               int* x, int* y) {
  const SDL_Rect& b = display.bounds;
  if (b.w <= 0 || b.h <= 0 || *x < b.x || *y < b.y ||
      *x >= b.x + b.w || *y >= b.y + b.h) {
    *x = *y = -1;
    return false;
  }
  const int px = *x - b.x, py = *y - b.y;
  if (display.rotated) {
    *x = canvasW - 1 - py * canvasW / b.h;
    *y = px * canvasH / b.w;
  } else {
    *x = px * canvasW / b.w;
    *y = py * canvasH / b.h;
  }
  return true;
}

inline void sndstationRotatePixels(const uint32_t* source, int width, int height,
                                   void* destination, int pitch) {
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      ((uint32_t*)((char*)destination + (width - 1 - x) * pitch))[y] = source[y * width + x];
}

#endif
