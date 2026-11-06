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
  void setFilter(bool enabled, uint8_t character, uint8_t mode, bool slope24dB,
                 float cutoffHz, float resonance, uint8_t color = 128) {
    if (resonance < 0.0f) resonance = 0.0f;
    if (resonance > 1.0f) resonance = 1.0f;
    filter_.configure(enabled, character, mode, slope24dB, cutoffHz, resonance, color);
  }
  void setFilter(bool enabled, uint8_t mode, bool slope24dB,
                 float cutoffHz, float resonance) {
    setFilter(enabled, 1, mode, slope24dB, cutoffHz, resonance);
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
  // Renderers can select these paths once per block. A full-level sustain
  // cannot change until the next note/configuration event, between blocks.
  bool filterActive() const { return filter_.active(); }
  bool unityEnvelope() const {
    return !envelopeEnabled_ ||
      (envelope_.stage() == EnvelopeType::Stage::sustain && envelope_.level() == 1.0f);
  }
  template <bool Filter = true, bool UnityEnvelope = false>
  void processStereo(float* stereo) {
    float envelope = UnityEnvelope ? 1.0f : (envelopeEnabled_ ? envelope_.next() : 1.0f);
    if constexpr (Filter) {
      stereo[0] = filter_.process(stereo[0], 0);
      stereo[1] = filter_.process(stereo[1], 1);
    }
    stereo[0] = stereo[0] * envelope * gain_;
    stereo[1] = stereo[1] * envelope * gain_;
  }
  bool envelopeActive() const { return !envelopeEnabled_ || envelope_.active(); }
  float envelopeLevel() const { return envelopeEnabled_ ? envelope_.level() : 1.0f; }
  // TRIG/one-shot voices need to detect "decay finished" (the sustain stage
  // traps at level 0 instead of going idle).
  auto envelopeStage() const { return envelope_.stage(); }

 private:
  MultimodeFilter filter_;
  EnvelopeType envelope_;
  float gain_ = 1.0f;
  bool envelopeEnabled_ = false;
};

#endif
