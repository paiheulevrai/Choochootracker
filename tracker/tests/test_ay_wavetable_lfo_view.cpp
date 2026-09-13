#include "doctest.h"
#include "common.h"
#include "waveform_display.h"

#include <cstring>
#include <filesystem>
#include <fstream>

TEST_SUITE("AY wavetable LFO view") {

TEST_CASE("linear LFO preview maps AY wavetable endpoints and zero") {
  CHECK(ayWavetableLfoPreviewLevel(0) == 0);
  CHECK(ayWavetableLfoPreviewLevel(15) == 255);
  CHECK(ayWavetableLfoPreviewLevel(7) < 128);
  CHECK(ayWavetableLfoPreviewLevel(8) > 128);
}

TEST_CASE("linear LFO preview draws a zero guide") {
  uint8_t pixels[32 * 16] = {};
  uint8_t wavetable[32] = {};
  Bitmap bitmap = {1, 1, 32, 16, pixels, nullptr};

  renderAYWavetableLfoPreview(&bitmap, wavetable);
  for (int x = 0; x < bitmap.widthPixels; ++x) CHECK(pixels[(bitmap.heightPixels / 2) * bitmap.widthPixels + x] == 64);
  for (int x = 0; x < bitmap.widthPixels; ++x) CHECK(pixels[(bitmap.heightPixels - 1) * bitmap.widthPixels + x] == 255);
}

TEST_CASE("AY wavetable LFO view setting survives save and load") {
  namespace fs = std::filesystem;
  const fs::path originalPath = fs::current_path();
  const fs::path testPath = fs::temp_directory_path() / "choochootracker-lfo-view-test";
  fs::remove_all(testPath);
  fs::create_directories(testPath);
  fs::current_path(testPath);

  initDefaultAppSettings();
  CHECK(appSettings.ayWavetableLfoView == 0);
  appSettings.ayWavetableLfoView = 1;
  REQUIRE(settingsSave() == 0);
  initDefaultAppSettings();
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.ayWavetableLfoView == 1);

  std::ofstream("settings.txt") << "screenWidth: 640\n";
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.ayWavetableLfoView == 0);

  fs::current_path(originalPath);
  fs::remove_all(testPath);
}

}
