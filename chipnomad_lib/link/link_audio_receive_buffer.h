#ifndef CHOOCHOO_LINK_AUDIO_RECEIVE_BUFFER_H
#define CHOOCHOO_LINK_AUDIO_RECEIVE_BUFFER_H

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "link_client.h"

// Allocated before enabling receive; only the audio consumer touches its state.
class LinkAudioLocalDelay {
 public:
  static constexpr int maxRate = 48000;
  static int framesFor(int rate, int millis) {
    assert(rate > 0 && rate <= maxRate);
    assert(millis >= 0 && millis <= linkAudioMaxCompensationMillis);
    return (rate * millis + 500) / 1000;
  }

  void reset() { delayFrames_ = -1; }

  void process(float* samples, int frames, int rate, int millis) {
    const int delay = framesFor(rate, millis);
    if (delay != delayFrames_ || rate != rate_) {
      delayFrames_ = delay;
      rate_ = rate;
      write_ = filled_ = 0;
    }
    if (!delay) return;
    for (int frame = 0; frame < frames; ++frame) {
      for (int channel = 0; channel < 2; ++channel) {
        const int index = write_ * 2 + channel;
        const float previous = filled_ < delay ? 0.0f : samples_[index];
        samples_[index] = samples[frame * 2 + channel];
        samples[frame * 2 + channel] = previous;
      }
      if (filled_ < delay) ++filled_;
      if (++write_ == delay) write_ = 0;
    }
  }

 private:
  float samples_[maxRate * linkAudioMaxCompensationMillis / 1000 * 2];
  int delayFrames_ = -1, rate_ = 0, write_ = 0, filled_ = 0;
};

// Single network producer, single audio consumer. Times use the local Link clock.
class LinkAudioReceiveBuffer {
 public:
  // 256 slots also cover 100 ms plus one 2048-frame callback when peers
  // send small 32-frame packets at CCT's maximum 48 kHz rate.
  static constexpr unsigned blockFrames = 128;
  static constexpr unsigned blockCount = 256;

  bool push(const int16_t* samples, unsigned frames, unsigned rate, double time) {
    if (!samples || !frames || rate < 8000 || rate > 192000 || !std::isfinite(time)) return false;
    unsigned write = write_.load(std::memory_order_relaxed);
    const unsigned read = read_.load(std::memory_order_acquire);
    const unsigned needed = (frames - 1) / blockFrames + 1;
    if (needed > blockCount - (write - read)) return false;
    while (frames) {
      auto& block = blocks_[write % blockCount];
      block.frames = std::min(frames, blockFrames);
      block.rate = rate;
      block.time = time;
      std::memcpy(block.samples, samples, block.frames * 2 * sizeof(*samples));
      time += block.frames * 1000000.0 / rate;
      frames -= block.frames;
      samples += block.frames * 2;
      ++write;
    }
    write_.store(write, std::memory_order_release);
    return true;
  }

  int mix(float* output, int frames, int rate, double time) {
    unsigned read = read_.load(std::memory_order_relaxed);
    const unsigned write = write_.load(std::memory_order_acquire);
    if (read == write) return 0;
    int mixed = 0;
    const double step = 1000000.0 / rate;
    for (int frame = 0; frame < frames; ++frame, time += step) {
      while (read != write) {
        const auto& block = blocks_[read % blockCount];
        const double position = (time - block.time) * block.rate / 1000000.0;
        if (position >= block.frames) { ++read; continue; }
        // Timestamp rounding/drift below one sample must not insert a zero
        // between otherwise consecutive packets.
        if (position <= -1.0 + 1e-6) break;
        const double samplePosition = std::max(0.0, position);
        const unsigned index = static_cast<unsigned>(samplePosition);
        const unsigned next = std::min(index + 1, block.frames - 1);
        const float fraction = static_cast<float>(samplePosition - index);
        const int16_t* nextSamples = block.samples + next * 2;
        if (index + 1 == block.frames && read + 1 != write) {
          const auto& following = blocks_[(read + 1) % blockCount];
          const double end = block.time + block.frames * 1000000.0 / block.rate;
          if (following.rate == block.rate &&
              std::fabs(following.time - end) < 1000000.0 / block.rate)
            nextSamples = following.samples;
        }
        for (int channel = 0; channel < 2; ++channel) {
          const float first = block.samples[index * 2 + channel];
          const float second = nextSamples[channel];
          output[frame * 2 + channel] += (first + (second - first) * fraction) / 32768.0f;
        }
        ++mixed;
        break;
      }
    }
    read_.store(read, std::memory_order_release);
    return mixed;
  }

  // Only the consumer discards queued samples; never reset the producer index.
  void discard() { read_.store(write_.load(std::memory_order_acquire), std::memory_order_release); }

 private:
  struct Block {
    double time;
    unsigned frames, rate;
    int16_t samples[blockFrames * 2];
  };
  Block blocks_[blockCount];
  std::atomic<unsigned> write_{0}, read_{0};
};

#endif
