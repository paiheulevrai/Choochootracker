#include "screen_project.h"
#include "audio_manager.h"
#include "corelib_gfx.h"
#include "link/link_client.h"
#include "midi/midi_router.h"
#include "pitch_table_utils.h"

namespace {

extern ScreenData tuningData, linkData, linkAudioData, syncOutData;

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

void field(int row, const char* label, const char* value, CellState state) {
  gfxClearRect(0, row + 2, 18, 1);
  gfxSetFgColor(appSettings.colorScheme.textLabel);
  gfxPrint(0, row + 2, label);
  gfxClearRect(18, row + 2, 16, 1);
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue :
                appSettings.colorScheme.textDefault);
  gfxPrintf(18, row + 2, "%.16s", value);
}

void cursor(int, int row) {
  if (currentScreen == &screenLinkOptions && row == 1) gfxCursor(0, row + 2, 10);
  else gfxCursor(18, row + 2, 3);
}
int submenuButton(int, int row) { return row == 1; }

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
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrint(0, 6, "Start/stop is shared both ways.");
}
void linkField(int, int row, CellState state) {
  field(row, row == 0 ? "Link start/stop" : "Link Audio",
        row == 0 ? (appSettings.linkTransport ? "On" : "Off") : "", state);
}
int linkEdit(int, int row, CellEditAction action) {
  if (row == 1) {
#if defined(DESKTOP_BUILD) || defined(PORTMASTER_BUILD)
    if (action == CellEditAction::tap || action == CellEditAction::doubleTap)
      screenSetup(&screenLinkAudio, 0);
#endif
    return 0;
  }
  if (!editToggle(action, &appSettings.linkTransport)) return 0;
  linkClientSetTransportSync(chipnomadState->linkClient, appSettings.linkTransport);
  saveSyncSettings();
  return 1;
}

void linkAudioStatic(void) {
  title("LINK AUDIO");
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrint(0, 6, "Stereo Link Audio (prototype)");
  gfxPrint(0, 7, "Peer: ChooChooTracker");
  gfxPrint(0, 8, "Channel: Master");
  gfxPrint(0, 10, "Receive: one remote CCT Master.");
  gfxPrint(0, 11, "Receive delay: 100 ms.");
  gfxPrint(0, 12, "Received audio is never sent.");
  gfxPrint(0, 13, "Local audio only mutes speakers.");
  gfxPrint(0, 14, "Options reset on restart.");
  if (!linkClientIsEnabled(chipnomadState->linkClient))
    gfxPrint(0, 16, "Connect Link on Project first.");
}
void linkAudioField(int, int row, CellState state) {
  const bool enabled = row == 0 ? linkClientIsAudioEnabled(chipnomadState->linkClient) :
    row == 1 ? linkClientIsAudioReceiveEnabled(chipnomadState->linkClient) :
    audioManager.getLocalOutputEnabled();
  field(row, row == 0 ? "Send" : row == 1 ? "Receive" : "Local audio", enabled ? "On" : "Off", state);
}
int linkAudioEdit(int, int row, CellEditAction action) {
  uint8_t enabled = row == 0 ? linkClientIsAudioEnabled(chipnomadState->linkClient) :
    row == 1 ? linkClientIsAudioReceiveEnabled(chipnomadState->linkClient) :
    audioManager.getLocalOutputEnabled();
  if (!editToggle(action, &enabled)) return 0;
  if (row == 0) {
    if (!linkClientSetAudioEnabled(chipnomadState->linkClient, enabled))
      screenMessage(MESSAGE_TIME_ERROR, "Link Audio unavailable");
  } else if (row == 1) {
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
void linkAudioRedraw(void) { screenFullRedraw(&linkAudioData); }
void syncOutRedraw(void) { screenFullRedraw(&syncOutData); }
void draw(void) {}
void linkAudioDraw(void) {
  gfxClearRect(0, 9, 34, 1);
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxPrintf(0, 9, "Input: %s", linkClientAudioReceiveStatus(chipnomadState->linkClient));
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
int linkAudioInput(int down, int keys, int taps) { return menuInput(&linkAudioData, &screenLinkOptions, down, keys, taps); }
int syncOutInput(int down, int keys, int taps) { return menuInput(&syncOutData, &screenProject, down, keys, taps); }

ScreenData tuningData = {2, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::song, columnCount, tuningStatic, cursor, nullptr, noHeader, noHeader, tuningField, tuningEdit, nullptr, nullptr, nullptr, nullptr, submenuButton};
#if defined(DESKTOP_BUILD) || defined(PORTMASTER_BUILD)
constexpr int linkRows = 2;
#else
constexpr int linkRows = 1;
#endif
ScreenData linkData = {linkRows, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::song, columnCount, linkStatic, cursor, nullptr, noHeader, noHeader, linkField, linkEdit, nullptr, nullptr, nullptr, nullptr, submenuButton};
ScreenData linkAudioData = {3, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::song, columnCount, linkAudioStatic, cursor, nullptr, noHeader, noHeader, linkAudioField, linkAudioEdit, nullptr, nullptr, nullptr, nullptr};
ScreenData syncOutData = {2, 0, 0, 0, -1, 0, 0, 0, 0, ScreenPlaybackLevel::song, columnCount, syncOutStatic, cursor, nullptr, noHeader, noHeader, syncOutField, syncOutEdit, nullptr, nullptr, nullptr, nullptr};

} // namespace

const AppScreen screenProjectTuning = {nullptr, setup, tuningRedraw, draw, tuningInput, playbackLevel};
const AppScreen screenLinkOptions = {nullptr, setup, linkRedraw, draw, linkInput, playbackLevel};
const AppScreen screenLinkAudio = {nullptr, setup, linkAudioRedraw, linkAudioDraw, linkAudioInput, playbackLevel};
const AppScreen screenSyncOut = {nullptr, setup, syncOutRedraw, draw, syncOutInput, playbackLevel};
