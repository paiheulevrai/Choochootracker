#include "braids_voice.h"

#include <math.h>
#include <string.h>

void BraidsVoice::init() {
  oscillator_.Init();
  blockPosition_ = kBlockSize;
  model_ = 0;
  active_ = false;
  gain_ = 1.0f;
  oscillator_.set_shape(braids::MACRO_OSC_SHAPE_CSAW);
  oscillator_.set_pitch(60 << 7);
  oscillator_.set_parameters(16384, 16384);
  memset(sync_, 0, sizeof(sync_));

  filterEnabled_ = false;
  filterMode_ = BraidsFilterMode::lowPass;
  filterSlope24dB_ = false;
  filterCutoffHz_ = 20000.0f;
  filterResonance_ = 0.0f;
  memset(filterIc1eq_, 0, sizeof(filterIc1eq_));
  memset(filterIc2eq_, 0, sizeof(filterIc2eq_));
  updateFilterCoefficients();

  envelopeEnabled_ = false;
  envelopeStage_ = EnvelopeStage::idle;
  envelopeLevel_ = 0.0f;
  envelopeAttackSeconds_ = 0.0f;
  envelopeDecaySeconds_ = 0.0f;
  envelopeSustain_ = 1.0f;
  envelopeReleaseSeconds_ = 0.0f;
  envelopeIncrement_ = 0.0f;
  envelopeSamplesLeft_ = 0;
}

bool BraidsVoice::setModel(uint8_t model) {
  if (model > braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META) return false;
  model_ = model;
  oscillator_.set_shape(static_cast<braids::MacroOscillatorShape>(model));
  return true;
}

void BraidsVoice::setPitch(int16_t pitch) {
  oscillator_.set_pitch(pitch);
}

void BraidsVoice::setParameters(uint16_t timbre, uint16_t color) {
  if (timbre > 32767) timbre = 32767;
  if (color > 32767) color = 32767;
  oscillator_.set_parameters(timbre, color);
}

void BraidsVoice::setGain(float gain) {
  gain_ = gain < 0.0f ? 0.0f : (gain > 1.0f ? 1.0f : gain);
}

void BraidsVoice::setFilter(bool enabled, BraidsFilterMode mode,
                            bool slope24dB, float cutoffHz,
                            float resonance) {
  filterEnabled_ = enabled;
  filterMode_ = mode;
  filterSlope24dB_ = slope24dB;
  filterCutoffHz_ = cutoffHz;
  filterResonance_ = resonance;
  updateFilterCoefficients();
}

void BraidsVoice::updateFilterCoefficients() {
  if (filterCutoffHz_ < 20.0f) filterCutoffHz_ = 20.0f;
  if (filterCutoffHz_ > kSampleRate * 0.45f) {
    filterCutoffHz_ = kSampleRate * 0.45f;
  }
  if (filterResonance_ < 0.0f) filterResonance_ = 0.0f;
  if (filterResonance_ > 1.0f) filterResonance_ = 1.0f;

  filterG_ = tanf(3.14159265358979323846f * filterCutoffHz_ / kSampleRate);
  float q = 0.5f + filterResonance_ * 19.5f;
  filterK_ = 1.0f / q;
  filterA1_ = 1.0f / (1.0f + filterG_ * (filterG_ + filterK_));
  filterA2_ = filterG_ * filterA1_;
  filterA3_ = filterG_ * filterA2_;
}

float BraidsVoice::processFilterStage(float input, int stage) {
  float v3 = input - filterIc2eq_[stage];
  float band = filterA1_ * filterIc1eq_[stage] + filterA2_ * v3;
  float low = filterIc2eq_[stage] + filterA2_ * filterIc1eq_[stage] + filterA3_ * v3;
  float high = input - filterK_ * band - low;

  filterIc1eq_[stage] = 2.0f * band - filterIc1eq_[stage];
  filterIc2eq_[stage] = 2.0f * low - filterIc2eq_[stage];

  switch (filterMode_) {
    case BraidsFilterMode::bandPass: return band;
    case BraidsFilterMode::highPass: return high;
    default: return low;
  }
}

void BraidsVoice::setEnvelope(bool enabled, float attackSeconds,
                              float decaySeconds, float sustain,
                              float releaseSeconds) {
  envelopeEnabled_ = enabled;
  envelopeAttackSeconds_ = attackSeconds < 0.0f ? 0.0f : attackSeconds;
  envelopeDecaySeconds_ = decaySeconds < 0.0f ? 0.0f : decaySeconds;
  envelopeSustain_ = sustain < 0.0f ? 0.0f : (sustain > 1.0f ? 1.0f : sustain);
  envelopeReleaseSeconds_ = releaseSeconds < 0.0f ? 0.0f : releaseSeconds;
}

void BraidsVoice::enterEnvelopeStage(EnvelopeStage stage) {
  envelopeStage_ = stage;
  float seconds = 0.0f;
  float target = envelopeLevel_;

  switch (stage) {
    case EnvelopeStage::attack:
      seconds = envelopeAttackSeconds_;
      target = 1.0f;
      break;
    case EnvelopeStage::decay:
      seconds = envelopeDecaySeconds_;
      target = envelopeSustain_;
      break;
    case EnvelopeStage::release:
      seconds = envelopeReleaseSeconds_;
      target = 0.0f;
      break;
    case EnvelopeStage::sustain:
      envelopeLevel_ = envelopeSustain_;
      envelopeSamplesLeft_ = 0;
      envelopeIncrement_ = 0.0f;
      return;
    default:
      envelopeLevel_ = 0.0f;
      envelopeSamplesLeft_ = 0;
      envelopeIncrement_ = 0.0f;
      active_ = false;
      return;
  }

  envelopeSamplesLeft_ = static_cast<uint32_t>(seconds * kSampleRate);
  if (envelopeSamplesLeft_ == 0) {
    envelopeLevel_ = target;
    if (stage == EnvelopeStage::attack) enterEnvelopeStage(EnvelopeStage::decay);
    else if (stage == EnvelopeStage::decay) enterEnvelopeStage(EnvelopeStage::sustain);
    else enterEnvelopeStage(EnvelopeStage::idle);
    return;
  }
  envelopeIncrement_ = (target - envelopeLevel_) / envelopeSamplesLeft_;
}

void BraidsVoice::noteOn() {
  active_ = true;
  strike();
  if (envelopeEnabled_) {
    envelopeLevel_ = 0.0f;
    enterEnvelopeStage(EnvelopeStage::attack);
  }
}

void BraidsVoice::noteOff() {
  if (envelopeEnabled_) enterEnvelopeStage(EnvelopeStage::release);
}

void BraidsVoice::kill() {
  active_ = false;
  envelopeStage_ = EnvelopeStage::idle;
  envelopeLevel_ = 0.0f;
}

float BraidsVoice::processEnvelope() {
  if (!envelopeEnabled_) return 1.0f;
  if (envelopeSamplesLeft_ > 0) {
    envelopeLevel_ += envelopeIncrement_;
    if (--envelopeSamplesLeft_ == 0) {
      if (envelopeStage_ == EnvelopeStage::attack) {
        envelopeLevel_ = 1.0f;
        enterEnvelopeStage(EnvelopeStage::decay);
      } else if (envelopeStage_ == EnvelopeStage::decay) {
        enterEnvelopeStage(EnvelopeStage::sustain);
      } else if (envelopeStage_ == EnvelopeStage::release) {
        enterEnvelopeStage(EnvelopeStage::idle);
      }
    }
  }
  return envelopeLevel_;
}

void BraidsVoice::strike() {
  active_ = true;
  oscillator_.Strike();
}

void BraidsVoice::renderBlock() {
  memset(sync_, 0, sizeof(sync_));
  oscillator_.Render(sync_, block_, kBlockSize);
  blockPosition_ = 0;
}

void BraidsVoice::render(float* output, size_t samples) {
  if (!output) return;
  if (!active_) {
    memset(output, 0, samples * sizeof(float));
    return;
  }

  for (size_t i = 0; i < samples; ++i) {
    if (blockPosition_ == kBlockSize) renderBlock();
    float sample = static_cast<float>(block_[blockPosition_++]) / 32768.0f;
    if (filterEnabled_) {
      sample = processFilterStage(sample, 0);
      if (filterSlope24dB_) sample = processFilterStage(sample, 1);
    }
    output[i] = sample * processEnvelope() * gain_;
  }
}
