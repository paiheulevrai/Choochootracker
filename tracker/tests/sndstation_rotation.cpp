#include "../platforms/sdl2/sndstation_display.h"
#include <SDL2/SDL.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>

static void check(bool value, const char* message) {
  if (!value) {
    fprintf(stderr, "%s: %s\n", message, SDL_GetError());
    exit(1);
  }
}

static void testDisplay(int width, int height, bool rotated) {
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
  check(surface != nullptr, "surface");
  SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
  check(renderer != nullptr, "renderer");
  SDL_Texture* canvas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, 640, 360);
  check(canvas != nullptr, "canvas");
  check(SDL_SetRenderTarget(renderer, canvas) == 0, "select canvas");
  const SDL_Rect quadrants[] = {{0, 0, 320, 180}, {320, 0, 320, 180},
                                {0, 180, 320, 180}, {320, 180, 320, 180}};
  const Uint8 colors[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}};
  for (int i = 0; i < 4; ++i) {
    SDL_SetRenderDrawColor(renderer, colors[i][0], colors[i][1], colors[i][2], 255);
    check(SDL_RenderFillRect(renderer, &quadrants[i]) == 0, "draw quadrant");
  }
  check(SDL_SetRenderTarget(renderer, nullptr) == 0, "select window");
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  const SNDStationDisplay display = sndstationDisplay(width, height, 640, 360);
  check(display.rotated == rotated, "rotation selection");
  check(SDL_RenderCopyEx(renderer, canvas, nullptr, &display.destination,
                         display.rotated ? -90.0 : 0.0, nullptr, SDL_FLIP_NONE) == 0,
        "present");
  SDL_RenderPresent(renderer);
  const SDL_Rect b = display.bounds;
  for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) {
    int px = b.x + b.w * (x * 2 + 1) / 4;
    int py = b.y + b.h * (y * 2 + 1) / 4;
    int lx = px, ly = py;
    check(sndstationMapInput(display, 640, 360, &lx, &ly), "touch mapping");
    const int quadrant = (ly >= 180 ? 2 : 0) + (lx >= 320 ? 1 : 0);
    const Uint32 pixel = static_cast<Uint32*>(surface->pixels)[py * surface->pitch / 4 + px];
    Uint8 r, g, blue, alpha;
    SDL_GetRGBA(pixel, surface->format, &r, &g, &blue, &alpha);
    check(r == colors[quadrant][0] && g == colors[quadrant][1] &&
          blue == colors[quadrant][2], "rotation and input do not match");
    const int expected = rotated ? (x ? (y ? 2 : 3) : (y ? 0 : 1)) : y * 2 + x;
    check(quadrant == expected, "incorrect rotation direction");
  }
  if (b.y > 0) {
    int x = b.x, y = b.y - 1;
    check(!sndstationMapInput(display, 640, 360, &x, &y), "letterbox touch accepted");
  }
  SDL_DestroyTexture(canvas);
  SDL_DestroyRenderer(renderer);
  SDL_FreeSurface(surface);
}

static void testSoftwareRotation() {
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 720, 1280, 32, SDL_PIXELFORMAT_ARGB8888);
  check(surface != nullptr, "software surface");
  SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
  check(renderer != nullptr, "software renderer");
  SDL_Texture* canvas = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, 640, 360);
  SDL_Texture* rotated = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                           SDL_TEXTUREACCESS_STREAMING, 360, 640);
  check(canvas && rotated, "software rotation textures");
  check(SDL_SetTextureBlendMode(rotated, SDL_BLENDMODE_NONE) == 0, "software texture blend mode");
  check(SDL_SetRenderTarget(renderer, canvas) == 0, "software canvas");
  for (int y = 0; y < 360; y += 12) for (int x = 0; x < 640; x += 16) {
    SDL_SetRenderDrawColor(renderer, x % 256, y % 256, (x + y) % 256, 255);
    SDL_Rect cell{x, y, 16, 12};
    check(SDL_RenderFillRect(renderer, &cell) == 0, "software test pattern");
  }
  std::vector<uint32_t> source(640 * 360), reference(720 * 1280);
  check(SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA8888,
                             source.data(), 640 * sizeof(uint32_t)) == 0, "read canvas");
  const SNDStationDisplay display = sndstationDisplay(720, 1280, 640, 360);
  check(SDL_SetRenderTarget(renderer, nullptr) == 0, "software window");
  check(SDL_RenderCopyEx(renderer, canvas, nullptr, &display.destination, -90, nullptr, SDL_FLIP_NONE) == 0,
        "reference rotation");
  SDL_RenderPresent(renderer);
  for (int y = 0; y < 1280; ++y)
    memcpy(reference.data() + y * 720, (char*)surface->pixels + y * surface->pitch, 720 * sizeof(uint32_t));
  void* pixels = nullptr;
  int pitch = 0;
  check(SDL_LockTexture(rotated, nullptr, &pixels, &pitch) == 0, "lock software texture");
  sndstationRotatePixels(source.data(), 640, 360, pixels, pitch);
  SDL_UnlockTexture(rotated);
  check(SDL_RenderCopy(renderer, rotated, nullptr, &display.bounds) == 0, "software rotation");
  SDL_RenderPresent(renderer);
  for (int y = 0; y < 1280; ++y)
    check(memcmp(reference.data() + y * 720, (char*)surface->pixels + y * surface->pitch,
                 720 * sizeof(uint32_t)) == 0, "software rotation differs from SDL");
  SDL_DestroyTexture(rotated);
  SDL_DestroyTexture(canvas);
  SDL_DestroyRenderer(renderer);
  SDL_FreeSurface(surface);
}

int main() {
  check(SDL_Init(0) == 0, "SDL init");
  const uint32_t source[] = {1, 2, 3, 4, 5, 6};
  uint32_t padded[9] = {};
  sndstationRotatePixels(source, 3, 2, padded, 3 * sizeof(uint32_t));
  const uint32_t expected[] = {3, 6, 0, 2, 5, 0, 1, 4, 0};
  check(memcmp(padded, expected, sizeof(expected)) == 0, "rotation with padded texture pitch");
  testDisplay(360, 640, true);
  testDisplay(800, 800, false);
  testDisplay(719, 1280, true);
  testSoftwareRotation();
  SDL_Quit();
  puts("SNDStation rotation tests passed");
}
