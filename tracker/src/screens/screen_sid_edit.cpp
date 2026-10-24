#include "screens.h"
#include "screen_instrument.h"
#include "project_utils.h"
#include "corelib_gfx.h"
#include "chipnomad_lib.h"
#include "sid_patch.h"
#include <algorithm>
#include <cstring>
#include <iterator>

// The SID recipe is one voice plus a silent ring/sync partner, not an FM
// operator bank. Keep the FM editor's eight-character cells and native values.
namespace {
const char* const waves[] = {"off", "tri", "saw", "TS", "pls", "TP", "SP", "TSP", "noi"};
const char* const offOn[] = {"off", "on"};
const char* const filters[] = {"off", "LP", "BP", "LB", "HP", "LH", "BH", "LBH"};
const char* const arpeggios[] = {"off", "maj", "min", "5th", "sus"};
const char* const sweeps[] = {"lin", "exp"};

struct Field {
  const char* tag;
  SIDParameter parameter;
  int min, max, step;
  const char* hint;
  const char* const* names = nullptr;
};
struct Row {
  const char* label;
  int count;
  Field cells[3];
};
const Row rows[] = {
  {"Voice", 3, {{"WAV", sidWave, 1, 8, 1, "Waveform: T triangle, S saw, P pulse", waves},
                {"RNG", sidRing, 0, 1, 1, "Ring modulation; uses silent partner", offOn},
                {"SYN", sidSync, 0, 1, 1, "Hard sync; uses silent partner", offOn}}},
  {"ADSR", 2, {{"AR", sidAttack, 0, 15, 1, "Attack time: 0 fast, F slow"},
               {"DR", sidDecay, 0, 15, 1, "Decay time: 0 fast, F slow"}}},
  {"", 2, {{"SL", sidSustain, 0, 15, 1, "Sustain level: 0 silent, F full"},
           {"RR", sidRelease, 0, 15, 1, "Release time: 0 fast, F slow"}}},
  {"Pulse", 3, {{"PW", sidPulse, 0, 4095, 128, "Pulse width: 000-FFF"},
                {"DEP", sidPulseDepth, 0, 2047, 128, "Pulse motion depth: native width units"},
                {"RT", sidPulseRate, 0, 1000, 10, "Pulse motion rate: tenths of Hz"}}},
  {"Filter", 3, {{"MOD", sidFilterMode, 0, 7, 1, "Filter: L low, B band, H high pass", filters},
                 {"CUT", sidCutoff, 0, 2047, 128, "Filter cutoff: 000-7FF"},
                 {"RES", sidResonance, 0, 15, 1, "Filter resonance: 0-F"}}},
  // Map the noncontiguous FFFF sentinel to -1 in the editor. It must never
  // produce invalid cutoff values between 0800 and FFFE.
  {"Motion", 1, {{"TGT", sidCutoffTarget, -1, 2047, 128, "Cutoff target; clear=off; GAT or 75 ticks"}}},
  {"Clock", 2, {{"HZ", sidMacroRate, 1, 200, 10, "Recipe clock in Hz, shown in hex"},
                {"GAT", sidGateFrames, 0, 1000, 10, "Gate ticks; 000 holds until note off"}}},
  {"Pitch", 3, {{"FIX", sidFixedFrequency, 0, 65535, 256, "SID frequency register; 0000 follows note"},
                {"MUL", sidPartnerRatio, 1, 16, 1, "Silent partner frequency multiplier"},
                {"ARP", sidArpeggio, 0, 4, 1, "Three-note recipe arpeggio", arpeggios}}},
  {"Sweep", 3, {{"TGT", sidSweepTarget, 0, 65535, 256, "Pitch sweep target register; 0000=off"},
                {"LEN", sidSweepFrames, 0, 1000, 10, "Pitch sweep ticks; 000 uses one tick"},
                {"MOD", sidSweepMode, 0, 1, 1, "Pitch sweep: linear or exponential", sweeps}}},
  {"Vibrato", 2, {{"DEP", sidVibratoDepth, 0, 4095, 128, "Vibrato depth: SID frequency units"},
                  {"RT", sidVibratoRate, 0, 1000, 10, "Vibrato rate: tenths of Hz"}}},
};
ScreenData data;
char hintOwnedText[48];
Instrument* current() { return &chipnomadState->project.instruments[cInstrument]; }
const Field* field(int col, int row) {
  return row >= 0 && row < int(std::size(rows)) && col >= 0 && col < rows[row].count
    ? &rows[row].cells[col] : nullptr;
}
int columns(int row) { return row >= 0 && row < int(std::size(rows)) ? rows[row].count : 1; }
int cellX(int col) { return 8 + 8 * col; }
int rowY(int row) { return 3 + row - data.topRow; }
bool visible(int row) { return row >= data.topRow && row < data.topRow + screenVisibleRows(); }
int width(const Field& f) {
  if (f.names || f.min < 0) return 3;
  return f.max > 4095 ? 4 : f.max > 255 ? 3 : f.max > 15 ? 2 : 1;
}
int value(const Field& f) {
  int v = current()->chip.sid.value[f.parameter];
  return f.parameter == sidCutoffTarget && v == 65535 ? -1 : v;
}
int parameterFX(int parameter) {
  switch (parameter) {
    case sidWave: return fxSWV;
    case sidAttack: return fxSAT;
    case sidDecay: return fxSDE;
    case sidSustain: return fxSSU;
    case sidRelease: return fxSRL;
    case sidPulse: return fxSCP;
    case sidCutoff: return fxSCT;
    case sidResonance: return fxSRN;
    case sidFilterMode: return fxSFTY;
    case sidRing: return fxSRG;
    case sidSync: return fxSSY;
    case sidPartnerRatio: return fxSPR;
    case sidMacroRate: return fxSMR;
    default: return -1;
  }
}
const char* overrideReason(int parameter) {
  // Imported macros run after the track overrides, so identify them first.
  if (const char* reason = sidImportedControlReason(current()->chip.sid, parameter)) return reason;
  const int fx = parameterFX(parameter);
  if (fx < 0) return nullptr;
  const auto* status = chipnomadGetPlaybackStatus(chipnomadState);
  if (!status || !status->isPlaying) return nullptr;
  for (int t = 0; t < chipnomadState->project.tracksCount; ++t) {
    const auto& track = status->tracks[t];
    if (!status->trackEnabled[t] || track.mode == PlaybackMode::stopped ||
        track.mode == PlaybackMode::none || track.note.instrument != cInstrument ||
        track.note.noteKilled || !track.note.fx[fx].isOn) continue;
    static char reason[48];
    snprintf(reason, sizeof(reason), "Track %d: %s active; editing preset", t + 1, fxNames[fx].name);
    return reason;
  }
  return nullptr;
}
uint32_t drawnOverrides = 0;
uint32_t overrideMask() {
  uint32_t mask = 0;
  for (int p = 1; p < sidParameterCount; ++p) if (overrideReason(p)) mask |= 1u << p;
  return mask;
}
void drawStatic() {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrintf(0, 0, "SID EDIT %02X", cInstrument);
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrint(12, 0, instrumentTypeName(InstrumentType::SID));
  gfxPrint(0, 1, current()->name);
  const int format = current()->chip.sid.program.format;
  drawnOverrides = overrideMask();
  if (format) {
    gfxPrint(0, 14, format == 2 ? "Source: GoatTracker tables" : "Source: imported SID macros");
    gfxPrintf(0, 16, "Source clock: %u Hz", current()->chip.sid.program.rate);
  }
  if (drawnOverrides) gfxPrint(0, 15, "* = program / FX override");
}
void drawRowHeader(int row, CellState state) {
  if (!visible(row) || !field(0, row)) return;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textDefault : appSettings.colorScheme.textInfo);
  gfxClearRect(0, rowY(row), 8, 1);
  gfxPrint(0, rowY(row), rows[row].label);
}
void drawField(int col, int row, CellState state) {
  const Field* f = field(col, row);
  if (!f || !visible(row)) return;
  gfxClearRect(cellX(col), rowY(row), 8, 1);
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrint(cellX(col), rowY(row), f->tag);
  if (overrideReason(f->parameter)) gfxPrint(cellX(col) + 3, rowY(row), "*");
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  const int v = value(*f), x = cellX(col) + 4, y = rowY(row);
  if (f->names) gfxPrint(x, y, f->names[std::clamp(v, f->min, f->max)]);
  else if (v < 0) gfxPrint(x, y, "off");
  else gfxPrintf(x, y, "%0*X", width(*f), v);
}
void drawColHeader(int, CellState) {}
void drawCursor(int col, int row) {
  const Field* f = field(col, row);
  if (f && visible(row)) gfxCursor(cellX(col) + 4, rowY(row), width(*f));
}
int onEdit(int col, int row, CellEditAction action) {
  const Field* f = field(col, row);
  if (!f) return 0;
  if (const char* reason = sidImportedControlReason(current()->chip.sid, f->parameter)) {
    screenMessage(MESSAGE_TIME_ERROR, "%s", reason);
    return 1;
  }
  action = convertMultiAction(action);
  int v = value(*f);
  if (action == CellEditAction::clear) v = f->min;
  else if (action == CellEditAction::increase) ++v;
  else if (action == CellEditAction::decrease) --v;
  else if (action == CellEditAction::increaseBig) v += f->step;
  else if (action == CellEditAction::decreaseBig) v -= f->step;
  else return 0;
  v = std::clamp(v, f->min, f->max);
  auto& stored = current()->chip.sid.value[f->parameter];
  const uint16_t next = v < 0 ? 65535 : v;
  if (stored != next) { stored = next; projectModified = 1; }
  return 1;
}
void setup(int) {
  data.cursorRow = data.cursorCol = data.topRow = 0;
  hintOwnedText[0] = '\0';
}
void fullRedraw() { screenFullRedraw(&data); }
void draw() {
  const uint32_t mask = overrideMask();
  if (mask != drawnOverrides) {
    drawnOverrides = mask;
    for (int row = 0; row < data.rows; ++row)
      for (int col = 0; col < columns(row); ++col)
        drawField(col, row, row == data.cursorRow && col == data.cursorCol ? CellState::focus : CellState::normal);
    gfxClearRect(0, 15, 32, 1);
    if (mask) { gfxSetFgColor(appSettings.colorScheme.textInfo); gfxPrint(0, 15, "* = program / FX override"); }
    drawCursor(data.cursorCol, data.cursorRow);
  }
  const Field* f = field(data.cursorCol, data.cursorRow);
  if (!f) return;
  const char* active = screenGetActiveMessage();
  if (!active[0] || !strcmp(active, hintOwnedText)) {
    const char* hint = overrideReason(f->parameter);
    if (!hint) hint = f->parameter == sidMacroRate && current()->chip.sid.program.format
      ? "Motion timebase; ticks use source clock" : f->hint;
    screenMessage(0, "%s", hint);
    snprintf(hintOwnedText, sizeof(hintOwnedText), "%s", screenGetActiveMessage());
  } else hintOwnedText[0] = '\0';
}
int onInput(int isKeyDown, int keys, int tapCount) {
  if (!keys) chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
  if (keys == keyOpt || keys == (keyLeft | keyShift)) {
    chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
    screenSetup(&screenInstrument, cInstrument);
    return 1;
  }
  if (keys == (keyEdit | keyPlay)) {
    if (isKeyDown && !chipnomadGetPlaybackStatus(chipnomadState)->isPlaying)
      chipnomadQueuePlaybackPreviewNote(chipnomadState, *pSongTrack,
        instrumentFirstNote(&chipnomadState->project, cInstrument), cInstrument);
    return 1;
  }
  return screenInput(&data, isKeyDown, keys, tapCount);
}
ScreenPlaybackLevel getPlaybackLevel() { return ScreenPlaybackLevel::phrase; }
struct Initializer {
  Initializer() {
    data = {};
    data.rows = std::size(rows);
    data.selectMode = -1;
    data.playbackLevel = ScreenPlaybackLevel::none;
    data.getColumnCount = columns;
    data.drawStatic = drawStatic;
    data.drawRowHeader = drawRowHeader;
    data.drawColHeader = drawColHeader;
    data.drawField = drawField;
    data.drawCursor = drawCursor;
    data.onEdit = onEdit;
  }
} initializer;
} // namespace

const AppScreen screenSIDEdit = {
  .init = nullptr,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = getPlaybackLevel,
};

