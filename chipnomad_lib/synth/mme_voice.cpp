#include "mme_voice.h"

#include <math.h>
#include <string.h>

static constexpr float kPi = 3.14159265358979323846f;
static float mmeClamp(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static float mmeWrap(float x) { return x - floorf(x); }

void MMEVoice::init(float sampleRate) {
  sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
  active_ = false; phaseA_ = phaseB_ = feedback_ = feedbackDC_ = 0.0f;
  memset(vocodeModLo_, 0, sizeof(vocodeModLo_)); memset(vocodeModHi_, 0, sizeof(vocodeModHi_));
  memset(vocodeCarLo_, 0, sizeof(vocodeCarLo_)); memset(vocodeCarHi_, 0, sizeof(vocodeCarHi_));
  memset(vocodeEnv_, 0, sizeof(vocodeEnv_)); post_.init(sampleRate_);
}

void MMEVoice::configure(const InstrumentMME* instrument, float pitchCents, float gain,
                         uint16_t cutoffHz, uint8_t resonance) {
  if (instrument) parameters_ = *instrument;
  frequency_ = 440.0f * powf(2.0f, (pitchCents - 6900.0f) / 1200.0f);
  post_.setGain(gain);
  post_.setFilter(parameters_.filterEnabled != 0, parameters_.filterCharacter,
    parameters_.filterMode, parameters_.filterSlope24dB != 0, cutoffHz, resonance / 255.0f);
  post_.setEnvelope(true, parameters_.attack * parameters_.attack / 13005.0f,
    parameters_.decay * parameters_.decay / 13005.0f, parameters_.sustain / 255.0f,
    parameters_.release * parameters_.release / 13005.0f, parameters_.envelopeShape);
}

void MMEVoice::noteOn() { phaseA_ = phaseB_ = feedback_ = feedbackDC_ = 0.0f; active_ = true; post_.noteOn(true); }
void MMEVoice::noteOff() { post_.noteOff(); }
void MMEVoice::kill() { active_ = false; post_.kill(); }

float MMEVoice::oscillator(float phase, int shape) const {
  phase = mmeWrap(phase);
  switch (shape) {
    case 1: return 1.0f - 4.0f * fabsf(phase - 0.5f);
    case 2: return 2.0f * phase - 1.0f;
    case 3: return phase < 0.5f ? 1.0f : -1.0f;
    case 4: return sinf(2.0f * kPi * phase) * sinf(2.0f * kPi * phase);
    default: return sinf(2.0f * kPi * phase);
  }
}

float MMEVoice::pairedOscillator(float phase, bool oscillatorB, float pairPosition) const {
  static constexpr int shapes[5][2] = {
    {0, 0}, {1, 0}, {2, 1}, {0, 2}, {3, 3}
  };
  int pair = (int)pairPosition;
  if (pair >= 4) return oscillator(phase, shapes[4][oscillatorB]);
  float blend = pairPosition - pair;
  float from = oscillator(phase, shapes[pair][oscillatorB]);
  return from + (oscillator(phase, shapes[pair + 1][oscillatorB]) - from) * blend;
}

// Adapted from Mutable Instruments Warps' diode ring-modulator model (MIT).
float MMEVoice::diode(float x) const {
  float sign = x > 0.0f ? 1.0f : -1.0f;
  float dead = fabsf(x) - 0.667f; dead += fabsf(dead); return 0.043247658f * dead * dead * sign;
}

float MMEVoice::shaper(float x) const {
  float amount = parameters_.shaper / 255.0f;
  if (amount <= 0.0f) return x;
  float saturated = tanhf(x * (1.0f + amount * 5.0f));
  float folded = fabsf(fmodf(saturated * (1.0f + amount * 3.0f) + 1.0f, 4.0f) - 2.0f) - 1.0f;
  float foldMix = mmeClamp((amount - .55f) / .45f, 0.0f, 1.0f);
  return saturated + (folded - saturated) * foldMix;
}

// Compact 20-band envelope vocoder.  Its band/envelope topology follows
// Warps' MIT vocoder; states are per voice so tracker notes remain isolated.
float MMEVoice::vocode(float modulator, float carrier) {
  float result = 0.0f, shift = parameters_.flow / 255.0f;
  float release = .003f + (1.0f - parameters_.amount / 255.0f) * .08f;
  for (int i = 0; i < 20; ++i) {
    float t = (float)i / 19.0f;
    float hz = 90.0f * powf(42.0f, mmeClamp(t + (shift - .5f) * .26f, 0.0f, 1.0f));
    float a = mmeClamp(2.0f * kPi * hz / sampleRate_, .0001f, .45f);
    vocodeModLo_[i] += a * (modulator - vocodeModLo_[i]);
    vocodeModHi_[i] += a * (vocodeModLo_[i] - vocodeModHi_[i]);
    vocodeCarLo_[i] += a * (carrier - vocodeCarLo_[i]);
    vocodeCarHi_[i] += a * (vocodeCarLo_[i] - vocodeCarHi_[i]);
    float env = fabsf(vocodeModLo_[i] - vocodeModHi_[i]);
    float rate = env > vocodeEnv_[i] ? .18f : release;
    vocodeEnv_[i] += rate * (env - vocodeEnv_[i]);
    result += (vocodeCarLo_[i] - vocodeCarHi_[i]) * mmeClamp(vocodeEnv_[i] * 6.0f, 0.0f, 2.0f);
  }
  return result * .18f;
}

void MMEVoice::render(float* output, size_t frames) {
  if (!output) return;
  if (!active_) { memset(output, 0, frames * sizeof(float)); return; }
  const float interval = (parameters_.interval - 128) / 128.0f;
  const float ratio = powf(2.0f, interval * 2.0f);
  // Morph both source oscillators across the five waveform-pair endpoints.
  const float pairPosition = parameters_.waves * (4.0f / 255.0f);
  const float amount = parameters_.amount / 255.0f, flow = parameters_.flow / 255.0f;
  static constexpr float shaperGain[7][9] = {
    {0.992512f, .874683f, .761782f, .721170f, .701094f, 1.011484f, 1.035431f, .415567f, .251189f},
    {0.994402f, 1.012236f, .866559f, .779612f, .741525f, 1.011517f, 1.035429f, .415568f, .251189f},
    {0.993494f, 1.032738f, .849110f, .776743f, .743856f, 1.011473f, 1.035431f, .415568f, .251189f},
    {0.992148f, 1.027351f, .874872f, .798137f, .756676f, 1.011463f, 1.035428f, .415568f, .251189f},
    {0.991767f, .964965f, .808480f, .749326f, .729518f, 1.011532f, 1.035434f, .415568f, .251189f},
    {0.999106f, .958805f, .800955f, .755353f, .719275f, 1.011618f, 1.035429f, .415568f, .251189f},
    {0.993601f, .889503f, .778649f, .724063f, .700066f, 1.011468f, 1.035428f, .415569f, .251189f}
  };
  const int gainIndex = parameters_.shaper == 255 ? 7 : parameters_.shaper / 32;
  const float gainFraction = parameters_.shaper == 255 ? 1.0f : (parameters_.shaper & 31) / 32.0f;
  const float gainA = shaperGain[(int)parameters_.model][gainIndex];
  const float gainB = shaperGain[(int)parameters_.model][gainIndex + 1];
  const float calibration = .9856f * expf(logf(gainA) + (logf(gainB) - logf(gainA)) * gainFraction);
  for (size_t i = 0; i < frames; ++i) {
    const float feedbackControl = parameters_.feedback / 255.0f;
    // The square keeps normal settings civil; the last third accelerates into
    // intentionally ugly territory. tanh below keeps the recursive state safe.
    const float feedbackGain = feedbackControl * feedbackControl * 2.5f;
    const float stepA = frequency_ / sampleRate_, stepB = frequency_ * ratio / sampleRate_;
    const bool masterWraps = phaseA_ + stepA >= 1.0f;
    if (parameters_.model == MMEModel::sync && masterWraps) {
      // Amount morphs from free-running B to a hard reset; Flow selects where
      // in B's cycle the reset lands, so it remains a real timbral control.
      const float resetPhase = flow;
      phaseB_ += (resetPhase - phaseB_) * amount;
    }
    const float injected = feedback_ * feedbackGain;
    float a = tanhf(pairedOscillator(phaseA_, false, pairPosition) + injected * (.35f + .75f * flow));
    float b = tanhf(pairedOscillator(phaseB_, true, pairPosition) - injected * (1.10f - .50f * flow));
    float sample = 0.0f;
    switch (parameters_.model) {
      case MMEModel::ring: {
        float analog = tanhf((diode(a + b * amount * 2.0f) + diode(a - b * amount * 2.0f)) * 12.0f);
        float digital = (4.0f * a * b * amount); digital /= 1.0f + fabsf(digital);
        sample = analog + (digital - analog) * flow; break;
      }
      case MMEModel::fold: {
        float sum = (a + b * (0.15f + amount) + a * b * .25f) * (.02f + amount * 1.4f);
        sample = fabsf(fmodf(sum + 1.0f, 4.0f) - 2.0f) - 1.0f; break;
      }
      case MMEModel::cross: {
        float ab = pairedOscillator(phaseA_ + (b + feedback_) * amount * 0.28f, false, pairPosition);
        float ba = pairedOscillator(phaseB_ + (a + feedback_) * amount * 0.28f, true, pairPosition);
        sample = ab + (ba - ab) * flow; break;
      }
      case MMEModel::vpm: {
        float ab = pairedOscillator(phaseA_ + (b + feedback_) * amount * .45f, false, pairPosition);
        float ba = pairedOscillator(phaseB_ + (a + feedback_) * amount * .45f, true, pairPosition);
        sample = ab + (ba - ab) * flow; break;
      }
      case MMEModel::sync: {
        sample = pairedOscillator(phaseB_, true, pairPosition) + a * (amount * .18f); break;
      }
      case MMEModel::logic: {
        int16_t ia = (int16_t)(a * 32767.0f), ib = (int16_t)(b * 32767.0f);
        float xorValue = (float)(ia ^ ib) / 32768.0f;
        float compare = fabsf(a) > fabsf(b) ? a : b;
        sample = (a + b) * (1.0f - amount) * .5f + (xorValue + (compare - xorValue) * flow) * amount; break;
      }
      case MMEModel::vocode: sample = vocode(b * amount, a); break;
      default: break;
    }
    sample = tanhf(sample + injected * 1.2f);
    sample = shaper(sample);
    const float rawFeedback = tanhf(feedback_ * (.10f + feedbackGain) + sample * (.45f + feedbackControl * 1.9f));
    // A recursive DC offset turns a wild patch into silence or a flat line.
    // Keep the turbulence, but only reinject its AC component.
    feedbackDC_ += .025f * (rawFeedback - feedbackDC_);
    feedback_ = (rawFeedback - feedbackDC_) * (.72f + feedbackControl * .22f);
    output[i] = post_.process(sample * calibration);
    phaseA_ = mmeWrap(phaseA_ + stepA);
    phaseB_ = mmeWrap(phaseB_ + stepB);
    if (!post_.envelopeActive()) { active_ = false; memset(output + i + 1, 0, (frames - i - 1) * sizeof(float)); break; }
  }
}
