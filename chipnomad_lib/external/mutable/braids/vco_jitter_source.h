// Copyright 2012 Emilie Gillet. MIT License.
#ifndef BRAIDS_VCO_JITTER_SOURCE_H_
#define BRAIDS_VCO_JITTER_SOURCE_H_

#include "braids/resources.h"
#include "stmlib/utils/random.h"

namespace braids {

class VcoJitterSource {
 public:
  void Init() {
    external_temperature_ = room_temperature_ = 0;
    phase_ = phase_step_ = 0;
  }

  int16_t Render(int32_t intensity) {
    if (stmlib::Random::GetWord() == 0) {
      phase_step_ = phase_step_ * 1664525L + 1013904223L;
      phase_ += (phase_step_ >> 16) * (phase_step_ >> 16);
      external_temperature_ = wav_sine[phase_ >> 24] << 8;
    }
    room_temperature_ += (external_temperature_ - room_temperature_) >> 16;
    return room_temperature_ * intensity >> 19;
  }

 private:
  uint32_t phase_step_, phase_;
  int32_t external_temperature_, room_temperature_;
};

}  // namespace braids
#endif
