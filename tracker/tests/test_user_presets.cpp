#include "doctest.h"
#include "user_presets.h"
#include "monk_patch.h"
#include "packaged_presets.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <cstring>
#include "screens/user_preset_browser.h"
#include "screens/selection_popup.h"
#include "screens/screen_instrument.h"
#include "chipnomad_lib.h"
#include "app_ui_mock.h"

namespace fs = std::filesystem;
namespace {
const char* compressed = "tests/fixtures/preset-packs/compressed.zip";
const char* stored = "tests/fixtures/preset-packs/stored.zip";
size_t find(const UserPresets& browser, const std::string& name) {
  for (size_t i=0;i<browser.items().size();++i) if(browser.items()[i].name==name) return i;
  FAIL("Missing browser item: " << name); return 0;
}
struct Fixture {
  fs::path root="test-user-preset-library";
  Fixture(){fs::remove_all(root);fs::create_directories(root/"My sounds");fs::copy_file(compressed,root/"Collection.zip");}
  ~Fixture(){fs::remove_all(root);}
};
void write(const fs::path& path,const std::vector<uint8_t>& bytes){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
}

TEST_CASE("Preset ZIP stored and deflated entries agree without extracting") {
  PresetZip a,b;std::string error;REQUIRE(a.open(stored,error));REQUIRE(b.open(compressed,error));
  REQUIRE(a.entries().size()==b.entries().size());
  for(const auto& e:a.entries()){
    std::vector<uint8_t>x,y;REQUIRE(a.read(e.name,x,error));REQUIRE(b.read(e.name,y,error));CHECK(x==y);
  }
  std::vector<uint8_t> unchanged{1,2,3};CHECK_FALSE(b.read("missing.cni",unchanged,error));CHECK(unchanged==std::vector<uint8_t>{1,2,3});
  for(const auto* path:{"../outside.cni","/absolute.cni","a/../b.cni","C:/x.cni","a\\b.cni","a//b.cni"})CHECK_FALSE(PresetZip::safePath(path));
  CHECK(PresetZip::safePath("Collection/Bass/Preset.cni"));
}

TEST_CASE("Preset ZIP rejects truncated and damaged input transactionally") {
  Fixture f;std::vector<uint8_t> data;std::string error;REQUIRE(readPresetFile(stored,data,error));
  auto corrupted=data;corrupted[40]^=1;write(f.root/"bad.zip",corrupted);
  PresetZip zip;std::vector<uint8_t> output{42};
  CHECK_FALSE(zip.open((f.root/"bad.zip").string(),error));
  REQUIRE(zip.open(stored,error));
  const auto entry=zip.entries().front();
  REQUIRE(entry.size>0);
  corrupted=data;corrupted[entry.offset]^=1;write(f.root/"crc.zip",corrupted);
  REQUIRE(zip.open((f.root/"crc.zip").string(),error));
  CHECK_FALSE(zip.read(entry.name,output,error));
  CHECK(error=="Preset ZIP checksum mismatch");
  CHECK(output==std::vector<uint8_t>{42});
  data.resize(data.size()-10);write(f.root/"short.zip",data);CHECK_FALSE(zip.open((f.root/"short.zip").string(),error));CHECK(zip.entries().empty());
}

TEST_CASE("USER browses a complete factory collection ZIP without installing its catalog") {
  Fixture f;
  fs::copy_file("packaging/common/instruments/FACTORY/2-the-fat-man-4-op.zip",f.root/"Hundreds of sounds.zip");
  UserPresets b;std::string error;b.setup(f.root.string(),InstrumentType::OPL3);REQUIRE(b.refresh(error));
  REQUIRE(b.enter(find(b,"Hundreds of sounds.zip"),error));
  REQUIRE(b.items().size()>100);
  auto p=std::make_unique<Project>();projectInit(p.get());
  size_t loaded=0;
  for(size_t i=0;i<b.items().size();++i)if(b.items()[i].kind==UserPresets::Kind::preset){
    REQUIRE(b.load(i,p.get(),0,error));CHECK(p->instruments[0].type==InstrumentType::OPL3);++loaded;
  }
  CHECK(loaded>100);projectFree(p.get());
}

TEST_CASE("USER follows real folders and ZIP folders and filters CNI engine types") {
  Fixture f;UserPresets b;std::string error;b.setup(f.root.string(),InstrumentType::OPL3);REQUIRE(b.refresh(error));
  REQUIRE(b.enter(find(b,"My sounds/"),error));CHECK(b.items().empty());CHECK_FALSE(b.atRoot());REQUIRE(b.back(error));
  REQUIRE(b.enter(find(b,"Collection.zip"),error));REQUIRE(b.enter(find(b,"Collection/"),error));
  REQUIRE(b.items().size()==2);REQUIRE(b.enter(find(b,"Bass/"),error));REQUIRE(b.items().size()==1);
  auto p=std::make_unique<Project>();projectInit(p.get());
  REQUIRE(b.load(0,p.get(),3,error));CHECK(p->instruments[3].type==InstrumentType::OPL3);
  CHECK(std::string(p->instruments[3].name)=="Acoustic Grand");
  auto saved=p->instruments[3];
  REQUIRE(b.back(error));REQUIRE(b.back(error));REQUIRE(b.enter(find(b,"Other/"),error));CHECK(b.items().empty());
  CHECK_FALSE(b.load(0,p.get(),3,error));CHECK(!memcmp(&saved,&p->instruments[3],sizeof(saved)));
  fs::remove(f.root/"Collection.zip");CHECK(!memcmp(&saved,&p->instruments[3],sizeof(saved)));projectFree(p.get());
}

TEST_CASE("USER DX7 bank opens into 32 patches and native files remain private folders") {
  Fixture f;UserPresets b;std::string error;b.setup(f.root.string(),InstrumentType::DX7);REQUIRE(b.refresh(error));
  REQUIRE(b.enter(find(b,"Collection.zip"),error));REQUIRE(b.enter(find(b,"DX7/"),error));
  REQUIRE(b.enter(find(b,"Test.syx"),error));CHECK(b.items().size()==32);
  auto p=std::make_unique<Project>();projectInit(p.get());
  REQUIRE(b.load(31,p.get(),0,error));CHECK(p->instruments[0].type==InstrumentType::DX7);CHECK(p->instruments[0].chip.dx7.sourceProgram==31);
  REQUIRE(b.back(error));CHECK(b.items().size()==1);projectFree(p.get());
}

TEST_CASE("USER loose CNI discovery does not require a TSV or follow symlinks") {
  Fixture f;PresetZip zip;std::vector<uint8_t> data;std::string error;REQUIRE(zip.open(stored,error));REQUIRE(zip.read("Collection/OPL3.cni",data,error));
  write(f.root/"My sounds"/"Sound.cni",data);
  write(f.root/"bad.cni",std::vector<uint8_t>{'b','a','d'});
  std::error_code ec;fs::create_directory_symlink(f.root,f.root/"loop",ec);
  UserPresets b;b.setup(f.root.string(),InstrumentType::OPL3);REQUIRE(b.refresh(error));CHECK(b.items().size()==2);
  REQUIRE(b.enter(find(b,"My sounds/"),error));REQUIRE(b.items().size()==1);
  b.setup(f.root.string(),InstrumentType::OPL3);REQUIRE(b.refresh(error));CHECK(b.label()=="My sounds");
  auto p=std::make_unique<Project>();projectInit(p.get());REQUIRE(b.load(0,p.get(),0,error));projectFree(p.get());
  b.setup(f.root.string(),InstrumentType::OPL2);REQUIRE(b.refresh(error));REQUIRE(b.enter(find(b,"My sounds/"),error));CHECK(b.items().empty());
}

TEST_CASE("USER preserves access to previous banks alongside the new USER folder") {
  Fixture f;UserPresets b;std::string error;
  b.setup((f.root/"USER").string(),InstrumentType::OPL3,f.root.string());
  REQUIRE(b.refresh(error));REQUIRE(b.items().size()==1);
  REQUIRE(b.enter(find(b,"Previous banks folder/"),error));
  REQUIRE(b.enter(find(b,"Collection.zip"),error));
  REQUIRE(b.enter(find(b,"Collection/"),error));
  REQUIRE(b.enter(find(b,"Bass/"),error));
  auto p=std::make_unique<Project>();projectInit(p.get());
  REQUIRE(b.load(0,p.get(),0,error));CHECK(p->instruments[0].type==InstrumentType::OPL3);
  projectFree(p.get());
  while(!b.atRoot())REQUIRE(b.back(error));
  fs::create_directories(f.root/"USER");
  fs::copy_file(compressed,f.root/"USER"/"New pack.zip");
  REQUIRE(b.refresh(error));REQUIRE(b.items().size()==2);
  CHECK(b.items()[find(b,"Previous banks folder/")].legacy);
  REQUIRE(b.enter(find(b,"New pack.zip"),error));
  REQUIRE(b.enter(find(b,"Collection/"),error));
  REQUIRE(b.enter(find(b,"Bass/"),error));
  CHECK(b.items().size()==1);CHECK(fs::exists(f.root/"Collection.zip"));
}

TEST_CASE("Every factory CNI remains loadable from its portable ZIP collection") {
  auto p=std::make_unique<Project>();projectInit(p.get());fillFXNames();
  auto entries=packagedPresets();REQUIRE(entries.size()==1196);
  for(const auto& e:entries){CAPTURE(e.path);REQUIRE(!e.archive.empty());REQUIRE(loadFMPreset("packaging/common/instruments/FACTORY",e,p.get(),0));CHECK(int(p->instruments[0].type)==e.type);}
  projectFree(p.get());
}

TEST_CASE("USER popup selects a ZIP preset and backs through folders without changing a song") {
  Fixture f;
  const auto original=fs::current_path();
  fs::create_directories(f.root/"instruments/USER/opl3");
  fs::copy_file(compressed,f.root/"instruments/USER/opl3/Pack.zip");
  struct Restore {fs::path path;~Restore(){fs::current_path(path);}} restore{original};
  fs::current_path(f.root);
  auto* prior=chipnomadState;auto* state=chipnomadCreate();chipnomadState=state;
  screensInitAll();cInstrument=0;getInstrumentFunctions(InstrumentType::OPL3).init(&state->project.instruments[0]);
  state->project.instruments[0].type=InstrumentType::OPL3;
  auto before=state->project.instruments[0];
  auto select=[] {screenSelectionPopup.onInput(1,keyEdit,0);screenSelectionPopup.onInput(0,keyEdit,0);};
  openUserPresetBrowser();CHECK(currentScreen==&screenSelectionPopup);CHECK(selectionPopupIsFullWidth());
  select(); // Pack.zip
  select(); // Collection/
  select(); // Bass/
  CHECK(!memcmp(&before,&state->project.instruments[0],sizeof(before)));
  select(); // Acoustic Grand
  CHECK(currentScreen==&screenInstrument);CHECK(std::string(state->project.instruments[0].name)=="Acoustic Grand");
  openUserPresetBrowser(); // Return to Bass/, not to the root.
  auto selected=state->project.instruments[0];
  for(int i=0;i<4;++i)screenSelectionPopup.onInput(1,keyOpt,0);
  CHECK(currentScreen==&screenInstrument);CHECK(!memcmp(&selected,&state->project.instruments[0],sizeof(selected)));
  chipnomadDestroy(state);chipnomadState=prior;
}

TEST_CASE("ALL USER scan flattens compatible files ZIPs and DX7 bank voices without moving USER") {
  Fixture f;PresetZip zip;std::vector<uint8_t> data;std::string error;
  REQUIRE(zip.open(stored,error));REQUIRE(zip.read("Collection/OPL3.cni",data,error));
  write(f.root/"My sounds"/"Loose.cni",data);
  UserPresets b;b.setup(f.root.string(),InstrumentType::OPL3);REQUIRE(b.refresh(error));
  REQUIRE(b.enter(find(b,"My sounds/"),error));
  auto refs=b.scan(error);CHECK(error.empty());REQUIRE(refs.size()==3);
  CHECK(b.label()=="My sounds");REQUIRE(b.items().size()==1);
  auto p=std::make_unique<Project>();projectInit(p.get());
  for(const auto& ref:refs){REQUIRE(b.load(ref,p.get(),0,error));CHECK(p->instruments[0].type==InstrumentType::OPL3);}
  auto before=p->instruments[0];
  fs::remove(f.root/"Collection.zip");
  for(const auto& ref:refs)if(!ref.archive.empty()){CHECK_FALSE(b.load(ref,p.get(),0,error));CHECK(!memcmp(&before,&p->instruments[0],sizeof(before)));}
  fs::copy_file(compressed,f.root/"Collection.zip");
  b.setup(f.root.string(),InstrumentType::DX7);refs=b.scan(error);CHECK(error.empty());REQUIRE(refs.size()==32);
  for(size_t n=0;n<refs.size();++n){REQUIRE(b.load(refs[n],p.get(),0,error));CHECK(p->instruments[0].chip.dx7.sourceProgram==int(n));}
  b.setup(f.root.string(),InstrumentType::OPL2);refs=b.scan(error);REQUIRE(refs.size()==1);
  REQUIRE(b.load(refs[0],p.get(),0,error));CHECK(p->instruments[0].type==InstrumentType::OPL2);
  projectFree(p.get());
}

TEST_CASE("ALL USER scan stops deep folders and retains direct USER navigation") {
  Fixture f;auto path=f.root;
  for(int i=0;i<34;++i){path/="Nested";fs::create_directory(path);}
  UserPresets b;std::string error;b.setup(f.root.string(),InstrumentType::OPL3);
  auto refs=b.scan(error);CHECK(error=="ALL: USER scan limit reached");CHECK(refs.size()==2);CHECK(b.atRoot());
  REQUIRE(b.enter(find(b,"Collection.zip"),error));CHECK(b.label()=="Collection.zip");
}

TEST_CASE("Collection UI retains top USER and ALL Unsorted loads a zipped DX7 bank voice") {
  Fixture f;const auto original=fs::current_path();
  fs::create_directories(f.root/"instruments/USER/dx7");
  fs::copy_file(compressed,f.root/"instruments/USER/dx7/Pack.zip");
  fs::copy("packaging/common/instruments/FACTORY",f.root/"instruments/FACTORY",fs::copy_options::recursive);
  struct Restore {fs::path path;~Restore(){fs::current_path(path);}} restore{original};fs::current_path(f.root);
  auto* prior=chipnomadState;auto* state=chipnomadCreate();chipnomadState=state;screensInitAll();cInstrument=0;
  getInstrumentFunctions(InstrumentType::DX7).init(&state->project.instruments[0]);
  instrumentFMSetContext(0,InstrumentType::DX7);
  auto text=[] {screenSelectionPopup.fullRedraw();std::string s;for(auto& row:mockGfxCells)s.append(row,40);return s;};
  auto press=[](int key){screenSelectionPopup.onInput(1,key,0);screenSelectionPopup.onInput(0,key,0);};
  instrumentPresetOpenCollections();auto menu=text();
  CHECK(menu.find("Factory Presets")!=std::string::npos);CHECK(menu.find("OpenDX7 Originals")!=std::string::npos);
  CHECK(menu.find("YSE")==std::string::npos);CHECK(menu.find("USER")!=std::string::npos);
  press(keyUp);press(keyEdit);CHECK(std::string(instrumentPresetCollectionName())=="USER");
  instrumentPresetOpenSounds();CHECK(selectionPopupIsFullWidth());CHECK(text().find("Pack.zip")!=std::string::npos);press(keyOpt);
  instrumentPresetOpenCollections();press(keyDown);press(keyEdit);CHECK(std::string(instrumentPresetCollectionName())=="ALL");
  auto before=state->project.instruments[0];instrumentPresetOpenSounds();
  for(int n=0;n<24&&text().find("> Unsorted")==std::string::npos;++n)press(keyDown);
  CHECK(text().find("> Unsorted")!=std::string::npos);CHECK(text().find("TEST VOICE")!=std::string::npos);
  CHECK(!memcmp(&before,&state->project.instruments[0],sizeof(before)));
  press(keyRight);press(keyUp);press(keyEdit); // Last voice in the user bank.
  CHECK(currentScreen==&screenInstrument);CHECK(std::string(state->project.instruments[0].name)=="TEST VOICE");
  CHECK(state->project.instruments[0].chip.dx7.sourceProgram==31);
  for(auto type:{InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::SegaPSG}) {
    getInstrumentFunctions(type).init(&state->project.instruments[0]);instrumentFMSetContext(0,type);instrumentPresetOpenCollections();menu=text();
    CHECK(menu.find(type==InstrumentType::SegaPSG?"Factory":"Factory Presets")!=std::string::npos);CHECK(menu.find("USER")!=std::string::npos);
    CHECK(menu.find("OpenDX7")==std::string::npos);CHECK(menu.find("YM2413")==std::string::npos);press(keyOpt);
  }
  chipnomadDestroy(state);chipnomadState=prior;
}

TEST_CASE("USER references restore nested ZIP and bank location for seamless stepping") {
  Fixture f;UserPresets b;std::string error;b.setup(f.root.string(),InstrumentType::DX7);
  auto refs=b.scan(error);REQUIRE(refs.size()==32);
  REQUIRE(b.focus(refs[31],error));CHECK(b.label()=="Test.syx");
  REQUIRE(b.items().size()==32);CHECK(b.reference(31).voice==31);CHECK(b.reference(31).archive=="Collection.zip");
  REQUIRE(b.back(error));CHECK(b.label()=="DX7");
  b.setup(f.root.string(),InstrumentType::OPL3);refs=b.scan(error);REQUIRE(refs.size()==2);
  for(const auto& ref:refs){REQUIRE(b.focus(ref,error));bool found=false;for(size_t i=0;i<b.items().size();++i)if(b.reference(i).path==ref.path)found=true;CHECK(found);}
  const auto label=b.label();auto missing=refs.front();missing.path="Missing/absent.cni";
  CHECK_FALSE(b.focus(missing,error));CHECK(b.label()==label);
}

TEST_CASE("SID single-file names retain model suffixes and ignore stale source names") {
  Fixture f;
  std::vector<uint8_t> goat(33);memcpy(goat.data(),"GTI5",4);
  memcpy(goat.data()+13,"Wrong name",10);
  write(f.root/"Violin_8580.ins",goat);
  write(f.root/"Violin_6581.ins",goat);
  UserPresets library;std::string error;library.setup(f.root.string(),InstrumentType::SID);
  REQUIRE(library.refresh(error));
  auto p=std::make_unique<Project>();projectInit(p.get());
  for(const char* label:{"Violin 6581","Violin 8580"}) {
    auto index=find(library,label);
    REQUIRE(library.load(index,p.get(),0,error));
    CHECK(std::string(p->instruments[0].chip.sid.presetName)==label);
    CHECK(std::string(p->instruments[0].name)==label);
    CHECK(memcmp(p->instruments[0].chip.sid.program.data,goat.data(),goat.size())==0);
    REQUIRE(library.focus(library.reference(index),error));
    CHECK(library.items()[0].name==label);
    REQUIRE(library.back(error));
  }
  projectFree(p.get());
}

TEST_CASE("SID USER columns keep model folders in one collection and restore the chosen sound") {
  Fixture f;
  const auto original=fs::current_path();
  fs::create_directories(f.root/"instruments/USER/sid");
  fs::copy_file("tests/fixtures/preset-packs/sid-models.zip",f.root/"instruments/USER/sid/Chip Pack.zip");
  struct Restore {fs::path path;~Restore(){fs::current_path(path);}} restore{original};
  fs::current_path(f.root);
  auto* prior=chipnomadState;auto* state=chipnomadCreate();chipnomadState=state;
  screensInitAll();cInstrument=0;
  getInstrumentFunctions(InstrumentType::SID).init(&state->project.instruments[0]);
  auto press=[](int key){screenSelectionPopup.onInput(1,key,0);screenSelectionPopup.onInput(0,key,0);};
  openUserPresetBrowser();
  REQUIRE(currentScreen==&screenSelectionPopup);
  CHECK_FALSE(selectionPopupIsFullWidth());
  screenSelectionPopup.fullRedraw();
  std::string collections,sounds;
  for(int row=2;row<18;++row){collections.append(mockGfxCells[row],18);sounds.append(mockGfxCells[row]+20,20);}
  const auto pack=collections.find("Chip Pack");
  REQUIRE(pack!=std::string::npos);
  CHECK(collections.find("Chip Pack",pack+1)==std::string::npos);
  CHECK(collections.find("6581")==std::string::npos);
  CHECK(collections.find("8580")==std::string::npos);
  CHECK(collections.find(".zip")==std::string::npos);
  CHECK(sounds.find("Violin 6581")!=std::string::npos);
  CHECK(sounds.find("Violin 8580")!=std::string::npos);
  press(keyRight);press(keyDown);press(keyEdit);
  REQUIRE(currentScreen==&screenInstrument);
  CHECK(std::string(state->project.instruments[0].chip.sid.presetName)=="Violin 8580");
  const auto selected=state->project.instruments[0];
  openUserPresetBrowser();press(keyRight);press(keyEdit);
  CHECK(currentScreen==&screenInstrument);
  CHECK(!memcmp(&selected,&state->project.instruments[0],sizeof(selected)));
  openUserPresetBrowser();press(keyOpt);
  CHECK(currentScreen==&screenInstrument);
  CHECK(!memcmp(&selected,&state->project.instruments[0],sizeof(selected)));
  chipnomadDestroy(state);chipnomadState=prior;
}

TEST_CASE("MONK defaults to ALL and preserves the bank while selecting and cycling both sources") {
  Fixture f;
  const auto original = fs::current_path();
  fs::create_directories(f.root / "instruments/FACTORY/monk");
  fs::create_directories(f.root / "instruments/USER/monk");
  struct Restore { fs::path path; ~Restore() { fs::current_path(path); } } restore{original};
  fs::current_path(f.root);
  auto* prior = chipnomadState;
  auto* state = chipnomadCreate(); chipnomadState = state;
  screensInitAll(); cInstrument = 0;
  auto& instrument = state->project.instruments[0];
  getInstrumentFunctions(InstrumentType::Monk).init(&instrument);
  CHECK(instrument.chip.monk.factory == monkBankAll);
  for (int bank : {monkBankFactory, monkBankUser}) {
    instrument.chip.monk.factory = bank;
    instrument.chip.monk.vowel = bank == monkBankFactory ? 32 : 224;
    std::strcpy(instrument.name, bank == monkBankFactory ? "Factory tone" : "User tone");
    REQUIRE(instrumentSave(&state->project, bank == monkBankFactory ?
      "instruments/FACTORY/monk/tone.cni" : "instruments/USER/monk/tone.cni", 0) == 0);
  }
  getInstrumentFunctions(InstrumentType::Monk).init(&instrument);
  auto press = [](int key) { screenSelectionPopup.onInput(1,key,0); screenSelectionPopup.onInput(0,key,0); };
  openUserPresetBrowser(); screenSelectionPopup.fullRedraw();
  std::string menu; for (auto& row : mockGfxCells) menu.append(row,40);
  CHECK(menu.find("ALL") != std::string::npos);
  CHECK(menu.find("FACTORY") != std::string::npos);
  CHECK(menu.find("USER") != std::string::npos);
  press(keyRight); press(keyEdit);
  REQUIRE(currentScreen == &screenInstrument);
  CHECK(instrument.chip.monk.factory == monkBankAll);
  const int first = instrument.chip.monk.vowel;
  CHECK((first == 32 || first == 224));
  cycleUserPreset(1);
  CHECK(instrument.chip.monk.factory == monkBankAll);
  CHECK(instrument.chip.monk.vowel == 256 - first);
  const auto selected = instrument;
  openUserPresetBrowser(); press(keyRight); press(keyEdit);
  CHECK(std::memcmp(&instrument, &selected, sizeof(instrument)) == 0);
  cycleUserPreset(1);
  CHECK(instrument.chip.monk.vowel == first);
  cycleUserPreset(-1);
  CHECK(instrument.chip.monk.vowel == 256 - first);
  REQUIRE(instrumentSave(&state->project, "all.cni", 0) == 0);
  instrument.chip.monk.factory = monkBankUser;
  REQUIRE(instrumentLoad(&state->project, "all.cni", 0) == 0);
  CHECK(instrument.chip.monk.factory == monkBankAll);
  for (int bank : {monkBankFactory, monkBankUser}) {
    instrument.chip.monk.factory = bank;
    cycleUserPreset(1);
    CHECK(instrument.chip.monk.factory == bank);
    CHECK(instrument.chip.monk.vowel == (bank == monkBankFactory ? 32 : 224));
  }
  chipnomadDestroy(state); chipnomadState = prior;
}
