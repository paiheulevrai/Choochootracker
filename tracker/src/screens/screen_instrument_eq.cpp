// EQ bank screen: the response of the three bands as a graph with a dot for
// each band, and the band settings below it. Hold EDIT on a band number and
// use the d-pad to move its dot: left/right for frequency, up/down for gain.
// It is opened from an instrument's EQ field; every instrument with the same
// EQ number shares these settings.
#include "screens.h"
#include "screen_instrument.h"
#include "project_utils.h"
#include "corelib_gfx.h"
#include "chipnomad_lib.h"
#include "synth/instrument_eq.h"
#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {
enum Row { rowEQ, rowBand, rowShape, rowFrequency, rowGain, rowQ, rowCount };

// Character layout. The graph keeps clear of the track list on the right.
constexpr int kGraphX = 3, kGraphY = 2, kGraphColumns = 31, kGraphRows = 11;
constexpr int kAxisY = kGraphY + kGraphRows, kTableY = kAxisY + 1;
constexpr int kValueX[INSTRUMENT_EQ_BANDS] = {6, 15, 24}, kValueWidth = 8;
constexpr float kLowestHz = 20.0f, kDecades = 3.0f;  // 20 Hz .. 20 kHz
// 6 dB per two rows puts the grid lines and their labels on whole rows.
constexpr float kDbPerRow = 3.0f;

enum Layer { layerGrid, layerBand, layerCurve, layerDot1, layerDot2, layerDot3, layerCount };
Bitmap* layers[layerCount];
int drawnBand = -1;
uint8_t bank = 0;
char hintOwnedText[48];

ScreenData data;

bool master() { return bank == PROJECT_MASTER_EQ_BANK; }
InstrumentEQ& eq() { return master() ? chipnomadState->project.masterEQ : chipnomadState->project.eqBanks[bank]; }
int selectedBand() { return data.cursorCol; }
bool hasGain(EQShape shape) {
  return shape == EQShape::lowShelf || shape == EQShape::bell || shape == EQShape::highShelf;
}
bool isCut(EQShape shape) { return shape == EQShape::lowCut || shape == EQShape::highCut; }
float sampleRate() { return chipnomadState->sampleRate > 0 ? (float)chipnomadState->sampleRate : 48000.0f; }

int bandColor(int band) {
  const ColorScheme& cs = appSettings.colorScheme;
  return band == 0 ? cs.warning : band == 1 ? cs.textTitles : cs.playMarkers;
}

int rowY(int row) { return kTableY + row - rowBand; }

///////////////////////////////////////////////////////////////////////////////
// Graph

int graphWidth() { return kGraphColumns * gfxGetCharWidth(); }
int graphHeight() { return kGraphRows * gfxGetCharHeight(); }
float pixelsPerDb() { return gfxGetCharHeight() / kDbPerRow; }
float xForHz(float hz) { return log10f(hz / kLowestHz) / kDecades * (graphWidth() - 1); }
float hzForX(float x) { return kLowestHz * powf(10.0f, kDecades * x / (graphWidth() - 1)); }
float yForDb(float db) { return graphHeight() * 0.5f - db * pixelsPerDb(); }

void plot(Bitmap* b, int x, int y, int alpha) {
  if (x < 0 || y < 0 || x >= b->widthPixels || y >= b->heightPixels) return;
  uint8_t& pixel = b->data[y * b->widthPixels + x];
  if (alpha > pixel) pixel = (uint8_t)std::min(alpha, 255);
}

// A column between two heights, for a filled area under a curve.
void span(Bitmap* b, int x, float y0, float y1, int alpha) {
  if (y0 > y1) std::swap(y0, y1);
  for (int y = (int)ceilf(y0); y <= (int)floorf(y1); ++y) plot(b, x, y, alpha);
}

// A curve two pixels thick, joining each column to the one before it.
void curve(Bitmap* b, const float* ys, int alpha) {
  for (int x = 0; x < b->widthPixels; ++x) {
    float a = ys[x], c = x ? ys[x - 1] : ys[x];
    if (a > c) std::swap(a, c);
    for (int y = (int)floorf(a - 1.0f); y <= (int)ceilf(c + 1.0f); ++y) {
      float distance = y < a ? a - y : y > c ? y - c : 0.0f;
      plot(b, x, y, (int)(alpha * std::max(0.0f, 1.0f - distance * 0.5f)));
    }
  }
}

void dot(Bitmap* b, float cx, float cy, float radius, float ring, int alpha) {
  const int reach = (int)ceilf(radius + ring + 1.0f);
  for (int y = (int)cy - reach; y <= (int)cy + reach; ++y)
    for (int x = (int)cx - reach; x <= (int)cx + reach; ++x) {
      const float d = hypotf(x - cx, y - cy);
      float coverage = std::clamp(radius + 0.5f - d, 0.0f, 1.0f);  // the filled middle
      if (ring > 0.0f) coverage = std::max(coverage, std::clamp(0.75f - fabsf(d - radius - ring), 0.0f, 1.0f));
      plot(b, x, y, (int)(alpha * coverage));
    }
}

bool layersReady() {
  for (auto*& layer : layers) {
    if (layer && (layer->widthPixels != graphWidth() || layer->heightPixels != graphHeight())) {
      gfxBitmapFree(layer);
      layer = nullptr;
    }
    if (!layer) layer = gfxBitmapCreate(kGraphColumns, kGraphRows);
    if (!layer) return false;
    gfxBitmapClear(layer);
  }
  return true;
}

void drawGrid(Bitmap* b) {
  static const float majors[] = {100.0f, 1000.0f, 10000.0f};
  static const float minors[] = {50.0f, 200.0f, 500.0f, 2000.0f, 5000.0f};
  for (float hz : majors) span(b, (int)roundf(xForHz(hz)), 0, b->heightPixels - 1, 110);
  for (float hz : minors) span(b, (int)roundf(xForHz(hz)), 0, b->heightPixels - 1, 45);
  for (int db = -12; db <= 12; db += 6) {
    const int y = (int)roundf(yForDb((float)db));
    for (int x = 0; x < b->widthPixels; ++x) plot(b, x, y, db ? 45 : 110);
  }
}

void drawGraph() {
  gfxClearRect(kGraphX, kGraphY, kGraphColumns, kGraphRows);
  if (!layersReady()) return;
  const InstrumentEQ& settings = eq();
  const int width = graphWidth(), band = selectedBand();
  const float zero = yForDb(0.0f), rate = sampleRate();
  float total[kGraphColumns * 64], own[kGraphColumns * 64];
  if (width > kGraphColumns * 64) return;
  for (int x = 0; x < width; ++x) {
    const float hz = hzForX((float)x);
    total[x] = yForDb(instrumentEQResponseDb(settings, hz, rate));
    own[x] = yForDb(instrumentEQBandResponseDb(settings.bands[band], hz, rate));
  }
  drawGrid(layers[layerGrid]);
  // The selected band's own shape, then the sum of all three on top.
  for (int x = 0; x < width; ++x) span(layers[layerBand], x, zero, own[x], 45);
  curve(layers[layerBand], own, 150);
  for (int x = 0; x < width; ++x) span(layers[layerCurve], x, zero, total[x], 35);
  curve(layers[layerCurve], total, 255);
  for (int i = 0; i < INSTRUMENT_EQ_BANDS; ++i) {
    const InstrumentEQBand& b = settings.bands[i];
    const float x = xForHz(std::clamp((float)b.frequency, kLowestHz, kLowestHz * 1000.0f));
    const float limit = graphHeight() - 7.0f;
    const float y = std::clamp(yForDb(instrumentEQBandResponseDb(b, (float)b.frequency, rate)), 6.0f, limit);
    // The dot sits on its band's own curve; a switched-off band is a ring.
    const bool off = b.shape == EQShape::off;
    dot(layers[layerDot1 + i], x, y, off ? 2.0f : 5.0f, i == band ? 3.0f : off ? 1.5f : 0.0f, off ? 150 : 255);
  }
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxDrawBitmap(layers[layerGrid], kGraphX, kGraphY);
  gfxSetFgColor(bandColor(band));
  gfxDrawBitmap(layers[layerBand], kGraphX, kGraphY);
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxDrawBitmap(layers[layerCurve], kGraphX, kGraphY);
  for (int i = 0; i < INSTRUMENT_EQ_BANDS; ++i) {
    gfxSetFgColor(bandColor(i));
    gfxDrawBitmap(layers[layerDot1 + i], kGraphX, kGraphY);
  }
  drawnBand = band;
}

///////////////////////////////////////////////////////////////////////////////
// Fields

int columns(int row) { return row == rowEQ ? 1 : INSTRUMENT_EQ_BANDS; }

void formatValue(char* text, size_t size, int band, int row) {
  const InstrumentEQBand& b = eq().bands[band];
  switch (row) {
    case rowBand: snprintf(text, size, "%d", band + 1); break;
    case rowShape: snprintf(text, size, "%s", instrumentEQShapeName(b.shape)); break;
    case rowFrequency:
      if (b.frequency < 1000) snprintf(text, size, "%u Hz", b.frequency);
      else snprintf(text, size, b.frequency < 10000 ? "%.2f kHz" : "%.1f kHz", b.frequency / 1000.0f);
      break;
    case rowGain:
      if (hasGain(b.shape)) snprintf(text, size, "%+.1f dB", instrumentEQGainDb(b.gain));
      else snprintf(text, size, "-");
      break;
    default: {
      const float q = instrumentEQQ(b.q);
      snprintf(text, size, q < 10.0f ? "%.2f" : "%.1f", q);
      break;
    }
  }
}

int fieldWidth(int col, int row) {
  if (row == rowEQ) return 2;
  char text[16];
  formatValue(text, sizeof(text), col, row);
  return (int)strlen(text);
}

void drawStatic() {
  const ColorScheme& cs = appSettings.colorScheme;
  gfxSetFgColor(cs.textTitles);
  gfxPrintf(0, 0, "EQ %02X", bank);
  // Which instruments an edit here changes.
  char users[40] = "Used by";
  int count = 0;
  for (int i = 0; !master() && i < PROJECT_MAX_INSTRUMENTS; ++i) {
    const Instrument& instrument = chipnomadState->project.instruments[i];
    if (instrument.type == InstrumentType::none || instrument.eq != bank) continue;
    if (++count <= 8) snprintf(users + strlen(users), sizeof(users) - strlen(users), " %02X", i);
  }
  if (count > 8) snprintf(users + strlen(users), sizeof(users) - strlen(users), " +%d", count - 8);
  gfxSetFgColor(cs.textInfo);
  gfxPrint(0, 1, master() ? "Master output" : count ? users : "Not used by any instrument");
  static const struct { int db; const char* text; } dbLabels[] = {
    {12, "+12"}, {6, " +6"}, {0, "  0"}, {-6, " -6"}, {-12, "-12"}};
  for (const auto& label : dbLabels)
    gfxPrint(0, kGraphY + kGraphRows / 2 - (int)(label.db / kDbPerRow), label.text);
  // Labels centred under their grid lines, at least a space apart.
  static const struct { float hz; const char* text; } hzLabels[] = {
    {50, "50"}, {100, "100"}, {500, "500"}, {1000, "1k"}, {5000, "5k"}, {10000, "10k"}};
  int nextFree = kGraphX;
  for (const auto& label : hzLabels) {
    const int length = (int)strlen(label.text);
    const float column = kGraphX + (xForHz(label.hz) + 0.5f) / gfxGetCharWidth();
    const int x = std::min(std::max((int)(column - length * 0.5f + 0.5f), nextFree), kGraphX + kGraphColumns - length);
    gfxPrint(x, kAxisY, label.text);
    nextFree = x + length + 1;
  }
  gfxSetFgColor(cs.textLabel);
  static const char* const labels[] = {"Band", "Shape", "Freq", "Gain", "Q"};
  for (int row = rowBand; row < rowCount; ++row) gfxPrint(0, rowY(row), labels[row - rowBand]);
  drawGraph();
}

void drawField(int col, int row, CellState state) {
  const ColorScheme& cs = appSettings.colorScheme;
  if (row == rowEQ) {
    gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textTitles);
    gfxPrintf(3, 0, "%02X", bank);
    return;
  }
  const InstrumentEQBand& b = eq().bands[col];
  char text[16];
  formatValue(text, sizeof(text), col, row);
  // Settings a band ignores are dimmed.
  const bool unused = row != rowBand && row != rowShape &&
    (b.shape == EQShape::off || (row == rowGain && !hasGain(b.shape)));
  gfxClearRect(kValueX[col], rowY(row), kValueWidth, 1);
  gfxSetFgColor(row == rowBand ? bandColor(col) : unused ? cs.textInfo :
                state == CellState::focus ? cs.textValue : cs.textDefault);
  gfxPrint(kValueX[col], rowY(row), text);
}

void drawCursor(int col, int row) {
  if (row == rowEQ) {
    gfxCursor(3, 0, 2);
    return;
  }
  gfxCursor(kValueX[col], rowY(row), fieldWidth(col, row));
}

void noHeader(int, CellState) {}

///////////////////////////////////////////////////////////////////////////////
// Editing

// Quarter-tone steps, so a step up and a step down return to the same value.
void stepFrequency(InstrumentEQBand& b, int quarterTones) {
  const int hz = (int)lroundf(b.frequency * powf(2.0f, quarterTones / 24.0f));
  int next = hz != b.frequency ? hz : b.frequency + (quarterTones > 0 ? 1 : -1);
  b.frequency = (uint16_t)std::clamp(next, (int)kEQFrequencyMin, (int)kEQFrequencyMax);
}

void stepGain(InstrumentEQBand& b, int halfDecibels) {
  b.gain = (int8_t)std::clamp(b.gain + halfDecibels, kEQGainMin, kEQGainMax);
}

void stepQ(InstrumentEQBand& b, int steps) {
  b.q = (uint8_t)std::clamp(b.q + steps, (int)kEQQMin, (int)kEQQMax);
}

int direction(CellEditAction action, int small, int big) {
  switch (action) {
    case CellEditAction::increase: return small;
    case CellEditAction::decrease: return -small;
    case CellEditAction::increaseBig: return big;
    case CellEditAction::decreaseBig: return -big;
    default: return 0;
  }
}

int editBand(int band, int row, CellEditAction action) {
  InstrumentEQBand& b = eq().bands[band];
  InstrumentEQ defaults;
  instrumentEQDefaults(&defaults);
  const InstrumentEQBand& initial = defaults.bands[band];
  if (action == CellEditAction::clear) {
    switch (row) {
      case rowBand: b = initial; break;
      case rowShape: b.shape = initial.shape; break;
      case rowFrequency: b.frequency = initial.frequency; break;
      case rowGain: b.gain = 0; break;
      default: b.q = initial.q; break;
    }
    return 1;
  }
  switch (row) {
    case rowBand:
      // Moving the dot: sideways is frequency, up and down is gain (or a
      // cut's resonance).
      if (action == CellEditAction::increase || action == CellEditAction::decrease) {
        stepFrequency(b, direction(action, 4, 0));
      } else if (int d = direction(action, 0, 1)) {
        if (hasGain(b.shape)) stepGain(b, 2 * d);
        else if (isCut(b.shape)) stepQ(b, 2 * d);
      }
      return direction(action, 1, 1) != 0;
    case rowShape: {
      const int shape = std::clamp((int)b.shape + direction(action, 1, 1), 0, (int)EQShape::totalCount - 1);
      b.shape = (EQShape)shape;
      return direction(action, 1, 1) != 0;
    }
    case rowFrequency:
      if (int d = direction(action, 1, 12)) { stepFrequency(b, d); return 1; }
      return 0;
    case rowGain:
      if (!hasGain(b.shape)) return 0;
      if (int d = direction(action, 1, 6)) { stepGain(b, d); return 1; }
      return 0;
    default:
      if (int d = direction(action, 1, 12)) { stepQ(b, d); return 1; }
      return 0;
  }
}

int changeBank(int step) {
  if (master() || !step) return 0;
  const int next = std::clamp(bank + step, 0, PROJECT_EQ_BANKS - 1);
  if (next != bank) {
    bank = (uint8_t)next;
    chipnomadState->project.instruments[cInstrument].eq = bank;
    projectModified = 1;
    screenFullRedraw(&data);
  }
  return 1;
}

int onEdit(int col, int row, CellEditAction action) {
  action = convertMultiAction(action);
  if (row == rowEQ) return changeBank(direction(action, 1, 16));
  if (!editBand(col, row, action)) return 0;
  projectModified = 1;
  // A new shape changes which settings are dimmed.
  for (int r = 0; r < rowCount; ++r)
    if (r != row) drawField(col, r, CellState::normal);
  drawGraph();
  return 1;
}

const char* hint() {
  if (data.cursorRow == rowEQ) return "";
  const InstrumentEQBand& b = eq().bands[data.cursorCol];
  switch (data.cursorRow) {
    case rowBand:
      return isCut(b.shape) ? "EDIT+L/R: freq, EDIT+U/D: resonance"
                            : "EDIT+L/R: freq, EDIT+U/D: gain";
    case rowShape: return "Cut, shelf or bell; Off skips the band";
    case rowFrequency: return "EDIT+U/D: half an octave";
    case rowGain: return hasGain(b.shape) ? "Boost or cut, up to 15 dB" : "Cuts have no gain";
    default: return isCut(b.shape) ? "Resonance at the cut frequency" : "Width: higher is narrower";
  }
}

void setup(int input) {
  if ((input >= 0 && input < PROJECT_EQ_BANKS) || input == PROJECT_MASTER_EQ_BANK) bank = (uint8_t)input;
  data.cursorRow = rowBand;
  data.cursorCol = 0;
  data.topRow = 0;
  drawnBand = -1;
  hintOwnedText[0] = '\0';
}

void fullRedraw() { screenFullRedraw(&data); }

void draw() {
  if (selectedBand() != drawnBand) drawGraph();
  const char* active = screenGetActiveMessage();
  if (!active[0] || !strcmp(active, hintOwnedText)) {
    screenMessage(0, "%s", hint());
    snprintf(hintOwnedText, sizeof(hintOwnedText), "%s", screenGetActiveMessage());
  } else hintOwnedText[0] = '\0';
}

int onInput(int isKeyDown, int keys, int tapCount) {
  if (!keys) chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
  if (isKeyDown && (keys == keyOpt || keys == (keyLeft | keyShift))) {
    chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
    screenMessage(0, "");
    screenSetup(master() ? &screenMixer : &screenInstrument, master() ? 0 : cInstrument);
    return 1;
  }
  if (!master() && keys == (keyEdit | keyPlay)) {
    if (isKeyDown && !chipnomadGetPlaybackStatus(chipnomadState)->isPlaying)
      chipnomadQueuePlaybackPreviewNote(chipnomadState, *pSongTrack,
        instrumentFirstNote(&chipnomadState->project, cInstrument), cInstrument);
    return 1;
  }
  return screenInput(&data, isKeyDown, keys, tapCount);
}

ScreenPlaybackLevel getPlaybackLevel() { return master() ? ScreenPlaybackLevel::song : ScreenPlaybackLevel::phrase; }

struct Initializer {
  Initializer() {
    data = {};
    data.rows = rowCount;
    data.cursorRow = rowBand;
    data.selectMode = -1;
    data.playbackLevel = ScreenPlaybackLevel::none;
    data.getColumnCount = columns;
    data.drawStatic = drawStatic;
    data.drawRowHeader = noHeader;
    data.drawColHeader = noHeader;
    data.drawField = drawField;
    data.drawCursor = drawCursor;
    data.onEdit = onEdit;
  }
} initializer;
}  // namespace

const AppScreen screenInstrumentEQ = {
  .init = nullptr,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = getPlaybackLevel,
};
