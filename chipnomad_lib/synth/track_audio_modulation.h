#pragma once

#include "audio_math.h"
#include "../project_instruments.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>

class TrackAudioVocoder {
 public:
  static constexpr int bands = 12;
  explicit TrackAudioVocoder(float sampleRate) {
    const float rate = std::max(8000.0f, sampleRate);
    sampleRate_ = rate;
    attack_ = 1.0f - std::exp(-1.0f / (0.001f * rate));
    levelAttack_ = 1.0f - std::exp(-1.0f / (0.005f * rate));
    levelRelease_ = 1.0f - std::exp(-1.0f / (0.1f * rate));
    for (int i = 0; i < bands; ++i) {
      const float frequency = 120.0f * std::pow(std::min(6500.0f, rate * 0.4f) / 120.0f,
        i / float(bands - 1));
      const float w = 6.283185307f * frequency / rate;
      const float alpha = std::sin(w) / 3.0f;
      filters_[i].b = alpha / (1.0f + alpha);
      filters_[i].a1 = -2.0f * std::cos(w) / (1.0f + alpha);
      filters_[i].a2 = (1.0f - alpha) / (1.0f + alpha);
    }
  }
  void reset() {
    controlFrame_ = 0;
    sourceLevel_ = 0;
    for (auto& filter : filters_) {
      for (auto& state : filter.state) state = {};
      filter.envelope = 0.0f;
    }
  }
  float releaseCoefficient(uint8_t value) {
    if (releaseValue_ != value) {
      releaseValue_ = value;
      release_ = 1.0f - std::exp(-1.0f /
        (std::max(0.005f, modulationAudioTimeSeconds(value)) * sampleRate_));
    }
    return release_;
  }
  void process(float source, float& left, float& right, float release) {
    const float level = std::fabs(source);
    sourceLevel_ += (level > sourceLevel_ ? levelAttack_ : levelRelease_) * (level - sourceLevel_);
    // Attenuate hot tracks before analysis without clipping away their formants.
    source /= std::max(0.25f, sourceLevel_ * 2.0f);
    float output[2] = {};
    for (auto& filter : filters_) {
      const float detected = std::fabs(filter.process(source, 0));
      filter.envelope += (detected > filter.envelope ? attack_ : release) *
        (detected - filter.envelope);
      const float gain = filter.envelope * 16.0f;
      output[0] += filter.process(left, 1) * gain;
      output[1] += filter.process(right, 2) * gain;
    }
    left = audioTanh(output[0] * 0.5f);
    right = audioTanh(output[1] * 0.5f);
    if (++controlFrame_ == 64) {
      controlFrame_ = 0;
      if (sourceLevel_ < 1.0e-20f) sourceLevel_ = 0;
      for (auto& filter : filters_) {
        if (filter.envelope < 1.0e-20f) filter.envelope = 0;
        for (auto& state : filter.state) {
          if (std::fabs(state.z1) < 1.0e-20f) state.z1 = 0;
          if (std::fabs(state.z2) < 1.0e-20f) state.z2 = 0;
        }
      }
    }
  }
 private:
  struct Filter {
    float b = 0, a1 = 0, a2 = 0, envelope = 0;
    struct State { float z1 = 0, z2 = 0; } state[6];
    float process(float input, int channel) {
      for (int stage = 0; stage < 2; ++stage) {
        auto& s = state[channel * 2 + stage];
        const float output = b * input + s.z1;
        s.z1 = -a1 * output + s.z2;
        s.z2 = -b * input - a2 * output;
        input = output;
      }
      return input;
    }
  } filters_[bands];
  float attack_ = 0;
  float sampleRate_ = 48000, release_ = 0;
  float sourceLevel_ = 0, levelAttack_ = 0, levelRelease_ = 0;
  int releaseValue_ = -1;
  uint8_t controlFrame_ = 0;
};

// Same producer-owned, audio-thread-released leases as BeadsPool.
template <int Capacity> class TrackAudioVocoderPool {
  struct Entry {
    std::atomic<unsigned> references{0};
    TrackAudioVocoder* dsp = nullptr;
  };
 public:
  class Handle {
   public:
    Handle() = default;
    Handle(const Handle& other) : entry_(other.entry_) { retain(); }
    Handle& operator=(const Handle& other) {
      if (entry_ != other.entry_) {
        release(); entry_ = other.entry_; retain();
      }
      return *this;
    }
    ~Handle() { release(); }
    TrackAudioVocoder* get() const { return entry_ ? entry_->dsp : nullptr; }
   private:
    friend class TrackAudioVocoderPool;
    explicit Handle(Entry* entry) : entry_(entry) { retain(); }
    void retain() { if (entry_) entry_->references.fetch_add(1, std::memory_order_relaxed); }
    void release() { if (entry_) entry_->references.fetch_sub(1, std::memory_order_release); }
    Entry* entry_ = nullptr;
  };
  ~TrackAudioVocoderPool() {
    for (auto& entry : entries_) {
      assert(entry.references.load(std::memory_order_relaxed) == 0);
      delete entry.dsp;
    }
  }
  Handle prepare(int slot, float sampleRate) {
    assert(slot >= 0 && slot < Capacity);
    auto& entry = entries_[slot];
    if (!entry.dsp) entry.dsp = new TrackAudioVocoder(sampleRate);
    return Handle(&entry);
  }
  void collect() {
    for (auto& entry : entries_) {
      if (entry.references.load(std::memory_order_acquire) == 0) {
        delete entry.dsp; entry.dsp = nullptr;
      }
    }
  }
 private:
  Entry entries_[Capacity];
};

inline float trackAudioDiode(float sample) {
  const float dead = std::max(0.0f, std::fabs(sample) - 0.667f) * 2.0f;
  return std::copysign(0.043247658f * dead * dead, sample);
}

inline float trackAudioCrossSample(AudioModulationMode mode, float carrier, float source) {
  carrier = std::clamp(carrier, -1.0f, 1.0f);
  if (carrier == 0.0f) return 0.0f;
  switch (mode) {
    case AudioModulationMode::diode:
      // Warps' MIT diode transfer, used by MME, with a balanced bridge.
      return audioTanh(12.0f * (trackAudioDiode(carrier + source) +
        trackAudioDiode(carrier - source)));
    case AudioModulationMode::fold:
      return (std::fabs(audioFmod4((carrier + source) * 3.0f + 9.0f) - 2.0f) - 1.0f) *
        std::fabs(carrier);
    case AudioModulationMode::bitXor: {
      const int a = (int)(carrier * 2047.0f), b = (int)(source * 2047.0f);
      const unsigned bits = ((unsigned)a ^ (unsigned)b) & 4095u;
      return std::clamp((bits >= 2048u ? (int)bits - 4096 : (int)bits) / 2047.0f,
        -1.0f, 1.0f) * std::fabs(carrier);
    }
    case AudioModulationMode::comparator:
      return carrier > source ? std::fabs(carrier) : -std::fabs(carrier);
    default: return carrier;
  }
}
