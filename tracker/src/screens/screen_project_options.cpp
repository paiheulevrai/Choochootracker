#include "screen_project.h"
#include "audio_manager.h"
#include "corelib_gfx.h"
#include "link/link_client.h"
#include "midi/midi_router.h"
#include "pitch_table_utils.h"

namespace {

extern ScreenData tuningData, linkData, syncOutData;

int columnCount(int) { return 1; }
void noHeader(int, CellState) {}
void setup(int) {}
ScreenPlaybackLevel playbackLevel(void) { return ScreenPlaybackLevel::song; }

int editToggle(CellEditAction action, uint8_t* value) {
  action = convertMultiAction(action);
  if (action == CellEditAction::tap || action == CellEditAction::doubleTap) {
    *value ^= 1;
    return 1;
  }
  return edit8noLast(action, value, 1, 0, 1);
}

void saveSyncSettings(void) {
  if (settingsSave()) screenMessage(MESSAGE_TIME_ERROR, "Could not save sync settings");
}

void title(const char* text) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0, 0, text);
}

void fieldAt(int y, const char* label, const char* value, CellState state) {
  gfxClearRect(0, y, 18, 1);
  gfxSetFgColor(appSettings.colorScheme.textLabel);
  gfxPrint(0, y, label);
  gfxClearRect(18, y, 16, 1);
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue :
                appSettings.colorScheme.textDefault);
  gfxPrintf(18, y, "%.16s", value);
}
void field(int row, const char* label, const char* value, CellState state) {
  fieldAt(row + 2, label, value, state);
}

void cursor(int, int row) {
  gfxCursor(18, row + 2, 3);
}
int submenuButton(int, int row) { return row == 1; }
int linkRowY(int row) { return row + 2 + (row > 0); }
void linkCursor(int, int row) { gfxCursor(18, linkRowY(row), row == 5 ? 6 : 3); }
int linkCellValid(int, int row) { return row != 1; }

void tuningStatic(void) { title("TUNING"); }
void tuningField(int, int row, CellState state) {
  field(row, row == 0 ? "Linear pitch" : "Pitch table",
        row == 0 ? (chipnomadState->project.linearPitch ? "On" : "Off") :
        chipnomadState->project.pitchTable.name, state);
}
int tuningEdit(int, int row, CellEditAction action) {
  if (row == 1) {
    if (action == CellEditAction::tap || action == CellEditAction::doubleTap)
      screenSetup(&screenPitchTable, 0);
    return 0;
  }
  if (!editToggle(action, &chipnomadState->project.linearPitch)) return 0;
  chipnomadQueuePlaybackStop(chipnomadState);
  reinitializePitchTable(&chipnomadState->project);
  projectModified = 1;
  return 1;
}

void linkStatic(void) {
  title("LINK OPTIONS");
#if defined(DESKTOP_BUILD) || defined(PORTMASTER_BUILD)
  gfxSetFgColor(appSettings.colorScheme.textLabel);
  gfxPrint(0, 4, "Link Audio");
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrint(0, 12, "Compensation: local + received.");
  gfxPrint(0, 13, "Active only with Link Receive.");
  gfxPrint(0, 14, "Options reset on restart.");
  if (!linkClientIsEnabled(chipnomadState->linkClient))
    gfxPrint(0, 16, "Connect Link on Project first.");
#else
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrint(0, 6, "Start/stop is shared both ways.");
#endif
}
void linkField(int, int row, CellState state) {
  if (row == 1) return;
  if (row == 5) {
    char value[16];
    snprintf(value, sizeof(value), "%03d ms",
             linkClientGetAudioCompensationMillis(chipnomadState->linkClient));
    fieldAt(linkRowY(row), " - Compensation", value, state);
    return;
  }
  const bool enabled = row == 0 ? appSettings.linkTransport :
    row == 2 ? linkClientIsAudioEnabled(chipnomadState->linkClient) :
    row == 3 ? linkClientIsAudioReceiveEnabled(chipnomadState->linkClient) :
    audioManager.getLocalOutputEnabled();
  const char* label = row == 0 ? "Link start/stop" : row == 2 ? " - Send" :
    row == 3 ? " - Receive" : " - Local audio";
  fieldAt(linkRowY(row), label, enabled ? "On" : "Off", state);
}
int linkEdit(int, int row, CellEditAction action) {
  if (row == 1) return 0;
  if (row == 0) {
    if (!editToggle(action, &appSettings.linkTransport)) return 0;
    linkClientSetTransportSync(chipnomadState->linkClient, appSettings.linkTransport);
    saveSyncSettings();
    return 1;
  }
  if (row == 5) {
    uint8_t value = linkClientGetAudioCompensationMillis(chipnomadState->linkClient);
    if (!edit8noLast(action, &value, 1, 0, linkAudioMaxCompensationMillis)) return 0;
    if (!linkClientSetAudioCompensationMillis(chipnomadState->linkClient, value))
      screenMessage(MESSAGE_TIME_ERROR, "Invalid compensation");
    return 1;
  }
  uint8_t enabled = row == 2 ? linkClientIsAudioEnabled(chipnomadState->linkClient) :
    row == 3 ? linkClientIsAudioReceiveEnabled(chipnomadState->linkClient) :
    audioManager.getLocalOutputEnabled();
  if (!editToggle(action, &enabled)) return 0;
  if (row == 2) {
    if (!linkClientSetAudioEnabled(chipnomadState->linkClient, enabled))
      screenMessage(MESSAGE_TIME_ERROR, "Link Audio unavailable");
  } else if (row == 3) {
    if (!linkClientSetAudioReceiveEnabled(chipnomadState->linkClient, enabled))
      screenMessage(MESSAGE_TIME_ERROR, "Link Audio unavailable");
  } else {
    audioManager.setLocalOutputEnabled(enabled);
  }
  return 1;
}

void syncOutStatic(void) {
  title("SYNC OUT");
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrint(0, 6, "Select output in Settings > MIDI.");
  if (appSettings.clockSource == ClockSource::midiClock)
    gfxPrint(0, 8, "Suppressed while Sync is MIDI.");
}
void syncOutField(int, int row, CellState state) {
  field(row, row == 0 ? "MIDI clock" : "MIDI transport",
        (row == 0 ? appSettings.midiSendClock : appSettings.midiSendTransport) ? "On" : "Off", state);
}
int syncOutEdit(int, int row, CellEditAction action) {
  uint8_t* value = row == 0 ? &appSettings.midiSendClock : &appSettings.midiSendTransport;
  if (!editToggle(action, value)) return 0;
  midiRouterSetSyncOutput(chipnomadState->midiRouter,
                         appSettings.midiSendClock, appSettings.midiSendTransport);
  saveSyncSettings();
  return 1;
}

void tuningRedraw(void) { screenFullRedraw(&tuningData); }
void linkRedraw(void) { screenFullRedraw(&linkData); }
void syncOutRedraw(void) { screenFullRedraw(&syncOutData); }
void draw(void) {}
void linkDraw(void) {
#if defined(DESKTOP_BUILD) || defined(PORTMASTER_BUILD)
  gfxClearRect(0, 10, 34, 1);
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrintf(0, 10, "Input: %s", linkClientAudioReceiveStatus(chipnomadState->linkClient));
#endif
}

int menuInput(ScreenData* data, const AppScreen* parent, int down, int keys, int taps) {
  if (keys == keyOpt) {
    screenSetup(parent, 0);
    return 1;
  }
  return screenInput(data, down, keys, taps);
}
int tuningInput(int down, int keys, int taps) { return menuInput(&tuningData, &screenProject, down, keys, taps); }
int linkInput(int down, int keys, int taps) { return menuInput(&linkData, &screenProject, down, keys, taps); }
int syncOutInput(int down, int keys, int taps) { return menuInput(&syncOutData, &screenProject, down, keys, taps); }

ScreenData tuningData = {2, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::song, columnCount, tuningStatic, cursor, nullptr, noHeader, noHeader, tuningField, tuningEdit, nullptr, nullptr, nullptr, nullptr, submenuButton};
#if defined(DESKTOP_BUILD) || defined(PORTMASTER_BUILD)
constexpr int linkRows = 6;
#else
constexpr int linkRows = 1;
#endif
ScreenData linkData = {linkRows, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::song, columnCount, linkStatic, linkCursor, nullptr, noHeader, noHeader, linkField, linkEdit, nullptr, nullptr, linkCellValid, nullptr};
ScreenData syncOutData = {2, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::song, columnCount, syncOutStatic, cursor, nullptr, noHeader, noHeader, syncOutField, syncOutEdit, nullptr, nullptr, nullptr, nullptr};

} // namespace

const AppScreen screenProjectTuning = {nullptr, setup, tuningRedraw, draw, tuningInput, playbackLevel};
const AppScreen screenLinkOptions = {nullptr, setup, linkRedraw, linkDraw, linkInput, playbackLevel};
const AppScreen screenSyncOut = {nullptr, setup, syncOutRedraw, draw, syncOutInput, playbackLevel};
