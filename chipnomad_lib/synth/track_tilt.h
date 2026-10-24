#ifndef CHOOCHOO_TRACK_TILT_H
#define CHOOCHOO_TRACK_TILT_H

#include <stdint.h>

class TrackTilt {
 public:
  void init(float sampleRate);
  float process(float input, int channel, uint8_t value, uint16_t pivotHz);
  bool isNeutral(uint8_t value, uint16_t pivotHz) const {
    return value == 0x80 && value_ == value && pivotHz_ == pivotHz && !smoothing_;
  }
  float passThrough(float input, int channel) {
    lowpass_[channel & 1] = input;
    return input;
  }

 private:
  void update(uint8_t value, uint16_t pivotHz);

  float sampleRate_;
  float lowpass_[2];
  float lowGain_, highGain_, drive_;
  float targetLowGain_, targetHighGain_, targetDrive_;
  float filterCoefficient_, smoothingCoefficient_;
  uint8_t value_;
  uint16_t pivotHz_;
  bool smoothing_;
};

#endif
