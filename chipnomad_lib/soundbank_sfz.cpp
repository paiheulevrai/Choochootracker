#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <sstream>

#include "soundbank.h"

namespace soundbank {
namespace {
using Opcodes = std::map<std::string, std::string>;
std::string trim(std::string s) {
  auto a = s.find_first_not_of(" \r\n\t");
  if (a == std::string::npos) return "";
  return s.substr(a, s.find_last_not_of(" \r\n\t") - a + 1);
}
float number(const std::string& s) {
  char* e = nullptr;
  float n = strtof(s.c_str(), &e);
  require(e != s.c_str() && !*e && std::isfinite(n) && fabsf(n) < 1e9f, "Invalid SFZ number: " + s);
  return n;
}
int note(const std::string& s) {
  if (s.empty() || !isalpha(uint8_t(s[0]))) return int(number(s));
  static const int keys[] = {9, 11, 0, 2, 4, 5, 7};
  int letter = tolower(uint8_t(s[0])) - 'a';
  require(letter >= 0 && letter < 7, "Invalid SFZ key");
  int n = keys[letter];
  size_t p = 1;
  if (p < s.size() && (s[p] == '#' || s[p] == 'b')) n += s[p++] == '#' ? 1 : -1;
  int octave = int(number(s.substr(p)));
  require(octave >= -1 && octave <= 9, "SFZ note octave out of MIDI range");
  return n + 12 * (octave + 1);
}
std::string preprocess(const Location& location, std::map<std::string, std::string>& macros,
                       std::set<std::string>& stack, int depth = 0) {
  require(depth < 16 && stack.insert(location.id()).second, "SFZ include cycle/depth exceeded");
  auto source = location.open(4 * 1024 * 1024);
  auto bytes = source->bytes(0, size_t(source->size()), 4 * 1024 * 1024);
  std::string text(bytes.begin(), bytes.end()), result;
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    auto clean = trim(line);
    if (clean.rfind("#define", 0) == 0) {
      std::istringstream in(clean.substr(7));
      std::string key, value;
      in >> key;
      std::getline(in, value);
      require(!key.empty() && key[0] == '$', "Invalid SFZ define");
      macros[key] = trim(value);
      continue;
    }
    for (const auto& m : macros) {
      size_t p = 0;
      while ((p = line.find(m.first, p)) != std::string::npos) {
        line.replace(p, m.first.size(), m.second);
        p += m.second.size();
        require(line.size() < 4 * 1024 * 1024, "SFZ macro expansion limit");
      }
    }
    clean = trim(line);
    if (clean.rfind("#include", 0) == 0) {
      auto a = clean.find('"'), b = clean.find_last_of('"');
      require(a != std::string::npos && b > a, "Invalid SFZ include");
      result +=
          preprocess(location.relative(clean.substr(a + 1, b - a - 1)), macros, stack, depth + 1);
    } else {
      result += line;
      result += '\n';
    }
    require(result.size() <= 4 * 1024 * 1024, "Expanded SFZ exceeds limit");
  }
  stack.erase(location.id());
  return result;
}
}  // namespace
void parseSFZ(Bank& b, const SourcePtr&) {
  std::map<std::string, std::string> macros;
  std::set<std::string> stack;
  auto text =
      preprocess(b.source, macros, stack);  // comments, headers, and opcode values including spaces
  std::string clean;
  bool quoted = false, block = false;
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (block) {
      if (c == '*' && i + 1 < text.size() && text[i + 1] == '/') {
        block = false;
        ++i;
      }
      continue;
    }
    if (c == '"') quoted = !quoted;
    if (!quoted && c == '/' && i + 1 < text.size() && text[i + 1] == '*') {
      block = true;
      ++i;
      continue;
    }
    if (!quoted && c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
      while (i < text.size() && text[i] != '\n') ++i;
      clean += '\n';
      continue;
    }
    clean += c;
  }
  require(!block && !quoted, "Unterminated SFZ comment/string");
  Preset preset;
  preset.id = "sfz";
  preset.name = b.name;
  Opcodes global, master, group, region, control;
  Opcodes* current = nullptr;
  bool inRegion = false;
  std::set<std::string> unsupported;
  auto flush = [&]() {
    if (!inRegion) return;
    require(preset.zones.size() < maxZones, "SFZ region limit exceeded");
    require(region.count("sample"), "SFZ region missing sample");
    if (region.count("lochan") && number(region["lochan"]) > 1) {
      inRegion = false;
      return;
    }
    if (region.count("hichan") && number(region["hichan"]) < 1) {
      inRegion = false;
      return;
    }
    if (region.count("sw_last") &&
        note(region["sw_last"]) != (region.count("sw_default") ? note(region["sw_default"]) : 24)) {
      inRegion = false;
      return;
    }
    for (const auto& opcode : region)
      if (opcode.first.rfind("on_locc", 0) == 0 || opcode.first.rfind("on_hicc", 0) == 0) {
        unsupported.insert("CC-triggered regions (not played)");
        inRegion = false;
        return;
      }
    Zone z;
    std::map<int, float> curve;
    std::string sample = region["sample"];
    if (sample.size() > 1 && sample.front() == '"' && sample.back() == '"')
      sample = sample.substr(1, sample.size() - 2);
    require(!sample.empty() && sample[0] != '*', "SFZ oscillator samples unsupported");
    auto directory = control["default_path"];
    if (directory.size() > 1 && directory.front() == '"' && directory.back() == '"')
      directory = directory.substr(1, directory.size() - 2);
    auto location =
        b.source.relative((directory.empty() ? std::string() : directory + "/") + sample);
    size_t ai = 0;
    for (; ai < b.assets.size(); ++ai)
      if (b.assets[ai].source.id() == location.id()) break;
    if (ai == b.assets.size()) b.assets.push_back({location});
    z.asset = ai;
    for (auto& op : region) {
      const auto& k = op.first;
      const auto& v = op.second;
      if (k == "sample" || k == "label") continue;
      if (k == "key") {
        z.root = z.loKey = z.hiKey = note(v);
        z.rootExplicit = true;
      }
    }
    for (auto& op : region) {
      const auto& k = op.first;
      const auto& v = op.second;
      if (k == "sample" || k == "key" || k == "label" || k == "group_label" || k == "region_label")
        continue;
      if (k == "lokey")
        z.loKey = note(v);
      else if (k == "hikey")
        z.hiKey = note(v);
      else if (k == "lovel")
        z.loVel = int(number(v));
      else if (k == "hivel")
        z.hiVel = int(number(v));
      else if (k == "pitch_keycenter") {
        z.root = note(v);
        z.rootExplicit = true;
      } else if (k == "tune")
        z.tune += number(v);
      else if (k == "transpose")
        z.tune += 100 * number(v);
      else if (k == "volume")
        z.gain *= std::pow(10.f, std::clamp(number(v), -144.f, 24.f) / 20.f);
      else if (k == "pan")
        z.pan = std::clamp(number(v) / 100.f, -1.f, 1.f);
      else if (k == "pitch_keytrack")
        z.keytrack = std::clamp(number(v), -1200.f, 1200.f);
      else if (k == "amp_veltrack")
        z.velocityTrack = std::clamp(number(v), -100.f, 100.f);
      else if (k == "offset" || k == "end" || k == "loop_start" || k == "loop_end") {
        double n = std::stod(v);
        require(std::isfinite(n) && n >= 0 && n < UINT32_MAX, "SFZ sample offset out of range");
        uint32_t value = uint32_t(n);
        if (k == "offset")
          z.start = value;
        else if (k == "end")
          z.end = value + 1;
        else if (k == "loop_start")
          z.loopStart = value;
        else
          z.loopEnd = value + 1;
      } else if (k == "loop_mode") {
        z.loopExplicit = true;
        if (v == "no_loop")
          z.loop = Loop::off;
        else if (v == "one_shot")
          z.loop = Loop::oneShot;
        else if (v == "loop_continuous")
          z.loop = Loop::continuous;
        else if (v == "loop_sustain")
          z.loop = Loop::sustain;
        else
          require(false, "Unsupported SFZ loop mode: " + v);
      } else if (k == "ampeg_attack")
        z.envelope.attack = std::clamp(number(v), 0.f, 100.f);
      else if (k == "ampeg_hold")
        z.envelope.hold = std::clamp(number(v), 0.f, 100.f);
      else if (k == "ampeg_delay")
        z.envelope.delay = std::clamp(number(v), 0.f, 100.f);
      else if (k == "ampeg_decay")
        z.envelope.decay = std::clamp(number(v), 0.f, 100.f);
      else if (k == "ampeg_sustain")
        z.envelope.sustain = std::clamp(number(v) / 100.f, 0.f, 1.f);
      else if (k == "ampeg_release")
        z.envelope.release = std::clamp(number(v), 0.f, 100.f);
      else if (k == "cutoff")
        z.cutoff = std::clamp(number(v), 20.f, 20000.f);
      else if (k == "resonance")
        z.resonance = std::clamp(number(v), 0.f, 40.f);
      else if (k == "fil_type") {
        if (v != "lpf_2p") unsupported.insert(k + "=" + v);
      } else if (k == "xfin_lovel")
        z.fadeInLo = int(number(v));
      else if (k == "xfin_hivel")
        z.fadeInHi = int(number(v));
      else if (k == "xfout_lovel")
        z.fadeOutLo = int(number(v));
      else if (k == "xfout_hivel")
        z.fadeOutHi = int(number(v));
      else if (k == "lorand")
        z.randomLow = std::clamp(number(v), 0.f, 1.f);
      else if (k == "hirand")
        z.randomHigh = std::clamp(number(v), 0.f, 1.f);
      else if (k == "pitch_random")
        z.pitchRandom = std::clamp(number(v), 0.f, 1200.f);
      else if (k == "amp_random")
        z.ampRandom = std::clamp(number(v), 0.f, 24.f);
      else if (k == "off_time")
        z.offTime = std::clamp(number(v), 0.f, 100.f);
      else if (k == "off_mode") {
        require(v == "normal" || v == "fast", "Unsupported SFZ off mode");
        if (v == "fast") z.offTime = 0;
      } else if (k.rfind("amp_velcurve_", 0) == 0)
        curve[std::clamp(int(number(k.substr(13))), 0, 127)] = std::clamp(number(v), 0.f, 1.f);
      else if (k == "lochan" || k == "hichan")
        unsupported.insert("MIDI channels other than 1 (default channel selected)");
      else if (k.rfind("sw_", 0) == 0)
        unsupported.insert("keyswitch changes (default articulation selected)");
      else if (k == "group")
        z.group = int(std::clamp(number(v), 0.f, 65535.f));
      else if (k == "off_by")
        z.offBy = int(std::clamp(number(v), 0.f, 65535.f));
      else if (k == "seq_length")
        z.sequenceLength = int(std::clamp(number(v), 1.f, 128.f));
      else if (k == "seq_position")
        z.sequencePosition = int(std::clamp(number(v), 1.f, 128.f));
      else if (k == "direction") {
        require(v == "forward" || v == "reverse", "Unsupported SFZ direction");
        z.reverse = v == "reverse";
      } else if (k == "trigger") {
        require(v == "attack" || v == "release" || v == "release_key",
                "Unsupported SFZ trigger: " + v);
        z.releaseTrigger = v != "attack";
      } else
        unsupported.insert(k);
    }
    if (!curve.empty()) {
      z.customVelocityCurve = true;
      if (!curve.count(0)) curve[0] = 0;
      if (!curve.count(127)) curve[127] = 1;
      for (auto a = curve.begin(), next = std::next(a); next != curve.end(); ++a, ++next)
        for (int v = a->first; v <= next->first; ++v)
          z.velocityCurve[v] = a->second + (next->second - a->second) * (v - a->first) /
                                               float(next->first - a->first);
    }
    require(z.fadeInLo >= 0 && z.fadeInHi <= 127 && z.fadeInLo <= z.fadeInHi && z.fadeOutLo >= 0 &&
                z.fadeOutHi <= 127 && z.fadeOutLo <= z.fadeOutHi,
            "Invalid SFZ velocity crossfade");
    preset.zones.push_back(z);
    inRegion = false;
  };
  for (size_t p = 0; p < clean.size();) {
    while (p < clean.size() && isspace(uint8_t(clean[p]))) ++p;
    if (p == clean.size()) break;
    if (clean[p] == '<') {
      flush();
      auto end = clean.find('>', p);
      require(end != std::string::npos, "Unclosed SFZ header");
      auto tag = trim(clean.substr(p + 1, end - p - 1));
      p = end + 1;
      if (tag == "control")
        current = &control;
      else if (tag == "global") {
        global.clear();
        master.clear();
        group.clear();
        current = &global;
      } else if (tag == "master") {
        master = global;
        group.clear();
        current = &master;
      } else if (tag == "group") {
        group = master.empty() ? global : master;
        current = &group;
      } else if (tag == "region") {
        region = group.empty() ? (master.empty() ? global : master) : group;
        current = &region;
        inRegion = true;
      } else {
        unsupported.insert("<" + tag + ">");
        current = nullptr;
      }
      continue;
    }
    size_t start = p;
    while (p < clean.size() && (isalnum(uint8_t(clean[p])) || clean[p] == '_' || clean[p] == '$'))
      ++p;
    std::string key = clean.substr(start, p - start);
    while (p < clean.size() && isspace(uint8_t(clean[p]))) ++p;
    require(!key.empty() && p < clean.size() && clean[p] == '=',
            "Malformed SFZ opcode near " + key);
    ++p;
    start = p;
    bool quote = false;
    while (p < clean.size()) {
      if (clean[p] == '"') quote = !quote;
      if (!quote && clean[p] == '<') break;
      if (!quote && isspace(uint8_t(clean[p]))) {
        size_t q = p;
        while (q < clean.size() && isspace(uint8_t(clean[q]))) ++q;
        size_t word = q;
        while (q < clean.size() && (isalnum(uint8_t(clean[q])) || clean[q] == '_')) ++q;
        while (q < clean.size() && isspace(uint8_t(clean[q]))) ++q;
        if (q < clean.size() && clean[q] == '=' && q > word) break;
      }
      ++p;
    }
    if (current) {
      auto value = trim(clean.substr(start, p - start));
      if (key == "key") {
        (*current)["lokey"] = value;
        (*current)["hikey"] = value;
        (*current)["pitch_keycenter"] = value;
      } else
        (*current)[key] = value;
    }
  }
  flush();
  for (auto& u : unsupported) preset.warnings.push_back("Unsupported SFZ: " + u);
  b.presets.push_back(std::move(preset));
}
}  // namespace soundbank
