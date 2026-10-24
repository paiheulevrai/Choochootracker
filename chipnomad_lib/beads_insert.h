#pragma once
#include <cstdint>

// Storage and rate-conversion coefficients are prepared by the producer when
// Beads is selected, before publishing the project; process/reset never allocate.
class BeadsInsert {
 public:
  explicit BeadsInsert(float sampleRate);
  ~BeadsInsert();
  BeadsInsert(const BeadsInsert&) = delete;
  BeadsInsert& operator=(const BeadsInsert&) = delete;
  bool ready() const { return impl_ != nullptr; }
  void reset();
  void process(float* left, float* right, int frames, const float* parameters);
 private:
  struct Impl;
  Impl* impl_;
};
