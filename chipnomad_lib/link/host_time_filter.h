// HostTimeFilter (T29): linear regression over the last N
// (sampleCounter, hostTimeMicros) pairs observed at audio-callback
// entry. Maps a sample counter to a host-time estimate so a requested
// beat-0 alignment can be expressed on Link's host-time scale. Owned by
// the backend state, touched only from the audio thread (no locks).
//
// The regression is intentionally simple: with a stable audio clock the
// pairs are near-perfectly linear, so a sliding-window least-squares fit
// gives a robust slope (micros per sample) and intercept. The window
// resets on enable(), sample-rate change or buffer-size change (see
// linkBackendResetHostTimeFilter).
//
// Header-only so the unit test (test_host_time_filter.cpp) can exercise
// the exact code the real backend runs; link_backend_link.cpp includes
// this file inside its anonymous namespace.
#ifndef __CHIPNOMAD_LIB__LINK__HOST_TIME_FILTER_H__
#define __CHIPNOMAD_LIB__LINK__HOST_TIME_FILTER_H__

#include <cmath>
#include <cstdint>

// HostTimeFilter (T29): linear regression over the last N
// (sampleCounter, hostTimeMicros) pairs observed at audio-callback
// entry. Maps a sample counter to a host-time estimate so a requested
// beat-0 alignment can be expressed on Link's host-time scale. Owned by
// the backend state, touched only from the audio thread (no locks).
//
// The regression is intentionally simple: with a stable audio clock the
// pairs are near-perfectly linear, so a sliding-window least-squares fit
// gives a robust slope (micros per sample) and intercept. The window
// resets on enable(), sample-rate change or buffer-size change (see
// linkBackendResetHostTimeFilter).
class HostTimeFilter {
public:
  static constexpr int kWindow = 64;

  void reset() {
    count_ = 0;
    next_ = 0;
    sumX_ = 0.0;
    sumY_ = 0.0;
    sumXX_ = 0.0;
    sumXY_ = 0.0;
  }

  void add(uint64_t sampleCounter, uint64_t hostTimeMicros) {
    const double x = (double)sampleCounter;
    const double y = (double)hostTimeMicros;
    if (count_ == kWindow) {
      // Sliding window: subtract the evicted pair from the running sums.
      const double ox = xs_[next_];
      const double oy = ys_[next_];
      sumX_ -= ox;
      sumY_ -= oy;
      sumXX_ -= ox * ox;
      sumXY_ -= ox * oy;
    } else {
      ++count_;
    }
    xs_[next_] = x;
    ys_[next_] = y;
    next_ = (next_ + 1) % kWindow;
    sumX_ += x;
    sumY_ += y;
    sumXX_ += x * x;
    sumXY_ += x * y;
  }

  // Returns the estimated host time (micros) for sample counter x, or
  // false while fewer than 2 pairs have been observed (no fit yet).
  bool estimate(uint64_t sampleCounter, uint64_t* outMicros) const {
    if (count_ < 2 || outMicros == nullptr) return false;
    const double n = (double)count_;
    const double meanX = sumX_ / n;
    const double meanY = sumY_ / n;
    const double sxx = sumXX_ - n * meanX * meanX;
    const double sxy = sumXY_ - n * meanX * meanY;
    if (sxx <= 0.0) return false;
    const double slope = sxy / sxx;          // micros per sample
    const double intercept = meanY - slope * meanX;
    const double y = intercept + slope * (double)sampleCounter;
    if (!(y > 0.0) || !std::isfinite(y)) return false;
    *outMicros = (uint64_t)y;
    return true;
  }

private:
  double xs_[kWindow];
  double ys_[kWindow];
  int count_ = 0;
  int next_ = 0;
  double sumX_ = 0.0;
  double sumY_ = 0.0;
  double sumXX_ = 0.0;
  double sumXY_ = 0.0;
};

#endif // __CHIPNOMAD_LIB__LINK__HOST_TIME_FILTER_H__
