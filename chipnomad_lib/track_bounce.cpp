#include "track_bounce.h"

#include <atomic>
#include <string.h>

// SPSC ring for interleaved stereo int16 samples. The engine (audio
// callback) pushes; the recording writer thread pops. Capacity is a power
// of two so index masking is a single AND.
//
// The engine renders per-sample floats; the ring stores int16 stereo pairs.
// audioRecorderCaptureTrackSample stashes odd (right) samples until their
// left partner arrives, then pushes the pair atomically.

namespace {

constexpr int kTrackBounceRingCapacity = 131072; // stereo frames (~2.7 s at 48 kHz)
constexpr int kTrackBouncePreviewCapacity = 1024;

std::atomic<int> g_activeTrack{-1};
std::atomic<int> g_captureEnabled{0};

// Ring state (stereo frames)
int16_t g_ring[kTrackBounceRingCapacity * 2];
std::atomic<int> g_head{0}; // writer position
std::atomic<int> g_tail{0}; // reader position
std::atomic<uint64_t> g_dropped{0};

// Stereo pairing accumulator: even sampleIndex is left, odd is right.
int16_t g_pendingLeft = 0;
bool g_pendingValid = false;

// Chunk watermark (engine thread only): the next sampleIndex the capture
// hook expects within the current render chunk. The capture sites only run
// for tracks with an active voice, so silent stretches never reach them -
// without gap filling the WAV would be missing every rest and play back
// fast. begin/end chunk bracket each render chunk; capture advances the
// watermark, end fills whatever is left with silence.
int g_chunkWatermark = 0;

// Rolling preview buffer (mono, decimated), written by the engine thread,
// read by the UI thread. Single-writer/single-reader with atomic cursor.
float g_preview[kTrackBouncePreviewCapacity];
std::atomic<int> g_previewCount{0};
int g_previewCursor = 0;
int g_previewDecimation = 0;

inline int16_t floatToS16(float sample) {
  if (sample > 1.0f) sample = 1.0f;
  if (sample < -1.0f) sample = -1.0f;
  return (int16_t)(sample * 32767.0f);
}

// Shared push path for real samples and silence gaps: feeds the decimated
// preview (keeping it time-aligned) and the SPSC ring (drop-on-full).
void pushStereoFrame(int16_t left, int16_t right) {
  if ((++g_previewDecimation & 15) == 0) {
    const float mono = (left + right) * 0.5f / 32768.0f;
    g_preview[g_previewCursor] = mono;
    g_previewCursor = (g_previewCursor + 1) % kTrackBouncePreviewCapacity;
    int count = g_previewCount.load(std::memory_order_relaxed);
    if (count < kTrackBouncePreviewCapacity)
      g_previewCount.store(count + 1, std::memory_order_relaxed);
  }
  const int head = g_head.load(std::memory_order_relaxed);
  const int tail = g_tail.load(std::memory_order_acquire);
  const int next = (head + 1) & (kTrackBounceRingCapacity - 1);
  if (next == tail) {
    g_dropped.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  g_ring[head * 2] = left;
  g_ring[head * 2 + 1] = right;
  g_head.store(next, std::memory_order_release);
}

void pushZeroFrames(int stereoFrameCount) {
  for (int i = 0; i < stereoFrameCount; ++i) pushStereoFrame(0, 0);
}

} // namespace

void audioRecorderBeginTrackChunk(void) {
  if (!g_captureEnabled.load(std::memory_order_acquire)) return;
  g_chunkWatermark = 0;
}

void audioRecorderEndTrackChunk(int frames) {
  if (!g_captureEnabled.load(std::memory_order_acquire)) return;
  const int total = frames * 2;
  if (g_chunkWatermark < total)
    pushZeroFrames((total - g_chunkWatermark) / 2);
  g_chunkWatermark = 0;
}

uint64_t trackBounceDropped(void) {
  return g_dropped.load(std::memory_order_relaxed);
}

void trackBounceStart(int trackIdx) {
  if (trackIdx < 0) {
    trackBounceStop();
    return;
  }
  g_tail.store(0, std::memory_order_relaxed);
  g_head.store(0, std::memory_order_relaxed);
  g_dropped.store(0, std::memory_order_relaxed);
  g_pendingValid = false;
  g_chunkWatermark = 0;
  g_previewCount.store(0, std::memory_order_relaxed);
  g_previewCursor = 0;
  g_previewDecimation = 0;
  g_activeTrack.store(trackIdx, std::memory_order_release);
  g_captureEnabled.store(1, std::memory_order_release);
}

void trackBounceStop(void) {
  g_captureEnabled.store(0, std::memory_order_release);
  g_activeTrack.store(-1, std::memory_order_release);
}

int trackBounceActiveTrack(void) {
  return g_activeTrack.load(std::memory_order_acquire);
}

bool audioRecorderWantsTrackSample(int trackIdx) {
  return g_captureEnabled.load(std::memory_order_acquire) != 0 &&
         g_activeTrack.load(std::memory_order_acquire) == trackIdx;
}

void audioRecorderCaptureTrackSample(int trackIdx, int sampleIndex, float sample) {
  if (!audioRecorderWantsTrackSample(trackIdx)) return;
  if (sampleIndex < g_chunkWatermark) return; // already captured (double site)

  // Fill the gap since the last captured sample with silence: the capture
  // sites only run while a voice is active, so rests between notes must be
  // pushed as zeros or the recording would be time-compressed.
  if (sampleIndex > g_chunkWatermark)
    pushZeroFrames((sampleIndex - g_chunkWatermark) / 2);

  // Stereo pairing: even sampleIndex = left, odd = right.
  const int16_t s16 = floatToS16(sample);
  if ((sampleIndex & 1) == 0) {
    g_pendingLeft = s16;
    g_pendingValid = true;
    g_chunkWatermark = sampleIndex + 1;
    return;
  }
  if (!g_pendingValid) return; // dropped left; skip orphan right
  g_pendingValid = false;
  pushStereoFrame(g_pendingLeft, s16);
  g_chunkWatermark = sampleIndex + 1;
}

int trackBouncePop(int16_t* stereo, int maxStereoSamples) {
  if (!stereo || maxStereoSamples <= 0) return 0;
  int popped = 0;
  int tail = g_tail.load(std::memory_order_relaxed);
  while (popped < maxStereoSamples) {
    const int head = g_head.load(std::memory_order_acquire);
    if (tail == head) break;
    stereo[popped * 2] = g_ring[tail * 2];
    stereo[popped * 2 + 1] = g_ring[tail * 2 + 1];
    tail = (tail + 1) & (kTrackBounceRingCapacity - 1);
    ++popped;
  }
  g_tail.store(tail, std::memory_order_release);
  return popped;
}

int trackBouncePreview(float* out, int maxCount) {
  if (!out || maxCount <= 0) return 0;
  const int count = g_previewCount.load(std::memory_order_acquire);
  const int n = count < maxCount ? count : maxCount;
  // Oldest-first: start at (cursor - count) mod capacity.
  int start = g_previewCursor - count;
  while (start < 0) start += kTrackBouncePreviewCapacity;
  for (int i = 0; i < n; ++i)
    out[i] = g_preview[(start + i) % kTrackBouncePreviewCapacity];
  return n;
}
