#ifndef MOBILE_GROOVE_BRAIDS_VOICE_H
#define MOBILE_GROOVE_BRAIDS_VOICE_H

#include <stddef.h>
#include <stdint.h>

#include "braids/macro_oscillator.h"

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
  void updateFilterCoefficients();
  float processFilterStage(float input, int stage);
  float processEnvelope();
  void enterEnvelopeStage(EnvelopeStage stage);

  braids::MacroOscillator oscillator_;
  int16_t block_[kBlockSize];
  uint8_t sync_[kBlockSize];
  size_t blockPosition_;
  uint8_t model_;
  bool active_;
  float gain_;

  bool filterEnabled_;
  BraidsFilterMode filterMode_;
  bool filterSlope24dB_;
  float filterCutoffHz_;
  float filterResonance_;
  float filterG_;
  float filterK_;
  float filterA1_;
  float filterA2_;
  float filterA3_;
  float filterIc1eq_[2];
  float filterIc2eq_[2];

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
