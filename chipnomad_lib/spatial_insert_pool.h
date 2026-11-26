#pragma once

#include "project_constants.h"
#include <atomic>

#include <stdint.h>

class SpatialInsert {
 public:
  virtual ~SpatialInsert() = default;
  virtual void reset() = 0;
  virtual void process(float* left, float* right, int frames, const float* parameters,
                       float tickRate) = 0;
};
static constexpr int spatialInsertModuleCount = 5;

class SpatialInsertPool {
  struct Entry {
    std::atomic<unsigned> references{0};
    SpatialInsert* dsp = nullptr;
  };
 public:
  static constexpr int slots = (PROJECT_MAX_TRACKS + 1) * 2;
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
    SpatialInsert* get() const { return entry_ ? entry_->dsp : nullptr; }
    SpatialInsert* operator->() const { return get(); }
    explicit operator bool() const { return get() != nullptr; }
   private:
    friend class SpatialInsertPool;
    explicit Handle(Entry* entry) : entry_(entry) { retain(); }
    void retain() { if (entry_) entry_->references.fetch_add(1, std::memory_order_relaxed); }
    void release() { if (entry_) entry_->references.fetch_sub(1, std::memory_order_release); }
    Entry* entry_ = nullptr;
  };

  SpatialInsertPool() = default;
  ~SpatialInsertPool();
  SpatialInsertPool(const SpatialInsertPool&) = delete;
  SpatialInsertPool& operator=(const SpatialInsertPool&) = delete;
  Handle prepare(int slot, int module, float sampleRate);
  void collect();
  int allocated() const;
 private:
  Entry entries_[slots * spatialInsertModuleCount];
};
