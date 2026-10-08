#include "screens.h"
#include "screen_layout.h"
#include "utils.h"
#include "project_utils.h"
#include "screen_instrument.h"
#include "common.h"
#include "corelib_gfx.h"
#include "chipnomad_lib.h"
#include "four_op_patch.h"
#include "opl_patch.h"
#include "opll_presets.h"
#include <algorithm>
#include <cstring>
#include <functional>
#include <vector>

// Operator-level editor for the Yamaha style FM instruments: Genesis FM and
// Arcade FM (4 operators), OPL2 / OPL3 (2 or 4 operators) and OPLL / VRC7
// (custom 2 operator tone). Every parameter is described by a Field that reads
// and writes the instrument in place, so the screen is a generic table.

namespace {

struct Field {
  const char* tag;                       // Short label printed before the value, may be empty.
  std::function<int()> get;
  std::function<void(int)> set;
  int min, max;
  const char* const* names;              // Optional value names indexed by value.
};
struct Row {
  const char* label;
  std::vector<Field> cells;
};

std::vector<Row> rows;
std::vector<const char*> header;         // Operator names, one per column.
ScreenData data;

Instrument* cur() { return &chipnomadState->project.instruments[cInstrument]; }

const char* const panNames[] = {"?", "L", "R", "LR"};
const char* const offOn[] = {"off", "on"};
const char* const lfoWaves[] = {"saw", "sq", "tri", "noi"};
const char* const topologyNames[] = {"2op", "4op", "dual"};

Field plain(const char* tag, uint8_t& v, int min, int max, const char* const* names = nullptr) {
  uint8_t* p = &v;
  return {tag, [p] { return int(*p); }, [p](int x) { *p = uint8_t(x); }, min, max, names};
}

// A bit field inside a byte. The optional hook runs after every write.
Field bits(const char* tag, uint8_t& v, int shift, int width, std::function<void()> after = nullptr,
           const char* const* names = nullptr) {
  uint8_t* p = &v;
  const int mask = (1 << width) - 1;
  return {tag, [=] { return (*p >> shift) & mask; },
          [=](int x) { *p = uint8_t((*p & ~(mask << shift)) | ((x & mask) << shift)); if (after) after(); },
          0, mask, names};
}

void markCustomOpll() {
  InstrumentOPLL& t = cur()->chip.opll;
  t.program = 0;
  strncpy(t.presetName, "Custom", sizeof(t.presetName) - 1);
}

bool isOpllType() { return isOPLL(cur()->type); }

void buildFourOp() {
  InstrumentFourOp& p = cur()->chip.fourOp;
  const bool genesis = cur()->type == InstrumentType::GenesisFM;
  header = {"S1", "S2", "S3", "S4"};
  rows.push_back({"Voice", {plain("ALG", p.algorithm, 0, 7), plain("FB", p.feedback, 0, 7), plain("PAN", p.pan, 1, 3, panNames)}});
  rows.push_back({"Sens", {plain("AMS", p.amplitudeSensitivity, 0, 3), plain("PMS", p.pitchSensitivity, 0, 7), plain("MSK", p.operatorMask, 0, 15)}});
  if (genesis) {
    rows.push_back({"LFO", {plain("LFO", p.lfoEnabled, 0, 1, offOn), plain("RT", p.lfoRate, 0, 7)}});
  } else {
    rows.push_back({"LFO", {plain("LFO", p.lfoEnabled, 0, 1, offOn), plain("RT", p.lfoRate, 0, 255), plain("WAV", p.lfoWave, 0, 3, lfoWaves)}});
    rows.push_back({"LFO depth", {plain("AMD", p.amplitudeDepth, 0, 127), plain("PMD", p.pitchDepth, 0, 127)}});
  }
  struct Column { const char* label; uint8_t FourOpOperator::* member; int max; };
  std::vector<Column> columns = {
    {"MUL", &FourOpOperator::multiplier, 15}, {"DT", &FourOpOperator::detune, 7},
    {"TL", &FourOpOperator::level, 127}, {"KS", &FourOpOperator::keyScale, 3},
    {"AR", &FourOpOperator::attack, 31}, {"D1R", &FourOpOperator::decay, 31},
    {"D2R", &FourOpOperator::sustainRate, 31}, {"RR", &FourOpOperator::release, 15},
    {"D1L", &FourOpOperator::sustainLevel, 15},
  };
  if (genesis) columns.push_back({"SSG", &FourOpOperator::ssg, 15});
  else { columns.push_back({"DT2", &FourOpOperator::detune2, 3}); columns.push_back({"AM", &FourOpOperator::amplitudeMod, 1}); }
  for (const Column& c : columns) {
    Row row{c.label, {}};
    for (int op = 0; op < 4; ++op) row.cells.push_back(plain("", p.operators[op].*(c.member), 0, c.max));
    rows.push_back(row);
  }
}

void buildOpl() {
  InstrumentOPL& p = cur()->chip.opl;
  const bool opl3 = cur()->type == InstrumentType::OPL3;
  const int ops = p.topology == OPLTopology::twoOperator ? 2 : 4;
  header = ops == 2 ? std::vector<const char*>{"MOD", "CAR"} : std::vector<const char*>{"M1", "C1", "M2", "C2"};
  if (opl3) {
    uint8_t* topology = reinterpret_cast<uint8_t*>(&p.topology);
    rows.push_back({"Mode", {plain("TOP", *topology, 0, 2, topologyNames)}});
  }
  for (int voice = 0; voice < (ops == 2 ? 1 : 2); ++voice)
    rows.push_back({voice ? "Voice 2" : "Voice", {plain("FB", p.feedback[voice], 0, 7), plain("CON", p.connection[voice], 0, 1),
                    plain("PAN", p.pan[voice], 1, 3, panNames)}});
  rows.push_back({"Deep", {plain("VIB", p.deepVibrato, 0, 1, offOn), plain("TRM", p.deepTremolo, 0, 1, offOn)}});
  struct Column { const char* label; uint8_t OPLOperator::* member; int max; };
  const Column columns[] = {
    {"MUL", &OPLOperator::multiplier, 15}, {"TL", &OPLOperator::level, 63},
    {"AR", &OPLOperator::attack, 15}, {"DR", &OPLOperator::decay, 15},
    {"SL", &OPLOperator::sustain, 15}, {"RR", &OPLOperator::release, 15},
    {"WAV", &OPLOperator::waveform, opl3 ? 7 : 3}, {"KSL", &OPLOperator::keyScale, 3},
    {"VIB", &OPLOperator::vibrato, 1}, {"TRM", &OPLOperator::tremolo, 1},
    {"SUS", &OPLOperator::sustained, 1}, {"KSR", &OPLOperator::rateScale, 1},
  };
  for (const Column& c : columns) {
    Row row{c.label, {}};
    for (int op = 0; op < ops; ++op) row.cells.push_back(plain("", p.operators[op].*(c.member), 0, c.max));
    rows.push_back(row);
  }
}

// OPLL register layout (YM2413 custom instrument, registers 0..7):
// 0/1 modulator/carrier AM VIB EG KSR MULT; 2 KSL(mod) TL(mod); 3 KSL(car) DC DM FB;
// 4/5 AR DR; 6/7 SL RR.
void buildOpll() {
  uint8_t* r = cur()->chip.opll.patch;
  header = {"MOD", "CAR"};
  auto hook = markCustomOpll;
  rows.push_back({"Feedback", {bits("FB", r[3], 0, 3, hook)}});
  auto both = [&](const char* label, int regMod, int regCar, int shift, int width) {
    rows.push_back({label, {bits("", r[regMod], shift, width, hook), bits("", r[regCar], shift, width, hook)}});
  };
  both("MUL", 0, 1, 0, 4);
  rows.push_back({"TL", {bits("", r[2], 0, 6, hook)}});
  rows.push_back({"KSL", {bits("", r[2], 6, 2, hook), bits("", r[3], 6, 2, hook)}});
  both("AR", 4, 5, 4, 4);
  both("DR", 4, 5, 0, 4);
  both("SL", 6, 7, 4, 4);
  both("RR", 6, 7, 0, 4);
  rows.push_back({"Wave", {bits("", r[3], 3, 1, hook, offOn), bits("", r[3], 4, 1, hook, offOn)}});
  both("AM", 0, 1, 7, 1);
  both("VIB", 0, 1, 6, 1);
  both("EG", 0, 1, 5, 1);
  both("KSR", 0, 1, 4, 1);
}

void rebuild() {
  rows.clear();
  header.clear();
  InstrumentType type = cur()->type;
  if (isFourOp(type)) buildFourOp();
  else if (isOPL(type)) buildOpl();
  else if (isOpllType()) buildOpll();
  data.rows = std::max<int>(1, rows.size());
  if (data.cursorRow >= data.rows) data.cursorRow = data.rows - 1;
}

int columns(int row) { return row < int(rows.size()) ? std::max<int>(1, rows[row].cells.size()) : 1; }
int cellX(int col) { return 8 + col * 8; }
int valueWidth(const Field& f) {
  if (!f.names) return f.max > 15 ? 3 : 2;
  int w = 1;
  for (int i = f.min; i <= f.max; ++i) w = std::max<int>(w, strlen(f.names[i]));
  return w;
}
int valueX(const Field& f, int col) { return cellX(col) + (f.tag[0] ? 4 : 0); }
int rowY(int row) { return 3 + row - data.topRow; }
bool visible(int row) { return row >= data.topRow && row < data.topRow + screenVisibleRows(); }

void drawStatic() {
  const ColorScheme cs = appSettings.colorScheme;
  gfxSetFgColor(cs.textTitles);
  gfxPrintf(0, 0, "FM EDIT %02X", cInstrument);
  gfxSetFgColor(cs.textInfo);
  gfxPrint(12, 0, instrumentTypeName(cur()->type));
  gfxPrint(0, 1, cur()->name);
  for (size_t i = 0; i < header.size(); ++i) gfxPrint(cellX(i), 2, header[i]);
}

void drawRowHeader(int row, CellState state) {
  if (!visible(row) || row >= int(rows.size())) return;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textDefault : appSettings.colorScheme.textInfo);
  gfxClearRect(0, rowY(row), 8, 1);
  gfxPrint(0, rowY(row), rows[row].label);
}

void drawField(int col, int row, CellState state) {
  if (!visible(row) || row >= int(rows.size()) || col >= int(rows[row].cells.size())) return;
  const Field& f = rows[row].cells[col];
  int y = rowY(row);
  gfxClearRect(cellX(col), y, 8, 1);
  if (f.tag[0]) { gfxSetFgColor(appSettings.colorScheme.textInfo); gfxPrint(cellX(col), y, f.tag); }
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  int v = f.get();
  if (f.names) gfxPrint(valueX(f, col), y, f.names[v]);
  else gfxPrintf(valueX(f, col), y, f.max > 15 ? "%02X" : "%X", v);
}

void drawCursor(int col, int row) {
  if (!visible(row) || row >= int(rows.size()) || col >= int(rows[row].cells.size())) return;
  const Field& f = rows[row].cells[col];
  gfxCursor(valueX(f, col), rowY(row), f.names ? valueWidth(f) : (f.max > 15 ? 2 : 1));
}

void drawColHeader(int, CellState) {}

int onEdit(int col, int row, CellEditAction action) {
  if (row >= int(rows.size()) || col >= int(rows[row].cells.size())) return 0;
  const Field& f = rows[row].cells[col];
  action = convertMultiAction(action);
  const int big = f.max >= 32 ? 8 : f.max >= 8 ? 4 : 1;
  int v = f.get();
  if (action == CellEditAction::clear) v = f.min;
  else if (action == CellEditAction::increase) ++v;
  else if (action == CellEditAction::decrease) --v;
  else if (action == CellEditAction::increaseBig) v += big;
  else if (action == CellEditAction::decreaseBig) v -= big;
  else return 0;
  f.set(std::clamp(v, f.min, f.max));
  projectModified = 1;
  // Changing the OPL3 mode adds or removes operator columns, so rebuild the table.
  size_t before = rows.size();
  size_t cells = rows[row].cells.size();
  rebuild();
  if (rows.size() != before || (row < int(rows.size()) && rows[row].cells.size() != cells) || isOPL(cur()->type)) {
    currentScreen->fullRedraw();
  }
  return 1;
}

void setup(int) {
  data.cursorRow = 0;
  data.cursorCol = 0;
  data.topRow = 0;
  rebuild();
}

void fullRedraw() {
  rebuild();
  screenFullRedraw(&data);
}

void draw() {}

int onInput(int isKeyDown, int keys, int tapCount) {
  if (keys == 0) chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
  if (keys == keyOpt || keys == (keyLeft | keyShift)) {
    screenSetup(&screenInstrument, cInstrument);
    return 1;
  }
  return screenInput(&data, isKeyDown, keys, tapCount);
}

ScreenPlaybackLevel getPlaybackLevel() { return ScreenPlaybackLevel::phrase; }

struct Initializer {
  Initializer() {
    data = {};
    data.rows = 1;
    data.selectMode = -1;
    data.playbackLevel = ScreenPlaybackLevel::none;
    data.getColumnCount = columns;
    data.drawStatic = drawStatic;
    data.drawCursor = drawCursor;
    data.drawRowHeader = drawRowHeader;
    data.drawColHeader = drawColHeader;
    data.drawField = drawField;
    data.onEdit = onEdit;
  }
} initializer;

}  // namespace

const AppScreen screenFMEdit = {
  .init = nullptr,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = getPlaybackLevel,
};

bool fmEditSupported(InstrumentType type) { return isFourOp(type) || isOPL(type) || isOPLL(type); }
