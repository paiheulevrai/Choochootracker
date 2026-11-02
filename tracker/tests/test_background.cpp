#include "doctest.h"
#include "background.h"
#include <cmath>
#include <vector>

TEST_CASE("legacy settings default to the solid backdrop") {
  AppSettings saved = appSettings;
  initDefaultAppSettings();
  CHECK(appSettings.backgroundMode == BackgroundMode::solid);
  CHECK(appSettings.backgroundIntensity == 25);
  CHECK(appSettings.backgroundSpeed == 50);
  CHECK(appSettings.backgroundReaction == 50);
  CHECK(appSettings.backgroundResolution == BackgroundResolution::low);
  appSettings = saved;
}

TEST_CASE("background monochrome detection tolerates tiny channel differences") {
  const uint32_t gray[] = {0xff202122, 0xff808080, 0xfffdfefd};
  const uint32_t color[] = {0xff203020};
  CHECK(backgroundPixelsAreMonochrome(gray, 3));
  CHECK_FALSE(backgroundPixelsAreMonochrome(color, 1));
}

TEST_CASE("background tint intensity blends from theme background") {
  CHECK((backgroundTintPixel(0xffffffff, 0x204060, 0) & 0xffffff) == 0x204060);
  CHECK((backgroundTintPixel(0xffffffff, 0x204060, 100) & 0xffffff) == 0x204060);
  CHECK((backgroundTintPixel(0xff000000, 0x204060, 100) & 0xffffff) == 0x000000);
  CHECK((backgroundTintPixel(0xffffffff, 0x000000, 100) & 0xffffff) == 0x606060);
}

TEST_CASE("animated backgrounds remain visible over pure black") {
  std::vector<uint32_t> pixels(64);
  backgroundRenderFrame(BackgroundMode::plasma, pixels.data(), 8, 8,
    0x000000, 0x64d6ff, 25, 50, 0, 1.0f, {});
  bool visible = false;
  for (uint32_t pixel : pixels) visible |= (pixel & 0xffffff) != 0;
  CHECK(visible);
}

TEST_CASE("all animated backgrounds blend only the theme background and accent") {
  std::vector<uint32_t> solid(64), full(64), half(64), changedAccent(64);
  for (int mode = (int)BackgroundMode::plasma; mode <= (int)BackgroundMode::moire; ++mode) {
    backgroundRenderFrame((BackgroundMode)mode, solid.data(), 8, 8,
      0x800000, 0x000080, 0, 50, 0, 1.25f, {});
    backgroundRenderFrame((BackgroundMode)mode, full.data(), 8, 8,
      0x800000, 0x000080, 100, 50, 0, 1.25f, {});
    backgroundRenderFrame((BackgroundMode)mode, half.data(), 8, 8,
      0x800000, 0x000080, 50, 50, 0, 1.25f, {});
    backgroundRenderFrame((BackgroundMode)mode, changedAccent.data(), 8, 8,
      0x800000, 0x008000, 100, 50, 0, 1.25f, {});
    CHECK(full != solid);
    CHECK(full != changedAccent);
    for (size_t i = 0; i < full.size(); ++i) {
      CHECK(solid[i] == 0xff800000);
      int red = full[i] >> 16 & 255, blue = full[i] & 255;
      CHECK((full[i] & 0xff00) == 0);
      CHECK(red <= 128);
      CHECK(blue <= 128);
      CHECK(red + blue >= 127);
      CHECK(red + blue <= 128);
      // The blend rounds both its percentage and its output channels down.
      CHECK(std::abs((int)(half[i] & 255) * 2 - blue) <= 3);
      CHECK(changedAccent[i] == (0xff000000u | (red << 16) | (blue << 8)));
    }
  }
}

TEST_CASE("background audio analysis separates low and high activity") {
  float low[256], high[256];
  for (int i = 0; i < 256; ++i) {
    low[i] = .5f * std::sin(i * .04f);
    high[i] = (i & 1) ? .5f : -.5f;
  }
  auto lowFeatures = backgroundAnalyzeSamples(low, 256, {});
  auto highFeatures = backgroundAnalyzeSamples(high, 256, {});
  CHECK(lowFeatures.low > lowFeatures.high);
  CHECK(highFeatures.high > highFeatures.low);
}

TEST_CASE("native background frames are deterministic and distinct") {
  std::vector<uint32_t> a(64), b(64), stars(64);
  BackgroundAudioFeatures audio;
  audio.rms = .2f; audio.low = .3f; audio.mid = .1f; audio.high = .05f; audio.transient = .4f;
  backgroundRenderFrame(BackgroundMode::plasma, a.data(), 8, 8, 0x304050, 0x64d6ff, 25, 50, 50, 1.25f, audio);
  backgroundRenderFrame(BackgroundMode::plasma, b.data(), 8, 8, 0x304050, 0x64d6ff, 25, 50, 50, 1.25f, audio);
  backgroundRenderFrame(BackgroundMode::stars, stars.data(), 8, 8, 0x304050, 0x64d6ff, 25, 50, 50, 1.25f, audio);
  CHECK(a == b);
  CHECK(a != stars);
}

TEST_CASE("all native background effects render distinct deterministic frames") {
  std::vector<uint32_t> reference(64), first(64), second(64);
  BackgroundAudioFeatures audio{.2f, .3f, .1f, .05f, .4f};
  backgroundRenderFrame(BackgroundMode::plasma, reference.data(), 8, 8,
    0x304050, 0x64d6ff, 50, 50, 50, 1.25f, audio);
  for (int value = (int)BackgroundMode::stars; value <= (int)BackgroundMode::moire; ++value) {
    BackgroundMode mode = (BackgroundMode)value;
    backgroundRenderFrame(mode, first.data(), 8, 8, 0x304050, 0x64d6ff, 50, 50, 50, 1.25f, audio);
    backgroundRenderFrame(mode, second.data(), 8, 8, 0x304050, 0x64d6ff, 50, 50, 50, 1.25f, audio);
    CHECK(first == second);
    CHECK(first != reference);
  }
}
