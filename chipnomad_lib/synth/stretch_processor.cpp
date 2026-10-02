#include "stretch_processor.h"

#include "../project_instruments.h"

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
                                 float tickRateHz, float pitchSemitones,
                                 uint8_t startMarker, uint8_t endMarker) {
  if (!stretch_ || !sample || !sample->data || sample->frameCount == 0 || stretchMode == 0) {
    configured_ = false;
    active_ = false;
    return;
  }
  auto* s = static_cast<signalsmith::stretch::SignalsmithStretch<float>*>(stretch_);

  sampleData_ = sample->data;
  sampleFrames_ = sample->frameCount;
  sampleChannels_ = sample->channels >= 2 ? 2 : 1;
  sampleRate_ = sample->sampleRate > 0 ? (double)sample->sampleRate : (double)outputSampleRate_;
  loopMode_ = sample->loopMode > 2 ? 0 : sample->loopMode;

  // Source window: same Start/End mapping as the plain playback path, with
  // the same reverse detection (Start > End plays the window backwards).
  uint32_t startFrame = (uint32_t)((uint64_t)startMarker * (sampleFrames_ - 1) / 255);
  uint32_t endFrame = endMarker == 255
    ? sampleFrames_
    : (uint32_t)((uint64_t)(endMarker + 1) * sampleFrames_ / 256);
  bool reverse = startMarker > endMarker;
  if (reverse) {
    uint32_t swap = startFrame;
    startFrame = endFrame;
    endFrame = swap;
  }
  if (endFrame <= startFrame) endFrame = startFrame + 1;
  if (endFrame > sampleFrames_) endFrame = sampleFrames_;
  bool windowChanged = sourceStartFrame_ != startFrame || sourceEndFrame_ != endFrame ||
    sourceReverse_ != reverse;
  sourceStartFrame_ = startFrame;
  sourceEndFrame_ = endFrame;
  sourceReverse_ = reverse;
  sourceDirection_ = reverse ? -1.0 : 1.0;

  // Target duration: stretchMode musical divisions at the current tempo.
  // 24 ticks = 1 beat, 96 ticks = 1 bar (4 beats).
  static const uint32_t kTargetTicks[7] = {0, 24, 48, 96, 192, 384, 768};
  uint32_t targetTicks = kTargetTicks[stretchMode <= 6 ? stretchMode : 0];
  double bpm = (double)tickRateHz * 60.0 / 24.0;
  double targetSeconds = (double)targetTicks / ((double)tickRateHz > 0.0 ? (double)tickRateHz : 50.0);
  (void)bpm;
  double sourceSeconds = (double)(sourceEndFrame_ - sourceStartFrame_) / sampleRate_;
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
    inputAccumulator_ = 0.0;
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
  auto* s = static_cast<signalsmith::stretch::SignalsmithStretch<float>*>(stretch_);
  // Prime the stretcher's internal latency so output starts immediately.
  // outputSeek allocates and resets internally; safe here (not in render()).
  size_t seekLength = s->outputSeekLength((float)(1.0 / stretchRatio_));
  if (seekLength > (size_t)(sourceEndFrame_ - sourceStartFrame_)) {
    seekLength = (size_t)(sourceEndFrame_ - sourceStartFrame_);
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
    // When the source is exhausted: loop modes wrap, one-shot feeds silence
    // and lets the stretcher drain its tail.
    bool exhausted = false;
    for (int i = 0; i < inputCount; ++i) {
      if (sourceReverse_) {
        if (sourcePosition_ <= (double)sourceStartFrame_) {
          if (loopMode_ == 1) {
            // Wrap to the top of the window, clamped inside [start, end) so
            // the interpolation never reads past the end of the sample.
            double length = (double)(sourceEndFrame_ - sourceStartFrame_);
            sourcePosition_ = (double)sourceEndFrame_ - 1.0 -
              std::fmod((double)sourceStartFrame_ - sourcePosition_, length);
            if (sourcePosition_ >= (double)sourceEndFrame_) sourcePosition_ = (double)sourceEndFrame_ - 1.0;
          } else if (loopMode_ == 2) {
            sourceDirection_ = 1.0;
            sourceReverse_ = false;
            // Reflect back into the window: the position may have stepped
            // below the start already.
            sourcePosition_ = (double)sourceStartFrame_ +
              ((double)sourceStartFrame_ - sourcePosition_);
            if (sourcePosition_ >= (double)sourceEndFrame_) sourcePosition_ = (double)sourceEndFrame_ - 1.0;
          } else {
            exhausted = true;
          }
        }
      } else if (sourcePosition_ >= (double)sourceEndFrame_) {
        if (loopMode_ == 1) {
          double length = (double)(sourceEndFrame_ - sourceStartFrame_);
          sourcePosition_ = sourceStartFrame_ +
            std::fmod(sourcePosition_ - (double)sourceStartFrame_, length);
        } else if (loopMode_ == 2) {
          sourceDirection_ = -1.0;
          sourceReverse_ = true;
          // Reflect back into the window: the position may have stepped past
          // the end already.
          double last = (double)(sourceEndFrame_ - 1);
          sourcePosition_ = last - (sourcePosition_ - last);
          if (sourcePosition_ < (double)sourceStartFrame_) sourcePosition_ = (double)sourceStartFrame_;
        } else {
          exhausted = true;
        }
      }
      if (exhausted) {
        for (size_t c = 0; c < kMaxChannels; ++c) inBuf_[(size_t)i * kMaxChannels + c] = 0.0f;
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
}
