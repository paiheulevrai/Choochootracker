#ifndef CHOOCHOO_DRUM_SYNTH_VOICE_H
#define CHOOCHOO_DRUM_SYNTH_VOICE_H

#include <stddef.h>
#include <stdint.h>
#include "voice_post_processor.h"
#include "../project_instruments.h"
#include "../external/digisophie/sophie.h"

class DrumSynthVoice {
 public:
  void init(float sampleRate);
  void configure(const InstrumentDrumSynth* instrument, float pitchCents, float gain,
                 uint16_t cutoffHz, uint8_t resonance, bool digiSophie = false);
  void noteOn();
  void noteOff();
  void kill();
  void render(float* output, size_t frames);
  bool active() const { return active_; }
  float envelopeLevel() const { return active_ ? envelope_ * (digiSophie_ ? post_.envelopeLevel() : 1.0f) : 0.0f; }

 private:
  float random();
  float oscillator(float frequency, int shape, int slot);
  float sophieSample();
  void renderClap(float* output, size_t frames);
  const InstrumentDrumSynth* instrument_;
  InstrumentDrumSynth parameters_;
  VoicePostProcessor<> post_;
  float sampleRate_, frequency_, age_, duration_, envelope_, phase_[8], noiseLow_;
  uint32_t randomState_;
  bool active_;
  ds_voice sophie_;
  ds_params sophieParams_;
  int32_t sophieBuffer_[DS_BLOCK_SIZE];
  size_t sophieRead_;
  float sophiePosition_, sophieLeft_, sophieRight_, sophieDecay_;
  bool digiSophie_, sophieTrigger_;
  int clapSpacing_, clapCountdown_, clapRemaining_;
  float clapSlap_, clapTail_, clapSlapDecay_, clapTailDecay_;
  float clapLow_, clapHigh_, clapLowAlpha_, clapHighAlpha_, clapMakeup_;
};

#endif
