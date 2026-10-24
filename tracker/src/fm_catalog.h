#pragma once
#include <string>
#include <vector>
#include "project_instruments.h"
#include "project.h"
struct FMPresetEntry {
  int type,bank;
  std::string bankName,category,name,path;
  int imported=-1;
  bool library=false;
  std::string archive;
};
// UI/offline only. Transactional, bounded metadata read; no synthesis allocation.
bool loadFMCatalog(const char* path,std::vector<FMPresetEntry>& entries);
struct DX7Library {
  std::vector<FMPresetEntry> entries;
  std::vector<InstrumentDX7> patches;
  int skipped=0;
  bool limited=false;
};
// Scan the persistent bank folder on the UI thread. Missing folders are empty.
// Bad files are isolated; selected instruments own their patch independently.
DX7Library scanDX7Library(const std::string& folder);

// Loads either a loose CNI or an entry in the catalog's ZIP collection.
bool loadFMPreset(const std::string& folder, const FMPresetEntry& entry, Project* project, int slot);

struct FMFactoryCollection {
  int id;
  std::string name;
  std::vector<int> presets; // Indices in the original catalog; no patch copies.
};
// Factory ownership differs from USER file compatibility: OPL3 can import an
// OPL2 USER patch without presenting OPL2's factory collection as its own.
std::vector<FMFactoryCollection> factoryCollections(const std::vector<FMPresetEntry>& catalog, InstrumentType type);
