#include "sintered_voice.h"
#include "audio_math.h"

#include <math.h>
#include <string.h>

static constexpr float kPi = 3.14159265358979323846f;
static float clampS(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
static float foldS(float x) { return fabsf(audioFmod4(x + 1.0f) - 2.0f) - 1.0f; }

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
    parameters_.filterSlope24dB != 0, cutoffHz, resonance / 255.0f,
    parameters_.filterColor);
}

void SinteredVoice::noteOn() {
  age_ = amplitude_ = feedback_ = dc_ = noiseLow_ = 0.0f; combIndex_ = 0; active_ = true;
  decayAge_ = 0.0f; gateHigh_ = true;
  randomState_ = 0x53494e54u; memset(phase_, 0, sizeof(phase_)); memset(comb_, 0, sizeof(comb_)); post_.noteOn(true);
  float d = parameters_.decay / 255.0f;
  static const float tails[] = {.85f, .70f, .48f, 1.10f, .42f, .80f, .85f};
  duration_ = .018f + d * d * tails[(int)parameters_.model];
}
void SinteredVoice::noteOff() { gateHigh_ = false; }
void SinteredVoice::kill() { active_ = gateHigh_ = false; post_.kill(); }
float SinteredVoice::random() { randomState_ = randomState_ * 1664525u + 1013904223u; return ((randomState_ >> 8) * (1.0f / 8388608.0f)) - 1.0f; }
float SinteredVoice::osc(float frequency, int slot) {
  phase_[slot] += frequency / sampleRate_; phase_[slot] -= floorf(phase_[slot]); return sinf(2.0f * kPi * phase_[slot]);
}
float SinteredVoice::saw(float frequency, int slot) {
  float increment = fminf(.45f, fmaxf(1.0f / sampleRate_, frequency / sampleRate_));
  phase_[slot] += increment; phase_[slot] -= floorf(phase_[slot]);
  float value = 2.0f * phase_[slot] - 1.0f;
  if (phase_[slot] < increment) {
    float t = phase_[slot] / increment;
    value -= t + t - t * t - 1.0f;
  } else if (phase_[slot] > 1.0f - increment) {
    float t = (phase_[slot] - 1.0f) / increment;
    value -= t * t + t + t + 1.0f;
  }
  return value;
}

void SinteredVoice::render(float* output, size_t frames) {
  if (!output) return;
  if (!active_) { memset(output, 0, frames * sizeof(float)); return; }
  const float baseMod = parameters_.mod / 255.0f, baseA = parameters_.a / 255.0f;
  const float baseB = parameters_.b / 255.0f, baseC = parameters_.c / 255.0f;
  static constexpr float modelGain[] = {1.822647f, 1.995262f, 1.995262f, 1.995262f, 1.995262f, 1.995262f, .75f};
  const float calibration = modelGain[(int)parameters_.model];
  const float motion = (parameters_.motion - 128) / 128.0f;
  const bool holding = parameters_.gate != 0 && gateHigh_;
  const float motionDepth = fabsf(motion);
  const float motionTime = .012f + motionDepth * .08f;
  const bool fixedA = parameters_.model != SinteredModel::burst &&
                      parameters_.model != SinteredModel::logic;
  const float fixedF2 = fixedA ? frequency_ * powf(2.0f, (baseA - .5f) * 5.0f) : 0.0f;
  const float knotFrequency = parameters_.model == SinteredModel::knot
      ? frequency_ * powf(2.0f, (baseB - .5f) * 7.0f) : 0.0f;
  const bool squall = parameters_.model == SinteredModel::squall;
  const int squallVoices = squall ? 3 + (int)(baseB * 2.99f) : 3;
  const float squallScale = squall ? 1.0f / sqrtf((float)squallVoices) : 1.0f;
  float squallFrequency[5] = {};
  if (squall) {
    const float spread = .02f + baseA * 2.5f;
    for (int voice = 0; voice < squallVoices; ++voice) {
      const float position = 2.0f * voice / (squallVoices - 1) - 1.0f;
      squallFrequency[voice] = frequency_ * powf(2.0f, position * spread / 12.0f);
    }
  }
  for (size_t i = 0; i < frames; ++i) {
    if (decayAge_ >= duration_) { kill(); memset(output + i, 0, (frames - i) * sizeof(float)); break; }
    float movement = 0.0f;
    if (parameters_.motion != 128) {
      float x = age_ / motionTime;
      const float envelope = motion < 0.0f ? (x < 1.0f ? sinf(kPi * x) : 0.0f) : expf(-3.0f * x);
      movement = envelope * motionDepth * 1.75f;
    }
    float mod = baseMod, a = baseA, b = baseB, c = baseC;
    switch (parameters_.model) {
      case SinteredModel::knot: mod = clampS(mod + movement * .65f); c = clampS(c + movement * .55f); break;
      case SinteredModel::shard: b = clampS(b + movement * .65f); c = clampS(c + movement * .55f); break;
      case SinteredModel::burst: a = clampS(a + movement * .7f); b = clampS(b + movement * .55f); c = clampS(c + movement * .35f); break;
      case SinteredModel::comb: b = clampS(b + movement * .45f); c = clampS(c + movement * .55f); break;
      case SinteredModel::logic: a = clampS(a + movement * .6f); c = clampS(c + movement * .7f); break;
      case SinteredModel::melt: b = clampS(b + movement * .7f); c = clampS(c + movement * .6f); break;
      case SinteredModel::squall: mod = clampS(mod + movement * .65f); break;
      default: break;
    }
    float n = random(); noiseLow_ += (n - noiseLow_) * (.01f + b * .25f);
    float bright = n - noiseLow_, sample = 0.0f;
    float f2 = fixedA ? fixedF2 : frequency_ * powf(2.0f, (a - .5f) * 5.0f);
    float x = osc(frequency_, 0), y = osc(f2 + feedback_ * frequency_ * mod * 1.5f, 1);
    switch (parameters_.model) {
      case SinteredModel::knot: {
        float z = osc(knotFrequency, 2);
        sample = sinf(2.0f * kPi * (phase_[0] + y * mod * .32f + z * mod * .18f));
        sample = sample * (1.0f - c) + foldS(sample * (1.0f + c * 14.0f)) * c;
        break;
      }
      case SinteredModel::shard: {
        float teeth = foldS((x + y * (1.0f + mod * 6.0f) + feedback_ * b * 4.0f) * (1.0f + c * 12.0f));
        sample = audioTanh(teeth * (1.0f + c * 5.0f)); break;
      }
      case SinteredModel::burst: {
        float color = bright * (1.0f - b) + noiseLow_ * b;
        sample = color * (a * (1.2f + mod * .8f)) + y * (1.0f - a) + feedback_ * mod * 1.5f;
        sample = foldS(sample * (1.0f + c * 8.0f)); break;
      }
      case SinteredModel::comb: {
        int delay = 1 + (int)(a * 62.0f); float delayed = comb_[(combIndex_ + 64 - delay) & 63];
        float excite = x + y * mod * .75f + bright * mod * .25f;
        comb_[combIndex_] = audioTanh((excite + delayed * c * 1.35f) * (.4f + mod * 1.6f)); combIndex_ = (combIndex_ + 1) & 63;
        sample = delayed * (1.0f - b * .92f) + excite * .18f; break;
      }
      case SinteredModel::logic: {
        int q = 2 + (int)(a * 126.0f), pattern = (int)(b * 3.99f); int ia = (int)((x + 1.0f) * q), ib = (int)((y + 1.0f) * q);
        int logic = pattern == 0 ? (ia ^ ib) : pattern == 1 ? (ia & ib) : pattern == 2 ? (ia | ib) : (ia > ib ? ia : ib);
        sample = ((logic % (q * 2)) / (float)q - 1.0f) * mod + x * (1.0f - mod); sample = foldS(sample * (1.0f + c * 15.0f)); break;
      }
      case SinteredModel::melt: {
        float warped = osc(frequency_ * (1.0f + y * mod * (3.0f + b * 24.0f) + feedback_ * b * 6.0f), 2);
        sample = audioTanh(warped * (1.0f + c * 13.0f) + bright * mod * b); break;
      }
      case SinteredModel::squall: {
        float fm = n * mod * 2.5f;
        float swarm = 0.0f;
        for (int voice = 0; voice < squallVoices; ++voice) {
          float modulated = fmaxf(1.0f, squallFrequency[voice] * (1.0f + fm));
          swarm += saw(modulated, voice);
        }
        sample = swarm * squallScale;
        break;
      }
      default: break;
    }
    // A brief noisy impact gives the resonators a drum-like excitation; the
    // smoothed feedback that follows keeps the tail fluid rather than buzzy.
    float impact = bright * expf(-age_ / (.0015f + .006f * (1.0f - b)));
    static const float impactMix[] = {.22f, .18f, .72f, .38f, .24f, .16f, .16f};
    sample += impact * impactMix[(int)parameters_.model] * (.25f + .75f * mod);
    float raw = audioTanh(sample + feedback_ * (.2f + mod * 1.4f)); dc_ += .02f * (raw - dc_);
    float target = (raw - dc_) * (.12f + baseMod * .82f);
    feedback_ += (target - feedback_) * (.025f + .10f * (1.0f - c));
    amplitude_ = expf(-5.5f * decayAge_ / duration_);
    float processed = post_.process(audioTanh(sample) * amplitude_ * calibration);
    if (parameters_.model == SinteredModel::squall) processed = audioTanh(processed * (1.0f + c * 15.0f));
    output[i] = audioTanh(processed);
    age_ += 1.0f / sampleRate_;
    if (!holding) decayAge_ += 1.0f / sampleRate_;
  }
}
