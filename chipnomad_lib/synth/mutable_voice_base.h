#ifndef CHOOCHOO_MUTABLE_VOICE_BASE_H
#define CHOOCHOO_MUTABLE_VOICE_BASE_H

#include <math.h>
#include <string.h>

#include "stmlib/utils/buffer_allocator.h"
#include "voice_post_processor.h"

// Plaits and Plaits-Alt share their source scheduling; only the Mutable voice
// type and its Patch/Modulations types differ.
template <typename MutableVoice, typename Patch, typename Modulations>
class MutableVoiceBase {
 public:
  static const size_t kBlockSize = 24;
  static constexpr float kSourceSampleRate = 48000.0f;

  void init(float outputSampleRate = 96000.0f) {
    stmlib::BufferAllocator allocator(allocatorMemory_, sizeof(allocatorMemory_));
    voice_.Init(&allocator);
    memset(&patch_, 0, sizeof(patch_));
    memset(&modulations_, 0, sizeof(modulations_));
    patch_.note = 60.0f;
    patch_.harmonics = patch_.timbre = patch_.morph = 0.5f;
    patch_.decay = patch_.lpg_colour = 0.5f;
    modulations_.trigger_patched = true;
    modulations_.level_patched = false;
    modulations_.level = 1.0f;
    blockPosition_ = kBlockSize;
    outputSampleRate_ = outputSampleRate > 0.0f ? outputSampleRate : 96000.0f;
    sourcePhase_ = 1.0f;
    previousSource_ = currentSource_ = 0.0f;
    auxMix_ = 0;
    active_ = gate_ = triggerPending_ = retriggerPending_ = false;
    envelopeMode_ = 0;
    quietSamples_ = 0;
    post_.init(outputSampleRate_);
  }

  void configure(uint8_t engine, uint16_t harmonics, uint16_t timbre,
                 uint16_t morph, uint8_t auxMix, uint8_t envelopeMode,
                 uint8_t decay, uint8_t sustain, float note, float gain) {
    patch_.engine = engine > 23 ? 23 : engine;
    patch_.harmonics = harmonics / 32767.0f;
    patch_.timbre = timbre / 32767.0f;
    patch_.morph = morph / 32767.0f;
    envelopeMode_ = envelopeMode == 0 ? 0 : 2;
    patch_.decay = envelopeMode_ == 0 ? decay / 255.0f : 0.5f;
    patch_.lpg_colour = envelopeMode_ == 0 ? sustain / 255.0f : 0.5f;
    modulations_.level_patched = envelopeMode_ == 2;
    patch_.note = note;
    auxMix_ = auxMix;
    post_.setGain(gain);
  }
  void setFilter(bool enabled, uint8_t character, uint8_t mode, bool slope24dB,
                 float cutoffHz, float resonance) {
    post_.setFilter(enabled, character, mode, slope24dB, cutoffHz, resonance);
  }
  void setEnvelope(float attack, float decay, float sustain, float release,
                   uint8_t shape = 0x80) {
    post_.setEnvelope(envelopeMode_ == 2, attack, decay, sustain, release, shape);
  }
  void noteOn() {
    retriggerPending_ = gate_;
    active_ = gate_ = true;
    triggerPending_ = !retriggerPending_;
    quietSamples_ = 0;
    if (envelopeMode_ == 2) post_.noteOn();
  }
  void noteOff() {
    gate_ = false;
    modulations_.trigger = 0.0f;
    if (envelopeMode_ == 2) post_.noteOff();
  }
  void kill() {
    active_ = gate_ = triggerPending_ = retriggerPending_ = false;
    modulations_.trigger = 0.0f;
    post_.kill();
    quietSamples_ = 0;
  }
  void render(float* output, size_t samples) {
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
      output[i] = post_.process(sample);
      if (envelopeMode_ == 2 && !post_.envelopeActive()) {
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
  bool active() const { return active_; }
  float envelopeLevel() const { return post_.envelopeLevel(); }

 private:
  void renderBlock() {
    modulations_.note = 0.0f;
    // Six-op FM needs a gate for its amp envelopes. Other Plaits engines use
    // a trigger pulse; holding that pulse high turns physical models into
    // sustained voices.
    const bool gateDriven = patch_.engine >= 2 && patch_.engine <= 4;
    if (retriggerPending_) {
      modulations_.trigger = 0.0f;
      retriggerPending_ = false;
      triggerPending_ = true;
    } else {
      modulations_.trigger = gateDriven && gate_ ? 1.0f : (triggerPending_ ? 1.0f : 0.0f);
      triggerPending_ = false;
    }
    modulations_.level = 1.0f;
    voice_.Render(patch_, modulations_, frames_, kBlockSize);
    blockPosition_ = 0;
  }
  float nextSourceSample() {
    if (blockPosition_ >= kBlockSize) renderBlock();
    const typename MutableVoice::Frame& frame = frames_[blockPosition_++];
    float main = frame.out / 32768.0f;
    float aux = frame.aux / 32768.0f;
    float mix = auxMix_ / 255.0f;
    return main + (aux - main) * mix;
  }

  MutableVoice voice_;
  char allocatorMemory_[16384];
  typename MutableVoice::Frame frames_[kBlockSize];
  Patch patch_;
  Modulations modulations_;
  size_t blockPosition_;
  float outputSampleRate_;
  float sourcePhase_, previousSource_, currentSource_;
  uint8_t auxMix_;
  bool active_, gate_, triggerPending_, retriggerPending_;
  uint8_t envelopeMode_;
  uint32_t quietSamples_;
  VoicePostProcessor<PlaitsEnvelope> post_;
};

#endif
