#include "doctest.h"
#include "fm_catalog.h"
#include <cstdio>
#include <fstream>
static std::string factoryPath(const char* file) {
  std::string path = std::string("packaging/common/instruments/FACTORY/") + file;
  return std::ifstream(path).good() ? path : "../" + path;
}
TEST_CASE("FM catalog supports ten thousand metadata entries transactionally") {
  FILE* f=fopen("test_fm_catalog.tsv","wb");REQUIRE(f);fputs("CCT-CHIP-CATALOG\t1\n",f);
  for(int i=0;i<10000;++i)fprintf(f,"24\t101\tScale fixture\tUnsorted\tSynthetic %05d\tsynthetic-%05d.cni\n",i,i);fclose(f);
  std::vector<FMPresetEntry> entries;REQUIRE(loadFMCatalog("test_fm_catalog.tsv",entries));CHECK(entries.size()==10000);CHECK(entries.back().name=="Synthetic 09999");
  f=fopen("test_fm_catalog.tsv","wb");REQUIRE(f);fputs("CCT-CHIP-CATALOG\t1\r\n25\t200\tGenesis\tKeys\tTest\ttest.cni\r\n",f);fclose(f);
  REQUIRE(loadFMCatalog("test_fm_catalog.tsv",entries));CHECK(entries.size()==1);CHECK(entries[0].type==int(InstrumentType::GenesisFM));
  f=fopen("test_fm_catalog.tsv","wb");REQUIRE(f);fputs("CCT-CHIP-CATALOG\t1\n24\t101\tBad\tUnsorted\tBad\t../escape.cni\n",f);fclose(f);
  CHECK_FALSE(loadFMCatalog("test_fm_catalog.tsv",entries));CHECK(entries.size()==1);std::remove("test_fm_catalog.tsv");
}
TEST_CASE("FM bundled catalogue loads all currently packaged records") {
  std::vector<FMPresetEntry> entries;
  REQUIRE(loadFMCatalog(factoryPath("catalog.tsv").c_str(),entries));CHECK(entries.size()==1120);
}

TEST_CASE("Factory collections keep engine ownership and downloaded names") {
  std::vector<FMPresetEntry> entries, builtins;
  REQUIRE(loadFMCatalog(factoryPath("catalog.tsv").c_str(),entries));
  REQUIRE(loadFMCatalog(factoryPath("builtins.tsv").c_str(),builtins));
  entries.insert(entries.end(),builtins.begin(),builtins.end());
  for(auto type:{InstrumentType::OPLL,InstrumentType::VRC7}) {
    auto groups=factoryCollections(entries,type);
    REQUIRE(groups.size()==1);CHECK(groups[0].name=="Factory Presets");CHECK(groups[0].presets.size()==73);
    for(int i:groups[0].presets)CHECK(entries[i].type==int(type));
  }
  auto dx=factoryCollections(entries,InstrumentType::DX7);
  REQUIRE(dx.size()==2);CHECK(dx[0].name=="Factory Presets");CHECK(dx[0].presets.size()==36);
  CHECK(dx[1].name=="OpenDX7 Originals");CHECK(dx[1].presets.size()==31);
  for(auto type:{InstrumentType::OPL2,InstrumentType::OPL3}) {
    auto groups=factoryCollections(entries,type);REQUIRE(groups.size()==2);
    bool fat=false,dmx=false;
    for(const auto& g:groups) {
      CHECK(g.id!=(type==InstrumentType::OPL2?2:1));
      if(g.id==(type==InstrumentType::OPL2?1:2)){fat=true;CHECK(g.presets.size()==181);}
      if(g.id==3){dmx=true;CHECK(g.presets.size()==(type==InstrumentType::OPL2?59:335));}
    }
    CHECK(fat);CHECK(dmx);
  }
  for(auto type:{InstrumentType::SegaPSG,InstrumentType::GBPulse,InstrumentType::GBNoise}) {
    auto groups=factoryCollections(entries,type);REQUIRE(groups.size()==1);CHECK(groups[0].name=="Factory Presets");
    for(int i:groups[0].presets)CHECK(entries[i].type==int(type));
  }
  for(auto type:{InstrumentType::GenesisFM,InstrumentType::ArcadeFM}) {
    auto groups=factoryCollections(entries,type);REQUIRE(groups.size()==2);
    CHECK(groups[0].name=="Factory Presets");CHECK(groups[0].presets.size()==24);
    CHECK(groups[1].id==(type==InstrumentType::GenesisFM?210:211));
    CHECK(groups[1].presets.size()==(type==InstrumentType::GenesisFM?49:57));
  }
}
