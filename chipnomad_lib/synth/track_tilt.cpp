#include "track_tilt.h"

#include <math.h>

void TrackTilt::init(float sampleRate) {
  sampleRate_ = sampleRate > 0.0f ? sampleRate : 96000.0f;
  lowpass_[0] = lowpass_[1] = 0.0f;
  lowGain_ = highGain_ = targetLowGain_ = targetHighGain_ = 1.0f;
  drive_ = targetDrive_ = 1.0f;
  filterCoefficient_ = 0.0f;
  smoothingCoefficient_ = 1.0f - expf(-1.0f / (sampleRate_ * 0.01f));
  value_ = 0x80;
  pivotHz_ = 1000;
}

void TrackTilt::update(uint8_t value, uint16_t pivotHz) {
  value_ = value;
  pivotHz_ = pivotHz < 250 ? 250 : (pivotHz > 4000 ? 4000 : pivotHz);
  float amount = value < 0x80 ? (0x80 - value) / 128.0f :
                                -(value - 0x80) / 127.0f;
  targetLowGain_ = powf(10.0f, amount * 9.0f / 20.0f);
  targetHighGain_ = powf(10.0f, -amount * 9.0f / 20.0f);
  targetDrive_ = value < 0x40 ? 1.0f + (0x40 - value) * 3.0f / 64.0f : 1.0f;
  filterCoefficient_ = 1.0f - expf(-6.283185307179586f * pivotHz_ / sampleRate_);
}

float TrackTilt::process(float input, int channel, uint8_t value, uint16_t pivotHz) {
  if (value != value_ || pivotHz != pivotHz_) update(value, pivotHz);
  lowGain_ += (targetLowGain_ - lowGain_) * smoothingCoefficient_;
  highGain_ += (targetHighGain_ - highGain_) * smoothingCoefficient_;
  drive_ += (targetDrive_ - drive_) * smoothingCoefficient_;
  if (value == 0x80) {
    lowpass_[channel & 1] = input;
    return input;
  }
  float& low = lowpass_[channel & 1];
  low += filterCoefficient_ * (input - low);
  float output = low * lowGain_ + (input - low) * highGain_;
  if (drive_ <= 1.001f) return output;
  output = tanhf(output * drive_) / tanhf(drive_);
  return output < -1.0f ? -1.0f : (output > 1.0f ? 1.0f : output);
}
