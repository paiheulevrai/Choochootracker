#ifdef DESKTOP_BUILD

#include <stdio.h>
#include "screens.h"
#include "screen_project.h"
#include "corelib_gfx.h"

static GfxImage *sky, *far, *middle, *mainLayer, *viaduct, *foreground, *train, *logo;
static int frame;
static int selected;

static GfxImage* load(const char* name) {
  char path[64];
  snprintf(path, sizeof(path), "title/%s", name);
  return gfxImageLoadBMP(path);
}

static void init(void) {
  sky = load("sky.bmp");
  far = load("far.bmp");
  middle = load("middle.bmp");
  mainLayer = load("main.bmp");
  viaduct = load("viaduct.bmp");
  foreground = load("foreground.bmp");
  train = load("train.bmp");
  logo = load("logo.bmp");
}

static void setup(int input) { frame = 0; selected = 0; }
static void fullRedraw(void) {}

static void drawWrapped(const GfxImage* image, int offset) {
  const int width = gfxImageWidth(image);
  if (!width) return;
  offset %= width;
  offset &= ~1; // The title is rendered at half resolution, so avoid half-pixel sampling.
  int x = 0;
  while (x < 640) {
    int part = width - offset;
    if (part > 640 - x) part = 640 - x;
    gfxImageDrawCrop(image, offset, 0, part, gfxImageHeight(image), x, 0);
    x += part;
    offset = 0;
  }
}

static void drawMenu(void) {
  const char* items[] = {"CONTINUE", "OPEN", "NEW"};
  for (int i = 0; i < 3; i++) {
    gfxSetFgColor(i == selected ? 0xffd665 : 0xf2e6c7);
    gfxTitlePrint(15, 7 + i, i == selected && ((frame / 20) & 1) == 0 ? ">" : " ");
    gfxTitlePrint(17, 7 + i, items[i]);
  }
}

static void draw(void) {
  gfxTitleBegin();
  drawWrapped(sky, frame / 12);
  drawWrapped(far, frame / 4);
  drawWrapped(middle, frame / 2);
  drawWrapped(mainLayer, frame / 2);
  int vibration = frame % 173 < 6 ? (frame & 1 ? -1 : 1) : 0;
  if (train) {
    const int trainWidth = gfxImageWidth(train);
    const int trainX = ((30 + frame / 2 + trainWidth) % (640 + trainWidth) - trainWidth) & ~1;
    gfxImageDrawCrop(train, 0, 0, trainWidth, gfxImageHeight(train), trainX, 268 + vibration * 2);
  }
  drawWrapped(viaduct, frame / 2);
  drawWrapped(foreground, frame);
  if (logo) gfxImageDrawCrop(logo, 0, 0, gfxImageWidth(logo), gfxImageHeight(logo), 120, 28);
  drawMenu();
  if (frame < 30) gfxTitleFadeBlack((uint8_t)(255 - frame * 255 / 30));
  gfxTitlePresent();
  frame++;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (!isKeyDown) return 1;
  if (keys == keyUp && selected > 0) { selected--; return 1; }
  if (keys == keyDown && selected < 2) { selected++; return 1; }
  if (keys != keyEdit) return 1;
  gfxTitleEnd();
  if (selected == 0) screenSetup(&screenSong, 0);
  else if (selected == 1) projectOpenFromScreen(&screenTitle);
  else projectCreateNewFromScreen(&screenSong);
  return 1;
}

const AppScreen screenTitle = {
  .init = init,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = NULL,
};

#endif
