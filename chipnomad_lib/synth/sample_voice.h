#ifndef CHOOCHOO_SAMPLE_VOICE_H
#define CHOOCHOO_SAMPLE_VOICE_H

#include <stddef.h>
#include <stdint.h>

struct InstrumentSample;

class SampleVoice {
 public:
  void init(float outputSampleRate);
  void configure(const InstrumentSample* sample, float pitchCents, float gain,
                 uint8_t start, uint8_t end, uint16_t cutoffHz,
                 uint8_t resonance);
  void noteOn();
  void noteOff();
  void kill();
  void render(float* output, size_t frames);
  bool active() const { return active_; }

 private:
  enum class EnvelopeStage : uint8_t { idle, attack, decay, sustain, release };

  void enterEnvelopeStage(EnvelopeStage stage);
  float processEnvelope();
  void updateFilter();
  float processFilter(float input, int channel, int stage);

  const InstrumentSample* sample_;
  float outputSampleRate_;
  double position_;
  double step_;
  uint32_t startFrame_;
  uint32_t endFrame_;
  bool active_;
  float gain_;
  uint16_t cutoffHz_;
  uint8_t resonance_;

  EnvelopeStage envelopeStage_;
  float envelopeLevel_;
  float envelopeIncrement_;
  uint32_t envelopeSamplesLeft_;

  float filterG_;
  float filterK_;
  float filterA1_;
  float filterA2_;
  float filterA3_;
  float filterIc1eq_[2][2];
  float filterIc2eq_[2][2];
};

int sampleLoadWav16(const char* path, InstrumentSample* sample,
                    char* error, size_t errorSize);

#endif
