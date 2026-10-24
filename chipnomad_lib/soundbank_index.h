#pragma once
#include "soundbank.h"
namespace soundbank {
struct BankEntry {
  std::string id, name, format, error, signature, dependencySignature;
  std::vector<std::pair<std::string, std::string>> presets;  // stable ID, display name
  std::vector<std::string> dependencies, warnings;
  bool cacheable = true;  // Missing references are retried on the next scan.
};
class BankIndex {
 public:
  explicit BankIndex(std::string root) : root_(std::move(root)) {}
  std::vector<BankEntry> scan(bool force = false) const;
  Location resolve(const std::string& id) const;
  const std::string& root() const { return root_; }

 private:
  std::string root_;
};
}  // namespace soundbank
