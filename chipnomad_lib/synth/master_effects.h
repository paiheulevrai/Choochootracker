#ifndef CHOOCHOO_MASTER_EFFECTS_H
#define CHOOCHOO_MASTER_EFFECTS_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "clouds/dsp/frame.h"
#include "clouds/dsp/fx/reverb.h"

struct Project;

class MasterEffects {
 public:
  void init(float sampleRate);
  void process(float* reverbBus, float* delayBus, float* output,
               size_t frames, const Project* project);

 private:
  float lowpass(float input, float coefficient, float& state);
  float lowpassCoefficient(float cutoff) const;

  float sampleRate_;
  uint16_t reverbMemory_[65536];
  clouds::Reverb reverb_;
  std::vector<clouds::FloatFrame> reverbFrames_;
  std::vector<float> delayLine_;
  size_t delayWrite_;
  float reverbFilterState_[2];
  float delayFilterState_[2];
};

#endif
