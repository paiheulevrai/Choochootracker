// Copyright 2012 Emilie Gillet.
// Copyright 2018 Tom Burns.
// Copyright 2026 Lyle Mills.
// SPDX-License-Identifier: MIT

#ifndef PLAITS_ALT_GUARD_PLAITS_DSP_ENGINE2_WAVETABLE_CHORD_ENGINE_H_
#define PLAITS_ALT_GUARD_PLAITS_DSP_ENGINE2_WAVETABLE_CHORD_ENGINE_H_

#include "plaits_alt/dsp/engine/engine.h"
#include "plaits_alt/dsp/engine2/scale_voices.h"

namespace plaits_alt {

class WavetableChordEngine : public Engine {
 public:
  WavetableChordEngine() { }
  ~WavetableChordEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator);
  virtual void Reset();
  virtual void LoadUserData(const uint8_t* user_data) { }
  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);
  virtual bool stereo_capable() const { return true; }

 private:
  ScaleVoiceBank voices_;

  DISALLOW_COPY_AND_ASSIGN(WavetableChordEngine);
};

}  // namespace plaits_alt

#endif  // PLAITS_DSP_ENGINE2_WAVETABLE_CHORD_ENGINE_H_
