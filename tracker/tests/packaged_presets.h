#pragma once
#include "fm_catalog.h"
#include "doctest.h"
inline std::vector<FMPresetEntry> packagedPresets() {
  std::vector<FMPresetEntry> result;
  for (const auto* name : {"catalog.tsv", "builtins.tsv"}) {
    std::vector<FMPresetEntry> entries;
    REQUIRE(loadFMCatalog((std::string("packaging/common/instruments/FACTORY/") + name).c_str(), entries));
    result.insert(result.end(), entries.begin(), entries.end());
  }
  return result;
}
