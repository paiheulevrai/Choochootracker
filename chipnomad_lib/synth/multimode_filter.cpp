#include "multimode_filter.h"

#include <math.h>
#include <string.h>

static const float kMinCutoff = 20.0f;
static const float kMaxCutoff = 20000.0f;

void MultimodeFilter::init(float sampleRate) {
  sampleRate_ = sampleRate > 0.0f ? sampleRate : 96000.0f;
  reset();
  configure(false, 0, 0, false, kMaxCutoff, 0.0f);
}

void MultimodeFilter::configure(bool enabled, uint8_t character, uint8_t mode, bool slope24dB,
                                float cutoffHz, float resonance) {
  enabled_ = enabled;
  character_ = character > 4 ? 1 : character;
  mode_ = mode > 2 ? 0 : mode;
  slope24dB_ = slope24dB;
  if (cutoffHz < kMinCutoff) cutoffHz = kMinCutoff;
  if (cutoffHz > kMaxCutoff) cutoffHz = kMaxCutoff;
  if (cutoffHz > sampleRate_ * 0.45f) cutoffHz = sampleRate_ * 0.45f;
  if (resonance < 0.0f) resonance = 0.0f;
  if (resonance > 1.0f) resonance = 1.0f;
  g_ = tanf(3.14159265358979323846f * cutoffHz / sampleRate_);
  float characterResonance = character_ == 2 ? resonance * 0.7f : resonance;
  k_ = 1.0f / (0.5f + characterResonance * 19.5f);
  a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
  a2_ = g_ * a1_;
  a3_ = g_ * a2_;
}

void MultimodeFilter::reset() {
  memset(ic1eq_, 0, sizeof(ic1eq_));
  memset(ic2eq_, 0, sizeof(ic2eq_));
  memset(driveState_, 0, sizeof(driveState_));
}

float MultimodeFilter::processStage(float input, int channel, int stage) {
  if (character_ == 3) input = tanhf(input * 2.0f - driveState_[channel] * 0.35f);
  else if (character_ == 4) input = tanhf(input * 2.5f - driveState_[channel] * 0.75f);
  float v3 = input - ic2eq_[channel][stage];
  float band = a1_ * ic1eq_[channel][stage] + a2_ * v3;
  float low = ic2eq_[channel][stage] + a2_ * ic1eq_[channel][stage] + a3_ * v3;
  float high = input - k_ * band - low;
  ic1eq_[channel][stage] = 2.0f * band - ic1eq_[channel][stage];
  ic2eq_[channel][stage] = 2.0f * low - ic2eq_[channel][stage];
  float output = mode_ == 1 ? band : (mode_ == 2 ? high : low);
  if (character_ >= 3) driveState_[channel] = output;
  return output;
}

float MultimodeFilter::process(float input, int channel) {
  if (!enabled_) return input;
  if (channel < 0 || channel > 1) channel = 0;
  float output = processStage(input, channel, 0);
  return slope24dB_ || character_ == 2 ? processStage(output, channel, 1) : output;
}

float filterCutoffFromControl(uint8_t value) {
  return kMinCutoff * powf(kMaxCutoff / kMinCutoff, value / 255.0f);
}

uint8_t filterControlFromCutoff(float cutoffHz) {
  if (cutoffHz <= kMinCutoff) return 0;
  if (cutoffHz >= kMaxCutoff) return 255;
  return (uint8_t)(255.0f * logf(cutoffHz / kMinCutoff) /
                   logf(kMaxCutoff / kMinCutoff) + 0.5f);
}
