#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>

#include "chip_program.h"
#include "doctest.h"
#include "external_presets.h"
#include "sid_patch.h"
#include "synth/sid_voice.h"
#include "synth/simple_chip_voice.h"
#include "user_presets.h"
namespace {
using Bytes = std::vector<uint8_t>;
Bytes read(const std::filesystem::path& p) {
  std::ifstream f(p, std::ios::binary);
  return Bytes(std::istreambuf_iterator<char>(f), {});
}
void write(const std::filesystem::path& p, const Bytes& b) {
  std::ofstream f(p, std::ios::binary);
  f.write(reinterpret_cast<const char*>(b.data()), b.size());
}
Bytes tfiFixture() {
  Bytes b(42);
  b[0] = 4;
  b[1] = 3;
  for (int n = 0; n < 4; ++n) {
    auto p = 2 + n * 10;
    b[p] = n + 1;
    b[p + 1] = 3;
    b[p + 2] = n * 7;
    b[p + 3] = 1;
    b[p + 4] = 31;
    b[p + 5] = 8;
    b[p + 6] = 3;
    b[p + 7] = 7;
    b[p + 8] = 5;
  }
  return b;
}
Bytes fuiPSG() {
  Bytes b = {'F', 'I', 'N', 'S', 201, 0,   0,   0,  'N', 'A', 5, 0,
             'T', 'e', 's', 't', 0,   'M', 'A', 28, 0,   8,   0};
  // volume 15 -> 10 -> 0; hold at step 1, release continues to 2
  Bytes m = {0, 3,   255, 1,
             0, 192, 0,   1};  // macro header: id,len,loop,release,mode,open,delay,speed
  b.insert(b.end(), m.begin(), m.end());
  for (int v : {15, 10, 0}) {
    b.push_back(v);
    b.insert(b.end(), 3, 0);
  }
  b.insert(b.end(), {255, 0, 0, 0, 0, 0});
  return b;
}
void roundtrip(const ExternalPreset& preset) {
  auto a = std::make_unique<Project>(), b = std::make_unique<Project>();
  projectInit(a.get());
  projectInit(b.get());
  fillFXNames();
  a->instruments[0] = preset.instrument;
  a->tables[0] = preset.table;
  REQUIRE(instrumentSave(a.get(), "build/tests/external-roundtrip.cni", 0) == 0);
  auto bytes = read("build/tests/external-roundtrip.cni");
  REQUIRE(instrumentLoadMemory(b.get(), bytes.data(), bytes.size(), 0) == 0);
  REQUIRE(instrumentSave(b.get(), "build/tests/external-roundtrip-2.cni", 0) == 0);
  bool identical = read("build/tests/external-roundtrip-2.cni") == bytes;
  CHECK(identical);
  projectFree(a.get());
  projectFree(b.get());
}
}  // namespace
TEST_CASE("External TFI import preserves operators and rejects malformed input transactionally") {
  std::vector<ExternalPreset> out;
  std::string error;
  auto b = tfiFixture();
  REQUIRE(importExternalPresets(InstrumentType::GenesisFM, "Test.TFI", b, out, error));
  REQUIRE(out.size() == 1);
  auto& p = out[0].instrument.chip.fourOp;
  CHECK(p.algorithm == 4);
  CHECK(p.feedback == 3);
  CHECK(p.operators[1].multiplier == 3);
  CHECK(p.operators[2].multiplier == 2);
  roundtrip(out[0]);
  for (size_t n = 0; n < b.size(); ++n) {
    Bytes truncated(b.begin(), b.begin() + n);
    CHECK_FALSE(
        importExternalPresets(InstrumentType::GenesisFM, "Test.tfi", truncated, out, error));
    CHECK(out.size() == 1);
    CHECK(out[0].instrument.chip.fourOp.algorithm == 4);
  }
  b[3] = 7;
  CHECK_FALSE(importExternalPresets(InstrumentType::GenesisFM, "Bad.tfi", b, out, error));
  CHECK_FALSE(
      importExternalPresets(InstrumentType::ArcadeFM, "Test.tfi", tfiFixture(), out, error));
}
TEST_CASE("USER loads external files transactionally and keeps an owned portable patch") {
  const auto root = std::filesystem::path("build/tests/external-user");
  std::filesystem::create_directories(root);
  write(root / "voice.tfi", tfiFixture());
  UserPresets browser;
  browser.setup(root.string(), InstrumentType::GenesisFM);
  std::string error;
  REQUIRE(browser.refresh(error));
  REQUIRE(browser.items().size() == 1);
  auto ref = browser.reference(0);
  auto project = std::make_unique<Project>();
  projectInit(project.get());
  REQUIRE(browser.load(ref, project.get(), 7, error));
  CHECK(project->instruments[7].chip.fourOp.algorithm == 4);
  auto before = project->instruments[7];
  write(root / "voice.tfi", {1, 2});
  CHECK_FALSE(browser.load(ref, project.get(), 7, error));
  CHECK(!memcmp(&before, &project->instruments[7], sizeof(before)));
  std::filesystem::remove(root / "voice.tfi");
  CHECK(project->instruments[7].chip.fourOp.algorithm == 4);
  projectFree(project.get());
}
TEST_CASE("Furnace PSG macros keep loop release timing and survive CNI and song saving") {
  std::vector<ExternalPreset> out;
  std::string error;
  auto bytes = fuiPSG();
  INFO(error);
  REQUIRE(importExternalPresets(InstrumentType::SegaPSG, "Test.fui", bytes, out, error));
  auto& p = out[0].instrument.chip.simpleChip;
  REQUIRE(p.program.format == 1);
  ChipMacroPlayer m;
  m.reset(p.program);
  CHECK(m.value(0) == 15);
  m.tick(p.program);
  CHECK(m.value(0) == 10);
  m.tick(p.program);
  CHECK(m.value(0) == 10);
  m.release(p.program);
  m.tick(p.program);
  CHECK(m.value(0) == 0);
  CHECK(m.done(0));
  roundtrip(out[0]);
  SimpleChipVoice a, b;
  a.init(48000);
  b.init(48000);
  a.configure(InstrumentType::SegaPSG, &p, 6000, .25f);
  b.configure(InstrumentType::SegaPSG, &p, 6000, .25f);
  a.noteOn();
  b.noteOn();
  std::vector<float> x(9600), y(x.size());
  a.render(x.data(), x.size());
  for (size_t n = 0; n < y.size();) {
    auto k = std::min<size_t>(127, y.size() - n);
    b.render(y.data() + n, k);
    n += k;
  }
  CHECK(x == y);
  double energy = 0;
  for (float v : x) {
    CHECK(std::isfinite(v));
    energy += v * v;
  }
  CHECK(energy > 1e-8);
  a.noteOff();
  a.render(x.data(), x.size());
  CHECK_FALSE(a.active());
  auto song = std::make_unique<Project>(), loaded = std::make_unique<Project>();
  projectInit(song.get());
  projectInit(loaded.get());
  REQUIRE(!projectLoad(song.get(), "packaging/common/projects/gm-midi-demo.cct"));
  song->instruments[0] = out[0].instrument;
  REQUIRE(projectSave(song.get(), "build/tests/external-program.cct") == 0);
  REQUIRE(projectLoad(loaded.get(), "build/tests/external-program.cct") == 0);
  CHECK(!memcmp(&p.program, &loaded->instruments[0].chip.simpleChip.program, sizeof(p.program)));
  projectFree(song.get());
  projectFree(loaded.get());
}
TEST_CASE("External community corpus audit") {
  const char* configured = std::getenv("CHOOCHOO_COMMUNITY_PACKS");
  if (!configured) return;
  std::filesystem::path root = configured;
  std::ofstream report(root / "compatibility.tsv");
  report << "engine\tfile\tvoices\tresult\n";
  const std::pair<const char*, InstrumentType> families[] = {
      {"C64", InstrumentType::SID},           {"GB", InstrumentType::GBPulse},
      {"GB", InstrumentType::GBNoise},        {"SN7", InstrumentType::SegaPSG},
      {"OPLL", InstrumentType::OPLL},         {"OPLL", InstrumentType::VRC7},
      {"OPL", InstrumentType::OPL2},          {"OPL", InstrumentType::OPL3},
      {"OPN", InstrumentType::GenesisFM},     {"OPM", InstrumentType::ArcadeFM},
      {"uhrwerk", InstrumentType::GenesisFM}, {"uhrwerk", InstrumentType::ArcadeFM}};
  unsigned accepted = 0, rejected = 0;
  for (auto [family, type] : families)
    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             root / "Furnace-Community/instruments" / family)) {
      if (!entry.is_regular_file() || !externalPresetExtension(type, entry.path().string()))
        continue;
      std::vector<ExternalPreset> out;
      std::string error;
      bool ok = importExternalPresets(type, entry.path().string(), read(entry.path()), out, error);
      report << int(type) << '\t' << entry.path().lexically_relative(root).string() << '\t'
             << out.size() << '\t' << (ok ? "OK " : "SKIP ") << error << '\n';
      if (ok) {
        ++accepted;
        for (const auto& p : out) {
          CAPTURE(entry.path().string());
          roundtrip(p);
        }
      } else
        ++rejected;
    }
  for (const auto& entry : std::filesystem::recursive_directory_iterator(root / "GoatTracker"))
    if (entry.is_regular_file() && entry.path().extension() == ".ins") {
      std::vector<ExternalPreset> out;
      std::string error;
      bool ok = importExternalPresets(InstrumentType::SID, entry.path().string(),
                                      read(entry.path()), out, error);
      report << 27 << '\t' << entry.path().lexically_relative(root).string() << '\t' << out.size()
             << '\t' << (ok ? "OK " : "SKIP ") << error << '\n';
      if (ok) {
        ++accepted;
        roundtrip(out[0]);
      } else
        ++rejected;
    }
  for (auto type : {InstrumentType::OPL2, InstrumentType::OPL3})
    for (const auto& entry : std::filesystem::directory_iterator(root))
      if ((entry.path().extension() == ".wopl" || entry.path().extension() == ".woplx")) {
        std::vector<ExternalPreset> out;
        std::string error;
        bool ok =
            importExternalPresets(type, entry.path().string(), read(entry.path()), out, error);
        report << int(type) << '\t' << entry.path().filename().string() << '\t' << out.size()
               << '\t' << (ok ? "OK " : "SKIP ") << error << '\n';
        if (ok) {
          ++accepted;
          for (const auto& p : out) roundtrip(p);
        } else
          ++rejected;
      }
  const std::pair<const char*, InstrumentType> extra[] = {
      {"TFI-Game-Sounds", InstrumentType::GenesisFM},
      {"Dexed-Community", InstrumentType::DX7},
      {"YMulator", InstrumentType::ArcadeFM}};
  for (auto [folder, type] : extra) {
    std::set<std::string> checked;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root / folder)) {
      if (!entry.is_regular_file() || !externalPresetExtension(type, entry.path().string()))
        continue;
      std::vector<ExternalPreset> out;
      std::string error;
      bool ok = importExternalPresets(type, entry.path().string(), read(entry.path()), out, error);
      report << int(type) << '\t' << entry.path().lexically_relative(root).string() << '\t'
             << out.size() << '\t' << (ok ? "OK " : "SKIP ") << error << '\n';
      if (ok) {
        ++accepted;
        if (checked.insert(entry.path().parent_path().string()).second)
          for (const auto& p : out) {
            CAPTURE(entry.path().string());
            roundtrip(p);
          }
      } else
        ++rejected;
    }
  }
  INFO("Accepted files: " << accepted << "; excluded: " << rejected);
  CHECK(accepted > 0);
}

TEST_CASE("External FM formats map chip register order and reject incompatible modes") {
  std::vector<ExternalPreset> out;
  std::string error;
  Bytes sbi(52);
  memcpy(sbi.data(), "SBI\x1a", 4);
  memcpy(sbi.data() + 4, "Register voice", 14);
  sbi[36] = 0xc3;
  sbi[37] = 4;
  sbi[38] = 0x85;
  sbi[40] = 0xf2;
  sbi[42] = 0x87;
  sbi[44] = 3;
  sbi[46] = 7;
  REQUIRE(importExternalPresets(InstrumentType::OPL2, "voice.sbi", sbi, out, error));
  CHECK(out[0].instrument.chip.opl.operators[0].multiplier == 3);
  CHECK(out[0].instrument.chip.opl.operators[0].attack == 15);
  CHECK(out[0].instrument.chip.opl.operators[0].level == 5);
  CHECK(out[0].instrument.chip.opl.operators[0].waveform == 3);
  roundtrip(out[0]);
  sbi[44] = 7;
  CHECK_FALSE(importExternalPresets(InstrumentType::OPL2, "voice.sbi", sbi, out, error));
  REQUIRE(importExternalPresets(InstrumentType::OPL3, "voice.sbi", sbi, out, error));
  Bytes fm = {'F', 'I', 'N', 'S', 201, 0, 14, 0, 'F', 'M', 36, 0, 244, 18, 0, 16};
  for (int n = 0; n < 4; ++n)
    fm.insert(fm.end(), {uint8_t(n + 1), uint8_t(n), 15, 3, 0, 0xf2, 0, 0});
  REQUIRE(importExternalPresets(InstrumentType::OPL3, "voice.fui", fm, out, error));
  CHECK(out[0].instrument.chip.opl.operators[1].multiplier == 3);
  CHECK(out[0].instrument.chip.opl.operators[2].multiplier == 2);
  CHECK_FALSE(importExternalPresets(InstrumentType::OPL2, "voice.fui", fm, out, error));
  fm[6] = 1;
  fm[12] = 0x54;
  REQUIRE(importExternalPresets(InstrumentType::GenesisFM, "voice.fui", fm, out, error));
  CHECK(out[0].instrument.chip.fourOp.operatorMask == 5);
  roundtrip(out[0]);
}
TEST_CASE("External WOPL hides empty slots but retains unnamed sounds and "
          "stable source numbers") {
  for (unsigned version : {1, 2, 3}) {
    CAPTURE(version);
    Bytes bank = {
        'W', 'O', 'P', 'L', '3', '-', 'B', 'A', 'N', 'K', 0, uint8_t(version),
        0,   0,   1,   0,   1,   0,   0};
    if (version >= 2) {
      bank.resize(19 + 68);
      bank[19 + 32] = 5;
      bank[19 + 33] = 2;
    }
    size_t start = bank.size(), size = version == 3 ? 66 : 62;
    bank.resize(start + 256 * size);
    for (unsigned n = 0; n < 256; ++n)
      bank[start + n * size + 39] = 4;
    auto slot = [&](unsigned n, bool silent) {
      auto offset = start + n * size;
      bank[offset + 39] = 0;
      for (unsigned op = 0; op < 4; ++op) {
        bank[offset + 42 + op * 5 + 1] = silent ? 63 : 0;
        bank[offset + 42 + op * 5 + 2] = silent ? 0 : 0xf0;
      }
      return offset;
    };
    slot(0, true);  // Unflagged empty bank entry.
    slot(1, false); // Unnamed playable melodic program.
    auto named = slot(2, true);
    memcpy(bank.data() + named, "Named silence", 13);
    slot(128, false); // Unnamed percussion entry.
    std::vector<ExternalPreset> out;
    std::string error;
    REQUIRE(importExternalPresets(InstrumentType::OPL2, "bank.wopl", bank, out,
                                  error));
    REQUIRE(out.size() == 3);
    CHECK(out[0].sourceIndex == 1);
    CHECK(out[0].name ==
          (version >= 2 ? "Program 1 [B261]" : "Program 1 [B0]"));
    CHECK(out[1].sourceIndex == 2);
    CHECK(out[1].name == "Named silence");
    CHECK(out[2].sourceIndex == 128);
    CHECK(out[2].name == "Drum 0 [B0]");
    roundtrip(out[0]);
    roundtrip(out[2]);
    // Single instruments with no internal name use their filename.
    Bytes single = {'W', 'O', 'P', 'L', '3', '-', 'I',
                    'N', 'S', 'T', 0,   2,   0,   0};
    single.insert(single.end(), bank.begin() + start + size,
                  bank.begin() + start + size + 62);
    REQUIRE(importExternalPresets(InstrumentType::OPL2, "Unnamed bell.opli",
                                  single, out, error));
    CHECK(out[0].name == "Unnamed bell");
  }
}
TEST_CASE("Imported macro delays and malformed saved programs are bounded") {
  auto bytes = fuiPSG();
  bytes[29] = 2;
  std::vector<ExternalPreset> out;
  std::string error;
  REQUIRE(importExternalPresets(InstrumentType::SegaPSG, "voice.fui", bytes, out, error));
  auto program = out[0].instrument.chip.simpleChip.program;
  ChipMacroPlayer player;
  player.reset(program);
  CHECK_FALSE(player.has(0));
  CHECK(player.value(0, 15) == 15);
  player.tick(program);
  CHECK_FALSE(player.has(0));
  player.tick(program);
  CHECK(player.has(0));
  for (unsigned n = 0; n < program.size; ++n) {
    auto truncated = program;
    truncated.size = n;
    CHECK_FALSE(validChipProgram(truncated));
  }
  program.data[0] = 16;
  CHECK_FALSE(validChipProgram(program));
  program = out[0].instrument.chip.simpleChip.program;
  memset(program.data + 8, 0x7f, 4);
  CHECK_FALSE(validChipProgram(program));
}
TEST_CASE("GoatTracker source tables render and roundtrip without source files") {
  Bytes bytes(29);
  memcpy(bytes.data(), "GTI5", 4);
  bytes[4] = 0x09;
  bytes[5] = 0xf8;
  bytes[6] = bytes[7] = 1;
  bytes[12] = 0x41;
  memcpy(bytes.data() + 13, "Owned SID", 9);
  bytes.insert(bytes.end(), {2, 0x41, 255, 0, 1, 2, 0x88, 255, 0, 1, 0, 0});
  std::vector<ExternalPreset> out;
  std::string error;
  INFO(error);
  REQUIRE(importExternalPresets(InstrumentType::SID, "voice.ins", bytes, out, error));
  roundtrip(out[0]);
  SIDVoice a, b;
  a.init(48000);
  b.init(48000);
  a.configure(&out[0].instrument.chip.sid, 6000, .25f);
  b.configure(&out[0].instrument.chip.sid, 6000, .25f);
  a.noteOn();
  b.noteOn();
  std::vector<float> x(4800), y(4800);
  a.render(x.data(), x.size());
  for (int i = 0; i < 4800; i += 100) b.render(y.data() + i, 100);
  CHECK(x == y);
  double energy = 0;
  for (float v : x) {
    CHECK(std::isfinite(v));
    energy += v * v;
  }
  CHECK(energy > 1e-8);
  bytes[9] = 1;
  CHECK_FALSE(importExternalPresets(InstrumentType::SID, "voice.ins", bytes, out, error));
}

TEST_CASE("Furnace Game Boy software envelopes use the native pulse and noise voices") {
  auto bytes = fuiPSG();
  bytes[6] = 2;
  bytes.insert(bytes.end(), {'G', 'B', 4, 0, 15, 64, 3, 0});
  for (auto type : {InstrumentType::GBPulse, InstrumentType::GBNoise}) {
    std::vector<ExternalPreset> out;
    std::string error;
    REQUIRE(importExternalPresets(type, "voice.fui", bytes, out, error));
    roundtrip(out[0]);
    auto& patch = out[0].instrument.chip.simpleChip;
    SimpleChipVoice a, b;
    a.init(48000);
    b.init(48000);
    a.configure(type, &patch, 6000, .25f);
    b.configure(type, &patch, 6000, .25f);
    a.noteOn();
    b.noteOn();
    std::vector<float> x(4800), y(4800);
    a.render(x.data(), x.size());
    for (int n = 0; n < 4800; n += 100) b.render(y.data() + n, 100);
    CHECK(x == y);
    double energy = 0;
    for (float v : x) {
      CHECK(std::isfinite(v));
      energy += v * v;
    }
    CHECK(energy > 1e-8);
    a.noteOff();
    a.render(x.data(), x.size());
    CHECK_FALSE(a.active());
  }
}
TEST_CASE("Furnace fixed notes translate source chip octaves before playback") {
  Bytes b = {'F', 'I', 'N', 'S', 201, 0,   0, 0, 'M', 'A', 15, 0,  8,  0,
             1,   1,   255, 255, 0,   192, 0, 1, 12,  0,   0,  64, 255};
  std::vector<ExternalPreset> out;
  std::string error;
  REQUIRE(importExternalPresets(InstrumentType::SegaPSG, "arp.fui", b, out, error));
  ChipMacroPlayer player;
  player.reset(out[0].instrument.chip.simpleChip.program);
  CHECK(player.value(1) == (48 | 0x40000000));
  b[6] = 2;
  b.insert(b.end(), {'G', 'B', 4, 0, 15, 64, 2, 0});
  REQUIRE(importExternalPresets(InstrumentType::GBNoise, "arp.fui", b, out, error));
  player.reset(out[0].instrument.chip.simpleChip.program);
  CHECK(player.value(1) == (96 | 0x40000000));
}

TEST_CASE("External ready ZIP libraries browse and load through USER") {
  const char* configured = std::getenv("CHOOCHOO_USER_PRESETS");
  if (!configured) return;
  const auto root = std::filesystem::path(configured);
  REQUIRE(std::filesystem::is_directory(root));
  for (auto type : {InstrumentType::DX7, InstrumentType::OPLL, InstrumentType::VRC7,
                    InstrumentType::OPL2, InstrumentType::OPL3, InstrumentType::GenesisFM,
                    InstrumentType::ArcadeFM, InstrumentType::SID, InstrumentType::SegaPSG,
                    InstrumentType::GBPulse, InstrumentType::GBNoise}) {
    CAPTURE(userPresetFolder(type));
    UserPresets browser;
    browser.setup((root / userPresetFolder(type)).string(), type);
    std::string error;
    auto presets = browser.scan(error);
    REQUIRE(!presets.empty());
    auto project = std::make_unique<Project>();
    projectInit(project.get());
    for (auto index : {size_t(0), presets.size() - 1}) {
      REQUIRE(browser.load(presets[index], project.get(), 3, error));
      CHECK(project->instruments[3].type == type);
      REQUIRE(browser.focus(presets[index], error));
      ExternalPreset loaded;
      loaded.instrument = project->instruments[3];
      loaded.table = project->tables[3];
      roundtrip(loaded);
    }
    projectFree(project.get());
  }
}
