#include "screen_midi_cc.h"
#include "screen_midi.h"
#include "common.h"
#include "corelib_gfx.h"
#include "corelib_input.h"
#include "project_utils.h"
#include "midi/midi_router.h"
#include <stdio.h>
#include <string.h>

static int learnRow = -1;
static uint32_t learnSerial;
static int columnCount(int);
static void drawStatic(void);
static void drawRowHeader(int, CellState);
static void drawColHeader(int, CellState);
static void drawCursor(int, int);
static void drawField(int, int, CellState);
static int onEdit(int, int, CellEditAction);
static ScreenData data = {
  .rows = PROJECT_MAX_MIDI_CC_MAPPINGS, .cursorRow = 0, .cursorCol = 0, .topRow = 0, .selectMode = -1,
  .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none, .getColumnCount = columnCount,
  .drawStatic = drawStatic, .drawCursor = drawCursor, .drawSelection = NULL,
  .drawRowHeader = drawRowHeader, .drawColHeader = drawColHeader, .drawField = drawField, .onEdit = onEdit,
  .onInput = NULL, .onRawInput = NULL, .isCellValid = NULL, .getLoopRange = NULL,
};
static int columnCount(int) { return 5; }
static int xForColumn(int col) { static const int x[] = {0, 5, 10, 16, 27}; return x[col]; }
static int widthForColumn(int col) { static const int w[] = {3, 4, 5, 10, 13}; return w[col]; }
static int globalTrackDestination(int destination) { return destination >= midiCCDestinationTrackMute && destination <= midiCCDestinationTrackDelaySend; }
static const char* globalDestinationName(int destination) {
  switch (destination) {
    case midiCCDestinationSongPlayStop: return "Song play/stop";
    case midiCCDestinationTrackMute: return "Track mute";
    case midiCCDestinationTrackSolo: return "Track solo";
    case midiCCDestinationTrackVolume: return "Track volume";
    case midiCCDestinationTrackReverbSend: return "Track reverb";
    case midiCCDestinationTrackDelaySend: return "Track delay";
    default: return NULL;
  }
}

static void instrumentLabel(int instrument, char* buffer, int bufferSize) {
  if (instrument < 0 || instrument >= PROJECT_MAX_INSTRUMENTS) {
    snprintf(buffer, bufferSize, "??");
    return;
  }
  const char* name = instrumentName(&chipnomadState->project, (uint8_t)instrument);
  snprintf(buffer, bufferSize, "%02X:%.*s", instrument, bufferSize - 4, name);
}

static void drawStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0, 0, "MIDI CC MAP");
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrint(xForColumn(0), 1, "ON");
  gfxPrint(xForColumn(1), 1, "CH");
  gfxPrint(xForColumn(2), 1, "CC");
  gfxPrint(xForColumn(3), 1, "INSTR");
  gfxPrint(xForColumn(4), 1, "DESTINATION");
}
static void drawRowHeader(int, CellState) {}
static void drawColHeader(int, CellState) {}
static void drawCursor(int col, int row) { gfxCursor(xForColumn(col), 2 + row - data.topRow, widthForColumn(col)); }

static void drawField(int col, int row, CellState state) {
  const MidiCCMapping& m = chipnomadState->project.midiCCMappings[row];
  const ColorScheme cs = appSettings.colorScheme;
  gfxSetFgColor(state == CellState::focus ? cs.textValue : cs.textDefault);
  int x = xForColumn(col), y = 2 + row - data.topRow; gfxClearRect(x, y, widthForColumn(col), 1);
  if (col == 0) gfxPrint(x, y, m.enabled ? "ON" : "--");
  else if (col == 1) gfxPrintf(x, y, "%02d", m.channel + 1);
  else if (col == 2) {
    if (learnRow == row) gfxPrint(x, y, "LEARN");
    else gfxPrintf(x, y, "%03d", m.cc);
  }
  else if (col == 3) {
    if (m.destination == midiCCDestinationSongPlayStop) { gfxPrint(x, y, "--"); return; }
    if (globalTrackDestination(m.destination)) { gfxPrintf(x, y, "TRK %d", m.instrument + 1); return; }
    char label[11];
    instrumentLabel(m.instrument, label, sizeof(label));
    gfxPrint(x, y, label);
  }
  else if (globalDestinationName(m.destination)) gfxPrint(x, y, globalDestinationName(m.destination));
  else if (!m.enabled || m.destination == midiCCDestinationNone) gfxPrint(x, y, "-");
  else if (m.instrument >= PROJECT_MAX_INSTRUMENTS ||
           !instrumentCCDestinationAvailable(&chipnomadState->project.instruments[m.instrument], m.destination))
    gfxPrint(x, y, "Unsupported");
  else gfxPrint(x, y, instrumentModDestinationNameForInstrument(&chipnomadState->project.instruments[m.instrument], m.destination));
}

static int nextDestination(const MidiCCMapping& m, int direction) {
  int value = m.destination;
  for (int i = 0; i <= midiCCDestinationTrackDelaySend; ++i) {
    value += direction;
    if (value < 0) value = midiCCDestinationTrackDelaySend;
    if (value > midiCCDestinationTrackDelaySend) value = 0;
    if (value == midiCCDestinationNone || globalDestinationName(value) ||
        (m.instrument < PROJECT_MAX_INSTRUMENTS &&
         instrumentCCDestinationAvailable(&chipnomadState->project.instruments[m.instrument], value))) return value;
  }
  return m.destination;
}

static int duplicateCC(int row, uint8_t channel, uint8_t cc) {
  for (int i = 0; i < PROJECT_MAX_MIDI_CC_MAPPINGS; ++i)
    if (i != row && chipnomadState->project.midiCCMappings[i].enabled &&
        chipnomadState->project.midiCCMappings[i].channel == channel &&
        chipnomadState->project.midiCCMappings[i].cc == cc) return 1;
  return 0;
}

static int onEdit(int col, int row, CellEditAction action) {
  MidiCCMapping& m = chipnomadState->project.midiCCMappings[row];
  if (col == 0 && (action == CellEditAction::tap || action == CellEditAction::doubleTap)) {
    if (!m.enabled && duplicateCC(row, m.channel, m.cc)) return 0;
    m.enabled ^= 1; projectModified = 1; return 1;
  }
  if (col == 2 && action == CellEditAction::tap) {
    learnRow = learnRow == row ? -1 : row;
    MidiCCIntent intent; midiRouterGetLastCC(chipnomadState->midiRouter, &intent, &learnSerial);
    return 1;
  }
  int direction = action == CellEditAction::increase || action == CellEditAction::increaseBig ? 1 :
                  action == CellEditAction::decrease || action == CellEditAction::decreaseBig ? -1 : 0;
  if (!direction) return 0;
  uint8_t newChannel = m.channel, newCC = m.cc;
  if (col == 1) newChannel = (uint8_t)((m.channel + direction + 16) % 16);
  else if (col == 2) newCC = (uint8_t)((m.cc + direction + 128) % 128);
  else if (col == 3) {
    int count = globalTrackDestination(m.destination) ? PROJECT_MAX_TRACKS : PROJECT_MAX_INSTRUMENTS;
    m.instrument = (uint8_t)((m.instrument + direction + count) % count);
  }
  else if (col == 4) m.destination = (uint8_t)nextDestination(m, direction);
  else return 0;
  if ((col == 1 || col == 2) && m.enabled && duplicateCC(row, newChannel, newCC)) return 0;
  m.channel = newChannel; m.cc = newCC;
  projectModified = 1; return 1;
}

static void setup(int) { data.cursorRow = data.cursorCol = data.topRow = 0; learnRow = -1; }
static void fullRedraw(void) { screenFullRedraw(&data); }
static void draw(void) {
  if (learnRow < 0) return;
  MidiCCIntent intent; uint32_t serial;
  if (!midiRouterGetLastCC(chipnomadState->midiRouter, &intent, &serial) || serial == learnSerial) return;
  MidiCCMapping& m = chipnomadState->project.midiCCMappings[learnRow];
  if (duplicateCC(learnRow, intent.channel, intent.cc)) return;
  m.channel = intent.channel; m.cc = intent.cc; m.enabled = 1;
  learnSerial = serial; learnRow = -1; projectModified = 1; fullRedraw();
}
static int onInput(int isKeyDown, int keys, int taps) {
  if (keys == keyOpt) { screenSetup(&screenMidi, 0); return 1; }
  return screenInput(&data, isKeyDown, keys, taps);
}
static ScreenPlaybackLevel playbackLevel(void) { return ScreenPlaybackLevel::none; }
const AppScreen screenMidiCC = {NULL, setup, fullRedraw, draw, onInput, playbackLevel};
