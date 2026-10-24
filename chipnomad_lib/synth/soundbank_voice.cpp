#include "soundbank_voice.h"

#include <algorithm>
#include <cmath>
void SoundbankVoice::init(float rate) {
  rate_ = rate;
  kill();
  lastOutput_ = transitionOffset_ = {};
  transitionFrames_ = 0;
  transitionPending_ = false;
  transitionLength_ = std::max(2u, uint32_t(rate * .003f));
  for (auto& l : layers_) {
    l.envelope.init(rate);
    l.filter.init(rate);
  }
  handle_ = 0;
  sequence_ = 0;
}
void SoundbankVoice::kill() {
  noteHeld_ = false;
  for (auto& l : layers_) {
    if (l.zone) transitionPending_ = true;
    l.zone = nullptr;
    l.envelope.kill();
  }
}
void SoundbankVoice::stopImmediately() {
  kill();
  lastOutput_ = transitionOffset_ = {};
  transitionFrames_ = 0;
  transitionPending_ = false;
}
bool SoundbankVoice::active() const {
  if (transitionFrames_ || (transitionPending_ &&
                           (lastOutput_[0] != 0 || lastOutput_[1] != 0))) return true;
  for (auto& l : layers_)
    if (l.zone) return true;
  return false;
}
float SoundbankVoice::envelopeLevel() const {
  float v = 0;
  for (auto& l : layers_)
    if (l.zone) v = std::max(v, l.envelope.level());
  return v;
}
void SoundbankVoice::configure(uint32_t handle, float cents, float gain, int velocity,
                               float cutoff, const SoundbankControls& controls) {
  if (handle_ != handle) {
    kill();
    handle_ = handle;
    sequence_ = 0;
  }
  cents_ = cents;
  gain_ = std::max(0.f, gain);
  velocity_ = std::clamp(velocity, 0, 127);
  cutoff_ = cutoff;
  controls_ = controls;
  for (auto& l : layers_)
    if (l.zone) updateLayer(l);
}
bool SoundbankVoice::matches(const soundbank::Zone& z, bool release) const {
  auto sequence = release ? noteSequence_ : sequence_;
  auto velocity = release ? noteVelocity_ : velocity_;
  uint32_t seed = (sequence + 1) * 747796405u + 2891336453u;
  seed = ((seed >> ((seed >> 28) + 4)) ^ seed) * 277803737u;
  double random = double((seed >> 22) ^ seed) / 4294967296.0;
  int key = release ? noteKey_ : int(std::lround(cents_ / 100.f));
  return random >= z.randomLow && random < z.randomHigh && key >= z.loKey && key <= z.hiKey &&
         velocity >= z.loVel && velocity <= z.hiVel &&
         sequence % z.sequenceLength == uint32_t(z.sequencePosition - 1);
}
void SoundbankVoice::updateLayer(Layer& l) {
  auto& z = l.zone->zone;
  l.step = l.zone->audio->sampleRate / rate_ *
           pow(2.0, ((cents_ - z.root * 100) * z.keytrack / 100 + z.tune + l.tune + controls_.tuneCents) / 1200.0);
  l.step = std::clamp(l.step, 1.0 / 65536, 65536.0);
  const float cutoff = std::min(controls_.cutoffHz < 0 ? z.cutoff : float(controls_.cutoffHz), cutoff_);
  const float resonance = controls_.resonanceTenths < 0 ? z.resonance : controls_.resonanceTenths / 10.f;
  l.filter.configure(cutoff < 19999, 0, 0, false, cutoff,
                     std::clamp(resonance / 40.f, 0.f, 1.f));
  if (l.choked) return; // Exclusive-group fades keep their authored choke time.
  l.decay = controls_.decayMs < 0 ? z.envelope.decay : controls_.decayMs / 1000.f;
  l.sustain = controls_.sustainPercent < 0 ? z.envelope.sustain : controls_.sustainPercent / 100.f;
  l.release = controls_.releaseMs < 0 ? z.envelope.release : controls_.releaseMs / 1000.f;
  l.envelope.updateRelease(l.release);
  if (!l.attacking && !l.holding) l.envelope.updateDecaySustain(l.decay,l.sustain);
}
void SoundbankVoice::startLayer(Layer& l, const soundbank::PreparedZone& z, size_t index) {
  l.zone = &z;
  l.position = z.zone.reverse ? z.zone.end - 1 : z.zone.start;
  l.released = false;
  l.choked = false;
  l.holding = false;
  l.attacking = true;
  auto time = [](int ms, float bank) { return ms < 0 ? bank : ms / 1000.f; };
  l.delay = uint32_t(time(controls_.delayMs, z.zone.envelope.delay) * rate_);
  l.hold = uint32_t(time(controls_.holdMs, z.zone.envelope.hold) * rate_);
  l.decay = time(controls_.decayMs, z.zone.envelope.decay);
  l.sustain = controls_.sustainPercent < 0 ? z.zone.envelope.sustain : controls_.sustainPercent / 100.f;
  l.release = time(controls_.releaseMs, z.zone.envelope.release);
  l.filter.reset();
  l.envelope.kill();
  l.envelope.configure(time(controls_.attackMs, z.zone.envelope.attack), 0, 1, l.release, 128);
  l.envelope.noteOn();
  float v = noteVelocity_ / 127.f;
  if (z.zone.velocityTrack < 0) v = 1 - v;
  uint32_t seed = (sequence_ + 1) * 1664525u + uint32_t(index) * 1013904223u;
  seed ^= seed >> 16;
  float random = (seed & 65535) / 32767.5f - 1;
  l.tune = random * z.zone.pitchRandom;
  l.gain =
      z.zone.gain *
      (z.zone.customVelocityCurve ? z.zone.velocityCurve[noteVelocity_]
                                  : powf(std::max(.0001f, v), fabsf(z.zone.velocityTrack) / 50.f)) *
      powf(10, random * z.zone.ampRandom / 20);
  if (z.zone.fadeInHi > z.zone.fadeInLo)
    l.gain *= sqrtf(std::clamp(
        (noteVelocity_ - z.zone.fadeInLo) / float(z.zone.fadeInHi - z.zone.fadeInLo), 0.f, 1.f));
  if (z.zone.fadeOutHi > z.zone.fadeOutLo)
    l.gain *= sqrtf(std::clamp(
        (z.zone.fadeOutHi - noteVelocity_) / float(z.zone.fadeOutHi - z.zone.fadeOutLo), 0.f, 1.f));
  l.left = z.zone.pan > 0 ? 1 - z.zone.pan : 1;
  l.right = z.zone.pan < 0 ? 1 + z.zone.pan : 1;
  updateLayer(l);
}
void SoundbankVoice::noteOn() {
  kill();
  transitionPending_ = true;
  auto p = soundbank::prepared(handle_);
  if (!p) return;
  noteHeld_ = true;
  noteKey_ = int(std::lround(cents_ / 100.f));
  noteVelocity_ = velocity_;
  noteSequence_ = sequence_;
  size_t index = 0;
  for (auto n : p->keys[std::clamp(int(std::lround(cents_ / 100.f)), 0, 127)]) {
    const auto& z = p->zones[n];
    if (z.zone.releaseTrigger || !matches(z.zone)) continue;
    if (index == maxLayers) break;
    auto& l = layers_[index++];
    startLayer(l, z, index);
  }
  ++sequence_;
}
void SoundbankVoice::noteOff() {
  if (!noteHeld_) return;
  noteHeld_ = false;
  for (auto& l : layers_)
    if (l.zone && (l.zone->zone.loop != soundbank::Loop::oneShot || controls_.releaseMs >= 0)) {
      if (l.release <= 0) transitionPending_ = true;
      l.released = true;
      l.attacking = l.holding = false;
      l.delay = l.hold = 0;
      l.envelope.noteOff();
    }
  auto p = soundbank::prepared(handle_);
  if (!p) return;
  size_t index = 0;
  for (auto n : p->keys[std::clamp(noteKey_, 0, 127)]) {
    const auto& z = p->zones[n];
    if (!z.zone.releaseTrigger || !matches(z.zone, true)) continue;
    while (index < maxLayers && layers_[index].zone) ++index;
    if (index == maxLayers) break;
    transitionPending_ = true;
    startLayer(layers_[index], z, index + 1);
    ++index;
  }
}
void SoundbankVoice::choke(int group, uint32_t handle) {
  if (!group || handle_ != handle) return;
  for (auto& l : layers_)
    if (l.zone && l.zone->zone.offBy == group) {
      l.choked = true;
      l.released = true;
      l.attacking = false;
      l.delay = l.hold = 0;
      l.envelope.configure(0, 0, 1, l.zone->zone.offTime, 128);
      l.envelope.noteOff();
    }
}
void SoundbankVoice::chokeFrom(const SoundbankVoice& in) {
  auto p = soundbank::prepared(in.handle_);
  if (!p) return;
  for (auto n : p->keys[std::clamp(int(std::lround(in.cents_ / 100.f)), 0, 127)]) {
    const auto& z = p->zones[n];
    if (!z.zone.releaseTrigger && in.matches(z.zone)) choke(z.zone.group, in.handle_);
  }
}
void SoundbankVoice::render(float* out, size_t frames) {
  for (size_t f = 0; f < frames; ++f) {
    float left = 0, right = 0;
    for (auto& l : layers_) {
      if (!l.zone) continue;
      auto& z = l.zone->zone;
      auto& audio = *l.zone->audio;
      if (l.delay) {
        --l.delay;
        continue;
      }
      if (!l.envelope.active()) {
        l.zone = nullptr;
        continue;
      }
      float env = l.envelope.level();
      if (l.attacking && l.envelope.stage() == Envelope::Stage::sustain) {
        l.attacking = false;
        l.holding = true;
      }
      if (l.holding && !l.released) {
        if (l.hold) {
          --l.hold;
          env = 1;
        } else {
          l.holding = false;
          l.envelope.configure(0, l.decay, l.sustain, l.release, 128);
          l.envelope.noteOn();
          env = l.envelope.next();
        }
      } else
        env = l.envelope.next();
      bool loop = (z.loop == soundbank::Loop::continuous ||
                   (z.loop == soundbank::Loop::sustain && !l.released));
      if (l.position >= z.end || l.position < ((z.loop == soundbank::Loop::continuous ||
                                                z.loop == soundbank::Loop::sustain)
                                                   ? std::min(z.start, z.loopStart)
                                                   : z.start)) {
        l.zone = nullptr;
        continue;
      }
      auto a = uint32_t(l.position);
      auto b = a + 1;
      float t = float(l.position - a);
      if (loop && b >= z.loopEnd && a >= z.loopStart) b = z.loopStart;
      if (b >= z.end) b = z.end - 1;
      auto sample = [&](int c) {
        c = std::min(c, int(audio.channels) - 1);
        float x = audio.pcm[size_t(a) * audio.channels + c] / 32768.f;
        float y = audio.pcm[size_t(b) * audio.channels + c] / 32768.f;
        return x + (y - x) * t;
      };
      float volume = env * l.gain * gain_;
      left += l.filter.process(sample(0), 0) * volume * l.left;
      right += l.filter.process(sample(1), 1) * volume * l.right;
      l.position += z.reverse ? -l.step : l.step;
      if (loop) {
        double length = z.loopEnd - z.loopStart;
        if (!z.reverse && l.position >= z.loopEnd)
          l.position = z.loopStart + fmod(l.position - z.loopStart, length);
        else if (z.reverse && l.position < z.loopStart)
          l.position = z.loopEnd - 1 - fmod(z.loopStart - l.position - 1, length);
      }
    }
    if (transitionPending_) {
      transitionOffset_ = {lastOutput_[0] - left, lastOutput_[1] - right};
      transitionFrames_ = (transitionOffset_[0] != 0 || transitionOffset_[1] != 0)
                              ? transitionLength_ : 0;
      transitionPending_ = false;
    }
    if (transitionFrames_) {
      const float weight = float(transitionFrames_ - 1) / (transitionLength_ - 1);
      left += transitionOffset_[0] * weight;
      right += transitionOffset_[1] * weight;
      --transitionFrames_;
    }
    lastOutput_ = {left, right};
    out[f * 2] = left;
    out[f * 2 + 1] = right;
  }
}
