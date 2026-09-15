#ifndef CHOOCHOO_DRUM_SYNTH_VOICE_H
#define CHOOCHOO_DRUM_SYNTH_VOICE_H

#include <stddef.h>
#include <stdint.h>
#include "voice_post_processor.h"
#include "../project_instruments.h"

class DrumSynthVoice {
 public:
  void init(float sampleRate);
  void configure(const InstrumentDrumSynth* instrument, float pitchCents, float gain,
                 uint16_t cutoffHz, uint8_t resonance);
  void noteOn();
  void noteOff();
  void kill();
  void render(float* output, size_t frames);
  bool active() const { return active_; }
  float envelopeLevel() const { return active_ ? envelope_ : 0.0f; }

 private:
  float random();
  float oscillator(float frequency, int shape, int slot);
  const InstrumentDrumSynth* instrument_;
  InstrumentDrumSynth parameters_;
  VoicePostProcessor<> post_;
  float sampleRate_, frequency_, age_, duration_, envelope_, phase_[8], noiseLow_;
  uint32_t randomState_;
  bool active_;
};

#endif
