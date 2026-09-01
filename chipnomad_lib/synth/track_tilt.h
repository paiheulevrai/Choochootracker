#ifndef CHOOCHOO_TRACK_TILT_H
#define CHOOCHOO_TRACK_TILT_H

#include <stdint.h>

class TrackTilt {
 public:
  void init(float sampleRate);
  float process(float input, int channel, uint8_t value, uint16_t pivotHz);

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
