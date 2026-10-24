#ifndef CHOOCHOO_MASTER_EFFECTS_H
#define CHOOCHOO_MASTER_EFFECTS_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "clouds/dsp/frame.h"
#include "clouds/dsp/fx/reverb.h"
#include "beads/dsp/frame.h"
#include "beads/dsp/fx/reverb.h"

struct Project;

class MasterEffects {
 public:
  void init(float sampleRate);
  void reset();
  bool tailActive() const;
  // Whether each effect is still producing an audible tail. Its bus must
  // keep being processed (with silence) until the tail has died away.
  bool reverbTailActive() const;
  bool delayTailActive() const;
  void process(float* reverbBus, float* delayBus, float* output,
               size_t frames, const Project* project);

 private:
  float lowpass(float input, float coefficient, float& state);
  float lowpassCoefficient(float cutoff) const;
  float highpass(float input, float coefficient, float& inputState, float& outputState);
  float highpassCoefficient(float cutoff) const;
  float delayRead(float delaySamples, int channel) const;
  void resetDelay();

  float sampleRate_;
  uint16_t reverbMemory_[65536];
  uint16_t beadsReverbMemory_[32768];
  clouds::Reverb reverb_;
  beads::Reverb beadsReverb_;
  std::vector<clouds::FloatFrame> reverbFrames_;
  std::vector<beads::FloatFrame> beadsReverbFrames_;
  std::vector<float> delayLine_;
  std::vector<float> springLine_;
  size_t delayWrite_;
  size_t springWrite_;
  float reverbFilterState_[2];
  float reverbHighpassInputState_[2];
  float reverbHighpassOutputState_[2];
  float beadsReverbHighpassInputState_[2];
  float beadsReverbHighpassOutputState_[2];
  float delayFilterState_[2];
  float tapeFilterState_[2];
  float springFilterState_[2];
  float flutterPhase_;
  size_t reverbTailFrames_;
  size_t delayTailFrames_;
  uint8_t activeReverbAlgorithm_;
  uint8_t activeDelayAlgorithm_;
};

#endif
