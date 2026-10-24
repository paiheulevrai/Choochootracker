#include "soundbank_index.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace soundbank {
namespace fs = std::filesystem;
static std::string dependencyStamp(const std::vector<std::string>& dependencies) {
  std::string result;
  for (const auto& dependency : dependencies) {
    auto path = dependency.substr(0, dependency.find("::"));
    std::error_code ec;
    auto size = fs::file_size(path, ec);
    if (ec) {
      result += "missing;";
      continue;
    }
    auto stamp = fs::last_write_time(path, ec).time_since_epoch().count();
    result += std::to_string(size) + ":" + std::to_string((long long)stamp) + ";";
  }
  return result;
}

Location BankIndex::resolve(const std::string& id) const {
  auto split = id.find("::");
  std::string path = normalizedPath(id.substr(0, split));
  Location l{(fs::path(root_) / path).string(),
             split == std::string::npos ? "" : normalizedPath(id.substr(split + 2))};
  auto canonicalRoot = fs::weakly_canonical(root_);
  auto canonicalFile = fs::weakly_canonical(l.path);
  auto relative = canonicalFile.lexically_relative(canonicalRoot).generic_string();
  require(!relative.empty() && relative != ".." && relative.rfind("../", 0) != 0,
          "Bank path escapes SoundBanks root");
  l.root = canonicalRoot.string();
  return l;
}
std::vector<BankEntry> BankIndex::scan(bool force) const {
  std::map<std::string, BankEntry> cached;
  fs::path cache = fs::path(root_) / ".choochoo-index-v3";
  if (!force && fs::exists(cache) && fs::file_size(cache) <= 64 * 1024 * 1024) {
    std::ifstream in(cache);
    std::string tag;
    while (in >> tag) {
      if (tag != "B") break;
      BankEntry b;
      size_t n = 0;
      in >> std::quoted(b.id) >> std::quoted(b.signature) >> std::quoted(b.dependencySignature) >>
          std::quoted(b.name) >> std::quoted(b.format) >> n;
      if (n > 16384) break;
      for (size_t i = 0; i < n && in; ++i) {
        std::string id, name;
        in >> std::quoted(id) >> std::quoted(name);
        b.presets.push_back({id, name});
      }
      size_t deps = 0;
      in >> deps;
      if (deps > maxZones) break;
      for (size_t i = 0; i < deps && in; ++i) {
        std::string dep;
        in >> std::quoted(dep);
        b.dependencies.push_back(dep);
      }
      size_t warnings = 0;
      in >> warnings;
      if (warnings > 128) break;
      for (size_t i = 0; i < warnings && in; ++i) {
        std::string warning;
        in >> std::quoted(warning);
        b.warnings.push_back(warning);
      }
      if (!in) break;
      cached[b.id] = std::move(b);
    }
  }
  std::vector<BankEntry> result;
  std::error_code ec;
  fs::create_directories(root_, ec);
  require(!ec, "Cannot create SoundBanks directory");
  auto supported = [](const std::string& e) {
    return e == ".sf2" || e == ".sf3" || e == ".sfz" || e == ".exs" || e == ".wav" || e == ".aif" ||
           e == ".aiff";
  };
  auto add = [&](const std::string& id, const std::string& signature) {
    if (result.size() >= 200000) throw std::runtime_error("SoundBanks index limit exceeded");
    auto old = cached.find(id);
    if (old != cached.end() && old->second.signature == signature &&
        old->second.dependencySignature == dependencyStamp(old->second.dependencies)) {
      result.push_back(old->second);
      return;
    }
    BankEntry entry;
    entry.id = id;
    entry.name = fs::path(id).stem().string();
    entry.format = extension(id).substr(1);
    entry.signature = signature;
    try {
      auto bank = parseBank(resolve(id));
      entry.name = bank.name;
      entry.format = bank.format;
      entry.warnings = bank.warnings;
      for (auto& p : bank.presets) {
        entry.presets.push_back({p.id, p.name});
        for (auto& w : p.warnings)
          if (entry.warnings.size() < 128) entry.warnings.push_back(w);
      }
      for (auto& a : bank.assets) {
        auto location = a.source;
        try {
          location = location.relocated();
        } catch (const std::exception& e) {
          entry.cacheable = false;
          if (entry.warnings.size() < 128) entry.warnings.push_back(e.what());
        }
        if (std::find(entry.dependencies.begin(), entry.dependencies.end(), location.id()) ==
            entry.dependencies.end())
          entry.dependencies.push_back(location.id());
      }
      entry.dependencySignature = dependencyStamp(entry.dependencies);
    } catch (const std::exception& e) {
      entry.error = e.what();
    }
    result.push_back(std::move(entry));
  };
  fs::recursive_directory_iterator iter(root_, fs::directory_options::skip_permission_denied, ec),
      end;
  for (; !ec && iter != end; iter.increment(ec)) {
    offlineYield();
    if (isPlatformMetadata(iter->path().lexically_relative(root_).generic_string())) {
      iter.disable_recursion_pending();
      continue;
    }
    if (iter.depth() > 32) {
      iter.disable_recursion_pending();
      continue;
    }
    if (iter->is_symlink(ec)) {
      iter.disable_recursion_pending();
      continue;
    }
    if (!iter->is_regular_file(ec)) continue;
    auto path = iter->path();
    auto ext = extension(path.string());
    if (ext != ".zip" && !supported(ext)) continue;
    auto id = path.lexically_relative(root_).generic_string();
    auto signature =
        std::to_string(iter->file_size(ec)) + ":" +
        std::to_string((long long)iter->last_write_time(ec).time_since_epoch().count());
    if (ec) break;
    if (ext != ".zip")
      add(id, signature);
    else {
      try {
        Archive archive(path.string());
        for (auto& member : archive.entries())
          if (!isPlatformMetadata(member.name) && supported(extension(member.name)))
            add(id + "::" + member.name, signature + ":" + std::to_string(member.crc));
      } catch (const std::exception& e) {
        BankEntry b;
        b.id = id;
        b.name = path.filename().string();
        b.format = "zip";
        b.error = e.what();
        result.push_back(b);
      }
    }
  }
  require(!ec, "SoundBanks scan failed: " + ec.message());
  std::sort(result.begin(), result.end(),
            [](const BankEntry& a, const BankEntry& b) { return a.id < b.id; });
  // Advisory cache: failure to write never prevents browsing. Preserve format
  // warnings; retry failed entries and missing references on the next scan.
  auto temporary = cache;
  temporary += ".tmp";
  std::ofstream out(temporary);
  for (auto& b : result)
    if (b.error.empty() && b.cacheable && b.warnings.size() <= 128) {
      out << "B " << std::quoted(b.id) << ' ' << std::quoted(b.signature) << ' '
          << std::quoted(b.dependencySignature) << ' ' << std::quoted(b.name) << ' '
          << std::quoted(b.format) << ' ' << b.presets.size();
      for (auto& p : b.presets) out << ' ' << std::quoted(p.first) << ' ' << std::quoted(p.second);
      out << ' ' << b.dependencies.size();
      for (auto& d : b.dependencies) out << ' ' << std::quoted(d);
      out << ' ' << b.warnings.size();
      for (auto& w : b.warnings) out << ' ' << std::quoted(w);
      out << '\n';
    }
  out.close();
  if (out) fs::rename(temporary, cache, ec);
  return result;
}
}  // namespace soundbank
