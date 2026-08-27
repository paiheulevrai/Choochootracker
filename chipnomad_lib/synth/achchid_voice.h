#ifndef MOBILE_GROOVE_ACHCHID_VOICE_H
#define MOBILE_GROOVE_ACHCHID_VOICE_H

#include <stdint.h>
#include "braids/macro_oscillator.h"
#include "rosic_Open303.h"

class AChChidVoice {
 public:
  void init(float sampleRate);
  void configure(uint8_t wave, int8_t fine, uint8_t model, uint16_t timbre, uint16_t color,
                 uint16_t cutoff, uint8_t resonance, uint8_t envMod, uint16_t decay,
                 uint8_t accent, float gain);
  void noteOn(int note, bool accent, bool slide, uint8_t slideValue);
  void noteOff();
  void kill();
  void render(float* output, int samples);
  bool active() const { return active_; }
  float envelopeLevel() const { return active_ ? 1.0f : 0.0f; }
 private:
  rosic::Open303 synth_;
  braids::MacroOscillator braids_;
  uint8_t wave_ = 1;
  int16_t braidsBlock_[24] = {};
  uint8_t braidsSync_[24] = {};
  int braidsPosition_ = 24;
  int16_t pitch_ = 60 << 7;
  int8_t fine_ = 0;
  float gain_ = 1.0f;
  bool active_ = false;
  void renderBraidsBlock();
};
#endif
