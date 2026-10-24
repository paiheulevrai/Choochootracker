// Audio input manager: arms capture devices, feeds level meters, and hands
// captured audio to the recorder (Phase 2 of the audio input recording
// feature).
//
// Threading contract (mirrors midi_io.cpp's SPSC queue and audio_monitor's
// 3-slot snapshot):
// - The SDL capture callback (audio thread) is the single producer: it never
//   allocates, locks, or waits. Full rings drop samples and bump a counter.
// - The UI thread is the single consumer for levels; the recorder's writer
//   thread is the single consumer for ring data.
// - All cross-thread state is lock-free atomics with release/acquire order.
#pragma once

#include "common.h"
#include <atomic>

constexpr int AUDIO_INPUT_LEVEL_SAMPLES = 256;

// Per-armed-slot SPSC ring of interleaved stereo int16 samples.
// Producer: SDL capture callback. Consumer: recorder writer thread.
// Capacity 8192 stereo samples ~= 170 ms at 48 kHz: enough headroom for
// writer-thread hiccups without unbounded memory.
struct AudioInputRing {
  static constexpr unsigned int CAPACITY = 8192;
  int16_t samples[CAPACITY * 2]; // interleaved stereo
  std::atomic<unsigned int> head{0}; // producer (capture callback)
  std::atomic<unsigned int> tail{0}; // consumer (writer thread)
  std::atomic<unsigned int> dropped{0};

  // Realtime-safe push from the capture callback. Returns false when full
  // (sample block dropped, counter bumped).
  bool push(const int16_t* stereo, int stereoSampleCount);
  // Drains up to stereoSampleCount samples; returns how many were read.
  int pop(int16_t* stereo, int stereoSampleCount);
  void clear();
};

// Per-armed-slot level data (mirrors AudioMonitor's decimation).
struct AudioInputLevels {
  float peaks[AUDIO_INPUT_MAX_ARMED][2]{};
  float samples[AUDIO_INPUT_MAX_ARMED][AUDIO_INPUT_LEVEL_SAMPLES]{};
};

// 3-slot snapshot machinery: producer = capture callback, consumer = UI
// thread. Slot states: 0 = free, 1 = published, 2 = reader-owned.
struct AudioInputLevelsSnapshot {
  AudioInputLevels data;
  std::atomic<int> state{0};
};

class AudioInputManager {
 public:
  AudioInputManager();

  // UI thread. Re-reads the platform device list (no SDL subsystem restart).
  void enumerate();
  int deviceCount() const { return deviceCount_; }
  // Returns 0 on success; non-zero when index is out of range.
  int deviceName(int index, char* buffer, int bufferSize) const;

  // UI thread. Arms a device (opens capture if not yet open). Returns the
  // armed slot index, or -1 on failure (no device, all slots full, capture
  // open failed).
  int arm(int deviceIndex);
  // UI thread. Disarms a slot; closes capture when no slots remain.
  void disarm(int armedSlot);
  // UI thread. Disarms all slots (used before device re-enumeration).
  void disarmAll(void);
  // Reopen an armed capture device at the output rate. Keeps the old rate on failure.
  int reconfigureRate(int sampleRate);
  int armedCount() const;
  // Returns the device index for an armed slot, or -1.
  int armedDeviceIndex(int slot) const;

  // UI thread. Copies the latest levels snapshot; returns false when none
  // has been published since the last receive.
  bool receiveLevels(AudioInputLevels& result);

  // UI thread. Monitor routing toggle (Phase 2.4).
  void setMonitor(int enabled) { monitor_.store(enabled ? 1 : 0, std::memory_order_release); }
  int monitor() const { return monitor_.load(std::memory_order_acquire); }

  // Recorder writer thread (Phase 3.3) drains this ring for a slot.
  AudioInputRing* ring(int armedSlot);

  // Audio thread (capture callback). Pushes a captured block into rings and
  // level snapshot accumulators. Never allocates or locks.
  void onCapture(const int16_t* buffer, int stereoSamples, int channels);

  // Audio thread (output callback). Mixes monitored capture into the output
  // buffer after chipnomadRender(). No-op when monitor is off.
  void mixMonitor(int16_t* buffer, int stereoSamples);

  // Test/diagnostic hooks.
  unsigned int droppedCount() const;
  bool captureOpen() const { return captureOpen_; }

  // SDL callback trampoline (registered with audioInputSetup).
  static void onCaptureTrampoline(const int16_t* buffer, int stereoSamples, int channels);

 private:
  // Test hook: drive the capture callback path without a real device.
  friend struct AudioInputManagerTestPeer;

  void openCaptureIfNeeded();
  void closeCaptureIfIdle();
  void publishLevels();

  char deviceNames_[AUDIO_INPUT_MAX_DEVICES][AUDIO_DEVICE_NAME_LENGTH + 1];
  int deviceCount_ = 0;
  int armed_[AUDIO_INPUT_MAX_ARMED]; // device index per slot, -1 = empty
  bool captureOpen_ = false;
  int captureRate_ = 48000;
  std::atomic<int> monitor_{0};

  AudioInputRing rings_[AUDIO_INPUT_MAX_ARMED];
  // Dedicated monitor ring: fed by onCapture when monitoring is on, drained
  // by mixMonitor on the output audio thread. Separate from the recorder
  // rings so monitoring never steals recorded data.
  AudioInputRing monitorRing_;
  int16_t monitorChunk_[1024 * 2];
  int monitorGain_ = 16384; // -6 dB (Q15)
  // Level accumulation (capture callback side).
  float peaks_[AUDIO_INPUT_MAX_ARMED][2]{};
  float samples_[AUDIO_INPUT_MAX_ARMED][AUDIO_INPUT_LEVEL_SAMPLES]{};
  int sampleCursor_ = 0;
  int decimation_ = 0;
  // 3-slot snapshot (mirrors AudioMonitor).
  AudioInputLevelsSnapshot slots_[3];
  std::atomic<int> published_{-1};
};

// Test-only peer exposing private capture-callback state to unit tests.
struct AudioInputManagerTestPeer {
  static void armSlotForTest(AudioInputManager& m, int slot, int deviceIndex) {
    m.armed_[slot] = deviceIndex;
  }
  static void disarmSlotForTest(AudioInputManager& m, int slot) {
    m.armed_[slot] = -1;
  }
  static void captureOpenForTest(AudioInputManager& m, bool open) {
    m.captureOpen_ = open;
  }
  static float peakForTest(AudioInputManager& m, int slot, int channel) {
    return m.peaks_[slot][channel];
  }
};

// Re-arms devices from appSettings.audioInputArmed after enumeration
// (implemented in screen_audio_input.cpp; needs both appSettings and the
// manager). Skips entries whose device no longer exists.
void syncArmedDevicesFromSettings(void);

// Recorder writer-thread lifecycle (Phase 3.3). UI thread only.
class RecordingWavWriter;
void audioInputManagerStartRecording(RecordingWavWriter* writer, int armedSlot);
unsigned int audioInputManagerRecordingDroppedCount(void);
// Track bounce variant (Phase 7): records the rendered output of a track.
void audioInputManagerStartTrackRecording(RecordingWavWriter* writer, int trackIdx);
void audioInputManagerStopRecording(void);
bool audioInputManagerRecordingActive(void);

extern AudioInputManager audioInputManager;
