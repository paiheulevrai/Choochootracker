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
  modulations_.level_patched = false;
  modulations_.level = 1.0f;
  blockPosition_ = kBlockSize;
  outputSampleRate_ = outputSampleRate > 0.0f ? outputSampleRate : 96000.0f;
  sourcePhase_ = 1.0f;
  previousSource_ = currentSource_ = 0.0f;
  auxMix_ = 0;
  active_ = gate_ = false;
  envelopeMode_ = 0;
  gain_ = 1.0f;
  quietSamples_ = 0;

  filter_.init(outputSampleRate_);

  envelopeStage_ = EnvelopeStage::idle;
  envelopeLevel_ = 0.0f;
  attackSeconds_ = decaySeconds_ = releaseSeconds_ = 0.0f;
  sustain_ = 1.0f;
  envelopeIncrement_ = 0.0f;
  envelopeSamplesLeft_ = 0;
}

void PlaitsVoice::configure(uint8_t engine, uint16_t harmonics,
                            uint16_t timbre, uint16_t morph, uint8_t auxMix,
                            uint8_t envelopeMode, uint8_t decay,
                            uint8_t sustain, float note, float gain) {
  patch_.engine = engine > 23 ? 23 : engine;
  patch_.harmonics = harmonics / 32767.0f;
  patch_.timbre = timbre / 32767.0f;
  patch_.morph = morph / 32767.0f;
  envelopeMode_ = envelopeMode > 2 ? 0 : envelopeMode;
  patch_.decay = envelopeMode_ == 0 ? decay / 255.0f : 0.5f;
  patch_.lpg_colour = envelopeMode_ == 0 ? sustain / 255.0f : 0.5f;
  modulations_.level_patched = envelopeMode_ != 0;
  patch_.note = note;
  auxMix_ = auxMix;
  gain_ = gain < 0.0f ? 0.0f : (gain > 1.0f ? 1.0f : gain);
}

void PlaitsVoice::setFilter(bool enabled, uint8_t mode, bool slope24dB,
                            float cutoffHz, float resonance) {
  filter_.configure(enabled, mode, slope24dB, cutoffHz, resonance);
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
  quietSamples_ = 0;
  modulations_.trigger = 1.0f;
  if (envelopeMode_ == 0) {
    envelopeLevel_ = 1.0f;
    envelopeStage_ = EnvelopeStage::sustain;
    envelopeSamplesLeft_ = 0;
    return;
  }
  envelopeLevel_ = 0.0f;
  enterEnvelopeStage(EnvelopeStage::attack);
}

void PlaitsVoice::noteOff() {
  gate_ = false;
  modulations_.trigger = 0.0f;
  if (envelopeMode_ == 0) return;
  enterEnvelopeStage(EnvelopeStage::release);
}

void PlaitsVoice::kill() {
  active_ = gate_ = false;
  modulations_.trigger = 0.0f;
  envelopeStage_ = EnvelopeStage::idle;
  envelopeLevel_ = 0.0f;
  quietSamples_ = 0;
}

void PlaitsVoice::renderBlock() {
  // patch.note is the base MIDI note. modulations.note is an offset and must
  // not repeat the base note, or Plaits adds it twice.
  modulations_.note = 0.0f;
  modulations_.trigger = gate_ ? 1.0f : 0.0f;
  modulations_.level = envelopeMode_ == 1 ? envelopeLevel_ : 1.0f;
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
    sample = filter_.process(sample);
    float envelope = envelopeMode_ == 0 ? 1.0f : processEnvelope();
    output[i] = sample * (envelopeMode_ == 2 ? envelope : 1.0f) * gain_;
    if (envelopeMode_ == 0 && !gate_) {
      quietSamples_ = fabsf(output[i]) < 0.00001f ? quietSamples_ + 1 : 0;
      if (quietSamples_ > (uint32_t)(outputSampleRate_ * 0.25f)) {
        active_ = false;
        memset(output + i + 1, 0, (samples - i - 1) * sizeof(float));
        break;
      }
    }
  }
}
