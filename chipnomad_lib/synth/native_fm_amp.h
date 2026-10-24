#pragma once
#include "../project_instruments.h"
#include "envelope.h"
#include <algorithm>
#include <cmath>

// Native FM envelopes remain in the core. This optional VCA is followed by a
// short event-only crossfade to avoid hard onset/retrigger discontinuities.
// No allocation, extra core, or change to native patch bytes is required.
class NativeFMAmp {
 public:
  void init(float rate) {
    envelope_.init(rate);
    transitionFrames_ = std::max(1, int(rate * .003f));
    gainFrames_ = std::max(1, int(rate * .001f));
    kill();
  }
  void configure(const InstrumentFMAmp& settings, float gain) {
    envelope_.configure(seconds(settings.attack), seconds(settings.decay),
      settings.sustain / 255.f, seconds(settings.release), settings.envelopeShape);
    if (settings.enabled && !enabled_ && gated_) envelope_.noteOn();
    enabled_ = settings.enabled != 0;
    gain = std::isfinite(gain) ? std::clamp(gain, 0.f, 1.f) : 0.f;
    if (gain != targetGain_) {
      targetGain_ = gain;
      gainRemaining_ = gainFrames_;
      gainStep_ = (gain - gain_) / gainFrames_;
    }
  }
  void noteOn() {
    from_[0] = last_[0]; from_[1] = last_[1];
    remaining_ = transitionFrames_;
    gated_ = true;
    if (enabled_) envelope_.noteOn();
  }
  void noteOff() { gated_ = false; if (enabled_) envelope_.noteOff(); }
  void kill() {
    envelope_.kill(); gated_ = false; remaining_ = 0;
    last_[0] = last_[1] = from_[0] = from_[1] = 0;
  }
  void process(float& left, float& right) {
    if (gainRemaining_ && !--gainRemaining_) gain_ = targetGain_;
    else if (gainRemaining_) gain_ += gainStep_;
    const float amp = gain_ * (enabled_ ? envelope_.next() : 1.f);
    left *= amp; right *= amp;
    if (remaining_) {
      const float x = 1.f - float(remaining_) / transitionFrames_;
      const float blend = x * x * (3.f - 2.f * x);
      left = from_[0] + blend * (left - from_[0]);
      right = from_[1] + blend * (right - from_[1]);
      --remaining_;
    }
    last_[0] = left; last_[1] = right;
  }
  float process(float value) { float right = value; process(value, right); return value; }
 private:
  static float seconds(uint8_t value) { return value * value / 13005.f; }
  Envelope envelope_;
  bool enabled_ = false, gated_ = false;
  float gain_ = 1, targetGain_ = 1, gainStep_ = 0;
  float last_[2]{}, from_[2]{};
  int transitionFrames_ = 144, remaining_ = 0, gainFrames_ = 48, gainRemaining_ = 0;
};
