#include "doctest.h"
#include "selection_popup.h"
#include "soundbank_browser.h"
#include "mocks/app_ui_mock.h"
#include "corelib_gfx.h"
#include "chipnomad_lib.h"
#include "project_utils.h"
#include "audio_manager.h"
#include "midi/midi_router.h"
#include <string>
#include <chrono>
#include <thread>

namespace {
std::vector<int> midiPrepared;
std::vector<std::pair<int, int>> midiPlayed;
int midiPrepareResult, midiStops;
struct MidiPopupFixture {
  ChipNomadState* savedState = chipnomadState;
  const AppScreen* savedScreen = currentScreen;
  AudioManager savedAudio = audioManager;
  SelectionItem items[2] = {{"One", 0}, {"Two", 1}};
  MidiPopupFixture() {
    chipnomadState = chipnomadCreate();
    projectInitAY(&chipnomadState->project);
    chipnomadInitChips(chipnomadState, 48000, nullptr);
    REQUIRE(chipnomadReserveRenderBuffers(chipnomadState, 2048) == 0);
    currentScreen = &screenSelectionPopup;
    midiPrepared.clear(); midiPlayed.clear(); midiStops = 0; midiPrepareResult = 1;
    audioManager.stopSamplePreview = [] { ++midiStops; };
    audioManager.previewMidiNote = [](uint8_t note, uint8_t velocity) {
      midiPlayed.emplace_back(note, velocity);
    };
    selectionPopupSetup("MIDI", items, 2, 0, nullptr, nullptr, false, nullptr,
      false, false, nullptr, false, [](int value) {
        midiPrepared.push_back(value); return midiPrepareResult;
      });
    midiStops = 0;
  }
  ~MidiPopupFixture() {
    selectionPopupSetup("", items, 2, 0, nullptr, nullptr);
    chipnomadDestroy(chipnomadState);
    chipnomadState = savedState; currentScreen = savedScreen; audioManager = savedAudio;
  }
  void note(int pitch = 48, int velocity = 93) {
    MidiPreviewIntent intent{};
    intent.note = pitch; intent.velocity = velocity; intent.noteOn = 1;
    selectionPopupMidiInput(intent);
  }
  void release() {
    MidiPreviewIntent intent{}; intent.stop = 1; selectionPopupMidiInput(intent);
  }
};
}

TEST_CASE("MIDI popup prepares only highlighted candidates and preserves pitch velocity and release") {
  MidiPopupFixture f;
  CHECK(selectionPopupHasMidiPreview());
  f.note(); f.note(55, 60);
  REQUIRE(midiPrepared.size() == 1);
  CHECK(midiPrepared[0] == 0);
  REQUIRE(midiPlayed.size() == 2);
  CHECK(midiPlayed[0] == std::make_pair(48, 93));
  CHECK(midiPlayed[1] == std::make_pair(55, 60));
  f.release();
  CHECK(midiPlayed.back().second == 0);
  screenSelectionPopup.onInput(1, keyDown, 1);
  CHECK(midiStops == 1);
  f.note();
  CHECK(midiPrepared.back() == 1);
  currentScreen = &screenInstrument;
  CHECK_FALSE(selectionPopupHasMidiPreview());
  screenSelectionPopup.draw();
  CHECK(midiStops == 2);
}

TEST_CASE("Pending MIDI bank audition cancels on release navigation exit and transport") {
  MidiPopupFixture f;
  midiPrepareResult = 0;
  f.note();
  CHECK(midiPlayed.empty());
  f.release();
  midiPrepareResult = 1;
  screenSelectionPopup.draw();
  CHECK(midiPlayed.size() == 1); // Only the release, never a delayed Note On.
  f.note();
  REQUIRE(midiPlayed.size() == 2);
  CHECK(midiPlayed.back().second == 93);
  midiPrepareResult = 0;
  screenSelectionPopup.onInput(1, keyDown, 1);
  f.note();
  screenSelectionPopup.onInput(1, keyUp, 1);
  midiPrepareResult = 1;
  screenSelectionPopup.draw();
  CHECK(midiPlayed.size() == 2);
  midiPrepareResult = 0;
  f.note();
  currentScreen = &screenInstrument;
  midiPrepareResult = 1;
  screenSelectionPopup.draw();
  CHECK(midiPlayed.size() == 2);
  currentScreen = &screenSelectionPopup;
  midiPrepareResult = 0;
  f.note();
  chipnomadState->project.song[0][0] = 0;
  chipnomadState->project.chains[0].rows[0].phrase = 0;
  REQUIRE(chipnomadQueueProjectRefresh(chipnomadState));
  REQUIRE(chipnomadQueuePlaybackStartPhrase(chipnomadState, 0, 0, 0, 1));
  float audio[4096];
  chipnomadRender(chipnomadState, audio, 2048);
  REQUIRE(chipnomadGetPlaybackStatus(chipnomadState)->transportPlaying);
  midiPrepareResult = 1;
  screenSelectionPopup.draw();
  CHECK(midiPlayed.size() == 2);
}

TEST_CASE("Returning to a MIDI popup invalidates its destroyed preview engine") {
  MidiPopupFixture f;
  f.note(); f.release();
  currentScreen = &screenInstrument;
  audioManager.stopSamplePreview();
  currentScreen = &screenSelectionPopup;
  screenSelectionPopup.setup(0);
  f.note();
  REQUIRE(midiPrepared.size() == 2);
  REQUIRE(midiPlayed.size() == 3);
  CHECK(midiPlayed.back() == std::make_pair(48, 93));
}

TEST_CASE("Collection preset selection stays below all eight track previews") {
  SelectionItem presets[24]{};
  std::string labels[24];
  for(int n=0;n<24;++n) { labels[n]="Preset "+std::to_string(n);presets[n]={labels[n].c_str(),n,nullptr,0,nullptr}; }
  SelectionItem collection{"Collection",-1,presets,24,nullptr};
  selectionPopupSetup("SOUNDFONT PRESETS",&collection,1,0,nullptr,nullptr,false,nullptr,true,true);
  for(int n=0;n<24;++n) {
    screenSelectionPopup.fullRedraw();
    CHECK(mockGfxCells[11][19]=='>');
    CHECK(std::string(mockGfxCells[11]+23,labels[n].size())==labels[n]);
    screenSelectionPopup.onInput(1,keyDown,1);
  }
  screenSelectionPopup.fullRedraw();
  CHECK(std::string(mockGfxCells[11]+23,8)=="Preset 0");
}
TEST_CASE("Long preset names scroll after idle and reset on selection changes") {
  SelectionItem presets[]={
    {"ABCDEFGHIJKLMNOPQRSTUVWXYZ long original preset",0,nullptr,0,nullptr},
    {"Second preset",1,nullptr,0,nullptr}};
  SelectionItem collection{"Collection",-1,presets,2,nullptr};
  selectionPopupSetup("SOUNDFONT PRESETS",&collection,1,0,nullptr,nullptr,false,nullptr,true,true);
  screenSelectionPopup.fullRedraw();screenSelectionPopup.draw();
  CHECK(mockGfxCells[11][23]=='A');
  std::this_thread::sleep_for(std::chrono::milliseconds(1200));
  screenSelectionPopup.draw();
  CHECK(mockGfxCells[11][23]!='A');
  screenSelectionPopup.onInput(1,keyDown,1);
  CHECK(std::string(mockGfxCells[11]+23,13)=="Second preset");
  screenSelectionPopup.onInput(1,keyUp,1);
  CHECK(mockGfxCells[11][23]=='A');
}

TEST_CASE("SoundFont folders enter directly and left returns through the panels") {
  static int selected, backs;
  selected=-1;backs=0;
  SelectionItem preset{"Organ",7,nullptr,0,nullptr};
  SelectionItem roots[]={{"Nested",-1000,&preset,1,nullptr,true},{"Leaf",-1001,&preset,1,nullptr}};
  auto setup=[&](int value) {selectionPopupSetup("Presets",roots,2,value,[](int n){selected=n;},[]{++backs;},false,nullptr,true,true,nullptr,true);};
  setup(-1000);screenSelectionPopup.onInput(1,keyRight,0);CHECK(selected==-1000);
  setup(-1000);selected=-1;screenSelectionPopup.onInput(1,keyEdit,0);CHECK(selected==-1000);
  setup(-1001);selected=-1;screenSelectionPopup.onInput(1,keyRight,0);CHECK(selected==-1);
  screenSelectionPopup.onInput(1,keyRight,0);CHECK(selected==-1);
  screenSelectionPopup.onInput(1,keyEdit,0);CHECK(selected==7);
  screenSelectionPopup.onInput(1,keyLeft,0);CHECK(backs==0);
  screenSelectionPopup.onInput(1,keyLeft,0);CHECK(backs==1);
}
TEST_CASE("SoundFont loading status occupies the third row and clears after completion") {
  static const char* status;
  status="Unzipping...";
  SelectionItem preset{"Organ",0,nullptr,0,nullptr};
  SelectionItem collection{"Collection",-1,&preset,1,nullptr};
  selectionPopupSetup("Presets",&collection,1,0,nullptr,nullptr,false,nullptr,true,true,[]{return status;},true);
  screenSelectionPopup.fullRedraw();CHECK(std::string(mockGfxCells[2],12)=="Unzipping...");
  status="Loading samples...";screenSelectionPopup.draw();CHECK(std::string(mockGfxCells[2],18)==status);
  status="";screenSelectionPopup.draw();CHECK(std::string(mockGfxCells[2],40)==std::string(40,' '));
  CHECK(mockGfxCells[11][19]=='>');
}

TEST_CASE("Collections use their full height and presets page without loading") {
  SelectionItem presets[40]{};
  std::string labels[40];
  for(int n=0;n<40;++n){labels[n]="Sound "+std::to_string(n);presets[n]={labels[n].c_str(),n,nullptr,0,nullptr};}
  SelectionItem collections[]={{"First",-1,presets,40,nullptr},{"Second",-2,presets,40,nullptr}};
  static int chosen;chosen=-999;
  selectionPopupSetup("Presets",collections,2,-1,[](int v){chosen=v;},nullptr,false,nullptr,true,false,nullptr,true);
  screenSelectionPopup.fullRedraw();
  CHECK(mockGfxCells[3][0]=='>');
  CHECK(std::string(mockGfxCells[19],40)==std::string(40,' '));
  CHECK(std::string(mockGfxCells[3]+4,5)=="First");
  CHECK(std::string(mockGfxCells[4]+4,6)=="Second");
  screenSelectionPopup.onInput(1,keyRight,0); // Enter the preset panel.
  screenSelectionPopup.onInput(1,keyRight,0); // Page down without selecting.
  CHECK(chosen==-999);
  CHECK(std::string(mockGfxCells[11]+23,8)=="Sound 15");
  screenSelectionPopup.onInput(1,keyRight,0);
  CHECK(std::string(mockGfxCells[11]+23,8)=="Sound 30");
  screenSelectionPopup.onInput(1,keyRight,0);
  CHECK(std::string(mockGfxCells[11]+23,8)=="Sound 39");
  screenSelectionPopup.onInput(1,keyEdit,0);CHECK(chosen==39);
  CHECK(screenSelectionPopup.onInput(1,keyPlay,0)==0);
  CHECK(screenSelectionPopup.getPlaybackLevel()==ScreenPlaybackLevel::song);
}

TEST_CASE("Collection browsing retains nested physical folders and hides ZIP sample trees") {
  std::vector<soundbank::BankEntry> banks;
  for(const char* id : {"Reddit/1offs/drumkits/Kick.sf2","Reddit/1offs/instruments/Organ.sf2",
      "Reddit/House/Bank/Collection.sf2","Sideloaded/VSCO/VSCO.zip::publisher/Strings/Violin.sfz"}) {
    soundbank::BankEntry b;b.id=id;b.format="sf2";b.presets.push_back({"0:0","Sound"});banks.push_back(b);
  }
  auto root=soundbank::browseCollections(banks,"");REQUIRE(root.size()==2);
  CHECK(root[0].name=="Reddit");CHECK(root[0].hasFolders);CHECK(root[0].presets.empty());
  auto reddit=soundbank::browseCollections(banks,"Reddit");REQUIRE(reddit.size()==2);
  CHECK(reddit[0].name=="1offs");CHECK(reddit[0].hasFolders);
  auto singles=soundbank::browseCollections(banks,"Reddit/1offs");REQUIRE(singles.size()==2);
  CHECK(singles[0].path=="Reddit/1offs/drumkits");CHECK(singles[0].presets.size()==1);
  auto vsco=soundbank::browseCollections(banks,"Sideloaded");REQUIRE(vsco.size()==1);
  CHECK_FALSE(vsco[0].hasFolders);CHECK(vsco[0].presets.size()==1);
}
TEST_CASE("An umbrella collection does not hide unrelated one-off samples") {
  std::vector<soundbank::BankEntry> banks;
  for(auto item : {std::pair{"Open/Gugak/Strings.exs","exs"},
      std::pair{"Open/Gugak/Samples/string.wav","wav"},
      std::pair{"Open/StargateDrums/kick.wav","wav"},
      std::pair{"Open/Fluid/Fluid.sf2","sf2"}}) {
    soundbank::BankEntry b;b.id=item.first;b.format=item.second;b.presets.push_back({"sample","Sound"});banks.push_back(b);
  }
  const soundbank::SupportingSamples supporting(banks);
  CHECK(supporting.hides(banks[1]));CHECK_FALSE(supporting.hides(banks[2]));
  const auto groups=soundbank::browseCollections(banks,"Open");
  REQUIRE(groups.size()==3);CHECK(groups[2].name=="StargateDrums");CHECK(groups[2].presets.size()==1);
}
