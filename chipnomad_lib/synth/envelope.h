#ifndef CHOOCHOO_ENVELOPE_H
#define CHOOCHOO_ENVELOPE_H

#include <stdint.h>
#include <math.h>

class Envelope {
 public:
  enum class Stage : uint8_t { idle, attack, decay, sustain, release };

  void init(float sampleRate) { sampleRate_ = sampleRate; kill(); }
  void configure(float attack, float decay, float sustain, float release,
                 uint8_t shape) {
    attack_ = attack < 0.0f ? 0.0f : attack;
    decay_ = decay < 0.0f ? 0.0f : decay;
    sustain_ = sustain < 0.0f ? 0.0f : (sustain > 1.0f ? 1.0f : sustain);
    release_ = release < 0.0f ? 0.0f : release;
    shape_ = shape;
  }
  // A retrigger resumes from the instantaneous level. Resetting to zero here
  // creates a full-scale step when a monophonic voice receives consecutive notes.
  void noteOn() { enter(Stage::attack); }
  void noteOff() { if (stage_ != Stage::idle) enter(Stage::release); }
  // Used by live sample-envelope controls; keep the current level on retiming.
  void updateDecaySustain(float decay, float sustain) {
    if (decay_==decay && sustain_==sustain) return;
    decay_=decay; sustain_=sustain;
    if (stage_==Stage::decay) enter(Stage::decay);
    else if (stage_==Stage::sustain) level_=sustain_;
  }
  void updateRelease(float release) {
    if (release_==release) return;
    release_=release;
    if (stage_==Stage::release) enter(Stage::release);
  }
  void kill() { stage_ = Stage::idle; level_ = 0.0f; start_ = 0.0f; samplesLeft_ = 0; }
  float next() {
    if (samplesLeft_) {
      float progress = 1.0f - (float)samplesLeft_ / (float)samplesTotal_;
      level_ = start_ + (target_ - start_) * curve(progress);
      if (!--samplesLeft_) {
        level_ = target_;
        if (stage_ == Stage::attack) enter(Stage::decay);
        else if (stage_ == Stage::decay) enter(Stage::sustain);
        else if (stage_ == Stage::release) enter(Stage::idle);
      }
    }
    return level_;
  }
  Stage stage() const { return stage_; }
  bool active() const { return stage_ != Stage::idle; }
  float level() const { return level_; }

 protected:
  virtual float curve(float x) const {
    // 00 = logarithmic, 80 = linear, FF = exponential.
    if (shape_ <= 0x80) return sqrtf(x) + (x - sqrtf(x)) * (shape_ / 128.0f);
    return x + (x * x - x) * ((shape_ - 0x80) / 127.0f);
  }
  void enter(Stage stage) {
    stage_ = stage;
    float seconds = 0.0f;
    if (stage == Stage::attack) { target_ = 1.0f; seconds = attack_; }
    else if (stage == Stage::decay) { target_ = sustain_; seconds = decay_; }
    else if (stage == Stage::release) { target_ = 0.0f; seconds = release_; }
    else if (stage == Stage::sustain) { level_ = sustain_; return; }
    else { level_ = 0.0f; return; }
    start_ = level_;
    samplesTotal_ = samplesLeft_ = (uint32_t)(seconds * sampleRate_);
    if (!samplesLeft_) { level_ = target_; if (stage == Stage::attack) enter(Stage::decay); else if (stage == Stage::decay) enter(Stage::sustain); else enter(Stage::idle); }
  }

 private:
  float sampleRate_ = 96000.0f, attack_ = 0.0f, decay_ = 0.0f, sustain_ = 1.0f, release_ = 0.0f;
  float level_ = 0.0f, start_ = 0.0f, target_ = 0.0f;
  uint32_t samplesLeft_ = 0, samplesTotal_ = 0;
  uint8_t shape_ = 0x80;
  Stage stage_ = Stage::idle;
};

#endif
