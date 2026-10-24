#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <set>

#include "soundbank.h"

namespace soundbank {
void parseSoundFont(Bank& b, const SourcePtr& source) {
  auto header = source->bytes(0, 12);
  require(!memcmp(header.data(), "RIFF", 4) && !memcmp(header.data() + 8, "sfbk", 4),
          "Invalid SoundFont RIFF");
  uint64_t end = uint64_t(read32(header.data() + 4)) + 8;
  require(end <= source->size() && end >= 12, "Truncated SoundFont");
  std::map<std::string, std::vector<uint8_t>> tables;
  uint64_t smpl = 0, smplSize = 0;
  int major = 2;
  std::function<void(uint64_t, uint64_t, int)> chunks = [&](uint64_t pos, uint64_t limit,
                                                            int depth) {
    require(depth < 4, "Invalid SoundFont nesting");
    while (pos + 8 <= limit) {
      auto h = source->bytes(pos, 8);
      uint32_t size = read32(h.data() + 4);
      uint64_t start = pos + 8;
      require(size <= limit - start, "Truncated SoundFont chunk");
      std::string id(reinterpret_cast<const char*>(h.data()), 4);
      if (id == "LIST") {
        require(size >= 4, "Invalid SoundFont LIST");
        chunks(start + 4, start + size, depth + 1);
      } else if (id == "smpl") {
        smpl = start;
        smplSize = size;
      } else if (id == "phdr" || id == "pbag" || id == "pgen" || id == "inst" || id == "ibag" ||
                 id == "igen" || id == "shdr" || id == "pmod" || id == "imod") {
        require(!tables.count(id), "Duplicate SoundFont table");
        tables[id] = source->bytes(start, size);
      } else if (id == "ifil") {
        require(size >= 4, "Invalid SoundFont version");
        auto v = source->bytes(start, 4);
        major = read16(v.data());
        require(major == 2 || major == 3, "Unsupported SoundFont version");
      } else if (id == "sm24")
        b.warnings.push_back("SF2 24-bit samples rendered at 16-bit precision");
      pos = start + size;  // SF3 writers may omit padding around compressed sample data.
      if ((size & 1) && pos < limit) {
        uint8_t pad = 0;
        source->read(pos, &pad, 1);
        if (!pad) ++pos;
      }
    }
    require(pos == limit, "Malformed SoundFont chunk boundary");
  };
  chunks(12, end, 0);
  require(smpl && smplSize, "Missing SoundFont sample data");
  auto table = [&](const char* name, size_t width, size_t minimum) -> const std::vector<uint8_t>& {
    auto i = tables.find(name);
    require(
        i != tables.end() && i->second.size() % width == 0 && i->second.size() / width >= minimum,
        std::string("Invalid SoundFont table: ") + name);
    return i->second;
  };
  auto& ph = table("phdr", 38, 2);
  auto& pb = table("pbag", 4, 1);
  auto& pg = table("pgen", 4, 1);
  auto& ins = table("inst", 22, 2);
  auto& ib = table("ibag", 4, 1);
  auto& ig = table("igen", 4, 1);
  auto& sh = table("shdr", 46, 2);
  for (const auto* name : {"pmod", "imod"})
    if (tables.count(name) && tables[name].size() > 10)
      b.warnings.push_back(std::string("SoundFont custom modulators unsupported: ") + name);
  auto name = [](const uint8_t* p) {
    size_t n = 0;
    while (n < 20 && p[n]) ++n;
    return std::string(reinterpret_cast<const char*>(p), n);
  };
  std::vector<uint32_t> frameCount;
  for (size_t i = 0; i + 46 < sh.size(); i += 46) {
    auto p = sh.data() + i;
    uint32_t start = read32(p + 20), stop = read32(p + 24), ls = read32(p + 28),
             le = read32(p + 32);
    uint16_t type = read16(p + 44);
    require(!(type & 0x8000), "ROM SoundFont samples unsupported");
    bool compressed = type & 16;
    require(stop > start, "Invalid SoundFont sample extent");
    Asset a;
    a.source = b.source;
    a.reader = source;
    a.sampleRate = read32(p + 36);
    a.root = p[40] == 255 ? 60 : p[40];
    a.tune = int8_t(p[41]);
    a.vorbis = compressed;
    uint64_t relative = uint64_t(start) * (compressed ? 1 : 2);
    a.length = compressed ? uint64_t(stop) - start + (stop < smplSize ? 1 : 0)
                          : (uint64_t(stop) - start) * 2;
    require(relative <= smplSize && a.length <= smplSize - relative,
            "SoundFont sample outside smpl");
    a.offset = smpl + relative;
    if (compressed) {
      a.loopStart = ls;
      a.loopEnd = le;
      require(major == 3, "Compressed samples require SF3 version");
    } else {
      if (ls >= start && le >= ls && le <= stop) {
        a.loopStart = ls - start;
        a.loopEnd = le - start;
      }
    }
    b.assets.push_back(a);
    frameCount.push_back(compressed ? 0 : stop - start);
  }
  using Gens = std::map<int, int>;
  auto bags = [&](const std::vector<uint8_t>& bag, const std::vector<uint8_t>& gen, size_t start,
                  size_t stop) {
    require(start <= stop && stop < bag.size() / 4, "SoundFont bag index out of range");
    std::vector<Gens> result;
    for (size_t i = start; i < stop; ++i) {
      auto a = read16(bag.data() + i * 4), z = read16(bag.data() + (i + 1) * 4);
      require(a <= z && z <= gen.size() / 4, "SoundFont generator index out of range");
      Gens values;
      for (size_t j = a; j < z; ++j) {
        int op = read16(gen.data() + j * 4);
        require(op <= 60, "Invalid SoundFont generator");
        values[op] = int16_t(read16(gen.data() + j * 4 + 2));
      }
      result.push_back(std::move(values));
    }
    return result;
  };
  auto inherit = [](Gens base, const Gens& local) {
    for (auto& g : local) base[g.first] = g.second;
    return base;
  };
  const std::set<int> supported = {0,  1,  2,  3,  4,  8,  9,  12, 17, 33, 34, 35, 36, 37,
                                   38, 41, 43, 44, 45, 48, 50, 51, 52, 53, 54, 56, 57, 58};
  std::set<std::string> identities;
  size_t totalZones = 0;
  for (size_t p = 0; p + 38 < ph.size(); p += 38) {
    Preset preset;
    preset.name = name(ph.data() + p);
    preset.id = std::to_string(read16(ph.data() + p + 22)) + ":" +
                std::to_string(read16(ph.data() + p + 20));
    require(identities.insert(preset.id).second, "Duplicate SoundFont bank/program identity");
    Gens presetGlobal;
    std::set<int> unsupported;
    for (auto pgLocal : bags(pb, pg, read16(ph.data() + p + 24), read16(ph.data() + p + 38 + 24))) {
      if (!pgLocal.count(41)) {
        presetGlobal = pgLocal;
        continue;
      }
      auto presetG = inherit(presetGlobal, pgLocal);
      int inst = presetG[41];
      require(inst >= 0 && size_t(inst + 1) < ins.size() / 22,
              "SoundFont instrument index out of range");
      Gens instrumentGlobal;
      for (auto igLocal : bags(ib, ig, read16(ins.data() + inst * 22 + 20),
                               read16(ins.data() + (inst + 1) * 22 + 20))) {
        if (!igLocal.count(53)) {
          instrumentGlobal = igLocal;
          continue;
        }
        auto g = inherit(instrumentGlobal, igLocal);
        int sample = g[53];
        require(sample >= 0 && size_t(sample) < b.assets.size(),
                "SoundFont sample index out of range");
        const auto& asset = b.assets[sample];
        Zone z;
        z.asset = sample;
        z.root = asset.root;
        z.rootExplicit = true;
        z.loopExplicit = true;
        auto value = [&](int op, int def = 0) {
          int v = g.count(op) ? g[op] : def;
          if (presetG.count(op) && op != 41 && op != 43 && op != 44 && op != 53 && op != 54 &&
              op != 57 && op != 58)
            v += presetG[op];
          return v;
        };
        for (auto& group : {g, presetG})
          for (auto& op : group)
            if (!supported.count(op.first) && op.second) unsupported.insert(op.first);
        auto range = [&](int op, int& low, int& high) {
          for (auto& map : {g, presetG})
            if (map.count(op)) {
              unsigned v = uint16_t(map.at(op));
              low = std::max(low, int(v & 255));
              high = std::min(high, int(v >> 8));
            }
        };
        range(43, z.loKey, z.hiKey);
        range(44, z.loVel, z.hiVel);
        if (z.loKey > z.hiKey || z.loVel > z.hiVel) continue;
        if (g.count(58) && g[58] >= 0) z.root = g[58];
        z.tune = asset.tune + value(51) * 100 + value(52);
        z.keytrack = std::clamp(value(56, 100), 0, 1200);
        z.pan = std::clamp(value(17) / 500.f, -1.f, 1.f);
        z.gain = powf(10, -std::clamp(value(48), 0, 1440) / 200.f);
        auto offset = [&](int fine, int coarse, int64_t base) {
          int64_t n = base + value(fine) + int64_t(value(coarse)) * 32768;
          require(n >= 0 && n <= UINT32_MAX, "SoundFont adjusted sample offset out of range");
          return uint32_t(n);
        };
        z.start = offset(0, 4, 0);
        int64_t endDelta = value(1) + int64_t(value(12)) * 32768;
        if (asset.vorbis) {
          require(endDelta == 0, "SF3 end-address adjustment unsupported");
        } else
          z.end = offset(1, 12, frameCount[sample]);
        int mode = value(54) & 3;
        z.loop = mode == 1 ? Loop::continuous : mode == 3 ? Loop::sustain : Loop::off;
        if (mode == 1 || mode == 3) {
          z.loopStart = offset(2, 45, asset.loopStart);
          z.loopEnd = offset(3, 50, asset.loopEnd);
        }
        auto seconds = [&](int op) {
          return powf(2, std::clamp(value(op, -12000), -12000, 8000) / 1200.f);
        };
        z.envelope.delay = seconds(33);
        z.envelope.attack = seconds(34);
        z.envelope.hold = seconds(35);
        z.envelope.decay = seconds(36);
        z.envelope.release = seconds(38);
        z.envelope.sustain = powf(10, -std::clamp(value(37), 0, 1440) / 200.f);
        z.cutoff = std::clamp(8.176f * powf(2, std::clamp(value(8, 13500), 1500, 13500) / 1200.f),
                              20.f, 20000.f);
        z.resonance = std::clamp(value(9) / 10.f, 0.f, 40.f);
        z.group = z.offBy = std::max(0, value(57));
        // The pan generator controls placement, including linked samples.
        // Header left/right flags describe sample links, not an implicit pan.
        require(preset.zones.size() < maxZones, "SoundFont preset zone limit exceeded");
        require(++totalZones <= 65536, "SoundFont bank metadata zone limit exceeded");
        preset.zones.push_back(z);
      }
    }
    for (auto op : unsupported)
      preset.warnings.push_back("Unsupported SoundFont generator " + std::to_string(op));
    b.presets.push_back(std::move(preset));
  }
}
}  // namespace soundbank
