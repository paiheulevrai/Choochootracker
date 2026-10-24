#pragma once
#include "../external/monksynth/synth_internal.h"
#include "../project_instruments.h"
#include <cstddef>
#include <memory>

// All ten unison sources and delay storage are allocated before playback.
class MonkSynthVoice {
 public:
  void init(float sampleRate, const MonkSynthVoice* prototype = nullptr);
  void unload();
  bool initialized() const { return bool(synth_); }
  void configure(const InstrumentMonk*, float pitchCents, float gain);
  void noteOn();
  void noteOff();
  void kill();
  void renderMono(float* mono, size_t frames); // Offline audition helper.
  void render(float* interleaved, size_t frames);
  bool active() const { return active_; }
  float envelopeLevel() const;
 private:
  void renderChunk(float* left, float* right, size_t frames);
  std::unique_ptr<MonkSynthEngine, decltype(&monk_synth_free)> synth_{nullptr, monk_synth_free};
  InstrumentMonk previous_{};
  float frequency_ = 440.0f, sampleRate_ = 48000.0f;
  float gain_ = 1.0f, currentGain_ = 1.0f, gainCoefficient_ = 1.0f;
  size_t silentFrames_ = 0;
  bool active_ = false, configured_ = false, released_ = false;
};
