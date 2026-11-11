#ifndef CHOOCHOO_LINK_AUDIO_CLOCK_H
#define CHOOCHOO_LINK_AUDIO_CLOCK_H

#include <algorithm>
#include <cmath>
#include <cstdint>

// SDL/ALSA may deliver several callbacks in a burst. Sample positions, not
// callback arrival times, determine the continuous audio timeline.
class LinkAudioClock {
 public:
  void reset() { rate_ = 0; }

  double time(uint64_t counter, int rate, double hostTime) {
    if (rate_ != rate || counter < counter_ ||
        std::fabs(hostTime - time_) > 250000.0) {
      rate_ = rate;
      counter_ = counter;
      time_ = hostTime;
      return time_;
    }
    const double duration = (counter - counter_) * 1000000.0 / rate;
    const double expected = time_ + duration;
    // Track clock drift slowly without turning scheduler jitter into large
    // sample skips (at most half a sample per 512-frame callback).
    const double correction = std::clamp((hostTime - expected) * 0.001,
                                         -duration * 0.001, duration * 0.001);
    time_ = expected + correction;
    counter_ = counter;
    return time_;
  }

 private:
  int rate_ = 0;
  uint64_t counter_ = 0;
  double time_ = 0;
};

#endif
