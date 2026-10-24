#pragma once
#include "beads_insert.h"
#include "project_constants.h"
#include <atomic>
static_assert(std::atomic<unsigned>::is_always_lock_free, "Beads leases require lock-free reference counts");

// The producer owns storage; snapshot/chain handles only retain it. Releasing
// the last handle on the audio thread never frees the DSP or its buffers.
class BeadsPool {
  struct Entry {
    std::atomic<unsigned> references{0};
    BeadsInsert* dsp = nullptr;
  };
 public:
  static constexpr int capacity = (PROJECT_MAX_TRACKS + 1) * 2;
  class Handle {
   public:
    Handle() = default;
    Handle(const Handle& other) : entry_(other.entry_) { retain(); }
    Handle& operator=(const Handle& other) {
      if (entry_ != other.entry_) {
        release();
        entry_ = other.entry_;
        retain();
      }
      return *this;
    }
    ~Handle() { release(); }
    BeadsInsert* get() const { return entry_ ? entry_->dsp : nullptr; }
    BeadsInsert* operator->() const { return get(); }
    explicit operator bool() const { return get() != nullptr; }
   private:
    friend class BeadsPool;
    explicit Handle(Entry* entry) : entry_(entry) { retain(); }
    void retain() { if (entry_) entry_->references.fetch_add(1, std::memory_order_relaxed); }
    void release() { if (entry_) entry_->references.fetch_sub(1, std::memory_order_release); }
    Entry* entry_ = nullptr;
  };

  BeadsPool() = default;
  ~BeadsPool(); // All handles must have been released; audio must be stopped.
  BeadsPool(const BeadsPool&) = delete;
  BeadsPool& operator=(const BeadsPool&) = delete;
  // Producer thread only. Each logical slot has its own entry, so coalesced
  // snapshots can share its lifetime without sharing audio between slots.
  Handle prepare(int slot, float sampleRate);
  void collect();
  int allocated() const; // Producer-thread diagnostics.
 private:
  Entry entries_[capacity];
};
