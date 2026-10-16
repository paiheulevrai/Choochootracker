// JM-inspired formant insert. See external/insert_fx/LICENSE-JM-Formant.
#ifndef CHOOCHOO_FORMANT_FILTER_H
#define CHOOCHOO_FORMANT_FILTER_H
#include <stdint.h>

struct FormantFrequencies { float f1, f2; };
// Exact byte anchors: 00 A, 20 E, 40 I, 60 O, 7F U.
FormantFrequencies formantVowelFrequencies(float position);
float formantCustomFrequency(int formant, float value);
float formantResonance(float value);

class FormantFilter {
 public:
  void reset(float sampleRate);
  // MODE, VOW, F1, F2, RES, MIX. Inactive controls are deliberately ignored.
  void set(const uint8_t values[8]);
  void process(float* left, float* right, int frames);
 private:
  struct Coefficients { double b0=1,b1=0,b2=0,a1=0,a2=0; };
  struct History { double x1=0,x2=0,y1=0,y2=0; };
  Coefficients coefficients_[2], destination_[2], delta_[2];
  History history_[2][2];
  float rate_=48000, slew_=0, mixSlew_=0;
  float targetLog_[3]{}, currentLog_[3]{};
  float targetMix_=0, mix_=0;
  int controlRemaining_=0;
  bool initialized_=false;
  void update();
};
#endif
