#pragma once
#include "project_constants.h"
#include <atomic>

constexpr int AUDIO_MONITOR_SAMPLES = 256;

// Display telemetry only: mono downmix of the final output and post-level,
// post-tilt track contributions. Peaks retain both channels independently.
struct AudioMonitorSnapshot {
  float mix[AUDIO_MONITOR_SAMPLES]{};
  float mixStereo[AUDIO_MONITOR_SAMPLES][2]{};
  float tracks[PROJECT_MAX_TRACKS][AUDIO_MONITOR_SAMPLES]{};
  float peaks[PROJECT_MAX_TRACKS][2]{};
};

// One audio producer, one UI consumer. No allocation, locks, or waiting in
// render/receive. The three slots keep a reader-owned snapshot immutable.
class AudioMonitor {
 public:
  AudioMonitor();
  ~AudioMonitor();
  bool reserve(int frames); // Call while audio is stopped.
  void setEnabled(bool enabled);
  bool enabled() const { return enabled_.load(std::memory_order_acquire); }
  void beginRender();
  void beginChunk(int frames);
  void add(int track, int sampleIndex, float value);
  void finishChunk(const float* stereo, int frames, int sampleRate);
  void publish();
  bool receive(AudioMonitorSnapshot& result);

 private:
  float* scratch_ = nullptr;
  int capacity_ = 0;
  int chunkFrames_ = 0;
  int cursor_ = 0;
  int decimation_ = 0;
  float mix_[AUDIO_MONITOR_SAMPLES]{};
  float mixStereo_[AUDIO_MONITOR_SAMPLES][2]{};
  float tracks_[PROJECT_MAX_TRACKS][AUDIO_MONITOR_SAMPLES]{};
  float peaks_[PROJECT_MAX_TRACKS][2]{};
  struct Slot {
    AudioMonitorSnapshot data;
    std::atomic<int> state{0}; // free, published, owned
  } slots_[3];
  std::atomic<int> published_{-1};
  std::atomic<bool> enabled_{true};
  std::atomic<bool> resetRequested_{false};
};
