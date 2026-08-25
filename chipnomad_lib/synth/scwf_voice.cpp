#include "scwf_voice.h"

#include "../project_instruments.h"
#include <math.h>
#include <string.h>

static float scwfEnvelopeTime(uint8_t value) {
  float normalized = value / 255.0f;
  return normalized * normalized * 5.0f;
}

int scwfDetuneCents(uint8_t value) {
  if (!value) return 0;
  if (value <= 127) return (int)(pow(200.0, (value - 1) / 126.0) + 0.5);
  return (value - 125) * 100;
}

double scwfFrequencyHz(int midiCents) {
  return 440.0 * pow(2.0, (midiCents - 6900) / 1200.0);
}

void SCWFVoice::init(float outputSampleRate) {
  instrument_ = nullptr;
  outputSampleRate_ = outputSampleRate;
  phase_[0] = phase_[1] = 0.0;
  step_[0] = step_[1] = 0.0;
  frameStart_[0] = frameStart_[1] = 0;
  cycleFrames_[0] = cycleFrames_[1] = 0;
  frameBlend_[0] = frameBlend_[1] = 0.0f;
  mix_ = 0.5f;
  active_ = false;
  post_.init(outputSampleRate);
}

void SCWFVoice::configure(const InstrumentSCWF* instrument, float pitchCents,
                          float gain, int detuneCents, uint8_t mix,
                          uint16_t cutoffHz, uint8_t resonance,
                          const uint16_t* frameSize, const uint8_t* frameIndex,
                          int attack, int decay, int sustain, int release, int envelopeShape) {
  instrument_ = instrument;
  post_.setGain(gain);
  mix_ = mix / 255.0f;
  if (!instrument_) return;
  // A one-cycle WAV is read exactly once per oscillator period. Its file
  // sample rate is deliberately irrelevant: A4 (6900 MIDI cents) is 440 wraps/s.
  const double baseHz = scwfFrequencyHz((int)pitchCents);
  for (int i = 0; i < 2; ++i) {
    const InstrumentSample& oscillator = instrument_->oscillator[i];
    cycleFrames_[i] = frameSize && frameSize[i] ? frameSize[i] : oscillator.frameCount;
    if (cycleFrames_[i] > oscillator.frameCount) cycleFrames_[i] = oscillator.frameCount;
    uint32_t tables = cycleFrames_[i] ? oscillator.frameCount / cycleFrames_[i] : 0;
    float position = frameIndex && tables > 1 ? frameIndex[i] * (tables - 1) / 255.0f : 0.0f;
    frameStart_[i] = (uint32_t)position * cycleFrames_[i];
    frameBlend_[i] = position - (uint32_t)position;
    const double cents = i ? detuneCents : 0.0;
    step_[i] = oscillator.data && cycleFrames_[i]
      ? baseHz * pow(2.0, cents / 1200.0) / outputSampleRate_
      : 0.0;
  }
  post_.setFilter(instrument_->filterEnabled != 0, instrument_->filterCharacter, instrument_->filterMode,
                  instrument_->filterSlope24dB != 0, cutoffHz, resonance / 255.0f);
  post_.setEnvelope(true, scwfEnvelopeTime(attack < 0 ? instrument_->attack : attack),
                    scwfEnvelopeTime(decay < 0 ? instrument_->decay : decay),
                    (sustain < 0 ? instrument_->sustain : sustain) / 255.0f,
                    scwfEnvelopeTime(release < 0 ? instrument_->release : release),
                    envelopeShape < 0 ? instrument_->envelopeShape : envelopeShape);
}

void SCWFVoice::noteOn() {
  if (!instrument_ || (!instrument_->oscillator[0].data && !instrument_->oscillator[1].data)) return;
  phase_[0] = phase_[1] = 0.0;
  active_ = true;
  post_.noteOn(true);
}

void SCWFVoice::noteOff() { if (active_) post_.noteOff(); }
void SCWFVoice::kill() { active_ = false; post_.kill(); }

float SCWFVoice::sampleAt(int oscillator, double phase) const {
  const InstrumentSample& source = instrument_->oscillator[oscillator];
  if (!source.data || source.frameCount == 0) return 0.0f;
  phase -= floor(phase);
  uint32_t count = cycleFrames_[oscillator];
  double position = phase * count;
  uint32_t frame = (uint32_t)position;
  uint32_t next = frame + 1 == count ? 0 : frame + 1;
  float fraction = (float)(position - frame);
  float a = source.data[(frameStart_[oscillator] + frame) * source.channels] / 32768.0f;
  float b = source.data[(frameStart_[oscillator] + next) * source.channels] / 32768.0f;
  float value = a + (b - a) * fraction;
  if (!frameBlend_[oscillator]) return value;
  uint32_t nextFrame = frameStart_[oscillator] + count;
  if (nextFrame + count > source.frameCount) nextFrame = 0;
  a = source.data[(nextFrame + frame) * source.channels] / 32768.0f;
  b = source.data[(nextFrame + next) * source.channels] / 32768.0f;
  return value + ((a + (b - a) * fraction) - value) * frameBlend_[oscillator];
}

void SCWFVoice::render(float* output, size_t frames) {
  memset(output, 0, frames * 2 * sizeof(float));
  if (!active_ || !instrument_) return;
  const float blend = mix_;
  for (size_t i = 0; i < frames; ++i) {
    if (!post_.envelopeActive()) { kill(); break; }
    float value = sampleAt(0, phase_[0]) * (1.0f - blend) + sampleAt(1, phase_[1]) * blend;
    for (int channel = 0; channel < 2; ++channel) output[i * 2 + channel] = post_.process(value, channel);
    phase_[0] += step_[0];
    phase_[1] += step_[1];
  }
}
