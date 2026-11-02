#include "doctest.h"
#include "external/ymfm/ymfm_opl.h"
#include "external/ymfm/ymfm_opn.h"
#include "external/ymfm/ymfm_opm.h"
#include "opll_presets.h"
#include <cstring>
#include <type_traits>

namespace {
template<class Chip, class Events>
void compareChannelZero(Chip& full, Chip& selected, Events events, int idle = 0) {
  typename Chip::output_data reference[127], actual[127];
  bool audible = false;
  for (int block = 0; block < 256; ++block) {
    events(block);
    full.generate(reference, 127);
    selected.generate(actual, 127, 1u);
    CAPTURE(block);
    REQUIRE(std::memcmp(reference, actual, sizeof(reference)) == 0);
    for (const auto& sample : actual)
      for (auto value : sample.data) audible |= value != idle;
  }
  CHECK(audible);
}

template<class Chip> void compareFourOp(int algorithm, int variant) {
  constexpr bool genesis = std::is_same<Chip, ymfm::ym2612>::value;
  ymfm::ymfm_interface fullInterface, selectedInterface;
  Chip full(fullInterface), selected(selectedInterface);
  full.reset(); selected.reset();
  auto write = [&](unsigned reg, unsigned value) {
    for (Chip* chip : {&full, &selected}) {
      chip->write_address(reg);
      chip->write_data(value);
    }
  };
  const int pan = variant % 3 + 1;
  if (genesis) {
    write(0x22, 8 | variant);
    write(0x27, 0); write(0x2b, 0);
    write(0xb0, (variant << 3) | algorithm);
    write(0xb4, ((pan & 1) ? 128 : 0) | ((pan & 2) ? 64 : 0) |
      ((variant & 3) << 4) | variant);
    write(0xa4, 0x24); write(0xa0, 0x3b);
  } else {
    write(0x0f, 0); write(0x18, 180 + variant * 10);
    write(0x19, 96); write(0x19, 128 | 96);
    write(0x1b, variant & 3);
    write(0x20, (pan << 6) | (variant << 3) | algorithm);
    write(0x38, (variant << 4) | (variant & 3));
    write(0x28, 0x4a); write(0x30, 0);
  }
  const int order[] = {0, 2, 1, 3};
  for (int op = 0; op < 4; ++op) {
    const int a = order[op] * (genesis ? 4 : 8);
    write((genesis ? 0x30 : 0x40) + a, (((variant + op) & 7) << 4) | (op + 1));
    write((genesis ? 0x40 : 0x60) + a, op == 3 ? 0 : 20);
    write((genesis ? 0x50 : 0x80) + a, ((variant & 3) << 6) | 31);
    write((genesis ? 0x60 : 0xa0) + a, 0x80 | (8 + variant));
    write((genesis ? 0x70 : 0xc0) + a, (genesis ? 0 : ((variant & 3) << 6)) | 4);
    write((genesis ? 0x80 : 0xe0) + a, 0x4f);
    if (genesis) write(0x90 + a, (variant + op * 3) & 15);
  }
  compareChannelZero(full, selected, [&](int block) {
    if (block == 1 || block == 128)
      write(genesis ? 0x28 : 0x08, (variant & 1 ? 15 : 8) << (genesis ? 4 : 3));
    if (block == 32) write(genesis ? 0xa0 : 0x30, 0x80);
    if (block == 64) write(genesis ? 0x40 : 0x60, 7);
    if (block == 96 || block == 200) write(genesis ? 0x28 : 0x08, 0);
  }, genesis ? 504 : 0);
}
}

TEST_CASE("Genesis and Arcade selected channels preserve full-chip PCM") {
  for (int algorithm = 0; algorithm < 8; ++algorithm)
    for (int variant = 0; variant < 8; ++variant) {
      CAPTURE(algorithm); CAPTURE(variant);
      compareFourOp<ymfm::ym2612>(algorithm, variant);
      compareFourOp<ymfm::ym2151>(algorithm, variant);
    }
}

TEST_CASE("Genesis selected channel retains the complete DAC idle baseline") {
  ymfm::ymfm_interface io;
  ymfm::ym2612 chip(io);
  chip.reset();
  ymfm::ym2612::output_data output[127];
  chip.generate(output, 127, 1u);
  for (const auto& sample : output) {
    CHECK(sample.data[0] == 504);
    CHECK(sample.data[1] == 504);
  }
}

TEST_CASE("All OPLL and VRC7 preset tones match full-chip PCM on channel zero") {
  for (auto type : {InstrumentType::OPLL, InstrumentType::VRC7})
    for (int program = 1; program <= 15; ++program) {
      CAPTURE(int(type)); CAPTURE(program);
      Instrument instrument{};
      instrument.type = type;
      REQUIRE(opllApplyPreset(&instrument, program));
      ymfm::ymfm_interface fullInterface, selectedInterface;
      ymfm::ym2413 full(fullInterface), selected(selectedInterface);
      full.reset(); selected.reset();
      auto write = [&](int reg, int value) {
        for (auto* chip : {&full, &selected}) {
          chip->write_address(reg); chip->write_data(value);
        }
      };
      for (int reg = 0; reg < 8; ++reg) write(reg, instrument.chip.opll.patch[reg]);
      write(0x30, 0); write(0x10, 0x40); write(0x20, 0x09);
      compareChannelZero(full, selected, [&](int block) {
        if (block == 1 || block == 128) write(0x20, 0x19);
        if (block == 32) write(0x10, 0x80);
        if (block == 64) write(2, instrument.chip.opll.patch[2] ^ 7);
        if (block == 96 || block == 200) write(0x20, 0x09);
      });
    }
}

namespace {
template<class Chip> void compareOPLChannels(int topology, int variant) {
  ymfm::ymfm_interface fullInterface, voiceInterface;
  Chip full(fullInterface), voice(voiceInterface);
  full.reset();
  voice.reset();
  auto write = [&](unsigned reg, unsigned value) {
    for (Chip* chip : {&full, &voice}) {
      if constexpr (Chip::OUTPUTS == 4) {
        if (reg & 0x100) chip->write_address_hi(reg);
        else chip->write_address(reg);
      } else chip->write_address(reg);
      chip->write_data(value);
    }
  };
  write(1, 0x20);
  if constexpr (Chip::OUTPUTS == 4) {
    write(0x105, 1);
    write(0x104, topology == 1 ? 1 : 0);
  }
  // The tracker uses melodic channels even for fixed-pitch drum patches.
  write(0xbd, (variant & 3) << 6);
  const int addresses[] = {0, 3, 8, 11};
  const int operators = topology == 0 ? 2 : 4;
  for (int op = 0; op < operators; ++op) {
    const int a = addresses[op];
    write(0x20 + a, 0xf0 | ((variant + op) & 15));
    write(0x40 + a, ((variant & 3) << 6) | (op & 1 ? 0 : 19));
    write(0x60 + a, 0xf3 + (variant & 7));
    write(0x80 + a, 0x45 + (variant & 7));
    write(0xe0 + a, (variant + op) % (Chip::OUTPUTS == 4 ? 8 : 4));
  }
  write(0xc0, 0x10 | ((variant & 7) << 1) | (variant & 1));
  write(0xc3, 0x20 | (((variant + 3) & 7) << 1) | ((variant >> 1) & 1));
  auto key = [&](bool on) {
    write(0xb0, 0x11 | (on ? 0x20 : 0));
    if (topology != 0) write(0xb3, 0x12 | (on ? 0x20 : 0));
  };
  write(0xa0, 0x98);
  write(0xa3, 0x43);
  key(true);
  const unsigned mask = topology == 0 ? 1 : 9;
  typename Chip::output_data reference[127], actual[127];
  bool audible = false;
  for (int block = 0; block < 256; ++block) {
    if (block == 32) write(0xa0, 0xd3); // Live pitch change.
    if (block == 64) write(0x40, 7); // Live operator-level change.
    if (block == 96 || block == 200) key(false);
    if (block == 128) key(true); // Retrigger after a release tail.
    full.generate(reference, 127);
    voice.generate(actual, 127, mask);
    CAPTURE(block);
    REQUIRE(std::memcmp(reference, actual, sizeof(reference)) == 0);
    for (const auto& sample : actual)
      for (auto value : sample.data) audible |= value != 0;
  }
  CHECK(audible);
}
}

TEST_CASE("OPL selected channels match full-chip PCM through modulation and retrigger") {
  for (int variant = 0; variant < 16; ++variant) {
    CAPTURE(variant);
    compareOPLChannels<ymfm::ym3812>(0, variant);
    for (int topology = 0; topology < 3; ++topology) {
      CAPTURE(topology);
      compareOPLChannels<ymfm::ymf262>(topology, variant);
    }
  }
}
