#include <stdio.h>
#include <string.h>
#include <math.h>
#include <new>
#include <chrono>
#include "title_night_sky.h"
#include "common.h"
#include "screens.h"
#include "screen_project.h"
#include "corelib_gfx.h"
#include "corelib_file.h"
#include "corelib_mainloop.h"

static GfxImage *sky, *scene, *foreground, *viaduct, *train, *logo;
static GfxImage *mountains, *meteor;
static uint64_t frame;
static bool nightMode;
static TitleNightSky* nightSky;
static std::chrono::steady_clock::time_point skyStartedAt;
static int selected;
static int continueAvailable;

// Pixel scrolling stays crisp, but must advance often enough to look fluid at
// the fixed 60 Hz title-screen cadence.
enum {
  SKY_SCROLL_FRAMES = 13,
  SCENE_SCROLL_FRAMES = 8,
  VIADUCT_SCROLL_FRAMES = 5,
  FOREGROUND_SCROLL_FRAMES = 3,
  TRAIN_SCROLL_FRAMES = 5,
};

static void unload(void) {
  delete nightSky; nightSky = NULL;
  gfxImageFree(sky); sky = NULL;
  gfxImageFree(scene); scene = NULL;
  gfxImageFree(foreground); foreground = NULL;
  gfxImageFree(viaduct); viaduct = NULL;
  gfxImageFree(train); train = NULL;
  gfxImageFree(logo); logo = NULL;
  gfxImageFree(mountains); mountains = NULL;
  gfxImageFree(meteor); meteor = NULL;
}

static GfxImage* load(const char* name) {
  char path[PATH_LENGTH + 64];
#ifdef ANDROID_BUILD
  extern int androidGetPrivateWorkspacePath(char*, int);
  char workspace[PATH_LENGTH];
  if (androidGetPrivateWorkspacePath(workspace, sizeof(workspace)) == 0) {
    snprintf(path, sizeof(path), "%s/title/%s", workspace, name);
    return gfxImageLoadBMP(path);
  }
#endif
  snprintf(path, sizeof(path), "title/%s", name);
  return gfxImageLoadBMP(path);
}

static void init(void) {
  unload();
  nightMode = appSettings.bootScreenNight != 0;
  if (nightMode) {
    nightSky = new (std::nothrow) TitleNightSky();
    sky = gfxImageCreateARGB(TitleNightSky::width, TitleNightSky::height);
    mountains = load("night_mountains.bmp");
    scene = load("night_scene.bmp");
    foreground = load("night_foreground.bmp");
    viaduct = load("night_viaduct.bmp");
    train = load("night_train.bmp");
    logo = load("night_logo.bmp");
    meteor = load("night_meteor.bmp");
    if (nightSky && sky && mountains && scene && foreground && viaduct && train && logo && meteor) {
      nightSky->render(0);
      if (gfxImageUpdateARGB(sky, nightSky->pixels)) return;
    }
    // Older/incomplete packages must still offer a usable title screen.
    unload();
    nightMode = false;
  }
  sky = load("snes_sky.bmp");
  scene = load("snes_scene.bmp");
  foreground = load("snes_foreground.bmp");
  viaduct = load("snes_viaduct.bmp");
  train = load("snes_train.bmp");
  logo = load("snes_logo.bmp");
}

static int canContinue(void) {
  if (autosaveLoadFailed) return 0;
#ifdef WEB_BUILD
  return 1;
#else
  FILE* autosave = fopen(getAutosavePath(), "rb");
  if (!autosave) return 0;
  fclose(autosave);
  return 1;
#endif
}

static void setup(int input) {
  if (!sky || nightMode != (appSettings.bootScreenNight != 0)) init();
  frame = 0;
  skyStartedAt = std::chrono::steady_clock::now();
  continueAvailable = canContinue();
  selected = continueAvailable ? 0 : 1;
}
static void fullRedraw(void) {}

static void drawWrapped(const GfxImage* image, int offset, int y) {
  const int width = gfxImageWidth(image);
  if (!width) return;
  offset %= width;
  int x = 0;
  while (x < 256) {
    int part = width - offset;
    if (part > 256 - x) part = 256 - x;
    gfxImageDrawCrop(image, offset, 0, part, gfxImageHeight(image), x, y);
    x += part;
    offset = 0;
  }
}

static int scrollOffset(const GfxImage* image, int framesPerPixel) {
  const int width = gfxImageWidth(image);
  return width ? (int)((frame / framesPerPixel) % width) : 0;
}

static void drawNightSky(void) {
  // Keep the sky's timing consistent on both 30 Hz and 60 Hz platforms.
  const uint64_t skyFrame = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - skyStartedAt).count() * 60 / 1000;
  const int slices = std::max(1, mainLoopGetTargetFps() / 10);
  const int rows = (TitleNightSky::height + slices - 1) / slices;
  if (nightSky->render(skyFrame, rows)) gfxImageUpdateARGB(sky, nightSky->pixels);
  gfxImageDrawCrop(sky, 0, 0, 256, 224, 0, 0);
  const int cycleFrame = (int)(skyFrame % 14400);
  struct ShootingStar { int start, x, y, dx, dy; };
  static const ShootingStar stars[] = {
    {420,32,20,76,32}, {1980,220,16,-82,38}, {3840,46,38,94,30},
    {6120,198,24,-80,33}, {8160,24,48,100,26}, {10980,228,10,-88,44},
    {13260,72,14,104,39}
  };
  // A mirrored second pass adds seven events without changing their quiet trails.
  for (int pass = 0; pass < 2; ++pass) for (auto star : stars) {
    if (pass) { star.start = (star.start + 1080) % 14400; star.x = 256 - star.x; star.dx = -star.dx; }
    const int age = cycleFrame - star.start;
    if (age < 0 || age >= 54) continue;
    const double fade = fmin(1.0, fmin((age + 1) / 7.0, (54 - age) / 16.0));
    for (int tail = 22; tail >= 0; --tail) {
      const double progress = (age - tail * .55) / 54.0;
      if (progress < 0) continue;
      const int color = (int)floor((1 - tail / 24.0) * 7 * fade + .5);
      gfxImageDrawCrop(meteor, color, 0, 1, 1,
        (int)floor(star.x + star.dx * progress + .5),
        (int)floor(star.y + star.dy * progress + .5));
    }
  }
}

static void drawMenu(void) {
  const char* items[] = {"CONTINUE", "OPEN", "NEW"};
  for (int i = 0; i < 3; i++) {
    const int textX = (32 - (int)strlen(items[i])) / 2;
    gfxSetFgColor(i == 0 && !continueAvailable ? 0x777777 :
      i == selected ? (nightMode ? 0xd8d4ad : 0xffd665) : (nightMode ? 0xb6c4d8 : 0xf2e6c7));
    gfxTitlePrint(textX - 2, 6 + i, i == selected && ((frame / 20) & 1) == 0 ? ">" : " ");
    gfxTitlePrint(textX, 6 + i, items[i]);
  }
}

// screenDraw() skips its normal gfxPrint-based message banner while the
// title screen is active (it renders through gfxTitle* calls instead), so an
// error set via screenMessage() - e.g. picking an invalid file through OPEN,
// which returns here - would otherwise vanish silently. Show it here too.
// The autosave-specific case takes priority: it existed but could not be
// parsed, CONTINUE was disabled above (see canContinue()), and the fallback
// demo project is loaded instead of the user's real work - too important to
// let a later, shorter-lived message push off screen.
static void drawTitleMessage(void) {
  if (autosaveLoadFailed) {
    const char* lines[] = {"AUTOSAVE FAILED TO LOAD", "DEMO PROJECT OPENED INSTEAD",
      autosaveKept > 0 ? "KEPT AS AUTOSAVE.CCT.UNREADABLE" : "AUTOSAVE PAUSED TO PROTECT IT"};
    gfxSetFgColor(appSettings.colorScheme.warning);
    for (int i = 0; i < (autosaveKept ? 3 : 2); i++) {
      const int textX = (32 - (int)strlen(lines[i])) / 2;
      gfxTitlePrint(textX, 10 + i, lines[i]);
    }
    return;
  }
  const char* message = screenGetActiveMessage();
  if (!message || !message[0]) return;
  gfxSetFgColor(appSettings.colorScheme.warning);
  int textX = (32 - (int)strlen(message)) / 2;
  if (textX < 0) textX = 0;
  gfxTitlePrint(textX, 10, message);
}

static void draw(void) {
  gfxTitleBegin();
  if (nightMode) {
    drawNightSky();
    drawWrapped(mountains, scrollOffset(mountains, SKY_SCROLL_FRAMES), 0);
  } else drawWrapped(sky, scrollOffset(sky, SKY_SCROLL_FRAMES), 0);
  // The scenery around the viaduct remains a separate, intermediate plane.
  drawWrapped(scene, scrollOffset(scene, SCENE_SCROLL_FRAMES), 112);
  int vibration = frame % 173 < 6 ? (frame & 1 ? -1 : 1) : 0;
  if (train) {
    const int trainWidth = gfxImageWidth(train);
    const int trainX = (int)((8 + frame / TRAIN_SCROLL_FRAMES + trainWidth) % (256 + trainWidth)) - trainWidth;
    gfxImageDrawCrop(train, 0, 0, trainWidth, gfxImageHeight(train), trainX, 133 + vibration);
  }
  drawWrapped(viaduct, scrollOffset(viaduct, VIADUCT_SCROLL_FRAMES), 152);
  drawWrapped(foreground, scrollOffset(foreground, FOREGROUND_SCROLL_FRAMES), 136);
  if (logo) gfxImageDrawCrop(logo, 0, 0, gfxImageWidth(logo), gfxImageHeight(logo), 52, 24);
  drawMenu();
  drawTitleMessage();
  if (frame < 30) gfxTitleFadeBlack((uint8_t)(255 - frame * 255 / 30));
  gfxTitlePresent();
  frame++;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (!isKeyDown) return 1;
  if (keys == keyUp && selected > 0) { selected--; return 1; }
  if (keys == keyDown && selected < 2) { selected++; return 1; }
  // Start is a second confirm key on the title screen only.
  if (keys != keyEdit && keys != keyPlay) return 1;
  gfxTitleEnd();
  unload();
  if (selected == 0 && continueAvailable) { screenSetup(&screenSong, 0); }
  else if (selected == 1) {
#ifdef WEB_BUILD
    projectOpenFromScreenAtPath(&screenTitle, "/projects");
#else
    projectOpenFromScreen(&screenTitle);
#endif
  }
  else { projectCreateNewFromScreen(&screenSong); }
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
