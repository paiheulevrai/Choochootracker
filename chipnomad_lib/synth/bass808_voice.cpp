#include "bass808_voice.h"
#include "audio_math.h"

#include <math.h>
#include <string.h>

namespace {
constexpr float kPi = 3.14159265358979323846f;
// 440 Hz at A4 (MIDI note 69), cents 6900.  Used so the rest of the codebase
// can pass its standard (note+12)*100 cents feed without any per-instrument
// pitch-table remapping.
constexpr float kReferenceCents = 6900.0f;
constexpr float kReferenceHz    = 440.0f;

// Nonzero glide values span ~5 ms to ~1.05 s.
constexpr float kGlideMinSeconds = 0.005f;
constexpr float kGlideMaxSeconds = 1.05f;

inline float centsToHz(float cents) {
  return kReferenceHz * exp2f((cents - kReferenceCents) / 1200.0f);
}

// Per-sample interpolation weight for the pitch one-pole.
inline float onePoleAlpha(float sampleRate, float timeConstantSeconds) {
  if (timeConstantSeconds <= 0.0f) return 0.0f;
  // expf(-1 / (sr * tc)) is the standard z-plane one-pole; rc = sr*tc samples.
  float rc = sampleRate * timeConstantSeconds;
  return 1.0f - expf(-1.0f / rc);
}

}  // namespace

float Bass808Voice::waveform(float& phase, float phaseInc, int shape) {
  phase += phaseInc;
  phase -= floorf(phase);
  const float p = phase;
  switch (shape) {
    case 1:  // triangle, ramps -1..+1 across the period
      return 1.0f - 4.0f * fabsf(p - 0.5f);
    case 2:  // saw, ramps -1..+1
      return 2.0f * p - 1.0f;
    case 3:  // square, 50% duty
      return p < 0.5f ? 1.0f : -1.0f;
    case 0:  // sine, the default for 808 sub-bass
    default:
      return sinf(2.0f * kPi * p);
  }
}

float Bass808Voice::glideCoefficient(uint8_t glideValue) const {
  if (!glideValue) return 0.0f;
  // Exponential 0..255 -> seconds. Linear in log-space feels musical.
  const float t = glideValue / 255.0f;
  const float seconds = kGlideMinSeconds *
                        powf(kGlideMaxSeconds / kGlideMinSeconds, t);
  return onePoleAlpha(sampleRate_, seconds);
}

void Bass808Voice::init(float sampleRate) {
  sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
  instrument_ = nullptr;
  active_ = false;
  phaseMain_ = 0.0f;
  phaseSub_  = 0.0f;
  currentCents_ = kReferenceCents;
  glideCents_   = kReferenceCents;
  glideAlpha_   = 0.0f;
  envelopeMode_ = 2;
  lastOutput_ = 0.0f;
  rampFrom_ = 0.0f;
  rampRemaining_ = 0;
  // Baseline de-click ramp (2 ms); configure() re-lengths it per pitch.
  rampFrames_ = sampleRate_ > 500.0f ? (uint32_t)(sampleRate_ * 0.002f) : 2;
  if (rampFrames_ < 2) rampFrames_ = 2;
  tailFrom_ = 0.0f;
  transitionFrames_ = 0;
  // 3 ms de-click tail, at least 2 frames so the linear weight never
  // divides by zero (same constant as SampleVoice's).
  transitionLength_ = sampleRate_ > 500.0f ? (uint32_t)(sampleRate_ * 0.003f) : 2;
  if (transitionLength_ < 2) transitionLength_ = 2;
  post_.init(sampleRate_);
}

void Bass808Voice::configure(const InstrumentBass808* instrument, float pitchCents,
                              float gain, uint16_t cutoffHz, uint8_t resonance,
                              uint8_t glideValue) {
  if (instrument) parameters_ = *instrument;
  instrument_ = instrument ? &parameters_ : nullptr;
  currentCents_ = pitchCents;
  post_.setGain(gain);
  if (!instrument_) return;

  // The shared post-processor handles the filter + ADSR + VCA.  The same
  // byte layout is shared by every voice-post instrument, so we just hand
  // the inherited InstrumentVoicePostSettings bytes straight through.
  post_.setFilter(instrument->filterEnabled != 0, instrument->filterCharacter,
                  instrument->filterMode, instrument->filterSlope24dB != 0,
                  cutoffHz, resonance / 255.0f, instrument->filterColor);
  const uint8_t nextMode = instrument->envelopeMode == 0 ? 0 : 2;
  const bool modeChanged = envelopeMode_ != nextMode;
  envelopeMode_ = nextMode;
  const bool trig = envelopeMode_ == 0;
  // Preserve Fatty's linear four-second VCA timing; TRIG maps D/C to a one-shot.
  const auto time = [](uint8_t value) {
    return value / 255.0f * 4.0f;
  };
  post_.setEnvelope(true,
                    trig ? 0.001f + instrument->sustain / 255.0f * 0.05f : time(instrument->attack),
                    trig ? instrument->decay / 255.0f * 4.0f : time(instrument->decay),
                    trig ? 0.0f : instrument->sustain / 255.0f,
                    trig ? 0.0f : fmaxf(0.003f, time(instrument->release)),
                    instrument->envelopeShape);

  // Glide coefficient is updated every block; if the FX value changes
  // mid-glide the new coefficient takes effect without resetting the
  // running target.  (We only reset on noteOn to allow a fresh legato.)
  glideAlpha_ = glideCoefficient(glideValue);

  // De-click ramp length scales with pitch: 2 ms is only ~10% of a 50 Hz
  // waveform, which still clicks. Ramp for at least 3/4 of one period of
  // the current note (12 ms cap) so the crossfade genuinely bridges the
  // waveform change. ONLY re-length while no ramp is in flight: the blend
  // math uses rampFrames_ as its denominator, so shrinking it mid-ramp
  // jumps the blend factor — an audible gain step that became obvious at
  // higher pitches (short ramps, frequent configure() updates).
  if (rampRemaining_ == 0) {
    const float freqHz = centsToHz(currentCents_);
    if (freqHz > 1.0f) {
      const float periodSeconds = 1.0f / freqHz;
      float rampSeconds = periodSeconds * 0.75f;
      if (rampSeconds < 0.002f) rampSeconds = 0.002f;
      if (rampSeconds > 0.012f) rampSeconds = 0.012f;
      rampFrames_ = (uint32_t)(sampleRate_ * rampSeconds);
      if (rampFrames_ < 2) rampFrames_ = 2;
    }
  }
  if (modeChanged && active_) noteOn();
}

void Bass808Voice::noteOn() {
  if (!instrument_) return;
  rampFrom_ = lastOutput_;
  rampRemaining_ = rampFrames_;
  transitionFrames_ = 0;
  if (!active_) glideCents_ = currentCents_;
  if (!active_) { phaseMain_ = 0.0f; phaseSub_ = 0.0f; }
  active_ = true;
  // Every note restarts its attack; the output ramp bridges the amplitude reset.
  post_.kill();
  post_.noteOn(true);
}

void Bass808Voice::noteOff() {
  // TRIG is one-shot: the pluck decays on its own and gate-off is ignored
  // (same as plaits' TRIG/LPG mode where the internal decay runs free).
  if (envelopeMode_ != 0) post_.noteOff();
}

void Bass808Voice::kill() {
  // De-click: a hard cut fades out over the tail instead of stepping to
  // zero (same as SampleVoice's kill transition).
  if (active_ && lastOutput_ != 0.0f) {
    tailFrom_ = lastOutput_;
    transitionFrames_ = transitionLength_;
  }
  active_ = false;
  rampRemaining_ = 0;
  post_.kill();
}

void Bass808Voice::render(float* output, size_t frames) {
  if (!output) return;
  if (!instrument_) {
    memset(output, 0, frames * sizeof(float));
    return;
  }

  size_t i = 0;
  const int wave = (int)instrument_->waveform;
  // Sub-oscillator: 0 = off, otherwise a 1-based index into the same
  // waveform table (1=sine, 2=tri, 3=saw, 4=square).  Same source waveform
  // table lets the UI share the picker and lets the player dial in any
  // sub timbre - default off keeps the bass clean.
  const int subWave = (int)instrument_->subWaveform;
  const bool subOn = subWave > 0;
  const int subShape = subOn ? (subWave - 1) : 0;
  const float subLevel = instrument_->subLevel / 255.0f;

  // Drive curve: tanh waveshaper, normalized to unity at drive=0 so the
  // post-filter cutoff tracking doesn't shift when drive is bumped. Same
  // pattern DrumSynth uses (drive = 1 + byte/255 * 5.5).
  const float drive = 1.0f + instrument_->drive / 255.0f * 5.5f;
  const float inverseDriveSaturation = 1.0f / audioTanh(drive);

  // A small static gain so the dry oscillator (~ +/-1) lands in roughly the
  // same loudness band as Braids/DrumSynth after the post stage.  0.7 leaves
  // headroom for the sub osc and for the drive stage's harmonics.
  constexpr float calibration = 0.70f;

  for (i = 0; i < frames; ++i) {
    if (!active_) break;
    // Update the pitch glide.  When glideAlpha_ == 0 (no glide) the
    // assignment is a direct snap; otherwise a one-pole exponential
    // approach.  Doing this per-sample (rather than per-block) keeps the
    // glide smooth even at the tracker's slowest tick rates.
    if (glideAlpha_ <= 0.0f) {
      glideCents_ = currentCents_;
    } else {
      glideCents_ += (currentCents_ - glideCents_) * glideAlpha_;
    }
    const float freqHz = centsToHz(glideCents_);

    // Main oscillator phase increment in cycles/sample.
    const float mainInc = freqHz / sampleRate_;
    const float main = waveform(phaseMain_, mainInc, wave);

    // Sub oscillator: one octave down.  A separate phase accumulator keeps
    // its phase continuous across glides (instead of halving mainInc which
    // would lose 1 octave of resolution at high pitches).
    float sample = main;
    if (subOn) {
      const float subInc = (freqHz * 0.5f) / sampleRate_;
      const float sub = waveform(phaseSub_, subInc, subShape);
      sample += sub * subLevel;
    }

    // Saturation stage: tanh waveshaper, normalized for unity at drive=0.
    // At low drive this is essentially linear; at high drive it adds the
    // odd-harmonic content that turns a clean sine into a grinding modern
    // 808 bass.  Filter and ADSR run in VoicePostProcessor::process().
    sample = audioTanh(sample * drive) * inverseDriveSaturation;

    sample = post_.process(sample * calibration);
    // De-click ramp: blend from the pre-trigger output into this note over
    // rampFrames_ frames (linear, like SampleVoice's finishFrame ramp).
    if (rampRemaining_) {
      const float blend = (float)(rampFrames_ - rampRemaining_ + 1) / (float)rampFrames_;
      sample = rampFrom_ + (sample - rampFrom_) * blend;
      --rampRemaining_;
    }
    lastOutput_ = sample;
    output[i] = sample;
    // Retire the voice through the de-click tail when its sound is done:
    // VCA — the envelope went fully idle (release finished). TRIG — the
    // one-shot pluck finished when the DECAY completed (the envelope then
    // sits in the sustain stage at level 0 and never goes idle on its own).
    // Level-based checks can't be used: the level is exactly 0 on the first
    // attack sample, which would retire the pluck before it starts.
    const bool done = envelopeMode_ == 0
      ? post_.envelopeStage() == decltype(post_.envelopeStage())::sustain
      : !post_.envelopeActive();
    if (done) {
      active_ = false;
      if (lastOutput_ != 0.0f) {
        tailFrom_ = lastOutput_;
        transitionFrames_ = transitionLength_;
      }
    }
  }

  // De-click tail: fade from the captured end/cut output down to silence
  // (same 3 ms linear fade as SampleVoice's kill transition). Continue
  // from where the main loop stopped; never restart the buffer at 0.
  if (i < frames) {
    size_t tail = 0;
    for (; tail < frames - i && transitionFrames_; ++tail) {
      const float weight = float(transitionFrames_ - 1) / float(transitionLength_ - 1);
      lastOutput_ = tailFrom_ * weight;
      output[i + tail] = lastOutput_;
      --transitionFrames_;
    }
    if (tail < frames - i) memset(output + i + tail, 0, (frames - i - tail) * sizeof(float));
  }
}
