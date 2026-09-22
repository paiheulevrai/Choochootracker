#include "sintered_voice.h"

#include <math.h>
#include <string.h>

static constexpr float kPi = 3.14159265358979323846f;
static float clampS(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
static float foldS(float x) { return fabsf(fmodf(x + 1.0f, 4.0f) - 2.0f) - 1.0f; }

void SinteredVoice::init(float sampleRate) {
  sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
  post_.init(sampleRate_); kill();
}

void SinteredVoice::configure(const InstrumentSintered* instrument, float pitchCents, float gain,
                              uint16_t cutoffHz, uint8_t resonance) {
  if (instrument) parameters_ = *instrument;
  frequency_ = 440.0f * powf(2.0f, (pitchCents - 6900.0f) / 1200.0f);
  post_.setGain(gain);
  post_.setFilter(parameters_.filterEnabled != 0, parameters_.filterCharacter, parameters_.filterMode,
    parameters_.filterSlope24dB != 0, cutoffHz, resonance / 255.0f);
}

void SinteredVoice::noteOn() {
  age_ = amplitude_ = feedback_ = dc_ = noiseLow_ = 0.0f; combIndex_ = 0; active_ = true;
  randomState_ = 0x53494e54u; memset(phase_, 0, sizeof(phase_)); memset(comb_, 0, sizeof(comb_)); post_.noteOn(true);
  float d = parameters_.decay / 255.0f;
  static const float tails[] = {.85f, .70f, .48f, 1.10f, .42f, .80f};
  duration_ = .018f + d * d * tails[(int)parameters_.model];
}
void SinteredVoice::kill() { active_ = false; post_.kill(); }
float SinteredVoice::random() { randomState_ = randomState_ * 1664525u + 1013904223u; return ((randomState_ >> 8) * (1.0f / 8388608.0f)) - 1.0f; }
float SinteredVoice::osc(float frequency, int slot) {
  phase_[slot] += frequency / sampleRate_; phase_[slot] -= floorf(phase_[slot]); return sinf(2.0f * kPi * phase_[slot]);
}

void SinteredVoice::render(float* output, size_t frames) {
  if (!output) return;
  if (!active_) { memset(output, 0, frames * sizeof(float)); return; }
  const float baseMod = parameters_.mod / 255.0f, baseA = parameters_.a / 255.0f;
  const float baseB = parameters_.b / 255.0f, baseC = parameters_.c / 255.0f;
  const float motion = (parameters_.motion - 128) / 127.0f;
  const float motionTime = .008f + (1.0f - fabsf(motion)) * .35f;
  for (size_t i = 0; i < frames; ++i) {
    if (age_ >= duration_) { kill(); memset(output + i, 0, (frames - i) * sizeof(float)); break; }
    float movement = 0.0f;
    if (parameters_.motion != 128) {
      float x = age_ / motionTime;
      movement = motion < 0.0f ? (x < 1.0f ? sinf(kPi * x) : 0.0f) : expf(-6.0f * x);
    }
    float mod = baseMod, a = baseA, b = baseB, c = baseC;
    switch (parameters_.model) {
      case SinteredModel::knot: mod = clampS(mod + movement * .65f); c = clampS(c + movement * .55f); break;
      case SinteredModel::shard: b = clampS(b + movement * .65f); c = clampS(c + movement * .55f); break;
      case SinteredModel::burst: a = clampS(a + movement * .7f); b = clampS(b + movement * .55f); c = clampS(c + movement * .35f); break;
      case SinteredModel::comb: b = clampS(b + movement * .45f); c = clampS(c + movement * .55f); break;
      case SinteredModel::logic: a = clampS(a + movement * .6f); c = clampS(c + movement * .7f); break;
      case SinteredModel::melt: b = clampS(b + movement * .7f); c = clampS(c + movement * .6f); break;
      default: break;
    }
    float n = random(); noiseLow_ += (n - noiseLow_) * (.01f + b * .25f);
    float bright = n - noiseLow_, sample = 0.0f;
    float f2 = frequency_ * powf(2.0f, (a - .5f) * 5.0f);
    float x = osc(frequency_, 0), y = osc(f2 + feedback_ * frequency_ * mod * 1.5f, 1);
    switch (parameters_.model) {
      case SinteredModel::knot: {
        float z = osc(frequency_ * powf(2.0f, (b - .5f) * 7.0f), 2);
        sample = sinf(2.0f * kPi * (phase_[0] + y * mod * .32f + z * mod * .18f));
        sample = sample * (1.0f - c) + foldS(sample * (1.0f + c * 14.0f)) * c;
        break;
      }
      case SinteredModel::shard: {
        float teeth = foldS((x + y * (1.0f + mod * 6.0f) + feedback_ * b * 4.0f) * (1.0f + c * 12.0f));
        sample = tanhf(teeth * (1.0f + c * 5.0f)); break;
      }
      case SinteredModel::burst: {
        float color = bright * (1.0f - b) + noiseLow_ * b;
        sample = color * (a * (1.2f + mod * .8f)) + y * (1.0f - a) + feedback_ * mod * 1.5f;
        sample = foldS(sample * (1.0f + c * 8.0f)); break;
      }
      case SinteredModel::comb: {
        int delay = 1 + (int)(a * 62.0f); float delayed = comb_[(combIndex_ + 64 - delay) & 63];
        float excite = x + y * mod * .75f + bright * mod * .25f;
        comb_[combIndex_] = tanhf((excite + delayed * c * 1.35f) * (.4f + mod * 1.6f)); combIndex_ = (combIndex_ + 1) & 63;
        sample = delayed * (1.0f - b * .92f) + excite * .18f; break;
      }
      case SinteredModel::logic: {
        int q = 2 + (int)(a * 126.0f), pattern = (int)(b * 3.99f); int ia = (int)((x + 1.0f) * q), ib = (int)((y + 1.0f) * q);
        int logic = pattern == 0 ? (ia ^ ib) : pattern == 1 ? (ia & ib) : pattern == 2 ? (ia | ib) : (ia > ib ? ia : ib);
        sample = ((logic % (q * 2)) / (float)q - 1.0f) * mod + x * (1.0f - mod); sample = foldS(sample * (1.0f + c * 15.0f)); break;
      }
      case SinteredModel::melt: {
        float warped = osc(frequency_ * (1.0f + y * mod * (3.0f + b * 24.0f) + feedback_ * b * 6.0f), 2);
        sample = tanhf(warped * (1.0f + c * 13.0f) + bright * mod * b); break;
      }
      default: break;
    }
    // A brief noisy impact gives the resonators a drum-like excitation; the
    // smoothed feedback that follows keeps the tail fluid rather than buzzy.
    float impact = bright * expf(-age_ / (.0015f + .006f * (1.0f - b)));
    static const float impactMix[] = {.22f, .18f, .72f, .38f, .24f, .16f};
    sample += impact * impactMix[(int)parameters_.model] * (.25f + .75f * mod);
    float raw = tanhf(sample + feedback_ * (.2f + mod * 1.4f)); dc_ += .02f * (raw - dc_);
    float target = (raw - dc_) * (.12f + baseMod * .82f);
    feedback_ += (target - feedback_) * (.025f + .10f * (1.0f - c));
    amplitude_ = expf(-5.5f * age_ / duration_);
    output[i] = tanhf(post_.process(tanhf(sample) * amplitude_));
    age_ += 1.0f / sampleRate_;
  }
}
