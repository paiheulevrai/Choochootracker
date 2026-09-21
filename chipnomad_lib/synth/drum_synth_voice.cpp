#include "drum_synth_voice.h"

#include <math.h>
#include <string.h>

static constexpr float pi = 3.14159265358979323846f;

void DrumSynthVoice::init(float sampleRate) {
  sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
  instrument_ = nullptr; active_ = false; age_ = duration_ = envelope_ = 0.0f;
  frequency_ = 110.0f; noiseLow_ = 0.0f; randomState_ = 0x43525452u;
  memset(phase_, 0, sizeof(phase_)); post_.init(sampleRate_);
}

void DrumSynthVoice::configure(const InstrumentDrumSynth* instrument, float pitchCents,
                               float gain, uint16_t cutoffHz, uint8_t resonance) {
  if (instrument) parameters_ = *instrument;
  instrument_ = instrument ? &parameters_ : nullptr;
  frequency_ = 440.0f * powf(2.0f, (pitchCents - 6900.0f) / 1200.0f);
  post_.setGain(gain);
  if (!instrument_) return;
  post_.setFilter(instrument_->filterEnabled != 0, instrument_->filterCharacter,
    instrument_->filterMode, instrument_->filterSlope24dB != 0, cutoffHz, resonance / 255.0f);
}

void DrumSynthVoice::noteOn() {
  if (!instrument_) return;
  age_ = 0.0f; envelope_ = 1.0f; noiseLow_ = 0.0f; active_ = true;
  memset(phase_, 0, sizeof(phase_));
  float d = instrument_->decay / 255.0f;
  duration_ = 0.018f + d * d * (instrument_->engine == DrumSynthEngine::hat ? 1.55f : 2.0f);
  post_.noteOn(true);
}
void DrumSynthVoice::noteOff() { }
void DrumSynthVoice::kill() { active_ = false; post_.kill(); }

float DrumSynthVoice::random() {
  randomState_ = randomState_ * 1664525u + 1013904223u;
  return ((randomState_ >> 8) * (1.0f / 8388608.0f)) - 1.0f;
}
float DrumSynthVoice::oscillator(float frequency, int shape, int slot) {
  phase_[slot] += frequency / sampleRate_;
  phase_[slot] -= floorf(phase_[slot]);
  float p = phase_[slot];
  if (shape == 1) return 1.0f - 4.0f * fabsf(p - 0.5f);
  if (shape == 2) return p < 0.5f ? 1.0f : -1.0f;
  return sinf(2.0f * pi * p);
}

void DrumSynthVoice::render(float* output, size_t frames) {
  if (!output) return;
  if (!active_ || !instrument_) { memset(output, 0, frames * sizeof(float)); return; }
  const float tone = instrument_->tone / 255.0f, sweep = instrument_->sweep / 255.0f;
  const float noise = instrument_->noise / 255.0f, fm = instrument_->fm / 255.0f;
  const float drive = 1.0f + instrument_->drive / 255.0f * 5.5f;
  for (size_t i = 0; i < frames; ++i) {
    if (age_ >= duration_) { kill(); memset(output + i, 0, (frames - i) * sizeof(float)); break; }
    float progress = age_ / duration_, fast = expf(-age_ / (0.006f + 0.050f * (1.0f - tone)));
    envelope_ = expf(-5.5f * progress);
    float n = random(); noiseLow_ += (n - noiseLow_) * (0.015f + tone * 0.25f);
    float brightNoise = n - noiseLow_, sample = 0.0f;
    switch (instrument_->engine) {
      case DrumSynthEngine::kick: {
        float f = frequency_ * (1.0f + sweep * 7.0f * fast);
        sample = oscillator(f, 0, 0) * .90f + oscillator(f * 2.0f, 1, 1) * fm * .30f;
        sample += brightNoise * noise * .45f * expf(-age_ / .006f);
        break;
      }
      case DrumSynthEngine::snare:
        sample = oscillator(frequency_ * (.65f + tone * 1.05f) * (1.0f + sweep * 1.1f * fast), 1, 0) * (1.0f - noise * .75f);
        sample += oscillator(frequency_ * (1.20f + fm * 1.50f), 0, 1) * (.12f + fm * .58f);
        sample += brightNoise * noise * .95f;
        break;
      case DrumSynthEngine::hat: {
        // No fundamental: unequal mode lifetimes keep the bank from settling
        // into a pitched square-wave chord.
        static const float ratios[] = {1.18f, 1.56f, 2.17f, 2.87f, 3.73f, 4.61f};
        float metal = 0.0f, base = (180.0f + tone * 260.0f + frequency_ * .30f) * (1.0f + sweep * .75f * fast);
        for (int o = 0; o < 6; ++o) {
          float modeDecay = expf(-age_ / (.025f + o * .020f + tone * .070f));
          metal += oscillator(base * (ratios[o] + fm * (o + 1) * .20f), 2, o) * modeDecay * (.06f + fm * .06f);
        }
        sample = metal + brightNoise * noise * 1.25f;
        break;
      }
      case DrumSynthEngine::clap: {
        float spacing = .010f + sweep * .018f;
        float burst = age_ < .010f || (age_ > spacing && age_ < spacing + .010f) ||
          (age_ > spacing * 2.0f && age_ < spacing * 2.0f + .010f) ? 1.0f : .55f;
        sample = brightNoise * (0.25f + noise * .75f) * burst;
        sample += oscillator(frequency_ * (2.0f + tone * 8.0f), 2, 0) * fm * .55f * fast;
        break;
      }
      case DrumSynthEngine::tom:
        sample = oscillator(frequency_ * (.90f + tone * .35f) * (1.0f + sweep * 1.3f * fast), 0, 0);
        sample += oscillator(frequency_ * (1.6f + fm * 1.4f), 0, 1) * fm * .55f;
        sample += brightNoise * noise * .55f * fast;
        break;
      case DrumSynthEngine::rim:
        sample = oscillator(frequency_ * (.85f + tone * .45f) * (1.0f + sweep * .25f), 1, 0) * .70f;
        sample += oscillator(frequency_ * (1.7f + fm * 2.5f), 0, 1) * (.10f + fm * .55f);
        sample += brightNoise * noise * .50f * fast;
        break;
      case DrumSynthEngine::fm: {
        float ratio = .25f * powf(2.0f, tone * 2.5f);
        float mod = oscillator(frequency_ * ratio, 0, 1) * fm * 9.0f * fast;
        sample = oscillator(frequency_ * (1.0f + sweep * 2.0f * fast + mod), 0, 0) * .85f + brightNoise * noise * .45f;
        break;
      }
      case DrumSynthEngine::noise:
        sample = (brightNoise * (1.0f - sweep * .75f) + noiseLow_ * sweep * .75f) * (.2f + noise * .8f);
        sample += oscillator(frequency_ * (1.0f + tone * 5.0f), 0, 0) * fm * .22f;
        break;
      case DrumSynthEngine::cowbell: {
        // Cross-modulation turns the two fixed square oscillators into a
        // controllably metallic pair while preserving the classic setting at zero.
        float mod = oscillator(frequency_ * 2.37f, 0, 2) * fm * 1.5f;
        float sweepPitch = 1.0f + sweep * .80f * fast;
        sample = oscillator(frequency_ * sweepPitch * (1.0f + mod), 2, 0) * .55f;
        sample += oscillator(frequency_ * sweepPitch * (1.39f + tone * .18f + mod * .42f), 2, 1) * .45f;
        sample += brightNoise * noise * .12f;
        break;
      }
      case DrumSynthEngine::cymbal: {
        static const float ratios[] = {1.31f, 1.79f, 2.41f, 3.16f, 4.07f, 5.23f};
        float metal = 0.0f, base = (250.0f + tone * 360.0f + frequency_ * .45f) * (1.0f + sweep * .90f * fast);
        for (int o = 0; o < 6; ++o) {
          float modeDecay = expf(-age_ / (.10f + o * .075f + tone * .70f));
          metal += oscillator(base * (ratios[o] + fm * (o + 1) * .18f), 2, o) * modeDecay * (.05f + fm * .06f);
        }
        sample = metal + brightNoise * (.10f + noise * .90f);
        break;
      }
      case DrumSynthEngine::shaker:
        sample = brightNoise * (.28f + noise * .72f) * (.45f + .55f * sinf(age_ * (28.0f + tone * 75.0f + sweep * 180.0f)));
        sample += oscillator(frequency_ * (4.0f + tone * 6.0f), 2, 0) * fm * .16f * fast;
        break;
      case DrumSynthEngine::clave:
        sample = oscillator(frequency_ * (2.2f + tone * 1.8f) * (1.0f + sweep * 1.5f * fast), 1, 0) * .85f;
        sample += oscillator(frequency_ * (3.7f + fm * 1.3f), 0, 1) * .25f;
        sample += brightNoise * noise * .20f * fast;
        break;
      default: break;
    }
    sample = tanhf(sample * drive) / tanhf(drive) * envelope_;
    output[i] = post_.process(sample);
    age_ += 1.0f / sampleRate_;
  }
}
