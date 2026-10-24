#include "doctest.h"
#include "chipnomad_lib.h"
#include "sid_patch.h"
#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "mocks/app_ui_mock.h"
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {
struct Editor {
  ChipNomadState* previous = chipnomadState;
  const AppScreen* previousScreen = currentScreen;
  AppSettings settings = appSettings;
  int previousInstrument = cInstrument;
  int previousModified = projectModified;
  ScreenData* table;
  Editor() {
    chipnomadState = chipnomadCreate();
    initDefaultAppSettings();
    screensInitAll();
    fillFXNames();
    cInstrument = 0;
    getInstrumentFunctions(InstrumentType::SID).init(&chipnomadState->project.instruments[0]);
    currentScreen = &screenSIDEdit;
    screenSIDEdit.setup(0);
    screenSIDEdit.fullRedraw();
    table = mockScreenData;
    projectModified = 0;
    screenMessage(0, "");
  }
  ~Editor() {
    chipnomadDestroy(chipnomadState);
    chipnomadState = previous;
    currentScreen = previousScreen;
    appSettings = settings;
    cInstrument = previousInstrument;
    projectModified = previousModified;
  }
  InstrumentSID& patch() { return chipnomadState->project.instruments[0].chip.sid; }
  void draw() {
    gfxClear();
    table->drawStatic();
    for (int row = 0; row < table->rows; ++row) {
      table->drawRowHeader(row, CellState::normal);
      for (int col = 0; col < table->getColumnCount(row); ++col)
        table->drawField(col, row, CellState::normal);
    }
  }
  std::string hint(int col, int row) {
    table->cursorCol = col; table->cursorRow = row;
    screenMessage(0, "");
    screenSIDEdit.draw();
    return screenGetActiveMessage();
  }
  void macro(unsigned id, int value) {
    auto& p = patch().program;
    p.format = 1; p.rate = 60;
    auto* m = p.data + p.size;
    const uint8_t header[] = {uint8_t(id), 1, 0, 255, 0, 0, 1, 0};
    memcpy(m, header, 8);
    for (int n = 0; n < 4; ++n) m[8+n] = uint32_t(value) >> (n*8);
    p.size += 12;
  }
};
}

TEST_CASE("SID editor entry and return preserve quick controls and patch") {
  Editor e;
  auto before = e.patch();
  instrumentFMSetContext(0, InstrumentType::SID);
  CHECK(screenInstrumentOPL.getColumnCount(4) == 2);
  screenInstrumentOPL.drawField(1, 4, CellState::focus);
  CHECK(std::string(mockGfxCells[7] + 28, 4) == "EDIT");
  screenInstrumentOPL.onEdit(1, 4, CellEditAction::tap);
  CHECK(currentScreen == &screenSIDEdit);
  CHECK(memcmp(&before, &e.patch(), sizeof(before)) == 0);
  CHECK(projectModified == 0);
  for (int keys : {int(keyOpt), keyLeft | keyShift}) {
    currentScreen = &screenSIDEdit;
    screenSIDEdit.onInput(1, keys, 1);
    CHECK(currentScreen == &screenInstrument);
  }
}

TEST_CASE("SID editor bounds stay valid and every recipe parameter saves") {
  Editor e;
  bool edited[sidParameterCount]{};
  for (int row = 0; row < e.table->rows; ++row) {
    for (int col = 0; col < e.table->getColumnCount(row); ++col) {
      CAPTURE(row); CAPTURE(col);
      auto& p = e.patch();
      initSIDPatch(&p);
      e.table->onEdit(col, row, CellEditAction::clear);
      REQUIRE(validSID(p));
      auto before = p;
      e.table->onEdit(col, row, CellEditAction::increase);
      int changes = 0;
      for (int n = 1; n < sidParameterCount; ++n) if (p.value[n] != before.value[n]) {
        CHECK_FALSE(edited[n]); edited[n] = true; ++changes;
      }
      CHECK(changes == 1);
      for (int n = 0; n < 300; ++n) {
        e.table->onEdit(col, row, CellEditAction::increaseBig);
        REQUIRE(validSID(p));
      }
      e.table->drawField(col, row, CellState::focus);
      e.table->drawCursor(col, row);
      CHECK(mockCursorX + mockCursorWidth <= 32);
      for (int n = 0; n < 300; ++n) {
        e.table->onEdit(col, row, CellEditAction::decreaseBig);
        REQUIRE(validSID(p));
      }
    }
  }
  for (int n = 1; n < sidParameterCount; ++n) CHECK(edited[n]);
  initSIDPatch(&e.patch());
  for (int row = 0; row < e.table->rows; ++row)
    for (int col = 0; col < e.table->getColumnCount(row); ++col)
      e.table->onEdit(col, row, CellEditAction::increaseBig);
  CHECK(projectModified == 1);
  auto loaded = std::make_unique<Project>(); projectInit(loaded.get());
  REQUIRE(instrumentSave(&chipnomadState->project, "build/tests/sid-edited.cni", 0) == 0);
  REQUIRE(instrumentLoad(loaded.get(), "build/tests/sid-edited.cni", 1) == 0);
  CHECK(memcmp(&e.patch(), &loaded->instruments[1].chip.sid, sizeof(InstrumentSID)) == 0);
  projectFree(loaded.get());
}

TEST_CASE("SID editor cutoff off sentinel and four digit fields render correctly") {
  Editor e;
  auto& target = e.patch().value[sidCutoffTarget];
  CHECK(target == 65535);
  e.table->onEdit(0, 5, CellEditAction::increase);
  CHECK(target == 0);
  e.table->onEdit(0, 5, CellEditAction::decrease);
  CHECK(target == 65535);
  e.table->onEdit(0, 5, CellEditAction::increaseBig);
  CHECK(target < 2048);
  e.table->onEdit(0, 5, CellEditAction::clear);
  CHECK(target == 65535);
  e.patch().value[sidFixedFrequency] = 65535;
  e.draw();
  CHECK(std::string(mockGfxCells[8] + 12, 3) == "off");
  CHECK(std::string(mockGfxCells[10] + 12, 4) == "FFFF");
  CHECK(std::string(mockGfxCells[10] + 16, 3) == "MUL");
  for (int row = 3; row <= 12; ++row)
    CHECK(std::string(mockGfxCells[row] + 32, 8) == "        ");
}

TEST_CASE("SID editor protects program-owned controls while keeping other controls editable") {
  Editor e;
  e.macro(2, 1500);
  REQUIRE(validSID(e.patch()));
  auto before = e.patch();
  e.draw();
  CHECK(mockGfxCells[6][11] == '*');
  CHECK(mockGfxCells[3][11] == ' ');
  for (int col = 0; col < 3; ++col) {
    CHECK(e.hint(col, 3) == "Pulse width: imported pulse macro");
    e.table->onEdit(col, 3, CellEditAction::increase);
  }
  CHECK(e.patch().value[sidPulse] == before.value[sidPulse]);
  CHECK(e.patch().value[sidPulseDepth] == before.value[sidPulseDepth]);
  CHECK(e.patch().value[sidPulseRate] == before.value[sidPulseRate]);
  CHECK(memcmp(&before.program, &e.patch().program, sizeof(ChipProgram)) == 0);
  CHECK(projectModified == 0);
  e.table->onEdit(0, 1, CellEditAction::increase);
  CHECK(e.patch().value[sidAttack] == before.value[sidAttack] + 1);
  CHECK(projectModified == 1);
  e.macro(3, 2); e.macro(5, 900); e.macro(1, 7);
  REQUIRE(validSID(e.patch()));
  CHECK(e.hint(0, 0) == "Waveform: imported waveform macro");
  CHECK(e.hint(1, 4) == "Cutoff: imported filter macro");
  CHECK(e.hint(0, 7) == "Pitch: imported arpeggio macro");
}

TEST_CASE("SID editor GoatTracker controls cannot silently edit unused values") {
  Editor e;
  auto& p = e.patch().program;
  p.format = 2; p.rate = 50; p.size = 33;
  memcpy(p.data, "GTI5", 4);
  REQUIRE(validSID(e.patch()));
  auto before = e.patch();
  for (int row = 0; row < e.table->rows; ++row)
    for (int col = 0; col < e.table->getColumnCount(row); ++col)
      e.table->onEdit(col, row, CellEditAction::increase);
  CHECK(memcmp(&before.program, &e.patch().program, sizeof(ChipProgram)) == 0);
  CHECK(e.patch().value[sidPulse] == before.value[sidPulse]);
  CHECK(projectModified == 0);
  REQUIRE(validSID(e.patch()));
  e.draw();
  CHECK(std::string(mockGfxCells[14], 40).find("Source: GoatTracker tables") == 0);
  CHECK(e.hint(0, 0) == "Waveform: imported Goat wave table");
}

TEST_CASE("SID editor FX markers follow matching active tracks without blocking edits") {
  Editor e;
  chipnomadGetPlaybackStatus(chipnomadState);
  auto& status = chipnomadState->uiPlaybackStatus;
  status = {};
  chipnomadState->project.tracksCount = 3;
  status.isPlaying = 1;
  status.trackEnabled[1] = 1;
  auto& track = status.tracks[1];
  track.mode = PlaybackMode::song;
  track.note.instrument = 0;
  track.note.fx[fxSCT].isOn = 1;
  track.note.fx[fxSCT].fxValue = 200;
  CHECK(e.hint(1, 4) == "Track 2: SCT active; editing preset");
  CHECK(e.hint(2, 4) == "Filter resonance: 0-F");
  e.draw();
  CHECK(mockGfxCells[7][19] == '*');
  auto before = e.patch();
  e.table->onEdit(1, 4, CellEditAction::increase);
  screenInstrumentOPL.onEdit(0, 7, CellEditAction::increase);
  CHECK(e.patch().value[sidCutoff] == before.value[sidCutoff] + 2);
  CHECK(projectModified == 1);
  CHECK(track.note.fx[fxSCT].fxValue == 200);
  track.note.instrument = 1;
  CHECK(e.hint(1, 4) == "Filter cutoff: 000-7FF");
  track.note.instrument = 0;
  track.mode = PlaybackMode::stopped;
  CHECK(e.hint(1, 4) == "Filter cutoff: 000-7FF");
  track.mode = PlaybackMode::song;
  status.trackEnabled[1] = 0;
  CHECK(e.hint(1, 4) == "Filter cutoff: 000-7FF");
  status.trackEnabled[1] = 1;
  status.isPlaying = 0;
  CHECK(e.hint(1, 4) == "Filter cutoff: 000-7FF");
  CHECK(mockGfxCells[7][19] == ' ');
  e.macro(5, 900);
  CHECK(e.hint(1, 4) == "Cutoff: imported filter macro");
}

TEST_CASE("SID editor observes real audio FX snapshots while editing the preset") {
  Editor e;
  auto& project = chipnomadState->project;
  REQUIRE(projectLoad(&project, "packaging/common/projects/gm-midi-demo.cct") == 0);
  project.tracksCount = 1;
  project.chipsCount = 1;
  project.song[0][0] = 0;
  project.chains[0].rows[0].phrase = 0;
  phraseClear(&project.phrases[0]);
  auto& row = project.phrases[0].rows[0];
  row.note = 45; row.instrument = 0; row.volume = PHRASE_VOLUME_MAX;
  row.fx[0] = {fxSCT, 200};
  getInstrumentFunctions(InstrumentType::SID).init(&project.instruments[0]);
  auto before = e.patch();
  chipnomadInitChips(chipnomadState, 48000, nullptr);
  REQUIRE(chipnomadQueueProjectRefresh(chipnomadState));
  REQUIRE(chipnomadQueuePlaybackStartSong(chipnomadState, 0, 0, 1));
  std::vector<float> audio(1920);
  chipnomadRender(chipnomadState, audio.data(), 960);
  CHECK(e.hint(1, 4) == "Track 1: SCT active; editing preset");
  CHECK(memcmp(&before, &e.patch(), sizeof(before)) == 0);
  e.table->onEdit(1, 4, CellEditAction::increase);
  CHECK(e.patch().value[sidCutoff] == before.value[sidCutoff] + 1);
  CHECK(row.fx[0].command == fxSCT);
  CHECK(row.fx[0].value == 200);
  chipnomadQueuePlaybackStop(chipnomadState);
  chipnomadRender(chipnomadState, audio.data(), 960);
  CHECK(e.hint(1, 4) == "Filter cutoff: 000-7FF");
}

TEST_CASE("SID home marks and protects imported controls") {
  Editor e;
  instrumentFMSetContext(0, InstrumentType::SID);
  e.macro(2, 1500);
  auto before = e.patch();
  screenInstrumentOPL.drawStatic();
  screenInstrumentOPL.drawField(1, 5, CellState::focus);
  CHECK(mockGfxCells[9][27] == '*');
  CHECK(std::string(mockGfxCells[10], 32).find("Program:") == std::string::npos);
  CHECK(std::string(mockGfxCells[15], 32).find("* = program controlled") == 0);
  screenInstrumentOPL.onEdit(1, 5, CellEditAction::increase);
  CHECK(memcmp(&before, &e.patch(), sizeof(before)) == 0);
  CHECK(projectModified == 0);
  CHECK(std::string(screenGetActiveMessage()) == "Pulse width: imported pulse macro");
  screenInstrumentOPL.onEdit(0, 8, CellEditAction::increase);
  CHECK(e.patch().value[sidAttack] == before.value[sidAttack] + 1);
  e.patch().program = {};
  screenInstrumentOPL.drawField(1, 5, CellState::normal);
  CHECK(mockGfxCells[9][27] == ' ');
  screenInstrumentOPL.drawStatic();
  CHECK(std::string(mockGfxCells[15], 32).find("* = program controlled") == std::string::npos);
  e.macro(1, 12); // Pitch is owned only in EDIT, not among the home controls.
  screenInstrumentOPL.drawStatic();
  CHECK(std::string(mockGfxCells[15], 32).find("* = program controlled") == std::string::npos);
}
