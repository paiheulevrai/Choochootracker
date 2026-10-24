#include "doctest.h"
#include "chipnomad_lib.h"
#include "corelib_gfx.h"
#include "screens.h"
#include "monk_patch.h"
#include "mocks/app_ui_mock.h"
#include <set>
#include <string>

namespace {
struct PopupSession {
  ChipNomadState* oldState = chipnomadState;
  const AppScreen* oldScreen = currentScreen;
  int oldOffset = gfxGetContentRowOffset();
  PopupSession() {
    chipnomadState = chipnomadCreate();
    screensInitAll();
    currentScreen = &screenPhrase;
    *pSongTrack = 3;
  }
  ~PopupSession() {
    chipnomadDestroy(chipnomadState);
    chipnomadState = oldState;
    currentScreen = oldScreen;
    gfxSetContentRowOffset(oldOffset);
  }
};

void checkPopup(FXId command, int table, int offset) {
  gfxClear();
  // Adjacent track information must survive the popup draw.
  mockGfxCells[8][36] = '#';
  fxEditFullDraw(command, 0, table);
  REQUIRE(mockCursorY >= 6);
  CHECK(std::string(mockGfxCells[5], 35) == std::string(35, ' '));
  REQUIRE(mockCursorY <= 18 - offset);
  REQUIRE(mockCursorX >= 1);
  REQUIRE(mockCursorX + mockCursorWidth <= 34);
  CHECK(mockTextRight <= 34);
  CHECK(mockTextBottom <= 18 - offset);
  CHECK(mockGfxCells[8][36] == '#');
  FXCell selected{FX_ID_EMPTY, 73}, last{};
  fxEditInput(0, 1, &selected, &last);
  CHECK(selected.command == command);
  CHECK(std::string(&mockGfxCells[mockCursorY][mockCursorX], 3) == fxNames[command].name);
  CHECK(std::string(mockGfxCells[19 - offset], 35) == std::string(35, ' '));
}
}

TEST_CASE("Lane FX popup keeps every existing engine and insert command visible") {
  PopupSession session;
  auto& project = chipnomadState->project;
  int engines = 0;
  for (int type = 1; type < int(InstrumentType::totalCount); ++type) {
    if (type == 14 || type == 15) continue; // Reserved project IDs.
    ++engines;
    getInstrumentFunctions(InstrumentType(type)).init(&project.instruments[0]);
    for (int offset : {0, 2}) for (int table : {0, 1}) {
      gfxSetContentRowOffset(offset);
      for (int module = 0; module < insertModuleCount; ++module) {
        insertSelect(&project.trackInserts[3][0], module);
        insertSelect(&project.trackInserts[3][1], module);
        fxEditFullDraw(fxARP, 0, table);
        std::set<FXId> seen;
        for (int step = 0; step < fxTotalCount; ++step) {
          FXCell selected{FX_ID_EMPTY, 73}, last{};
          fxEditInput(0, 1, &selected, &last);
          if (!seen.insert(selected.command).second) break;
          CAPTURE(type); CAPTURE(module); CAPTURE(offset); CAPTURE(table); CAPTURE(selected.command);
          checkPopup(selected.command, table, offset);
          fxEditInput(keyEdit | keyRight, 1, &selected, &last);
        }
        CHECK(seen.size() > 20);
        for (int slot = 0; slot < 2; ++slot)
          for (int parameter = 0; parameter < insertDescriptor(module).count; ++parameter)
            CHECK(seen.count(fxF11 + slot * 8 + parameter) == 1);
      }
    }
  }
  CHECK(engines == 27);
}

TEST_CASE("Lane FX group spacing stays compact when switching help types") {
  PopupSession session;
  auto& project = chipnomadState->project;
  getInstrumentFunctions(InstrumentType::DX7).init(&project.instruments[0]);
  insertSelect(&project.trackInserts[3][0], insertRotary);
  insertSelect(&project.trackInserts[3][1], insertCompressor);
  for (FXId fx : {FXId(fxARP), FXId(fxOL1), FXId(fxOAR), FXId(fxM44), FXId(fxF11), FXId(fxF26)}) {
    CAPTURE(fx);
    getInstrumentFunctions(fx == fxOAR ? InstrumentType::OPL2 : InstrumentType::DX7).init(&project.instruments[0]);
    checkPopup(fx, 0, 0);
    CHECK(std::string(mockGfxCells[6], 13) == " Sequencer FX");
    bool end = false;
    for (int row = 6; row <= 18; ++row) {
      bool blank = std::string(mockGfxCells[row], 34) == std::string(34, ' ');
      if (blank) end = true;
      else CHECK_FALSE(end);
    }
  }
}

TEST_CASE("Lane FX popup bounds context-dependent modulation and drum help") {
  PopupSession session;
  auto& instrument = chipnomadState->project.instruments[0];
  getInstrumentFunctions(InstrumentType::DrumSynth).init(&instrument);
  for (int model = 0; model < int(DrumSynthEngine::totalCount); ++model) {
    instrument.chip.drumSynth.engine = DrumSynthEngine(model);
    const auto* definition = getInstrumentDefinition(instrument.type);
    for (int n = 0; n < definition->fxCount; ++n) {
      FXId command = definition->fxList[n].fx;
      if (instrumentFXAvailableForInstrument(&instrument, command)) checkPopup(command, 0, 0);
    }
  }
  for (int type = 0; type < int(ModulationType::totalCount); ++type) {
    for (auto& modulation : instrument.modulation) modulation.type = ModulationType(type);
    for (int command = fxM1A; command <= fxM44; ++command) checkPopup(command, 1, 0);
  }
}

TEST_CASE("Lane FX long groups scroll and use the same grid for drawing and navigation") {
  PopupSession session;
  auto& instrument = chipnomadState->project.instruments[0];
  getInstrumentFunctions(InstrumentType::AChChid).init(&instrument);
  FXCell selected{}, last{};
  fxEditFullDraw(fxASL, 0, 0);
  fxEditInput(keyEdit | keyDown, 1, &selected, &last);
  fxEditInput(0, 1, &selected, &last);
  CHECK(selected.command == fxAEM); // Ninth command wraps at the sidebar boundary.
  CHECK(mockCursorX == 1);
  fxEditInput(keyEdit | keyUp, 1, &selected, &last);
  fxEditInput(0, 1, &selected, &last);
  CHECK(selected.command == fxASL);

  // Exercise the viewport with a deliberately tall existing group, without
  // introducing fake commands or relying on today's largest synth catalogue.
  struct Columns {
    int saved = fxGroups[0].columns;
    ~Columns() { fxGroups[0].columns = saved; }
  } columns;
  fxGroups[0].columns = 1;
  checkPopup(fxARP, 0, 0);
  CHECK(mockGfxCells[18][34] == 'v');
  checkPopup(fxPAN, 0, 0);
  CHECK(mockGfxCells[6][34] == '^');
  fxEditInput(keyEdit | keyUp, 1, &selected, &last);
  fxEditInput(0, 1, &selected, &last);
  CHECK(selected.command == fxSLE);
  CHECK(mockCursorY <= 18);
  fxEditInput(keyEdit | keyRight, 1, &selected, &last);
  fxEditInput(keyEdit | keyRight, 1, &selected, &last);
  fxEditInput(0, 1, &selected, &last);
  CHECK(selected.command == fxRSN);
  CHECK(mockGfxCells[6][34] == ' '); // New group returns to the compact origin.
}

TEST_CASE("Lane FX uses the available width while keeping modulation slots together") {
  PopupSession session;
  getInstrumentFunctions(InstrumentType::SoundFont).init(&chipnomadState->project.instruments[0]);
  for (int offset : {0, 2}) {
    gfxSetContentRowOffset(offset);
    checkPopup(fxRSN, 0, offset);
    const int trackRow = mockCursorY;
    checkPopup(fxTPN, 0, offset);
    CHECK(mockCursorY == trackRow);
    CHECK(mockCursorX == 17);
    checkPopup(fxM1A, 0, offset);
    const int modRow = mockCursorY;
    checkPopup(fxM14, 0, offset);
    CHECK(mockCursorY == modRow);
    CHECK(mockCursorX == 17);
    checkPopup(fxM2A, 0, offset);
    CHECK(mockCursorY == modRow + 1);
    CHECK(mockCursorX == 1);
  }
}

extern int currentGroup;
FXGroup* getVisibleGroup(int visibleIdx, InstrumentType instType);
TEST_CASE("MONK exposes all sixteen controls together without a duplicate envelope group") {
  PopupSession session;
  getInstrumentFunctions(InstrumentType::Monk).init(&chipnomadState->project.instruments[0]);
  for (int offset : {0, 2}) for (int table : {0, 1}) {
    gfxSetContentRowOffset(offset);
    for (const auto& control : monkControls) {
      checkPopup(control.fx, table, offset);
      auto* group = getVisibleGroup(currentGroup, InstrumentType::Monk);
      REQUIRE(group != nullptr);
      CHECK(group->instType == InstrumentType::Monk);
      CHECK(group->count == 16);
      CHECK(group->columns == 8);
    }
    checkPopup(fxTDC, table, offset);
    FXCell selected{}, last{};
    fxEditInput(keyEdit | keyLeft, 1, &selected, &last);
    fxEditInput(0, 1, &selected, &last);
    CHECK(selected.command == fxTPN); // No envelope entries before Trigger FX.
  }
}
