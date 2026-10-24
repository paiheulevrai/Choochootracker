#include "fm_catalog.h"
#include "preset_zip.h"
#include "simple_chip_presets.h"
#include "project_instruments.h"
#include "opl_patch.h"
#include "four_op_patch.h"
#include "opll_presets.h"
#include "dx7_patch.h"
#include <filesystem>
#include <algorithm>
#include <set>
#include <cctype>
#include <cstdio>
#include <cstring>
bool loadFMCatalog(const char* path,std::vector<FMPresetEntry>& entries) {
  FILE* f=fopen(path,"rb");if(!f)return false;
  auto fail=[&]{fclose(f);return false;};
  char line[1024];if(!fgets(line,sizeof(line),f))return fail();
  size_t length=strlen(line);while(length&&(line[length-1]=='\n'||line[length-1]=='\r'))line[--length]=0;
  if(strcmp(line,"CCT-CHIP-CATALOG\t1"))return fail();
  std::vector<FMPresetEntry> parsed;
  while(fgets(line,sizeof(line),f)) {
    char* newline=strchr(line,'\n');
    if(!newline||parsed.size()>=65536)return fail();
    *newline=0;if(newline>line&&newline[-1]=='\r')newline[-1]=0;
    std::vector<std::string> fields;char* start=line;
    for(char* c=line;;++c)if(*c=='\t'||!*c){bool end=!*c;*c=0;fields.emplace_back(start);start=c+1;if(end)break;}
    if((fields.size()!=6&&fields.size()!=7)||fields[2].empty()||fields[2].size()>63||fields[3].empty()||fields[3].size()>63||fields[4].empty()||fields[4].size()>63||fields[5].size()>200||fields[5].size()<5||fields[5].substr(fields[5].size()-4)!=".cni"||fields[5].find_first_of("/\\:")!=std::string::npos||fields[5].find("..")!=std::string::npos)return fail();
    int type=0,bank=0;char extra;
    if(sscanf(fields[0].c_str(),"%d%c",&type,&extra)!=1||(!isOPLL((InstrumentType)type)&&!isOPL((InstrumentType)type)&&!isFourOp((InstrumentType)type)&&type!=int(InstrumentType::DX7)&&type!=int(InstrumentType::SID)&&!isSimpleChip((InstrumentType)type))||sscanf(fields[1].c_str(),"%d%c",&bank,&extra)!=1||bank<1||bank>65535)return fail();
    if(fields.size()==7 && (!PresetZip::safePath(fields[6]) || fields[6].size()<5 || fields[6].substr(fields[6].size()-4)!=".zip"))return fail();
    parsed.push_back({type,bank,fields[2],fields[3],fields[4],fields[5]});
    if(fields.size()==7)parsed.back().archive=fields[6];
  }
  bool ok=!ferror(f);fclose(f);if(ok)entries.swap(parsed);return ok;
}

DX7Library scanDX7Library(const std::string& folder) {
  namespace fs=std::filesystem;
  DX7Library result;
  std::vector<fs::path> files;
  std::error_code error;
  auto root=fs::path(folder).lexically_normal();
  fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,error),end;
  if(error) { if(error!=std::errc::no_such_file_or_directory)++result.skipped; return result; }
  size_t visited=0;
  for(;it!=end;it.increment(error)) {
    if(error){++result.skipped;error.clear();break;}
    if(++visited>16384){result.limited=true;break;}
    auto status=it->symlink_status(error);
    if(error){++result.skipped;error.clear();continue;}
    if(fs::is_directory(status)&&it.depth()>=8){it.disable_recursion_pending();result.limited=true;}
    if(!fs::is_regular_file(status))continue; // Do not follow symbolic links.
    auto extension=it->path().extension().string();
    std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return std::tolower(c);});
    if(extension==".syx")files.push_back(it->path());
  }
  std::sort(files.begin(),files.end());
  if(files.size()>2048){files.resize(2048);result.limited=true;}
  size_t totalBytes=0;
  std::set<int> banks;
  for(const auto& path:files) {
    auto size=fs::file_size(path,error);
    if(error||size>1024*1024){++result.skipped;error.clear();continue;}
    if(totalBytes+size>64*1024*1024){result.limited=true;break;}
    totalBytes+=size;
    FILE* f=fopen(path.string().c_str(),"rb");
    if(!f){++result.skipped;continue;}
    std::vector<uint8_t> bytes(size+1);
    size_t read=fread(bytes.data(),1,bytes.size(),f);bool failed=ferror(f);fclose(f);
    std::vector<InstrumentDX7> patches;std::string reason;
    if(failed||read!=size||!importDX7SysEx(bytes.data(),read,patches,reason)){++result.skipped;continue;}
    if(result.patches.size()+patches.size()>60000){result.limited=true;break;}
    std::string relative=path.lexically_relative(root).generic_string();
    std::string label=relative.substr(0,relative.size()-4);
    for(char& c:label)if(static_cast<unsigned char>(c)<32)c=' ';
    std::vector<size_t> starts{0};
    for(size_t n=1;n<patches.size();++n)if(patches[n].sourceProgram==0)starts.push_back(n);
    starts.push_back(patches.size());
    for(size_t chunk=0;chunk+1<starts.size();++chunk) {
      if(banks.size()==25536){result.limited=true;return result;}
      // Stable identity across launches, independent of filesystem enumeration order.
      uint32_t hash=2166136261u;
      for(unsigned char c:relative+"#"+std::to_string(chunk)){hash^=c;hash*=16777619u;}
      int bank=40000+hash%25536;
      while(banks.count(bank))bank=40000+(bank-40000+1)%25536;
      banks.insert(bank);
      auto name=label+(starts.size()>2?" ["+std::to_string(chunk+1)+"]":"");
      for(size_t n=starts[chunk];n<starts[chunk+1];++n){
        auto p=patches[n];p.bankId=bank;
        result.entries.push_back({int(InstrumentType::DX7),bank,name,"Unsorted",p.presetName,relative,int(result.patches.size()),true});
        result.patches.push_back(p);
      }
    }
  }
  return result;
}

bool loadFMPreset(const std::string& folder,const FMPresetEntry& entry,Project* project,int slot) {
  const auto root=std::filesystem::path(folder);
  if(entry.archive.empty())return instrumentLoad(project,(root/entry.path).string().c_str(),slot)==0;
  PresetZip zip;std::vector<uint8_t> bytes;std::string error;
  return zip.open((root/entry.archive).string(),error)&&zip.read(entry.path,bytes,error)&&
    instrumentLoadMemory(project,bytes.data(),bytes.size(),slot)==0;
}

std::vector<FMFactoryCollection> factoryCollections(const std::vector<FMPresetEntry>& catalog, InstrumentType type) {
  std::vector<FMFactoryCollection> result;
  for(size_t n=0;n<catalog.size();++n) {
    const auto& e=catalog[n];
    if(e.imported>=0||e.library)continue;
    bool owned=e.type==int(type);
    // Fat Man 4-op and DMX include some two-operator voices in their OPL3 sets.
    if(type==InstrumentType::OPL3&&e.type==int(InstrumentType::OPL2))owned=e.bank==2||e.bank==3;
    if(type==InstrumentType::OPL2&&e.bank==2)owned=false;
    if(!owned)continue;
    int id=e.bank;
    if(isOPLL(type)||isSimpleChip(type)||
       (type==InstrumentType::DX7&&(e.bank==102||e.bank==103))||
       (type==InstrumentType::GenesisFM&&e.bank==200)||
       (type==InstrumentType::ArcadeFM&&e.bank==201)||
       (type==InstrumentType::SID&&e.bank==600))id=0;
    auto it=std::find_if(result.begin(),result.end(),[&](const auto& group){return group.id==id;});
    if(it==result.end()) {
      result.push_back({id,id==0?"Factory Presets":e.bankName,{}});
      it=result.end()-1;
    }
    it->presets.push_back(int(n));
  }
  std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.id==0&&b.id!=0;});
  if(result.size()==1)result[0].name="Factory Presets";
  return result;
}
