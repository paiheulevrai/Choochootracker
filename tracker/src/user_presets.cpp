#include "user_presets.h"
#include "dx7_patch.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <sstream>
#include <cstring>
#include <functional>

namespace fs = std::filesystem;
namespace {
std::string extension(const std::string& path) {
  auto s = fs::path(path).extension().string();
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}
// Single SID source files often contain truncated or copy-pasted internal names.
// Use the complete filename, retaining model/variant suffixes, in browser and patch.
std::string sidFileName(const std::string& path) {
  auto label=fs::path(path).stem().string();
  bool start=true;
  for(char& c:label) {
    if(c=='_'||c=='-')c=' ';
    if(start)c=std::toupper(static_cast<unsigned char>(c));
    start=c==' ';
  }
  return label;
}
bool hidden(const std::string& path) {
  for (const auto& part : fs::path(path)) {
    auto s = part.string();
    if (s.empty() || s[0] == '.' || s == "__MACOSX") return true;
  }
  return false;
}
bool nativeType(InstrumentType t) { return userPresetFolder(t) != nullptr; }
bool metadata(const std::vector<uint8_t>& data, InstrumentType& type, std::string& name) {
  if (data.empty()) return false;
  std::istringstream in(std::string(reinterpret_cast<const char*>(data.data()), std::min<size_t>(data.size(), 8192)));
  std::string line; bool named = false, typed = false;
  if (!std::getline(in, line) || line.find("# ChipNomad Instrument ") != 0) return false;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.find("- Name: ") == 0) { name = line.substr(8); named = true; }
    if (line.find("- Type: ") == 0) {
      int n; char tail;
      if (sscanf(line.c_str() + 8, "%d %c", &n, &tail) != 1) return false;
      type = InstrumentType(n); typed = true;
    }
    if (named && typed) return nativeType(type);
  }
  return false;
}
}

const char* userPresetFolder(InstrumentType t) {
  switch (t) {
    case InstrumentType::Monk: return "monk";
    case InstrumentType::DX7: return "dx7";
    case InstrumentType::OPLL: return "opll";
    case InstrumentType::VRC7: return "vrc7";
    case InstrumentType::OPL2: return "opl2";
    case InstrumentType::OPL3: return "opl3";
    case InstrumentType::GenesisFM: return "genesis";
    case InstrumentType::ArcadeFM: return "arcade";
    case InstrumentType::SID: return "sid";
    case InstrumentType::SegaPSG: return "sega";
    case InstrumentType::GBPulse: return "gb-pulse";
    case InstrumentType::GBNoise: return "gb-noise";
    default: return nullptr;
  }
}
bool userPresetCompatible(InstrumentType target, InstrumentType source) {
  return nativeType(target) && (target == source || (target == InstrumentType::OPL3 && source == InstrumentType::OPL2));
}
void UserPresets::setup(const std::string& root, InstrumentType type, const std::string& legacyRoot) {
  if (root != root_ || type != type_ || legacyRoot != legacyRoot_ || stack_.empty()) stack_ = {{"", "", false, false}};
  root_ = root; type_ = type; legacyRoot_ = legacyRoot;
}
bool UserPresets::bytes(const std::string& path, std::vector<uint8_t>& data, std::string& error) {
  if(!scanBudget_){error="USER scan size limit reached";return false;}
  bool ok=stack_.back().zip ? archive_.read(path, data, error) :
    readPresetFile((fs::path(locationRoot()) / path).string(), data, error,std::min<size_t>(scanBudget_,1024*1024));
  if(ok&&scanBudget_!=size_t(-1)) {
    if(data.size()>scanBudget_){error="USER scan size limit reached";scanBudget_=0;return false;}
    scanBudget_-=data.size();
  }
  return ok;
}
bool UserPresets::addPreset(const std::string& name, const std::string& path, std::string& error) {
  const auto ext = extension(path);
  if (ext != ".cni" && !externalPresetExtension(type_,path)) return true;
  std::vector<uint8_t> data;
  std::string reason;
  if (!bytes(path, data, reason)) { error=reason; return false; }
  if (ext != ".cni") {
    std::vector<ExternalPreset> parsed;
    if (!importExternalPresets(type_,path,data,parsed,reason)) { error=reason; return false; }
    if(!reason.empty())error=reason;
    if (parsed.size() == 1) items_.push_back({type_==InstrumentType::SID?sidFileName(path):parsed[0].name, path, Kind::preset, parsed[0].sourceIndex});
    else items_.push_back({name, path, Kind::bank});
  } else {
    std::string label; InstrumentType type;
    if (!metadata(data, type, label)) { error = "Invalid CNI metadata"; return false; }
    if (userPresetCompatible(type_, type)) items_.push_back({label.empty() ? name : label, path, Kind::preset});
  }
  return true;
}
bool UserPresets::refresh(std::string& error) {
  items_.clear(); voices_.clear(); error.clear();
  if (stack_.empty()) return false;
  auto location = stack_.back();
  if (location.zip) {
    auto path=fs::path(locationRoot())/location.path;
    if(scanBudget_!=size_t(-1)) {
      std::error_code ec;auto size=fs::file_size(path,ec);
      if(ec||size>scanBudget_){error="USER scan size limit reached";scanBudget_=0;return false;}
      scanBudget_-=size;
    }
    if(!archive_.open(path.string(),error))return false;
  }
  if (location.bank) {
    std::vector<uint8_t> data;
    if (!bytes(location.zip ? location.member : location.path, data, error) ||
        !importExternalPresets(type_, location.zip ? location.member : location.path, data, voices_, error)) return false;
    for (size_t n = 0; n < voices_.size(); ++n)
      items_.push_back({type_==InstrumentType::SID&&voices_.size()==1?sidFileName(location.zip?location.member:location.path):voices_[n].name, location.zip ? location.member : location.path, Kind::preset, voices_[n].sourceIndex});
    return true;
  }
  std::string skipped;
  if (location.zip) {
    std::vector<std::string> directories;
    const auto prefix = location.member.empty() ? "" : location.member + "/";
    for (const auto& e : archive_.entries()) {
      if (hidden(e.name) || e.name.compare(0, prefix.size(), prefix)) continue;
      const auto tail = e.name.substr(prefix.size());
      if (tail.empty()) continue;
      const auto slash = tail.find('/');
      if (slash != std::string::npos) {
        const auto name = tail.substr(0, slash);
        if (std::find(directories.begin(), directories.end(), name) == directories.end()) {
          directories.push_back(name); items_.push_back({name + "/", prefix + name, Kind::folder});
        }
      } else if (!e.directory) addPreset(tail, e.name, skipped);
    }
  } else {
    std::error_code ec;
    if (atRoot() && !legacyRoot_.empty() && legacyRoot_ != root_) {
      if (fs::is_directory(fs::symlink_status(legacyRoot_, ec)))
        items_.push_back({"Previous banks folder/", "", Kind::folder, -1, true});
      ec.clear();
    }
    fs::directory_iterator it(fs::path(locationRoot()) / location.path, fs::directory_options::skip_permission_denied, ec), end;
    if (ec) {
      if (ec == std::errc::no_such_file_or_directory && atRoot()) return true;
      error = "Cannot open user folder"; return false;
    }
    size_t visited = 0;
    for (; it != end; it.increment(ec)) {
      if (ec || ++visited > 8192) { skipped = "User folder limit reached"; break; }
      auto status = it->symlink_status(ec);
      if (ec) { skipped = "Unreadable user file"; ec.clear(); continue; }
      const auto name = it->path().filename().string();
      if (hidden(name) || fs::is_symlink(status)) continue;
      const auto path = (fs::path(location.path) / name).generic_string();
      if (fs::is_directory(status)) items_.push_back({name + "/", path, Kind::folder});
      else if (fs::is_regular_file(status)) {
        if (extension(name) == ".zip") items_.push_back({name, path, Kind::zip});
        else addPreset(name, path, skipped);
      }
    }
  }
  std::stable_sort(items_.begin(), items_.end(), [](const Item& a, const Item& b) {
    if ((a.kind == Kind::preset) != (b.kind == Kind::preset)) return b.kind == Kind::preset;
    return a.name < b.name;
  });
  if (!skipped.empty()) error = skipped;
  return true;
}
bool UserPresets::enter(size_t index, std::string& error) {
  if (index >= items_.size() || items_[index].kind == Kind::preset) return false;
  if (stack_.size() >= 32) { error = "Folder nesting limit reached"; return false; }
  auto next = stack_.back();
  const auto item = items_[index];
  if (item.legacy) { next = {"", "", false, false, true}; }
  else if (item.kind == Kind::zip) { next.path = item.path; next.member.clear(); next.zip = true; }
  else if (next.zip) next.member = item.path;
  else next.path = item.path;
  next.bank = item.kind == Kind::bank;
  stack_.push_back(next);
  if (refresh(error)) return true;
  const auto failure = error; stack_.pop_back(); std::string ignored; refresh(ignored); error = failure;
  return false;
}
bool UserPresets::back(std::string& error) {
  if (atRoot()) return false;
  stack_.pop_back(); return refresh(error);
}
std::string UserPresets::label() const {
  if (stack_.empty()) return "";
  const auto& l = stack_.back();
  if (l.legacy && l.path.empty()) return "Previous banks folder";
  return fs::path(l.member.empty() ? l.path : l.member).filename().string();
}
UserPresets::Reference UserPresets::reference(size_t index) const {
  if(stack_.empty()||index>=items_.size()||items_[index].kind!=Kind::preset)return {};
  const auto& item=items_[index];const auto& location=stack_.back();
  return {item.name,item.path,location.zip?location.path:"",item.voice,location.legacy};
}
bool UserPresets::focus(const Reference& preset,std::string& error) {
  if(!PresetZip::safePath(preset.path)||(!preset.archive.empty()&&!PresetZip::safePath(preset.archive)))return false;
  std::vector<Location> next={{"","",false,false}};
  if(preset.legacy)next.push_back({"","",false,false,true});
  auto folders=[&](const std::string& path){
    std::string prefix;
    for(const auto& part:fs::path(path).parent_path()) {
      prefix+=(prefix.empty()?"":"/")+part.string();auto location=next.back();
      if(location.zip)location.member=prefix;else location.path=prefix;
      next.push_back(location);
    }
  };
  if(!preset.archive.empty()) {
    folders(preset.archive);auto location=next.back();
    location.path=preset.archive;location.member.clear();location.zip=true;next.push_back(location);
  }
  folders(preset.path);
  if(preset.voice>=0) {
    auto location=next.back();location.bank=true;
    if(location.zip)location.member=preset.path;else location.path=preset.path;
    next.push_back(location);
  }
  if(next.size()>32)return false;
  auto saved=stack_;stack_=std::move(next);
  if(refresh(error))for(size_t i=0;i<items_.size();++i) {
    auto r=reference(i);
    if(r.path==preset.path&&r.archive==preset.archive&&r.voice==preset.voice&&r.legacy==preset.legacy)return true;
  }
  stack_=std::move(saved);std::string ignored;refresh(ignored);return false;
}
bool UserPresets::load(size_t index, Project* destination, int slot, std::string& error) {
  if (!destination || slot < 0 || slot >= PROJECT_MAX_INSTRUMENTS || index >= items_.size() || items_[index].kind != Kind::preset) return false;
  return load(reference(index),destination,slot,error);
}
bool UserPresets::load(const Reference& item, Project* destination, int slot, std::string& error) {
  if(!destination||slot<0||slot>=PROJECT_MAX_INSTRUMENTS||!PresetZip::safePath(item.path)||(!item.archive.empty()&&!PresetZip::safePath(item.archive)))return false;
  error.clear();
  auto staged = std::make_unique<Project>(); projectInit(staged.get());
  std::vector<uint8_t> data; bool ok;
  const auto& root=item.legacy?legacyRoot_:root_;
  if(item.archive.empty())ok=readPresetFile((fs::path(root)/item.path).string(),data,error);
  else {PresetZip zip;ok=zip.open((fs::path(root)/item.archive).string(),error)&&zip.read(item.path,data,error);}
  if (ok && item.voice >= 0) {
    std::vector<ExternalPreset> parsed;
    ok = importExternalPresets(type_,item.path,data,parsed,error);
    auto selected=std::find_if(parsed.begin(),parsed.end(),[&](const ExternalPreset& p){return p.sourceIndex==item.voice;});
    ok=ok&&selected!=parsed.end();
    if(ok){
      staged->instruments[0]=selected->instrument;staged->tables[0]=selected->table;
      if(type_==InstrumentType::SID&&parsed.size()==1) {
        auto label=sidFileName(item.path);auto& instrument=staged->instruments[0];
        snprintf(instrument.name,sizeof(instrument.name),"%s",label.c_str());
        snprintf(instrument.chip.sid.presetName,sizeof(instrument.chip.sid.presetName),"%s",label.c_str());
      }
    }
  } else if (ok) ok = instrumentLoadMemory(staged.get(), data.data(), data.size(), 0) == 0;
  ok = ok && userPresetCompatible(type_, staged->instruments[0].type);
  if (ok) {
    instrumentClear(&destination->instruments[slot]);
    destination->instruments[slot] = staged->instruments[0];
    destination->instruments[slot].type = type_;
    if(extension(item.path)!=".syx")destination->tables[slot] = staged->tables[0];
    staged->instruments[0] = {};
  } else if (error.empty()) error = "Invalid or incompatible user preset";
  projectFree(staged.get()); return ok;
}

std::vector<UserPresets::Reference> UserPresets::scan(std::string& error) {
  auto saved=stack_;stack_={{"","",false,false}};
  scanBudget_=64*1024*1024;
  std::vector<Reference> result;
  size_t visited=0,folders=0;bool limited=false;std::string warning;
  std::function<void()> walk=[&] {
    if(++folders>256){limited=true;return;}
    std::string why;
    if(!refresh(why)){warning=why;return;}
    if(!why.empty())warning=why;
    auto entries=items_;const auto location=stack_.back();
    for(const auto& item:entries) {
      if(++visited>8192||!scanBudget_){limited=true;break;}
      if(item.kind==Kind::preset) {
        result.push_back({item.name,item.path,location.zip?location.path:"",item.voice,location.legacy});
      } else if(stack_.size()<32&&folders<256) {
        auto next=location;
        if(item.legacy)next={"","",false,false,true};
        else if(item.kind==Kind::zip){next.path=item.path;next.member.clear();next.zip=true;}
        else if(next.zip)next.member=item.path;
        else next.path=item.path;
        next.bank=item.kind==Kind::bank;
        stack_.push_back(next);walk();stack_.pop_back();
      } else limited=true;
    }
  };
  walk();scanBudget_=size_t(-1);stack_=std::move(saved);
  std::string ignored;refresh(ignored);
  error=limited?"ALL: USER scan limit reached":warning.empty()?"":"ALL: some USER presets skipped";
  return result;
}
