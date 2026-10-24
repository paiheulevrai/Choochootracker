// Legacy EXS offsets based on Matt Allan's MIT-licensed renoise-exs24.
// See docs/soundbanks/EXS-parser-LICENSE.md. No proprietary SDK is used.
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <filesystem>

#include "soundbank.h"
namespace soundbank {
void parseEXS(Bank& b, const SourcePtr& source) {
  auto bytes = source->bytes(0, size_t(source->size()), 16 * 1024 * 1024);
  require(bytes.size() >= 84, "Truncated EXS header");
  auto magic = std::string(reinterpret_cast<const char*>(bytes.data() + 16), 4);
  bool big = magic == "SOBT" || magic == "SOBJ";
  require(big || magic == "TBOS" || magic == "JBOS", "Unsupported EXS variant");
  bool expanded = read32(bytes.data() + 4, big) > 0x8000;
  Preset preset;
  preset.id = "exs";
  preset.name = b.name;
  std::map<uint32_t, Location> samples;
  std::vector<std::pair<Zone, uint32_t>> zones;
  auto str = [](const uint8_t* p, size_t n) {
    size_t len = 0;
    while (len < n && p[len]) ++len;
    std::string s(reinterpret_cast<const char*>(p), len);
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
  };
  for (size_t pos = 0; pos < bytes.size();) {
    require(bytes.size() - pos >= 84, "Truncated EXS chunk");
    auto p = bytes.data() + pos;
    uint32_t size = read32(p + 4, big);
    if (expanded && size > 0x8000) size -= 0x8000;
    require(size <= bytes.size() - pos - 84, "Truncated EXS payload");
    size_t total = size_t(size) + 84;
    auto kind = p[3] & 15;
    if (kind == 0) {
      auto name = str(p + 20, 64);
      auto normalized = name;
      normalized.erase(0, normalized.find_first_not_of(" \t"));
      for (auto& c : normalized) c = char(std::tolower(static_cast<unsigned char>(c)));
      if (!normalized.empty() && normalized != "(null)" && normalized != "null") preset.name = name;
    } else if (kind == 1) {
      require(total >= 188 && zones.size() < maxZones, "Unsupported EXS zone size/count");
      Zone z;
      z.root = p[85];
      z.rootExplicit = true;
      z.tune = int8_t(p[86]) + 100 * int8_t(p[164]);
      z.pan = std::clamp(float(int8_t(p[87])) / 100.f, -1.f, 1.f);
      z.gain = powf(10, float(int8_t(p[88])) / 20);
      z.loKey = p[90];
      z.hiKey = p[91];
      if (p[84] & 8) {
        z.loVel = p[93];
        z.hiVel = p[94];
      }
      z.keytrack = (p[84] & 2) ? 0 : 100;
      z.reverse = p[84] & 4;
      z.start = read32(p + 96, big);
      z.end = read32(p + 100, big);
      z.loopStart = read32(p + 104, big);
      z.loopEnd = read32(p + 108, big);
      z.loopExplicit = true;
      if (total >= 196 && read32(p + 192, big))
        preset.warnings.push_back("EXS additional sample offset unsupported");
      if (total >= 192 && read32(p + 188, big))
        preset.warnings.push_back("EXS sample fade unsupported");
      z.loop = (p[117] & 1) ? Loop::continuous : (p[84] & 1) ? Loop::oneShot : Loop::off;
      auto group = read32(p + 172, big);
      z.group = group == UINT32_MAX ? 0 : int(group + 1);
      if (read32(p + 112, big)) preset.warnings.push_back("EXS loop crossfade unsupported");
      if (p[84] & 64) preset.warnings.push_back("EXS output routing unsupported");
      zones.push_back({z, read32(p + 176, big)});
    } else if (kind == 2)
      preset.warnings.push_back("EXS group playback/envelope parameters unsupported");
    else if (kind == 3) {
      require(size == 336 || size == 592 || size == 600, "Unsupported EXS sample record");
      auto directory = str(p + 164, 256), name = total > 420 ? str(p + 420, 256) : str(p + 20, 64);
      require(!name.empty(), "Empty EXS sample reference");
      Location loc = b.source.relative(name);
      // Keep the original directory suffix to distinguish identically named
      // samples in different source folders. Never open an author's absolute
      // path: relocation searches only this archive or the permitted root.
      std::replace(directory.begin(), directory.end(), '\\', '/');
      if (directory.size() >= 2 && directory[1] == ':') directory.erase(0, 2);
      std::replace(directory.begin(), directory.end(), ':', '/');  // Legacy HFS paths.
      while (!directory.empty() && directory.front() == '/') directory.erase(0, 1);
      if (!directory.empty())
        loc = b.source.relative(directory + "/" + name);
      // Zone references are ordinal indexes, not the optional chunk IDs.
      // Real AutoSampler exports often write zero to every sample chunk ID.
      samples[uint32_t(samples.size())] = loc;
    } else if (kind == 4)
      preset.warnings.push_back("EXS instrument synthesis parameters unsupported");
    else
      preset.warnings.push_back("Unsupported EXS chunk type " + std::to_string(kind));
    pos += total;
  }
  for (auto& item : zones) {
    auto sample = samples.find(item.second);
    require(sample != samples.end(), "EXS zone references missing sample record");
    size_t ai = 0;
    for (; ai < b.assets.size(); ++ai)
      if (b.assets[ai].source.id() == sample->second.id()) break;
    if (ai == b.assets.size()) b.assets.push_back({sample->second});
    item.first.asset = ai;
    preset.zones.push_back(item.first);
  }
  b.presets.push_back(std::move(preset));
}
}  // namespace soundbank
