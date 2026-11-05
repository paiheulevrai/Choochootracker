#ifndef CHOOCHOO_BASS808_VOICE_H
#define CHOOCHOO_BASS808_VOICE_H

#include <stddef.h>
#include <stdint.h>
#include "voice_post_processor.h"
#include "../project_instruments.h"

// Modern "808" sustained bass synthesizer voice.
//
// Not a TR-808 kick-drum recreation. This is a tuned sub-bass synthesizer
// voice in the spirit of modern trap/hip-hop 808 bass: a sustained,
// pitch-trackable mono (or chord) oscillator with selectable waveforms,
// a one-octave-below sub-oscillator, an ADSR-controlled VCA, a saturating
// drive stage, and a one-pole pitch slew (portamento/glide).
//
// All shared post-processing (multimode filter + ADSR + gain) is delegated
// to VoicePostProcessor, exactly like Braids / DrumSynth / Sintered. The
// instrument's filterEnabled/filterCharacter/filterMode/filterSlope24dB/
// filterCutoffHz/filterResonance/filterColor and attack/decay/sustain/
// release/envelopeShape bytes drive that stage through the same code path
// every other synth uses, so the voice itself only owns its oscillator
// bank, the glide state, and the waveshaper.
class Bass808Voice {
 public:
  void init(float sampleRate);

  // Reconfigures DSP state from the project's Instrument struct + the
  // per-row resolved cutoff/resonance/gain.  Mirrors DrumSynthVoice /
  // SinteredVoice so updateBass808Voices can call it on every block.
  //
  // pitchCents: absolute pitch in cents (A4=440 Hz = 6900), already including
  //             the track's fineOffset + pitch modulation. 6900 (A4) maps to
  //             440 Hz via the standard 12-TET formula.
  // glideValue: 0..255 glide time. 0 = instant (no portamento), 255 = ~1.05 s.
  //             The voice reads this every block, so fxGLD can smooth it via
  //             the shared slewEngineFX path the same way DrumSynth does for
  //             fxDDC etc.  When the value changes between blocks the slew
  //             coefficient is updated without resetting the running target.
  void configure(const InstrumentBass808* instrument, float pitchCents, float gain,
                 uint16_t cutoffHz, uint8_t resonance, uint8_t glideValue);

  void noteOn();
  void noteOff();
  void kill();

  void render(float* output, size_t frames);

  bool active() const { return active_; }
  float envelopeLevel() const { return post_.envelopeLevel(); }

 private:
  // Single-sample oscillator. phase is in [0,1); the accumulator is updated
  // by frequency/sampleRate each call. shape 0=sine, 1=triangle, 2=saw,
  // 3=square.  Band-limiting is intentionally minimal: an 808 bass sits at
  // the bottom of the spectrum where aliasing is rarely audible, and the
  // shared MultimodeFilter downstream removes most out-of-band content.
  static float waveform(float& phase, float phaseInc, int shape);
  // Glide coefficient (one-pole alpha) for a given glideValue byte.
  float glideCoefficient(uint8_t glideValue) const;

  const InstrumentBass808* instrument_ = nullptr;
  InstrumentBass808 parameters_{};
  VoicePostProcessor<> post_;

  float sampleRate_ = 48000.0f;
  float phaseMain_ = 0.0f;     // [0,1)
  float phaseSub_  = 0.0f;     // [0,1), runs at half the main rate
  float currentCents_ = 6900.0f;  // last computed pitch (target glide end)
  float glideCents_   = 6900.0f;  // instantaneous pitch (post-glide)
  float glideAlpha_   = 0.0f;     // one-pole coefficient, 0 = snap, ~1 = slow
  bool  active_ = false;
};

#endif  // CHOOCHOO_BASS808_VOICE_H
