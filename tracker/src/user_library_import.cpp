// Browser-only USER collection ZIP import. Bank/sample bytes are never converted.
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>
#include "soundbank_source.h"

extern "C" EMSCRIPTEN_KEEPALIVE const char* webImportUserZip(const char* path) {
  namespace fs = std::filesystem;
  static std::string message;
  size_t imported = 0, kept = 0, skipped = 0;
  try {
    soundbank::Archive archive(path);
    const std::set<std::string> engines = {"arcade", "dx7", "gb-noise", "gb-pulse", "genesis",
      "opl2", "opl3", "opll", "sega", "sid", "vrc7", "soundfont"};
    struct File { std::string member, target; uint64_t bytes; };
    std::vector<File> files;
    uint64_t total = 0;
    for (const auto& entry : archive.entries()) {
      if (entry.name.empty() || entry.name.back() == '/' || soundbank::isPlatformMetadata(entry.name)) continue;
      auto relative = soundbank::normalizedPath(entry.name);
      if (relative.rfind("instruments/", 0) == 0) relative.erase(0, 12);
      if (relative.rfind("USER/", 0) != 0) { ++skipped; continue; }
      auto slash = relative.find('/', 5);
      if (slash == std::string::npos || !engines.count(relative.substr(5, slash - 5))) { ++skipped; continue; }
      const auto target = fs::path("/user/instruments") / relative;
      if (fs::exists(target)) { ++kept; continue; }
      total += entry.size;
      soundbank::require(total <= 1024ull * 1024 * 1024 && entry.size <= 512ull * 1024 * 1024,
                         "USER ZIP exceeds browser import budget; select a smaller collection");
      files.push_back({entry.name, target.string(), entry.size});
    }
    for (const auto& file : files) {
      auto source = archive.open(file.member, file.bytes);
      auto target = fs::path(file.target);
      fs::create_directories(target.parent_path());
      // A duplicate entry or a user edit during cooperative yields must also be kept.
      if (fs::exists(target)) { ++kept; continue; }
      auto temporary = target; temporary += ".choochoo-import-part";
      soundbank::require(!fs::exists(temporary), "Incomplete import file exists; existing data kept");
      try {
        std::ofstream output(temporary, std::ios::binary);
        soundbank::require(bool(output), "Cannot create imported file");
        std::array<char, 65536> buffer;
        for (uint64_t offset = 0; offset < file.bytes;) {
          auto count = size_t(std::min<uint64_t>(buffer.size(), file.bytes - offset));
          source->read(offset, buffer.data(), count);
          output.write(buffer.data(), count);
          soundbank::require(bool(output), "Cannot write imported file");
          offset += count;
        }
        output.close();
        soundbank::require(bool(output), "Cannot finish imported file");
        if (fs::exists(target)) { fs::remove(temporary); ++kept; }
        else { fs::rename(temporary, target); ++imported; }
      } catch (...) { fs::remove(temporary); throw; }
    }
    message = std::to_string(imported) + " USER files imported; " + std::to_string(kept) +
              " existing files kept; " + std::to_string(skipped) + " unrelated files skipped.";
  } catch (const std::exception& error) {
    message = "Import stopped after " + std::to_string(imported) + " files. Existing files kept. " + error.what();
  }
  return message.c_str();
}
#endif
