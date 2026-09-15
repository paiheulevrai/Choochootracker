#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "utils.h"

static const char* engineName(DrumSynthEngine e) {
  static const char* names[] = {"KICK", "SNARE", "HAT", "CLAP", "TOM", "RIM", "FM", "NOISE", "COWBELL", "CYMBAL", "SHAKER", "CLAVE"};
  return (int)e < 12 ? names[(int)e] : "KICK";
}
static const char* macroName(DrumSynthEngine e, int macro) {
  static const char* generic[] = {"Decay", "Tone", "Sweep", "Noise", "FM", "Drive"};
  static const char* kick[] = {"Decay", "Tone", "Sweep", "Click", "Harm", "Drive"};
  static const char* snare[] = {"Decay", "Tone", "Snap", "Wire", "Body", "Drive"};
  static const char* hat[] = {"Decay", "Tone", "Reso", "Noise", "Metal", "Drive"};
  static const char* fm[] = {"Decay", "Ratio", "M.Dec", "Noise", "Index", "Drive"};
  const char* const* names = e == DrumSynthEngine::kick ? kick : e == DrumSynthEngine::snare ? snare :
    e == DrumSynthEngine::hat ? hat : e == DrumSynthEngine::fm ? fm : generic;
  return names[macro];
}
static uint8_t* macro(InstrumentDrumSynth* d, int index) {
  uint8_t* values[] = {&d->decay, &d->tone, &d->sweep, &d->noise, &d->fm, &d->drive};
  return index >= 0 && index < 6 ? values[index] : NULL;
}
static int columns(int row) { return row < 3 ? instrumentCommonColumnCount(row) : row == 3 ? 1 : row <= 8 ? 2 : 1; }
static void drawStatic(void) {
  instrumentCommonDrawStatic(); InstrumentDrumSynth* d = &chipnomadState->project.instruments[cInstrument].chip.drumSynth;
  gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0, 6, "Engine"); gfxPrint(0, 7, "SOUND");
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  for (int i = 0; i < 6; ++i) gfxPrint(0, 8 + i, drumSynthMacroUsed(d->engine, i) ? macroName(d->engine, i) : "---");
  instrumentCommonDrawVoicePostStatic(0);
}
static void drawCursor(int col, int row) {
  if (row < 3) { instrumentCommonDrawCursor(col, row); return; }
  if (col && row >= 4 && row <= 8) { instrumentCommonDrawVoicePostCursor(col, row); return; }
  if (row == 3) gfxCursor(11, 6, 28); else gfxCursor(11, row + 4, 7);
}
static void drawField(int col, int row, CellState state) {
  if (row < 3) { instrumentCommonDrawField(col, row, state); return; }
  InstrumentDrumSynth* d = &chipnomadState->project.instruments[cInstrument].chip.drumSynth;
  if (col && row >= 4 && row <= 8) { instrumentCommonDrawVoicePostField(col, row, state, d); return; }
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  // Macro fields share their rows with the post-filter labels at x=18.
  // Do not erase the right-hand column while redrawing a left macro.
  gfxClearRect(11, row == 3 ? 6 : row + 4, row == 3 ? 20 : 7, 1);
  if (row == 3) gfxPrintf(11, 6, "%02d %s", (int)d->engine, engineName(d->engine));
  else if (drumSynthMacroUsed(d->engine, row - 4)) {
    if (uint8_t* value = macro(d, row - 4)) gfxPrint(11, row + 4, byteToHex(*value));
  } else gfxPrint(11, row + 4, "--");
}
static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentDrumSynth* d = &chipnomadState->project.instruments[cInstrument].chip.drumSynth;
  if (col && row >= 4 && row <= 8) { int ok = instrumentCommonOnEditVoicePost(col, row, action, d); if (ok) projectModified = 1; return ok; }
  int ok = row == 3 ? edit8noLast(action, (uint8_t*)&d->engine, 1, 0, 11) :
    (drumSynthMacroUsed(d->engine, row - 4) && macro(d, row - 4) ? edit8noLast(action, macro(d, row - 4), 16, 0, 255) : 0);
  if (ok) { projectModified = 1; screenFullRedraw(&screenInstrumentDrumSynth); }
  return ok;
}
ScreenData screenInstrumentDrumSynth = {
  .rows = 10, .cursorRow = 0, .cursorCol = 0, .topRow = 0, .selectMode = -1,
  .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none, .getColumnCount = columns,
  .drawStatic = drawStatic, .drawCursor = drawCursor, .drawSelection = NULL,
  .drawRowHeader = NULL, .drawColHeader = NULL, .drawField = drawField, .onEdit = onEdit,
  .onInput = NULL, .onRawInput = NULL, .isCellValid = NULL, .getLoopRange = NULL,
};
