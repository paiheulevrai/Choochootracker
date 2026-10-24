#pragma once
#include <array>

#include "../soundbank_runtime.h"
#include "../soundbank_controls.h"
#include "envelope.h"
#include "multimode_filter.h"
class SoundbankVoice {
 public:
  static constexpr size_t maxLayers = 8;
  void init(float rate);
  void configure(uint32_t handle, float cents, float gain, int velocity, float cutoff = 20000,
                 const SoundbankControls& controls = soundbankDefaultControls());
  void noteOn();
  void noteOff();
  void kill();
  void stopImmediately(); // Transport output owns the final fade after rendering ends.
  void render(float* stereo, size_t frames);
  bool active() const;
  float envelopeLevel() const;
  uint32_t handle() const { return handle_; }
  void choke(int group, uint32_t handle);
  void chokeFrom(const SoundbankVoice& incoming);

 private:
  struct Layer {
    const soundbank::PreparedZone* zone = nullptr;
    double position = 0, step = 1;
    float gain = 1, left = 1, right = 1, tune = 0;
    uint32_t delay = 0, hold = 0;
    bool released = false, holding = false, attacking = false, choked = false;
    Envelope envelope;
    float decay = 0, sustain = 1, release = 0;
    MultimodeFilter filter;
  };
  std::array<Layer, maxLayers> layers_{};
  float rate_ = 48000, cents_ = 6000, gain_ = 1, cutoff_ = 20000;
  int velocity_ = 127;
  SoundbankControls controls_ = soundbankDefaultControls();
  uint32_t handle_ = 0, sequence_ = 0;
  bool matches(const soundbank::Zone& z, bool release = false) const;
  void startLayer(Layer& layer, const soundbank::PreparedZone& zone, size_t index);
  bool noteHeld_ = false;
  int noteKey_ = 60, noteVelocity_ = 127;
  uint32_t noteSequence_ = 0;
  // A short output correction joins retriggers/cuts without retaining old assets.
  std::array<float, 2> lastOutput_{}, transitionOffset_{};
  uint32_t transitionFrames_ = 0, transitionLength_ = 144;
  bool transitionPending_ = false;
  void updateLayer(Layer& layer);
};
