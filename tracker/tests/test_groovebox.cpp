#include "../external/doctest.h"
#include "../src/screens/screen_groovebox.h"
#include <memory>
#include "app_ui_mock.h"
#include "project_utils.h"
#include <cstring>
#include "selection_popup.h"
#include "copy_paste.h"

struct GrooveboxUIFixture {
  ChipNomadState* savedState = chipnomadState;
  AppSettings savedSettings = appSettings;
  int* savedRow = pSongRow;
  int* savedTrack = pSongTrack;
  int* savedBar = pChainRow;
  int row = 0, track = 0, bar = 0;
  bool savedModified = projectModified;
  bool savedEditor = grooveboxEditorActive;
  bool savedCanonical = canonicalSimplifiedActive;
  const AppScreen* savedScreen = currentScreen;
  GrooveboxUIFixture() {
    chipnomadState = chipnomadCreate();
    projectInitAY(&chipnomadState->project);
    fillFXNames();
    pSongRow = &row; pSongTrack = &track; pChainRow = &bar;
    projectModified = false;
    screenGroovebox.init();
    screenGroovebox.setup(-1);
    grooveboxEditorActive = true;
    canonicalSimplifiedActive = false;
    currentScreen = &screenGroovebox;
  }
  ~GrooveboxUIFixture() {
    chipnomadDestroy(chipnomadState);
    chipnomadState = savedState;
    appSettings = savedSettings;
    pSongRow = savedRow; pSongTrack = savedTrack; pChainRow = savedBar;
    projectModified = savedModified;
    grooveboxEditorActive = savedEditor;
    canonicalSimplifiedActive = savedCanonical;
    currentScreen = savedScreen;
  }
};

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox semantic text colours follow the active theme and field focus") {
  auto& p = chipnomadState->project;
  p.song[0][0] = 0;
  p.chains[0].rows[0].phrase = 0;
  auto& event = p.phrases[0].rows[0];
  event.note = 48; event.instrument = 2; event.volume = 64;
  event.fx[0] = {fxHOP, 0};
  for (int palette : {0x100000, 0x200000}) {
    auto& cs = appSettings.colorScheme;
    cs.textTitles = palette + 1; cs.textLabel = palette + 2;
    cs.textDefault = palette + 3; cs.textNames = palette + 4;
    cs.textValue = palette + 5; cs.textEmpty = palette + 6;
    cs.textInfo = palette + 7; cs.warning = palette + 8;
    cs.cursor = palette + 9; cs.playMarkers = palette + 10;
    screenGroovebox.init(); screenGroovebox.setup(-1);
    CHECK(mockGfxColors[0][0] == cs.textTitles);
    CHECK(mockGfxColors[0][13] == cs.textLabel);
    CHECK(mockGfxColors[0][17] == cs.textDefault);
    CHECK(mockGfxColors[13][3] == cs.textValue);
    CHECK(mockGfxColors[13][8] == cs.textValue);
    CHECK(mockGfxColors[14][3] == cs.textLabel);
    CHECK(mockGfxColors[14][8] == cs.textDefault);
    CHECK(mockGfxColors[13][18] == cs.textNames);
    CHECK(mockGfxColors[14][18] == cs.textEmpty);
    CHECK(mockGfxColors[3][2] == cs.textTitles);
    CHECK(mockGfxColors[3][4] == cs.textEmpty);
    CHECK(mockGfxColors[3][3] == cs.cursor);
    CHECK(mockGfxColors[17][0] == cs.textInfo);
    screenGroovebox.onInput(1, keyRight, 1);
    CHECK(mockGfxColors[13][8] == cs.textNames);
    CHECK(mockGfxColors[13][13] == cs.textValue);
    CHECK(mockGfxColors[13][18] == cs.textValue);
    screenGroovebox.onInput(1, keyShift | keyUp, 1);
    CHECK(mockGfxColors[0][20] == cs.textValue);
    CHECK(mockGfxColors[0][27] == cs.textValue);
    CHECK(mockGfxColors[13][18] == cs.textNames);
    p.song[1][0] = 0;
    screenGroovebox.fullRedraw();
    CHECK(mockGfxColors[11][0] == cs.warning);
    CHECK(mockGfxColors[3][6] == cs.textEmpty);
    p.song[1][0] = EMPTY_VALUE_16;
  }
  CHECK_FALSE(projectModified);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox navigation is read-only and shared edits are refused") {
  auto& p = chipnomadState->project;
  p.song[0][0] = 4;
  p.chains[4].rows[0].phrase = 12;
  p.phrases[12].rows[0].note = 48;
  p.song[1][0] = 4;
  screenGroovebox.setup(0);
  screenGroovebox.onInput(1, keyOpt | keyRight, 1);
  CHECK_FALSE(projectModified);
  CHECK(p.phrases[12].rows[1].note == EMPTY_VALUE_8);
  screenGroovebox.onInput(1, keyEdit, 1);
  CHECK_FALSE(projectModified);
  CHECK(p.phrases[12].rows[1].note == EMPTY_VALUE_8);
  CHECK(std::strstr(screenGetActiveMessage(), "Shared") != nullptr);
  screenGroovebox.onInput(1, keyShift | keyLeft, 1);
  CHECK(currentScreen == &screenSong);
  screenGroovebox.onInput(1, keyShift | keyRight, 1);
  CHECK(currentScreen != &screenChain);
  CHECK(currentScreen != &screenSong);
  CHECK(grooveboxEditorActive);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox note editing respects Phrase note lock and pitch bounds") {
  auto& p = chipnomadState->project;
  p.song[0][0] = 0;
  p.chains[0].rows[0].phrase = 0;
  auto& row = p.phrases[0].rows[0];
  row.note = 48;
  p.scaleMode = 1;
  p.scaleApply = 1;
  p.scalePreset = scaleCustom;
  p.scaleCustomMask = (1 << 0) | (1 << 4) | (1 << 7);
  screenGroovebox.setup(-1);
  screenGroovebox.onInput(1, keyEdit | keyRight, 1);
  CHECK(row.note == 52);
  screenGroovebox.onInput(1, keyEdit | keyLeft, 1);
  CHECK(row.note == 48);
  p.scaleTracksMask = 0;
  screenGroovebox.onInput(1, keyEdit | keyRight, 1);
  CHECK(row.note == 49);
  p.scaleTracksMask = 0xff;
  p.pitchTable.octaveSize = 24;
  screenGroovebox.onInput(1, keyEdit | keyRight, 1);
  CHECK(row.note == 50);
  row.note = p.pitchTable.length - 1;
  screenGroovebox.onInput(1, keyEdit | keyRight, 1);
  CHECK(row.note == p.pitchTable.length - 1);
  row.note = 0;
  screenGroovebox.onInput(1, keyEdit | keyLeft, 1);
  CHECK(row.note == 0);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox first empty FX tap opens and commits the existing picker") {
  auto& p = chipnomadState->project;
  p.song[0][0] = 0;
  p.chains[0].rows[0].phrase = 0;
  screenGroovebox.setup(-1);
  screenGroovebox.onInput(1, keyRight, 1);
  screenGroovebox.onInput(1, keyEdit, 1);
  screenGroovebox.fullRedraw();
  screenGroovebox.onInput(0, 0, 1);
  CHECK(p.phrases[0].rows[0].fx[0].command != FX_ID_EMPTY);
  CHECK(projectModified);
  screenGroovebox.onInput(1, keyEdit | keyOpt, 1);
  CHECK(p.phrases[0].rows[0].fx[0].command == FX_ID_EMPTY);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox double instrument edit selects a free instrument like Phrase") {
  auto& p = chipnomadState->project;
  p.song[0][0] = 0;
  p.chains[0].rows[0].phrase = 0;
  p.phrases[0].rows[0].instrument = 7;
  screenGroovebox.setup(-1);
  screenGroovebox.onInput(1, keyDown, 1);
  screenGroovebox.onInput(1, keyEdit, 2);
  CHECK(p.phrases[0].rows[0].instrument == findEmptyInstrument(&p, 0));
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox empty clipboard paste does not allocate or modify the project") {
  resetCopyBuffers();
  screenGroovebox.onInput(1, keyShift | keyEdit, 1);
  CHECK(chipnomadState->project.song[0][0] == EMPTY_VALUE_16);
  CHECK_FALSE(projectModified);
  CHECK(std::strstr(screenGetActiveMessage(), "clipboard") != nullptr);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox Phrase clipboard paste targets the selected FX pair and supports undo") {
  auto& p = chipnomadState->project;
  p.song[0][0] = 0;
  p.chains[0].rows[0].phrase = 0;
  p.phrases[0].rows[0].fx[0] = {fxHOP, 7};
  copyPhrase(0, 3, 0, 4, 0, 0);
  screenGroovebox.setup(-1);
  screenGroovebox.onInput(1, keyRight, 1);
  screenGroovebox.onInput(1, keyDown, 1);
  screenGroovebox.onInput(1, keyDown, 1);
  screenGroovebox.onInput(1, keyOpt | keyRight, 1);
  screenGroovebox.onInput(1, keyShift | keyEdit, 1);
  CHECK(p.phrases[0].rows[1].fx[2].command == fxHOP);
  CHECK(p.phrases[0].rows[1].fx[2].value == 7);
  CHECK(p.phrases[0].rows[1].fx[0].command == FX_ID_EMPTY);
  CHECK(p.phrases[0].rows[1].note == EMPTY_VALUE_8);
  REQUIRE(grooveboxAction(GrooveboxAction::undo, 0, 0));
  CHECK(p.phrases[0].rows[1].fx[2].command == FX_ID_EMPTY);
  CHECK(p.phrases[0].rows[0].fx[0].command == fxHOP);
  resetCopyBuffers();
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox allocates canonical data only on deliberate editing") {
  auto& p = chipnomadState->project;
  CHECK(p.song[0][0] == EMPTY_VALUE_16);
  screenGroovebox.draw();
  CHECK(p.song[0][0] == EMPTY_VALUE_16);
  CHECK_FALSE(projectModified);
  screenGroovebox.onInput(1, keyEdit, 1);
  int c = p.song[0][0];
  REQUIRE(c < PROJECT_MAX_CHAINS);
  int f = p.chains[c].rows[0].phrase;
  REQUIRE(f < PROJECT_MAX_PHRASES);
  CHECK(p.phrases[f].rows[0].note == 48);
  CHECK(p.phrases[f].rows[0].instrument == 0);
  CHECK(p.phrases[f].rows[0].volume == PHRASE_VOLUME_MAX);
  CHECK(p.viewMode == ViewMode::classic);
  CHECK(projectModified);
  for (const auto& pattern : p.patterns)
    CHECK(pattern.bars[0].phrase == EMPTY_VALUE_16);
  screenGroovebox.onInput(1, keyRight, 1);
  screenGroovebox.onInput(1, keyEdit | keyUp, 1);
  screenGroovebox.onInput(0, 0, 0);
  screenGroovebox.onInput(1, keyRight, 1);
  auto& fx = p.phrases[f].rows[0].fx[0];
  const int before = fx.value;
  screenGroovebox.onInput(1, keyEdit | keyRight, 1);
  CHECK(fx.value == before + 1);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox full phrase pool leaves the Song unchanged") {
  auto& p = chipnomadState->project;
  for (auto& phrase : p.phrases) phrase.rows[0].note = 48;
  screenGroovebox.onInput(1, keyEdit, 1);
  CHECK(p.song[0][0] == EMPTY_VALUE_16);
  CHECK_FALSE(projectModified);
  CHECK(std::strstr(screenGetActiveMessage(), "No free") != nullptr);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox appends only the next bar and retains material after gaps") {
  auto& p = chipnomadState->project;
  p.song[0][0] = 0;
  p.chains[0].rows[0].phrase = 0;
  p.phrases[0].rows[0].note = NOTE_OFF;
  p.chains[0].rows[5].phrase = 5;
  REQUIRE(grooveboxAction(GrooveboxAction::emptyBar, 0, 0));
  REQUIRE(p.chains[0].rows[1].phrase < PROJECT_MAX_PHRASES);
  CHECK(p.phrases[0].rows[0].note == NOTE_OFF);
  CHECK(p.chains[0].rows[5].phrase == 5);
  REQUIRE(grooveboxAction(GrooveboxAction::doubleLength, 0, 0));
  auto original = std::make_unique<Project>(p);
  projectModified = false;
  CHECK_FALSE(grooveboxAction(GrooveboxAction::doubleLength, 0, 0));
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
  CHECK_FALSE(projectModified);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox full chain pool does not clear a phrase") {
  auto& p = chipnomadState->project;
  for (auto& map : p.chainToPatternMap) map = 0;
  auto original = std::make_unique<Project>(p);
  screenGroovebox.onInput(1, keyEdit, 1);
  CHECK_FALSE(projectModified);
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
  CHECK(std::strstr(screenGetActiveMessage(), "No free") != nullptr);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox FX picker confirms and closes on release") {
  auto& p = chipnomadState->project;
  screenGroovebox.onInput(1, keyEdit, 1);
  screenGroovebox.onInput(1, keyRight, 1);
  screenGroovebox.onInput(1, keyEdit | keyUp, 1);
  screenGroovebox.onInput(0, 0, 0);
  auto& row = p.phrases[p.chains[p.song[0][0]].rows[0].phrase].rows[0];
  CHECK(row.fx[0].command != FX_ID_EMPTY);
  screenGroovebox.onInput(1, keyShift | keyRight, 1);
  CHECK(currentScreen == &screenInstrument);
  CHECK(grooveboxEditorActive);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox study split keeps field focus below while B moves the grid") {
  auto original = std::make_unique<Project>(chipnomadState->project);
  screenGroovebox.fullRedraw();
  CHECK(mockCursorX == 8);
  CHECK(mockCursorY == 13);
  CHECK(mockCursorWidth == 3);
  screenGroovebox.onInput(1, keyRight, 1);
  CHECK(mockCursorX == 18);
  CHECK(mockCursorY == 13);
  screenGroovebox.onInput(1, keyDown, 1);
  CHECK(mockCursorX == 18);
  CHECK(mockCursorY == 14);
  screenGroovebox.onInput(1, keyRight, 1);
  CHECK(mockCursorX == 23);
  CHECK(mockCursorWidth == 2);
  screenGroovebox.onInput(1, keyOpt | keyRight, 1);
  screenGroovebox.onInput(1, keyOpt | keyDown, 1);
  screenGroovebox.onInput(0, keyOpt, 0);
  screenGroovebox.onInput(0, 0, 0);
  CHECK(mockCursorX == 23);
  CHECK(mockCursorY == 14);
  CHECK(row == 0);
  CHECK(track == 1);
  CHECK(mockGfxCells[4][5] == '<');
  CHECK(std::memcmp(original.get(), &chipnomadState->project, sizeof(Project)) == 0);
  CHECK_FALSE(projectModified);
  screenGroovebox.onInput(1, keyLeft, 1);
  screenGroovebox.onInput(1, keyLeft, 1);
  screenGroovebox.onInput(1, keyUp, 1);
  screenGroovebox.onInput(1, keyEdit, 1);
  auto& p = chipnomadState->project;
  REQUIRE(p.song[0][1] < PROJECT_MAX_CHAINS);
  int f = p.chains[p.song[0][1]].rows[0].phrase;
  CHECK(p.phrases[f].rows[1].note == 48);
  CHECK(p.phrases[f].rows[0].note == EMPTY_VALUE_8);
  CHECK(p.song[0][0] == EMPTY_VALUE_16);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox study field navigation reaches every detail without wrapping") {
  const int path[] = {keyDown, keyDown, keyDown, keyDown, keyRight, keyUp,
    keyRight, keyRight, keyUp, keyLeft, keyUp, keyRight, keyUp, keyLeft, keyLeft, keyLeft};
  const int x[] = {8,8,8,8,8,8,18,23,23,18,18,23,23,18,8,8};
  const int y[] = {14,15,16,16,16,15,15,15,14,14,13,13,13,13,13,13};
  for (int i = 0; i < 16; ++i) {
    screenGroovebox.onInput(1, path[i], 1);
    CHECK(mockCursorX == x[i]);
    CHECK(mockCursorY == y[i]);
  }
  for (int i = 0; i < 20; ++i) screenGroovebox.onInput(1, keyOpt | keyRight, 1);
  for (int i = 0; i < 10; ++i) screenGroovebox.onInput(1, keyOpt | keyDown, 1);
  CHECK(track == 7);
  CHECK(mockGfxCells[10][11] == '<');
  CHECK(mockCursorY == 13);
  for (int i = 0; i < 20; ++i) screenGroovebox.onInput(1, keyOpt | keyLeft, 1);
  for (int i = 0; i < 10; ++i) screenGroovebox.onInput(1, keyOpt | keyUp, 1);
  CHECK(track == 0);
  CHECK(mockGfxCells[3][3] == '<');
  CHECK_FALSE(projectModified);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox study clear resets note companions but retains FX and undo") {
  auto& p = chipnomadState->project;
  screenGroovebox.onInput(1, keyEdit, 1);
  int f = p.chains[p.song[0][0]].rows[0].phrase;
  auto& event = p.phrases[f].rows[0];
  event.fx[0] = {fxHOP, 0};
  screenGroovebox.onInput(1, keyEdit | keyOpt, 1);
  CHECK(event.note == EMPTY_VALUE_8);
  CHECK(event.instrument == EMPTY_VALUE_8);
  CHECK(event.volume == EMPTY_VALUE_16);
  CHECK(event.fx[0].command == fxHOP);
  REQUIRE(grooveboxAction(GrooveboxAction::undo, 0, 0));
  CHECK(event.note == 48);
  CHECK(event.instrument == 0);
  screenGroovebox.onInput(1, keyEdit | keyOpt, 1);
  screenGroovebox.onInput(1, keyEdit | keyOpt, 1);
  CHECK(event.note == NOTE_OFF);
  screenGroovebox.onInput(1, keyEdit | keyRight, 1);
  CHECK(event.note == NOTE_OFF);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox study detail fields align with the stored data") {
  auto& p = chipnomadState->project;
  p.song[0][0] = 0;
  p.chains[0].rows[0] = {0, 0xE8};
  auto& event = p.phrases[0].rows[0];
  event.note = NOTE_OFF;
  event.instrument = 0x12;
  event.volume = 0x34;
  event.fx[0] = {fxHOP, 0x56};
  event.fx[1] = {fxSPD, 0x78};
  event.fx[2] = {fxARP, 0x9A};
  screenGroovebox.fullRedraw();
  CHECK(std::string(&mockGfxCells[13][8], 3) == "OFF");
  CHECK(std::string(&mockGfxCells[14][8], 2) == "12");
  CHECK(std::string(&mockGfxCells[15][8], 2) == "34");
  CHECK(std::string(&mockGfxCells[13][18], 3) == "HOP");
  CHECK(std::string(&mockGfxCells[14][18], 3) == "SPD");
  CHECK(std::string(&mockGfxCells[15][18], 3) == "ARP");
  CHECK(std::string(&mockGfxCells[13][23], 2) == "56");
  CHECK(std::string(&mockGfxCells[14][23], 2) == "78");
  CHECK(std::string(&mockGfxCells[15][23], 2) == "9A");
  CHECK(std::string(&mockGfxCells[16][8], 2) == "E8");
  CHECK_FALSE(projectModified);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox view entry includes canonical Simplified without converting classic data") {
  screenViewSettings.setup(0);
  REQUIRE(mockScreenData != nullptr);
  auto edit = mockScreenData->onEdit;
  grooveboxEditorActive = false;
  edit(0, 0, CellEditAction::decrease);
  CHECK(canonicalSimplifiedActive);
  CHECK_FALSE(grooveboxEditorActive);
  CHECK(chipnomadState->project.viewMode == ViewMode::classic);
  CHECK_FALSE(projectModified);
  edit(0, 0, CellEditAction::decrease);
  CHECK(grooveboxEditorActive);
  CHECK_FALSE(canonicalSimplifiedActive);
  CHECK(chipnomadState->project.viewMode == ViewMode::classic);
  CHECK_FALSE(projectModified);
  edit(0, 0, CellEditAction::increase);
  CHECK(canonicalSimplifiedActive);
  edit(0, 0, CellEditAction::increase);
  CHECK_FALSE(grooveboxEditorActive);
  CHECK(chipnomadState->project.viewMode == ViewMode::classic);
  chipnomadState->project.viewMode = ViewMode::simplified;
  edit(0, 0, CellEditAction::increase);
  CHECK(currentScreen != &screenGroovebox);
  CHECK_FALSE(grooveboxEditorActive);
  CHECK(chipnomadState->project.viewMode == ViewMode::simplified);
  screenGrooveboxAfterConversion.setup(0);
  CHECK_FALSE(grooveboxEditorActive);
  chipnomadState->project.viewMode = ViewMode::classic;
  screenGrooveboxAfterConversion.setup(0);
  CHECK(grooveboxEditorActive);
  row = 11; track = 2; bar = 8;
  screenGrooveboxAfterConversion.getPlaybackLevel();
  CHECK(row == 11);
  CHECK(track == 2);
  CHECK(bar == 8);
  screenGrooveboxAfterConversion.fullRedraw();
}

TEST_CASE("Groovebox resolves canonical bars without altering the project") {
  auto p = std::make_unique<Project>();
  projectInit(p.get());
  p->tracksCount = 8;
  p->song[0][0] = 4;
  p->chains[4].rows[0].phrase = 12;
  p->chains[4].rows[0].transpose = 5;
  CHECK(grooveboxPhraseAt(p.get(), 0, 0, 0) == 12);
  CHECK(grooveboxPhraseAt(p.get(), 0, 0, 1) == -1);
  CHECK(grooveboxPhraseAt(p.get(), 0, 8, 0) == -1);
  CHECK(grooveboxPhraseAt(p.get(), -1, 0, 0) == -1);
  CHECK(p->chains[4].rows[0].transpose == 5);
  CHECK_FALSE(grooveboxCellShared(p.get(), 0, 0, 0));
  p->song[1][0] = 4;
  CHECK(grooveboxCellShared(p.get(), 0, 0, 0));
  p->song[1][0] = EMPTY_VALUE_16;
  p->chains[5].rows[0].phrase = 12;
  CHECK(grooveboxCellShared(p.get(), 0, 0, 0));
  p->chains[5].rows[0].phrase = EMPTY_VALUE_16;
  p->patterns[3].bars[15].phrase = 12;
  CHECK(grooveboxCellShared(p.get(), 0, 0, 0));
}

TEST_CASE("Groovebox preserves differing lengths and references after gaps") {
  auto p = std::make_unique<Project>();
  projectInit(p.get());
  p->tracksCount = 8;
  p->song[0][0] = 1;
  p->song[0][1] = 2;
  for (int b = 0; b < 16; ++b) {
    if (b < 4) p->chains[1].rows[b].phrase = b;
    p->chains[2].rows[b].phrase = b + 16;
  }
  CHECK(grooveboxPhraseAt(p.get(), 0, 0, 4) == -1);
  CHECK(grooveboxPhraseAt(p.get(), 0, 1, 15) == 31);
  p->chains[1].rows[8].phrase = 31;
  CHECK(grooveboxPhraseAt(p.get(), 0, 0, 8) == -1);
  CHECK(grooveboxCellShared(p.get(), 0, 1, 15));
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox bank allocation and empty/OFF bars retain distinct semantics") {
  auto& p = chipnomadState->project;
  p.tracksCount = 8;
  REQUIRE(grooveboxAction(GrooveboxAction::emptyBar, 0, 2));
  CHECK(p.song[0][2] == 0x30);
  CHECK(p.chains[0x30].rows[0].phrase == 0x100);
  CHECK(p.phrases[0x100].rows[0].note == EMPTY_VALUE_8);
  CHECK(grooveboxTrackLength(&p, 0, 2) == 1);
  REQUIRE(grooveboxAction(GrooveboxAction::offBar, 0, 2));
  int off = p.chains[0x30].rows[1].phrase;
  REQUIRE(off < PROJECT_MAX_PHRASES);
  CHECK(p.phrases[off].rows[0].note == NOTE_OFF);
  CHECK(p.phrases[off].rows[1].note == EMPTY_VALUE_8);
  REQUIRE(grooveboxAction(GrooveboxAction::undo, 0, 2));
  CHECK(grooveboxTrackLength(&p, 0, 2) == 1);
  CHECK(p.phrases[off].rows[0].note == EMPTY_VALUE_8);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox new clips use one-based hexadecimal track chain banks") {
  auto& p = chipnomadState->project;
  p.tracksCount = 8;
  for (int t = 0; t < 8; ++t) {
    for (int r = 0; r < 3; ++r) {
      REQUIRE(grooveboxAction(GrooveboxAction::emptyBar, r, t));
      int expected = (t + 1) * 0x10 + r;
      CHECK(p.song[r][t] == expected);
      CHECK(p.chains[expected].rows[0].phrase == t * 0x80 + r);
    }
  }
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox chain allocation skips occupied references and falls back safely") {
  auto& p = chipnomadState->project;
  p.tracksCount = 8;
  p.song[5][0] = 0x10;
  p.chains[0x11].rows[15].phrase = 0;
  p.chainToPatternMap[0x12] = 0;
  auto original = std::make_unique<Project>(p);
  CHECK(grooveboxFindFreeChain(&p, 0) == 0x13);
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
  for (int c = 0x10; c <= 0x1F; ++c) p.chainToPatternMap[c] = 0;
  CHECK(grooveboxFindFreeChain(&p, 0) == 0x90);
  for (int c = 0x90; c <= 0x9F; ++c) p.chainToPatternMap[c] = 0;
  CHECK(grooveboxFindFreeChain(&p, 0) == 0x00);
  for (int c = 0x80; c <= 0x8F; ++c) p.chainToPatternMap[c] = 0;
  CHECK(grooveboxFindFreeChain(&p, 7) == 0x00);
  for (int c = 0; c < 16; ++c) p.chainToPatternMap[c] = 0;
  CHECK(grooveboxFindFreeChain(&p, 7) == 0x20);
  for (auto& map : p.chainToPatternMap) map = 0;
  CHECK(grooveboxFindFreeChain(&p, 0) == EMPTY_VALUE_16);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox Start delegates to standard app transport in both pages") {
  chipnomadState->project.song[0][0] = 0;
  chipnomadState->project.chains[0].rows[0].phrase = 0;
  auto original = std::make_unique<Project>(chipnomadState->project);
  for (int page = 0; page < 2; ++page) {
    CHECK(screenGroovebox.onInput(1, keyPlay, 1) == 0);
    CHECK(screenGroovebox.onInput(1, keyPlay, 2) == 0);
    CHECK(screenGroovebox.onInput(1, keyShift | keyPlay, 1) == 0);
    screenGroovebox.onInput(1, keyOpt, 1);
    screenGroovebox.onInput(0, 0, 0);
  }
  CHECK(std::memcmp(original.get(), &chipnomadState->project, sizeof(Project)) == 0);
  CHECK_FALSE(projectModified);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox empty scene preparation is atomic banked and undoable") {
  auto& p = chipnomadState->project;
  p.tracksCount = 8;
  appSettings.defaultPatternLength = 3;
  auto original = std::make_unique<Project>(p);
  screenGroovebox.setup(0);
  for (int t = 0; t < 8; ++t) {
    REQUIRE(p.song[0][t] == (t + 1) * 0x10);
    CHECK(grooveboxTrackLength(&p, 0, t) == 3);
    for (int b = 0; b < 3; ++b) {
      CHECK(p.chains[p.song[0][t]].rows[b].phrase == t * 0x80 + b);
      CHECK(phraseIsEmpty(&p, t * 0x80 + b));
    }
  }
  CHECK(projectModified);
  REQUIRE(grooveboxAction(GrooveboxAction::undo, 0, 0));
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
  for (auto& phrase : p.phrases) phrase.rows[0].note = 48;
  for (int i = 0; i < 23; ++i) phraseClear(&p.phrases[i]);
  original = std::make_unique<Project>(p);
  projectModified = false;
  CHECK_FALSE(grooveboxPrepareScene(0));
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
  CHECK_FALSE(projectModified);
  CHECK(std::strstr(screenGetActiveMessage(), "No free phrases") != nullptr);
  for (auto& phrase : p.phrases) phraseClear(&phrase);
  for (auto& map : p.chainToPatternMap) map = 0;
  for (int i = 0; i < 7; ++i) p.chainToPatternMap[i] = EMPTY_VALUE_16;
  original = std::make_unique<Project>(p);
  CHECK_FALSE(grooveboxPrepareScene(0));
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
  CHECK(std::strstr(screenGetActiveMessage(), "No free chains") != nullptr);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox preparation reuses exclusive empty Clips references and preserves existing music") {
  auto& p = chipnomadState->project;
  p.song[0][0] = 0x13;
  p.chains[0x13].rows[5].transpose = 0xE8;
  auto original = std::make_unique<Project>(p);
  REQUIRE(grooveboxPrepareScene(0));
  CHECK(p.song[0][0] == 0x13);
  CHECK(p.chains[0x13].rows[0].phrase < PROJECT_MAX_PHRASES);
  REQUIRE(grooveboxAction(GrooveboxAction::undo, 0, 0));
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
  p.song[1][0] = 0x13;
  original = std::make_unique<Project>(p);
  REQUIRE(grooveboxPrepareScene(0));
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
  p.song[1][0] = EMPTY_VALUE_16;
  p.chains[0x13].rows[0].phrase = 7;
  original = std::make_unique<Project>(p);
  REQUIRE(grooveboxPrepareScene(0));
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox simplified Length header resizes selected track and restores focus") {
  auto& p = chipnomadState->project;
  appSettings.defaultPatternLength = 1;
  REQUIRE(grooveboxPrepareScene(0));
  screenGroovebox.onInput(1, keyShift | keyUp, 1);
  CHECK(mockCursorY == 0);
  CHECK(mockCursorX == 27);
  screenGroovebox.onInput(1, keyEdit | keyRight, 1);
  CHECK(grooveboxTrackLength(&p, 0, 0) == 2);
  CHECK(grooveboxTrackLength(&p, 0, 1) == 1);
  CHECK(bar == 0);
  auto original = std::make_unique<Project>(p);
  screenGroovebox.onInput(1, keyEdit | keyUp, 1);
  CHECK(grooveboxTrackLength(&p, 0, 0) == 4);
  REQUIRE(grooveboxAction(GrooveboxAction::undo, 0, 0));
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
  screenGroovebox.onInput(1, keyEdit | keyUp, 1);
  original = std::make_unique<Project>(p);
  screenGroovebox.onInput(1, keyEdit | keyDown, 1);
  CHECK(grooveboxTrackLength(&p, 0, 0) == 2);
  REQUIRE(grooveboxAction(GrooveboxAction::undo, 0, 0));
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
  screenGroovebox.onInput(1, keyEdit | keyOpt, 1);
  CHECK(grooveboxTrackLength(&p, 0, 0) == 1);
  screenGroovebox.onInput(1, keyEdit | keyLeft, 1);
  CHECK(grooveboxTrackLength(&p, 0, 0) == 1);
  screenGroovebox.onInput(1, keyShift | keyDown, 1);
  CHECK(mockCursorY == 13);
  screenGroovebox.onInput(1, keyShift | keyLeft, 1);
  CHECK(currentScreen == &screenSong);
  CHECK(grooveboxEditorActive);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox full-size new scene and Length limits are bounded") {
  auto& p = chipnomadState->project;
  appSettings.defaultPatternLength = 16;
  REQUIRE(grooveboxPrepareScene(0));
  for (int t = 0; t < 8; ++t) {
    CHECK(grooveboxTrackLength(&p, 0, t) == 16);
    for (int b = 0; b < 16; ++b)
      CHECK(p.chains[p.song[0][t]].rows[b].phrase == t * 0x80 + b);
  }
  auto original = std::make_unique<Project>(p);
  screenGroovebox.onInput(1, keyShift | keyUp, 1);
  screenGroovebox.onInput(1, keyEdit | keyRight, 1);
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
  screenGroovebox.onInput(1, keyEdit | keyUp, 1);
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
  screenGroovebox.onInput(1, keyEdit | keyDown, 1);
  CHECK(grooveboxTrackLength(&p, 0, 0) == 8);
  REQUIRE(grooveboxAction(GrooveboxAction::undo, 0, 0));
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox Scenes entry prepares new scene with one undo") {
  auto& p = chipnomadState->project;
  auto original = std::make_unique<Project>(p);
  row = 1;
  screenGroovebox.setup(1);
  for (int t = 0; t < 8; ++t) CHECK(p.song[1][t] == (t + 1) * 0x10);
  REQUIRE(grooveboxAction(GrooveboxAction::undo, 1, 0));
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox independence is atomic, includes retained bars and is undoable") {
  auto& p = chipnomadState->project;
  p.song[0][0] = p.song[1][0] = 4;
  p.chains[4].rows[0].phrase = p.chains[4].rows[1].phrase = 12;
  p.chains[4].rows[1].transpose = 232;
  p.chains[4].rows[8].phrase = 15;
  p.phrases[12].rows[0].note = 48;
  p.phrases[15].rows[0].fx[0] = {fxHOP, 0};
  auto original = std::make_unique<Project>(p);
  REQUIRE(grooveboxAction(GrooveboxAction::makeIndependent, 0, 0));
  int c = p.song[0][0];
  REQUIRE(c != 4);
  CHECK(p.song[1][0] == 4);
  CHECK(p.chains[c].rows[0].phrase != p.chains[c].rows[1].phrase);
  CHECK(p.chains[c].rows[1].transpose == 232);
  CHECK(p.chains[c].rows[2].phrase == EMPTY_VALUE_16);
  CHECK(p.phrases[p.chains[c].rows[8].phrase].rows[0].fx[0].command == fxHOP);
  CHECK_FALSE(grooveboxCellShared(&p, 0, 0, 0));
  REQUIRE(grooveboxAction(GrooveboxAction::undo, 0, 0));
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
  for (auto& phrase : p.phrases) phrase.rows[0].note = 48;
  phraseClear(&p.phrases[100]);
  original = std::make_unique<Project>(p);
  CHECK_FALSE(grooveboxAction(GrooveboxAction::makeIndependent, 0, 0));
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox double and shorten preserve transpose and undo references") {
  auto& p = chipnomadState->project;
  REQUIRE(grooveboxAction(GrooveboxAction::emptyBar, 0, 0));
  int c = p.song[0][0], f = p.chains[c].rows[0].phrase;
  p.chains[c].rows[0].transpose = 24;
  p.phrases[f].rows[5].fx[2] = {fxHOP, 0};
  REQUIRE(grooveboxAction(GrooveboxAction::doubleLength, 0, 0));
  CHECK(grooveboxTrackLength(&p, 0, 0) == 2);
  CHECK(p.chains[c].rows[1].phrase != f);
  CHECK(p.chains[c].rows[1].transpose == 24);
  CHECK(p.phrases[p.chains[c].rows[1].phrase].rows[5].fx[2].command == fxHOP);
  REQUIRE(grooveboxAction(GrooveboxAction::shorten, 0, 0));
  CHECK(grooveboxTrackLength(&p, 0, 0) == 1);
  REQUIRE(grooveboxAction(GrooveboxAction::undo, 0, 0));
  CHECK(grooveboxTrackLength(&p, 0, 0) == 2);
  p.chains[c].rows[2].phrase = EMPTY_VALUE_16;
  p.chains[c].rows[3].phrase = 50;
  auto original = std::make_unique<Project>(p);
  CHECK_FALSE(grooveboxAction(GrooveboxAction::doubleLength, 0, 0));
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox linked scene copy never overwrites and uses no slots") {
  auto& p = chipnomadState->project;
  REQUIRE(grooveboxAction(GrooveboxAction::offBar, 0, 0));
  p.songHighlight[0][0] = 1;
  REQUIRE(grooveboxAction(GrooveboxAction::copyScene, 0, 0));
  REQUIRE(grooveboxAction(GrooveboxAction::pasteScene, 1, 0));
  CHECK(p.song[1][0] == p.song[0][0]);
  CHECK(p.songHighlight[1][0] == 1);
  CHECK(grooveboxCellShared(&p, 1, 0, 0));
  auto original = std::make_unique<Project>(p);
  CHECK_FALSE(grooveboxAction(GrooveboxAction::pasteScene, 0, 0));
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
  REQUIRE(grooveboxAction(GrooveboxAction::undo, 1, 0));
  CHECK(p.song[1][0] == EMPTY_VALUE_16);
  CHECK(p.songHighlight[1][0] == 0);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox note edits undo allocated slots and scene navigation is read-only") {
  auto& p = chipnomadState->project;
  auto original = std::make_unique<Project>(p);
  row = 1;
  screenGroovebox.setup(-1);
  screenGroovebox.draw();
  CHECK_FALSE(projectModified);
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
  screenGroovebox.onInput(1, keyEdit, 1);
  REQUIRE(p.song[1][0] < PROJECT_MAX_CHAINS);
  REQUIRE(grooveboxAction(GrooveboxAction::undo, 1, 0));
  CHECK(std::memcmp(original.get(), &p, sizeof(Project)) == 0);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox live scene is one queue command and rejects invalid destinations") {
  auto& p = chipnomadState->project;
  p.tracksCount = 8;
  for (int t = 0; t < 8; ++t) REQUIRE(grooveboxAction(GrooveboxAction::offBar, 0, t));
  chipnomadInitChips(chipnomadState, 48000, nullptr);
  REQUIRE(chipnomadReserveRenderBuffers(chipnomadState, 512) == 0);
  for (int i = 0; i < 62; ++i) REQUIRE(chipnomadQueuePlaybackStopPreview(chipnomadState, 0));
  REQUIRE(chipnomadQueuePlaybackQueueLiveScene(chipnomadState, 0, 0));
  CHECK_FALSE(chipnomadQueuePlaybackQueueLiveScene(chipnomadState, 0, 0));
  float buffer[1024];
  chipnomadRender(chipnomadState, buffer, 512);
  const auto* status = chipnomadGetPlaybackStatus(chipnomadState);
  for (int t = 0; t < 8; ++t)
    CHECK((status->tracks[t].mode == PlaybackMode::live || status->tracks[t].queue.mode == PlaybackMode::live));
  CHECK_FALSE(chipnomadQueuePlaybackQueueLiveScene(chipnomadState, -1, 0));
  p.song[1][3] = PROJECT_MAX_CHAINS;
  CHECK_FALSE(chipnomadQueuePlaybackQueueLiveScene(chipnomadState, 1, 0));
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox live scene boundaries retain differing canonical lengths") {
  auto& p = chipnomadState->project;
  p.tracksCount = 2;
  p.tickRate = 60;
  for (auto& speed : p.grooves[0].speed) speed = 1;
  for (int t = 0; t < 2; ++t) {
    REQUIRE(grooveboxAction(GrooveboxAction::emptyBar, 0, t));
    REQUIRE(grooveboxAction(GrooveboxAction::emptyBar, 1, t));
  }
  REQUIRE(grooveboxAction(GrooveboxAction::emptyBar, 0, 1));
  chipnomadInitChips(chipnomadState, 48000, nullptr);
  REQUIRE(chipnomadReserveRenderBuffers(chipnomadState, 512) == 0);
  float buffer[1024];
  REQUIRE(chipnomadQueuePlaybackQueueLiveScene(chipnomadState, 0, 0));
  chipnomadRender(chipnomadState, buffer, 512);
  REQUIRE(chipnomadQueuePlaybackQueueLiveScene(chipnomadState, 1, 0));
  for (int i = 0; i < 30; ++i) chipnomadRender(chipnomadState, buffer, 512);
  auto* status = chipnomadGetPlaybackStatus(chipnomadState);
  CHECK(status->tracks[0].songRow == 1);
  CHECK(status->tracks[1].songRow == 0);
  CHECK(status->tracks[1].queue.songRow == 1);
  for (int i = 0; i < 30; ++i) chipnomadRender(chipnomadState, buffer, 512);
  status = chipnomadGetPlaybackStatus(chipnomadState);
  CHECK(status->tracks[1].songRow == 1);
  CHECK(grooveboxTrackLength(&p, 0, 0) == 1);
  CHECK(grooveboxTrackLength(&p, 0, 1) == 2);
}

TEST_CASE_FIXTURE(GrooveboxUIFixture, "Groovebox full-width tools show the complete root action explanation") {
  static const SelectionItem items[] = {
    {"Make independent", 0, nullptr, 0,
      "Copies all bars incl. retained data. New IDs can change conditional FX. Undo available."}
  };
  selectionPopupSetup("TOOLS", items, 1, 0, nullptr, nullptr, true);
  screenSelectionPopup.fullRedraw();
  std::string helper;
  for (int row = 15; row <= 18; ++row) {
    std::string line(mockGfxCells[row], 40);
    auto last = line.find_last_not_of(' ');
    if (last != std::string::npos) helper += line.substr(0, last + 1) + " ";
  }
  CHECK(helper.find("New IDs can change conditional FX.") != std::string::npos);
  CHECK(helper.find("Undo available.") != std::string::npos);
}
