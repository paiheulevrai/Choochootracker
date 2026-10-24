#pragma once
#include "project.h"
#include "preset_zip.h"
#include "external_presets.h"
#include <string>
#include <vector>

const char* userPresetFolder(InstrumentType type);
bool userPresetCompatible(InstrumentType target, InstrumentType source);

// A single engine's folder browser. No catalog required, no ZIP extraction.
class UserPresets {
public:
  enum class Kind { folder, zip, bank, preset };
  struct Item { std::string name, path; Kind kind; int voice = -1; bool legacy = false; };
  struct Reference { std::string name, path, archive; int voice = -1; bool legacy = false; };
  void setup(const std::string& root, InstrumentType type, const std::string& legacyRoot = "");
  bool refresh(std::string& error);
  bool enter(size_t index, std::string& error);
  bool back(std::string& error);
  bool load(size_t index, Project* destination, int slot, std::string& error);
  bool load(const Reference& preset, Project* destination, int slot, std::string& error);
  Reference reference(size_t index) const;
  bool focus(const Reference& preset, std::string& error);
  // Bounded flattened view for ALL; dedicated USER navigation stays hierarchical.
  std::vector<Reference> scan(std::string& error);
  const std::vector<Item>& items() const { return items_; }
  std::string label() const;
  const std::string& rootPath() const { return root_; }
  bool atRoot() const { return stack_.size() <= 1; }
private:
  struct Location { std::string path, member; bool zip = false, bank = false, legacy = false; };
  const std::string& locationRoot() const { return stack_.back().legacy ? legacyRoot_ : root_; }
  bool bytes(const std::string& path, std::vector<uint8_t>& data, std::string& error);
  bool addPreset(const std::string& name, const std::string& path, std::string& error);
  std::string root_, legacyRoot_;
  InstrumentType type_ = InstrumentType::none;
  std::vector<Location> stack_;
  std::vector<Item> items_;
  PresetZip archive_;
  std::vector<ExternalPreset> voices_;
  size_t scanBudget_ = size_t(-1);
};
