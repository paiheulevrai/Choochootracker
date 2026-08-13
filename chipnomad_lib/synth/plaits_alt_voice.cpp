#include "plaits_alt_voice.h"

#include <math.h>
#include <string.h>

#include "stmlib/utils/buffer_allocator.h"

void PlaitsAltVoice::init(float outputSampleRate) {
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
  active_ = gate_ = triggerPending_ = false;
  envelopeMode_ = 0;
  gain_ = 1.0f;
  quietSamples_ = 0;

  filter_.init(outputSampleRate_);

  envelope_.init(outputSampleRate_);
}

void PlaitsAltVoice::configure(uint8_t engine, uint16_t harmonics,
                            uint16_t timbre, uint16_t morph, uint8_t auxMix,
                            uint8_t envelopeMode, uint8_t decay,
                            uint8_t sustain, float note, float gain) {
  patch_.engine = engine > 23 ? 23 : engine;
  patch_.harmonics = harmonics / 32767.0f;
  patch_.timbre = timbre / 32767.0f;
  patch_.morph = morph / 32767.0f;
  // LEVEL is retained as a file-format value but behaves as click-free VCA.
  envelopeMode_ = envelopeMode == 0 ? 0 : 2;
  patch_.decay = envelopeMode_ == 0 ? decay / 255.0f : 0.5f;
  patch_.lpg_colour = envelopeMode_ == 0 ? sustain / 255.0f : 0.5f;
  // VCA uses a constant patched level to bypass Plaits' internal LPG. Our
  // sample-rate envelope below then controls amplitude without block clicks.
  modulations_.level_patched = envelopeMode_ == 2;
  patch_.note = note;
  auxMix_ = auxMix;
  gain_ = gain < 0.0f ? 0.0f : (gain > 1.0f ? 1.0f : gain);
}

void PlaitsAltVoice::setFilter(bool enabled, uint8_t mode, bool slope24dB,
                            float cutoffHz, float resonance) {
  filter_.configure(enabled, mode, slope24dB, cutoffHz, resonance);
}

void PlaitsAltVoice::setEnvelope(float attackSeconds, float decaySeconds,
                              float sustain, float releaseSeconds, uint8_t shape) {
  envelope_.configure(attackSeconds, decaySeconds, sustain, releaseSeconds, shape);
}

void PlaitsAltVoice::noteOn() {
  active_ = gate_ = true;
  triggerPending_ = true;
  quietSamples_ = 0;
  if (envelopeMode_ == 0) {
    return;
  }
  envelope_.noteOn();
}

void PlaitsAltVoice::noteOff() {
  gate_ = false;
  modulations_.trigger = 0.0f;
  if (envelopeMode_ == 0) return;
  envelope_.noteOff();
}

void PlaitsAltVoice::kill() {
  active_ = gate_ = false;
  triggerPending_ = false;
  modulations_.trigger = 0.0f;
  envelope_.kill();
  quietSamples_ = 0;
}

void PlaitsAltVoice::renderBlock() {
  // patch.note is the base MIDI note. modulations.note is an offset and must
  // not repeat the base note, or Plaits adds it twice.
  modulations_.note = 0.0f;
  modulations_.trigger = triggerPending_ ? 1.0f : 0.0f;
  triggerPending_ = false;
  modulations_.level = 1.0f;
  voice_.Render(patch_, modulations_, frames_, kBlockSize);
  blockPosition_ = 0;
}

float PlaitsAltVoice::nextSourceSample() {
  if (blockPosition_ >= kBlockSize) renderBlock();
  const plaits_alt::Voice::Frame& frame = frames_[blockPosition_++];
  float main = frame.out / 32768.0f;
  float aux = frame.aux / 32768.0f;
  float mix = auxMix_ / 255.0f;
  return main + (aux - main) * mix;
}

void PlaitsAltVoice::render(float* output, size_t samples) {
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
    float envelope = envelopeMode_ == 0 ? 1.0f : envelope_.next();
    output[i] = sample * (envelopeMode_ == 2 ? envelope : 1.0f) * gain_;
    if (envelopeMode_ == 2 && !envelope_.active()) {
      active_ = false;
      memset(output + i + 1, 0, (samples - i - 1) * sizeof(float));
      break;
    }
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
