#include "sample_ops.h"

#include <string.h>

// Resolve the caller's selection to concrete bounds. selStart == selEnd
// means "whole sample"; inverted ranges are swapped; bounds are clamped.
// Returns 0 on success, nonzero when no usable range remains.
static int resolveRange(const InstrumentSample* s, uint32_t* selStart, uint32_t* selEnd) {
  if (!s->data || s->frameCount == 0) return sampleOpErrorNoSample;
  uint32_t start = *selStart;
  uint32_t end = *selEnd;
  if (start > end) { uint32_t swap = start; start = end; end = swap; }
  if (start > s->frameCount) start = s->frameCount;
  if (end > s->frameCount) end = s->frameCount;
  if (start == end) {
    // Whole-sample convention
    start = 0;
    end = s->frameCount;
  }
  if (end <= start) return sampleOpErrorRange;
  *selStart = start;
  *selEnd = end;
  return sampleOpOk;
}

// Clamp a frame to the range valid for the marker type and convert back to
// the 0-255 scale. Start markers cover [0, frameCount-1]; end markers are an
// exclusive bound and cover [0, frameCount].
static uint8_t clampMarkerToFrame(uint32_t frameCount, uint32_t frame, int isEndMarker) {
  if (isEndMarker) {
    if (frame > frameCount) frame = frameCount;
    return sampleFrameToEndMarker(frameCount, frame);
  }
  if (frameCount > 0 && frame >= frameCount) frame = frameCount - 1;
  return sampleFrameToStartMarker(frameCount, frame);
}

// Crop remap: everything at/after the cut point shifts left by it; frames
// before the cut clamp to the new start.
static uint8_t remapMarkerCrop(uint32_t oldFrameCount, uint8_t marker,
                               uint32_t cutFrom, uint32_t newFrameCount,
                               int isEndMarker) {
  uint32_t frame = isEndMarker ? sampleMarkerToEndFrame(oldFrameCount, marker)
                               : sampleMarkerToStartFrame(oldFrameCount, marker);
  frame = frame >= cutFrom ? frame - cutFrom : 0;
  return clampMarkerToFrame(newFrameCount, frame, isEndMarker);
}

// Delete remap: frames after the removed span shift by its length; frames
// inside it clamp to the join point.
static uint8_t remapMarkerDelete(uint32_t oldFrameCount, uint8_t marker,
                                 uint32_t cutFrom, uint32_t removed,
                                 uint32_t newFrameCount, int isEndMarker) {
  uint32_t frame = isEndMarker ? sampleMarkerToEndFrame(oldFrameCount, marker)
                               : sampleMarkerToStartFrame(oldFrameCount, marker);
  if (frame >= cutFrom + removed) {
    frame -= removed;
  } else if (frame > cutFrom) {
    frame = cutFrom;
  }
  return clampMarkerToFrame(newFrameCount, frame, isEndMarker);
}

int sampleOpCrop(InstrumentSample* s, uint32_t selStart, uint32_t selEnd) {
  uint32_t start = selStart;
  uint32_t end = selEnd;
  int res = resolveRange(s, &start, &end);
  if (res) return res;
  const uint32_t length = end - start;
  if (length == s->frameCount) return sampleOpOk; // full-sample crop: no-op

  int16_t* data = (int16_t*)malloc((size_t)length * s->channels * sizeof(int16_t));
  if (!data) return sampleOpErrorMemory;
  memcpy(data, s->data + (size_t)start * s->channels,
         (size_t)length * s->channels * sizeof(int16_t));

  const uint32_t oldFrameCount = s->frameCount;
  const uint8_t oldStart = s->start;
  const uint8_t oldEnd = s->end;
  free(s->data);
  s->data = data;
  s->frameCount = length;
  s->start = remapMarkerCrop(oldFrameCount, oldStart, start, length, 0);
  s->end = remapMarkerCrop(oldFrameCount, oldEnd, start, length, 1);
  return sampleOpOk;
}

int sampleOpNormalize(InstrumentSample* s, uint32_t selStart, uint32_t selEnd) {
  uint32_t start = selStart;
  uint32_t end = selEnd;
  int res = resolveRange(s, &start, &end);
  if (res) return res;

  int16_t peak = 0;
  for (uint32_t i = start; i < end; ++i) {
    const int16_t* frame = s->data + (size_t)i * s->channels;
    for (uint8_t c = 0; c < s->channels; ++c) {
      int16_t v = frame[c];
      if (v < 0) v = (int16_t)-v;
      if (v > peak) peak = v;
    }
  }
  if (peak == 0) return sampleOpOk; // silent selection: no-op
  if (peak == 32767) return sampleOpOk; // already full scale

  const int64_t gain = 32767 / peak;
  for (uint32_t i = start; i < end; ++i) {
    int16_t* frame = s->data + (size_t)i * s->channels;
    for (uint8_t c = 0; c < s->channels; ++c) {
      int64_t scaled = (int64_t)frame[c] * gain;
      if (scaled > 32767) scaled = 32767;
      else if (scaled < -32768) scaled = -32768;
      frame[c] = (int16_t)scaled;
    }
  }
  return sampleOpOk;
}

int sampleOpDelete(InstrumentSample* s, uint32_t selStart, uint32_t selEnd) {
  uint32_t start = selStart;
  uint32_t end = selEnd;
  int res = resolveRange(s, &start, &end);
  if (res) return res;
  const uint32_t removed = end - start;
  if (removed >= s->frameCount) return sampleOpErrorWholeSample;

  const uint32_t newFrameCount = s->frameCount - removed;
  int16_t* data = (int16_t*)malloc((size_t)newFrameCount * s->channels * sizeof(int16_t));
  if (!data) return sampleOpErrorMemory;
  memcpy(data, s->data, (size_t)start * s->channels * sizeof(int16_t));
  memcpy(data + (size_t)start * s->channels,
         s->data + (size_t)end * s->channels,
         (size_t)(s->frameCount - end) * s->channels * sizeof(int16_t));

  const uint32_t oldFrameCount = s->frameCount;
  const uint8_t oldStart = s->start;
  const uint8_t oldEnd = s->end;
  free(s->data);
  s->data = data;
  s->frameCount = newFrameCount;
  s->start = remapMarkerDelete(oldFrameCount, oldStart, start, removed, newFrameCount, 0);
  s->end = remapMarkerDelete(oldFrameCount, oldEnd, start, removed, newFrameCount, 1);
  return sampleOpOk;
}

int sampleOpSilence(InstrumentSample* s, uint32_t selStart, uint32_t selEnd) {
  uint32_t start = selStart;
  uint32_t end = selEnd;
  int res = resolveRange(s, &start, &end);
  if (res) return res;
  memset(s->data + (size_t)start * s->channels, 0,
         (size_t)(end - start) * s->channels * sizeof(int16_t));
  return sampleOpOk;
}

int sampleOpFade(InstrumentSample* s, uint32_t selStart, uint32_t selEnd, int fadeIn) {
  uint32_t start = selStart;
  uint32_t end = selEnd;
  int res = resolveRange(s, &start, &end);
  if (res) return res;
  const uint32_t length = end - start;

  for (uint32_t i = 0; i < length; ++i) {
    // Linear ramp in int32: gain = i/len (in) or (len-1-i)/len (out)
    uint32_t numerator = fadeIn ? i : (length - 1 - i);
    int32_t gainNum = (int32_t)numerator;
    int16_t* frame = s->data + (size_t)(start + i) * s->channels;
    for (uint8_t c = 0; c < s->channels; ++c) {
      int32_t scaled = (int32_t)frame[c] * gainNum / (int32_t)length;
      frame[c] = (int16_t)scaled;
    }
  }
  return sampleOpOk;
}

int sampleOpReverse(InstrumentSample* s, uint32_t selStart, uint32_t selEnd) {
  uint32_t start = selStart;
  uint32_t end = selEnd;
  int res = resolveRange(s, &start, &end);
  if (res) return res;
  const uint32_t length = end - start;

  // Swap mirrored frames; a frame's channels move as one unit so the
  // stereo image is preserved. The middle frame of an odd-length selection
  // stays in place.
  for (uint32_t i = 0; i < length / 2; ++i) {
    int16_t* a = s->data + (size_t)(start + i) * s->channels;
    int16_t* b = s->data + (size_t)(start + (length - 1 - i)) * s->channels;
    for (uint8_t c = 0; c < s->channels; ++c) {
      int16_t tmp = a[c];
      a[c] = b[c];
      b[c] = tmp;
    }
  }
  return sampleOpOk;
}

int sampleOpPrepareUndo(InstrumentSample* s, SampleUndo* slot) {
  if (!s->data || s->frameCount == 0) return sampleOpErrorNoSample;
  int16_t* data = (int16_t*)malloc((size_t)s->frameCount * s->channels * sizeof(int16_t));
  if (!data) return sampleOpErrorMemory;
  memcpy(data, s->data, (size_t)s->frameCount * s->channels * sizeof(int16_t));
  free(slot->data); // overwrite any previous undo state (depth-1)
  slot->data = data;
  slot->header = *s;
  slot->header.data = NULL;
  slot->active = 1;
  return sampleOpOk;
}

int sampleOpApplyUndo(InstrumentSample* s, SampleUndo* slot) {
  if (!slot->active || !slot->data) return sampleOpErrorNoUndo;
  int16_t* current = s->data;
  InstrumentSample currentHeader = *s;
  s->data = slot->data;
  s->frameCount = slot->header.frameCount;
  s->sampleRate = slot->header.sampleRate;
  s->channels = slot->header.channels;
  s->start = slot->header.start;
  s->end = slot->header.end;
  // Slice state travels with the undo too: AUTO detection and the slice
  // editors mutate the sentinel, sensitivity and bounds, so UNDO must
  // restore them alongside the audio data (Phase 2).
  s->slice = slot->header.slice;
  s->autoSensitivity = slot->header.autoSensitivity;
  memcpy(s->sliceBounds, slot->header.sliceBounds, sizeof(s->sliceBounds));
  slot->data = current;
  slot->header = currentHeader;
  slot->header.data = NULL;
  // Slot stays active: the next apply toggles back.
  return sampleOpOk;
}

void sampleOpFreeUndo(SampleUndo* slot) {
  free(slot->data);
  slot->data = NULL;
  slot->active = 0;
}
