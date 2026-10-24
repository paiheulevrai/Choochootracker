#pragma once
#include <algorithm>
#include <map>
#include <cctype>
#include <tuple>
#include <set>
#include "soundbank_index.h"
namespace soundbank {
inline std::string browserLabel(std::string name) {
  auto dot=name.rfind('.');
  if (dot!=std::string::npos) {
    auto ext=name.substr(dot);
    for(auto& c:ext)c=char(std::tolower(static_cast<unsigned char>(c)));
    if (ext==".zip" || ext==".sf2" || ext==".sf3" || ext==".sfz" ||
        ext==".exs" || ext==".wav" || ext==".aif" || ext==".aiff") name.resize(dot);
  }
  auto dash=name.rfind('-');
  if (dash!=std::string::npos && name.size()-dash-1==40 &&
      name.find_first_not_of("0123456789abcdefABCDEF",dash+1)==std::string::npos)
    name.resize(dash);
  // Freesound transport names: numeric ID__uploader__descriptive-name.
  auto first=name.find("__"), second=first==std::string::npos?first:name.find("__",first+2);
  if(first>0 && first!=std::string::npos && second!=std::string::npos &&
     name.find_first_not_of("0123456789",0)==first) name.erase(0,second+2);
  std::replace(name.begin(),name.end(),'_',' ');
  return name;
}
inline bool rawSample(const BankEntry& bank) {
  return bank.format == "wav" || bank.format == "aif" || bank.format == "aiff";
}
inline std::vector<std::string> bankFolders(std::string id) {
  auto archive=id.find("::");
  if (archive!=std::string::npos) id.replace(archive,2,"/");
  std::vector<std::string> parts;
  size_t start=0, slash;
  while ((slash=id.find('/',start))!=std::string::npos) {
    parts.push_back(id.substr(start,slash-start)); start=slash+1;
  }
  parts.push_back(id.substr(start)); return parts;
}
inline std::set<std::string> mappedCollections(const std::vector<BankEntry>& banks) {
  std::set<std::string> result;
  for (const auto& bank:banks) if (!rawSample(bank)) result.insert(bankFolders(bank.id).front());
  return result;
}
struct SupportingSamples {
  std::set<std::string> archives, directories;
  explicit SupportingSamples(const std::vector<BankEntry>& banks) {
    for(const auto& bank:banks) {
      if(rawSample(bank) || !bank.error.empty())continue;
      const auto split=bank.id.find("::");
      if(split!=std::string::npos)archives.insert(bank.id.substr(0,split));
      else if(bank.format=="sfz" || bank.format=="exs") {
        const auto slash=bank.id.rfind('/');
        directories.insert(slash==std::string::npos?std::string():bank.id.substr(0,slash));
      }
    }
  }
  bool hides(const BankEntry& bank) const {
    if(!rawSample(bank))return false;
    const auto split=bank.id.find("::");
    if(split!=std::string::npos)return archives.count(bank.id.substr(0,split));
    auto path=bank.id;
    for(;;) {
      const auto slash=path.rfind('/');
      path=slash==std::string::npos?std::string():path.substr(0,slash);
      if(directories.count(path))return true;
      if(path.empty())return false;
    }
  }
};
struct PresetReference { int bank, preset; std::string label; };
struct Collection {
  std::string name;
  std::vector<PresetReference> presets;
  std::string path;
  bool hasFolders = false;
};
inline std::string sortLabel(std::string value) {
  for(auto& c:value)c=char(std::tolower(static_cast<unsigned char>(c)));
  return value;
}
inline std::vector<Collection> collections(const std::vector<BankEntry>& banks) {
  std::map<std::string,std::set<std::string>> formats;
  const SupportingSamples supporting(banks);
  for(const auto& b:banks) if(!rawSample(b)) formats[bankFolders(b.id).front()].insert(b.format);
  std::map<std::string,Collection> groups;
  for(size_t n=0;n<banks.size();++n) {
    const auto& b=banks[n]; if(!b.error.empty())continue;
    auto parts=bankFolders(b.id); auto top=parts.front();
    if(supporting.hides(b)) continue;
    auto name=parts.size()>1 || !rawSample(b)?browserLabel(top):std::string("Loose samples");
    if(formats[top].size()>1 && !rawSample(b))name+=" ("+b.format+")";
    auto& group=groups[sortLabel(name)];group.name=name;
    for(size_t p=0;p<b.presets.size();++p)
      group.presets.push_back({int(n),int(p),browserLabel(b.presets[p].second)});
  }
  std::vector<Collection> result;
  for(auto& entry:groups) {
    auto& g=entry.second;
    std::sort(g.presets.begin(),g.presets.end(),[&](const auto& a,const auto& b){
      return std::make_tuple(sortLabel(a.label),banks[a.bank].id,banks[a.bank].presets[a.preset].first)<
             std::make_tuple(sortLabel(b.label),banks[b.bank].id,banks[b.bank].presets[b.preset].first);
    });
    if(!g.presets.empty())result.push_back(std::move(g));
  }
  return result;
}
// Navigate physical folders while preserving the publisher's ZIP as a single
// collection. Internal ZIP sample trees are dependencies, not user categories.
inline std::vector<Collection> browseCollections(const std::vector<BankEntry>& banks,
                                                const std::string& parent) {
  const SupportingSamples supporting(banks);
  std::map<std::string, Collection> folders;
  const auto prefix = parent.empty() ? std::string() : parent + "/";
  for (size_t n = 0; n < banks.size(); ++n) {
    const auto& bank = banks[n];
    if (!bank.error.empty() || supporting.hides(bank)) continue;
    const auto disk = bank.id.substr(0, bank.id.find("::"));
    const auto slash = disk.rfind('/');
    const auto directory = slash == std::string::npos ? std::string() : disk.substr(0, slash);
    if (directory != parent && directory.rfind(prefix, 0) != 0) continue;
    const auto relative = directory == parent ? std::string() : directory.substr(prefix.size());
    const auto separator = relative.find('/');
    const auto child = relative.substr(0, separator);
    const auto path = child.empty() ? parent : prefix + child;
    auto& group = folders[path];
    group.name = child.empty() ? "Presets here" : browserLabel(child);
    group.path = path;
    if (separator != std::string::npos) { group.hasFolders = true; continue; }
    for (size_t p = 0; p < bank.presets.size(); ++p)
      group.presets.push_back({int(n), int(p), browserLabel(bank.presets[p].second)});
  }
  std::vector<Collection> result;
  for (auto& pair : folders) {
    auto& group = pair.second;
    std::map<std::string,std::set<std::string>> duplicateFormats;
    for (const auto& preset : group.presets)
      duplicateFormats[sortLabel(preset.label)].insert(banks[preset.bank].format);
    for (auto& preset : group.presets)
      if (duplicateFormats[sortLabel(preset.label)].size()>1)
        preset.label+=" ("+banks[preset.bank].format+")";
    std::sort(group.presets.begin(), group.presets.end(), [](const auto& a, const auto& b) {
      return std::make_tuple(sortLabel(a.label), a.bank, a.preset) <
             std::make_tuple(sortLabel(b.label), b.bank, b.preset);
    });
    result.push_back(std::move(group));
  }
  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
    return std::make_tuple(sortLabel(a.name), a.path) < std::make_tuple(sortLabel(b.name), b.path);
  });
  return result;
}
// Walk physical collection folders in the same order as the browser. ZIP
// members stay together within their containing folder, not alphabetized
// across unrelated collections under the USER root.
inline std::vector<std::pair<int,int>> presetSequence(const std::vector<BankEntry>& banks) {
  using PathKey = std::vector<std::pair<std::string,std::string>>;
  std::map<PathKey,Collection> groups;
  const SupportingSamples supporting(banks);
  for(size_t b=0;b<banks.size();++b) {
    const auto& bank=banks[b];
    if(!bank.error.empty() || supporting.hides(bank))continue;
    auto disk=bank.id.substr(0,bank.id.find("::"));
    auto parts=bankFolders(disk);parts.pop_back();
    PathKey key;
    for(const auto& part:parts)key.emplace_back(sortLabel(browserLabel(part)),part);
    key.emplace_back(sortLabel("Presets here"),"");
    for(size_t p=0;p<bank.presets.size();++p)
      groups[key].presets.push_back({int(b),int(p),browserLabel(bank.presets[p].second)});
  }
  std::vector<std::pair<int,int>> result;
  for(auto& entry:groups) {
    auto& presets=entry.second.presets;
    std::map<std::string,std::set<std::string>> formats;
    for(const auto& p:presets)formats[sortLabel(p.label)].insert(banks[p.bank].format);
    for(auto& p:presets)if(formats[sortLabel(p.label)].size()>1)p.label+=" ("+banks[p.bank].format+")";
    std::sort(presets.begin(),presets.end(),[](const auto& a,const auto& b){
      return std::make_tuple(sortLabel(a.label),a.bank,a.preset)<std::make_tuple(sortLabel(b.label),b.bank,b.preset);
    });
    for(const auto& p:presets)result.emplace_back(p.bank,p.preset);
  }
  return result;
}
inline std::pair<int,int> cyclePreset(const std::vector<BankEntry>& banks,
    const std::vector<std::pair<int,int>>& sequence,
    const std::string& bank, const std::string& preset, int direction) {
  int current=-1;
  for(size_t n=0;n<sequence.size();++n) {
    const auto& ref=sequence[n];
    if(banks[ref.first].id==bank && banks[ref.first].presets[ref.second].first==preset)current=int(n);
  }
  if(sequence.empty())return {-1,-1};
  const int n=int(sequence.size());
  return sequence[current<0?(direction>0?0:n-1):(current+(direction>0?1:n-1))%n];
}
inline std::pair<int,int> cyclePreset(const std::vector<BankEntry>& banks,
    const std::string& bank, const std::string& preset, int direction) {
  return cyclePreset(banks,presetSequence(banks),bank,preset,direction);
}
} // namespace soundbank
