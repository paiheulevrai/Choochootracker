#include "external_presets.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

#include "chip_program.h"
#include "dx7_patch.h"
#include "four_op_patch.h"
#include "opl_patch.h"
#include "opll_presets.h"
#include "sid_patch.h"
#include "simple_chip_presets.h"

namespace {
using Bytes = std::vector<uint8_t>;
void need(bool ok, const std::string& why) {
  if (!ok) throw std::runtime_error(why);
}
struct Reader {
  const Bytes& b;
  size_t pos = 0, end;
  explicit Reader(const Bytes& data) : b(data), end(data.size()) {}
  Reader(const Bytes& data, size_t start, size_t stop) : b(data), pos(start), end(stop) {
    need(start <= stop && stop <= data.size(), "Truncated preset");
  }
  size_t left() const { return end - pos; }
  unsigned u8() {
    need(pos < end, "Truncated preset");
    return b[pos++];
  }
  unsigned le16() {
    unsigned n = u8();
    return n | (u8() << 8);
  }
  unsigned be16() {
    unsigned n = u8();
    return (n << 8) | u8();
  }
  uint32_t le32() {
    uint32_t n = le16();
    return n | (uint32_t(le16()) << 16);
  }
  void skip(size_t n) {
    need(n <= left(), "Truncated preset");
    pos += n;
  }
  std::string fixed(size_t n) {
    need(n <= left(), "Truncated preset");
    std::string s(reinterpret_cast<const char*>(b.data() + pos), n);
    pos += n;
    auto z = s.find('\0');
    if (z != std::string::npos) s.resize(z);
    return s;
  }
  std::string str() {
    size_t start = pos;
    while (u8()) {
    }
    return std::string(reinterpret_cast<const char*>(b.data() + start), pos - start - 1);
  }
};
std::string ext(const std::string& p) {
  auto s = std::filesystem::path(p).extension().string();
  for (auto& c : s) c = char(std::tolower((unsigned char)c));
  return s;
}
std::string trim(std::string s) {
  auto a = s.find_first_not_of(" \r\n\t"), b = s.find_last_not_of(" \r\n\t");
  return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
std::string cleanName(std::string s) {
  for (char& c : s)
    if ((unsigned char)c < 32 || c == 127) c = ' ';
  s = trim(s);
  if (s.size() > 63) s.resize(63);
  return s.empty() ? "Imported preset" : s;
}
void name(ExternalPreset& p, const std::string& s) {
  p.name = cleanName(s);
  snprintf(p.instrument.name, sizeof(p.instrument.name), "%s",
           trim(p.name.substr(0, sizeof(p.instrument.name) - 1)).c_str());
  auto& i = p.instrument;
  if (isOPL(i.type))
    snprintf(i.chip.opl.presetName, 64, "%s", p.name.c_str());
  else if (isFourOp(i.type))
    snprintf(i.chip.fourOp.presetName, 64, "%s", p.name.c_str());
  else if (isOPLL(i.type))
    snprintf(i.chip.opll.presetName, 64, "%s", p.name.c_str());
  else if (i.type == InstrumentType::SID)
    snprintf(i.chip.sid.presetName, 64, "%s", p.name.c_str());
}
ExternalPreset init(InstrumentType t, const std::string& s) {
  ExternalPreset p;
  getInstrumentFunctions(t).init(&p.instrument);
  p.instrument.type = t;
  tableClear(&p.table);
  name(p, s);
  return p;
}
void validate(const ExternalPreset& p) {
  const auto& i = p.instrument;
  need(!isFourOp(i.type) || validFourOp(i.type, i.chip.fourOp), "Invalid four-operator parameters");
  need(!isOPL(i.type) || validOPL(i.type, i.chip.opl), "Invalid OPL parameters");
  need(!isSimpleChip(i.type) || validSimpleChip(i.type, i.chip.simpleChip),
       "Invalid PSG parameters");
  need(i.type != InstrumentType::SID || validSID(i.chip.sid), "Invalid SID parameters");
}
ExternalPreset tfi(const Bytes& b, const std::string& title) {
  need(b.size() == 42, "TFI must contain exactly 42 bytes");
  auto p = init(InstrumentType::GenesisFM, title);
  auto& f = p.instrument.chip.fourOp;
  f.algorithm = b[0];
  f.feedback = b[1];
  for (int j = 0; j < 4; ++j) {
    int offset = 2 + 10 * std::array<int, 4>{0, 2, 1, 3}[j];
    const auto* o = b.data() + offset;
    need(o[1] <= 6, "Invalid TFI detune");
    f.operators[j] = {o[0], uint8_t(o[1] >= 3 ? o[1] - 3 : 7 - o[1]),
                      o[2], o[3],
                      o[4], o[5],
                      o[6], o[7],
                      o[8], o[9],
                      0,    0};
  }
  validate(p);
  return p;
}
std::vector<int> nums(const std::string& s, const std::vector<int>& bounds) {
  std::istringstream in(s);
  std::vector<int> v;
  int n;
  while (in >> n) {
    need(v.size() < bounds.size() && n >= 0 && n <= bounds[v.size()],
         "Preset parameter out of range");
    v.push_back(n);
  }
  need(in.eof() && v.size() == bounds.size(), "Invalid preset numbers");
  return v;
}
void opm(const Bytes& b, std::vector<ExternalPreset>& out, std::string& warning) {
  std::string text(b.begin(), b.end());
  if (text.compare(0, 3, "\xef\xbb\xbf") == 0) text.erase(0, 3);
  std::istringstream in(text);
  std::string line;
  std::map<std::string, std::vector<int>> fields;
  std::set<int> ids;
  std::string title;
  int id = -1, skipped = 0;
  std::string firstReason;
  auto finish = [&] {
    if (id < 0) return;
    need(fields.size() == 6, "Incomplete OPM voice");
    auto& c = fields.at("CH");
    auto& l = fields.at("LFO");
    if ((c[0] != 0 && c[0] != 64 && c[0] != 127) || c[6] || l[4] || (c[5] & 7)) {
      ++skipped;
      if (firstReason.empty()) firstReason = "noise or partial panning";
      return;
    }
    auto p = init(InstrumentType::ArcadeFM, title);
    p.sourceIndex = id;
    auto& f = p.instrument.chip.fourOp;
    f.sourceProgram = id;
    f.bankId = 32768;
    f.pan = c[0] == 0 ? 1 : c[0] == 127 ? 2 : 3;
    f.feedback = c[1];
    f.algorithm = c[2];
    f.amplitudeSensitivity = c[3];
    f.pitchSensitivity = c[4];
    f.operatorMask = c[5] >> 3;
    f.lfoEnabled = 1;
    f.lfoRate = l[0];
    f.amplitudeDepth = l[1];
    f.pitchDepth = l[2];
    f.lfoWave = l[3];
    int n = 0;
    for (const char* k : {"M1", "C1", "M2", "C2"}) {
      auto& v = fields.at(k);
      need(v[10] == 0 || v[10] == 128, "OPM AM must be 0 or 128");
      f.operators[n++] = {
          uint8_t(v[7]), uint8_t(v[8]),      uint8_t(v[5]), uint8_t(v[6]), uint8_t(v[0]),
          uint8_t(v[1]), uint8_t(v[2]),      uint8_t(v[3]), uint8_t(v[4]), 0,
          uint8_t(v[9]), uint8_t(v[10] >> 7)};
    }
    validate(p);
    out.push_back(p);
  };
  while (std::getline(in, line)) {
    need(line.size() <= 1024, "OPM line too long");
    auto comment = line.find("//");
    if (comment != std::string::npos) line.resize(comment);
    comment = line.find(';');
    if (comment != std::string::npos) line.resize(comment);
    line = trim(line);
    if (line.empty()) continue;
    if (line.rfind("@:", 0) == 0) {
      finish();
      fields.clear();
      std::istringstream h(line.substr(2));
      need(bool(h >> id) && id >= 0 && id <= 65535 && ids.insert(id).second && ids.size() <= 4096,
           "Invalid or duplicate OPM voice number");
      std::getline(h, title);
      title = trim(title);
      need(!title.empty(), "Missing OPM voice name");
      continue;
    }
    auto colon = line.find(':');
    need(id >= 0 && colon != std::string::npos, "Invalid OPM field");
    auto key = trim(line.substr(0, colon));
    need(!fields.count(key), "Duplicate OPM field");
    std::vector<int> bounds;
    if (key == "CH")
      bounds = {127, 7, 7, 3, 7, 120, 128};
    else if (key == "LFO")
      bounds = {255, 127, 127, 3, 31};
    else {
      need(key == "M1" || key == "C1" || key == "M2" || key == "C2", "Unknown OPM field");
      bounds = {31, 31, 31, 15, 15, 127, 3, 15, 7, 3, 128};
    }
    fields[key] = nums(line.substr(colon + 1), bounds);
  }
  finish();
  if (skipped) warning = std::to_string(skipped) + " OPM voices skipped: " + firstReason;
}
void oplOperator(OPLOperator& o, Reader& r) {
  unsigned a = r.u8(), k = r.u8(), d = r.u8(), s = r.u8(), w = r.u8();
  need(w <= 7, "Invalid OPL waveform");
  o = {uint8_t(a & 15), uint8_t(k & 63),       uint8_t(d >> 4),
       uint8_t(d & 15), uint8_t(s >> 4),       uint8_t(s & 15),
       uint8_t(w),      uint8_t(k >> 6),       uint8_t((a >> 6) & 1),
       uint8_t(a >> 7), uint8_t((a >> 5) & 1), uint8_t((a >> 4) & 1)};
}
void wopl(const Bytes& b, bool single, InstrumentType target, const std::string& fallback,
          std::vector<ExternalPreset>& out, std::string& warning) {
  Reader r(b);
  need(r.fixed(11) == (single ? "WOPL3-INST" : "WOPL3-BANK"), "Invalid WOPL header");
  unsigned version = r.le16();
  need(version >= 1 && version <= (single ? 2u : 3u), "Unsupported WOPL version");
  unsigned melodic = 1, drums = 0, flags = 0, volume = 0, singleDrum = 0;
  if (single) {
    singleDrum = r.u8();
    need(singleDrum <= 1, "Invalid OPLI type");
  } else {
    melodic = r.be16();
    drums = r.be16();
    flags = r.u8();
    volume = r.u8();
    need(flags <= 3 && volume <= 11, "Unsupported WOPL bank settings");
  }
  unsigned banks = melodic + drums;
  need(banks && banks <= 64, "WOPL bank size limit");
  std::vector<unsigned> bankIds(banks);
  if (!single && version >= 2)
    for (unsigned n = 0; n < banks; ++n) {
      r.skip(32);
      unsigned lsb = r.u8(), msb = r.u8();
      need(lsb <= 127 && msb <= 127, "Invalid WOPL bank number");
      bankIds[n] = msb * 128 + lsb;
    }
  size_t count = single ? 1 : banks * 128, record = (!single && version >= 3) ? 66 : 62;
  need(r.left() == count * record, "Truncated or trailing WOPL data");
  unsigned skipped = 0;
  for (size_t n = 0; n < count; ++n) {
    auto title = r.fixed(32);
    bool unnamed = trim(title).empty();
    auto p = init(target, title);
    auto& o = p.instrument.chip.opl;
    p.sourceIndex = int(n);
    o.noteOffset[0] = int16_t(r.be16());
    o.noteOffset[1] = int16_t(r.be16());
    o.velocityOffset = int8_t(r.u8());
    o.secondDetune = int8_t(r.u8());
    o.drumKey = r.u8();
    unsigned bits = r.u8();
    o.topology = bits & 2   ? OPLTopology::dualVoice
                 : bits & 1 ? OPLTopology::fourOperator
                            : OPLTopology::twoOperator;
    o.fixedNote = bool(bits & 64);
    o.percussion = single ? singleDrum : n / 128 >= melodic;
    o.deepTremolo = flags & 1;
    o.deepVibrato = (flags >> 1) & 1;
    o.volumeModel = volume;
    o.bankId = 32768;
    o.sourceBank = bankIds[single ? 0 : n / 128];
    o.sourceProgram = single ? 0 : n % 128;
    for (int k = 0; k < 2; ++k) {
      unsigned v = r.u8();
      need(!(v & 0xf0), "Unsupported WOPL routing");
      o.feedback[k] = (v >> 1) & 7;
      o.connection[k] = v & 1;
    }
    for (int k : {1, 0, 3, 2}) oplOperator(o.operators[k], r);
    if (record == 66) {
      o.keyOnDuration = r.be16();
      o.keyOffDuration = r.be16();
    }
    if (bits & 4) continue;
    // Some older banks leave unused slots unflagged. Only hide unnamed,
    // fully attenuated operators with no attack; keep named silent patches
    // and unnamed playable voices, preserving their original slot numbers.
    unsigned operators = o.topology == OPLTopology::twoOperator ? 2 : 4;
    if (!single && unnamed &&
        std::all_of(o.operators, o.operators + operators,
                    [](const OPLOperator& op) { return op.attack == 0 && op.level == 63; }))
      continue;
    if (bits & 0xb8) {
      ++skipped;
      continue;
    }
    if (!validOPL(target, o)) {
      ++skipped;
      continue;
    }
    if (unnamed)
      name(p, single ? fallback
                     : std::string(o.percussion ? "Drum " : "Program ") +
                           std::to_string(o.sourceProgram) + " [B" +
                           std::to_string(o.sourceBank) + "]");
    out.push_back(p);
  }
  if (skipped) warning = std::to_string(skipped) + " WOPL voices incompatible with this engine";
}
// WOPLX is the bank editor's textual exchange format. Field names, ranges and
// carrier/modulator order follow its format, not the filename or bank labels.
void woplx(const Bytes& bytes, InstrumentType target, std::vector<ExternalPreset>& out,
           std::string& warning) {
  std::string text(bytes.begin(), bytes.end());
  need(text.rfind("WOPLX-BANK\n", 0) == 0 || text.rfind("WOPLX-BANK\r\n", 0) == 0,
       "Invalid WOPLX header");
  std::istringstream in(text);
  std::string line;
  std::map<std::string, int> globals, bank, attrs, route;
  std::map<int, std::map<std::string, int>> ops;
  std::set<std::string> flags;
  bool info = false, inside = false, have = false, drum = false;
  int program = 0, index = 0, skipped = 0;
  std::string title;
  auto number = [](std::string s) {
    s = trim(s);
    size_t n = 0;
    long v = 0;
    try {
      v = std::stol(s, &n);
    } catch (...) {
      throw std::runtime_error("Invalid WOPLX number");
    }
    need(n == s.size() && v >= -32768 && v <= 65535, "Invalid WOPLX number");
    return int(v);
  };
  auto assignments = [&](std::string s) {
    std::map<std::string, int> result;
    std::istringstream parts(s);
    std::string part;
    while (std::getline(parts, part, ';')) {
      part = trim(part);
      if (part.empty()) continue;
      auto eq = part.find('=');
      need(eq != std::string::npos, "Invalid WOPLX assignment");
      auto k = trim(part.substr(0, eq));
      need(!result.count(k), "Duplicate WOPLX field");
      result[k] = number(part.substr(eq + 1));
    }
    return result;
  };
  auto value = [](const std::map<std::string, int>& m, const std::string& k, int lo, int hi,
                  int def = 0) {
    auto it = m.find(k);
    int v = it == m.end() ? def : it->second;
    need(v >= lo && v <= hi, "WOPLX field out of range: " + k);
    return v;
  };
  auto finish = [&] {
    if (!have) return;
    have = false;
    int source = index++;
    need(index <= 8192, "WOPLX instrument limit");
    if (flags.count("BLANK")) return;
    need(flags.count("2OP") + flags.count("4OP") + flags.count("DV") == 1,
         "Invalid WOPLX topology");
    bool two = flags.count("2OP");
    need(ops.size() == (two ? 2u : 4u) && route.size() == (two ? 2u : 4u),
         "Incomplete WOPLX instrument");
    auto p = init(target, title.empty() ? "Program " + std::to_string(program) : title);
    p.sourceIndex = source;
    auto& o = p.instrument.chip.opl;
    o.bankId = 32768;
    o.sourceBank =
        value(bank, "MIDI_BANK_MSB", 0, 127) * 128 + value(bank, "MIDI_BANK_LSB", 0, 127);
    o.sourceProgram = program;
    o.percussion = drum;
    o.fixedNote = flags.count("FN");
    o.topology = two                  ? OPLTopology::twoOperator
                 : flags.count("4OP") ? OPLTopology::fourOperator
                                      : OPLTopology::dualVoice;
    o.deepVibrato = value(globals, "DEEP_VIBRATO", 0, 1);
    o.deepTremolo = value(globals, "DEEP_TREMOLO", 0, 1);
    o.volumeModel = value(globals, "VOLUME_MODEL", 0, 11);
    o.drumKey = value(attrs, "DRUM_KEY", 0, 127, 60);
    o.noteOffset[0] = value(attrs, "NOTE_OFF_1", -127, 127);
    o.noteOffset[1] = value(attrs, "NOTE_OFF_2", -127, 127);
    o.secondDetune = value(attrs, "FINE_TUNE", -128, 127);
    o.velocityOffset = value(attrs, "VEL_OFF", -128, 127);
    o.keyOnDuration = value(attrs, "DUR_K_ON", 0, 65535);
    o.keyOffDuration = value(attrs, "DUR_K_OFF", 0, 65535);
    for (int k = 0; k < 2; ++k) {
      o.feedback[k] = value(route, "FB" + std::to_string(k + 1), 0, 7);
      o.connection[k] = value(route, "CONN" + std::to_string(k + 1), 0, 1);
    }
    for (unsigned k = 0; k < (two ? 2u : 4u); ++k) {
      auto& v = ops.at(std::array<int, 4>{1, 0, 3, 2}[k]);
      need(v.size() == 12, "Incomplete WOPLX operator");
      auto& d = o.operators[k];
      d = {uint8_t(value(v, "ML", 0, 15)), uint8_t(value(v, "TL", 0, 63)),
           uint8_t(value(v, "AT", 0, 15)), uint8_t(value(v, "DC", 0, 15)),
           uint8_t(value(v, "ST", 0, 15)), uint8_t(value(v, "RL", 0, 15)),
           uint8_t(value(v, "WF", 0, 7)),  uint8_t(value(v, "KL", 0, 3)),
           uint8_t(value(v, "VB", 0, 1)),  uint8_t(value(v, "AM", 0, 1)),
           uint8_t(value(v, "EG", 0, 1)),  uint8_t(value(v, "KR", 0, 1))};
    }
    if (validOPL(target, o))
      out.push_back(p);
    else
      ++skipped;
  };
  std::getline(in, line);
  while (std::getline(in, line)) {
    need(line.size() <= 1024, "WOPLX line limit");
    line = trim(line);
    if (line == "BANK_INFO:") {
      info = true;
      continue;
    }
    if (info) {
      if (line == "BANK_INFO_END") info = false;
      continue;
    }
    if (line.empty()) continue;
    if (line == "MELODIC_BANK:" || line == "PERCUSSION_BANK:") {
      need(!inside, "Nested WOPLX bank");
      inside = true;
      drum = line[0] == 'P';
      bank.clear();
      continue;
    }
    if (line == "MELODIC_BANK_END" || line == "PERCUSSION_BANK_END") {
      need(inside, "Unexpected WOPLX bank end");
      finish();
      inside = false;
      continue;
    }
    if (line.rfind("INSTRUMENT=", 0) == 0) {
      need(inside && bank.count("MIDI_BANK_MSB") && bank.count("MIDI_BANK_LSB"),
           "Missing WOPLX bank identity");
      finish();
      need(line.back() == ':', "Invalid WOPLX voice");
      program = number(line.substr(11, line.size() - 12));
      need(program >= 0 && program <= 127, "Invalid WOPLX program");
      have = true;
      title.clear();
      attrs.clear();
      route.clear();
      ops.clear();
      flags.clear();
      continue;
    }
    if (line.rfind("NAME=", 0) == 0) {
      if (have)
        title = line.substr(5);
      else
        need(inside, "WOPLX name outside bank");
      continue;
    }
    if (line.rfind("FLAGS:", 0) == 0) {
      need(have && flags.empty(), "Duplicate WOPLX flags");
      std::istringstream f(line.substr(6));
      std::string s;
      while (std::getline(f, s, ';')) {
        s = trim(s);
        if (s.empty()) continue;
        need(s == "2OP" || s == "4OP" || s == "DV" || s == "FN" || s == "BLANK",
             "Unsupported WOPLX flags");
        flags.insert(s);
      }
      continue;
    }
    if (line.rfind("ATTRS:", 0) == 0) {
      need(have && attrs.empty(), "Invalid WOPLX attributes");
      attrs = assignments(line.substr(6));
      for (auto& a : attrs)
        need(std::set<std::string>{"DRUM_KEY", "NOTE_OFF_1", "NOTE_OFF_2", "FINE_TUNE", "VEL_OFF",
                                   "DUR_K_ON", "DUR_K_OFF"}
                 .count(a.first),
             "Unknown WOPLX attribute");
      continue;
    }
    if (line.rfind("FBCONN:", 0) == 0) {
      need(have && route.empty(), "Invalid WOPLX routing");
      route = assignments(line.substr(7));
      for (auto& a : route)
        need(std::set<std::string>{"FB1", "FB2", "CONN1", "CONN2"}.count(a.first),
             "Unknown WOPLX routing field");
      continue;
    }
    if (line.size() > 4 && line.rfind("OP", 0) == 0 && line[2] >= '0' && line[2] <= '3' &&
        line[3] == ':') {
      int k = line[2] - '0';
      need(have && !ops.count(k), "Invalid WOPLX operator");
      ops[k] = assignments(line.substr(4));
      for (auto& a : ops[k])
        need(std::set<std::string>{"ML", "TL", "AT", "DC", "ST", "RL", "WF", "KL", "VB", "AM", "EG",
                                   "KR"}
                 .count(a.first),
             "Unknown WOPLX operator field");
      continue;
    }
    auto eq = line.find('=');
    need(eq != std::string::npos, "Unknown WOPLX field");
    auto key = line.substr(0, eq);
    auto& map = inside ? bank : globals;
    need(!have && !map.count(key), "Duplicate WOPLX field");
    need(inside ? (key == "MIDI_BANK_MSB" || key == "MIDI_BANK_LSB")
                : (key == "DEEP_VIBRATO" || key == "DEEP_TREMOLO" || key == "VOLUME_MODEL"),
         "Unsupported WOPLX setting");
    map[key] = number(line.substr(eq + 1));
  }
  need(!inside && !info, "Truncated WOPLX bank");
  if (skipped) warning = std::to_string(skipped) + " WOPLX voices incompatible with this engine";
}
struct Macro {
  int id = 0, loop = 255, release = 255, mode = 0, open = 0, delay = 0, speed = 1;
  std::vector<int32_t> data;
};
struct Furnace {
  unsigned version = 0, type = 0;
  std::string title;
  Bytes fm, gb, c64;
  std::map<int, Macro> macros;
  bool opMacros = false, extra = false;
};
void readMacros(Reader& r, Furnace& f, bool operators) {
  unsigned header = r.le16();
  need(header >= 8 && header <= 32, "Unsupported Furnace macro header");
  while (r.left()) {
    unsigned id = r.u8();
    if (id == 255) break;
    size_t start = r.pos - 1;
    Macro m;
    m.id = id;
    unsigned n = r.u8();
    m.loop = r.u8();
    m.release = r.u8();
    m.mode = r.u8();
    m.open = r.u8();
    m.delay = r.u8();
    m.speed = r.u8();
    r.skip(header - 8);
    need(r.pos == start + header, "Invalid macro header");
    unsigned size = m.open >> 6;
    for (unsigned i = 0; i < n; ++i)
      m.data.push_back(size == 0   ? int32_t(r.u8())
                       : size == 1 ? int8_t(r.u8())
                       : size == 2 ? int16_t(r.le16())
                                   : int32_t(r.le32()));
    if (n) {
      if (operators)
        f.opMacros = true;
      else {
        need(!f.macros.count(id), "Duplicate Furnace macro");
        f.macros[id] = std::move(m);
      }
    }
  }
}
Furnace newFurnace(const Bytes& b) {
  Reader r(b);
  need(r.fixed(4) == "FINS", "Unsupported Furnace container (expected single instrument)");
  Furnace f;
  f.version = r.le16();
  f.type = r.le16();
  need(f.version >= 127 && f.version <= 251, "Unsupported Furnace instrument version");
  std::set<std::string> seen;
  while (r.left()) {
    auto code = r.fixed(2);
    if (code == "EN") break;
    unsigned size = r.le16();
    need(size <= r.left(), "Truncated Furnace feature");
    Reader block(b, r.pos, r.pos + size);
    r.skip(size);
    need(seen.insert(code).second, "Duplicate Furnace feature");
    if (code == "NA")
      f.title = block.str();
    else if (code == "FM")
      f.fm = Bytes(b.begin() + block.pos, b.begin() + block.end);
    else if (code == "GB")
      f.gb = Bytes(b.begin() + block.pos, b.begin() + block.end);
    else if (code == "64")
      f.c64 = Bytes(b.begin() + block.pos, b.begin() + block.end);
    else if (code == "MA")
      readMacros(block, f, false);
    else if (code.size() == 2 && code[0] == 'O' && code[1] >= '1' && code[1] <= '4')
      readMacros(block, f, true);
    else
      f.extra = true;
  }
  return f;
}
Furnace oldFurnace(const Bytes& b) {
  Reader h(b);
  need(h.fixed(16) == "-Furnace instr.-", "Invalid Furnace header");
  Furnace f;
  f.version = h.le16();
  need(f.version >= 29 && f.version <= 112, "Unsupported legacy Furnace version");
  h.skip(2);
  unsigned offset = h.le32();
  need(h.le16() == 0 && h.le16() == 0, "Furnace embedded waves/samples unsupported");
  need(offset >= 32 && offset < b.size(), "Invalid Furnace instrument offset");
  Reader r(b, offset, b.size());
  need(r.fixed(4) == "INST", "Invalid Furnace instrument block");
  unsigned size = r.le32();
  need(size <= r.left(), "Truncated Furnace instrument block");
  need(r.le16() == f.version, "Mismatched Furnace versions");
  f.type = r.u8();
  r.skip(1);
  f.title = r.str();
  unsigned alg = r.u8(), fb = r.u8(), fms = r.u8(), ams = r.u8(), count = r.u8(), preset = r.u8();
  r.skip(2);
  f.fm = {uint8_t((f.type == 14 && count == 2 ? 0x30 : 0xf0) | (f.type == 14 ? count : 4)),
          uint8_t((alg << 4) | fb), uint8_t((ams << 4) | fms),
          uint8_t((count == 4 ? 16 : 0) | preset)};
  for (int i = 0; i < 4; ++i) {
    unsigned am = r.u8(), ar = r.u8(), dr = r.u8(), mul = r.u8(), rr = r.u8(), sl = r.u8(),
             tl = r.u8(), dt2 = r.u8(), rs = r.u8(), dt = r.u8(), d2 = r.u8(), ssg = r.u8();
    unsigned dam = r.u8(), dvb = r.u8(), egt = r.u8(), ksl = r.u8(), sus = r.u8(), vib = r.u8(),
             ws = r.u8(), ksr = r.u8();
    r.skip(12);
    need(am <= 1 && ar <= 31 && dr <= 31 && mul <= 15 && rr <= 15 && sl <= 15 && tl <= 127 &&
             dt2 <= 3 && rs <= 3 && dt <= 7 && d2 <= 31 && ssg <= 15 && ws <= 7 && ksl <= 3 &&
             sus <= 1 && vib <= 1 && ksr <= 1,
         "Invalid legacy Furnace operator");
    f.fm.insert(f.fm.end(),
                {uint8_t((ksr << 7) | (dt << 4) | mul), uint8_t((sus << 7) | tl),
                 uint8_t((rs << 6) | (vib << 5) | ar), uint8_t((am << 7) | (ksl << 5) | dr),
                 uint8_t((egt << 7) | d2), uint8_t((sl << 4) | rr), uint8_t((dvb << 4) | ssg),
                 uint8_t((dam << 5) | (dt2 << 3) | ws)});
  }
  unsigned gv = r.u8(), gd = r.u8(), gl = r.u8(), gs = r.u8();
  need(gv <= 15 && gd <= 1 && gl <= 7 && gs <= 64, "Invalid GB envelope");
  f.gb = {uint8_t(gv | (gd << 4) | (gl << 5)), uint8_t(gs), 2, 0};
  unsigned tri = r.u8(), saw = r.u8(), pulse = r.u8(), noise = r.u8(), a = r.u8(), d = r.u8(),
           s = r.u8(), rel = r.u8(), duty = r.le16(), ring = r.u8(), sync = r.u8(), filter = r.u8(),
           initFilter = r.u8(), cutVol = r.u8(), res = r.u8(), lp = r.u8(), bp = r.u8(),
           hp = r.u8(), ch3 = r.u8(), cut = r.le16(), dutyAbs = r.u8(), cutAbs = r.u8();
  need(a <= 15 && d <= 15 && s <= 15 && rel <= 15 && res <= 15 && duty <= 4095 && cut <= 2047,
       "Invalid C64 parameters");
  f.c64 = {uint8_t(tri | saw << 1 | pulse << 2 | noise << 3 | filter << 4 | cutVol << 5 |
                   initFilter << 6 | dutyAbs << 7),
           uint8_t(lp | hp << 1 | bp << 2 | ch3 << 3 | cutAbs << 4 | ring << 6 | sync << 7),
           uint8_t(a << 4 | d),
           uint8_t(s << 4 | rel),
           uint8_t(duty),
           uint8_t(duty >> 8),
           uint8_t(cut),
           uint8_t((cut >> 8) | (res << 4))};
  r.skip(16);
  std::array<Macro, 20> m;
  std::array<unsigned, 20> lengths{};
  for (int i = 0; i < 20; ++i) m[i].id = i;
  auto lengthsRead = [&](int first, int count) {
    for (int i = first; i < first + count; ++i) {
      lengths[i] = r.le32();
      need(lengths[i] <= 255, "Furnace macro length limit");
    }
  };
  auto positions = [&](int first, int count, bool release) {
    for (int i = first; i < first + count; ++i) {
      uint32_t v = r.le32();
      need(v == 0xffffffff || v <= 255, "Invalid Furnace macro point");
      (release ? m[i].release : m[i].loop) = v == 0xffffffff ? 255 : v;
    }
  };
  auto opens = [&](int first, int count) {
    for (int i = first; i < first + count; ++i) m[i].open = r.u8();
  };
  auto values = [&](int first, int count) {
    for (int i = first; i < first + count; ++i)
      for (unsigned k = 0; k < lengths[i]; ++k) m[i].data.push_back(int32_t(r.le32()));
  };
  lengthsRead(0, 8);
  positions(0, 8, false);
  unsigned arpMode = r.u8();
  r.skip(3);
  values(0, 8);
  lengthsRead(8, 4);
  positions(8, 4, false);
  opens(0, 12);
  values(8, 4);
  auto skipOpHeaders = [&](int count, bool extended) {
    unsigned total = 0;
    for (int op = 0; op < 4; ++op) {
      for (int k = 0; k < count; ++k) {
        unsigned n = r.le32();
        need(n <= 255, "Operator macro size limit");
        total += n;
      }
      r.skip(count * 4);
      if (extended) r.skip(count * 4);
      r.skip(count);
    }
    r.skip(total);
    if (total) f.opMacros = true;
  };
  skipOpHeaders(12, false);
  if (f.version >= 44) {
    positions(0, 12, true);
    r.skip(4 * 12 * 4);
  }
  if (f.version >= 61) skipOpHeaders(8, true);
  if (f.version >= 63) {
    if (r.u8() && f.type == 14) f.extra = true;
    r.skip(7);
  }
  if (f.version >= 67) {
    if (r.u8()) {
      r.skip(720);
      f.extra = true;
    }
  }
  if (f.version >= 73) r.skip(8);
  if (f.version >= 76) {
    lengthsRead(12, 8);
    positions(12, 8, false);
    positions(12, 8, true);
    opens(12, 8);
    values(12, 8);
    r.skip(44);
  }
  if (f.version >= 77) r.skip(2);
  if (f.version >= 79) {
    r.skip(10);
    if (r.u8()) f.extra = true;
    r.skip(6);
  }
  if (f.version >= 84)
    for (int i = 0; i < 20; ++i)
      if (i != 1) m[i].mode = r.u8();
  if (f.version >= 89) {
    if (r.u8() && f.type == 3) f.extra = true;
  }
  if (f.version >= 93) r.skip(32);
  if (f.version >= 104) r.skip(2);
  if (f.version >= 105) {
    unsigned n = r.u8();
    r.skip(n * 3);
    if (n && f.type == 2) f.extra = true;
  }
  if (f.version >= 106) {
    unsigned soft = r.u8(), init = r.u8();
    f.gb[2] = (soft ? 1 : 0) | (init ? 2 : 0);
  }
  if (f.version >= 107) r.skip(13);
  if (f.version >= 109) r.skip(7);
  if (f.version >= 111) {
    for (auto& v : m) v.speed = r.u8();
    for (auto& v : m) v.delay = r.u8();
    r.skip(4 * 40);
  }
  if (f.version < 31)
    for (auto& v : m[1].data) v -= 12;
  if (f.version < 112 && arpMode)
    for (auto& v : m[1].data) v ^= 0x40000000;
  if (f.version < 112 && arpMode && m[1].data.size() < 255 &&
      (m[1].loop >= int(m[1].data.size()) ||
       (m[1].release > m[1].loop && m[1].release < int(m[1].data.size()))))
    m[1].data.push_back(0);
  if (f.type == 3 && f.version < 87) {
    if (!dutyAbs)
      for (auto& v : m[2].data) v -= 12;
    if (cutVol && !cutAbs)
      for (auto& v : m[0].data) v -= 18;
  }
  for (auto& v : m)
    if (!v.data.empty()) f.macros.emplace(v.id, std::move(v));
  return f;
}
void encodeMacros(Furnace& f, ExternalPreset& p) {
  auto t = p.instrument.type;
  bool sid = t == InstrumentType::SID;
  ChipProgram* program = sid               ? &p.instrument.chip.sid.program
                         : isSimpleChip(t) ? &p.instrument.chip.simpleChip.program
                                           : nullptr;
  need(!f.opMacros, "Furnace operator macros are not supported");
  for (auto& entry : f.macros) {
    Macro m = entry.second;
    if (m.data.empty()) continue;
    need(!(m.open & 6), "Furnace ADSR/LFO macro mode is not supported");
    bool allZero = std::all_of(m.data.begin(), m.data.end(), [](int v) { return v == 0; });
    if (!program) {
      if (isOPLL(t) && m.id == 3 && m.mode == 0 && m.delay == 0 && m.data[0] >= 0 &&
          m.data[0] <= 15 &&
          std::all_of(m.data.begin(), m.data.end(), [&](int v) { return v == m.data[0]; })) {
        if (m.data[0]) {
          opllApplyPreset(&p.instrument, m.data[0]);
          name(p, p.name);
        }
        continue;
      }
      if (m.id == 0 && m.delay == 0 && m.mode == 0 &&
          std::all_of(m.data.begin(), m.data.end(),
                      [&](int v) { return v == (isOPLL(t) ? 15 : 127); }))
        continue;
      need(allZero && (m.id == 1 || m.id == 4),
           "Animated FM macro " + std::to_string(m.id) + " is not supported");
      continue;
    }
    if (sid && m.id == 0 && f.version < 187 && (f.c64[0] & 32)) m.id = 8;
    if (m.id == 4) {
      need(allZero, "Furnace fine-pitch macro requires source tuning mode");
      continue;
    }
    if (m.id == 12) {
      need(std::all_of(m.data.begin(), m.data.end(), [](int v) { return v == 3; }),
           "Furnace panning macro is not supported");
      continue;
    }
    need(m.id == 0 || m.id == 1 || m.id == 2 || (sid && (m.id == 3 || m.id == 8)),
         "Unsupported Furnace macro " + std::to_string(m.id));
    if (m.id == 2 && sid) need(f.c64[0] & 128, "Relative SID pulse macros are not supported");
    if (m.id == 8) {
      need(f.c64[1] & 16, "Relative SID filter macros are not supported");
      m.id = 5;
    }
    if (m.id == 3)
      for (auto& v : m.data) need(v > 0 && v <= 8, "Unsupported SID waveform combination");
    if (m.id == 0 && (t == InstrumentType::GBPulse || t == InstrumentType::GBNoise) &&
        f.gb.size() >= 3 && !(f.gb[2] & 1))
      continue;
    if (m.id == 0)
      for (auto v : m.data) need(v >= 0 && v <= 15, "Invalid volume macro");
    if (m.id == 2 && !sid)
      for (auto v : m.data) need(v >= 0 && v <= 3, "Invalid duty macro");
    if (m.id == 2 && sid)
      for (auto v : m.data) need(v >= 0 && v <= 4095, "Invalid SID duty");
    if (m.id == 5)
      for (auto v : m.data) need(v >= 0 && v <= 2047, "Invalid SID cutoff");
    if (m.id == 1)
      for (auto& v : m.data) {
        // Source fixed notes use chip-specific octaves. Canonical fixed notes
        // use MIDI pitch, except GB noise where note - 60 indexes its table.
        const bool fixed = v < 0 ? !(uint32_t(v) & 0x40000000) : bool(v & 0x40000000);
        if (fixed) {
          int note = (v ^ 0x40000000) + (sid ? 12 : t == InstrumentType::GBNoise ? 84 : 36);
          need(note >= 0 && note <= 180, "Absolute source note outside playback range");
          v = note | 0x40000000;
        }
        int n = (uint32_t(v) & 0xc0000000) == 0x40000000 ? v ^ 0x40000000 : v;
        need(n >= -120 && n <= 180, "Invalid arpeggio macro");
      }
    need(m.mode == 0, "Unsupported Furnace macro mode");
    need(m.speed >= 1, "Invalid macro speed");
    need(m.loop == 255 || m.loop < int(m.data.size()), "Invalid macro loop");
    need(m.release == 255 || m.release < int(m.data.size()), "Invalid macro release");
    size_t n = 8 + 4 * m.data.size();
    need(program->size + n <= sizeof(program->data), "Imported macro storage limit");
    program->format = 1;
    program->rate = 60;
    auto* dst = program->data + program->size;
    dst[0] = m.id;
    dst[1] = m.data.size();
    dst[2] = m.loop;
    dst[3] = m.release;
    dst[4] = (m.open & 8) ? 2 : 0;
    dst[5] = m.delay;
    dst[6] = m.speed;
    dst[7] = 0;
    for (size_t j = 0; j < m.data.size(); ++j) {
      uint32_t v = m.data[j];
      for (int k = 0; k < 4; ++k) dst[8 + 4 * j + k] = v >> (8 * k);
    }
    program->size += n;
  }
}
void applyFM(Furnace& source, ExternalPreset& p) {
  Reader r(source.fm);
  unsigned flags = r.u8(), algfb = r.u8(), sens = r.u8(), opll = r.u8();
  if (source.version >= 224) need(r.u8() == 0, "Fixed FM block is unsupported");
  unsigned count = flags & 15;
  need(count == 2 || count == 4, "Invalid FM operator count");
  std::array<std::array<uint8_t, 8>, 4> ops{};
  for (unsigned k = 0; k < count; ++k)
    for (auto& x : ops[k]) x = r.u8();
  need(!r.left() || source.version < 127, "Trailing FM data");
  auto t = p.instrument.type;
  if (isFourOp(t)) {
    need(count == 4, "Four FM operators required");
    auto& d = p.instrument.chip.fourOp;
    d.algorithm = (algfb >> 4) & 7;
    d.feedback = algfb & 7;
    d.amplitudeSensitivity = (sens >> 4) & 3;
    d.pitchSensitivity = sens & 7;
    d.operatorMask = 0;
    for (int k = 0; k < 4; ++k) {
      int n = std::array<int, 4>{0, 2, 1, 3}[k];
      auto& x = ops[n];
      if (flags & (16 << k)) d.operatorMask |= 1 << k;
      d.operators[k] = {uint8_t(x[0] & 15),
                        uint8_t((x[0] >> 4) & 7),
                        uint8_t(x[1] & 127),
                        uint8_t(x[2] >> 6),
                        uint8_t(x[2] & 31),
                        uint8_t(x[3] & 31),
                        uint8_t(x[4] & 31),
                        uint8_t(x[5] & 15),
                        uint8_t(x[5] >> 4),
                        uint8_t(t == InstrumentType::GenesisFM ? x[6] & 15 : 0),
                        uint8_t(t == InstrumentType::ArcadeFM ? (x[7] >> 3) & 3 : 0),
                        uint8_t(x[3] >> 7)};
    }
    need(!d.amplitudeSensitivity && !d.pitchSensitivity,
         "FM LFO speed is song-specific; patch import cannot restore it");
  } else if (isOPL(t)) {
    auto& d = p.instrument.chip.opl;
    bool four = opll & 16;
    need(!four || t == InstrumentType::OPL3, "Four-operator patch requires OPL3");
    d.topology = four ? OPLTopology::fourOperator : OPLTopology::twoOperator;
    unsigned alg = (algfb >> 4) & 7;
    need(alg < (four ? 4u : 2u), "Invalid OPL algorithm");
    d.connection[0] = alg & 1;
    d.connection[1] = (alg >> 1) & 1;
    d.feedback[0] = algfb & 7;
    need(((flags >> 4) & (four ? 15u : 3u)) == (four ? 15u : 3u),
         "Disabled OPL operators unsupported");
    for (unsigned k = 0; k < (four ? 4u : 2u); ++k) {
      auto& x = ops[four ? std::array<int, 4>{0, 2, 1, 3}[k] : k];
      d.operators[k] = {uint8_t(x[0] & 15), uint8_t(x[1] & 127),      uint8_t(x[2] & 31),
                        uint8_t(x[3] & 31), uint8_t(x[5] >> 4),       uint8_t(x[5] & 15),
                        uint8_t(x[7] & 7),  uint8_t((x[3] >> 5) & 3), uint8_t((x[2] >> 5) & 1),
                        uint8_t(x[3] >> 7), uint8_t(x[1] >> 7),       uint8_t(x[0] >> 7)};
    }
  } else {
    auto& d = p.instrument.chip.opll;
    unsigned preset = opll & 15;
    need((opll & 31) < 16, "OPLL drum preset unsupported");
    if (preset) {
      opllApplyPreset(&p.instrument, preset);
      name(p, source.title);
      return;
    }
    d.program = 0;
    auto& a = ops[0];
    auto& b = ops[1];
    d.patch[0] =
        (a[0] & 15) | ((a[0] >> 7) << 4) | ((a[6] & 8) << 2) | ((a[2] & 32) << 1) | (a[3] & 128);
    d.patch[1] =
        (b[0] & 15) | ((b[0] >> 7) << 4) | ((b[6] & 8) << 2) | ((b[2] & 32) << 1) | (b[3] & 128);
    d.patch[2] = ((a[3] & 96) << 1) | (a[1] & 63);
    d.patch[3] = ((b[3] & 96) << 1) | ((sens & 7) ? 16 : 0) | ((sens & 48) ? 8 : 0) | (algfb & 7);
    d.patch[4] = ((a[2] & 15) << 4) | (a[3] & 15);
    d.patch[5] = ((b[2] & 15) << 4) | (b[3] & 15);
    d.patch[6] = a[5];
    d.patch[7] = b[5];
    need(!(b[1] & 127), "OPLL carrier volume must be applied by its tracker");
  }
  validate(p);
}
void markGameBoy(ExternalPreset& p, bool softwareEnvelope) {
  auto& prog = p.instrument.chip.simpleChip.program;
  need(prog.size + 12 <= sizeof(prog.data), "Imported macro storage limit");
  const uint8_t marker[] = {15, 1, 255, 255, 0, 0, 1, 0, uint8_t(softwareEnvelope), 0, 0, 0};
  memcpy(prog.data + prog.size, marker, sizeof(marker));
  prog.size += sizeof(marker);
  prog.format = 1;
  prog.rate = 60;
}
ExternalPreset furnace(const Bytes& b, InstrumentType target, const std::string& fallback) {
  auto f = b.size() >= 4 && !memcmp(b.data(), "FINS", 4) ? newFurnace(b) : oldFurnace(b);
  unsigned want = target == InstrumentType::GenesisFM  ? 1
                  : target == InstrumentType::ArcadeFM ? 33
                  : isOPL(target)                      ? 14
                  : isOPLL(target)                     ? 13
                  : target == InstrumentType::SID      ? 3
                  : target == InstrumentType::SegaPSG  ? 0
                                                       : 2;
  need(f.type == want, "Furnace instrument belongs to another chip");
  need(!f.extra, "Furnace instrument uses unsupported features");
  auto p = init(target, f.title.empty() ? fallback : f.title);
  if (isFourOp(target) || isOPL(target) || isOPLL(target)) {
    need(!f.fm.empty(), "Missing Furnace FM patch");
    applyFM(f, p);
  } else if (target == InstrumentType::SID) {
    need(f.c64.size() >= 8, "Missing Furnace SID patch");
    auto& d = p.instrument.chip.sid;
    initSIDPatch(&d);
    name(p, f.title.empty() ? fallback : f.title);
    auto* v = d.value;
    v[sidPulseDepth] = v[sidPulseRate] = v[sidVibratoDepth] = v[sidVibratoRate] = 0;
    v[sidWave] = f.c64[0] & 15;
    v[sidAttack] = f.c64[2] >> 4;
    v[sidDecay] = f.c64[2] & 15;
    v[sidSustain] = f.c64[3] >> 4;
    v[sidRelease] = f.c64[3] & 15;
    v[sidPulse] = f.c64[4] | (f.c64[5] << 8);
    v[sidCutoff] = f.c64[6] | ((f.c64[7] & 7) << 8);
    v[sidResonance] = f.c64[7] >> 4;
    v[sidFilterMode] =
        (f.c64[0] & 16) ? ((f.c64[1] & 1) | ((f.c64[1] & 4) >> 1) | ((f.c64[1] & 2) << 1)) : 0;
    need(!(f.c64[1] & 0xe8), "SID patch depends on another voice or retained oscillator state");
  } else {
    auto& d = p.instrument.chip.simpleChip;
    d.attack = d.decay = 0;
    d.sustain = 255;
    d.release = 0;
    d.filterEnabled = 0;
    d.segaBassExtension = 0;
    d.mode = 0;
    d.sweepPeriod = d.sweepShift = d.sweepNegate = 0;
    if (target != InstrumentType::SegaPSG) {
      need(f.gb.size() >= 2, "Missing Furnace Game Boy envelope");
      need(f.gb[1] == 64, "Game Boy sound-length counter unsupported");
      need(f.gb.size() < 4 || f.gb[3] == 0, "Game Boy hardware envelope sequence unsupported");
      d.envelopeInitial = f.gb[0] & 15;
      d.envelopeIncrease = (f.gb[0] >> 4) & 1;
      d.envelopePeriod = f.gb[0] >> 5;
    }
  }
  encodeMacros(f, p);
  if (target == InstrumentType::GBPulse || target == InstrumentType::GBNoise) {
    // Lane 15 is source metadata, independently of tracker macro IDs.
    markGameBoy(p, f.gb.size() >= 3 && (f.gb[2] & 1));
  }
  validate(p);
  return p;
}
ExternalPreset dmp(const Bytes& b, InstrumentType target, const std::string& title) {
  Reader r(b);
  need(r.u8() == 11, "Only DefleMask DMP version 11 is supported");
  unsigned system = r.u8(), mode = r.u8();
  Furnace f;
  f.version = 201;
  f.title = title;
  if (mode == 1) {
    need((system == 2 && target == InstrumentType::GenesisFM) ||
             (system == 8 && target == InstrumentType::ArcadeFM),
         "DMP chip does not match this engine");
    unsigned fms = r.u8(), fb = r.u8(), alg = r.u8(), ams = r.u8();
    need(fms <= 7 && fb <= 7 && alg <= 7 && ams <= 3, "Invalid DMP FM controls");
    f.fm = {244, uint8_t((alg << 4) | fb), uint8_t((ams << 4) | fms), 16};
    for (int i = 0; i < 4; ++i) {
      unsigned mul = r.u8(), tl = r.u8(), ar = r.u8(), dr = r.u8(), sl = r.u8(), rr = r.u8(),
               am = r.u8(), rs = r.u8(), dt = r.u8(), d2 = r.u8(), ssg = r.u8();
      need(mul <= 15 && tl <= 127 && ar <= 31 && dr <= 31 && sl <= 15 && rr <= 15 && am <= 1 &&
               rs <= 3 && (dt & 15) <= 7 && (dt >> 4) <= 3 && d2 <= 31 && ssg <= 15,
           "Invalid DMP operator");
      f.fm.insert(f.fm.end(), {uint8_t(((dt & 7) << 4) | mul), uint8_t(tl), uint8_t((rs << 6) | ar),
                               uint8_t((am << 7) | dr), uint8_t(d2), uint8_t((sl << 4) | rr),
                               uint8_t(ssg), uint8_t((dt >> 4) << 3)});
    }
    need(!r.left(), "Trailing DMP data");
    auto p = init(target, title);
    applyFM(f, p);
    return p;
  }
  need(mode == 0 && ((system == 3 && target == InstrumentType::SegaPSG) ||
                     (system == 4 &&
                      (target == InstrumentType::GBPulse || target == InstrumentType::GBNoise))),
       "Unsupported DMP chip/type");
  auto macro = [&](int id) {
    Macro m;
    m.id = id;
    unsigned n = r.u8();
    for (unsigned i = 0; i < n; ++i) m.data.push_back(int32_t(r.le32()));
    if (n) {
      m.loop = r.u8();
      f.macros[id] = std::move(m);
    }
  };
  if (system != 4) macro(0);
  macro(1);
  unsigned fixed = r.u8();
  need(fixed <= 1, "Invalid DMP arpeggio mode");
  for (auto& v : f.macros[1].data) {
    if (fixed)
      v |= 0x40000000;
    else
      v -= 12;
  }
  macro(2);
  macro(3);
  need(f.macros[3].data.empty(), "DMP wavetable channel unsupported");
  auto p = init(target, title);
  auto& d = p.instrument.chip.simpleChip;
  d.attack = d.decay = d.release = 0;
  d.sustain = 255;
  d.filterEnabled = 0;
  d.mode = 0;
  d.segaBassExtension = 0;
  d.sweepPeriod = d.sweepShift = d.sweepNegate = 0;
  if (system == 4) {
    d.envelopeInitial = r.u8();
    d.envelopeIncrease = r.u8();
    d.envelopePeriod = r.u8();
    need(r.u8() == 64, "DMP GB sound-length counter unsupported");
  }
  need(!r.left(), "Trailing DMP data");
  encodeMacros(f, p);
  if (system == 4) markGameBoy(p, false);
  validate(p);
  return p;
}
ExternalPreset goat(const Bytes& b, const std::string& title) {
  need(b.size() >= 33 && b.size() <= sizeof(ChipProgram::data) && !memcmp(b.data(), "GTI5", 4),
       "Expected GoatTracker GTI5 instrument");
  auto p = init(InstrumentType::SID, title);
  auto& d = p.instrument.chip.sid;
  d.program.format = 2;
  d.program.rate = 50;
  d.program.size = b.size();
  memcpy(d.program.data, b.data(), b.size());
  need(validChipProgram(d.program), "Invalid GoatTracker table layout");
  Reader r(b, 13, 29);
  name(p, r.fixed(16));
  if (p.name == "Imported preset") name(p, title);
  d.value[sidPulseDepth] = d.value[sidPulseRate] = d.value[sidFilterMode] =
      d.value[sidVibratoDepth] = 0;
  d.value[sidAttack] = b[4] >> 4;
  d.value[sidDecay] = b[4] & 15;
  d.value[sidSustain] = b[5] >> 4;
  d.value[sidRelease] = b[5] & 15;
  d.value[sidPulse] = 2048;
  d.value[sidWave] = 2;
  need(!b[9], "GoatTracker instrument vibrato requires source playback settings");
  size_t pos = 29;
  for (unsigned table = 0; table < 4; ++table) {
    unsigned n = b[pos++];
    for (unsigned k = 0; k < n; ++k) {
      unsigned a = b[pos + k], v = b[pos + n + k];
      if (a == 255) {
        need(v <= n, "Invalid GoatTracker table jump");
        continue;
      }
      if (table == 0) {
        need(a < 0xf0 || a == 0xf5 || a == 0xf6, "GoatTracker wavetable command unsupported");
        if (a >= 0x10 && a < 0xe0) {
          need(!(a & 6), "SID ring/sync depends on another tracker voice");
          need((a >> 4) <= 8, "Unsupported combined-noise waveform");
        }
      }
    }
    pos += 2 * n;
  }
  validate(p);
  return p;
}
ExternalPreset sbi(const Bytes& b, InstrumentType target, const std::string& title) {
  need(b.size() == 52 && !memcmp(b.data(), "SBI\x1a", 4),
       "Expected 52-byte Sound Blaster SBI instrument");
  Reader r(b, 4, b.size());
  auto p = init(target, r.fixed(32));
  if (p.name == "Imported preset") name(p, title);
  auto& d = p.instrument.chip.opl;
  for (int n = 0; n < 2; ++n) {
    Bytes registers;
    for (int k = 0; k < 5; ++k) registers.push_back(b[36 + k * 2 + n]);
    Reader op(registers);
    oplOperator(d.operators[n], op);
  }
  need(!(b[46] & 0xf0), "Unsupported SBI routing");
  d.feedback[0] = (b[46] >> 1) & 7;
  d.connection[0] = b[46] & 1;
  validate(p);
  return p;
}
}  // namespace

bool externalPresetExtension(InstrumentType t, const std::string& path) {
  auto e = ext(path);
  if (t == InstrumentType::DX7) return e == ".syx";
  if (e == ".dmp") return isSimpleChip(t) || isFourOp(t);
  if (e == ".fui")
    return t == InstrumentType::SID || isSimpleChip(t) || isFourOp(t) || isOPL(t) || isOPLL(t);
  if (t == InstrumentType::GenesisFM) return e == ".tfi";
  if (t == InstrumentType::ArcadeFM) return e == ".opm";
  if (isOPL(t)) return e == ".wopl" || e == ".opli" || e == ".woplx" || e == ".sbi";
  if (t == InstrumentType::SID) return e == ".ins";
  return false;
}
bool importExternalPresets(InstrumentType t, const std::string& path, const Bytes& data,
                           std::vector<ExternalPreset>& output, std::string& warning) {
  warning.clear();
  try {
    need(!data.empty() && data.size() <= 1024 * 1024, "Preset exceeds 1 MiB limit");
    need(externalPresetExtension(t, path), "Unsupported format for this engine");
    std::vector<ExternalPreset> parsed;
    auto e = ext(path), title = std::filesystem::path(path).stem().string();
    if (e == ".syx") {
      std::vector<InstrumentDX7> voices;
      std::string error;
      need(importDX7SysEx(data.data(), data.size(), voices, error), error);
      for (size_t n = 0; n < voices.size(); ++n) {
        auto p = init(t, voices[n].presetName);
        p.instrument.chip.dx7 = voices[n];
        p.sourceIndex = n;
        parsed.push_back(p);
      }
    } else if (e == ".tfi")
      parsed.push_back(tfi(data, title));
    else if (e == ".opm")
      opm(data, parsed, warning);
    else if (e == ".wopl" || e == ".opli")
      wopl(data, e == ".opli", t, title, parsed, warning);
    else if (e == ".woplx")
      woplx(data, t, parsed, warning);
    else if (e == ".sbi")
      parsed.push_back(sbi(data, t, title));
    else if (e == ".fui")
      parsed.push_back(furnace(data, t, title));
    else if (e == ".dmp")
      parsed.push_back(dmp(data, t, title));
    else if (e == ".ins")
      parsed.push_back(goat(data, title));
    else
      need(false, "Unsupported preset format");
    need(!parsed.empty(), warning.empty() ? "No compatible instruments in this file" : warning);
    output = std::move(parsed);
    return true;
  } catch (const std::exception& e) {
    warning = e.what();
    return false;
  }
}
