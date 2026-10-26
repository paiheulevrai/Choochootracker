#include "doctest.h"
#include "common.h"

#include <cstdio>

namespace {

int loadThemeText(const char* text) {
  const char* path = "test_theme_colors.cth";
  FILE* file = fopen(path, "w");
  REQUIRE(file);
  fputs(text, file);
  fclose(file);
  const int result = loadTheme(path);
  std::remove(path);
  return result;
}

}  // namespace

TEST_CASE("Themes without the newer colours get them filled in") {
  const AppSettings saved = appSettings;
  // The built-in text colours on another background, with a names colour
  // saved under the earlier key, which is ignored.
  REQUIRE(loadThemeText("colorBackground: 0x101010\ncolorTextInfo: 0x52799b\ncolorTextDefault: 0xc9bfa5\n"
                        "colorNamedValues: 0xc9bfa5\ncolorTextValue: 0xffd665\ncolorCursor: 0x65d8ff\n") == 0);
  CHECK(appSettings.colorScheme.textNames == 0x9fcf8f);
  CHECK(appSettings.colorScheme.textLabel == 0x7d8fa6);
  CHECK(appSettings.colorScheme.valueBar == 0x64d6ff);

  // Another palette: labels dimmed towards the background, names as the
  // default text, bars in the cursor colour.
  REQUIRE(loadThemeText("colorBackground: 0x000000\ncolorTextDefault: 0xc0c0c0\ncolorTextValue: 0xffffff\n"
                        "colorCursor: 0x00ff00\n") == 0);
  CHECK(appSettings.colorScheme.textLabel == 0x909090);
  CHECK(appSettings.colorScheme.textNames == 0xc0c0c0);
  CHECK(appSettings.colorScheme.valueBar == 0x00ff00);

  // Colours a theme sets are kept.
  REQUIRE(loadThemeText("colorTextDefault: 0xc9bfa5\ncolorTextValue: 0xffd665\ncolorTextLabels: 0x123456\n"
                        "colorTextNames: 0x654321\ncolorValueBars: 0xabcdef\n") == 0);
  CHECK(appSettings.colorScheme.textLabel == 0x123456);
  CHECK(appSettings.colorScheme.textNames == 0x654321);
  CHECK(appSettings.colorScheme.valueBar == 0xabcdef);
  appSettings = saved;
}
