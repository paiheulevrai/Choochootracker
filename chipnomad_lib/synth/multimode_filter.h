#ifndef CHOOCHOO_MULTIMODE_FILTER_H
#define CHOOCHOO_MULTIMODE_FILTER_H

#include <stdint.h>

class MultimodeFilter {
 public:
  void init(float sampleRate);
  void configure(bool enabled, uint8_t mode, bool slope24dB,
                 float cutoffHz, float resonance);
  void reset();
  float process(float input, int channel = 0);

 private:
  float processStage(float input, int channel, int stage);

  float sampleRate_;
  bool enabled_;
  uint8_t mode_;
  bool slope24dB_;
  float g_, k_, a1_, a2_, a3_;
  float ic1eq_[2][2];
  float ic2eq_[2][2];
};

float filterCutoffFromControl(uint8_t value);
uint8_t filterControlFromCutoff(float cutoffHz);

#endif
