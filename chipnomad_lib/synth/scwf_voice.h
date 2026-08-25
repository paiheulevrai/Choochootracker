#ifndef CHOOCHOO_SCWF_VOICE_H
#define CHOOCHOO_SCWF_VOICE_H

#include <stddef.h>
#include <stdint.h>
#include "voice_post_processor.h"

struct InstrumentSCWF;

#define SCWF_DETUNE_MAX 149
int scwfDetuneCents(uint8_t value);
double scwfFrequencyHz(int midiCents);

class SCWFVoice {
 public:
  void init(float outputSampleRate);
  void configure(const InstrumentSCWF* instrument, float pitchCents, float gain,
                 int detuneCents, uint8_t mix, uint16_t cutoffHz, uint8_t resonance,
                 const uint16_t* frameSize = NULL, const uint8_t* frameIndex = NULL,
                 int attack = -1, int decay = -1, int sustain = -1,
                 int release = -1, int envelopeShape = -1);
  void noteOn();
  void noteOff();
  void kill();
  void render(float* output, size_t frames);
  bool active() const { return active_; }
  float envelopeLevel() const { return post_.envelopeLevel(); }

 private:
  const InstrumentSCWF* instrument_;
  float outputSampleRate_;
  double phase_[2];
  double step_[2];
  uint32_t frameStart_[2];
  uint32_t cycleFrames_[2];
  float frameBlend_[2];
  float mix_;
  bool active_;
  float sampleAt(int oscillator, double phase) const;
  VoicePostProcessor<> post_;
};

#endif
