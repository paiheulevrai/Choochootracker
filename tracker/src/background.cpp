#include "background.h"
#include "corelib/corelib_gfx.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

static uint32_t pixels[BACKGROUND_MAX_WIDTH * BACKGROUND_MAX_HEIGHT];
static BackgroundAudioFeatures audioFeatures;
static float animationPhase;
static int accumulatedMicros;
static bool imageLoadFailed;
static std::chrono::steady_clock::time_point lastTick;

static int renderWidth(void) { return BACKGROUND_LOW_WIDTH << (int)appSettings.backgroundResolution; }
static int renderHeight(void) { return BACKGROUND_LOW_HEIGHT << (int)appSettings.backgroundResolution; }

static int clampByte(int value) { return value < 0 ? 0 : value > 255 ? 255 : value; }
static float clampUnit(float value) { return value < 0 ? 0 : value > 1 ? 1 : value; }

static int visibleTintColor(int base) {
  int r = base >> 16 & 255, g = base >> 8 & 255, b = base & 255;
  int brightest = std::max(r, std::max(g, b));
  if (brightest >= 96) return base;
  int lift = 96 - brightest;
  return clampByte(r + lift) << 16 | clampByte(g + lift) << 8 | clampByte(b + lift);
}

static uint32_t mixColor(int base, int target, int amount) {
  amount = std::max(0, std::min(100, amount));
  int r = ((base >> 16 & 255) * (100 - amount) + (target >> 16 & 255) * amount) / 100;
  int g = ((base >> 8 & 255) * (100 - amount) + (target >> 8 & 255) * amount) / 100;
  int b = ((base & 255) * (100 - amount) + (target & 255) * amount) / 100;
  return 0xff000000u | (uint32_t)(r << 16 | g << 8 | b);
}

bool backgroundPixelsAreMonochrome(const uint32_t* source, size_t count) {
  if (!source || !count) return false;
  for (size_t i = 0; i < count; ++i) {
    int r = source[i] >> 16 & 255, g = source[i] >> 8 & 255, b = source[i] & 255;
    if (std::max(r, std::max(g, b)) - std::min(r, std::min(g, b)) > 3) return false;
  }
  return true;
}

uint32_t backgroundTintPixel(uint32_t pixel, int baseColor, int intensity) {
  int r = pixel >> 16 & 255, g = pixel >> 8 & 255, b = pixel & 255;
  int luminance = (r * 54 + g * 183 + b * 19) >> 8;
  int tintColor = visibleTintColor(baseColor);
  int tinted = ((tintColor >> 16 & 255) * luminance / 255) << 16 |
               ((tintColor >> 8 & 255) * luminance / 255) << 8 |
               (tintColor & 255) * luminance / 255;
  return mixColor(baseColor, tinted, intensity);
}

BackgroundAudioFeatures backgroundAnalyzeSamples(const float* samples, int count,
    const BackgroundAudioFeatures& previous) {
  BackgroundAudioFeatures result = previous;
  if (!samples || count <= 0) {
    result.rms *= .85f; result.low *= .85f; result.mid *= .85f;
    result.high *= .85f; result.transient *= .6f;
    return result;
  }
  float lowPass = 0, highPass = 0, rms = 0, low = 0, mid = 0, high = 0;
  for (int i = 0; i < count; ++i) {
    float sample = std::max(-2.0f, std::min(2.0f, samples[i]));
    lowPass += .035f * (sample - lowPass);
    highPass += .28f * (sample - highPass);
    float middle = highPass - lowPass, top = sample - highPass;
    rms += sample * sample; low += lowPass * lowPass;
    mid += middle * middle; high += top * top;
  }
  float scale = 2.5f / std::sqrt((float)count);
  float freshRms = clampUnit(std::sqrt(rms) * scale);
  float freshLow = clampUnit(std::sqrt(low) * scale * 1.7f);
  float freshMid = clampUnit(std::sqrt(mid) * scale * 1.3f);
  float freshHigh = clampUnit(std::sqrt(high) * scale);
  result.transient = std::max(previous.transient * .55f,
    clampUnit((freshRms - previous.rms * 1.18f) * 5.0f));
  result.rms = previous.rms * .65f + freshRms * .35f;
  result.low = previous.low * .7f + freshLow * .3f;
  result.mid = previous.mid * .7f + freshMid * .3f;
  result.high = previous.high * .7f + freshHigh * .3f;
  return result;
}

void backgroundRenderFrame(BackgroundMode mode, uint32_t* destination, int width, int height,
    int baseColor, int accentColor, int intensity, int speed, int reaction, float phase,
    const BackgroundAudioFeatures& audio) {
  if (!destination || width <= 0 || height <= 0) return;
  float react = std::max(0, std::min(100, reaction)) / 100.0f;
  float motion = std::max(0, std::min(100, speed)) / 40.0f;
  float ballX[3], ballY[3];
  if (mode == BackgroundMode::metaballs) for (int ball = 0; ball < 3; ++ball) {
    ballX[ball] = .5f + std::sin(phase * motion * (.45f + ball * .13f) + ball * 2.1f) * .32f;
    ballY[ball] = .5f + std::cos(phase * motion * (.38f + ball * .11f) + ball * 1.7f) * .32f;
  }
  for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
    float value = 1.0f;
    if (mode == BackgroundMode::plasma) {
      float px = (float)x / width, py = (float)y / height;
      value = .62f + .19f * std::sin(px * 18 + phase * motion + audio.low * react * 3) +
        .13f * std::sin(py * 23 - phase * motion * .7f + audio.mid * react * 4) +
        .09f * std::sin((px + py) * 31 + phase * .4f + audio.high * react * 5);
    } else if (mode == BackgroundMode::stars) {
      int layer = 1 + y % 3;
      int travel = (int)(phase * motion * (6 + layer * 9) + audio.transient * react * 20);
      int sx = (x + travel) % width;
      uint32_t n = (uint32_t)(sx * 1973 + y * 9277 + 0x68bc21ebu);
      n ^= n << 13; n ^= n >> 17; n ^= n << 5;
      float depth = layer / 3.0f;
      bool star = (n & 1023) < 7;
      value = star ? 1.25f + depth * .35f + audio.transient * react : .72f;
    } else if (mode == BackgroundMode::waves) {
      float center = height * .5f;
      float wave = std::sin((x * .16f) + phase * motion * 2) * height * .08f;
      wave *= 1 + audio.rms * react * 3;
      float distance = std::fabs(y - center - wave);
      value = distance < 1.5f ? 1.45f : distance < 4 ? .95f : .72f;
    } else if (mode == BackgroundMode::tunnel) {
      float dx = (x - width * (.5f + audio.low * react * .08f)) / height;
      float dy = (y - height * (.5f + audio.mid * react * .08f)) / height;
      float radius = std::sqrt(dx * dx + dy * dy);
      value = .72f + .28f * (.5f + .5f * std::sin(2.2f / (radius + .055f) - phase * motion * 4));
    } else if (mode == BackgroundMode::metaballs) {
      float nx = (float)x / width, ny = (float)y / height;
      float field = 0;
      for (int ball = 0; ball < 3; ++ball) {
        float dx = nx - ballX[ball], dy = ny - ballY[ball];
        field += (.012f + audio.low * react * .006f) / (dx * dx + dy * dy + .008f);
      }
      value = field > 1.15f ? 1.28f : field > .65f ? 1.0f : .7f + field * .18f;
    } else if (mode == BackgroundMode::spectrum) {
      int bars = 8, barWidth = std::max(1, width / bars), bar = std::min(bars - 1, x / barWidth);
      float band = bar < 3 ? audio.low : bar < 6 ? audio.mid : audio.high;
      float idle = .16f + .08f * std::sin(bar * 2.3f + phase * motion);
      float level = idle + band * react * .72f;
      int top = height - (int)(level * height);
      bool lit = y >= top && x % barWidth < std::max(1, barWidth * 3 / 4);
      value = lit ? 1.1f + (height - y) / (float)height * .35f : .7f;
    } else if (mode == BackgroundMode::rain) {
      int column = x / 4;
      uint32_t seed = (uint32_t)(column * 2654435761u + 0x9e3779b9u);
      int speedup = (int)(audio.transient * react * 20);
      int head = ((int)(phase * motion * (18 + seed % 19)) + speedup + seed % height) % height;
      int distance = (head - y + height) % height;
      bool stroke = (x & 3) != 3 && ((y + (int)(seed >> 8)) & 3) != 3;
      value = stroke && distance < 18 ? 1.35f - distance * .035f : .69f;
    } else if (mode == BackgroundMode::moire) {
      float nx = (x - width * .42f) / height, ny = (y - height * .5f) / height;
      float ax = nx + std::sin(phase * motion * .3f) * .15f;
      float bx = nx - .25f - std::cos(phase * motion * .27f) * .12f;
      float rings = std::sin((ax * ax + ny * ny) * 190 + phase * motion) +
        std::sin((bx * bx + ny * ny) * 190 - phase * motion * 1.2f);
      value = .78f + rings * (.11f + audio.high * react * .06f);
    }
    // Map the effect's brightness range to the theme background/accent gradient.
    int amount = (int)(clampUnit((value - .65f) / .95f) * std::max(0, std::min(100, intensity)));
    destination[y * width + x] = mixColor(baseColor, accentColor, amount);
  }
}

bool backgroundIsAnimated(void) {
  return appSettings.backgroundMode >= BackgroundMode::plasma &&
    appSettings.backgroundMode <= BackgroundMode::moire;
}

bool backgroundNeedsAudio(void) {
  return backgroundIsAnimated() && appSettings.backgroundReaction > 0;
}

bool backgroundImageLoadFailed(void) { return imageLoadFailed; }

bool backgroundApplySettings(void) {
  gfxBackgroundSetBaseColor(appSettings.colorScheme.background);
  imageLoadFailed = false;
  accumulatedMicros = 0;
  lastTick = std::chrono::steady_clock::now();
  if (appSettings.backgroundMode == BackgroundMode::solid) {
    gfxBackgroundSetSolid();
    return true;
  }
  if (appSettings.backgroundMode == BackgroundMode::image) {
    imageLoadFailed = !appSettings.backgroundImagePath[0] ||
      !gfxBackgroundLoadBMP(appSettings.backgroundImagePath,
        (int)appSettings.backgroundLayout, (int)appSettings.backgroundTint,
        appSettings.backgroundIntensity);
    if (imageLoadFailed) gfxBackgroundSetSolid();
    return !imageLoadFailed;
  }
  int width = renderWidth(), height = renderHeight();
  backgroundRenderFrame(appSettings.backgroundMode, pixels, width,
    height, appSettings.colorScheme.background, appSettings.colorScheme.valueBar, appSettings.backgroundIntensity,
    appSettings.backgroundSpeed, appSettings.backgroundReaction, animationPhase, audioFeatures);
  gfxBackgroundSetPixels(pixels, width, height);
  return true;
}

void backgroundInit(void) {
  animationPhase = 0;
  audioFeatures = {};
  backgroundApplySettings();
}

void backgroundCleanup(void) { gfxBackgroundSetSolid(); }

bool backgroundTick(const float* samples, int sampleCount) {
  if (!backgroundIsAnimated()) return false;
  const auto now = std::chrono::steady_clock::now();
  int measured = (int)std::chrono::duration_cast<std::chrono::microseconds>(now - lastTick).count();
  lastTick = now;
  accumulatedMicros += measured > 0 && measured < 250000 ? measured : 33333;
  const int frameMicros = appSettings.backgroundResolution == BackgroundResolution::ultra ? 66666 :
    appSettings.backgroundResolution == BackgroundResolution::high ? 50000 :
    appSettings.backgroundResolution == BackgroundResolution::medium ? 40000 : 33333;
  if (accumulatedMicros < frameMicros) return false;
  float elapsed = accumulatedMicros / 1000000.0f;
  accumulatedMicros %= frameMicros;
  audioFeatures = backgroundAnalyzeSamples(backgroundNeedsAudio() ? samples : nullptr,
    backgroundNeedsAudio() ? sampleCount : 0, audioFeatures);
  animationPhase += elapsed;
  int width = renderWidth(), height = renderHeight();
  backgroundRenderFrame(appSettings.backgroundMode, pixels, width,
    height, appSettings.colorScheme.background, appSettings.colorScheme.valueBar, appSettings.backgroundIntensity,
    appSettings.backgroundSpeed, appSettings.backgroundReaction, animationPhase, audioFeatures);
  if (appSettings.backgroundMode == BackgroundMode::waves) {
    const float reaction = appSettings.backgroundReaction / 100.0f;
    const uint32_t line = mixColor(appSettings.colorScheme.background,
      appSettings.colorScheme.valueBar,
      appSettings.backgroundIntensity);
    int previousY = height / 2;
    for (int x = 0; x < width; ++x) {
      float sample = backgroundNeedsAudio() && samples && sampleCount > 0
        ? samples[x * sampleCount / width] * reaction
        : std::sin(x * .16f + animationPhase * appSettings.backgroundSpeed / 25.0f) * .12f;
      int y = std::max(1, std::min(height - 2,
        height / 2 - (int)(sample * height * .36f)));
      int from = std::min(previousY, y), to = std::max(previousY, y);
      for (int py = from; py <= to; ++py) pixels[py * width + x] = line;
      previousY = y;
    }
  }
  gfxBackgroundSetPixels(pixels, width, height);
  return true;
}
