#include "doctest.h"
#include "common.h"
#include "app.h"
#include "app_ui_mock.h"
#include "chipnomad_lib_live_stick.h"
#include "corelib_file.h"
#include "corelib_gfx.h"
#include "corelib_audio.h"
#include "project_utils.h"
#include "screen_audio_input.h"
#include "midi/midi_router.h"
#include "audio_manager.h"
#include "selection_popup.h"
#include "screen_instrument.h"

#include <chrono>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <string>

extern int mockAudioOutputDeviceCount;
extern char mockSelectedAudioOutput[128];
namespace {
const InputCode keyboardLive = {InputDeviceType::keyboard, 1001};
const InputCode gamepadLive = {InputDeviceType::gamepad, 1002};
const InputCode logicalLive = {InputDeviceType::logical, keyMotionLive};

void input(InputCode code, bool down) {
  MainLoopEventData event = {};
  event.type = down ? MainLoopEvent::keyDown : MainLoopEvent::keyUp;
  event.data.input = code;
  appOnEvent(event);
}
void motion(int key, bool down) { input({InputDeviceType::logical, key}, down); }
char indicator() {
  // Isolate the status indicator from waveform rendering (audio is mocked).
  int tracks = chipnomadState->project.tracksCount;
  chipnomadState->project.tracksCount = 0;
  appDraw();
  chipnomadState->project.tracksCount = tracks;
  return mockGfxCells[19][39];
}

struct MidiButtonInput {
  std::deque<uint8_t> values;
  MidiBackend backend{};
  MidiButtonInput() {
    backend.userdata = &values;
    backend.pollInput = [](void* userdata, MidiEvent* event) {
      auto& pending = *static_cast<std::deque<uint8_t>*>(userdata);
      if (pending.empty()) return 0;
      *event = {0, 0xb0, 0, 41, pending.front()};
      pending.pop_front();
      return 1;
    };
    backend.scheduleOutput = [](void*, const MidiEvent*, uint64_t) {};
    midiRouterSetBackend(&backend);
  }
  ~MidiButtonInput() { midiRouterSetBackend(nullptr); }
};

struct StickLiveFixture {
  AppSettings savedSettings = appSettings;
  ChipNomadState* savedState = chipnomadState;
  std::filesystem::path originalPath = std::filesystem::current_path();
  std::filesystem::path testPath = std::filesystem::temp_directory_path() /
    ("choochootracker-stick-live-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  // settingsSave()/settingsLoad() resolve their path from the running
  // executable's own directory (see corelib_file.cpp), not the process cwd,
  // so chdir'ing into testPath does not isolate settings.txt between test
  // cases. Track and clean up the real resolved path directly instead.
  std::string realSettingsPath = [] {
    char defaultDir[PATH_LENGTH];
    if (fileGetDefaultDirectory(defaultDir, sizeof(defaultDir)) != 0) return std::string();
    return std::string(defaultDir) + "/settings.txt";
  }();

  StickLiveFixture() {
    std::filesystem::create_directories(testPath);
    std::filesystem::current_path(testPath);
    char settingsDirectory[PATH_LENGTH];
    if (fileGetDefaultDirectory(settingsDirectory, sizeof(settingsDirectory)) == 0)
      realSettingsPath = std::string(settingsDirectory) + "/settings.txt";
    if (!realSettingsPath.empty()) std::filesystem::remove(realSettingsPath);
    initDefaultAppSettings();
    mockAudioReconfigureResult = 0;
    mockAudioReconfigureCalls = 0;
    mockAudioReconfigureRate = 0;
    mockAudioReconfigureSize = 0;
    mockAudioCpuLoad = 0;
    mockAudioOutputDeviceCount = 0;
    mockAudioRestartResult = 0;
    mockAudioRestartCalls = 0;
    mockAudioOutputNames.clear();
    mockAudioOutputAttempts.clear();
    mockAudioRestartResults.clear();
    mockOpenedAudioOutput.clear();
    mockAudioOutputOpen = true;
    screenClearMessage();
    mockAudioRefreshResult = 0;
    mockAudioRefreshCalls = 0;
    mockSelectedAudioOutput[0] = 0;
    appSettings.keyMapping.keyMotionLive[0] = keyboardLive;
    appSettings.keyMapping.keyMotionLive[1] = gamepadLive;
    inputRawCallback = nullptr;
    appSetup();
    appActivateProject();
    currentScreen = &screenSong;
    // The spine routes Project/Settings + SHIFT+RIGHT to the Chain screen
    // only when the Song cursor sits on a non-empty cell (an empty cell
    // lands on Song instead — the chain screen would show garbage). Give
    // every song row a chain so the classic spine holds.
    for (int row = 0; row < PROJECT_MAX_LENGTH; ++row)
      for (int track = 0; track < PROJECT_MAX_TRACKS; ++track)
        chipnomadState->project.song[row][track] = 0;
  }
  ~StickLiveFixture() {
    inputRawCallback = nullptr;
    appCleanup();
    mockAudioOutputNames.clear();
    mockAudioOutputAttempts.clear();
    mockAudioRestartResults.clear();
    mockAudioOutputDeviceCount = 0;
    mockAudioRestartResult = 0;
    chipnomadState = savedState;
    appSettings = savedSettings;
    chipnomadSetLiveStickEnabled(0);
    chipnomadSetMotionRecordMode(0, 0);
    std::filesystem::current_path(originalPath);
    std::filesystem::remove_all(testPath);
    if (!realSettingsPath.empty()) std::filesystem::remove(realSettingsPath);
  }
};

struct MidiNoteInput {
  std::deque<MidiEvent> events;
  MidiBackend backend{};
  uint64_t now = 1000000;
  MidiNoteInput() {
    backend.userdata = this;
    backend.pollInput = [](void* data, MidiEvent* event) {
      auto& pending = static_cast<MidiNoteInput*>(data)->events;
      if (pending.empty()) return 0;
      *event = pending.front();
      pending.pop_front();
      return 1;
    };
    backend.nowMicros = [](void* data) { return static_cast<MidiNoteInput*>(data)->now; };
    backend.scheduleOutput = [](void*, const MidiEvent*, uint64_t) {};
    midiRouterSetBackend(&backend);
  }
  ~MidiNoteInput() { midiRouterSetBackend(nullptr); }
};
}

TEST_CASE_FIXTURE(StickLiveFixture, "Shift horizontal navigation exits branch screens to the spine") {
  struct Route { const AppScreen *from, *left, *right; };
  const Route routes[] = {
    {&screenProject,&screenMixer,&screenChain},
    {&screenSettings,&screenMixer,&screenChain},
    {&screenControlsSettings,&screenMixer,&screenChain},
    {&screenAudioSettings,&screenMixer,&screenChain},
    {&screenSynthSettings,&screenMixer,&screenChain},
    {&screenMixerSettings,&screenMixer,&screenChain},
    {&screenGraphicsSettings,&screenMixer,&screenChain},
    {&screenBackgroundSettings,&screenMixer,&screenChain},
    {&screenTrackVisuals,&screenMixer,&screenChain},
    {&screenMidi,&screenMixer,&screenChain},
    {&screenMidiChannelMap,&screenMixer,&screenChain},
    {&screenMidiCC,&screenMixer,&screenChain},
    {&screenGroove,&screenChain,&screenInstrument},
    {&screenModulation,&screenPhrase,&screenTable},
    {&screenInsertFX,&screenPhrase,&screenTable},
    {&screenAYWavetable,&screenInstrument,&screenTable},
  };
  for (const auto& route : routes) for (int direction : {keyLeft,keyRight}) {
    if (route.from == &screenInsertFX) screenInsertFXOpenTrack();
    else screenSetup(route.from, 0);
    appDraw();
    motion(keyShift,true);
    motion(direction,true);
    const auto* destination = direction == keyLeft ? route.left : route.right;
    appDraw();
    CHECK(currentScreen == destination);
    motion(direction,false);
    CHECK(currentScreen == destination); // releasing direction while still holding Shift
    motion(keyShift,false);
    CHECK(currentScreen == destination);
  }
}

TEST_SUITE("Stick live") {
TEST_CASE_FIXTURE(StickLiveFixture, "MIDI Auto Channel plays the phrase instrument with REC off") {
  MidiNoteInput midi;
  int savedInstrument = cInstrument;
  auto savedPhraseInstrument = mockPhraseMidiInstrument;
  cInstrument = 1;
  mockPhraseMidiInstrument = 2;
  auto& project = chipnomadState->project;
  projectInitAY(&project);
  project.instruments[1].type = project.instruments[2].type = InstrumentType::AY1;
  chipnomadInitChips(chipnomadState, 48000, nullptr);
  REQUIRE(chipnomadQueueProjectRefresh(chipnomadState));
  float audio[4096]{};
  chipnomadRender(chipnomadState, audio, 2048);
  *pSongTrack = 0;
  currentScreen = &screenPhrase;
  projectModified = 0;
  midi.events.push_back({1000, 0x90, 15, 60, 100});
  appOnEvent({MainLoopEvent::tick});
  chipnomadRender(chipnomadState, audio, 2048);
  CHECK(chipnomadState->playbackState.tracks[0].note.instrument == 2);
  CHECK(chipnomadState->playbackState.tracks[0].note.pitchBase == 48);
  CHECK(projectModified == 0);
  MidiRecordEvent recorded{};
  CHECK_FALSE(chipnomadConsumeMidiRecordEvent(chipnomadState, &recorded));
  midi.events.push_back({2000, 0x80, 15, 60, 0});
  appOnEvent({MainLoopEvent::tick});
  chipnomadRender(chipnomadState, audio, 2048);
  currentScreen = &screenInstrument;
  midi.events.push_back({3000, 0x90, 15, 64, 90});
  appOnEvent({MainLoopEvent::tick});
  chipnomadRender(chipnomadState, audio, 2048);
  CHECK(chipnomadState->playbackState.tracks[0].note.instrument == 1);
  CHECK(chipnomadState->playbackState.tracks[0].note.pitchBase == 52);
  CHECK(projectModified == 0);
  auto play = [&](const AppScreen* screen, int expectedInstrument) {
    currentScreen = screen;
    midi.events.push_back({4000, 0x90, 15, 67, 90});
    appOnEvent({MainLoopEvent::tick});
    chipnomadRender(chipnomadState, audio, 2048);
    CHECK(chipnomadState->playbackState.tracks[0].note.instrument == expectedInstrument);
  };
  cInstrument = 2;
  play(&screenModulation, 2);
  cInstrument = 1;
  play(&screenMixer, 2);
  mockTableMidiInstrument = 1;
  play(&screenTable, 1);
  play(&screenMixer, 1);
  mockPoolMidiInstrument = 2;
  play(&screenInstrumentPool, 2);
  mockPoolMidiInstrument = 1;
  play(&screenInstrumentPool, 1);
  play(&screenMixer, 1);
  mockTableMidiInstrument = mockPoolMidiInstrument = EMPTY_VALUE_8;
  cInstrument = savedInstrument;
  mockPhraseMidiInstrument = savedPhraseInstrument;
}

TEST_CASE_FIXTURE(StickLiveFixture, "MIDI REC records only the final release on its original track") {
  MidiNoteInput midi;
  auto savedPhraseInstrument = mockPhraseMidiInstrument;
  mockPhraseMidiInstrument = 0;
  auto& project = chipnomadState->project;
  projectInitAY(&project);
  project.tickRate = 48;
  project.instruments[0].type = InstrumentType::AY1;
  project.song[0][0] = 0;
  project.chains[0].rows[0].phrase = 0;
  project.phrases[0].rows[0].instrument = 0;
  chipnomadInitChips(chipnomadState, 48000, nullptr);
  playbackInit(&chipnomadState->playbackState, &chipnomadState->audioProject);
  REQUIRE(chipnomadQueuePlaybackStartSong(chipnomadState, 0, 0, 1));
  float audio[2000]{};
  auto render = [&] {
    chipnomadRender(chipnomadState, audio, 1000);
    midi.now += 20833;
  };
  render();
  REQUIRE(chipnomadGetPlaybackStatus(chipnomadState)->tracks[0].mode == PlaybackMode::song);
  currentScreen = &screenPhrase;
  *pSongTrack = 0;
  motion(keyMotionRecord, true);
  REQUIRE(chipnomadMotionMode() == 1);
  auto send = [&](uint8_t type, uint8_t note, uint8_t velocity) {
    midi.events.push_back({midi.now, type, 15, note, velocity});
    appOnEvent({MainLoopEvent::tick});
    render();
  };
  MidiRecordEvent recorded{};
  send(0x90, 60, 100);
  INFO("MIDI instrument: ", chipnomadState->playbackState.tracks[0].note.instrument,
    ", mode: ", (int)chipnomadGetPlaybackStatus(chipnomadState)->tracks[0].mode,
    ", record overflow: ", chipnomadGetMidiRecordOverflow(chipnomadState));
  REQUIRE(chipnomadConsumeMidiRecordEvent(chipnomadState, &recorded));
  CHECK(recorded.note == 48);
  send(0x90, 64, 90);
  REQUIRE(chipnomadConsumeMidiRecordEvent(chipnomadState, &recorded));
  CHECK(recorded.note == 52);
  send(0x80, 64, 0);
  CHECK_FALSE(chipnomadConsumeMidiRecordEvent(chipnomadState, &recorded));
  for (int i = 0; i < 6; ++i) render();
  *pSongTrack = 1;
  send(0x80, 60, 0);
  appOnEvent({MainLoopEvent::tick});
  CHECK(project.phrases[0].rows[1].note == NOTE_OFF);
  CHECK(project.phrases[0].rows[1].instrument == EMPTY_VALUE_8);
  CHECK(project.phrases[0].rows[1].volume == EMPTY_VALUE_16);
  CHECK(projectModified);
  motion(keyMotionRecord, false);
  mockPhraseMidiInstrument = savedPhraseInstrument;
}

TEST_CASE_FIXTURE(StickLiveFixture, "MIDI Auto Channel and recording mode persist and reject invalid settings") {
  CHECK(appSettings.midiAutoChannel == 16);
  CHECK(appSettings.midiRecordMode == MidiRecordMode::quantized);
  appSettings.midiAutoChannel = 12;
  appSettings.midiRecordMode = MidiRecordMode::free;
  REQUIRE(settingsSave() == 0);
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.midiAutoChannel == 12);
  CHECK(appSettings.midiRecordMode == MidiRecordMode::free);
  std::ofstream(realSettingsPath) << "midiAutoChannel: 17\nmidiRecordMode: INVALID\n";
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.midiAutoChannel == 16);
  CHECK(appSettings.midiRecordMode == MidiRecordMode::quantized);
  std::ofstream(realSettingsPath) << "midiAutoChannel: 0\nmidiRecordMode: FREE\n";
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.midiAutoChannel == 0);
  CHECK(appSettings.midiRecordMode == MidiRecordMode::free);
}

TEST_CASE_FIXTURE(StickLiveFixture, "MIDI menu exposes saved Auto Channel and Quantized Free controls") {
  screenMidi.fullRedraw();
  REQUIRE(mockScreenData);
  auto* screen = mockScreenData;
  CHECK(screen->rows == 9);
  screen->drawField(0, 7, CellState::focus);
  CHECK(std::string(mockGfxCells[12], 12) == "Auto channel");
  screen->drawField(0, 8, CellState::focus);
  CHECK(std::string(mockGfxCells[13] + 16, 9) == "QUANTIZED");
  CHECK(screen->onEdit(0, 8, CellEditAction::tap));
  CHECK(appSettings.midiRecordMode == MidiRecordMode::free);
  CHECK(screen->onEdit(0, 7, CellEditAction::decrease));
  CHECK(appSettings.midiAutoChannel == 15);
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.midiAutoChannel == 15);
  CHECK(appSettings.midiRecordMode == MidiRecordMode::free);
}

TEST_CASE_FIXTURE(StickLiveFixture, "FM motion records one native command without a selector") {
  auto& row=chipnomadState->project.phrases[0].rows[0];
  phraseClear(&chipnomadState->project.phrases[0]);
  REQUIRE(chipnomadMotionPushEvent({0,0,fxOMU,7,0}));
  appOnEvent({MainLoopEvent::tick});
  CHECK(row.fx[2][0]==fxOMU);CHECK(row.fx[2][1]==7);
  CHECK(row.fx[0][0]==EMPTY_VALUE_8);CHECK(row.fx[1][0]==EMPTY_VALUE_8);
  REQUIRE(chipnomadMotionPushEvent({0,0,fxOMU,9,0}));
  appOnEvent({MainLoopEvent::tick});CHECK(row.fx[2][1]==9);
  REQUIRE(chipnomadMotionPushEvent({0,0,fxOMU,0,1}));
  appOnEvent({MainLoopEvent::tick});CHECK(row.fx[2][0]==EMPTY_VALUE_8);
}

TEST_CASE_FIXTURE(StickLiveFixture, "HOLD and TOGGLE use keyboard, gamepad and logical presses") {
  for (InputCode code : {keyboardLive, gamepadLive, logicalLive}) {
    appSetStickLiveMode(StickLiveMode::hold);
    CHECK_FALSE(chipnomadLiveStickIsEnabled());
    input(code, true);
    CHECK(chipnomadLiveStickIsEnabled());
    CHECK(indicator() == '~');
    input(code, true);
    input(code, false);
    CHECK_FALSE(chipnomadLiveStickIsEnabled());
    CHECK(indicator() == ' ');

    appSetStickLiveMode(StickLiveMode::toggle);
    input(code, true);
    input(code, true); // repeat/duplicate down
    CHECK(chipnomadLiveStickIsEnabled());
    input(code, false);
    input(code, false); // duplicate up
    CHECK(chipnomadLiveStickIsEnabled());
    CHECK(indicator() == '~');
    input(code, true);
    input(code, true);
    CHECK_FALSE(chipnomadLiveStickIsEnabled());
    input(code, false);
    CHECK(indicator() == ' ');
  }
}

TEST_CASE_FIXTURE(StickLiveFixture, "overlapping input delivery toggles only once until all sources release") {
  appSetStickLiveMode(StickLiveMode::toggle);
  input(keyboardLive, true);
  input(gamepadLive, true);
  input(logicalLive, true);
  CHECK(chipnomadLiveStickIsEnabled());
  input(keyboardLive, false);
  input(gamepadLive, true); // still held after the keyboard duplicate released
  input(gamepadLive, false);
  input(logicalLive, false);
  CHECK(chipnomadLiveStickIsEnabled());
  input(gamepadLive, true);
  CHECK_FALSE(chipnomadLiveStickIsEnabled());
  input(gamepadLive, false);
}

TEST_CASE_FIXTURE(StickLiveFixture, "PortMaster controller duplicate cannot stick after leaving key mapping") {
  const InputCode controllerDuplicate = {InputDeviceType::gamepad, 9001};
  const InputCode keyboardUp = {InputDeviceType::keyboard, 9002};
  appSettings.keyMapping.keyUp[0] = keyboardUp;

  // Knulli/PortMaster can emit an unmapped SDL controller event alongside a
  // gptokeyb key. Before the fix it was recorded on Key Mapping, but its
  // release was ignored after switching to Settings, poisoning later input.
  currentScreen = &screenKeyMapping;
  input(controllerDuplicate, true);
  currentScreen = &screenSong;
  input(controllerDuplicate, false);

  mockLastInputKeys = 0;
  input(keyboardUp, true);
  CHECK(mockLastInputKeys == keyUp);
  input(keyboardUp, false);
}

TEST_CASE_FIXTURE(StickLiveFixture, "FREE keeps Stick live active regardless of input") {
  appSetStickLiveMode(StickLiveMode::free);
  CHECK(chipnomadLiveStickIsEnabled());
  CHECK(indicator() == '~');
  for (InputCode code : {keyboardLive, gamepadLive, logicalLive}) {
    input(code, true);
    input(code, false);
    CHECK(chipnomadLiveStickIsEnabled());
  }
  motion(keyMotionRecord, true);
  CHECK(indicator() == '*');
  motion(keyMotionRecord, false);
  CHECK(chipnomadLiveStickIsEnabled());
  CHECK(indicator() == '~');
  appSetStickLiveMode(StickLiveMode::toggle);
  CHECK_FALSE(chipnomadLiveStickIsEnabled());
}

TEST_CASE_FIXTURE(StickLiveFixture, "mode changes clear the latch and reconcile held input") {
  appSetStickLiveMode(StickLiveMode::toggle);
  input(keyboardLive, true);
  input(keyboardLive, false);
  appSetStickLiveMode(StickLiveMode::hold);
  CHECK_FALSE(chipnomadLiveStickIsEnabled());
  input(keyboardLive, true);
  appSetStickLiveMode(StickLiveMode::toggle);
  CHECK_FALSE(chipnomadLiveStickIsEnabled());
  input(keyboardLive, true); // a repeat after switching is not a new press
  CHECK_FALSE(chipnomadLiveStickIsEnabled());
  input(keyboardLive, false);
  input(keyboardLive, true);
  CHECK(chipnomadLiveStickIsEnabled());
  appSetStickLiveMode(StickLiveMode::hold);
  CHECK(chipnomadLiveStickIsEnabled()); // still physically held
  input(keyboardLive, false);
  CHECK_FALSE(chipnomadLiveStickIsEnabled());
  appSetStickLiveMode(StickLiveMode::toggle);
  CHECK_FALSE(chipnomadLiveStickIsEnabled());
}

TEST_CASE_FIXTURE(StickLiveFixture, "Record and Erase remain momentary with indicator priority") {
  for (StickLiveMode mode : {StickLiveMode::hold, StickLiveMode::toggle, StickLiveMode::free}) {
    appSetStickLiveMode(mode);
    for (int key : {keyMotionRecord, keyMotionErase}) {
      motion(key, true);
      CHECK(chipnomadLiveStickIsEnabled());
      CHECK(chipnomadMotionMode() == (key == keyMotionRecord ? 1 : 2));
      CHECK(indicator() == (key == keyMotionRecord ? '*' : 'x'));
      motion(key, false);
      CHECK(chipnomadLiveStickIsEnabled() == (mode == StickLiveMode::free));
      CHECK(chipnomadMotionMode() == 0);
    }
    input(keyboardLive, true);
    if (mode == StickLiveMode::toggle) input(keyboardLive, false);
    motion(keyMotionRecord, true);
    CHECK(indicator() == '*');
    chipnomadSetMotionRecordOverflow();
    CHECK(indicator() == '!');
    motion(keyMotionErase, true);
    CHECK(indicator() == 'x');
    motion(keyMotionErase, false);
    CHECK(indicator() == '!');
    motion(keyMotionRecord, false);
    CHECK(chipnomadLiveStickIsEnabled());
    CHECK(indicator() == '~');
    input(keyboardLive, false);
  }
}

TEST_CASE_FIXTURE(StickLiveFixture, "mode changes and toggling off respect Record and Erase") {
  for (int key : {keyMotionRecord, keyMotionErase}) {
    appSetStickLiveMode(StickLiveMode::hold);
    motion(key, true);
    appSetStickLiveMode(StickLiveMode::toggle);
    CHECK(chipnomadLiveStickIsEnabled());
    input(keyboardLive, true);
    input(keyboardLive, false);
    input(keyboardLive, true); // toggle off while recording/erasing
    input(keyboardLive, false);
    CHECK(chipnomadLiveStickIsEnabled());
    motion(key, false);
    CHECK_FALSE(chipnomadLiveStickIsEnabled());
    motion(key, true);
    input(keyboardLive, true);
    input(keyboardLive, false);
    appSetStickLiveMode(StickLiveMode::hold); // clear a live latch during Record/Erase
    CHECK(chipnomadLiveStickIsEnabled());
    motion(key, false);
    CHECK_FALSE(chipnomadLiveStickIsEnabled());
  }
}

TEST_CASE_FIXTURE(StickLiveFixture, "key mapping capture suppresses live actions even if callback clears itself") {
  appSetStickLiveMode(StickLiveMode::toggle);
  for (InputCode code : {keyboardLive, gamepadLive, logicalLive}) {
    inputRawCallback = [](InputCode, int) { inputRawCallback = nullptr; };
    input(code, true);
    CHECK_FALSE(chipnomadLiveStickIsEnabled());
    input(code, false);
    CHECK_FALSE(chipnomadLiveStickIsEnabled());
  }
  input(keyboardLive, true);
  input(keyboardLive, false);
  inputRawCallback = [](InputCode, int) {};
  input(keyboardLive, true);
  input(keyboardLive, false);
  CHECK(chipnomadLiveStickIsEnabled());
}

TEST_CASE_FIXTURE(StickLiveFixture, "normal exit saves mode but restart clears the active latch") {
  appSetStickLiveMode(StickLiveMode::toggle);
  input(keyboardLive, true);
  input(keyboardLive, false);
  appOnEvent({MainLoopEvent::exit, {}});
  appCleanup();
  chipnomadState = nullptr;
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.stickLiveMode == StickLiveMode::toggle);
  appSetup();
  appActivateProject();
  currentScreen = &screenSong;
  CHECK_FALSE(chipnomadLiveStickIsEnabled());
  CHECK(indicator() == ' ');
  appSetStickLiveMode(StickLiveMode::hold);
  REQUIRE(settingsSave() == 0);
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.stickLiveMode == StickLiveMode::hold);
  appSetStickLiveMode(StickLiveMode::free);
  REQUIRE(settingsSave() == 0);
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.stickLiveMode == StickLiveMode::free);
}

TEST_CASE_FIXTURE(StickLiveFixture, "absent, older and invalid settings default to HOLD") {
  REQUIRE(settingsLoad() != 0); // no settings file yet
  CHECK(appSettings.stickLiveMode == StickLiveMode::hold);
  for (const char* content : {"screenWidth: 640\n", "stickLiveMode: INVALID\n",
       "stickLiveMode: 1\n", "stickLiveMode: TOGGLEjunk\n", "stickLiveMode: \n"}) {
    appSettings.stickLiveMode = StickLiveMode::toggle;
    std::ofstream(realSettingsPath) << content;
    REQUIRE(settingsLoad() == 0);
    CHECK(appSettings.stickLiveMode == StickLiveMode::hold);
  }
}

TEST_CASE_FIXTURE(StickLiveFixture, "Settings menus and audio controls align") {
    REQUIRE(screenSettings.draw != nullptr);
    REQUIRE(screenSettings.setup != nullptr);
    REQUIRE(screenControlsSettings.draw != nullptr);
    REQUIRE(screenAudioSettings.draw != nullptr);
    REQUIRE(screenSynthSettings.draw != nullptr);
    REQUIRE(screenSynthSettings.setup != nullptr);
    REQUIRE(screenMixerSettings.draw != nullptr);
    REQUIRE(screenMixerSettings.setup != nullptr);
    REQUIRE(screenGraphicsSettings.draw != nullptr);
    REQUIRE(screenGraphicsSettings.setup != nullptr);
  screenSettings.fullRedraw();
  REQUIRE(mockScreenData != nullptr);
  CHECK(mockScreenData->rows == 8);
  auto* screen = mockScreenData;
  const char* labels[] = {"Controls", "Audio", "MIDI", "Synths", "Mixer", "Graphics", "Quit ChooChooTracker"};
  const int lines[] = {2, 3, 4, 5, 6, 7, 18};
  const int widths[] = {8, 5, 4, 6, 5, 8, 19};
  const AppScreen* destinations[] = {&screenControlsSettings, &screenAudioSettings, &screenMidi, &screenSynthSettings, &screenMixerSettings, &screenGraphicsSettings};
  mockQuitTriggered = 0;
  for (int i = 0; i < 7; ++i) {
    int row = i < 6 ? i : 7;
    screen->drawField(0, row, CellState::focus);
    CHECK(std::string(mockGfxCells[lines[i]], std::string(labels[i]).size()) == labels[i]);
    screen->drawCursor(0, row);
    CHECK(mockCursorX == 0);
    CHECK(mockCursorY == lines[i]);
    CHECK(mockCursorWidth == widths[i]);
    screen->onEdit(0, row, CellEditAction::tap);
    if (i < 6) {
      CHECK(currentScreen == destinations[i]);
      CHECK_FALSE(mockQuitTriggered);
    }
  }
  CHECK(mockQuitTriggered);

  screenControlsSettings.fullRedraw();
  screen = mockScreenData;
  CHECK(screen->rows == 4);
  gfxClear();
  screen->drawStatic();
  for (int row = 0; row < screen->rows; ++row) screen->drawField(0, row, CellState::normal);
  CHECK(std::string(mockGfxCells[2], 11) == "Keys repeat");
  CHECK(std::string(mockGfxCells[3], 15) == " - Repeat delay");
  CHECK(std::string(mockGfxCells[4], 15) == " - Repeat speed");
  CHECK(std::string(mockGfxCells[5], 31) == std::string(31, ' '));
  CHECK(std::string(mockGfxCells[7], 31) == std::string(31, ' '));
  CHECK(std::string(mockGfxCells[8], 11) == "Key mapping");
  screen->drawCursor(0, 2);
  CHECK(mockCursorY == 6);
  screen->drawField(0, 2, CellState::focus);
  CHECK(std::string(mockGfxCells[6], 15) == "Stick live mode");
  REQUIRE(screen->onEdit(0, 2, CellEditAction::increase) == 1);
  CHECK(appSettings.stickLiveMode == StickLiveMode::toggle);

  mockAudioCpuLoad = 37;
  screenAudioSettings.fullRedraw();
  screen = mockScreenData;
  CHECK(screen->rows == 6);
  gfxClear();
  screen->drawStatic();
  screen->drawField(0, 0, CellState::focus);
  CHECK(std::string(mockGfxCells[2], 11) == "Sample rate");
  CHECK(std::string(mockGfxCells[2] + 23, 6) == "48 kHz");
  REQUIRE(screen->onEdit(0, 0, CellEditAction::decrease) == 1);
  CHECK(mockAudioReconfigureRate == 44100);
  CHECK(appSettings.audioSampleRate == 44100);
  screen->drawField(0, 0, CellState::focus);
  CHECK(std::string(mockGfxCells[2] + 23, 8) == "44.1 kHz");
  screen->drawCursor(0, 0);
  CHECK(mockCursorWidth == 8);
  REQUIRE(screen->onEdit(0, 0, CellEditAction::decrease) == 1);
  CHECK(mockAudioReconfigureRate == 32000);
  CHECK(appSettings.audioSampleRate == 32000);
  REQUIRE(screen->onEdit(0, 0, CellEditAction::clear) == 1);
  CHECK(appSettings.audioSampleRate == 48000);
  mockAudioReconfigureResult = 1;
  REQUIRE(screen->onEdit(0, 0, CellEditAction::decrease) == 1);
  CHECK(appSettings.audioSampleRate == 48000);
  mockAudioReconfigureResult = 0;
  screen->drawField(0, 1, CellState::focus);
  CHECK(std::string(mockGfxCells[3], 11) == "Buffer size");
  CHECK(std::string(mockGfxCells[3] + 23, 3) == "512");
  appSettings.audioBufferSize = 64;
  screen->drawField(0, 1, CellState::focus);
  CHECK(std::string(mockGfxCells[3] + 23, 4) == "64  ");
  appSettings.audioBufferSize = 2048;
  screen->drawCursor(0, 1);
  CHECK(mockCursorWidth == 4);
  appSettings.audioBufferSize = 256;
  screen->drawField(0, 2, CellState::focus);
  CHECK(std::string(mockGfxCells[6], 16) == "Audio interfaces");
  CHECK(std::string(mockGfxCells[7], 15) == " - Audio Output");
  CHECK(std::string(mockGfxCells[7] + 15, 7) == "Default");
  CHECK(screen->isCellValid(0, 2));
  screen->drawField(0, 3, CellState::normal);
  CHECK(std::string(mockGfxCells[4], 8) == "CPU load");
  CHECK(std::string(mockGfxCells[4] + 23, 4) == "037%");
  CHECK(std::string(mockGfxCells[5], 31) == std::string(31, ' '));
  CHECK_FALSE(screen->isCellValid(0, 3));
  screen->drawField(0, 4, CellState::focus);
  CHECK(std::string(mockGfxCells[8], 14) == " - Audio Input");
  CHECK(std::string(mockGfxCells[8] + 23, 3) == "OFF");
  CHECK(screen->isCellValid(0, 4));
  REQUIRE(screen->onEdit(0, 4, CellEditAction::tap) == 1);
  CHECK(appSettings.audioInputEnabled == 1);
  REQUIRE(screen->onEdit(0, 4, CellEditAction::tap) == 1);
  CHECK(appSettings.audioInputEnabled == 0);
  screen->drawField(0, 5, CellState::focus);
  CHECK(std::string(mockGfxCells[9], 16) == " - Input devices");
  CHECK(screen->isCellValid(0, 5));
  REQUIRE(screen->onEdit(0, 5, CellEditAction::tap) == 1);
  CHECK(currentScreen == &screenAudioInput);
  REQUIRE(screen->onEdit(0, 1, CellEditAction::increase) == 1);
  CHECK(mockAudioReconfigureSize == 512);
  CHECK(appSettings.audioBufferSize == 512);
  REQUIRE(screen->onEdit(0, 1, CellEditAction::clear) == 1);
  CHECK(appSettings.audioBufferSize == 32);
  const int callsAtMinimum = mockAudioReconfigureCalls;
  REQUIRE(screen->onEdit(0, 1, CellEditAction::decrease) == 1);
  CHECK(mockAudioReconfigureCalls == callsAtMinimum);
  mockAudioReconfigureResult = 1;
  REQUIRE(screen->onEdit(0, 1, CellEditAction::increase) == 1);
  CHECK(mockAudioReconfigureSize == 64);
  CHECK(appSettings.audioBufferSize == 32);
}

TEST_CASE_FIXTURE(StickLiveFixture, "audio buffer settings use supported sizes") {
  CHECK(appSettings.audioBufferSize == 512);
  struct StoredSetting { const char* text; int expected; };
  for (const auto& setting : {StoredSetting{"audioBufferSize: 4906\n", 512},
                              StoredSetting{"audioBufferSize: 1500\n", 1024},
                              StoredSetting{"audioBufferSize: 1600\n", 2048},
                              StoredSetting{"audioBufferSize: 2048\n", 2048},
                              StoredSetting{"audioBufferSize: 9000\n", 2048},
                              StoredSetting{"audioBufferSize: 1\n", 32},
                              StoredSetting{"audioBufferSize: 700\n", 512}}) {
    std::ofstream(realSettingsPath) << setting.text;
    REQUIRE(settingsLoad() == 0);
    CHECK(appSettings.audioBufferSize == setting.expected);
  }
}

TEST_CASE_FIXTURE(StickLiveFixture, "audio output selection saves and restores the device") {
  mockAudioOutputDeviceCount = 1;
  mockAudioRestartResult = 0;
  mockAudioRestartCalls = 0;
  screenAudioSettings.fullRedraw();
  auto* screen = mockScreenData;
  REQUIRE(screen->onEdit(0, 2, CellEditAction::tap) == 1);
  CHECK(appSettings.audioOutputDeviceName[0] == 0);
  CHECK(mockAudioRestartCalls == 0);
  REQUIRE(screen->onEdit(0, 2, CellEditAction::increase) == 1);
  CHECK(std::strcmp(appSettings.audioOutputDeviceName, "ROCCAT Juke") == 0);
  CHECK(std::strcmp(mockSelectedAudioOutput, "ROCCAT Juke") == 0);
  CHECK(mockAudioRestartCalls == 1);
  appSettings.audioOutputDeviceName[0] = 0;
  REQUIRE(settingsLoad() == 0);
  CHECK(std::strcmp(appSettings.audioOutputDeviceName, "ROCCAT Juke") == 0);
  mockAudioRestartResult = 1;
  REQUIRE(screen->onEdit(0, 2, CellEditAction::clear) == 1);
  CHECK(std::strcmp(appSettings.audioOutputDeviceName, "ROCCAT Juke") == 0);
  CHECK(std::strcmp(mockSelectedAudioOutput, "ROCCAT Juke") == 0);
  mockAudioRestartResult = 0;
  REQUIRE(screen->onEdit(0, 2, CellEditAction::clear) == 1);
  CHECK(appSettings.audioOutputDeviceName[0] == 0);
  CHECK(mockSelectedAudioOutput[0] == 0);
  mockAudioOutputDeviceCount = 0;
}

TEST_CASE_FIXTURE(StickLiveFixture, "audio output skips unavailable devices in either direction") {
  mockAudioOutputNames = {"audiocodec, ", "USB Audio", "HDMI"};
  screenAudioSettings.fullRedraw();
  auto* screen = mockScreenData;
  CellEditAction action = CellEditAction::increase;
  std::vector<std::string> expected;
  std::string selected;
  SUBCASE("Default passes a busy codec and opens USB Audio") {
    mockAudioRestartResults = {1, 0};
    expected = {"audiocodec, ", "USB Audio"};
    selected = "USB Audio";
  }
  SUBCASE("reverse from USB Audio passes the codec and opens Default") {
    strcpy(appSettings.audioOutputDeviceName, "USB Audio");
    action = CellEditAction::decrease;
    mockAudioRestartResults = {1, 0};
    expected = {"audiocodec, ", ""};
  }
  SUBCASE("reverse wraps from Default to HDMI") {
    action = CellEditAction::decrease;
    expected = {"HDMI"};
    selected = "HDMI";
  }
  SUBCASE("multiple unavailable devices do not prevent HDMI selection") {
    mockAudioRestartResults = {1, 1, 0};
    expected = {"audiocodec, ", "USB Audio", "HDMI"};
    selected = "HDMI";
  }
  SUBCASE("a disappearing device is skipped") {
    mockAudioOutputNames[0].clear();
    expected = {"USB Audio"};
    selected = "USB Audio";
  }
  SUBCASE("a missing saved output still allows Default") {
    strcpy(appSettings.audioOutputDeviceName, "Unplugged USB");
    mockAudioRestartResults = {1, 1, 1, 0};
    expected = {"audiocodec, ", "USB Audio", "HDMI", ""};
  }
  REQUIRE(screen->onEdit(0, 2, action) == 1);
  CHECK(mockAudioOutputAttempts == expected);
  CHECK(mockAudioRestartCalls == (int)expected.size());
  CHECK(mockAudioOutputOpen);
  CHECK(mockOpenedAudioOutput == selected);
  CHECK(std::string(appSettings.audioOutputDeviceName) == selected);
  CHECK(std::string(screenGetActiveMessage()).empty());
  strcpy(appSettings.audioOutputDeviceName, "not saved");
  REQUIRE(settingsLoad() == 0);
  CHECK(std::string(appSettings.audioOutputDeviceName) == selected);
}

TEST_CASE_FIXTURE(StickLiveFixture, "audio output recovers after a bounded unsuccessful search") {
  mockAudioOutputNames = {"audiocodec, ", "USB Audio"};
  screenAudioSettings.fullRedraw();
  auto* screen = mockScreenData;
  CellEditAction action = CellEditAction::increase;
  std::vector<std::string> expected;
  std::string selected, message;
  bool open = true;
  int messageFrames = MESSAGE_TIME;
  SUBCASE("only Default works") {
    mockAudioRestartResults = {1, 1, 0};
    expected = {"audiocodec, ", "USB Audio", ""};
    message = "No other output";
  }
  SUBCASE("previous USB output is restored") {
    strcpy(appSettings.audioOutputDeviceName, "USB Audio");
    mockAudioRestartResults = {1, 1, 0};
    expected = {"", "audiocodec, ", "USB Audio"};
    selected = "USB Audio";
    message = "No other output";
  }
  SUBCASE("Default recovers after the previous USB output also fails") {
    strcpy(appSettings.audioOutputDeviceName, "USB Audio");
    mockAudioRestartResults = {1, 1, 1, 0};
    expected = {"", "audiocodec, ", "USB Audio", ""};
    message = "Using Default";
  }
  SUBCASE("reset does not select an unrelated USB output") {
    strcpy(appSettings.audioOutputDeviceName, "USB Audio");
    action = CellEditAction::clear;
    mockAudioRestartResults = {1, 0};
    expected = {"", "USB Audio"};
    selected = "USB Audio";
    message = "Default unavailable";
  }
  SUBCASE("total failure preserves the last valid saved selection") {
    strcpy(appSettings.audioOutputDeviceName, "USB Audio");
    mockAudioRestartResult = 1;
    expected = {"", "audiocodec, ", "USB Audio", ""};
    selected = "USB Audio";
    message = "Audio output failed";
    messageFrames = MESSAGE_TIME_ERROR;
    open = false;
  }
  SUBCASE("no enumerated devices leave the active Default stream alone") {
    mockAudioOutputNames.clear();
    message = "No other output";
  }
  REQUIRE(settingsSave() == 0);
  REQUIRE(screen->onEdit(0, 2, action) == 1);
  CHECK(mockAudioOutputAttempts == expected);
  CHECK(mockAudioRestartCalls == (int)expected.size());
  CHECK(mockAudioOutputOpen == open);
  if (open) CHECK(mockOpenedAudioOutput == selected);
  CHECK(std::string(mockSelectedAudioOutput) == selected);
  CHECK(std::string(appSettings.audioOutputDeviceName) == selected);
  CHECK(std::string(screenGetActiveMessage()) == message);
  CHECK(mockMessageFrames == messageFrames);
  strcpy(appSettings.audioOutputDeviceName, "not saved");
  REQUIRE(settingsLoad() == 0);
  CHECK(std::string(appSettings.audioOutputDeviceName) == selected);
}

TEST_CASE_FIXTURE(StickLiveFixture, "audio output retries previously unavailable interfaces") {
  mockAudioOutputNames = {"audiocodec, ", "USB Audio"};
  mockAudioRestartResults = {1, 1, 0, 1, 0};
  screenAudioSettings.fullRedraw();
  auto* screen = mockScreenData;
  REQUIRE(screen->onEdit(0, 2, CellEditAction::increase) == 1);
  CHECK(std::string(screenGetActiveMessage()) == "No other output");
  REQUIRE(screen->onEdit(0, 2, CellEditAction::increase) == 1);
  CHECK(mockAudioOutputAttempts == std::vector<std::string>{"audiocodec, ", "USB Audio", "", "audiocodec, ", "USB Audio"});
  CHECK(mockOpenedAudioOutput == "USB Audio");
  CHECK(std::string(appSettings.audioOutputDeviceName) == "USB Audio");
  CHECK(std::string(screenGetActiveMessage()).empty());
}

TEST_CASE_FIXTURE(StickLiveFixture, "audio output tap and reset on Default do not reopen audio") {
  mockAudioOutputNames = {"audiocodec, ", "USB Audio"};
  screenAudioSettings.fullRedraw();
  auto* screen = mockScreenData;
  for (auto action : {CellEditAction::tap, CellEditAction::clear})
    CHECK(screen->onEdit(0, 2, action) == 1);
  CHECK(mockAudioRestartCalls == 0);
  CHECK(mockAudioOutputAttempts.empty());
  CHECK(mockAudioOutputOpen);
  CHECK(std::string(screenGetActiveMessage()).empty());
}

TEST_CASE_FIXTURE(StickLiveFixture, "audio sample rate settings accept only 32 or 48 kHz") {
  struct StoredSetting { const char* text; int expected; };
  for (const auto& setting : {StoredSetting{"audioSampleRate: 32000\n", 32000},
                              StoredSetting{"audioSampleRate: 48000\n", 48000},
                              StoredSetting{"audioSampleRate: 96000\n", 48000},
                              StoredSetting{"audioSampleRate: 0\n", 48000}}) {
    std::ofstream(realSettingsPath) << setting.text;
    REQUIRE(settingsLoad() == 0);
    CHECK(appSettings.audioSampleRate == setting.expected);
  }
}
}

TEST_SUITE("MIDI settings") {

TEST_CASE_FIXTURE(StickLiveFixture, "MIDI CC destination combines target and parameter with the standard popup") {
  auto& project = chipnomadState->project;
  project.tracksCount = 2;
  getInstrumentFunctions(InstrumentType::Braids).init(&project.instruments[3]);
  std::strcpy(project.instruments[3].name, "Long Bass Name");
  screenMidiCC.setup(0);
  screenMidiCC.fullRedraw();
  auto* data = mockScreenData;
  REQUIRE(data);
  CHECK(data->getColumnCount(0) == 4);
  CHECK(data->rows == 64);
  CHECK(screenMidiCC.getPlaybackLevel() == ScreenPlaybackLevel::song);
  CHECK(screenMidiCC.onInput(1, keyPlay, 0) == 0);
  gfxClear();
  data->drawStatic();
  data->drawField(3, 0, CellState::normal);
  CHECK(std::string(mockGfxCells[1] + 14, 11) == "DESTINATION");
  CHECK(std::string(mockGfxCells[2] + 14, 10) == "Unassigned");
  currentScreen = &screenMidiCC;
  gfxPrint(34, 3, "VALUE!");
  appDraw();
  CHECK(std::string(mockGfxCells[3] + 34, 6) == "VALUE!"); // No playback sidebar over the table.
  project.midiCCMappings[0].cc = 1;
  CHECK(data->onEdit(2, 0, CellEditAction::decrease) == 1); // Blank rows do not reserve CC 0.
  CHECK(project.midiCCMappings[0].cc == 0);
  project.midiCCMappings[1].destination = midiCCDestinationTrackMute;
  project.midiCCMappings[0].cc = 1;
  CHECK(data->onEdit(2, 0, CellEditAction::decrease) == 0); // Real assignments still prevent duplicates.
  CHECK(project.midiCCMappings[0].cc == 1);
  project.midiCCMappings[1].destination = midiCCDestinationNone;

  data->cursorRow = 63;
  data->cursorCol = 3;
  data->topRow = 55;
  auto& mapping = project.midiCCMappings[63];
  projectModified = 0;
  currentScreen = &screenMidiCC;
  auto open = [&] {
    screenMidiCC.onInput(1, keyEdit, 1);
    CHECK(currentScreen == &screenMidiCC); // Wait for release, allowing EDIT+L/R.
    screenMidiCC.onInput(0, 0, 1);
    REQUIRE(currentScreen == &screenSelectionPopup);
    screenSelectionPopup.fullRedraw();
  };
  auto press = [](int key) { screenSelectionPopup.onInput(1, key, 1); };
  open();
  std::string menu;
  for (const auto& row : mockGfxCells) menu.append(row, 40);
  CHECK(menu.find("Unassigned") != std::string::npos);
  CHECK(menu.find("Track 2") != std::string::npos);
  CHECK(menu.find("03 Long Bass") != std::string::npos);
  CHECK(menu.find("Track 3") == std::string::npos);
  press(keyOpt);
  CHECK(currentScreen == &screenMidiCC);
  screenMidiCC.setup(-1);
  CHECK(data->cursorRow == 63);
  CHECK(data->cursorCol == 3);
  CHECK(data->topRow == 55);
  CHECK(projectModified == 0);

  open();
  press(keyDown); // Song
  press(keyRight);
  press(keyEdit);
  CHECK(currentScreen == &screenMidiCC);
  CHECK(mapping.destination == midiCCDestinationSongPlayStop);
  CHECK(projectModified == 1);
  screenMidiCC.setup(-1);
  screenMidiCC.onInput(1, keyEdit | keyRight, 0);
  screenMidiCC.onInput(0, 0, 0);
  CHECK(currentScreen == &screenMidiCC); // Cycling does not also open the menu.
  CHECK(mapping.destination == midiCCDestinationTrackMute);
  CHECK(mapping.instrument == 0);
  screenMidiCC.onInput(1, keyEdit | keyLeft, 0);
  CHECK(mapping.destination == midiCCDestinationSongPlayStop);

  open();
  press(keyDown); // Track 1
  press(keyDown); // Track 2
  press(keyRight);
  press(keyDown); // Solo
  press(keyEdit);
  CHECK(mapping.destination == midiCCDestinationTrackSolo);
  CHECK(mapping.instrument == 1);
  screenMidiCC.setup(-1);
  mapping.enabled = 0;
  data->drawField(3, 63, CellState::normal);
  CHECK(std::string(mockGfxCells[10] + 14, 13) == "Track 2: Solo");

  open();
  press(keyDown); // Instrument 03
  press(keyRight);
  press(keyEdit); // Volume
  CHECK(mapping.instrument == 3);
  CHECK(mapping.destination == 1);
  data->drawField(3, 63, CellState::normal);
  CHECK(std::string(mockGfxCells[10] + 14, 26).find(": Volume") != std::string::npos);
  CHECK(mockTextRight < 40);
  data->drawCursor(3, 63);
  CHECK(std::string(screenGetActiveMessage()).find("Long Bass Name: Volume") != std::string::npos);
}

TEST_CASE_FIXTURE(StickLiveFixture, "MIDI song play/stop toggles on presses and ignores button release") {
  MidiButtonInput button;
  projectInitAY(&chipnomadState->project);
  chipnomadInitChips(chipnomadState, 48000, nullptr);
  auto tick = [&] {
    appOnEvent({MainLoopEvent::tick});
    float audio[2048];
    chipnomadRender(chipnomadState, audio, 1024);
  };
  *pSongRow = *pSongTrack = 0;
  chipnomadState->project.song[0][0] = 0;
  chipnomadState->project.chains[0].rows[0].phrase = 0;
  tick(); // Clear any mapping state left by a previous app instance.
  chipnomadState->project.midiCCMappings[0] = {1, 0, 41, 0, midiCCDestinationSongPlayStop};
  REQUIRE(chipnomadQueueProjectRefresh(chipnomadState));
  tick(); // Configuring a mapping must not start playback.
  REQUIRE_FALSE(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying);

  SUBCASE("held presses toggle once and release leaves playback unchanged") {
    button.values = {127};
    tick();
    CHECK(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying);
    button.values = {127}; // A repeated high value is not another press.
    tick();
    CHECK(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying);
    button.values = {0};
    tick();
    CHECK(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying);
    button.values = {127};
    tick();
    CHECK_FALSE(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying);
    button.values = {0};
    tick();
    CHECK_FALSE(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying);
  }
  SUBCASE("a quick press and release between UI ticks is not lost") {
    button.values = {127, 0};
    tick();
    CHECK(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying);
    button.values = {127, 0};
    tick();
    CHECK_FALSE(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying);
  }
  SUBCASE("two complete presses between UI ticks leave transport unchanged") {
    button.values = {127, 0, 127, 0};
    tick();
    CHECK_FALSE(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying);
  }
  SUBCASE("MIDI Learn does not trigger playback from the learning press") {
    chipnomadState->project.midiCCMappings[0].enabled = 0;
    button.values = {127};
    tick();
    chipnomadState->project.midiCCMappings[0].enabled = 1;
    tick();
    button.values = {0};
    tick();
    CHECK_FALSE(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying);
    button.values = {127};
    tick();
    CHECK(chipnomadGetPlaybackStatus(chipnomadState)->isPlaying);
  }
}

TEST_CASE_FIXTURE(StickLiveFixture, "MIDI mute and solo toggle on presses while track volume stays continuous") {
  MidiButtonInput button;
  struct RestoreAudioManager {
    AudioManager saved = audioManager;
    ~RestoreAudioManager() { audioManager = saved; }
  } restore;
  static int muteCalls, soloCalls;
  muteCalls = soloCalls = 0;
  audioManager.toggleTrackMute = [](int track) { CHECK(track == 2); ++muteCalls; };
  audioManager.toggleTrackSolo = [](int track) { CHECK(track == 2); ++soloCalls; };
  auto tick = [&] { appOnEvent({MainLoopEvent::tick}); };
  tick();
  auto& mapping = chipnomadState->project.midiCCMappings[63];
  for (int destination : {midiCCDestinationTrackMute, midiCCDestinationTrackSolo}) {
    CAPTURE(destination);
    mapping = {1, 0, 41, 2, (uint8_t)destination};
    tick();
    int& calls = destination == midiCCDestinationTrackMute ? muteCalls : soloCalls;
    for (int press = 1; press <= 2; ++press) {
      button.values = {127};
      tick();
      CHECK(calls == press);
      button.values = {127, 0}; // Repeated high value and release do not toggle.
      tick();
      CHECK(calls == press);
    }
    button.values = {127, 0}; // A whole tap between UI ticks still toggles.
    tick();
    CHECK(calls == 3);
  }
  mapping.destination = midiCCDestinationTrackVolume;
  tick();
  button.values = {127};
  tick();
  CHECK(chipnomadState->project.trackVolume[2] == 100);
  button.values = {0};
  tick();
  CHECK(chipnomadState->project.trackVolume[2] == 0);
  CHECK(muteCalls == 3);
  CHECK(soloCalls == 3);
}

TEST_CASE_FIXTURE(StickLiveFixture, "screenMidi shows device rows and links to channel mapping") {
  screenMidi.fullRedraw();
  REQUIRE(mockScreenData != nullptr);
  CHECK(mockScreenData->rows == 9);
  auto* screen = mockScreenData;

  screen->drawField(0, 0, CellState::focus);
  CHECK(std::string(mockGfxCells[2], 7) == "MIDI In");
  CHECK(std::string(mockGfxCells[2] + 10, 3) == "OFF");
  screen->drawCursor(0, 0);
  CHECK(mockCursorX == 10);
  CHECK(mockCursorY == 2);
  CHECK(mockCursorWidth == 3); // "OFF"
  CHECK(screen->onEdit(0, 0, CellEditAction::increase) == 1); // handled, wraps straight back to OFF (no ports)
  CHECK(appSettings.midiInputDevice == -1);

  screen->drawField(0, 1, CellState::focus);
  CHECK(std::string(mockGfxCells[3], 8) == "MIDI Out");
  CHECK(std::string(mockGfxCells[3] + 10, 3) == "OFF");

  screen->drawField(0, 2, CellState::focus);
  CHECK(std::string(mockGfxCells[5], 15) == "Channel mapping");
  screen->onEdit(0, 2, CellEditAction::tap);
  CHECK(currentScreen == &screenMidiChannelMap);

  currentScreen = &screenMidi;
  screen->drawField(0, 3, CellState::focus);
  CHECK(std::string(mockGfxCells[6], 15) == "MIDI CC mapping");
  screen->onEdit(0, 3, CellEditAction::tap);
  CHECK(currentScreen == &screenMidiCC);

  CHECK(appSettings.midiSendClock == 0);
  CHECK(appSettings.midiSendTransport == 0);
  CHECK(screen->onEdit(0, 4, CellEditAction::tap) == 1);
  CHECK(appSettings.midiSendClock == 1);
  CHECK(appSettings.midiSendTransport == 0);
  CHECK(screen->onEdit(0, 5, CellEditAction::tap) == 1);
  CHECK(appSettings.midiSendTransport == 1);
  screen->drawField(0, 4, CellState::focus);
  CHECK(std::string(mockGfxCells[8], 10) == "Send clock");
  CHECK(std::string(mockGfxCells[8] + 16, 2) == "ON");
  appSettings.clockSource = ClockSource::midiClock;
  screen->drawField(0, 5, CellState::focus);
  CHECK(std::string(mockGfxCells[9] + 20, 12) == "(MIDI input)");
  screen->drawField(0, 6, CellState::focus);
  CHECK(std::string(mockGfxCells[10], 10) == "MIDI delay");
  CHECK(screen->onEdit(0, 6, CellEditAction::increaseBig) == 1);
  CHECK(appSettings.midiOutputDelayMs == 10);

  currentScreen = &screenMidi;
  CHECK(screenMidi.onInput(1, keyOpt, 0) == 1);
  CHECK(currentScreen == &screenSettings);
}

TEST_CASE_FIXTURE(StickLiveFixture, "screenMidi shows NOT FOUND for a configured device that isn't currently available") {
  strncpy(appSettings.midiInputDeviceName, "Some USB Keyboard", MIDI_DEVICE_NAME_LENGTH);
  appSettings.midiInputDevice = -1; // Not resolved to a live port (none attached under test).

  screenMidi.fullRedraw();
  REQUIRE(mockScreenData != nullptr);
  mockScreenData->drawField(0, 0, CellState::focus);
  CHECK(std::string(mockGfxCells[2] + 10, 9) == "NOT FOUND");
}

TEST_CASE_FIXTURE(StickLiveFixture, "MIDI device names round-trip through settings save/load") {
  strncpy(appSettings.midiInputDeviceName, "Keystation Mini", MIDI_DEVICE_NAME_LENGTH);
  strncpy(appSettings.midiOutputDeviceName, "FluidSynth virtual port", MIDI_DEVICE_NAME_LENGTH);
  REQUIRE(settingsSave() == 0);

  initDefaultAppSettings();
  CHECK(appSettings.midiInputDeviceName[0] == '\0');

  REQUIRE(settingsLoad() == 0);
  CHECK(std::string(appSettings.midiInputDeviceName) == "Keystation Mini");
  CHECK(std::string(appSettings.midiOutputDeviceName) == "FluidSynth virtual port");
}

TEST_CASE_FIXTURE(StickLiveFixture, "audio input master switch preserves devices and migrates old settings") {
  appSettings.audioInputDeviceCount = 1;
  std::strcpy(appSettings.audioInputDeviceNames[0], "USB Audio");
  appSettings.audioInputArmed[0] = 0;
  appSettings.audioInputEnabled = 1;
  REQUIRE(settingsSave() == 0);

  REQUIRE(audioInputSetEnabled(0) == 0);
  CHECK(appSettings.audioInputEnabled == 0);
  CHECK(appSettings.audioInputArmed[0] == 0);
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.audioInputEnabled == 0);
  CHECK(appSettings.audioInputArmed[0] == 0);

  FILE* file = fopen(realSettingsPath.c_str(), "w");
  REQUIRE(file != nullptr);
  fprintf(file, "audioInputDeviceCount: 1\n");
  fprintf(file, "audioInputDeviceName.0: USB Audio\n");
  fprintf(file, "audioInputArmed.0: USB Audio\n");
  fclose(file);
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.audioInputEnabled == 1);
}

TEST_CASE_FIXTURE(StickLiveFixture, "MIDI clock mode round-trips through settings save/load") {
  appSettings.clockSource = ClockSource::midiClock;
  REQUIRE(settingsSave() == 0);

  initDefaultAppSettings();
  CHECK(appSettings.clockSource == ClockSource::internal);

  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.clockSource == ClockSource::midiClock);

  appSettings.midiSendClock = 1;
  appSettings.midiSendTransport = 1;
  REQUIRE(settingsSave() == 0);
  initDefaultAppSettings();
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.midiSendClock == 1);
  CHECK(appSettings.midiSendTransport == 1);

  // Invalid values fall back to the default rather than being trusted.
  FILE* file = fopen(realSettingsPath.c_str(), "w");
  REQUIRE(file != nullptr);
  fprintf(file, "midiClockMode: 7\nmidiSendClock: 7\nmidiSendTransport: -1\n");
  fclose(file);
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.clockSource == ClockSource::internal);
  CHECK(appSettings.midiSendClock == 0);
  CHECK(appSettings.midiSendTransport == 0);
}

TEST_CASE_FIXTURE(StickLiveFixture, "screenMidiChannelMap cycles channel-to-instrument assignment") {
  screenMidiChannelMap.fullRedraw();
  REQUIRE(mockScreenData != nullptr);
  CHECK(mockScreenData->rows == MIDI_CHANNEL_COUNT);
  auto* screen = mockScreenData;

  screen->drawField(0, 0, CellState::focus);
  CHECK(std::string(mockGfxCells[2], 5) == "CH 01");
  CHECK(std::string(mockGfxCells[2] + 23, 3) == "OFF");

  REQUIRE(screen->onEdit(0, 0, CellEditAction::increase) == 1);
  CHECK(appSettings.midiChannelInstrument[0] == 0);
  screen->drawField(0, 0, CellState::focus);
  std::string expected = std::string(byteToHex(0)) + ": " + instrumentName(&chipnomadState->project, 0);
  CHECK(std::string(mockGfxCells[2] + 23, expected.size()) == expected);

  REQUIRE(screen->onEdit(0, 0, CellEditAction::decrease) == 1);
  CHECK(appSettings.midiChannelInstrument[0] == -1); // back to OFF
  REQUIRE(screen->onEdit(0, 0, CellEditAction::decrease) == 1);
  CHECK(appSettings.midiChannelInstrument[0] == PROJECT_MAX_INSTRUMENTS - 1); // wraps the other way

  currentScreen = &screenMidiChannelMap;
  CHECK(screenMidiChannelMap.onInput(1, keyOpt, 0) == 1);
  CHECK(currentScreen == &screenMidi);
}

TEST_CASE_FIXTURE(StickLiveFixture, "midiChannelInstrument round-trips through settings save/load and clamps garbage") {
  appSettings.midiChannelInstrument[0] = 5;
  appSettings.midiChannelInstrument[15] = 42;
  REQUIRE(settingsSave() == 0);

  initDefaultAppSettings();
  CHECK(appSettings.midiChannelInstrument[0] == -1);

  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.midiChannelInstrument[0] == 5);
  CHECK(appSettings.midiChannelInstrument[15] == 42);
  for (int i = 1; i < 15; i++) CHECK(appSettings.midiChannelInstrument[i] == -1);

  // Out-of-range values (from a hand-edited or corrupted settings.txt) clamp to OFF.
  // Written to fileGetDefaultDirectory()'s own resolved path rather than a bare
  // relative "settings.txt": that call resolves the running test binary's own
  // executable directory (see fileGetDefaultDirectory()'s desktop fallback),
  // not this fixture's temp cwd, so a relative write here would silently miss
  // the file settingsLoad() actually reads.
  char defaultDir[PATH_LENGTH];
  REQUIRE(fileGetDefaultDirectory(defaultDir, sizeof(defaultDir)) == 0);
  std::string settingsPath = std::string(defaultDir) + PATH_SEPARATOR_STR + "settings.txt";
  std::ofstream(settingsPath) << "midiChannelInstrument: 999,-5,3\n";
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.midiChannelInstrument[0] == -1);
  CHECK(appSettings.midiChannelInstrument[1] == -1);
  CHECK(appSettings.midiChannelInstrument[2] == 3);
}

}

TEST_CASE_FIXTURE(StickLiveFixture, "Persistent waveform is opt-in and its Settings toggle persists") {
  CHECK(appSettings.persistentWaveform == 0);
  screenGraphicsSettings.fullRedraw();
  REQUIRE(mockScreenData != nullptr);
  auto* screen = mockScreenData;
  int modified = projectModified;
  CHECK(screen->rows == 13);
  screen->drawField(0, 3, CellState::focus);
  CHECK(std::string(mockGfxCells[6] + 23, 3) == "OFF");
  REQUIRE(screen->onEdit(0, 3, CellEditAction::tap) == 1);
  CHECK(appSettings.persistentWaveform == 1);
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.persistentWaveform == 1);
  REQUIRE(screen->onEdit(0, 3, CellEditAction::tap) == 1);
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.persistentWaveform == 0);
  CHECK(projectModified == modified);
}

TEST_CASE_FIXTURE(StickLiveFixture, "Personal visual settings coexist and survive saving either option") {
  mockQuitTriggered = 0;
  appSettings.trackVisuals[0].mode = TrackVisualMode::audio;
  screenGraphicsSettings.fullRedraw();
  REQUIRE(mockScreenData != nullptr);
  auto* settings = mockScreenData;
  REQUIRE(settings->onEdit(0, 3, CellEditAction::tap) == 1);
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.persistentWaveform == 1);
  CHECK(appSettings.trackVisuals[0].mode == TrackVisualMode::audio);
  settings->onEdit(0, 4, CellEditAction::tap);
  CHECK(currentScreen == &screenTrackVisuals);
  CHECK_FALSE(mockQuitTriggered);
  appSettings.trackVisuals[1].mode = TrackVisualMode::audio;
  REQUIRE(settingsSave() == 0);
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.persistentWaveform == 1);
  CHECK(appSettings.trackVisuals[1].mode == TrackVisualMode::audio);
}

TEST_CASE_FIXTURE(StickLiveFixture, "audio device removal restarts the audio handler and shows the notice") {
  // The disconnect path rebuilds everything audio: the output device is
  // reopened through AudioManager::refreshDevices, capture is torn down and
  // re-enumerated, and the helper box shows the animated notice.
  mockAudioRefreshResult = 0;
  mockAudioRefreshCalls = 0;
  mockAudioRestartNotificationFrames = 0;

  MainLoopEventData event = {};
  event.type = MainLoopEvent::audioDeviceRemoved;
  event.data.value = 1; // capture device removed
  appOnEvent(event);

  CHECK(mockAudioRefreshCalls == 1);
  CHECK(mockAudioRestartNotificationFrames > 0);
  CHECK(screenAudioRestartActive());

  // The notice is transient: it clears without any further event.
  mockAudioRestartNotificationFrames = 1;
  CHECK(screenAudioRestartActive());
  mockAudioRestartNotificationFrames = 0;
  CHECK_FALSE(screenAudioRestartActive());

  // A second disconnect restarts again (hot-unplug/replug cycles).
  appOnEvent(event);
  CHECK(mockAudioRefreshCalls == 2);
}

TEST_CASE_FIXTURE(StickLiveFixture, "audio input Refresh restarts the output through AudioManager") {
  screenAudioInput.fullRedraw();
  REQUIRE(mockScreenData != nullptr);
  CHECK(mockScreenData->onEdit(1, 0, CellEditAction::tap) == 1);
  CHECK(mockAudioRefreshCalls == 1);
}

TEST_CASE_FIXTURE(StickLiveFixture, "Link transport preference and source survive settings reload") {
  CHECK(appSettings.linkTransport == 1);
  appSettings.linkTransport = 0;
  appSettings.clockSource = ClockSource::link;
  REQUIRE(settingsSave() == 0);
  initDefaultAppSettings();
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.linkTransport == 0);
  CHECK(appSettings.clockSource == ClockSource::link);
  FILE* file = fopen(realSettingsPath.c_str(), "w");
  REQUIRE(file != nullptr);
  fprintf(file, "clockSource: 2\n");
  fclose(file);
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.clockSource == ClockSource::link);
  CHECK(appSettings.linkTransport == 1); // Older settings retain transport following.
}
