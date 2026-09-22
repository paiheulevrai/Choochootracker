#ifndef CHOOCHOO_SINTERED_VOICE_H
#define CHOOCHOO_SINTERED_VOICE_H

#include <stddef.h>
#include <stdint.h>
#include "voice_post_processor.h"
#include "../project_instruments.h"

class SinteredVoice {
 public:
  void init(float sampleRate);
  void configure(const InstrumentSintered* instrument, float pitchCents, float gain,
                 uint16_t cutoffHz, uint8_t resonance);
  void noteOn();
  void noteOff() {}
  void kill();
  void render(float* output, size_t frames);
  bool active() const { return active_; }
  float envelopeLevel() const { return active_ ? amplitude_ : 0.0f; }

 private:
  float osc(float frequency, int slot);
  float random();
  InstrumentSintered parameters_{};
  VoicePostProcessor<> post_;
  float sampleRate_ = 48000.0f, frequency_ = 440.0f, age_ = 0.0f, duration_ = 0.1f, amplitude_ = 0.0f;
  float phase_[3]{}, feedback_ = 0.0f, dc_ = 0.0f, noiseLow_ = 0.0f, comb_[64]{};
  uint32_t randomState_ = 0x53494e54u;
  int combIndex_ = 0;
  bool active_ = false;
};

#endif
