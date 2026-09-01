#include "braids_voice.h"

#include <math.h>
#include <string.h>
#include "stmlib/utils/dsp.h"

void BraidsVoice::init(float outputSampleRate) {
  oscillator_.Init();
  blockPosition_ = kBlockSize;
  model_ = 0;
  active_ = false;
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
  outputSampleRate_ = outputSampleRate > 0.0f ? outputSampleRate : kSampleRate;
  sourcePhase_ = 1.0f;
  previousSource_ = currentSource_ = 0.0f;
  memset(decimatorHistory_, 0, sizeof(decimatorHistory_));
  decimatorPosition_ = 0;

  post_.init(outputSampleRate_);
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
  post_.setGain(gain);
}

void BraidsVoice::setFilter(bool enabled, uint8_t character, BraidsFilterMode mode,
                            bool slope24dB, float cutoffHz,
                            float resonance) {
  post_.setFilter(enabled, character, static_cast<uint8_t>(mode), slope24dB, cutoffHz, resonance);
}

void BraidsVoice::setEnvelope(bool enabled, float attackSeconds,
                              float decaySeconds, float sustain,
                              float releaseSeconds, uint8_t shape) {
  post_.setEnvelope(enabled, attackSeconds, decaySeconds, sustain, releaseSeconds, shape);
}

void BraidsVoice::noteOn() {
  active_ = true;
  strike();
  post_.noteOn();
}

void BraidsVoice::noteOff() {
  post_.noteOff();
}

void BraidsVoice::kill() {
  active_ = false;
  post_.kill();
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

float BraidsVoice::nextSourceSample() {
  if (blockPosition_ == kBlockSize) renderBlock();
  return static_cast<float>(block_[blockPosition_++]) / 32768.0f;
}

float BraidsVoice::filterSourceSample(float sample) {
  static const float coefficients[] = {
    -0.001700396904f, 0.002937331571f, -0.006730091366f,
     0.014093887904f, -0.026785035820f, 0.049098960594f,
    -0.096938332776f, 0.315619563324f
  };
  decimatorPosition_ = decimatorPosition_ ? decimatorPosition_ - 1 : 30;
  decimatorHistory_[decimatorPosition_] = sample;
  decimatorHistory_[decimatorPosition_ + 31] = sample;
  const float* history = decimatorHistory_ + decimatorPosition_;
  float filtered = 0.500808226947f * history[15];
  for (size_t i = 0; i < 8; ++i)
    filtered += coefficients[i] * (history[i * 2] + history[30 - i * 2]);
  return filtered;
}

void BraidsVoice::render(float* output, size_t samples) {
  if (!output) return;
  if (!active_) {
    memset(output, 0, samples * sizeof(float));
    return;
  }

  const float sourceStep = kSampleRate / outputSampleRate_;
  for (size_t i = 0; i < samples; ++i) {
    sourcePhase_ += sourceStep;
    while (sourcePhase_ >= 1.0f) {
      sourcePhase_ -= 1.0f;
      previousSource_ = currentSource_;
      float source = nextSourceSample();
      currentSource_ = outputSampleRate_ < kSampleRate
        ? filterSourceSample(source) : source;
    }
    float sample = previousSource_ + (currentSource_ - previousSource_) * sourcePhase_;
    output[i] = post_.process(sample);
    if (!post_.envelopeActive()) {
      active_ = false;
      memset(output + i + 1, 0, (samples - i - 1) * sizeof(float));
      break;
    }
  }
}
