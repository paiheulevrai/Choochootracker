#include "screen_midi_cc.h"
#include "screen_midi.h"
#include "common.h"
#include "corelib_gfx.h"
#include "corelib_input.h"
#include "project_utils.h"
#include "midi/midi_router.h"
#include "selection_popup.h"
#include <stdio.h>
#include <string.h>
#include <vector>

static int learnRow = -1;
static uint32_t learnSerial;
static int destinationButtonDown;
static int popupRow;
static int columnCount(int);
static void drawStatic(void);
static void drawRowHeader(int, CellState);
static void drawColHeader(int, CellState);
static void drawCursor(int, int);
static void drawField(int, int, CellState);
static int onEdit(int, int, CellEditAction);
static int isButtonCell(int col, int) { return col != 1; }
static ScreenData data = {
  .rows = PROJECT_MAX_MIDI_CC_MAPPINGS, .cursorRow = 0, .cursorCol = 0, .topRow = 0, .selectMode = -1,
  .selectStartRow = 0, .selectStartCol = 0, .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::song, .getColumnCount = columnCount,
  .drawStatic = drawStatic, .drawCursor = drawCursor, .drawSelection = NULL,
  .drawRowHeader = drawRowHeader, .drawColHeader = drawColHeader, .drawField = drawField, .onEdit = onEdit,
  .onInput = NULL, .onRawInput = NULL, .isCellValid = NULL, .getLoopRange = NULL,
  .isButtonCell = isButtonCell,
};
static int columnCount(int) { return 4; }
static int xForColumn(int col) { static const int x[] = {0, 4, 8, 14}; return x[col]; }
static int widthForColumn(int col) { static const int w[] = {3, 3, 5, 26}; return w[col]; }
static int globalTrackDestination(int destination) { return destination >= midiCCDestinationTrackMute && destination <= midiCCDestinationTrackDelaySend; }
static const char* globalDestinationName(int destination) {
  switch (destination) {
    case midiCCDestinationSongPlayStop: return "Song play/stop";
    case midiCCDestinationTrackMute: return "Mute";
    case midiCCDestinationTrackSolo: return "Solo";
    case midiCCDestinationTrackVolume: return "Volume";
    case midiCCDestinationTrackReverbSend: return "Reverb send";
    case midiCCDestinationTrackDelaySend: return "Delay send";
    default: return NULL;
  }
}

static void destinationLabel(const MidiCCMapping& m, char* buffer, int bufferSize) {
  if (m.destination == midiCCDestinationNone) snprintf(buffer, bufferSize, "Unassigned");
  else if (m.destination == midiCCDestinationSongPlayStop) snprintf(buffer, bufferSize, "Song: Play/stop");
  else if (globalTrackDestination(m.destination) && m.instrument < PROJECT_MAX_TRACKS)
    snprintf(buffer, bufferSize, "Track %d: %s", m.instrument + 1, globalDestinationName(m.destination));
  else if (m.instrument < PROJECT_MAX_INSTRUMENTS &&
           instrumentCCDestinationAvailable(&chipnomadState->project.instruments[m.instrument], m.destination)) {
    const char* parameter = instrumentCCDestinationName(&chipnomadState->project.instruments[m.instrument], m.destination);
    // Keep the parameter visible; the cursor hint has room for the full name.
    int nameWidth = bufferSize - 6 - (int)strlen(parameter);
    if (nameWidth > 0)
      snprintf(buffer, bufferSize, "%02X %.*s: %s", m.instrument, nameWidth,
               instrumentName(&chipnomadState->project, m.instrument), parameter);
    else snprintf(buffer, bufferSize, "%02X: %s", m.instrument, parameter);
  } else snprintf(buffer, bufferSize, "Unsupported");
}

// One menu choice contains both parts of the saved mapping; no format change.
static int destinationValue(const MidiCCMapping& m) {
  return m.destination == midiCCDestinationNone || m.destination == midiCCDestinationSongPlayStop
    ? m.destination : (m.instrument << 8) | m.destination;
}
static void setDestination(int row, int value) {
  auto& m = chipnomadState->project.midiCCMappings[row];
  m.instrument = (uint8_t)(value >> 8);
  m.destination = (uint8_t)value;
  projectModified = 1;
}

static std::vector<SelectionItem> destinationCategories;
static std::vector<SelectionItem> destinationChoices;
static char trackLabels[PROJECT_MAX_TRACKS][12];
static char instrumentLabels[PROJECT_MAX_INSTRUMENTS][PROJECT_INSTRUMENT_NAME_LENGTH + 4];

static void buildDestinations() {
  destinationCategories.clear();
  destinationChoices.clear();
  // Child pointers stay stable while the flat list is built and displayed.
  destinationChoices.reserve(2 + PROJECT_MAX_TRACKS * 5 + PROJECT_MAX_INSTRUMENTS * 14);
  destinationChoices.push_back({"Unassigned", midiCCDestinationNone, nullptr, 0});
  destinationCategories.push_back(destinationChoices.back());
  destinationChoices.push_back({"Play/stop", midiCCDestinationSongPlayStop, nullptr, 0,
                                "Press to start, press again to stop"});
  destinationCategories.push_back({"Song", -1, &destinationChoices.back(), 1});
  for (int track = 0; track < chipnomadState->project.tracksCount && track < PROJECT_MAX_TRACKS; ++track) {
    int first = (int)destinationChoices.size();
    for (int d = midiCCDestinationTrackMute; d <= midiCCDestinationTrackDelaySend; ++d)
      destinationChoices.push_back({globalDestinationName(d), (track << 8) | d, nullptr, 0});
    snprintf(trackLabels[track], sizeof(trackLabels[track]), "Track %d", track + 1);
    destinationCategories.push_back({trackLabels[track], -1, &destinationChoices[first], 5});
  }
  for (int instrument = 0; instrument < PROJECT_MAX_INSTRUMENTS; ++instrument) {
    if (instrumentIsEmpty(&chipnomadState->project, instrument)) continue;
    int first = (int)destinationChoices.size();
    const auto* inst = &chipnomadState->project.instruments[instrument];
    for (int d = 1; d < midiCCDestinationSongPlayStop; ++d)
      if (instrumentCCDestinationAvailable(inst, d))
        destinationChoices.push_back({instrumentCCDestinationName(inst, d), (instrument << 8) | d,
                                      nullptr, 0, instrumentName(&chipnomadState->project, instrument)});
    int count = (int)destinationChoices.size() - first;
    if (!count) continue;
    snprintf(instrumentLabels[instrument], sizeof(instrumentLabels[instrument]), "%02X %s",
             instrument, instrumentName(&chipnomadState->project, instrument));
    destinationCategories.push_back({instrumentLabels[instrument], -1, &destinationChoices[first], count});
  }
}
static void destinationCancelled() { screenSetup(&screenMidiCC, -1); }
static void destinationSelected(int value) {
  setDestination(popupRow, value);
  destinationCancelled();
}
static void openDestinationPopup(int row) {
  popupRow = row;
  learnRow = -1;
  buildDestinations();
  screenMessage(0, "");
  selectionPopupSetup("MIDI CC DESTINATION", destinationCategories.data(), (int)destinationCategories.size(),
                      destinationValue(chipnomadState->project.midiCCMappings[row]),
                      destinationSelected, destinationCancelled);
  screenSetup(&screenSelectionPopup, 0);
}
static int cycleDestination(int row, int direction) {
  buildDestinations();
  int value = destinationValue(chipnomadState->project.midiCCMappings[row]);
  int count = (int)destinationChoices.size();
  int next = 0;
  for (int i = 0; i < count; ++i)
    if (destinationChoices[i].value == value) { next = (i + direction + count) % count; break; }
  setDestination(row, destinationChoices[next].value);
  return 1;
}

static void drawStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles); gfxPrint(0, 0, "MIDI CC MAP");
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrint(xForColumn(0), 1, "ON");
  gfxPrint(xForColumn(1), 1, "CH");
  gfxPrint(xForColumn(2), 1, "CC");
  gfxPrint(xForColumn(3), 1, "DESTINATION");
}
static void drawRowHeader(int, CellState) {}
static void drawColHeader(int, CellState) {}
static void destinationHint(int row) {
  char label[41];
  destinationLabel(chipnomadState->project.midiCCMappings[row], label, sizeof(label));
  screenMessage(0, "%s", label);
}
static void drawCursor(int col, int row) {
  gfxCursor(xForColumn(col), 2 + row - data.topRow, widthForColumn(col));
  if (col == 3) destinationHint(row);
  else screenMessage(0, "");
}

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
  else {
    char label[27];
    destinationLabel(m, label, sizeof(label));
    if (state != CellState::focus && m.destination == midiCCDestinationNone)
      gfxSetFgColor(cs.textEmpty);
    gfxPrint(x, y, label);
  }
}

static int duplicateCC(int row, uint8_t channel, uint8_t cc) {
  for (int i = 0; i < PROJECT_MAX_MIDI_CC_MAPPINGS; ++i)
    if (i != row && chipnomadState->project.midiCCMappings[i].enabled &&
        chipnomadState->project.midiCCMappings[i].destination != midiCCDestinationNone &&
        chipnomadState->project.midiCCMappings[i].channel == channel &&
        chipnomadState->project.midiCCMappings[i].cc == cc) return 1;
  return 0;
}

static int onEdit(int col, int row, CellEditAction action) {
  MidiCCMapping& m = chipnomadState->project.midiCCMappings[row];
  if (col == 3 && action == CellEditAction::tap) { openDestinationPopup(row); return 1; }
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
  else if (col == 3) return cycleDestination(row, direction);
  else return 0;
  if ((col == 1 || col == 2) && m.enabled && duplicateCC(row, newChannel, newCC)) return 0;
  m.channel = newChannel; m.cc = newCC;
  projectModified = 1; return 1;
}

static void setup(int input) {
  if (input >= 0) data.cursorRow = data.cursorCol = data.topRow = 0;
  learnRow = -1;
  destinationButtonDown = 0;
}
static void fullRedraw(void) { screenFullRedraw(&data); }
static void draw(void) {
  if (data.cursorCol == 3 && !screenGetActiveMessage()[0]) destinationHint(data.cursorRow);
  if (learnRow < 0) return;
  MidiCCIntent intent; uint32_t serial;
  if (!midiRouterGetLastCC(chipnomadState->midiRouter, &intent, &serial) || serial == learnSerial) return;
  MidiCCMapping& m = chipnomadState->project.midiCCMappings[learnRow];
  if (duplicateCC(learnRow, intent.channel, intent.cc)) return;
  m.channel = intent.channel; m.cc = intent.cc; m.enabled = 1;
  learnSerial = serial; learnRow = -1; projectModified = 1; fullRedraw();
}
static int onInput(int isKeyDown, int keys, int taps) {
  if (data.cursorCol == 3) {
    auto action = popupEditInput(isKeyDown, keys, &destinationButtonDown);
    if (action == PopupEditInput::hold) return 1;
    if (action == PopupEditInput::open) { openDestinationPopup(data.cursorRow); return 1; }
    if (action == PopupEditInput::cycle) {
      cycleDestination(data.cursorRow, keys & keyLeft ? -1 : 1);
      fullRedraw();
      return 1;
    }
  }
  if (keys == keyOpt) { screenSetup(&screenMidi, 0); return 1; }
  if (keys == keyPlay || keys == (keyPlay | keyShift)) return 0;
  return screenInput(&data, isKeyDown, keys, taps);
}
static ScreenPlaybackLevel playbackLevel(void) { return ScreenPlaybackLevel::song; }
const AppScreen screenMidiCC = {NULL, setup, fullRedraw, draw, onInput, playbackLevel};
