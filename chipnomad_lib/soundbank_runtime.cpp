#include "soundbank_runtime.h"

#include <array>
#include <stdexcept>
namespace soundbank {
namespace {
struct Slot {
  uint32_t generation = 0;
  std::shared_ptr<const PreparedPreset> preset;
};
std::array<Slot, 1024> slots;
}  // namespace
uint32_t publish(std::shared_ptr<const PreparedPreset> p) {
  for (size_t i = 0; i < slots.size(); ++i) {
    auto& s = slots[i];
    if (!s.preset) {
      s.generation = (s.generation + 1) & 0x3fffff;
      if (!s.generation) ++s.generation;
      s.preset = std::move(p);
      return (s.generation << 10) | uint32_t(i);
    }
  }
  throw std::runtime_error("Sound bank preset slot limit reached");
}
const PreparedPreset* prepared(uint32_t h) {
  if (!h) return nullptr;
  auto& s = slots[h & 1023];
  return s.generation == (h >> 10) ? s.preset.get() : nullptr;
}
void collect(const std::function<bool(uint32_t)>& used) {
  for (size_t i = 0; i < slots.size(); ++i) {
    auto& s = slots[i];
    if (s.preset && !used((s.generation << 10) | uint32_t(i))) s.preset.reset();
  }
}
}  // namespace soundbank
