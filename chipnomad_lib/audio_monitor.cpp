#include "audio_monitor.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

AudioMonitor::AudioMonitor() { reserve(4096); }
AudioMonitor::~AudioMonitor() { free(scratch_); }

void AudioMonitor::setEnabled(bool enabled) {
  const bool wasEnabled = enabled_.exchange(enabled, std::memory_order_acq_rel);
  if (enabled && !wasEnabled) resetRequested_.store(true, std::memory_order_release);
}

bool AudioMonitor::reserve(int frames) {
  if (frames <= 0) return false;
  if (frames <= capacity_) return scratch_ != nullptr;
  if ((size_t)frames > std::numeric_limits<size_t>::max() / (2 * PROJECT_MAX_TRACKS * sizeof(float))) return false;
  float* memory = (float*)calloc((size_t)frames * 2 * PROJECT_MAX_TRACKS, sizeof(float));
  if (!memory) return false;
  free(scratch_);
  scratch_ = memory;
  capacity_ = frames;
  return true;
}

void AudioMonitor::beginRender() {
  if (!enabled()) return;
  if (resetRequested_.exchange(false, std::memory_order_acq_rel)) {
    chunkFrames_ = cursor_ = decimation_ = 0;
    memset(mix_, 0, sizeof(mix_));
    memset(mixStereo_, 0, sizeof(mixStereo_));
    memset(tracks_, 0, sizeof(tracks_));
    int published = published_.exchange(-1, std::memory_order_acq_rel);
    if (published >= 0) {
      int expected = 1;
      slots_[published].state.compare_exchange_strong(expected, 0, std::memory_order_acq_rel);
    }
  }
  memset(peaks_, 0, sizeof(peaks_));
}
void AudioMonitor::beginChunk(int frames) {
  if (!enabled()) return;
  chunkFrames_ = scratch_ && frames > 0 && frames <= capacity_ ? frames : 0;
  if (chunkFrames_) memset(scratch_, 0, (size_t)frames * 2 * PROJECT_MAX_TRACKS * sizeof(float));
}
void AudioMonitor::add(int track, int sampleIndex, float value) {
  if (!enabled()) return;
  if (chunkFrames_ && track >= 0 && track < PROJECT_MAX_TRACKS && sampleIndex >= 0 && sampleIndex < chunkFrames_ * 2)
    scratch_[track * chunkFrames_ * 2 + sampleIndex] += value;
}

void AudioMonitor::finishChunk(const float* stereo, int frames, int sampleRate) {
  if (!enabled()) return;
  // About 21 ms of history at any sample rate, across tick/FLFO chunk edges.
  const int stride = std::max(1, sampleRate / 12000);
  for (int frame = 0; frame < frames; ++frame) {
    const bool capture = decimation_ == 0;
    for (int track = 0; track < PROJECT_MAX_TRACKS; ++track) {
      const float* p = frame < chunkFrames_ ? scratch_ + track * chunkFrames_ * 2 + frame * 2 : nullptr;
      float left = p ? p[0] : 0, right = p ? p[1] : 0;
      peaks_[track][0] = std::max(peaks_[track][0], fabsf(left));
      peaks_[track][1] = std::max(peaks_[track][1], fabsf(right));
      if (capture) tracks_[track][cursor_] = (left + right) * 0.5f;
    }
    if (capture) {
      mixStereo_[cursor_][0] = stereo[frame * 2];
      mixStereo_[cursor_][1] = stereo[frame * 2 + 1];
      mix_[cursor_] = (mixStereo_[cursor_][0] + mixStereo_[cursor_][1]) * 0.5f;
      cursor_ = (cursor_ + 1) % AUDIO_MONITOR_SAMPLES;
    }
    decimation_ = (decimation_ + 1) % stride;
  }
}

void AudioMonitor::publish() {
  if (!enabled()) return;
  int previous = published_.exchange(-1, std::memory_order_acq_rel);
  if (previous >= 0) {
    int expected = 1;
    slots_[previous].state.compare_exchange_strong(expected, 0, std::memory_order_acq_rel);
  }
  for (int slot = 0; slot < 3; ++slot) {
    int expected = 0;
    if (!slots_[slot].state.compare_exchange_strong(expected, 2, std::memory_order_acq_rel)) continue;
    auto& data = slots_[slot].data;
    for (int i = 0; i < AUDIO_MONITOR_SAMPLES; ++i) {
      int source = (cursor_ + i) % AUDIO_MONITOR_SAMPLES;
      data.mix[i] = mix_[source];
      data.mixStereo[i][0] = mixStereo_[source][0];
      data.mixStereo[i][1] = mixStereo_[source][1];
      for (int t = 0; t < PROJECT_MAX_TRACKS; ++t) data.tracks[t][i] = tracks_[t][source];
    }
    memcpy(data.peaks, peaks_, sizeof(peaks_));
    slots_[slot].state.store(1, std::memory_order_release);
    published_.store(slot, std::memory_order_release);
    return;
  }
}

bool AudioMonitor::receive(AudioMonitorSnapshot& result) {
  if (!enabled()) return false;
  int slot = published_.load(std::memory_order_acquire);
  if (slot < 0) return false;
  int expected = 1;
  if (!slots_[slot].state.compare_exchange_strong(expected, 2, std::memory_order_acq_rel)) return false;
  int published = slot;
  published_.compare_exchange_strong(published, -1, std::memory_order_acq_rel);
  result = slots_[slot].data;
  slots_[slot].state.store(0, std::memory_order_release);
  return true;
}
