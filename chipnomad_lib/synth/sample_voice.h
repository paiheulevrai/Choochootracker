#ifndef CHOOCHOO_SAMPLE_VOICE_H
#define CHOOCHOO_SAMPLE_VOICE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "voice_post_processor.h"
#include "stretch_processor.h"

struct InstrumentSample;

class SampleVoice {
 public:
  void init(float outputSampleRate);
  void configure(const InstrumentSample* sample, float pitchCents, float gain,
                 float speedPercent, uint8_t start, uint8_t end, uint8_t loopMode, uint16_t cutoffHz,
                 uint8_t resonance, int attack = -1, int decay = -1, int sustain = -1,
                 int release = -1, int envelopeShape = -1, uint8_t sliceCount = 0,
                 uint8_t sliceIndex = 0, uint8_t stretchMode = 0, float tickRateHz = 50.0f,
                 uint8_t speedAlgorithm = 0, uint8_t forceReverse = 0);
  void noteOn();
  void noteOff();
  void kill();
  void render(float* output, size_t frames);
  bool active() const { return active_; }
  float envelopeLevel() const { return post_.envelopeLevel(); }
  // Current playback position in source sample frames, whichever render
  // path is active: the stretch processor's read cursor, the granular
  // path's interpolated grain cursor, or the plain path's position.
  // Read from the UI thread for the playback marker - same tolerated
  // cross-thread pattern as the voice monitors.
  double playbackFrame() const {
    if (useStretch_) return stretch_.sourcePosition();
    if (granular_) return grainPosition_[0] + direction_ * step_ * grainAge_[0];
    return position_;
  }

 private:
  const InstrumentSample* sample_;
  float outputSampleRate_;
  double position_;
  double step_;
  int direction_;
  bool reverse_;
  uint8_t loopMode_;
  float timeStretch_;
  bool granular_;
  bool grainExhausted_;
  double grainPosition_[2];
  double nextGrainPosition_;
  uint32_t grainAge_[2];
  uint32_t grainSize_;
  uint32_t grainHop_;
  uint32_t startFrame_;
  uint32_t endFrame_;
  bool active_;
  StretchProcessor stretch_;
  bool useStretch_;
  bool advancePosition();
  float sampleAt(double position, int channel) const;
  float grainSampleAt(double position, int channel) const;
  VoicePostProcessor<> post_;
};

uint8_t sampleNormalizeSlice(uint8_t slice);
void sampleSliceFrames(uint32_t frameCount, uint8_t sliceCount, uint8_t sliceIndex,
                       uint32_t* startFrame, uint32_t* endFrame);

// Slice sentinel encoding (plan decision D1). The single `slice` byte holds
// both the mode and the slice count:
//   0        off
//   1..64    EQUAL with that many slices (legacy values 2/4/8/16/32 stay valid)
//   65..128  AUTO with (value - 64) slices
//   129..192 LAZY with (value - 128) slices
//   193..255 reserved (loads as off)
enum SliceMode : uint8_t {
  sliceModeOff = 0,
  sliceModeEqual = 1,
  sliceModeAuto = 2,
  sliceModeLazy = 3,
};

// Builds a sentinel from a mode and count (count clamped to 1..64; off
// always yields 0 regardless of count).
uint8_t sampleEncodeSlice(SliceMode mode, uint8_t count);
SliceMode sampleDecodeSliceMode(uint8_t slice);
uint8_t sampleDecodeSliceCount(uint8_t slice);
// Pass-through for valid sentinels, 0 for anything else (garbage, reserved
// range). Used when loading so unknown bytes never become live slices.
uint8_t sampleNormalizeSliceEx(uint8_t slice);
struct InstrumentSample;
// 1 when the sample's slice setting makes notes select slices (EQUAL, AUTO
// and LAZY do; LAZY slices are hand-placed but still map C-0 upwards).
int sampleActsAsSliced(const InstrumentSample* sample);

// --- Slice bounds editing (Phase 1, universal editing model) -------------
// sliceBounds[] holds the start frame of each slice; the last slice ends at
// the loop end marker. All helpers are pure functions over the sample: no
// UI, no audio calls. They keep the sentinel's count field in sync with the
// array and clamp everything to PROJECT_SAMPLE_MAX_SLICES.

// Number of populated bounds entries: the sentinel's count when the mode is
// active, else 0. Legacy samples (sentinel set, bounds all zero) report the
// sentinel count - the even-division fallback still applies for playback.
uint8_t sampleSliceBoundCount(const InstrumentSample* sample);

// Fills sliceBounds[0..count-1] with an even division of the loop region
// (start..end markers) and writes the sentinel for the given mode. Returns
// the effective count (clamped to 1..64). This is the EQUAL initializer and
// the Phase 1 AUTO placeholder.
uint8_t sampleSliceInitEven(InstrumentSample* sample, SliceMode mode, uint8_t count);

// LAZY initializer: clears all bounds and sets a single whole-loop slice.
uint8_t sampleSliceInitLazy(InstrumentSample* sample);

// AUTO initializer (Phase 2): runs spectral-flux transient detection over
// the loop region with the sample's autoSensitivity and stores up to
// `count` onsets as bounds. Falls back to the even division when nothing
// is detected (silence, tiny region). Allocates scratch buffers - run it
// with audio paused. Defined in sample_transient.cpp.
uint8_t sampleSliceInitAuto(InstrumentSample* sample, uint8_t count);

// Splits the current slice at its midpoint: inserts a new bound at
// (bounds[index] + bounds[index+1]) / 2 (the loop end for the last slice),
// increments the count and returns the new right-hand slice's index (the
// caller's "current slice" after the split). Returns -1 when the sample is
// not sliced, index is out of range or the count is already at the cap.
int sampleSliceSplit(InstrumentSample* sample, uint8_t index);

// Removes the slice starting at bounds[index]: shifts the following bounds
// down, decrements the count and returns the index the caller should make
// current (index - 1, or 0 when deleting the first slice). Returns -1 when
// the sample is not sliced or index is out of range. Deleting the last
// remaining slice turns the mode off (sentinel 0, bounds cleared).
int sampleSliceDelete(InstrumentSample* sample, uint8_t index);

// Inserts a new slice starting exactly at `frame` (kept sorted; the caller
// supplies the frame, e.g. a playback position). Returns the new slice's
// index, or -1 when the sample is not sliced, the frame is outside the loop
// region, it duplicates an existing bound or the count is at the cap.
// Phase 3's LAZY playback-drop reuses this.
int sampleSliceInsertAtFrame(InstrumentSample* sample, uint32_t frame);

// Playback-drop variant of sampleSliceInsertAtFrame: rejects `frame` when it
// sits within minGapFrames of an existing bound (or of the loop start), so
// rapid EDIT taps don't pile up micro-slices. Returns the new slice's index
// (and sets the caller's current slice), or -1 when rejected (too close,
// outside the loop region, duplicate, count at the cap or not sliced).
int sampleSliceInsertAtFrameGapped(InstrumentSample* sample, uint32_t frame,
                                   uint32_t minGapFrames);

// Moves the current slice's start frame by delta frames, clamped so the
// bound stays inside its own slice (between the previous bound and the
// next bound / loop end). Returns the new frame, or -1 when the sample is
// not sliced or index is out of range.
int32_t sampleSliceNudge(InstrumentSample* sample, uint8_t index, int32_t delta);

// Start frame of slice `index` (bounds-aware; falls back to the even
// division of the loop region when the bounds array is empty). Returns -1
// when the sample is not sliced or index is out of range.
int32_t sampleSliceStartFrame(const InstrumentSample* sample, uint8_t index,
                              uint8_t start, uint8_t end);
int sampleLoadWav16(const char* path, InstrumentSample* sample,
                    char* error, size_t errorSize);
int sampleLoadWav16File(FILE* file, const char* path, InstrumentSample* sample,
                        char* error, size_t errorSize);
// Cue-aware loader: behaves exactly like sampleLoadWav16File and, when
// outCueFrames/outCueCount are non-NULL, reports the WAV's `cue ` chunk
// sample offsets (up to 64; extra cues are ignored). outCueCount is always
// written (0 when the file has no cue chunk). The sample itself is not
// modified - applying cues to sliceBounds is the caller's decision.
int sampleLoadWav16FileCues(FILE* file, const char* path, InstrumentSample* sample,
                            uint32_t* outCueFrames, uint8_t* outCueCount,
                            char* error, size_t errorSize);
int sampleLoadWav16Cues(const char* path, InstrumentSample* sample,
                        uint32_t* outCueFrames, uint8_t* outCueCount,
                        char* error, size_t errorSize);

// Writes the sample as an uncompressed 16-bit PCM WAV (44-byte RIFF header,
// little-endian fields written byte-wise so the code is endian-agnostic).
// Returns 0 on success, 1 on failure with a message in error. The sample is
// not modified; markers and path are the caller's business.
int sampleSaveWav16(const InstrumentSample* sample, const char* path,
                    char* error, size_t errorSize);

// Cue-chunk variant: when cueCount > 0, a standard `cue ` chunk is written
// between fmt and data with one cue point per slice start frame (visible as
// markers in DAWs like Audacity). cueCount 0 / cueFrames NULL produces
// byte-identical output to sampleSaveWav16.
int sampleSaveWav16WithCues(const InstrumentSample* sample, const char* path,
                            const uint32_t* cueFrames, uint8_t cueCount,
                            char* error, size_t errorSize);

#endif
