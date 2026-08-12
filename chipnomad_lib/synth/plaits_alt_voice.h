#ifndef CHOOCHOO_PLAITS_ALT_VOICE_H
#define CHOOCHOO_PLAITS_ALT_VOICE_H

#include <stddef.h>
#include <stdint.h>

#include "plaits_alt/dsp/voice.h"
#include "multimode_filter.h"

class PlaitsAltVoice {
 public:
  static const size_t kBlockSize = 24;
  static constexpr float kSourceSampleRate = 48000.0f;

  void init(float outputSampleRate = 96000.0f);
  void configure(uint8_t engine, uint16_t harmonics, uint16_t timbre,
                 uint16_t morph, uint8_t auxMix, uint8_t envelopeMode,
                 uint8_t decay, uint8_t sustain, float note, float gain);
  void setFilter(bool enabled, uint8_t mode, bool slope24dB,
                 float cutoffHz, float resonance);
  void setEnvelope(float attackSeconds, float decaySeconds, float sustain,
                   float releaseSeconds);
  void noteOn();
  void noteOff();
  void kill();
  void render(float* output, size_t samples);

  bool active() const { return active_; }

 private:
  enum class EnvelopeStage : uint8_t { idle, attack, decay, sustain, release };

  void renderBlock();
  float nextSourceSample();
  float processEnvelope();
  void enterEnvelopeStage(EnvelopeStage stage);

  plaits_alt::Voice voice_;
  char allocatorMemory_[16384];
  plaits_alt::Voice::Frame frames_[kBlockSize];
  plaits_alt::Patch patch_;
  plaits_alt::Modulations modulations_;
  size_t blockPosition_;
  float outputSampleRate_;
  float sourcePhase_;
  float previousSource_;
  float currentSource_;
  uint8_t auxMix_;
  bool active_;
  bool gate_;
  bool triggerPending_;
  uint8_t envelopeMode_;
  float gain_;
  uint32_t quietSamples_;

  MultimodeFilter filter_;

  EnvelopeStage envelopeStage_;
  float envelopeLevel_;
  float attackSeconds_, decaySeconds_, sustain_, releaseSeconds_;
  float envelopeIncrement_;
  uint32_t envelopeSamplesLeft_;
};

#endif
