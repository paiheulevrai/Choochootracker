// Standalone screen regression, run from tracker with Linux GCC (WSL works):
// g++ -std=c++17 -O2 -ffunction-sections -fdata-sections -Isrc -Isrc/corelib \
//   -Isrc/screens -I../chipnomad_lib tests/check_achchid_layout.cpp \
//   -Wl,--gc-sections -o build/check_achchid_layout && build/check_achchid_layout
// Include the screen to exercise its actual drawing without the application.
#include "../src/screens/instrument_achchid.cpp"
#include <cassert>
#include <cstdarg>
#include <cstring>

AppSettings appSettings = {};
static ChipNomadState state;
ChipNomadState* chipnomadState = &state;
int cInstrument = 0;
static char cells[20][41];

void gfxSetFgColor(int) {}
void gfxPrint(int x, int y, const char* text) {
  assert(y >= 0 && y < 20);
  for (; *text && x < 40; ++text, ++x) {
    assert(x >= 0);
    cells[y][x] = *text;
  }
}
void gfxPrintf(int x, int y, const char* format, ...) {
  char text[128];
  va_list args;
  va_start(args, format);
  vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  gfxPrint(x, y, text);
}
void gfxCursor(int x, int, int width) { assert(x + width <= 34); }
void instrumentCommonDrawCursor(int, int) { assert(false); }
void instrumentCommonDrawField(int, int, CellState) { assert(false); }
const char* modelCatalogName(InstrumentType, int) { return ""; }

static void checkField(int row, const char* expected) {
  memset(cells, ' ', sizeof(cells));
  for (auto& line : cells) line[40] = '\0';
  drawField(1, row, CellState::focus);
  // appDraw paints track status after screenDraw, starting at column 34.
  gfxPrint(34, row + 4, " 5 ---");
  cells[row + 4][34] = '\0';
  if (!strstr(cells[row + 4], expected)) {
    fprintf(stderr, "Expected %s, displayed [%s]\n", expected, cells[row + 4]);
    assert(false);
  }
  drawCursor(1, row);
}

int main() {
  auto& a = state.project.instruments[0].chip.achchid;
  for (int cutoff = 1050; cutoff <= 1061; ++cutoff) {
    a.cutoff = cutoff;
    char expected[8];
    snprintf(expected, sizeof(expected), "%d", cutoff);
    checkField(3, expected);
  }
  a.cutoff = 20000; checkField(3, "20000");
  a.resonance = 100; checkField(4, "100");
  a.envMod = 100; checkField(5, "100");
  a.decay = 2000; checkField(6, "2000ms");
  a.accent = 100; checkField(7, "100");
  puts("aChChid values and cursors stay clear of track status");
}
