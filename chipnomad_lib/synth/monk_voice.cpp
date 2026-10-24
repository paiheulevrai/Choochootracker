#include "monk_voice.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

void MonkSynthVoice::init(float sampleRate, const MonkSynthVoice* prototype) {
  const float rate = monk_sanitize_sample_rate(sampleRate);
  const bool canCopy = prototype && prototype->synth_ && prototype->sampleRate_ == rate;
  if (!synth_) {
    if (canCopy) {
      auto* copy = static_cast<MonkSynthEngine*>(std::malloc(sizeof(MonkSynthEngine)));
      if (copy) std::memcpy(copy, prototype->synth_.get(), sizeof(MonkSynthEngine));
      synth_.reset(copy);
    } else synth_.reset(monk_synth_new(rate));
  } else {
    if (sampleRate_ != rate) {
      if (canCopy) std::memcpy(synth_.get(), prototype->synth_.get(), sizeof(MonkSynthEngine));
      else monk_synth_set_sample_rate(synth_.get(), rate);
    }
    monk_synth_reset(synth_.get());
  }
  sampleRate_ = rate;
  gainCoefficient_ = 1.0f - std::exp(-1.0f / (0.005f * sampleRate_));
  active_ = configured_ = released_ = false;
  silentFrames_ = 0;
}

void MonkSynthVoice::unload() {
  synth_.reset();
  active_ = configured_ = released_ = false;
  silentFrames_ = 0;
}

void MonkSynthVoice::configure(const InstrumentMonk* p, float pitchCents, float gain) {
  if (!p || !synth_) { kill(); return; }
  auto* s = synth_.get();
  const float hz = 440.0f * std::pow(2.0f, (pitchCents - 6900.0f) / 1200.0f);
  const bool glideChanged = !configured_ || previous_.glide != p->glide;
  const bool pitchChanged = !configured_ || frequency_ != hz || glideChanged;
  gain_ = std::clamp(gain, 0.0f, 1.0f);
  if (!active_) currentGain_ = gain_;
  monk_synth_set_glide(s, p->glide / 255.0f);
  if (!configured_ || previous_.unison != p->unison)
    monk_synth_set_unison(s, std::clamp(int(p->unison), 1, 10));
  if (!configured_ || previous_.detune != p->detune)
    monk_synth_set_unison_detune(s, p->detune * (50.0f / 255.0f));
  monk_synth_set_voice(s, p->character / 255.0f);
  monk_synth_set_unison_voice_spread(s, p->voiceSpread * (0.5f / 255.0f));
  if (!configured_ || previous_.vowel != p->vowel || glideChanged)
    monk_synth_set_vowel(s, p->vowel / 255.0f);
  monk_synth_set_aspiration(s, p->breath / 255.0f);
  monk_synth_set_vibrato(s, p->vibrato / 255.0f);
  monk_synth_set_vibrato_rate(s, p->rate / 255.0f);
  monk_synth_set_attack(s, p->attack * (5.0f / 255.0f));
  monk_synth_set_decay(s, p->decay * (5.0f / 255.0f));
  monk_synth_set_sustain(s, p->sustain / 255.0f);
  monk_synth_set_release(s, p->release * (5.0f / 255.0f));
  monk_synth_set_delay_mix(s, p->delayMix / 255.0f);
  monk_synth_set_delay_rate(s, p->delayRate / 255.0f);
  monk_synth_set_level(s, p->level / 255.0f);
  s->last_base_hz = hz;
  if (pitchChanged) for (int n = 0; n < s->unison_count; ++n) {
    float offset = s->unison_count > 1 ? s->unison_detune * (2.0f * n / (s->unison_count - 1) - 1.0f) : 0;
    monk_voice_set_pitch_target(&s->voices[n], hz * std::pow(2.0f, offset / 1200.0f));
  }
  // A count change during a release must not create newly sustained singers.
  if (released_) for (auto& voice : s->voices) monk_voice_note_off(&voice);
  frequency_ = hz; previous_ = *p; configured_ = true;
}

void MonkSynthVoice::noteOn() {
  if (!synth_ || !configured_) return;
  auto* s = synth_.get();
  for (int n = 0; n < s->unison_count; ++n) {
    float offset = s->unison_count > 1 ? s->unison_detune * (2.0f * n / (s->unison_count - 1) - 1.0f) : 0;
    float hz = frequency_ * std::pow(2.0f, offset / 1200.0f);
    monk_voice_note_on(&s->voices[n], hz, 1.0f);
    monk_voice_set_pitch_target(&s->voices[n], hz);
  }
  active_ = true; released_ = false; silentFrames_ = 0;
}
void MonkSynthVoice::noteOff() {
  released_ = true;
  if (synth_) for (auto& voice : synth_->voices) monk_voice_note_off(&voice);
}
void MonkSynthVoice::kill() {
  if (active_ && synth_) monk_synth_reset(synth_.get());
  active_ = false; released_ = false; silentFrames_ = 0;
}
float MonkSynthVoice::envelopeLevel() const {
  if (!active_) return 0;
  float level = 0;
  for (const auto& v : synth_->voices) level = std::max(level, v.env_level);
  return level;
}
void MonkSynthVoice::renderChunk(float* left, float* right, size_t frames) {
  if (!active_ || !synth_) {
    std::fill_n(left, frames, 0); std::fill_n(right, frames, 0); return;
  }
  monk_synth_process(synth_.get(), left, right, uint32_t(frames));
  bool sounding = monk_synth_is_active(synth_.get()) != 0;
  for (size_t i = 0; i < frames; ++i) {
    currentGain_ += gainCoefficient_ * (gain_ - currentGain_);
    left[i] *= currentGain_; right[i] *= currentGain_;
    if (sounding || std::abs(left[i]) + std::abs(right[i]) > 1e-6f) silentFrames_ = 0;
    else ++silentFrames_;
  }
  // Drain a full grain and, when enabled, a delay interval before retiring the tail.
  size_t quiet = MONK_MAX_GRAIN;
  if (previous_.delayMix) quiet += size_t(std::max(synth_->delay.target_delay_l, synth_->delay.target_delay_r));
  if (silentFrames_ > quiet) kill();
}
void MonkSynthVoice::render(float* output, size_t frames) {
  if (!output) return;
  float left[32], right[32];
  while (frames) {
    size_t n = std::min(frames, size_t(32)); renderChunk(left, right, n);
    for (size_t i = 0; i < n; ++i) { output[2*i] = left[i]; output[2*i+1] = right[i]; }
    output += n * 2; frames -= n;
  }
}
void MonkSynthVoice::renderMono(float* output, size_t frames) {
  if (!output) return;
  float left[32], right[32];
  while (frames) {
    size_t n = std::min(frames, size_t(32)); renderChunk(left, right, n);
    for (size_t i = 0; i < n; ++i) output[i] = (left[i] + right[i]) * 0.5f;
    output += n; frames -= n;
  }
}
