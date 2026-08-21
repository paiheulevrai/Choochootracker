#include "braids_voice.h"

#include <math.h>
#include <string.h>
#include "stmlib/utils/dsp.h"

void BraidsVoice::init() {
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

  post_.init(kSampleRate);
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

void BraidsVoice::render(float* output, size_t samples) {
  if (!output) return;
  if (!active_) {
    memset(output, 0, samples * sizeof(float));
    return;
  }

  for (size_t i = 0; i < samples; ++i) {
    if (blockPosition_ == kBlockSize) renderBlock();
    float sample = static_cast<float>(block_[blockPosition_++]) / 32768.0f;
    output[i] = post_.process(sample);
    if (!post_.envelopeActive()) {
      active_ = false;
      memset(output + i + 1, 0, (samples - i - 1) * sizeof(float));
      break;
    }
  }
}
