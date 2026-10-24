#include "doctest.h"
#include "external/ymfm/ymfm_opl.h"
#include <cstring>

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
