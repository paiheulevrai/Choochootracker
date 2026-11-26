#include "spatial_insert_pool.h"

#include "insert_fx.h"
#include "synth/effect_filters.h"
#include "clouds/dsp/frame.h"
#include "clouds/dsp/fx/reverb.h"
#include "beads/dsp/frame.h"
#include "beads/dsp/fx/reverb.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <new>
#include <vector>

namespace {
constexpr int blockSize = 128;
using namespace effect_filters;

class CloudsInsert final : public SpatialInsert {
 public:
  explicit CloudsInsert(float rate) : rate_(rate) {
    reverb_.Init(memory_, rate_);
  }
  void reset() override {
    reverb_.Init(memory_, rate_);
    filter_[0] = filter_[1] = 0.0f;
    hpInput_[0] = hpInput_[1] = hpOutput_[0] = hpOutput_[1] = 0.0f;
  }
  void process(float* left, float* right, int frames, const float* p, float) override {
    for (int offset = 0; offset < frames; offset += blockSize) {
      const int count = std::min(blockSize, frames - offset);
      const float lp = lowpassCoefficient(p[2], rate_);
      const float hp = highpassCoefficient(p[3], rate_);
      for (int i = 0; i < count; ++i) {
        const float l = highpass(left[offset + i], hp, hpInput_[0], hpOutput_[0]);
        const float r = highpass(right[offset + i], hp, hpInput_[1], hpOutput_[1]);
        frames_[i].l = lowpass(l, lp, filter_[0]);
        frames_[i].r = lowpass(r, lp, filter_[1]);
      }
      reverb_.set_amount(1.0f);
      reverb_.set_input_gain(0.2f);
      reverb_.set_time(p[0]);
      reverb_.set_diffusion(0.625f);
      reverb_.set_lp(0.02f + p[1] * 0.96f);
      reverb_.Process(frames_, count);
      for (int i = 0; i < count; ++i) {
        left[offset + i] = frames_[i].l;
        right[offset + i] = frames_[i].r;
      }
    }
  }
 private:
  float rate_;
  uint16_t memory_[65536]{};
  clouds::ResettableReverb reverb_;
  clouds::FloatFrame frames_[blockSize]{};
  float filter_[2]{};
  float hpInput_[2]{}, hpOutput_[2]{};
};

class BeadsReverbInsert final : public SpatialInsert {
 public:
  explicit BeadsReverbInsert(float rate) : rate_(rate) {
    reverb_.Init(memory_, rate_);
  }
  void reset() override {
    reverb_.Init(memory_, rate_);
    hpInput_[0] = hpInput_[1] = hpOutput_[0] = hpOutput_[1] = 0.0f;
  }
  void process(float* left, float* right, int frames, const float* p, float) override {
    for (int offset = 0; offset < frames; offset += blockSize) {
      const int count = std::min(blockSize, frames - offset);
      const float hp = highpassCoefficient(p[2], rate_);
      for (int i = 0; i < count; ++i) {
        frames_[i].l = highpass(left[offset + i], hp, hpInput_[0], hpOutput_[0]);
        frames_[i].r = highpass(right[offset + i], hp, hpInput_[1], hpOutput_[1]);
      }
      reverb_.set_mix(1.0f);
      reverb_.set_time(0.3f + 0.6f * p[0] * p[0] * p[0]);
      reverb_.set_lp(p[1]);
      reverb_.Process(frames_, count);
      for (int i = 0; i < count; ++i) {
        left[offset + i] = frames_[i].l;
        right[offset + i] = frames_[i].r;
      }
    }
  }
 private:
  float rate_;
  uint16_t memory_[32768]{};
  beads::ResettableReverb reverb_;
  beads::FloatFrame frames_[blockSize]{};
  float hpInput_[2]{}, hpOutput_[2]{};
};

class DelayInsert final : public SpatialInsert {
 public:
  DelayInsert(int module, float rate) : module_(module), rate_(rate) {
    delayLine_.resize((size_t)(rate_ * 8.0f) * 2);
    if (module_ == insertTapeEcho)
      springLine_.resize((size_t)(rate_ * 0.12f) * 2);
    if (module_ == insertDiffusingEchoverb) {
      static const float seconds[8] = {0.0071f, 0.0113f, 0.0173f, 0.0293f,
                                       0.0083f, 0.0131f, 0.0197f, 0.0311f};
      size_t size = 0;
      for (int i = 0; i < 8; ++i) {
        echoOffset_[i] = size;
        echoLength_[i] = std::max<size_t>(1, (size_t)(rate_ * seconds[i]));
        size += echoLength_[i];
      }
      echoDiffusion_.resize(size);
      echoSmoothing_ = 1.0f - expf(-1.0f / (0.020f * rate_));
      echoAgeRelease_ = expf(-1.0f / (0.050f * rate_));
      echoAgeGainSmoothing_ = 1.0f - expf(-8.0f / (0.005f * rate_));
    }
    reset();
  }

  void reset() override {
    std::fill(echoWrite_, echoWrite_ + 8, 0);
    std::fill(echoWritten_, echoWritten_ + 8, 0);
    delayWrite_ = springWrite_ = 0;
    delayWritten_ = springWritten_ = 0;
    delayFilter_[0] = delayFilter_[1] = 0.0f;
    tapeFilter_[0] = tapeFilter_[1] = 0.0f;
    springFilter_[0] = springFilter_[1] = 0.0f;
    flutterPhase_ = 0.0f;
    echoAgePeak_ = echoAgeInputPower_ = echoAgeOutputPower_ = 0.0f;
    echoAgeGain_ = 1.0f;
    echoAgeHeld_[0] = echoAgeHeld_[1] = 0.0f;
    echoAgePhase_ = 1.0f;
    echoAgeControlFrame_ = 0;
    echoHpInput_[0] = echoHpInput_[1] = echoHpOutput_[0] = echoHpOutput_[1] = 0.0f;
    std::fill(echoParameters_, echoParameters_ + 6, 0.0f);
    echoParametersReady_ = false;
  }

  void process(float* left, float* right, int frames, const float* p, float tickRate) override {
    if (module_ == insertPingPongDelay) processPingPong(left, right, frames, p, tickRate);
    else if (module_ == insertTapeEcho) processTape(left, right, frames, p, tickRate);
    else processEchoverb(left, right, frames, p, tickRate);
  }

 private:
  float delayRead(float samples, int channel) const {
    const size_t capacity = delayLine_.size() / 2;
    if (capacity < 3) return 0.0f;
    samples = std::max(1.0f, std::min(samples, (float)capacity - 1.0f));
    float read = (float)delayWrite_ - samples;
    while (read < 0.0f) read += capacity;
    while (read >= capacity) read -= capacity;
    const size_t first = (size_t)read;
    const size_t next = first + 1 == capacity ? 0 : first + 1;
    const float fraction = read - (float)first;
    const float a = delayWritten_ == capacity || first < delayWritten_
      ? delayLine_[first * 2 + channel] : 0.0f;
    const float b = delayWritten_ == capacity || next < delayWritten_
      ? delayLine_[next * 2 + channel] : 0.0f;
    return a + fraction * (b - a);
  }

  void processPingPong(float* left, float* right, int frames, const float* p, float tickRate) {
    const size_t capacity = delayLine_.size() / 2;
    const float delay = std::max(1.0f, std::min(
      floorf(rate_ * p[0] / tickRate + 0.5f), (float)capacity - 1.0f));
    const float filter = lowpassCoefficient(p[2], rate_);
    for (int i = 0; i < frames; ++i) {
      const float inL = left[i], inR = right[i];
      const float wetL = lowpass(delayRead(delay, 0), filter, delayFilter_[0]);
      const float wetR = lowpass(delayRead(delay, 1), filter, delayFilter_[1]);
      delayLine_[delayWrite_ * 2] = inL + wetR * p[1];
      delayLine_[delayWrite_ * 2 + 1] = inR + wetL * p[1];
      left[i] = wetL;
      right[i] = wetR;
      if (++delayWrite_ == capacity) delayWrite_ = 0;
      if (delayWritten_ < capacity) ++delayWritten_;
    }
  }

  void processTape(float* left, float* right, int frames, const float* p, float tickRate) {
    static constexpr uint8_t divisionTicks[] = {6, 8, 12, 18, 24, 36, 48};
    static constexpr uint8_t headMasks[] = {1, 2, 4, 3, 5, 6, 7};
    const uint8_t division = (uint8_t)std::max(0.0f, std::min(6.0f, p[0]));
    const uint8_t heads = headMasks[(int)std::max(0.0f, std::min(6.0f, p[1]))];
    const int headCount = ((heads & 1) != 0) + ((heads & 2) != 0) + ((heads & 4) != 0);
    const float feedback = p[2] / std::max(1, headCount);
    const size_t capacity = delayLine_.size() / 2;
    const float baseDelay = std::max(1.0f, std::min(rate_ * divisionTicks[division] / tickRate,
                                                   ((float)capacity - 2.0f) / 3.0f));
    const float tone = lowpassCoefficient(800.0f + 19200.0f * p[3] * p[3], rate_);
    const float springControl = p[5];
    const float springGain = springControl * (0.8f + 0.7f * springControl);
    const float springFeedback = 0.58f + 0.32f * springControl * springControl;
    const size_t springCapacity = springLine_.size() / 2;
    const float springDelayA = std::max(1.0f, std::min(rate_ * 0.031f, (float)springCapacity - 2.0f));
    const float springDelayB = std::max(1.0f, std::min(rate_ * 0.047f, (float)springCapacity - 2.0f));
    const float springFilter = lowpassCoefficient(5500.0f, rate_);
    const float flutterDepth = rate_ * 0.020f * p[4] * p[4];
    for (int i = 0; i < frames; ++i) {
      const float inL = left[i], inR = right[i];
      const float flutter = sinf(flutterPhase_) * flutterDepth;
      float tapeL = 0.0f, tapeR = 0.0f;
      for (int head = 0; head < 3; ++head) if (heads & (1 << head)) {
        tapeL += delayRead(baseDelay * (head + 1) + flutter, 0);
        tapeR += delayRead(baseDelay * (head + 1) + flutter, 1);
      }
      const float wetL = lowpass(tapeL, tone, tapeFilter_[0]);
      const float wetR = lowpass(tapeR, tone, tapeFilter_[1]);
      float springL = 0.0f, springR = 0.0f;
      if (springCapacity) {
        const size_t a = (springWrite_ + springCapacity - (size_t)springDelayA) % springCapacity;
        const size_t b = (springWrite_ + springCapacity - (size_t)springDelayB) % springCapacity;
        const float aL = springWritten_ >= (size_t)springDelayA ? springLine_[a * 2] : 0.0f;
        const float aR = springWritten_ >= (size_t)springDelayA ? springLine_[a * 2 + 1] : 0.0f;
        const float bL = springWritten_ >= (size_t)springDelayB ? springLine_[b * 2] : 0.0f;
        const float bR = springWritten_ >= (size_t)springDelayB ? springLine_[b * 2 + 1] : 0.0f;
        springL = lowpass(0.5f * (aL + bL),
                          springFilter, springFilter_[0]);
        springR = lowpass(0.5f * (aR + bR),
                          springFilter, springFilter_[1]);
        springLine_[springWrite_ * 2] = inL + springR * springFeedback;
        springLine_[springWrite_ * 2 + 1] = inR + springL * springFeedback;
        if (++springWrite_ == springCapacity) springWrite_ = 0;
        if (springWritten_ < springCapacity) ++springWritten_;
      }
      delayLine_[delayWrite_ * 2] = inL + wetL * feedback;
      delayLine_[delayWrite_ * 2 + 1] = inR + wetR * feedback;
      left[i] = wetL + springL * springGain;
      right[i] = wetR + springR * springGain;
      flutterPhase_ += 6.283185307179586f * 0.25f / rate_;
      if (flutterPhase_ >= 6.283185307179586f) flutterPhase_ -= 6.283185307179586f;
      if (++delayWrite_ == capacity) delayWrite_ = 0;
      if (delayWritten_ < capacity) ++delayWritten_;
    }
  }

  void processEchoverb(float* left, float* right, int frames, const float* p, float tickRate) {
    const size_t capacity = delayLine_.size() / 2;
    const float targets[6] = {
      std::max(1.0f, std::min(rate_ * p[0] / tickRate, (float)capacity - 2.0f)),
      p[1], lowpassCoefficient(p[5], rate_), highpassCoefficient(p[4], rate_),
      p[2], p[3]
    };
    if (!echoParametersReady_) {
      std::copy(targets, targets + 6, echoParameters_);
      echoParametersReady_ = true;
    }
    for (int i = 0; i < frames; ++i) {
      for (int parameter = 0; parameter < 6; ++parameter)
        echoParameters_[parameter] += echoSmoothing_ * (targets[parameter] - echoParameters_[parameter]);
      const float diffuse = echoParameters_[4];
      const float age = echoParameters_[5];
      const float ageCurve = age * age * age;
      const float resolution = 1.0f / 32768.0f + ageCurve * (0.125f - 1.0f / 32768.0f);
      const float drive = 12.0f * ageCurve;
      float wet[2] = {delayRead(echoParameters_[0], 0), delayRead(echoParameters_[0], 1)};
      float loops[2], clouds[2];
      for (int channel = 0; channel < 2; ++channel) {
        loops[channel] = lowpass(wet[channel], echoParameters_[2], delayFilter_[channel]);
        loops[channel] = highpass(loops[channel], echoParameters_[3], echoHpInput_[channel],
                                  echoHpOutput_[channel]);
      }
      const float peak = std::max(fabsf(loops[0]), fabsf(loops[1]));
      const bool transient = peak > echoAgePeak_ * 1.5f;
      echoAgePeak_ = std::max(peak, echoAgePeak_ * echoAgeRelease_);
      if (age > 0.0f) {
        const float step = std::max(1.0e-12f, echoAgePeak_) * resolution;
        echoAgePhase_ += 1.0f - ageCurve * (1.0f - std::min(1.0f, 1200.0f / rate_));
        const bool capture = echoAgePhase_ >= 1.0f || transient;
        if (capture) echoAgePhase_ = transient ? 0.0f : echoAgePhase_ - floorf(echoAgePhase_);
        float inputPower = 0.0f, outputPower = 0.0f;
        for (int channel = 0; channel < 2; ++channel) {
          inputPower += loops[channel] * loops[channel];
          if (capture) {
            const float quantized = roundf(loops[channel] / step) * step;
            const float normalized = quantized / std::max(1.0e-12f, echoAgePeak_);
            echoAgeHeld_[channel] = quantized * (1.0f + drive * 0.25f) /
              (1.0f + drive * fabsf(normalized));
          }
          outputPower += echoAgeHeld_[channel] * echoAgeHeld_[channel];
        }
        const float follow = 1.0f - echoAgeRelease_;
        echoAgeInputPower_ += follow * (inputPower - echoAgeInputPower_);
        echoAgeOutputPower_ += follow * (outputPower - echoAgeOutputPower_);
        if (++echoAgeControlFrame_ == 8) {
          echoAgeControlFrame_ = 0;
          const float gain = std::min(4.0f, sqrtf((echoAgeInputPower_ + 1.0e-20f) /
            (echoAgeOutputPower_ + 1.0e-20f)));
          echoAgeGain_ += echoAgeGainSmoothing_ * (gain - echoAgeGain_);
        }
        for (int channel = 0; channel < 2; ++channel) {
          const float processed = echoAgeHeld_[channel] * echoAgeGain_;
          loops[channel] += age * (processed - loops[channel]);
          loops[channel] = std::max(-2.0f, std::min(2.0f, loops[channel]));
        }
      } else {
        echoAgePhase_ = 1.0f;
      }
      for (int channel = 0; channel < 2; ++channel) {
        float cloud = loops[channel];
        for (int stage = 0; stage < 4; ++stage) {
          const int index = channel * 4 + stage;
          const size_t position = echoOffset_[index] + echoWrite_[index];
          const float delayed = echoWritten_[index] >= echoLength_[index]
            ? echoDiffusion_[position] : 0.0f;
          const float spread = delayed - 0.65f * cloud;
          echoDiffusion_[position] = cloud + 0.65f * spread;
          cloud = spread;
          if (++echoWrite_[index] == echoLength_[index]) echoWrite_[index] = 0;
          if (echoWritten_[index] < echoLength_[index]) ++echoWritten_[index];
        }
        clouds[channel] = cloud;
      }
      const float rotated[2] = {0.8f * clouds[0] - 0.6f * clouds[1],
                                0.6f * clouds[0] + 0.8f * clouds[1]};
      for (int channel = 0; channel < 2; ++channel) {
        const float loop = loops[channel] + diffuse * (rotated[channel] - loops[channel]);
        const float input = channel ? right[i] : left[i];
        delayLine_[delayWrite_ * 2 + channel] = input + loop * echoParameters_[1];
        if (channel) right[i] = wet[channel];
        else left[i] = wet[channel];
      }
      if (++delayWrite_ == capacity) delayWrite_ = 0;
      if (delayWritten_ < capacity) ++delayWritten_;
    }
  }

  int module_;
  float rate_;
  std::vector<float> delayLine_, springLine_, echoDiffusion_;
  size_t delayWrite_ = 0, springWrite_ = 0, delayWritten_ = 0, springWritten_ = 0;
  size_t echoOffset_[8]{}, echoLength_[8]{}, echoWrite_[8]{}, echoWritten_[8]{};
  float delayFilter_[2]{}, tapeFilter_[2]{}, springFilter_[2]{};
  float flutterPhase_ = 0.0f;
  float echoAgePeak_ = 0.0f, echoAgeInputPower_ = 0.0f, echoAgeOutputPower_ = 0.0f;
  float echoAgeGain_ = 1.0f, echoAgeHeld_[2]{}, echoAgePhase_ = 1.0f;
  float echoAgeRelease_ = 0.0f, echoAgeGainSmoothing_ = 0.0f;
  uint8_t echoAgeControlFrame_ = 0;
  float echoHpInput_[2]{}, echoHpOutput_[2]{};
  float echoParameters_[6]{}, echoSmoothing_ = 0.0f;
  bool echoParametersReady_ = false;
};

SpatialInsert* createSpatialInsert(int module, float rate) {
  if (!(rate > 0.0f)) return nullptr;
  if (module == insertCloudsReverb) return new (std::nothrow) CloudsInsert(rate);
  if (module == insertBeadsReverb) return new (std::nothrow) BeadsReverbInsert(rate);
  if (module == insertPingPongDelay || module == insertTapeEcho ||
      module == insertDiffusingEchoverb)
    return new (std::nothrow) DelayInsert(module, rate);
  return nullptr;
}
}

SpatialInsertPool::~SpatialInsertPool() {
  for (auto& entry : entries_) {
    assert(entry.references.load(std::memory_order_relaxed) == 0);
    delete entry.dsp;
  }
}

SpatialInsertPool::Handle SpatialInsertPool::prepare(int slot, int module, float sampleRate) {
  const int type = module - insertCloudsReverb;
  if (slot < 0 || slot >= slots || type < 0 || type >= spatialInsertModuleCount || !(sampleRate > 0.0f)) {
    std::fprintf(stderr, "Invalid spatial insert request: slot %d, module %d, rate %.1f\n",
                 slot, module, sampleRate);
    return {};
  }
  auto& entry = entries_[slot * spatialInsertModuleCount + type];
  if (!entry.dsp) {
    try {
      entry.dsp = createSpatialInsert(module, sampleRate);
    } catch (const std::bad_alloc&) {
      std::fprintf(stderr, "Unable to allocate spatial insert: slot %d, module %d\n", slot, module);
      return {};
    }
    if (!entry.dsp) {
      std::fprintf(stderr, "Unable to allocate spatial insert: slot %d, module %d\n", slot, module);
      return {};
    }
  } else if (entry.references.load(std::memory_order_acquire) == 0) {
    entry.dsp->reset();
  }
  return Handle(&entry);
}

void SpatialInsertPool::collect() {
  for (auto& entry : entries_) {
    if (entry.references.load(std::memory_order_acquire) == 0) {
      delete entry.dsp;
      entry.dsp = nullptr;
    }
  }
}

int SpatialInsertPool::allocated() const {
  int count = 0;
  for (const auto& entry : entries_) count += entry.dsp != nullptr;
  return count;
}
