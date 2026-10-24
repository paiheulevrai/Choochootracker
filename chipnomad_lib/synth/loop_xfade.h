#ifndef CHOOCHOO_LOOP_XFADE_H
#define CHOOCHOO_LOOP_XFADE_H

#include <stddef.h>
#include <stdint.h>

// Seamless loop crossfade for sample voices.
//
// When a looped voice reaches its wrap point the raw wrap is a hard cut: the
// last sample before the seam is followed by the first sample after it, and
// any phase mismatch between the two loop passes clicks. The X-fade engine
// overlaps the two passes instead: for the last N frames before the seam it
// mixes the outgoing pass (still reading across the seam) with the incoming
// pass (already reading from the top of the loop), fading one out while the
// other fades in. The seam itself stays phase-aligned - the incoming pointer
// is placed so that at every mix index it reads the sample the loop would
// have produced at that index, so the fade only removes the discontinuity,
// never shifts the loop.
//
// Real-time safety: all state is member-resident, no allocation, no locks,
// no transcendentals on the audio thread. The equal-power curve is a
// compile-time 256-entry table.
class LoopXfade {
 public:
  LoopXfade() = default;

  // Configure the fade. lengthFrames is the fade window in source frames
  // (already clamped by the caller so incoming and outgoing windows never
  // overlap); amount 0 disables the engine entirely and render() short
  // circuits to the plain path.
  void configure(uint32_t lengthFrames) {
    length_ = lengthFrames;
    active_ = false;
    phase_ = 0;
  }

  // True when the engine will crossfade (amount > 0 and a window exists).
  bool enabled() const { return length_ > 0; }

  // Configured fade window in source frames (0 = disabled). The voice reads
  // it to detect when the playhead enters the fade window before a wrap.
  uint32_t length() const { return length_; }

  // Abort an in-flight fade, keeping the configured window. A new note must
  // not resume a fade armed for the previous one.
  void reset() {
    active_ = false;
    phase_ = 0;
  }

  // True while a fade is in progress (between arm() and completion).
  bool active() const { return active_; }

  // Arm a fade between two read pointers.
  //   outgoingPos: where the fading-out pass keeps reading (it continues
  //     along its natural trajectory to the seam without wrapping).
  //   incomingPos: where the fading-in pass starts reading.
  //   outgoingDir/incomingDir: read directions of the two passes (+1/-1).
  //   phase: initial fade phase in frames. 0 arms at the window start; the
  //     voice arms mid-window when the playhead entered it mid-step, so the
  //     first mixed sample continues the plain path without a jump.
  // For a forward-loop wrap: outgoing = playhead, incoming = loop start +
  //   phase, both directions +1 (at mix index i the incoming reads
  //   loopStart + i while the outgoing reads loopEnd - fadeLen + i).
  // For a ping-pong turn at the loop end: outgoing = playhead, incoming =
  //   loop end - 1 - phase, directions +1/-1 (the incoming pass reads
  //   backwards from the turn point).
  // For a ping-pong turn at the loop start: outgoing = playhead, incoming =
  //   loop start + phase, directions -1/+1.
  void arm(double outgoingPos, double incomingPos, int outgoingDir,
           int incomingDir, double phase = 0.0) {
    if (length_ == 0) return; // disabled engine never arms
    outgoingPos_ = outgoingPos;
    incomingPos_ = incomingPos;
    outgoingDir_ = outgoingDir;
    incomingDir_ = incomingDir;
    phase_ = phase;
    active_ = true;
  }

  // Advance the fade by `step` source frames. Both read pointers move with
  // the phase (each along its own direction), so the caller never touches
  // the pointers directly. Returns false exactly once when the fade
  // completes; the caller then transfers the incoming pointer to the main
  // playhead (see takeIncoming).
  bool advance(double step) {
    if (!active_) return false;
    outgoingPos_ += step * outgoingDir_;
    incomingPos_ += step * incomingDir_;
    phase_ += step;
    if (phase_ >= (double)length_) {
      active_ = false;
      return false;
    }
    return true;
  }

  // After advance() reports completion, hand the incoming pointer to the
  // main playhead.
  void takeIncoming(double* position, int* direction) const {
    *position = incomingPos_;
    *direction = incomingDir_;
  }

  // Outgoing read pointer (the fading-out pass). The voice mirrors it into
  // the main playhead during the fade so the UI cursor keeps moving.
  double outgoingPosition() const { return outgoingPos_; }

  // Mixed sample for one channel at the current fade phase. `reader` reads
  // a source frame (linear interpolation is done by the caller's sampleAt,
  // so both pointers interpolate identically to the plain path).
  // 2 lookups per sample inside the window, 0 outside (the caller only
  // calls this while active()).
  template <typename Reader>
  float sample(Reader&& reader, int channel) const {
    float gOut, gIn;
    gains(phase_, gOut, gIn);
    return gOut * reader(outgoingPos_, channel) +
           gIn * reader(incomingPos_, channel);
  }

  // Equal-power gain pair for phase `p` (in frames, 0..length). The
  // outgoing gain follows cos(t*pi/2), the incoming gain sin(t*pi/2), so
  // the two passes sum to constant power. Implemented as a 256-entry table
  // lookup on the normalized phase - no transcendentals on the audio
  // thread. The mapping puts phase 0 at index 0 (gOut=1, gIn=0) and phase
  // `length` at index 255 (gOut=0, gIn=1), so the fade reaches EXACT full
  // handover at its end: the last mixed sample is the pure incoming read,
  // identical to what the plain path resumes with.
  void gains(double phase, float& gOut, float& gIn) const {
    int index = (int)(phase * 255.0 / (double)length_ + 0.5);
    if (index < 0) index = 0;
    if (index > 255) index = 255;
    gOut = gainOut(index);
    gIn = gainIn(index);
  }

  // Current fade phase in frames (for tests and diagnostics).
  double phase() const { return phase_; }

  // Equal-power gain tables, computed at compile time. Defined after the
  // class: a constexpr constructor cannot run in a constant expression
  // until its class is fully defined, so the instance cannot be a static
  // member initialized inside the class body.
  struct Tables {
    float out[256] = {};
    float in[256] = {};
    constexpr Tables();
  };
  float gainOut(int index) const;
  float gainIn(int index) const;

 private:
  // Fade window in source frames; 0 disables the engine.
  uint32_t length_ = 0;
  bool active_ = false;
  double phase_ = 0.0;
  double outgoingPos_ = 0.0;
  double incomingPos_ = 0.0;
  int outgoingDir_ = 1;
  int incomingDir_ = 1;
};

// Equal-power gains, quantized to 256 steps. Outgoing follows cos(t*pi/2),
// incoming sin(t*pi/2) with t = 0..1 across the fade. Computed at compile
// time; the float tables are the Q15 values scaled back to 0..1 so the
// render path only does table reads and multiplies.
namespace loopxfade {
constexpr int kTableBits = 15;
constexpr int32_t kTableScale = 1 << kTableBits;
constexpr int32_t quantizeCos(int i) {
  // 32768 * cos(i * pi / 2 / 255), rounded. The angle step is pi/510:
  // i runs 0..255 across a quarter period. Evaluated by the compiler;
  // no runtime trig.
  const double angle = (double)i * 3.14159265358979323846 / 510.0;
  return (int32_t)(kTableScale * __builtin_cos(angle) + 0.5);
}
constexpr int32_t quantizeSin(int i) {
  const double angle = (double)i * 3.14159265358979323846 / 510.0;
  return (int32_t)(kTableScale * __builtin_sin(angle) + 0.5);
}
}  // namespace loopxfade

inline constexpr LoopXfade::Tables::Tables() {
  for (int i = 0; i < 256; ++i) {
    out[i] = (float)loopxfade::quantizeCos(i) / (float)loopxfade::kTableScale;
    in[i] = (float)loopxfade::quantizeSin(i) / (float)loopxfade::kTableScale;
  }
}

inline constexpr LoopXfade::Tables kLoopXfadeTables{};

inline float LoopXfade::gainOut(int index) const { return kLoopXfadeTables.out[index]; }
inline float LoopXfade::gainIn(int index) const { return kLoopXfadeTables.in[index]; }

#endif
