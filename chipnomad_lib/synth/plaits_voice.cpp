#include "plaits_voice.h"

#include <math.h>
#include <string.h>

#include "stmlib/utils/buffer_allocator.h"

void PlaitsVoice::init(float outputSampleRate) {
  stmlib::BufferAllocator allocator(allocatorMemory_, sizeof(allocatorMemory_));
  voice_.Init(&allocator);
  memset(&patch_, 0, sizeof(patch_));
  memset(&modulations_, 0, sizeof(modulations_));
  patch_.note = 60.0f;
  patch_.harmonics = patch_.timbre = patch_.morph = 0.5f;
  patch_.decay = 0.5f;
  patch_.lpg_colour = 0.5f;
  modulations_.trigger_patched = true;
  modulations_.level_patched = true;
  modulations_.level = 1.0f;
  blockPosition_ = kBlockSize;
  outputSampleRate_ = outputSampleRate > 0.0f ? outputSampleRate : 96000.0f;
  sourcePhase_ = 1.0f;
  previousSource_ = currentSource_ = 0.0f;
  auxMix_ = 0;
  active_ = gate_ = false;
  gain_ = 1.0f;

  filterEnabled_ = false;
  filterMode_ = 0;
  filterSlope24dB_ = false;
  filterCutoffHz_ = 20000.0f;
  filterResonance_ = 0.0f;
  memset(filterIc1eq_, 0, sizeof(filterIc1eq_));
  memset(filterIc2eq_, 0, sizeof(filterIc2eq_));
  updateFilterCoefficients();

  envelopeStage_ = EnvelopeStage::idle;
  envelopeLevel_ = 0.0f;
  attackSeconds_ = decaySeconds_ = releaseSeconds_ = 0.0f;
  sustain_ = 1.0f;
  envelopeIncrement_ = 0.0f;
  envelopeSamplesLeft_ = 0;
}

void PlaitsVoice::configure(uint8_t engine, uint16_t harmonics,
                            uint16_t timbre, uint16_t morph, uint8_t auxMix,
                            float note, float gain) {
  patch_.engine = engine > 23 ? 23 : engine;
  patch_.harmonics = harmonics / 32767.0f;
  patch_.timbre = timbre / 32767.0f;
  patch_.morph = morph / 32767.0f;
  patch_.note = note;
  auxMix_ = auxMix;
  gain_ = gain < 0.0f ? 0.0f : (gain > 1.0f ? 1.0f : gain);
}

void PlaitsVoice::setFilter(bool enabled, uint8_t mode, bool slope24dB,
                            float cutoffHz, float resonance) {
  filterEnabled_ = enabled;
  filterMode_ = mode > 2 ? 0 : mode;
  filterSlope24dB_ = slope24dB;
  filterCutoffHz_ = cutoffHz;
  filterResonance_ = resonance;
  updateFilterCoefficients();
}

void PlaitsVoice::setEnvelope(float attackSeconds, float decaySeconds,
                              float sustain, float releaseSeconds) {
  attackSeconds_ = attackSeconds < 0.0f ? 0.0f : attackSeconds;
  decaySeconds_ = decaySeconds < 0.0f ? 0.0f : decaySeconds;
  sustain_ = sustain < 0.0f ? 0.0f : (sustain > 1.0f ? 1.0f : sustain);
  releaseSeconds_ = releaseSeconds < 0.0f ? 0.0f : releaseSeconds;
}

void PlaitsVoice::enterEnvelopeStage(EnvelopeStage stage) {
  envelopeStage_ = stage;
  float seconds = 0.0f;
  float target = envelopeLevel_;
  if (stage == EnvelopeStage::attack) { seconds = attackSeconds_; target = 1.0f; }
  else if (stage == EnvelopeStage::decay) { seconds = decaySeconds_; target = sustain_; }
  else if (stage == EnvelopeStage::release) { seconds = releaseSeconds_; target = 0.0f; }
  else if (stage == EnvelopeStage::sustain) {
    envelopeLevel_ = sustain_;
    envelopeSamplesLeft_ = 0;
    return;
  } else {
    envelopeLevel_ = 0.0f;
    envelopeSamplesLeft_ = 0;
    active_ = false;
    return;
  }
  envelopeSamplesLeft_ = (uint32_t)(seconds * outputSampleRate_);
  if (!envelopeSamplesLeft_) {
    envelopeLevel_ = target;
    if (stage == EnvelopeStage::attack) enterEnvelopeStage(EnvelopeStage::decay);
    else if (stage == EnvelopeStage::decay) enterEnvelopeStage(EnvelopeStage::sustain);
    else enterEnvelopeStage(EnvelopeStage::idle);
  } else {
    envelopeIncrement_ = (target - envelopeLevel_) / envelopeSamplesLeft_;
  }
}

void PlaitsVoice::noteOn() {
  active_ = gate_ = true;
  modulations_.trigger = 1.0f;
  envelopeLevel_ = 0.0f;
  enterEnvelopeStage(EnvelopeStage::attack);
}

void PlaitsVoice::noteOff() {
  gate_ = false;
  modulations_.trigger = 0.0f;
  enterEnvelopeStage(EnvelopeStage::release);
}

void PlaitsVoice::kill() {
  active_ = gate_ = false;
  modulations_.trigger = 0.0f;
  envelopeStage_ = EnvelopeStage::idle;
  envelopeLevel_ = 0.0f;
}

void PlaitsVoice::renderBlock() {
  modulations_.note = patch_.note;
  modulations_.trigger = gate_ ? 1.0f : 0.0f;
  voice_.Render(patch_, modulations_, frames_, kBlockSize);
  blockPosition_ = 0;
}

float PlaitsVoice::nextSourceSample() {
  if (blockPosition_ >= kBlockSize) renderBlock();
  const plaits::Voice::Frame& frame = frames_[blockPosition_++];
  float main = frame.out / 32768.0f;
  float aux = frame.aux / 32768.0f;
  float mix = auxMix_ / 255.0f;
  return main + (aux - main) * mix;
}

float PlaitsVoice::processEnvelope() {
  if (envelopeSamplesLeft_) {
    envelopeLevel_ += envelopeIncrement_;
    if (!--envelopeSamplesLeft_) {
      if (envelopeStage_ == EnvelopeStage::attack) enterEnvelopeStage(EnvelopeStage::decay);
      else if (envelopeStage_ == EnvelopeStage::decay) enterEnvelopeStage(EnvelopeStage::sustain);
      else if (envelopeStage_ == EnvelopeStage::release) enterEnvelopeStage(EnvelopeStage::idle);
    }
  }
  return envelopeLevel_;
}

void PlaitsVoice::updateFilterCoefficients() {
  if (filterCutoffHz_ < 20.0f) filterCutoffHz_ = 20.0f;
  if (filterCutoffHz_ > outputSampleRate_ * 0.45f) filterCutoffHz_ = outputSampleRate_ * 0.45f;
  if (filterResonance_ < 0.0f) filterResonance_ = 0.0f;
  if (filterResonance_ > 1.0f) filterResonance_ = 1.0f;
  filterG_ = tanf(3.14159265358979323846f * filterCutoffHz_ / outputSampleRate_);
  filterK_ = 1.0f / (0.5f + filterResonance_ * 19.5f);
  filterA1_ = 1.0f / (1.0f + filterG_ * (filterG_ + filterK_));
  filterA2_ = filterG_ * filterA1_;
  filterA3_ = filterG_ * filterA2_;
}

float PlaitsVoice::processFilterStage(float input, int stage) {
  float v3 = input - filterIc2eq_[stage];
  float band = filterA1_ * filterIc1eq_[stage] + filterA2_ * v3;
  float low = filterIc2eq_[stage] + filterA2_ * filterIc1eq_[stage] + filterA3_ * v3;
  float high = input - filterK_ * band - low;
  filterIc1eq_[stage] = 2.0f * band - filterIc1eq_[stage];
  filterIc2eq_[stage] = 2.0f * low - filterIc2eq_[stage];
  return filterMode_ == 1 ? band : (filterMode_ == 2 ? high : low);
}

void PlaitsVoice::render(float* output, size_t samples) {
  if (!output) return;
  if (!active_) { memset(output, 0, samples * sizeof(float)); return; }
  const float sourceStep = kSourceSampleRate / outputSampleRate_;
  for (size_t i = 0; i < samples; ++i) {
    sourcePhase_ += sourceStep;
    while (sourcePhase_ >= 1.0f) {
      sourcePhase_ -= 1.0f;
      previousSource_ = currentSource_;
      currentSource_ = nextSourceSample();
    }
    float sample = previousSource_ + (currentSource_ - previousSource_) * sourcePhase_;
    if (filterEnabled_) {
      sample = processFilterStage(sample, 0);
      if (filterSlope24dB_) sample = processFilterStage(sample, 1);
    }
    output[i] = sample * processEnvelope() * gain_;
  }
}
