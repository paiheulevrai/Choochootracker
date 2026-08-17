#include "master_effects.h"

#include <algorithm>
#include <math.h>

#include "project.h"

static float mixerGain(uint8_t value) {
  if (value == 0) return 0.0f;
  return powf(10.0f, -60.0f * (100 - value) / 2000.0f);
}

void MasterEffects::init(float sampleRate) {
  sampleRate_ = sampleRate > 0.0f ? sampleRate : 96000.0f;
  reverb_.Init(reverbMemory_);
  reverbFrames_.clear();
  delayLine_.assign((size_t)(sampleRate_ * 8.0f) * 2, 0.0f);
  delayWrite_ = 0;
  reverbFilterState_[0] = reverbFilterState_[1] = 0.0f;
  delayFilterState_[0] = delayFilterState_[1] = 0.0f;
}

float MasterEffects::lowpass(float input, float cutoff, float& state) {
  if (cutoff < 20.0f) cutoff = 20.0f;
  if (cutoff > sampleRate_ * 0.45f) cutoff = sampleRate_ * 0.45f;
  float coefficient = 1.0f - expf(-6.283185307179586f * cutoff / sampleRate_);
  state += coefficient * (input - state);
  return state;
}

void MasterEffects::process(float* reverbBus, float* delayBus, float* output,
                            size_t frames, const Project* project) {
  if (!project || !output) return;

  if (reverbBus) {
    reverbFrames_.resize(frames);
    for (size_t i = 0; i < frames; ++i) {
      reverbFrames_[i].l = lowpass(reverbBus[i * 2], project->reverbFilterCutoffHz, reverbFilterState_[0]);
      reverbFrames_[i].r = lowpass(reverbBus[i * 2 + 1], project->reverbFilterCutoffHz, reverbFilterState_[1]);
    }
    reverb_.set_amount(1.0f);
    reverb_.set_input_gain(0.2f);
    float time = project->reverbTime / 255.0f;
    reverb_.set_time(project->perceptualEffects
      ? 0.20f * powf(0.98f / 0.20f, time)
      : 0.2f + time * 0.78f);
    reverb_.set_diffusion(0.625f);
    float damping = project->reverbDamping / 255.0f;
    reverb_.set_lp(project->perceptualEffects
      ? 0.02f + 0.96f * damping * damping
      : 0.02f + damping * 0.96f);
    reverb_.Process(reverbFrames_.data(), frames);
    float gain = project->perceptualEffects ? mixerGain(project->reverbReturn) : project->reverbReturn / 100.0f;
    for (size_t i = 0; i < frames; ++i) {
      output[i * 2] += reverbFrames_[i].l * gain;
      output[i * 2 + 1] += reverbFrames_[i].r * gain;
    }
  }

  if (!delayBus || delayLine_.empty()) return;
  size_t capacity = delayLine_.size() / 2;
  float tickRate = project->tickRate > 0.0f ? project->tickRate : 50.0f;
  size_t delaySamples = (size_t)(sampleRate_ * project->delayTicks / tickRate + 0.5f);
  delaySamples = std::max<size_t>(1, std::min(delaySamples, capacity - 1));
  float feedback = std::min<int>(project->delayFeedback, 95) / 100.0f;
  float returnGain = project->perceptualEffects ? mixerGain(project->delayReturn) : project->delayReturn / 100.0f;
  for (size_t i = 0; i < frames; ++i) {
    size_t read = (delayWrite_ + capacity - delaySamples) % capacity;
    float wetL = lowpass(delayLine_[read * 2], project->delayFilterCutoffHz, delayFilterState_[0]);
    float wetR = lowpass(delayLine_[read * 2 + 1], project->delayFilterCutoffHz, delayFilterState_[1]);
    delayLine_[delayWrite_ * 2] = delayBus[i * 2] + wetR * feedback;
    delayLine_[delayWrite_ * 2 + 1] = delayBus[i * 2 + 1] + wetL * feedback;
    output[i * 2] += wetL * returnGain;
    output[i * 2 + 1] += wetR * returnGain;
    delayWrite_ = (delayWrite_ + 1) % capacity;
  }
}
