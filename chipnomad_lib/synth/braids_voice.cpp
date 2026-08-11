#include "braids_voice.h"

#include <math.h>
#include <string.h>
#include "stmlib/utils/dsp.h"

void BraidsVoice::init() {
  oscillator_.Init();
  blockPosition_ = kBlockSize;
  model_ = 0;
  active_ = false;
  gain_ = 1.0f;
  basePitch_ = 60 << 7;
  bits_ = 6;
  drift_ = signature_ = 0;
  signatureSeed_ = 0x434354u;
  signatureWaveshaper_.Init(signatureSeed_);
  jitterSource_.Init();
  oscillator_.set_shape(braids::MACRO_OSC_SHAPE_CSAW);
  oscillator_.set_pitch(60 << 7);
  oscillator_.set_parameters(16384, 16384);
  memset(sync_, 0, sizeof(sync_));

  filter_.init(kSampleRate);

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
  basePitch_ = pitch;
}

void BraidsVoice::setGlobalSettings(uint8_t bits, uint8_t drift,
                                    uint8_t signature, uint32_t signatureSeed) {
  bits_ = bits > 6 ? 6 : bits;
  drift_ = drift > 4 ? 4 : drift;
  signature_ = signature > 4 ? 4 : signature;
  if (signatureSeed && signatureSeed != signatureSeed_) {
    signatureSeed_ = signatureSeed;
    signatureWaveshaper_.Init(signatureSeed_);
  }
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
  filter_.configure(enabled, static_cast<uint8_t>(mode), slope24dB,
                    cutoffHz, resonance);
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
  oscillator_.set_pitch(basePitch_ + jitterSource_.Render(drift_));
  oscillator_.Render(sync_, block_, kBlockSize);
  static const uint16_t masks[] = {
    0xc000, 0xe000, 0xf000, 0xf800, 0xff00, 0xfff0, 0xffff
  };
  uint16_t signature = signature_ * signature_ * 4095;
  for (size_t i = 0; i < kBlockSize; ++i) {
    int16_t sample = block_[i] & masks[bits_];
    block_[i] = stmlib::Mix(sample, signatureWaveshaper_.Transform(sample),
                            signature);
  }
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
    sample = filter_.process(sample);
    output[i] = sample * processEnvelope() * gain_;
  }
}
