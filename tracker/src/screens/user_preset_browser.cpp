#include "opl_patch.h"
#include "opll_presets.h"
#include "four_op_patch.h"
#include "simple_chip_presets.h"
#include "user_preset_browser.h"
#include "screen_instrument.h"
#include "selection_popup.h"
#include "user_presets.h"
#include "corelib_file.h"
#include "chipnomad_lib.h"
#include "project_utils.h"
#include "audio_manager.h"
#include "monk_patch.h"
#include <map>
#include <memory>
#include <cstring>
#include <tuple>
#include <filesystem>

namespace {
InstrumentType type = InstrumentType::none;
int selectedBank(){const auto&i=chipnomadState->project.instruments[cInstrument];return i.type==InstrumentType::Monk ? i.chip.monk.factory : 0;}
bool factorySelected(){return selectedBank()==monkBankFactory;}
bool allSelected(){return chipnomadState->project.instruments[cInstrument].type==InstrumentType::Monk&&selectedBank()==monkBankAll;}
using LibraryKey=std::pair<InstrumentType,int>;
LibraryKey libraryKey(){return {type,selectedBank()};}
std::map<LibraryKey, UserPresets> libraries;
UserPresets* library = nullptr;
std::vector<SelectionItem> choices;
std::vector<std::string> labels;
bool userColumns=false;
std::vector<UserPresets::Reference> userReferences;
std::vector<std::vector<SelectionItem>> collectionSounds;
std::vector<std::string> collectionPaths;
std::unique_ptr<Project> audition;
struct Cursor {bool valid=false;UserPresets::Reference selected;};
std::map<std::tuple<int,InstrumentType,int>,Cursor> cursors;
std::map<LibraryKey,std::vector<UserPresets::Reference>> sequences;
struct PresetLoad {
  UserPresets* library;
  const UserPresets::Reference* reference;
  int value;
  int instrument;
  std::string* error;
};
int loadPreset(void* userdata) {
  auto* load = static_cast<PresetLoad*>(userdata);
  const auto& before = chipnomadState->project.instruments[load->instrument];
  const int bank = before.type == InstrumentType::Monk ? before.chip.monk.factory : monkBankUser;
  bool loaded = (load->reference
    ? load->library->load(*load->reference, &chipnomadState->project, load->instrument, *load->error)
    : load->library->load(load->value, &chipnomadState->project, load->instrument, *load->error));
  auto& after = chipnomadState->project.instruments[load->instrument];
  if (loaded && after.type == InstrumentType::Monk) after.chip.monk.factory = bank;
  return !loaded;
}
bool samePreset(const UserPresets::Reference& a,const UserPresets::Reference& b) {
  return a.path==b.path&&a.archive==b.archive&&a.voice==b.voice&&a.legacy==b.legacy;
}
Cursor& cursor(){return cursors[{cInstrument,chipnomadState->project.instruments[cInstrument].type,selectedBank()}];}

bool setupBrowserLibrary(UserPresets& target) {
  if (!allSelected()) return setupUserPresetLibrary(target, type, factorySelected());
  UserPresets factory;
  if (!setupUserPresetLibrary(target, type, false) || !setupUserPresetLibrary(factory, type, true)) return false;
  // Reuse the browser's secondary-root support to retain each preset's origin.
  // In MONK ALL, a Reference marked legacy points at FACTORY, not previous banks.
  target.setup(target.rootPath(), type, factory.rootPath());
  return true;
}

constexpr int backValue = -2, emptyValue = -3;
void show();
void stop() {
  if (type == InstrumentType::Monk) chipnomadQueueMonkPreview(chipnomadState, *pSongTrack, nullptr);
  else if (type == InstrumentType::DX7) chipnomadQueueDX7Preview(chipnomadState, *pSongTrack, nullptr);
  else if (type == InstrumentType::SID) chipnomadQueueSIDPreview(chipnomadState, *pSongTrack, nullptr);
  else if (isOPLL(type)) chipnomadQueueOPLLPreview(chipnomadState, *pSongTrack, nullptr);
  else if (isOPL(type)) chipnomadQueueOPLPreview(chipnomadState, *pSongTrack, type, nullptr);
  else if (isFourOp(type)) chipnomadQueueFourOpPreview(chipnomadState, *pSongTrack, type, nullptr);
  else chipnomadQueueSimpleChipPreview(chipnomadState, *pSongTrack, type, nullptr);
  if (audition) { projectFree(audition.get()); audition.reset(); }
}
void preview(int value, bool held) {
  stop();
  if (!held || value < 0) return;
  if (userColumns) {
    if (size_t(value) >= userReferences.size()) return;
  } else if (size_t(value) >= library->items().size() ||
             library->items()[value].kind != UserPresets::Kind::preset) return;
  audition = std::make_unique<Project>(); projectInit(audition.get()); std::string error;
  if (!(userColumns ? library->load(userReferences[value], audition.get(), 0, error) :
                      library->load(value, audition.get(), 0, error))) { screenMessage(MESSAGE_TIME_ERROR, "%s", error.c_str()); return; }
  const auto& i = audition->instruments[0];
  if (type == InstrumentType::Monk) chipnomadQueueMonkPreview(chipnomadState, *pSongTrack, &i.chip.monk);
  else if (type == InstrumentType::DX7) chipnomadQueueDX7Preview(chipnomadState, *pSongTrack, &i.chip.dx7);
  else if (type == InstrumentType::SID) chipnomadQueueSIDPreview(chipnomadState, *pSongTrack, &i.chip.sid);
  else if (isOPLL(type)) chipnomadQueueOPLLPreview(chipnomadState, *pSongTrack, &i.chip.opll);
  else if (isOPL(type)) chipnomadQueueOPLPreview(chipnomadState, *pSongTrack, type, &i.chip.opl);
  else if (isFourOp(type)) chipnomadQueueFourOpPreview(chipnomadState, *pSongTrack, type, &i.chip.fourOp);
  else chipnomadQueueSimpleChipPreview(chipnomadState, *pSongTrack, type, &i.chip.simpleChip);
}
void back() {
  stop();
  if (userColumns || library->atRoot()) { screenSetup(&screenInstrument, cInstrument); return; }
  std::string error; library->back(error);
  if (!error.empty()) screenMessage(MESSAGE_TIME_ERROR, "%s", error.c_str());
  show();
}
void select(int value) {
  stop();
  if (value == backValue) { back(); return; }
  if (userColumns) {
    if (value < 0 || size_t(value) >= userReferences.size()) return;
    std::string error;
    PresetLoad load{library, &userReferences[value], 0, cInstrument, &error};
    if (audioManager.mutateProject(loadPreset, &load) == 0) {
      rememberUserPreset(userReferences[value]);
      projectModified = 1;
      screenSetup(&screenInstrument, cInstrument);
    }
    if (!error.empty()) screenMessage(MESSAGE_TIME_ERROR, "%s", error.c_str());
    return;
  }
  if (value < 0 || size_t(value) >= library->items().size()) return;
  std::string error;
  if (library->items()[value].kind != UserPresets::Kind::preset) {
    library->enter(value, error); show();
  } else {
    PresetLoad load{library, nullptr, value, cInstrument, &error};
    if (audioManager.mutateProject(loadPreset, &load) == 0) {
    rememberUserPreset(library->reference(value));
    projectModified = 1; screenSetup(&screenInstrument, cInstrument);
    }
  }
  if (!error.empty()) screenMessage(MESSAGE_TIME_ERROR, "%s", error.c_str());
}
void showUserColumns() {
  std::string error;
  userReferences = library->scan(error);
  using FileKey = std::tuple<bool, std::string, std::string>;
  std::map<FileKey, int> fileCounts;
  for (const auto& ref : userReferences) ++fileCounts[{ref.legacy, ref.archive, ref.path}];
  std::map<std::string, std::vector<int>> groups;
  for (size_t n = 0; n < userReferences.size(); ++n) {
    const auto& ref = userReferences[n];
    const bool bank = fileCounts[{ref.legacy, ref.archive, ref.path}] > 1;
    const auto path = bank ? ref.path : std::filesystem::path(ref.path).parent_path().generic_string();
    std::string key = allSelected() ? (ref.legacy ? "FACTORY/" : "USER/") : ref.legacy ? "Previous banks/" : "";
    // An archive is one collection even when it nests sounds under 6581/8580.
    key += ref.archive.empty() ? path : ref.archive;
    if (!key.empty() && key.back() == '/') key.pop_back();
    if (key.empty()) key = "Loose presets";
    groups[key].push_back(int(n));
  }
  choices.clear();
  labels.clear();
  collectionPaths.clear();
  collectionSounds.clear();
  labels.reserve(groups.size());
  collectionPaths.reserve(groups.size());
  collectionSounds.resize(groups.size());
  choices.push_back({"< Back", backValue, nullptr, 0, nullptr});
  size_t group = 0;
  int selected = userReferences.empty() ? backValue : 0;
  for (const auto& entry : groups) {
    collectionPaths.push_back(entry.first);
    auto label = std::filesystem::path(entry.first).filename().string();
    if (std::filesystem::path(label).extension() == ".zip")
      label = std::filesystem::path(label).stem().string();
    if (label.empty()) label = entry.first;
    labels.push_back(label);
    auto& children = collectionSounds[group];
    children.reserve(entry.second.size());
    for (int n : entry.second) {
      const auto& ref = userReferences[n];
      children.push_back({ref.name.c_str(), n, nullptr, 0, ref.name.c_str()});
      if (cursor().valid && samePreset(ref, cursor().selected)) selected = n;
    }
    choices.push_back({labels.back().c_str(), -100-int(group), children.data(),
                       int(children.size()), collectionPaths.back().c_str()});
    ++group;
  }
  if (groups.empty()) choices.push_back({"(No presets)", emptyValue, nullptr, 0, nullptr});
  char title[32]; snprintf(title, sizeof(title), "%s %s", instrumentTypeName(type), allSelected() ? "ALL" : "USER");
  selectionPopupSetup(title, choices.data(), int(choices.size()), selected,
                      select, back, false, preview, true);
  screenSetup(&screenSelectionPopup, 0);
  if (!error.empty()) screenMessage(MESSAGE_TIME_ERROR, "%s", error.c_str());
}
void show() {
  if (userColumns) { showUserColumns(); return; }
  labels.clear(); choices.clear();
  labels.push_back(library->atRoot() ? "< Back to instrument" : "../");
  for (const auto& i : library->items())
    labels.push_back(i.name + (i.kind == UserPresets::Kind::zip || i.kind == UserPresets::Kind::bank ? " >" : ""));
  if (library->items().empty()) labels.push_back("(No compatible presets)");
  choices.push_back({labels[0].c_str(), backValue, nullptr, 0, nullptr});
  for (size_t i = 0; i < library->items().size(); ++i)
    choices.push_back({labels[i + 1].c_str(), int(i), nullptr, 0, labels[i + 1].c_str()});
  if (library->items().empty()) choices.push_back({labels.back().c_str(), emptyValue, nullptr, 0, nullptr});
  char title[32]; snprintf(title, sizeof(title), "%s %s", instrumentTypeName(type), factorySelected() ? "FACTORY" : "USER");
  int selected=library->items().empty()?backValue:0;
  const auto& position=cursor();
  if(position.valid)for(size_t i=0;i<library->items().size();++i)
    if(samePreset(library->reference(i),position.selected))selected=int(i);
  if (!position.valid && type == InstrumentType::Monk)
    for (size_t i = 0; i < library->items().size(); ++i)
      if (library->items()[i].name == chipnomadState->project.instruments[cInstrument].name) selected = int(i);
  selectionPopupSetup(title, choices.data(), int(choices.size()), selected,
                      select, back, true, preview);
  screenSetup(&screenSelectionPopup, 0);
}
}

bool setupUserPresetLibrary(UserPresets& target, InstrumentType type, bool factory) {
  const auto* subfolder = userPresetFolder(type);
  if (!subfolder) return false;
  std::string base = "instruments/";
  bool external = fileIsRunningFromAppImage();
#ifdef ANDROID_BUILD
  external = true;
#endif
#ifdef _WIN32
  external = true;
#endif
  if (external) {
    char directory[1024];
    if (fileGetDefaultDirectory(directory, sizeof(directory))) return false;
    base = std::string(directory) + "/instruments/";
  }
#ifdef WEB_BUILD
  // MONK saves belong to persistent browser storage; factory sounds are bundled.
  if (type == InstrumentType::Monk) base = factory ? "/instruments/" : "/user/instruments/";
#endif
  target.setup(base + (factory ? "FACTORY/" : "USER/") + subfolder, type, factory ? "" : base + "banks/" + subfolder);
  return true;
}

void openUserPresetBrowser() {
  type = chipnomadState->project.instruments[cInstrument].type;
  userColumns = type == InstrumentType::SID || (type == InstrumentType::Monk && !factorySelected());
  library = &libraries[libraryKey()];
  if(!setupBrowserLibrary(*library))return;
  sequences[libraryKey()].clear();
  std::string error;
  if(cursor().valid)library->focus(cursor().selected,error);
  if (!library->refresh(error)) {
    while (!library->atRoot()) library->back(error);
    library->refresh(error);
  }
  show();
  if (!error.empty()) screenMessage(MESSAGE_TIME_ERROR, "%s", error.c_str());
}

void rememberUserPreset(const UserPresets::Reference& preset) {
  auto& position=cursor();position.valid=true;position.selected=preset;sequences[{chipnomadState->project.instruments[cInstrument].type,selectedBank()}].clear();
}
void cycleUserPreset(int direction) {
  type=chipnomadState->project.instruments[cInstrument].type;
  library=&libraries[libraryKey()];if(!setupBrowserLibrary(*library))return;
  auto& position=cursor();auto& sequence=sequences[libraryKey()];std::string error;
  if(sequence.empty())sequence=library->scan(error);
  if(error.rfind("ALL: ",0)==0)error.erase(0,5);
  if(!error.empty())screenMessage(MESSAGE_TIME_ERROR,"%s",error.c_str());
  const int count=int(sequence.size());
  if(!count){screenMessage(MESSAGE_TIME_ERROR,"No compatible USER presets");return;}
  int index=-1;
  if(position.valid)for(int i=0;i<count;++i)if(samePreset(position.selected,sequence[i])){index=i;break;}
  if(index<0&&!position.valid) {
    const auto& current=chipnomadState->project.instruments[cInstrument];
    for(int i=0;i<count;++i)if(sequence[i].name==current.name&&
      (sequence[i].voice<0||(type==InstrumentType::DX7&&sequence[i].voice==current.chip.dx7.sourceProgram))){index=i;break;}
  }
  if(index<0)index=direction>0?-1:0;
  for(int tries=0;tries<count;++tries) {
    index=(index+(direction>0?1:-1)+count)%count;const auto& preset=sequence[index];
    PresetLoad load{library, &preset, 0, cInstrument, &error};
    if(audioManager.mutateProject(loadPreset, &load) != 0)continue;
    position.selected=preset;position.valid=true;
    projectModified=1;screenSetup(&screenInstrument,cInstrument);return;
  }
  sequence.clear();screenMessage(MESSAGE_TIME_ERROR,"USER presets could not load");
}
