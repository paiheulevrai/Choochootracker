#pragma once
#include <cstdint>
#include <string>
#include <vector>

// UI/offline only. No extraction to disk, ZIP64, encryption or nested archives.
class PresetZip {
public:
  struct Entry {
    std::string name;
    uint32_t offset, compressed, size, crc;
    uint16_t method, flags;
    bool directory;
  };
  bool open(const std::string& path, std::string& error);
  bool read(const std::string& name, std::vector<uint8_t>& output, std::string& error) const;
  const std::vector<Entry>& entries() const { return entries_; }
  static bool safePath(const std::string& path);
private:
  std::vector<uint8_t> bytes_;
  std::vector<Entry> entries_;
};

bool readPresetFile(const std::string& path, std::vector<uint8_t>& bytes, std::string& error,
                    size_t limit = 1024 * 1024);
