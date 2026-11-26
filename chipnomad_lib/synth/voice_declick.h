#ifndef CHOOCHOO_VOICE_DECLICK_H
#define CHOOCHOO_VOICE_DECLICK_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

class VoiceDeclick {
 public:
  void init(float sampleRate) {
    length_ = (uint32_t)(sampleRate * 0.002f);
    if (length_ < 2) length_ = 2;
    inverseLength_ = 1.0f / length_;
    inverseTailLength_ = 1.0f / (length_ - 1);
    rampRemaining_ = tailRemaining_ = 0;
    last_ = rampFrom_ = tailFrom_ = 0.0f;
  }
  void trigger(bool smooth) {
    rampFrom_ = last_;
    rampRemaining_ = smooth ? length_ : 0;
    tailRemaining_ = 0;
  }
  void cut(bool active) {
    if (active && last_ != 0.0f) {
      tailFrom_ = last_;
      tailRemaining_ = length_;
    }
    rampRemaining_ = 0;
  }
  float process(float sample) {
    if (rampRemaining_) {
      const float blend = (length_ - rampRemaining_ + 1) * inverseLength_;
      sample = rampFrom_ + (sample - rampFrom_) * blend;
      --rampRemaining_;
    }
    return last_ = sample;
  }
  void renderTail(float* output, size_t frames) {
    size_t i = 0;
    for (; i < frames && tailRemaining_; ++i) {
      last_ = tailFrom_ * (tailRemaining_ - 1) * inverseTailLength_;
      output[i] = last_;
      --tailRemaining_;
    }
    if (i < frames) memset(output + i, 0, (frames - i) * sizeof(float));
  }
  bool tailActive() const { return tailRemaining_ != 0; }

 private:
  uint32_t length_ = 2, rampRemaining_ = 0, tailRemaining_ = 0;
  float inverseLength_ = 0.5f, inverseTailLength_ = 1.0f;
  float last_ = 0.0f, rampFrom_ = 0.0f, tailFrom_ = 0.0f;
};

#endif
