#define SNDSTATION_BUILD
#include "../platforms/sdl2/corelib_gfx.cpp"
#include <cassert>
#include <cstring>
#include <vector>

int main() {
  assert(SDL_Init(0) == 0);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 720, 1280, 32, SDL_PIXELFORMAT_ARGB8888);
  assert(surface);
  renderer = SDL_CreateSoftwareRenderer(surface);
  assert(renderer);
  rendererUsesGpu = 0;
  logicalW = screenW = 640;
  logicalH = screenH = 360;
  sndOutputW = 720;
  sndOutputH = 1280;
  assert(createCompositionTexture());
  assert(sndRotatedTexture && sndRotationPixels);
  for (int y = 0; y < 360; y += 12) for (int x = 0; x < 640; x += 16) {
    SDL_SetRenderDrawColor(renderer, x % 256, y % 256, (x + y) % 256, 255);
    SDL_Rect cell{x, y, 16, 12};
    assert(SDL_RenderFillRect(renderer, &cell) == 0);
  }
  const SNDStationDisplay display = sndstationDisplay(720, 1280, 640, 360);
  selectRenderTarget(NULL);
  assert(SDL_RenderCopyEx(renderer, compositionTexture, NULL, &display.destination, -90, NULL, SDL_FLIP_NONE) == 0);
  SDL_RenderPresent(renderer);
  std::vector<uint32_t> reference(720 * 1280);
  for (int y = 0; y < 1280; ++y)
    memcpy(reference.data() + y * 720, (char*)surface->pixels + y * surface->pitch, 720 * sizeof(uint32_t));
  useCompositionTarget();
  for (int frame = 0; frame < 3; ++frame) {
    isDirty = 1;
    gfxUpdateScreen();
    assert(!sndRotationFailed);
    for (int y = 0; y < 1280; ++y)
      assert(memcmp(reference.data() + y * 720, (char*)surface->pixels + y * surface->pitch,
                    720 * sizeof(uint32_t)) == 0);
  }
  sndRotationFailed = true;
  isDirty = 1;
  gfxUpdateScreen();
  for (int y = 0; y < 1280; ++y)
    assert(memcmp(reference.data() + y * 720, (char*)surface->pixels + y * surface->pitch,
                  720 * sizeof(uint32_t)) == 0);
  destroyCompositionTexture();
  assert(!sndRotatedTexture && !sndRotationPixels && !sndRotationFailed);
  sndOutputW = 719;
  assert(createCompositionTexture());
  assert(!sndRotatedTexture && !sndRotationPixels);
  destroyCompositionTexture();
  SDL_DestroyRenderer(renderer);
  SDL_FreeSurface(surface);
  SDL_Quit();
}
