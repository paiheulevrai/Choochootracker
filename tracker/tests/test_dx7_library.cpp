#include "doctest.h"
#include "fm_catalog.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstring>

TEST_CASE("DX7 drop-in library rescans files and subfolders without session state") {
  namespace fs=std::filesystem;
  auto root=fs::temp_directory_path()/"choochoo-dx7-library-test";
  REQUIRE(fs::create_directory(root));
  struct Cleanup {fs::path path;~Cleanup(){std::error_code e;fs::remove_all(path,e);}} cleanup{root};
  std::ifstream source("../tools/chip_banks/sources/dx7/yse-originals.syx",std::ios::binary);
  REQUIRE(source.good());
  std::vector<char> bytes((std::istreambuf_iterator<char>(source)),{});
  auto write=[&](const fs::path& path,int copies){std::ofstream f(path,std::ios::binary);for(int n=0;n<copies;++n)f.write(bytes.data(),bytes.size());};
  CHECK(scanDX7Library((root/"missing").string()).entries.empty());
  fs::create_directory(root/"Pads");write(root/"Pads/Warm.SYX",1);
  write(root/"Four banks.syx",4);
  std::ofstream(root/"broken.syx")<<"not a bank";
  std::ofstream(root/"ignored.txt")<<"not a bank";
  std::error_code ec;fs::create_symlink(root/"Pads/Warm.SYX",root/"alias.syx",ec);
  auto first=scanDX7Library(root.string());
  REQUIRE(first.entries.size()==160);CHECK(first.patches.size()==160);CHECK(first.skipped==1);CHECK_FALSE(first.limited);
  CHECK(first.entries[0].bankName=="Four banks [1]");
  CHECK(first.entries[96].bankName=="Four banks [4]");
  CHECK(first.entries[128].bankName=="Pads/Warm");
  CHECK(first.entries[0].bank!=first.entries[32].bank);
  auto second=scanDX7Library(root.string());
  REQUIRE(second.entries.size()==first.entries.size());
  for(size_t n=0;n<first.entries.size();++n){
    CHECK(first.entries[n].bank==second.entries[n].bank);
    CHECK(first.entries[n].imported==int(n));CHECK(first.entries[n].library);
    CHECK(first.patches[n].bankId==first.entries[n].bank);
  }
  // The selected instrument owns its voice after the source bank is removed.
  InstrumentDX7 selected=first.patches[128];
  fs::remove(root/"Pads/Warm.SYX");write(root/"Added.syx",1);
  auto refreshed=scanDX7Library(root.string());
  CHECK(refreshed.entries.size()==160);
  CHECK(refreshed.entries.front().bankName=="Added");
  CHECK(!memcmp(selected.voice,first.patches[128].voice,155));
  CHECK(refreshed.entries[32].bank==first.entries[0].bank);
}

TEST_CASE("DX7 library isolates checksum errors and oversized files") {
  namespace fs=std::filesystem;
  auto root=fs::temp_directory_path()/"choochoo-dx7-library-bad-test";
  REQUIRE(fs::create_directory(root));
  struct Cleanup {fs::path path;~Cleanup(){std::error_code e;fs::remove_all(path,e);}} cleanup{root};
  std::ifstream source("../tools/chip_banks/sources/dx7/yse-originals.syx",std::ios::binary);
  std::vector<char> bytes((std::istreambuf_iterator<char>(source)),{});REQUIRE(bytes.size()>10);
  {std::ofstream f(root/"good.syx",std::ios::binary);f.write(bytes.data(),bytes.size());}
  bytes[7]^=1;
  {std::ofstream f(root/"checksum.syx",std::ios::binary);f.write(bytes.data(),bytes.size());}
  {std::ofstream f(root/"large.syx",std::ios::binary);f.seekp(1024*1024);f.put(0);}
  auto library=scanDX7Library(root.string());CHECK(library.entries.size()==32);CHECK(library.skipped==2);
}
