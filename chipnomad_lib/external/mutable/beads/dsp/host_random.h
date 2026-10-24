#pragma once
#include <cstdint>
namespace beads {
// The firmware has a global RNG. A scoped, thread-local state preserves its
// algorithm while making host instances independent of each other and Plaits.
class HostRandom {
 public:
  inline static thread_local uint32_t state = 1;
  static float GetFloat() {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state) / 4294967296.0f;
  }
  struct Scope {
    uint32_t& instance;
    uint32_t previous;
    explicit Scope(uint32_t& s) : instance(s), previous(state) { state = s; }
    ~Scope() { instance = state; state = previous; }
  };
};
}
