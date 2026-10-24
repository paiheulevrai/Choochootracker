#include "doctest.h"
#include "common.h"
#include "project_utils.h"
#include "app_ui_mock.h"
#include "corelib_file.h"
#include "corelib_gfx.h"
#include "waveform_display.h"
#include "monitor_display.h"
#include "audio_monitor.h"
#include "chips/chips.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
struct VisualFixture {
  AppSettings saved = appSettings;
  ChipNomadState* state = chipnomadState;
  const AppScreen* screen = currentScreen;
  std::string settingsPath, contents;
  bool existed;
  VisualFixture() {
    char path[PATH_LENGTH];
    REQUIRE(fileGetDefaultDirectory(path, sizeof(path)) == 0);
    settingsPath = std::string(path) + "/settings.txt";
    existed = std::filesystem::exists(settingsPath);
    if (existed) { std::ifstream f(settingsPath); contents.assign(std::istreambuf_iterator<char>(f), {}); }
    initDefaultAppSettings();
    chipnomadState = chipnomadCreate();
    REQUIRE(chipnomadState != nullptr);
    projectInitAY(&chipnomadState->project);
    chipnomadInitChips(chipnomadState, 48000, nullptr);
    waveformDisplayInit();
    monitorDisplayInit();
  }
  ~VisualFixture() {
    chipnomadDestroy(chipnomadState);
    chipnomadState = state; appSettings = saved; currentScreen = screen;
    if (existed) { std::ofstream f(settingsPath); f << contents; }
    else std::filesystem::remove(settingsPath);
  }
};
bool blank(Bitmap* bitmap) {
  return bitmap && std::all_of(bitmap->data, bitmap->data + bitmap->widthPixels * bitmap->heightPixels,
    [](uint8_t value) { return value == 0; });
}
void refreshWaveforms() {
  waveformDisplayInvalidate();
  waveformDisplayRefresh();
}
}

TEST_CASE_FIXTURE(VisualFixture, "Track display defaults to Detailed and round-trips each mode") {
  { std::ofstream f(settingsPath); f << "themeName: OldTheme\n"; }
  REQUIRE(settingsLoad() == 0);
  for (const auto& visual : appSettings.trackVisuals)
    CHECK(visual.mode == TrackVisualMode::detailed);
  appSettings.trackVisuals[0].mode = TrackVisualMode::audio;
  appSettings.trackVisuals[7].mode = TrackVisualMode::audio;
  REQUIRE(settingsSave() == 0);
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.trackVisuals[0].mode == TrackVisualMode::audio);
  CHECK(appSettings.trackVisuals[7].mode == TrackVisualMode::audio);
  CHECK(appSettings.trackVisuals[1].mode == TrackVisualMode::detailed);
  CHECK(std::string(appSettings.themeName) == "OldTheme");
  { std::ofstream f(settingsPath); f << "trackVisuals0: 1\ntrackVisuals9: 1\n"
    "trackVisuals1: 2\ntrackVisuals2: -1\ntrackVisuals3: invalid\n"; }
  REQUIRE(settingsLoad() == 0);
  for (const auto& visual : appSettings.trackVisuals)
    CHECK(visual.mode == TrackVisualMode::detailed);
}

TEST_CASE_FIXTURE(VisualFixture, "Earlier per-layer settings retain modes and discard hidden layers") {
  { std::ofstream f(settingsPath); f << "trackVisuals1: 1,0,0,0\ntrackVisuals8: 0,0,0,0\n"; }
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.trackVisuals[0].mode == TrackVisualMode::audio);
  CHECK(appSettings.trackVisuals[7].mode == TrackVisualMode::detailed);
  REQUIRE(settingsSave() == 0);
  std::ifstream f(settingsPath);
  std::string saved((std::istreambuf_iterator<char>(f)), {});
  CHECK(saved.find("trackVisuals1: 1\n") != std::string::npos);
  CHECK(saved.find("trackVisuals8: 0\n") != std::string::npos);
  chipnomadState->project.instruments[0].type = InstrumentType::AY1;
  auto& track = chipnomadState->uiPlaybackStatus.tracks[7];
  track.note.instrument = 0; track.note.pitchFinal = 48;
  chipnomadState->chips[7]->setRegister(7, 0x37);
  chipnomadState->chips[7]->setRegister(8, 15);
  refreshWaveforms();
  CHECK_FALSE(blank(waveformDisplayGetBitmap(7)));
  CHECK_FALSE(blank(waveformDisplayGetBitmap(0))); // Audio's silent centre line remains visible.
}

TEST_CASE_FIXTURE(VisualFixture, "Boot screen defaults to original and persists the night option") {
  { std::ofstream f(settingsPath); f << "themeName: ExistingTheme\n"; }
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.bootScreenNight == 0);
  appSettings.bootScreenNight = 1;
  REQUIRE(settingsSave() == 0);
  initDefaultAppSettings();
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.bootScreenNight == 1);
  CHECK(std::string(appSettings.themeName) == "ExistingTheme");
  for (const char* value : {"-1", "2", "garbage"}) {
    { std::ofstream f(settingsPath); f << "bootScreenNight: " << value << '\n'; }
    REQUIRE(settingsLoad() == 0);
    CHECK(appSettings.bootScreenNight == 0);
  }
}

TEST_CASE_FIXTURE(VisualFixture, "Boot screen Graphics row toggles and saves without changing the theme") {
  currentScreen = &screenGraphicsSettings;
  screenGraphicsSettings.setup(0);
  screenGraphicsSettings.fullRedraw();
  ScreenData* table = mockScreenData;
  REQUIRE(table != nullptr);
  REQUIRE(table->onEdit(0, 6, CellEditAction::tap) == 1);
  table->drawField(0, 6, CellState::focus);
  CHECK(std::string(mockGfxCells[8], 11) == "Boot screen");
  CHECK(std::string(mockGfxCells[8] + 23, 8) == "Night   ");
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.bootScreenNight == 1);
  REQUIRE(table->onEdit(0, 6, CellEditAction::tap) == 1);
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.bootScreenNight == 0);
}

TEST_CASE_FIXTURE(VisualFixture, "One display choice switches each track and saves bulk choices") {
  currentScreen = &screenTrackVisuals;
  screenTrackVisuals.setup(0);
  screenTrackVisuals.fullRedraw();
  ScreenData* table = mockScreenData;
  REQUIRE(table != nullptr);
  CHECK(table->getColumnCount(0) == 1);
  REQUIRE(table->onEdit(0, 2, CellEditAction::tap) == 1);
  CHECK(appSettings.trackVisuals[2].mode == TrackVisualMode::audio);
  CHECK(appSettings.trackVisuals[1].mode == TrackVisualMode::detailed);
  table->drawField(0, 2, CellState::focus);
  CHECK(std::string(mockGfxCells[5] + 4, 14) == "Audio waveform");
  table->onEdit(0, 2, CellEditAction::tap);
  table->drawField(0, 2, CellState::focus);
  CHECK(std::string(mockGfxCells[5] + 4, 14) == "Detailed      ");
  table->onEdit(1, PROJECT_MAX_TRACKS, CellEditAction::tap);
  for (const auto& visual : appSettings.trackVisuals)
    CHECK(visual.mode == TrackVisualMode::audio);
  table->onEdit(0, PROJECT_MAX_TRACKS, CellEditAction::tap);
  for (const auto& visual : appSettings.trackVisuals)
    CHECK(visual.mode == TrackVisualMode::detailed);
  table->onEdit(0, 2, CellEditAction::tap);
  table->onEdit(0, PROJECT_MAX_TRACKS + 1, CellEditAction::tap);
  CHECK(currentScreen == &screenSettings);
  REQUIRE(settingsLoad() == 0);
  CHECK(appSettings.trackVisuals[2].mode == TrackVisualMode::audio);
  CHECK(appSettings.trackVisuals[1].mode == TrackVisualMode::detailed);
}

TEST_CASE_FIXTURE(VisualFixture, "Detailed voice display includes waveform and envelope together") {
  chipnomadState->project.instruments[0].type = InstrumentType::Braids;
  chipnomadState->uiPlaybackStatus.tracks[0].note.instrument = 0;
  auto& voice = chipnomadState->voiceMonitors[0];
  voice.active = 1; voice.envelope = 1;
  std::fill_n(voice.samples, VOICE_MONITOR_SAMPLES, 0.25f);
  refreshWaveforms();
  Bitmap* bitmap = waveformDisplayGetBitmap(0);
  REQUIRE(bitmap != nullptr);
  CHECK(bitmap->data[1] == 160); // Full envelope produces the top overlay.
  CHECK(std::count(bitmap->data, bitmap->data + bitmap->widthPixels * bitmap->heightPixels, 255) > 0);
  voice.active = 0;
  refreshWaveforms();
  CHECK(blank(waveformDisplayGetBitmap(0)));
}

TEST_CASE_FIXTURE(VisualFixture, "AY Detailed display reads its own track and includes noise and envelope") {
  chipnomadState->project.instruments[0].type = InstrumentType::AY1;
  auto& track = chipnomadState->uiPlaybackStatus.tracks[5];
  track.note.instrument = 0; track.note.pitchFinal = 48;
  SoundChip* chip = chipnomadState->chips[5];
  REQUIRE(chip != nullptr);
  chip->setRegister(7, 0x3f); chip->setRegister(8, 15);
  refreshWaveforms();
  Bitmap* bitmap = waveformDisplayGetBitmap(5);
  REQUIRE(bitmap != nullptr);
  CHECK(bitmap->data[0] == 255); // Track 6's channel A on its own AY chip.
  CHECK(bitmap->data[bitmap->widthPixels] == 0);
  chip->setRegister(7, 0x37); // Noise only on channel A.
  refreshWaveforms();
  CHECK_FALSE(blank(waveformDisplayGetBitmap(5)));
  CHECK(bitmap->data[bitmap->widthPixels] > 0); // Noise texture below the trace.
  chip->setRegister(7, 0x3e); chip->setRegister(8, 0x10); chip->setRegister(13, 0);
  refreshWaveforms();
  bitmap = waveformDisplayGetBitmap(5);
  CHECK(std::count(bitmap->data, bitmap->data + bitmap->widthPixels * bitmap->heightPixels, 160) > 0);
  track.note.pitchFinal = EMPTY_VALUE_8;
  refreshWaveforms();
  CHECK(blank(waveformDisplayGetBitmap(5)));
}

TEST_CASE_FIXTURE(VisualFixture, "Audio display uses summed snapshots independently of instrument graphics") {
  auto* monitor = chipnomadState->audioMonitor;
  float mix[128]{};
  monitor->beginRender(); monitor->beginChunk(64);
  for (int i = 0; i < 128; ++i) monitor->add(0, i, 0.5f);
  monitor->finishChunk(mix, 64, 12000); monitor->publish(); monitorDisplayUpdate();
  appSettings.trackVisuals[0].mode = TrackVisualMode::audio;
  refreshWaveforms();
  Bitmap* bitmap = waveformDisplayGetBitmap(0);
  REQUIRE(bitmap != nullptr);
  std::vector<uint8_t> before(bitmap->data, bitmap->data + bitmap->widthPixels * bitmap->heightPixels);
  chipnomadState->project.instruments[0].type = InstrumentType::AY1;
  auto& track = chipnomadState->uiPlaybackStatus.tracks[0];
  track.note.instrument = 0; track.note.pitchFinal = 48;
  chipnomadState->chips[0]->setRegister(7, 0x37);
  chipnomadState->chips[0]->setRegister(8, 15);
  refreshWaveforms();
  bitmap = waveformDisplayGetBitmap(0);
  CHECK(std::equal(before.begin(), before.end(), bitmap->data));
  CHECK_FALSE(blank(bitmap));
}

TEST_CASE("Audio mini readout preserves narrow peaks and leaves padding clear") {
  std::vector<uint8_t> pixels(16 * 24, 255);
  Bitmap bitmap{}; bitmap.widthPixels = 16; bitmap.heightPixels = 24; bitmap.data = pixels.data();
  float samples[256]{}; samples[7] = 1; samples[8] = -1; samples[250] = NAN;
  renderTrackAudioWaveform(&bitmap, samples, 256);
  CHECK(pixels[1 * 16 + 1] == 255);
  CHECK(pixels[21 * 16 + 1] == 255);
  for (int y=0;y<24;++y) { CHECK(pixels[y*16] == 0); CHECK(pixels[y*16+15] == 0); }
  for (int x=0;x<16;++x) { CHECK(pixels[x] == 0); CHECK(pixels[23*16+x] == 0); }
  std::fill_n(samples, 256, 0);
  renderTrackAudioWaveform(&bitmap, samples, 256);
  CHECK(pixels[11*16+5] == 64);
}

TEST_CASE_FIXTURE(VisualFixture, "Upstream Detailed display supports native chip voice monitors") {
  const InstrumentType types[] = {InstrumentType::OPLL, InstrumentType::VRC7,
    InstrumentType::OPL2, InstrumentType::OPL3, InstrumentType::DX7,
    InstrumentType::GenesisFM, InstrumentType::ArcadeFM,
    InstrumentType::SegaPSG, InstrumentType::GBPulse, InstrumentType::GBNoise};
  auto& track = chipnomadState->uiPlaybackStatus.tracks[5];
  track.note.instrument = 0;
  track.note.pitchFinal = 48;
  auto& voice = chipnomadState->voiceMonitors[5];
  voice.active = 1;
  voice.envelope = 0.5f;
  for (int i = 0; i < VOICE_MONITOR_SAMPLES; ++i) voice.samples[i] = (i & 1) ? 0.5f : -0.5f;
  for (auto type : types) {
    CAPTURE((int)type);
    chipnomadState->project.instruments[0].type = type;
    refreshWaveforms();
    auto* bitmap = waveformDisplayGetBitmap(5);
    REQUIRE(bitmap != nullptr);
    CHECK_FALSE(blank(bitmap));
    CHECK(std::count(bitmap->data, bitmap->data + bitmap->widthPixels * bitmap->heightPixels, 160) > 0);
  }
}
