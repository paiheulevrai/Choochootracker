#ifndef CHOOCHOO_VOICE_POST_PROCESSOR_H
#define CHOOCHOO_VOICE_POST_PROCESSOR_H

#include "envelope.h"
#include "multimode_filter.h"

// Shared source -> filter -> VCA path. Sources stay responsible for generation.
template <typename EnvelopeType = Envelope>
class VoicePostProcessor {
 public:
  void init(float sampleRate) {
    filter_.init(sampleRate);
    envelope_.init(sampleRate);
  }
  void setGain(float gain) {
    gain_ = gain < 0.0f ? 0.0f : (gain > 1.0f ? 1.0f : gain);
  }
  void setFilter(bool enabled, uint8_t mode, bool slope24dB,
                 float cutoffHz, float resonance) {
    if (resonance < 0.0f) resonance = 0.0f;
    if (resonance > 1.0f) resonance = 1.0f;
    filter_.configure(enabled, mode, slope24dB, cutoffHz, resonance * resonance);
  }
  void setEnvelope(bool enabled, float attack, float decay, float sustain,
                   float release, uint8_t shape) {
    envelopeEnabled_ = enabled;
    envelope_.configure(attack, decay, sustain, release, shape);
  }
  void noteOn(bool resetFilter = false) {
    if (resetFilter) filter_.reset();
    if (envelopeEnabled_) envelope_.noteOn();
  }
  void noteOff() { if (envelopeEnabled_) envelope_.noteOff(); }
  void kill() { envelope_.kill(); }
  float process(float sample, int channel = 0) {
    float envelope = envelopeEnabled_ ? envelope_.next() : 1.0f;
    return filter_.process(sample, channel) * envelope * gain_;
  }
  bool envelopeActive() const { return !envelopeEnabled_ || envelope_.active(); }
  float envelopeLevel() const { return envelopeEnabled_ ? envelope_.level() : 1.0f; }

 private:
  MultimodeFilter filter_;
  EnvelopeType envelope_;
  float gain_ = 1.0f;
  bool envelopeEnabled_ = false;
};

#endif
