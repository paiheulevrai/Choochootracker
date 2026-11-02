#pragma once

#include "common.h"
#include <stddef.h>
#include <stdint.h>

constexpr int BACKGROUND_LOW_WIDTH = 80;
constexpr int BACKGROUND_LOW_HEIGHT = 60;
constexpr int BACKGROUND_MAX_WIDTH = 640;
constexpr int BACKGROUND_MAX_HEIGHT = 480;

struct BackgroundAudioFeatures {
  float rms = 0;
  float low = 0;
  float mid = 0;
  float high = 0;
  float transient = 0;
};

void backgroundInit(void);
void backgroundCleanup(void);
bool backgroundApplySettings(void);
bool backgroundTick(const float* samples, int sampleCount);
bool backgroundIsAnimated(void);
bool backgroundNeedsAudio(void);
bool backgroundImageLoadFailed(void);

bool backgroundPixelsAreMonochrome(const uint32_t* pixels, size_t count);
uint32_t backgroundTintPixel(uint32_t pixel, int baseColor, int intensity);
BackgroundAudioFeatures backgroundAnalyzeSamples(const float* samples, int count,
  const BackgroundAudioFeatures& previous);
void backgroundRenderFrame(BackgroundMode mode, uint32_t* pixels, int width, int height,
  int baseColor, int accentColor, int intensity, int speed, int reaction, float phase,
  const BackgroundAudioFeatures& audio);
