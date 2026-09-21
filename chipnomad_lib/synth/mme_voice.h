#ifndef CHOOCHOO_MME_VOICE_H
#define CHOOCHOO_MME_VOICE_H

#include <stddef.h>
#include <stdint.h>
#include "voice_post_processor.h"
#include "../project_instruments.h"

// MME contains small, tracker-oriented adaptations of the MIT-licensed Warps
// ring, fold, XOR/comparator and filter-bank vocoder algorithms.  The full
// Eurorack firmware and hardware layer are intentionally not part of this
// voice; see external attribution for Emilie Gillet's original work.
class MMEVoice {
 public:
  void init(float sampleRate);
  void configure(const InstrumentMME* instrument, float pitchCents, float gain,
                 uint16_t cutoffHz, uint8_t resonance);
  void noteOn();
  void noteOff();
  void kill();
  void render(float* output, size_t frames);
  bool active() const { return active_; }
  float envelopeLevel() const { return active_ ? post_.envelopeLevel() : 0.0f; }

 private:
  float oscillator(float phase, int shape) const;
  float diode(float x) const;
  float shaper(float x) const;
  float vocode(float modulator, float carrier);
  InstrumentMME parameters_{};
  VoicePostProcessor<> post_;
  float sampleRate_ = 48000.0f, frequency_ = 440.0f;
  float phaseA_ = 0.0f, phaseB_ = 0.0f, feedback_ = 0.0f, feedbackDC_ = 0.0f;
  float vocodeModLo_[20]{}, vocodeModHi_[20]{}, vocodeCarLo_[20]{}, vocodeCarHi_[20]{}, vocodeEnv_[20]{};
  uint32_t noise_ = 0x4d4d4531u;
  bool active_ = false;
};

#endif
