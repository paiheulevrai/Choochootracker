// Copyright 2012 Emilie Gillet. MIT License.
#ifndef BRAIDS_SIGNATURE_WAVESHAPER_H_
#define BRAIDS_SIGNATURE_WAVESHAPER_H_

#include "stmlib/stmlib.h"
#include "stmlib/utils/dsp.h"
#include "braids/resources.h"
#include <stdlib.h>

namespace braids {

class SignatureWaveshaper {
 public:
  void Init(uint32_t seed) {
    int32_t skew = seed & 15; seed >>= 4;
    int32_t sigmoid_strength = seed & 31; seed >>= 5;
    int32_t bumplets_frequency = (seed & 3) + 3; seed >>= 2;
    int32_t bumplets_width = ((seed & 7) + 1) << 7;
    bumplets_width *= bumplets_width;
    for (int i = 0; i < 256; ++i) {
      int16_t x = (i - 128) << 8;
      x = stmlib::Mix(x, i * i - 32768, skew << 11);
      int16_t sigmoid = x * (8192 + (sigmoid_strength << 10)) /
          (8192 + (sigmoid_strength * abs(x) >> 5));
      int16_t bumplets = wav_sine[(i * bumplets_frequency) & 255];
      uint16_t gain = x * x / bumplets_width + 16;
      gain = 32768 * 128 / (128 + gain);
      transfer_[i] = stmlib::Mix(sigmoid, bumplets, gain);
    }
    transfer_[256] = transfer_[255];
  }

  int32_t Transform(int16_t sample) const {
    uint16_t i = sample + 32768;
    int32_t a = transfer_[i >> 8];
    int32_t b = transfer_[(i >> 8) + 1];
    return a + ((b - a) * (i & 0xff) >> 8);
  }

 private:
  int32_t transfer_[257];
};

}  // namespace braids
#endif
