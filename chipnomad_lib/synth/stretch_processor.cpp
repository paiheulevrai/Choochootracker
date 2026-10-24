#include "stretch_processor.h"

#include "../project_instruments.h"
#include "loop_xfade.h"

#include <cmath>
#include <new>
#include <signalsmith-stretch.h>

// Duck-typed buffer adapters for the Signalsmith API. The library only needs
// `obj[c][i]` to yield a (reference to a) sample: channel c, frame i. Our
// scratch buffers are interleaved stereo (frame stride kMaxChannels), so the
// channel proxy adds the channel offset on top of the frame offset.
namespace {
struct StretchIn {
  const float* base;
  int frameOffset;
  struct Chan {
    const float* d;
    int frameOffset;
    int channel;
    float operator[](int i) const {
      return d[(size_t)(i + frameOffset) * 2 + (size_t)channel];
    }
  };
  Chan operator[](int c) const { return Chan{base, frameOffset, c}; }
};

struct StretchOut {
  float* base;
  int frameOffset;
  struct Chan {
    float* d;
    int frameOffset;
    int channel;
    float& operator[](int i) const {
      return d[(size_t)(i + frameOffset) * 2 + (size_t)channel];
    }
  };
  Chan operator[](int c) const { return Chan{base, frameOffset, c}; }
};
}  // namespace

StretchProcessor::~StretchProcessor() {
  delete static_cast<signalsmith::stretch::SignalsmithStretch<float>*>(stretch_);
}

void StretchProcessor::configure(const InstrumentSample* sample, uint8_t stretchMode,
                                 float tickRateHz, float pitchSemitones,
                                 uint8_t startMarker, uint8_t endMarker, uint8_t loopMode,
                                 uint8_t loopStart) {
  configure(sample, stretchMode, 0, tickRateHz, pitchSemitones, startMarker, endMarker, loopMode,
            loopStart);
}

void StretchProcessor::init(double outputSampleRate, bool cheap) {
  outputSampleRate_ = outputSampleRate;
  cheap_ = cheap;
  if (!stretch_) {
    // Fixed seed: deterministic output across runs.
    stretch_ = new (std::nothrow) signalsmith::stretch::SignalsmithStretch<float>(0xC7C0u);
  }
  if (stretch_) {
    auto* s = static_cast<signalsmith::stretch::SignalsmithStretch<float>*>(stretch_);
    if (cheap_) {
      s->presetCheaper(kMaxChannels, (float)outputSampleRate_, true);
    } else {
      s->presetDefault(kMaxChannels, (float)outputSampleRate_, false);
    }
  }
  configured_ = false;
  active_ = false;
  reset();
}

void StretchProcessor::configure(const InstrumentSample* sample, uint8_t stretchMode,
                                 uint16_t speedPercent, float tickRateHz, float pitchSemitones,
                                 uint8_t startMarker, uint8_t endMarker, uint8_t loopMode,
                                 uint8_t loopStart) {
  if (!stretch_ || !sample || !sample->data || sample->frameCount == 0 ||
      (stretchMode == 0 && speedPercent == 0)) {
    configured_ = false;
    active_ = false;
    return;
  }
  auto* s = static_cast<signalsmith::stretch::SignalsmithStretch<float>*>(stretch_);

  sampleData_ = sample->data;
  sampleFrames_ = sample->frameCount;
  sampleChannels_ = sample->channels >= 2 ? 2 : 1;
  sampleRate_ = sample->sampleRate > 0 ? (double)sample->sampleRate : (double)outputSampleRate_;
  // Effective playback mode (instrument setting or SPL override): 0 one-shot,
  // 1 loop, 2 loop with X-fade, 3 ping-pong, 4 ping-pong with X-fade. The
  // X-fade variants wrap exactly like the plain modes; the crossfade is
  // applied to the material fed into the stretcher at the wrap (see the
  // input fill loop below).
  loopMode_ = loopMode > 4 ? 0 : loopMode;
  bool xfadeOn = loopMode_ == 2 || loopMode_ == 4;
  if (loopMode_ == 2 || loopMode_ == 4) loopMode_ -= 1;

  // Source window: same Start/End mapping as the plain playback path, with
  // the same reverse detection (Start > End plays the window backwards).
  uint32_t startFrame = sampleMarkerToStartFrame(sampleFrames_, startMarker);
  uint32_t endFrame = sampleMarkerToEndFrame(sampleFrames_, endMarker);
  bool reverse = startMarker > endMarker;
  if (reverse) {
    uint32_t swap = startFrame;
    startFrame = endFrame;
    endFrame = swap;
  }
  if (endFrame <= startFrame) endFrame = startFrame + 1;
  if (endFrame > sampleFrames_) endFrame = sampleFrames_;
  // windowReverse_ records the CONFIGURED window direction (marker order);
  // sourceReverse_/sourceDirection_ are runtime playback state that ping-pong
  // flips in process(). Comparing the runtime flag here would treat every
  // direction flip as a window change (re-seek + playhead reset), and the
  // engine's per-tick reconfigure would stomp the flip back to forward.
  bool windowChanged = sourceStartFrame_ != startFrame || sourceEndFrame_ != endFrame ||
    windowReverse_ != reverse;
  sourceStartFrame_ = startFrame;
  sourceEndFrame_ = endFrame;
  windowReverse_ = reverse;

  // Loop wrap point: the EFFECTIVE loop start marker (instrument setting
  // plus the LST FX offset) when a loop mode is active, clamped inside the
  // window (start <= loop < end), the window start otherwise. Mirrors
  // SampleVoice::configure. Deliberately NOT part of windowChanged: tweaking
  // the marker mid-note must not re-seek the playhead, the next wrap simply
  // picks up the new point.
  if (loopMode_ != 0) {
    loopStartFrame_ = sampleMarkerToStartFrame(sampleFrames_, loopStart);
    if (loopStartFrame_ < sourceStartFrame_) loopStartFrame_ = sourceStartFrame_;
    if (loopStartFrame_ >= sourceEndFrame_) loopStartFrame_ = sourceEndFrame_ - 1;
  } else {
    loopStartFrame_ = sourceStartFrame_;
  }

  // X-fade window: a fraction of the loop span, clamped so the incoming and
  // outgoing windows never overlap (the incoming pass must finish fading in
  // before the outgoing pass reaches the seam). Mirrors SampleVoice's
  // quarter rule. The window is recomputed on every configure() - the wrap
  // branches below only consult it when the playhead approaches the seam.
  xfadeLength_ = 0;
  if (xfadeOn && loopMode_ != 0) {
    uint32_t loopLength = sourceEndFrame_ - loopStartFrame_;
    uint32_t fadeLen = loopLength / 4;
    xfadeLength_ = fadeLen;
  }

  double sourceSeconds = (double)(sourceEndFrame_ - sourceStartFrame_) / sampleRate_;
  double targetSeconds;
  if (stretchMode != 0) {
    // Target duration: stretchMode musical divisions at the current tempo.
    // 24 ticks = 1 beat, 96 ticks = 1 bar (4 beats).
    static const uint32_t kTargetTicks[7] = {0, 24, 48, 96, 192, 384, 768};
    uint32_t targetTicks = kTargetTicks[stretchMode <= 6 ? stretchMode : 0];
    targetSeconds = (double)targetTicks / ((double)tickRateHz > 0.0 ? (double)tickRateHz : 50.0);
  } else {
    // Speed mode keeps pitch steady and changes duration, matching the dirty
    // granular path: 200% speed consumes twice as much source per output frame.
    targetSeconds = sourceSeconds * 100.0 / (double)speedPercent;
  }
  stretchRatio_ = (sourceSeconds > 0.0 && targetSeconds > 0.0)
    ? targetSeconds / sourceSeconds
    : 1.0;
  if (stretchRatio_ < 0.03125) stretchRatio_ = 0.03125;   // 32x compression cap
  if (stretchRatio_ > 64.0) stretchRatio_ = 64.0;         // 64x expansion cap

  // Pitch is applied by the stretcher (time stays stretched, pitch shifts).
  // Audio-safe to change mid-note: it only affects future spectra.
  s->setTransposeSemitones((float)pitchSemitones, 8000.0f / (float)outputSampleRate_);

  sourceStep_ = sampleRate_ / outputSampleRate_;

  // Per-tick reconfiguration (BPM/pitch/marker changes) must not disturb an
  // active note: keep the running position and accumulator, just follow the
  // new ratio. Only a change of the actual source window forces a re-seek.
  if (!configured_ || windowChanged) {
    sourcePosition_ = (double)sourceStartFrame_;
    sourceReverse_ = windowReverse_;
    sourceDirection_ = windowReverse_ ? -1.0 : 1.0;
    inputAccumulator_ = 0.0;
    xfadeActive_ = false;
    xfadePhase_ = 0.0;
    primed_ = false;
    draining_ = false;
    active_ = false;
  }
  configured_ = true;
}

void StretchProcessor::noteOn() {
  if (!configured_ || !stretch_) {
    active_ = false;
    return;
  }
  // A new note always starts from the window edge in the window's own
  // direction; leftover ping-pong direction from a previous note must not
  // leak into the priming seek.
  sourceReverse_ = windowReverse_;
  sourceDirection_ = windowReverse_ ? -1.0 : 1.0;
  // A new note must not resume a fade armed for the previous one.
  xfadeActive_ = false;
  xfadePhase_ = 0.0;
  auto* s = static_cast<signalsmith::stretch::SignalsmithStretch<float>*>(stretch_);
  // Priming seek: outputSeek consumes its input as warm-up and renders it as
  // the beginning of the output, so the live feed continues from
  // start + seekLength*step and the first pass plays from the region edge.
  size_t seekLength = s->outputSeekLength((float)(1.0 / stretchRatio_));
  const double seekFrames = (double)seekLength * sourceStep_;
  const double windowFrames = (double)(sourceEndFrame_ - sourceStartFrame_);
  // When the window cannot supply the seek (short sample or extreme stretch),
  // the clamped seek produces only a near-silent pre-roll and parks the feed
  // at the region edge, where the wrap branches immediately fold it to the
  // loop start marker - the first pass would never play from the region
  // start. In loop modes a feed landing at/past the loop start marker is
  // equally wrong: playback would begin at the marker instead of the region
  // marker. Skip the seek and start cold at the region edge; the stretcher
  // fades in naturally while the first pass plays from the region marker.
  bool skipSeek = seekFrames >= windowFrames;
  if (!skipSeek && loopMode_ != 0) {
    skipSeek = sourceReverse_
      ? (double)(sourceEndFrame_ - 1) - seekFrames <= (double)loopStartFrame_
      : (double)sourceStartFrame_ + seekFrames >= (double)loopStartFrame_;
  }
  if (skipSeek) {
    s->reset();
    sourcePosition_ = sourceReverse_ ? (double)(sourceEndFrame_ - 1)
                                     : (double)sourceStartFrame_;
    inputAccumulator_ = 0.0;
    primed_ = true;
    draining_ = false;
    active_ = true;
    return;
  }
  struct SeekIn {
    const int16_t* data;
    uint32_t channels;
    uint32_t start;
    uint32_t end;
    double step;
    bool reverse;
    struct Chan {
      const int16_t* d;
      uint32_t channels;
      uint32_t start;
      uint32_t end;
      double step;
      bool reverse;
      uint32_t channel;
      // Reads the source at the same rate the live path will, so the primed
      // content lines up with what process() feeds next. Reverse windows read
      // backwards from the end of the window.
      float operator[](int i) const {
        double position = reverse
          ? (double)end - 1.0 - (double)i * step
          : (double)start + (double)i * step;
        if (position < (double)start) position = (double)start;
        if (position > (double)(end - 1)) position = (double)(end - 1);
        uint32_t frame = (uint32_t)position;
        uint32_t next = frame + 1 < end ? frame + 1 : frame;
        double frac = position - (double)frame;
        uint32_t srcChannel = channels == 1 ? 0 : channel;
        float a = d[(size_t)frame * channels + srcChannel] / 32768.0f;
        float b = d[(size_t)next * channels + srcChannel] / 32768.0f;
        return a + (b - a) * (float)frac;
      }
    };
    Chan operator[](int c) const { return Chan{data, channels, start, end, step, reverse, (uint32_t)c}; }
  };
  s->outputSeek(SeekIn{sampleData_, sampleChannels_, sourceStartFrame_, sourceEndFrame_,
                       sourceStep_, sourceReverse_}, (int)seekLength);
  // outputSeek consumed `seekLength` source frames at the live read rate;
  // continue right after them (backwards for reverse windows).
  sourcePosition_ = sourceReverse_
    ? (double)sourceEndFrame_ - 1.0 - (double)seekLength * sourceStep_
    : (double)sourceStartFrame_ + (double)seekLength * sourceStep_;
  if (sourcePosition_ < (double)sourceStartFrame_) sourcePosition_ = (double)sourceStartFrame_;
  if (sourcePosition_ > (double)(sourceEndFrame_ - 1)) sourcePosition_ = (double)(sourceEndFrame_ - 1);
  inputAccumulator_ = 0.0;
  primed_ = true;
  draining_ = false;
  active_ = true;
}

bool StretchProcessor::process(float* out, size_t frames) {
  if (!active_ || !configured_ || !stretch_) return true;
  auto* s = static_cast<signalsmith::stretch::SignalsmithStretch<float>*>(stretch_);

  size_t done = 0;
  while (done < frames) {
    size_t chunk = frames - done;
    if (chunk > kMaxBlockFrames) chunk = kMaxBlockFrames;

    // Drain mode: the source is gone; copy out the tail flushed at exhaustion.
    if (draining_) {
      size_t n = drainRemaining_ < chunk ? drainRemaining_ : chunk;
      bool bad = false;
      for (size_t i = 0; i < n; ++i) {
        float l = drainBuf_[(drainOffset_ + i) * kMaxChannels];
        float r = drainBuf_[(drainOffset_ + i) * kMaxChannels + 1];
        if (!std::isfinite(l) || !std::isfinite(r)) { bad = true; break; }
        // Short linear fade over the last 64 frames avoids a click at the end.
        size_t remaining = drainRemaining_ - i;
        float gain = remaining < 64 ? (float)remaining / 64.0f : 1.0f;
        out[(done + i) * 2] = l * gain;
        out[(done + i) * 2 + 1] = r * gain;
      }
      if (bad) { active_ = false; return false; }
      drainOffset_ += n;
      drainRemaining_ -= n;
      done += n;
      if (drainRemaining_ == 0) {
        active_ = false;
        primed_ = false;
        draining_ = false;
        return true;
      }
      if (done >= frames) return true;
      continue;
    }

    // How much source material this chunk consumes (fractional accumulator
    // keeps the long-run ratio exact).
    double inputCountFrac = (double)chunk / stretchRatio_;
    int inputCount = (int)std::floor(inputCountFrac + inputAccumulator_);
    inputAccumulator_ += inputCountFrac - (double)inputCount;
    if (inputCount < 0) inputCount = 0;
    // Never overflow the input scratch buffer (strong compression).
    if ((size_t)inputCount > kMaxBlockFrames) inputCount = (int)kMaxBlockFrames;

    // Fill the input buffer from the source window with linear interpolation.
    // When the source is exhausted: loop modes wrap at the LOOP START frame
    // (the window start when no loop is active), one-shot feeds silence
    // and lets the stretcher drain its tail.
    //
    // X-fade modes (3/4): the wrap is crossfaded on the INPUT side. For the
    // last xfadeLength_ source frames before the seam the outgoing pass
    // (still reading across the seam) is mixed with the incoming pass
    // (already reading from the top of the loop), using the same equal-power
    // gains as the plain path's LoopXfade engine. The incoming pointer is
    // phase-aligned with the outgoing one, so the handover at the seam lands
    // exactly on the position the hard wrap would have produced - the fade
    // removes the discontinuity without shifting the loop. The stretcher
    // then sees a smooth input instead of a step, so no spectral artifact
    // is generated at the wrap.
    bool exhausted = false;
    for (int i = 0; i < inputCount; ++i) {
      if (sourceReverse_) {
        if (sourcePosition_ <= (double)loopStartFrame_) {
          if (loopMode_ == 1) {
            // Wrap to the top of the window, clamped inside [start, end) so
            // the interpolation never reads past the end of the sample.
            double length = (double)(sourceEndFrame_ - loopStartFrame_);
            sourcePosition_ = (double)sourceEndFrame_ - 1.0 -
              std::fmod((double)loopStartFrame_ - sourcePosition_, length);
            if (sourcePosition_ >= (double)sourceEndFrame_) sourcePosition_ = (double)sourceEndFrame_ - 1.0;
          } else if (loopMode_ == 3) {
            sourceDirection_ = 1.0;
            sourceReverse_ = false;
            // Reflect back into the window: the position may have stepped
            // below the start already.
            sourcePosition_ = (double)loopStartFrame_ +
              ((double)loopStartFrame_ - sourcePosition_);
            if (sourcePosition_ >= (double)sourceEndFrame_) sourcePosition_ = (double)sourceEndFrame_ - 1.0;
          } else {
            exhausted = true;
          }
        }
      } else if (sourcePosition_ >= (double)sourceEndFrame_) {
        if (loopMode_ == 1) {
          double length = (double)(sourceEndFrame_ - loopStartFrame_);
          sourcePosition_ = loopStartFrame_ +
            std::fmod(sourcePosition_ - (double)loopStartFrame_, length);
        } else if (loopMode_ == 3) {
          sourceDirection_ = -1.0;
          sourceReverse_ = true;
          // Reflect back into the window: the position may have stepped past
          // the end already.
          double last = (double)(sourceEndFrame_ - 1);
          sourcePosition_ = last - (sourcePosition_ - last);
          if (sourcePosition_ < (double)loopStartFrame_) sourcePosition_ = (double)loopStartFrame_;
        } else {
          exhausted = true;
        }
      }
      if (exhausted) {
        for (size_t c = 0; c < kMaxChannels; ++c) inBuf_[(size_t)i * kMaxChannels + c] = 0.0f;
        continue;
      }
      // X-fade window: the playhead is inside the last xfadeLength_ source
      // frames before the seam (or a fade is already in flight). The
      // outgoing pass keeps its natural trajectory across the seam; the
      // incoming pass is phase-aligned with it. The wrap branches above
      // still fold the outgoing pointer - the fade is computed from the
      // pre-wrap geometry, then the mixed read uses the folded position.
      // Descending forward loops (mode 1) hard-wrap like the plain path;
      // only ping-pong (mode 3) arms a fade on the way down.
      bool inWindow = xfadeLength_ > 0 &&
        (xfadeActive_ ||
         (sourceDirection_ > 0.0
           ? (double)sourceEndFrame_ - sourcePosition_ <= (double)xfadeLength_
           : loopMode_ == 3 && sourcePosition_ - (double)loopStartFrame_ <= (double)xfadeLength_));
      if (inWindow) {
        double phase;
        if (!xfadeActive_) {
          phase = (double)xfadeLength_ -
            (sourceDirection_ > 0.0
              ? (double)sourceEndFrame_ - sourcePosition_
              : sourcePosition_ - (double)loopStartFrame_);
          if (phase < 0.0) phase = 0.0;
          if (phase > (double)xfadeLength_) phase = (double)xfadeLength_;
          // Arm: outgoing = the playhead's pre-wrap trajectory, incoming =
          // the wrap target phase-aligned with it. The outgoing pointer is
          // NOT folded here - the mixed read below folds it via sampleAt.
          double outPos = sourcePosition_;
          int outDir = sourceDirection_ > 0.0 ? 1 : -1;
          double inPos;
          int inDir;
          if (loopMode_ == 1) {
            // Forward wrap: incoming restarts at the loop start marker.
            inPos = (double)loopStartFrame_ + phase;
            inDir = 1;
          } else if (outDir > 0) {
            // Ping-pong turn at the region end: incoming reads backwards
            // from the last frame, mirroring the fold
            // position_ = last - overshoot.
            inPos = (double)(sourceEndFrame_ - 1) - phase;
            inDir = -1;
          } else {
            // Ping-pong turn at the loop start marker: incoming reads
            // forwards from the marker, mirroring
            // position_ = first + (first - pos).
            inPos = (double)loopStartFrame_ + phase;
            inDir = 1;
          }
          xfadeOutPos_ = outPos;
          xfadeInPos_ = inPos;
          xfadeOutDir_ = outDir;
          xfadeInDir_ = inDir;
          xfadePhase_ = phase;
          xfadeActive_ = true;
        }
        // Mixed read: outgoing keeps its natural trajectory (it may already
        // be past the seam - the fold below maps it back into the window),
        // incoming steps from the wrap target.
        float gOut, gIn;
        xfadeGains(xfadePhase_, gOut, gIn);
        uint32_t outFrame = (uint32_t)xfadeOutPos_;
        uint32_t outNext = outFrame + 1 < sourceEndFrame_ ? outFrame + 1 : outFrame;
        double outFrac = xfadeOutPos_ - (double)outFrame;
        uint32_t inFrame = (uint32_t)xfadeInPos_;
        uint32_t inNext = inFrame + 1 < sourceEndFrame_ ? inFrame + 1 : inFrame;
        double inFrac = xfadeInPos_ - (double)inFrame;
        for (size_t c = 0; c < kMaxChannels; ++c) {
          uint32_t srcChannel = sampleChannels_ == 1 ? 0 : (uint32_t)c;
          float outA = sampleData_[(size_t)outFrame * sampleChannels_ + srcChannel] / 32768.0f;
          float outB = sampleData_[(size_t)outNext * sampleChannels_ + srcChannel] / 32768.0f;
          float outSample = outA + (outB - outA) * (float)outFrac;
          float inA = sampleData_[(size_t)inFrame * sampleChannels_ + srcChannel] / 32768.0f;
          float inB = sampleData_[(size_t)inNext * sampleChannels_ + srcChannel] / 32768.0f;
          float inSample = inA + (inB - inA) * (float)inFrac;
          inBuf_[(size_t)i * kMaxChannels + c] =
            gOut * outSample + gIn * inSample;
        }
        // Advance the fade pointers by the source step. The outgoing pass
        // may cross the seam mid-fade: the fold below maps it back into the
        // window, so the mixed read stays inside the sample data.
        xfadeOutPos_ += (double)xfadeOutDir_ * sourceStep_;
        xfadeInPos_ += (double)xfadeInDir_ * sourceStep_;
        xfadePhase_ += sourceStep_;
        if (xfadePhase_ >= (double)xfadeLength_) {
          // Fade complete: hand the incoming pointer to the main playhead.
          xfadeActive_ = false;
          sourcePosition_ = xfadeInPos_;
          sourceDirection_ = (double)xfadeInDir_;
          sourceReverse_ = xfadeInDir_ < 0;
          // The incoming pointer may sit a hair past the seam (fractional
          // step); fold it back into the window so the next iteration
          // starts clean.
          if (sourceReverse_) {
            if (sourcePosition_ < (double)loopStartFrame_) {
              sourcePosition_ = (double)loopStartFrame_;
            }
          } else if (sourcePosition_ >= (double)sourceEndFrame_) {
            sourcePosition_ = (double)loopStartFrame_;
          }
        }
        continue;
      }
      uint32_t frame = (uint32_t)sourcePosition_;
      // The interpolant is a function of position only; the direction is
      // already handled by the position ramp. Blending towards frame - 1 for
      // reverse playback would mirror the fractional part and distort.
      uint32_t next = frame + 1 < sourceEndFrame_ ? frame + 1 : frame;
      double frac = sourcePosition_ - (double)frame;
      for (size_t c = 0; c < kMaxChannels; ++c) {
        uint32_t srcChannel = sampleChannels_ == 1 ? 0 : (uint32_t)c;
        float a = sampleData_[(size_t)frame * sampleChannels_ + srcChannel] / 32768.0f;
        float b = sampleData_[(size_t)next * sampleChannels_ + srcChannel] / 32768.0f;
        inBuf_[(size_t)i * kMaxChannels + c] = a + (b - a) * (float)frac;
      }
      sourcePosition_ += sourceDirection_ * sourceStep_;
    }

    s->process(StretchIn{inBuf_, 0}, (int)inputCount,
               StretchOut{outBuf_, 0}, (int)chunk);

    // Interleave to the output and validate.
    for (size_t i = 0; i < chunk; ++i) {
      float l = outBuf_[i * kMaxChannels];
      float r = outBuf_[i * kMaxChannels + 1];
      if (!std::isfinite(l) || !std::isfinite(r)) {
        active_ = false;
        return false;
      }
      out[(done + i) * 2] = l;
      out[(done + i) * 2 + 1] = r;
    }
    done += chunk;

    if (exhausted && loopMode_ == 0) {
      // One-shot: the source is gone. Flush the stretcher's tail ONCE into
      // the drain buffer (flush() resets internal state, so it must not be
      // called again), then copy it out over the next render calls.
      draining_ = true;
      size_t tail = (size_t)s->outputLatency() + chunk;
      if (tail > kMaxDrainFrames) tail = kMaxDrainFrames;
      memset(drainBuf_, 0, tail * kMaxChannels * sizeof(float));
      s->flush(StretchOut{drainBuf_, 0}, (int)tail);
      drainRemaining_ = tail;
      drainOffset_ = 0;
      // Re-run this iteration in drain mode so the tail starts immediately.
      done -= chunk;
    }
  }
  return true;
}

void StretchProcessor::reset() {
  inputAccumulator_ = 0.0;
  sourcePosition_ = 0.0;
  sourceDirection_ = 1.0;
  sourceReverse_ = false;
  active_ = false;
  primed_ = false;
  draining_ = false;
  drainRemaining_ = 0;
  drainOffset_ = 0;
  xfadeActive_ = false;
  xfadePhase_ = 0.0;
}

// Equal-power gains, quantized to 256 steps - the same curve the plain
// path's LoopXfade engine uses (cos/sin quarter period, phase 0 -> gOut=1,
// phase length -> gIn=1). Reads the shared kLoopXfadeTables so both paths
// stay bit-identical.
void StretchProcessor::xfadeGains(double phase, float& gOut, float& gIn) const {
  int index = (int)(phase * 255.0 / (double)xfadeLength_ + 0.5);
  if (index < 0) index = 0;
  if (index > 255) index = 255;
  gOut = kLoopXfadeTables.out[index];
  gIn = kLoopXfadeTables.in[index];
}
