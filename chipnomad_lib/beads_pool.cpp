#include "beads_pool.h"
#include <cassert>
#include <new>

BeadsPool::~BeadsPool() {
  for (auto& entry : entries_) {
    assert(entry.references.load(std::memory_order_relaxed) == 0);
    delete entry.dsp;
  }
}

BeadsPool::Handle BeadsPool::prepare(int slot, float sampleRate) {
  if (slot < 0 || slot >= capacity) return {};
  auto& entry = entries_[slot];
  if (!entry.dsp) {
    auto* dsp = new (std::nothrow) BeadsInsert(sampleRate);
    if (!dsp || !dsp->ready()) {
      delete dsp;
      return {};
    }
    entry.dsp = dsp;
  }
  return Handle(&entry);
}

void BeadsPool::collect() {
  for (auto& entry : entries_) {
    if (entry.references.load(std::memory_order_acquire) == 0) {
      delete entry.dsp;
      entry.dsp = nullptr;
    }
  }
}

int BeadsPool::allocated() const {
  int count = 0;
  for (const auto& entry : entries_) count += entry.dsp != nullptr;
  return count;
}
