#include "doctest.h"
#include "screens.h"
#include "corelib_gfx.h"
#include <initializer_list>

TEST_CASE("Modulation draw and touch rows agree in both waveform modes") {
  const uint8_t saved = appSettings.persistentWaveform;
  const int expected[16] = {2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18};
  for (int enabled = 0; enabled < 2; ++enabled) {
    appSettings.persistentWaveform = enabled;
    CHECK(screenModulationRowY(-1) == -1);
    CHECK(screenModulationRowY(16) == -1);
    for (int row = 0; row < 16; ++row) {
      CHECK(screenModulationRowY(row) == expected[row]);
      CHECK(screenModulationRowAt(expected[row]) == row);
    }
    for (int y = 0; y < 20; ++y) {
      bool field = false;
      for (int row = 0; row < 16; ++row) field |= expected[row] == y;
      if (!field) CHECK(screenModulationRowAt(y) == -1);
    }
  }
  appSettings.persistentWaveform = saved;
}

TEST_CASE("Scope reserves rows only on SONG") {
  const AppScreen* saved = currentScreen;
  uint8_t oldEnabled = appSettings.persistentWaveform;
  for (int enabled : {0, 1}) {
    appSettings.persistentWaveform = enabled;
    currentScreen = &screenSong;
    CHECK(screenScopeRows(currentScreen) == (enabled ? 2 : 0));
    CHECK(screenVisibleRows() == (enabled ? 14 : 16));
    for (const AppScreen* screen : {&screenChain, &screenPhrase, &screenTable,
         &screenInstrument, &screenInstrumentPool, &screenMixer, &screenGroove,
         &screenModulation, &screenSettings, &screenProject, &screenAYWavetable,
         &screenTitle, &screenInsertFX, &screenSIDEdit, &screenFMEdit}) {
      currentScreen = screen;
      CHECK(screenScopeRows(screen) == 0);
      CHECK(screenVisibleRows() == 16);
    }
  }
  currentScreen = saved;
  appSettings.persistentWaveform = oldEnabled;
  gfxSetContentRowOffset(2);
  { ScreenOverlayCoordinates overlay; CHECK(gfxGetContentRowOffset() == 0); }
  CHECK(gfxGetContentRowOffset() == 2);
  gfxSetContentRowOffset(0);
}
