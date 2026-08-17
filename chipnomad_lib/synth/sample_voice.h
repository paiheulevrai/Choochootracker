#ifndef CHOOCHOO_SAMPLE_VOICE_H
#define CHOOCHOO_SAMPLE_VOICE_H

#include <stddef.h>
#include <stdint.h>
#include "voice_post_processor.h"

struct InstrumentSample;

class SampleVoice {
 public:
  void init(float outputSampleRate);
  void configure(const InstrumentSample* sample, float pitchCents, float gain,
                 float speedPercent, uint8_t start, uint8_t end, uint8_t loopMode, uint16_t cutoffHz,
                 uint8_t resonance);
  void noteOn();
  void noteOff();
  void kill();
  void render(float* output, size_t frames);
  bool active() const { return active_; }
  float envelopeLevel() const { return post_.envelopeLevel(); }

 private:
  const InstrumentSample* sample_;
  float outputSampleRate_;
  double position_;
  double step_;
  int direction_;
  bool reverse_;
  uint8_t loopMode_;
  float timeStretch_;
  bool granular_;
  bool grainExhausted_;
  double grainPosition_[2];
  double nextGrainPosition_;
  uint32_t grainAge_[2];
  uint32_t grainSize_;
  uint32_t grainHop_;
  uint32_t startFrame_;
  uint32_t endFrame_;
  bool active_;
  bool advancePosition();
  float sampleAt(double position, int channel) const;
  float grainSampleAt(double position, int channel) const;
  VoicePostProcessor<> post_;
};

int sampleLoadWav16(const char* path, InstrumentSample* sample,
                    char* error, size_t errorSize);

#endif
