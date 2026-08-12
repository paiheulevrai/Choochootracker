// Copyright 2026 Lyle Mills.
// SPDX-License-Identifier: MIT
//
// Dynamic stochastic (GENDY-inspired) synthesis engine.
//
// OUT: the breakpoint wave read by phase_/segment_ (stepped->linear->smooth by
// MACRO). AUX: a stepped sample-and-hold of the breakpoint. In stereo mode,
// OUT/AUX become L/R: the breakpoint set mutates and phase_/segment_ advance
// once (shared), and the R channel reads the same breakpoints at the antipodal
// phase (half a cycle away) with an independent segment lookup, giving a
// decorrelated wave from the same breakpoint set; the stepped AUX is dropped.

#ifndef PLAITS_ALT_GUARD_PLAITS_DSP_ENGINE2_GENDY_ENGINE_H_
#define PLAITS_ALT_GUARD_PLAITS_DSP_ENGINE2_GENDY_ENGINE_H_

#include "plaits_alt/dsp/engine/engine.h"

namespace plaits_alt {

const int kMaxGendyBreakpoints = 12;

class GendyEngine : public Engine {
 public:
  GendyEngine() { }
  ~GendyEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator);
  virtual void Reset();
  virtual void LoadUserData(const uint8_t* user_data) { }
  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);
  virtual bool stereo_capable() const { return PLAITS_STEREO_GENDY; }
  virtual bool fast_fm_capable() const { return true; }

 private:
  void Randomize(int num_breakpoints);
  void Mutate(float amplitude_step, float duration_step);
  void UpdateBoundaries();
  float Walk(float value, float amount, float minimum, float maximum);

  float amplitude_[kMaxGendyBreakpoints];
  float duration_[kMaxGendyBreakpoints];
  float boundary_[kMaxGendyBreakpoints];
  float phase_;
  int segment_;
  int num_breakpoints_;

  DISALLOW_COPY_AND_ASSIGN(GendyEngine);
};

}  // namespace plaits_alt

#endif  // PLAITS_DSP_ENGINE2_GENDY_ENGINE_H_
