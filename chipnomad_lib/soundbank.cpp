#include "soundbank.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <stdexcept>

namespace soundbank {
std::string Asset::identity() const {
  std::error_code ec;
  auto stamp = std::filesystem::last_write_time(source.path, ec).time_since_epoch().count();
  auto size = std::filesystem::file_size(source.path, ec);
  return std::to_string((long long)stamp) + ":" + std::to_string(size) + ":" + source.id() + "@" +
         std::to_string(offset) + ":" + std::to_string(length) + ":" + std::to_string(sampleRate) +
         ":" + std::to_string(vorbis);
}
Bank parseBank(const Location& location) {
  Bank b;
  b.source = location;
  b.name = std::filesystem::path(location.member.empty() ? location.path : location.member)
               .stem()
               .string();
  auto ext = extension(location.member.empty() ? location.path : location.member);
  SourcePtr source;
  if (ext != ".wav" && ext != ".aif" && ext != ".aiff") source = location.open();
  b.format = ext.substr(1);
  if (ext == ".sf2" || ext == ".sf3")
    parseSoundFont(b, source);
  else if (ext == ".sfz")
    parseSFZ(b, source);
  else if (ext == ".exs")
    parseEXS(b, source);
  else if (ext == ".wav" || ext == ".aif" || ext == ".aiff") {
    b.assets.push_back({location});
    Preset p;
    p.id = "sample";
    p.name = b.name;
    p.zones.push_back({});
    b.presets.push_back(p);
  } else
    require(false, "Unsupported sound bank format");
  require(!b.presets.empty(), "No instrument presets found");
  for (auto& p : b.presets) {
    std::sort(p.warnings.begin(), p.warnings.end());
    p.warnings.erase(std::unique(p.warnings.begin(), p.warnings.end()), p.warnings.end());
    require(p.zones.size() <= maxZones, "Preset zone limit exceeded");
    for (auto& z : p.zones)
      require(z.asset < b.assets.size() && z.loKey >= 0 && z.hiKey <= 127 && z.loKey <= z.hiKey &&
                  z.loVel >= 0 && z.hiVel <= 127 && z.loVel <= z.hiVel && z.root >= 0 &&
                  z.root <= 127 && z.sequenceLength >= 1 && z.sequenceLength <= 128 &&
                  z.sequencePosition >= 1 && z.sequencePosition <= z.sequenceLength,
              "Invalid zone mapping");
  }
  return b;
}
size_t SampleCache::bytes() const {
  size_t n = 0;
  for (auto& a : audio_) n += a.second->pcm.size() * sizeof(int16_t);
  return n;
}
void SampleCache::trim() {
  for (auto i = audio_.begin(); i != audio_.end();)
    if (i->second.use_count() == 1)
      i = audio_.erase(i);
    else
      ++i;
}
std::shared_ptr<const PreparedPreset> SampleCache::prepare(const Bank& b, const std::string& id) {
  auto p =
      std::find_if(b.presets.begin(), b.presets.end(), [&](const Preset& p) { return p.id == id; });
  require(p != b.presets.end(), "Saved preset is missing (no substitution)");
  auto result = std::make_shared<PreparedPreset>();
  result->id = p->id;
  result->name = p->name;
  result->warnings = b.warnings;
  result->warnings.insert(result->warnings.end(), p->warnings.begin(), p->warnings.end());
  LoadStageScope progress(LoadStage::loading);
  std::set<const Audio*> counted;
  for (auto z : p->zones) {
    auto asset = b.assets.at(z.asset);
    if (!asset.reader) asset.source = asset.source.relocated();
    auto key = asset.identity();
    auto it = audio_.find(key);
    if (it == audio_.end()) {
      auto used = bytes();
      if (used >= limit_) {
        trim();
        used = bytes();
      }
      require(used < limit_, "Sample cache full; unload unused presets");
      std::shared_ptr<Audio> a;
      try {
        a = decodeAudio(asset, limit_ - used);
      } catch (const std::exception& e) {
        throw std::runtime_error(std::string(e.what()) + ": " + asset.source.id());
      }
      it = audio_.emplace(key, a).first;
    }
    auto a = it->second;
    if (!z.rootExplicit) {
      z.root = a->root;
      z.tune += a->tune;
    }
    if (!z.loopExplicit && a->loopEnd > a->loopStart) {
      z.loop = Loop::continuous;
      z.loopStart = a->loopStart;
      z.loopEnd = a->loopEnd;
    }
    if (!z.end) z.end = a->frames();
    if ((z.loop == Loop::continuous || z.loop == Loop::sustain) && !z.loopEnd) {
      if (!z.loopStart) z.loopStart = a->loopStart;
      z.loopEnd = a->loopEnd ? a->loopEnd : z.end;
    }
    require(z.start < z.end && z.end <= a->frames(),
            "Sample range outside audio: " + asset.source.id());
    if (z.loop == Loop::continuous || z.loop == Loop::sustain)
      require(z.loopEnd > z.loopStart && z.loopEnd <= z.end,
              "Invalid sample loop [" + std::to_string(z.loopStart) + "," +
                  std::to_string(z.loopEnd) + ") in [" + std::to_string(z.start) + "," +
                  std::to_string(z.end) + "): " + asset.source.id());
    if (counted.insert(a.get()).second) {
      result->bytes += a->pcm.size() * 2;
      for (const auto& warning : a->warnings)
        if (std::find(result->warnings.begin(), result->warnings.end(), warning) ==
            result->warnings.end())
          result->warnings.push_back(warning);
    }
    for (int key = z.loKey; key <= z.hiKey; ++key)
      result->keys[key].push_back(uint16_t(result->zones.size()));
    result->zones.push_back({z, a});
  }
  for (const auto& key : result->keys)
    for (int velocity = 0; velocity < 128; ++velocity) {
      offlineYield();
      std::map<int, std::array<int, 128>> sequences;
      for (auto n : key) {
        const auto& z = result->zones[n].zone;
        if (velocity >= z.loVel && velocity <= z.hiVel)
          ++sequences[z.sequenceLength][z.sequencePosition - 1];
      }
      int layers = 0;
      for (auto& sequence : sequences)
        layers += *std::max_element(sequence.second.begin(), sequence.second.end());
      require(layers <= 8, "Preset exceeds eight simultaneous layers per note");
    }
  require(!result->zones.empty(), "Preset has no playable zones");
  return result;
}
}  // namespace soundbank
