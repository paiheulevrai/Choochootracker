#ifndef CHOOCHOO_STRETCH_PROCESSOR_H
#define CHOOCHOO_STRETCH_PROCESSOR_H

#include <stddef.h>
#include <stdint.h>

struct InstrumentSample;

// Musical-division time-stretch for sample voices, backed by
// signalsmith::stretch::SignalsmithStretch (vendored header-only library).
//
// A voice with Stretch enabled plays its Start/End window so that it lasts a
// chosen musical division (1 beat .. 8 bars) at the current project tempo.
// The stretch ratio is recomputed on every configure() so tempo changes are
// followed in real time. The granular speedPercent path is bypassed while
// Stretch is active.
class StretchProcessor {
 public:
  StretchProcessor() = default;
  ~StretchProcessor();
  StretchProcessor(const StretchProcessor&) = delete;
  StretchProcessor& operator=(const StretchProcessor&) = delete;

  void init(double outputSampleRate, bool cheap);
  // stretchMode: 0 = off, 1 = 1 beat, 2 = 2 beats, 3 = 1 bar, 4 = 2 bars,
  // 5 = 4 bars, 6 = 8 bars. tickRateHz is the project tick rate (BPM =
  // tickRateHz * 60 / 24). pitchSemitones is the voice's transposition in
  // semitones (applied by the stretcher so playback speed stays 1:1).
  // loopMode is the EFFECTIVE playback mode (0 one-shot, 1 loop, 2 loop
  // X-fade, 3 ping-pong, 4 ping-pong X-fade) after SPL/motion overrides -
  // passed in by the engine instead of being read from the sample so
  // FX-driven modes apply while stretching. loopStart is
  // likewise the EFFECTIVE loop start marker (instrument setting plus the LST
  // FX offset) - passed in for the same reason.
  void configure(const InstrumentSample* sample, uint8_t stretchMode,
                 uint16_t speedPercent, float tickRateHz, float pitchSemitones, uint8_t startMarker,
                 uint8_t endMarker, uint8_t loopMode, uint8_t loopStart = 0);
  void configure(const InstrumentSample* sample, uint8_t stretchMode,
                 float tickRateHz, float pitchSemitones, uint8_t startMarker,
                 uint8_t endMarker, uint8_t loopMode, uint8_t loopStart = 0);
  void noteOn();
  // Renders up to `frames` stereo-interleaved output frames into `out`.
  // Returns false when the voice must be killed (non-finite audio or, for
  // non-looping material, the tail has been fully drained).
  bool process(float* out, size_t frames);
  void reset();
  bool active() const { return active_; }
  // True once noteOn() has primed the stretcher for the current note; lets the
  // voice lazy-prime when Stretch is enabled mid-note without retriggering
  // after a one-shot drain has finished.
  bool primed() const { return primed_; }
  // Current read position in source sample frames (for the playback marker).
  double sourcePosition() const { return sourcePosition_; }

 private:
  static constexpr size_t kMaxBlockFrames = 8192;
  static constexpr size_t kMaxChannels = 2;

  void* stretch_ = nullptr;  // signalsmith::stretch::SignalsmithStretch<float>*
  float inBuf_[kMaxBlockFrames * kMaxChannels];
  float outBuf_[kMaxBlockFrames * kMaxChannels];
  const int16_t* sampleData_ = nullptr;
  uint32_t sampleFrames_ = 0;
  uint32_t sampleChannels_ = 1;
  double sampleRate_ = 0.0;
  uint32_t sourceStartFrame_ = 0;
  uint32_t sourceEndFrame_ = 0;
  double sourcePosition_ = 0.0;
  double sourceStep_ = 1.0;
  double sourceDirection_ = 1.0;
  bool sourceReverse_ = false;
  // Configured window direction (marker order). sourceReverse_ above is
  // runtime playback state that ping-pong flips while playing; this one only
  // changes when the window itself is reconfigured.
  bool windowReverse_ = false;
  double stretchRatio_ = 1.0;
  double inputAccumulator_ = 0.0;
  double outputSampleRate_ = 0.0;
  uint8_t loopMode_ = 0;
  // Loop wrap point in source frames: the LOOP START marker clamped inside
  // the window when a loop mode is active, the window start otherwise.
  uint32_t loopStartFrame_ = 0;
  // Input-side wrap crossfade (X-fade loop modes 3/4): equal-power gains
  // applied to the material fed into the stretcher around the seam. The
  // outgoing pass keeps its natural trajectory across the seam; the
  // incoming pass is phase-aligned with it, so the handover lands exactly
  // on the position the hard wrap would have produced.
  uint32_t xfadeLength_ = 0;
  bool xfadeActive_ = false;
  double xfadeOutPos_ = 0.0;
  double xfadeInPos_ = 0.0;
  int xfadeOutDir_ = 1;
  int xfadeInDir_ = 1;
  double xfadePhase_ = 0.0;
  // Equal-power gain pair for phase `p` (0..xfadeLength_): the same
  // 256-entry Q15 equal-power curve the plain path's LoopXfade engine uses.
  void xfadeGains(double phase, float& gOut, float& gIn) const;
  bool cheap_ = false;
  bool active_ = false;
  bool configured_ = false;
  bool primed_ = false;
  bool draining_ = false;
  size_t drainRemaining_ = 0;
  size_t drainOffset_ = 0;
  // Tail buffer: flushed once at exhaustion, then copied out over calls.
  static constexpr size_t kMaxDrainFrames = 16384;
  float drainBuf_[kMaxDrainFrames * kMaxChannels];
};

#endif
