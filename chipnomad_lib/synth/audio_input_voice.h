#pragma once
#include "voice_post_processor.h"
#include "../project_instruments.h"
#include <cstddef>

// One shared capture block can feed several tracks without consuming it.
class AudioInputVoice {
 public:
  void init(float sampleRate) { post_.init(sampleRate); kill(); }
  void configure(const InstrumentVoicePostSettings& p, float gain) {
    post_.setGain(gain);
    post_.setFilter(p.filterEnabled != 0, p.filterCharacter, p.filterMode,
      p.filterSlope24dB != 0, p.filterCutoffHz, p.filterResonance / 255.0f, p.filterColor);
    post_.setEnvelope(true, p.attack * p.attack / 13005.0f,
      p.decay * p.decay / 13005.0f, p.sustain / 255.0f,
      p.release * p.release / 13005.0f, p.envelopeShape);
  }
  void setInput(const float* stereo, size_t frames = (size_t)-1) { input_ = stereo; inputFrames_ = frames; }
  void noteOn() { post_.noteOn(!active_); active_ = true; }
  void noteOff() { post_.noteOff(); }
  void kill() { active_ = false; post_.kill(); input_ = nullptr; inputFrames_ = 0; }
  bool active() const { return active_; }
  float envelopeLevel() const { return active_ ? post_.envelopeLevel() : 0.0f; }
  void render(float* stereo, size_t frames) {
    for (size_t i = 0; i < frames; ++i) {
      stereo[i * 2] = active_ && input_ && i < inputFrames_ ? input_[i * 2] : 0.0f;
      stereo[i * 2 + 1] = active_ && input_ && i < inputFrames_ ? input_[i * 2 + 1] : 0.0f;
      if (active_) {
        post_.processStereo(stereo + i * 2);
        active_ = post_.envelopeActive();
      }
    }
  }
 private:
  VoicePostProcessor<> post_;
  const float* input_ = nullptr;
  size_t inputFrames_ = 0;
  bool active_ = false;
};
