#ifndef MOBILE_GROOVE_BRAIDS_VOICE_H
#define MOBILE_GROOVE_BRAIDS_VOICE_H

#include <stddef.h>
#include <stdint.h>

#include "braids/macro_oscillator.h"
#include "braids/signature_waveshaper.h"
#include "braids/vco_jitter_source.h"
#include "multimode_filter.h"

enum class BraidsFilterMode : uint8_t {
  lowPass,
  bandPass,
  highPass,
};

class BraidsVoice {
 public:
  static const size_t kBlockSize = 24;
  static constexpr float kSampleRate = 96000.0f;

  void init();
  bool setModel(uint8_t model);
  void setPitch(int16_t pitch);
  void setParameters(uint16_t timbre, uint16_t color);
  void setGain(float gain);
  void setGlobalSettings(uint8_t bits, uint8_t drift, uint8_t signature,
                         uint32_t signatureSeed);
  void setFilter(bool enabled, BraidsFilterMode mode, bool slope24dB,
                 float cutoffHz, float resonance);
  void setEnvelope(bool enabled, float attackSeconds, float decaySeconds,
                   float sustain, float releaseSeconds);
  void noteOn();
  void noteOff();
  void kill();
  void strike();
  void render(float* output, size_t samples);

  uint8_t model() const { return model_; }
  bool active() const { return active_; }

 private:
  enum class EnvelopeStage : uint8_t { idle, attack, decay, sustain, release };

  void renderBlock();
  float processEnvelope();
  void enterEnvelopeStage(EnvelopeStage stage);

  braids::MacroOscillator oscillator_;
  int16_t block_[kBlockSize];
  uint8_t sync_[kBlockSize];
  size_t blockPosition_;
  uint8_t model_;
  bool active_;
  float gain_;
  int16_t basePitch_;
  uint8_t bits_, drift_, signature_;
  uint32_t signatureSeed_;
  braids::SignatureWaveshaper signatureWaveshaper_;
  braids::VcoJitterSource jitterSource_;

  MultimodeFilter filter_;

  bool envelopeEnabled_;
  EnvelopeStage envelopeStage_;
  float envelopeLevel_;
  float envelopeAttackSeconds_;
  float envelopeDecaySeconds_;
  float envelopeSustain_;
  float envelopeReleaseSeconds_;
  float envelopeIncrement_;
  uint32_t envelopeSamplesLeft_;
};

#endif
