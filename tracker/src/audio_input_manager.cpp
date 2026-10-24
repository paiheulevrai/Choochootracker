#include "audio_input_manager.h"
#include <math.h>

#include "audio_recorder.h"
#include "track_bounce.h"
#include <chrono>
#include <thread>
#include "corelib_audio.h"
#include "audio_input_diagnostics.h"

#include <cstring>

AudioInputManager audioInputManager;
static std::atomic<int> g_captureRecordingSlot{-1};

AudioInputManager::AudioInputManager() {
  for (int i = 0; i < AUDIO_INPUT_MAX_ARMED; ++i) armed_[i] = -1;
}

bool AudioInputRing::push(const int16_t* stereo, int stereoSampleCount) {
  if (!stereo || stereoSampleCount <= 0) return false;
  unsigned int localHead = head.load(std::memory_order_relaxed);
  const unsigned int localTail = tail.load(std::memory_order_acquire);
  const unsigned int free = (localTail + CAPACITY - localHead - 1) % CAPACITY;
  if ((unsigned int)stereoSampleCount > free) {
    // Publish whole capture blocks or none: a partially written block is
    // inaudible, so count all of its frames as lost.
    dropped.fetch_add(stereoSampleCount, std::memory_order_relaxed);
    audioInputDiagnostics::captureDropped.fetch_add(stereoSampleCount,
                                                    std::memory_order_relaxed);
    return false;
  }
  unsigned int next = localHead;
  for (int i = 0; i < stereoSampleCount; ++i) {
    samples[next * 2] = stereo[i * 2];
    samples[next * 2 + 1] = stereo[i * 2 + 1];
    next = (next + 1) % CAPACITY;
  }
  head.store(next, std::memory_order_release);
  return true;
}

int AudioInputRing::pop(int16_t* stereo, int stereoSampleCount) {
  unsigned int localTail = tail.load(std::memory_order_relaxed);
  int read = 0;
  while (read < stereoSampleCount) {
    // Empty when the read cursor has caught up with the write cursor.
    if (localTail == head.load(std::memory_order_acquire)) break;
    stereo[read * 2] = samples[localTail * 2];
    stereo[read * 2 + 1] = samples[localTail * 2 + 1];
    localTail = (localTail + 1) % CAPACITY;
    ++read;
  }
  tail.store(localTail, std::memory_order_release);
  return read;
}

void AudioInputRing::clear() {
  tail.store(head.load(std::memory_order_acquire), std::memory_order_release);
  dropped.store(0, std::memory_order_relaxed);
}

void AudioInputManager::enumerate() {
  int count = audioInputDeviceCount();
  if (count > AUDIO_INPUT_MAX_DEVICES) count = AUDIO_INPUT_MAX_DEVICES;
  for (int i = 0; i < count; ++i) {
    if (audioInputDeviceName(i, deviceNames_[i], sizeof(deviceNames_[i]))) {
      deviceNames_[i][0] = 0;
    }
  }
  deviceCount_ = count;
}

int AudioInputManager::deviceName(int index, char* buffer, int bufferSize) const {
  if (index < 0 || index >= deviceCount_) return 1;
  strncpy(buffer, deviceNames_[index], bufferSize - 1);
  buffer[bufferSize - 1] = 0;
  return 0;
}

int AudioInputManager::armedCount() const {
  int count = 0;
  for (int i = 0; i < AUDIO_INPUT_MAX_ARMED; ++i)
    if (armed_[i] >= 0) ++count;
  return count;
}

int AudioInputManager::armedDeviceIndex(int slot) const {
  if (slot < 0 || slot >= AUDIO_INPUT_MAX_ARMED) return -1;
  return armed_[slot];
}

int AudioInputManager::arm(int deviceIndex) {
  if (deviceIndex < 0 || deviceIndex >= deviceCount_) return -1;
  // Already armed?
  for (int i = 0; i < AUDIO_INPUT_MAX_ARMED; ++i)
    if (armed_[i] == deviceIndex) return i;
  int slot = -1;
  for (int i = 0; i < AUDIO_INPUT_MAX_ARMED; ++i)
    if (armed_[i] < 0) { slot = i; break; }
  if (slot < 0) return -1; // all 8 slots full
  armed_[slot] = deviceIndex;
  rings_[slot].clear();
  openCaptureIfNeeded();
  if (!captureOpen_) {
    armed_[slot] = -1;
    return -1;
  }
  return slot;
}

void AudioInputManager::disarm(int armedSlot) {
  if (armedSlot < 0 || armedSlot >= AUDIO_INPUT_MAX_ARMED) return;
  if (armed_[armedSlot] < 0) return;
  armed_[armedSlot] = -1;
  rings_[armedSlot].clear();
  closeCaptureIfIdle();
}

void AudioInputManager::disarmAll(void) {
  for (int slot = 0; slot < AUDIO_INPUT_MAX_ARMED; ++slot) {
    if (armed_[slot] < 0) continue;
    armed_[slot] = -1;
    rings_[slot].clear();
  }
  closeCaptureIfIdle();
}

int AudioInputManager::reconfigureRate(int sampleRate) {
  if (sampleRate <= 0) return 1;
  if (sampleRate == captureRate_) return 0;
  const int oldRate = captureRate_;
  const bool wasOpen = captureOpen_;
  if (wasOpen) {
    audioInputCleanup();
    captureOpen_ = false;
    monitorRing_.clear();
    for (auto& ring : rings_) ring.clear();
  }
  captureRate_ = sampleRate;
  if (wasOpen) {
    openCaptureIfNeeded();
    if (!captureOpen_) {
      captureRate_ = oldRate;
      openCaptureIfNeeded();
      return 1;
    }
  }
  return 0;
}

void AudioInputManager::openCaptureIfNeeded() {
  if (captureOpen_) return;
  // v1: single active capture device. The first armed slot's device is
  // opened; arming additional devices shares the same capture stream.
  int deviceIndex = -1;
  for (int i = 0; i < AUDIO_INPUT_MAX_ARMED; ++i)
    if (armed_[i] >= 0) { deviceIndex = armed_[i]; break; }
  if (deviceIndex < 0) return;
  if (audioInputSetup(onCaptureTrampoline, captureRate_, 512, 2, deviceIndex)) return;
  captureOpen_ = true;
}

void AudioInputManager::closeCaptureIfIdle() {
  if (!captureOpen_) return;
  for (int i = 0; i < AUDIO_INPUT_MAX_ARMED; ++i)
    if (armed_[i] >= 0) return;
  audioInputCleanup();
  captureOpen_ = false;
}

void AudioInputManager::onCaptureTrampoline(const int16_t* buffer, int stereoSamples, int channels) {
  audioInputManager.onCapture(buffer, stereoSamples, channels);
}

void AudioInputManager::onCapture(const int16_t* buffer, int stereoSamples, int channels) {
  if (!captureOpen_ || stereoSamples <= 0) return;
  const int ch = channels == 1 ? 1 : 2;

  // Level accumulation + decimated waveform history (mirrors AudioMonitor).
  // Peaks decay ~0.5 dB per callback (~93 callbacks/s at 48k/512) for a
  // ~46 dB/s fall at 48 kHz - a usable peak-meter release; -1 dB per callback made the
  // bar vanish in a fraction of a second.
  for (int slot = 0; slot < AUDIO_INPUT_MAX_ARMED; ++slot) {
    if (armed_[slot] < 0) continue;
    peaks_[slot][0] *= 0.944f; // -0.5 dB
    peaks_[slot][1] *= 0.944f;
  }
  for (int i = 0; i < stereoSamples; ++i) {
    // Peak meters track magnitude in the 0..1 float domain (AudioMonitor
    // convention); raw int16 values pinned the meter full-scale.
    float leftRaw = (float)buffer[i * ch];
    float rightRaw = (float)(ch == 2 ? buffer[i * ch + 1] : buffer[i * ch]);
    float left = fabsf(leftRaw) / 32768.0f;
    float right = fabsf(rightRaw) / 32768.0f;
    for (int slot = 0; slot < AUDIO_INPUT_MAX_ARMED; ++slot) {
      if (armed_[slot] < 0) continue;
      if (left > peaks_[slot][0]) peaks_[slot][0] = left;
      if (right > peaks_[slot][1]) peaks_[slot][1] = right;
    }
    if (decimation_ == 0) {
      // Signed and normalized: the waveform history feeds renderFloatPreview.
      const float mono = (leftRaw + rightRaw) * 0.5f / 32768.0f;
      for (int slot = 0; slot < AUDIO_INPUT_MAX_ARMED; ++slot) {
        if (armed_[slot] < 0) continue;
        samples_[slot][sampleCursor_] = mono;
      }
      sampleCursor_ = (sampleCursor_ + 1) % AUDIO_INPUT_LEVEL_SAMPLES;
    }
    decimation_ = (decimation_ + 1) % 4; // 8 or 12 kHz history at 32 or 48 kHz
  }

  // Only the active take needs recording data. Idle input still feeds meters
  // and the separate monitor ring, without filling recorder rings.
  const int recordingSlot = g_captureRecordingSlot.load(std::memory_order_acquire);
  if (recordingSlot >= 0) rings_[recordingSlot].push(buffer, stereoSamples);
  // Monitor ring: only when monitoring is on (saves the copy otherwise).
  if (monitor_.load(std::memory_order_acquire)) {
    monitorRing_.push(buffer, stereoSamples);
  }

  publishLevels();
}

void AudioInputManager::publishLevels() {
  // Recycle the previously published slot. The UI consumer flips it to
  // state 2 (owned) when it takes the data, so only CAS 1 -> 0 here; a slot
  // still owned by a slow reader is skipped this round (3-slot design).
  int previous = published_.exchange(-1, std::memory_order_acq_rel);
  if (previous >= 0) {
    int expected = 1;
    slots_[previous].state.compare_exchange_strong(expected, 0,
                                                   std::memory_order_acq_rel);
  }
  for (int slot = 0; slot < 3; ++slot) {
    int expected = 0;
    if (!slots_[slot].state.compare_exchange_strong(expected, 2,
                                                    std::memory_order_acq_rel))
      continue;
    auto& data = slots_[slot].data;
    for (int i = 0; i < AUDIO_INPUT_MAX_ARMED; ++i) {
      data.peaks[i][0] = peaks_[i][0];
      data.peaks[i][1] = peaks_[i][1];
      memcpy(data.samples[i], samples_[i], sizeof(samples_[i]));
    }
    slots_[slot].state.store(1, std::memory_order_release);
    published_.store(slot, std::memory_order_release);
    return;
  }
}
bool AudioInputManager::receiveLevels(AudioInputLevels& result) {
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

// ---------------------------------------------------------------------------
// Recorder writer thread (Phase 3.3). Modeled on midi_io.cpp's drain thread:
// a dedicated std::thread drains the armed slot's SPSC ring into a
// RecordingWavWriter. Started by audioInputManagerStartRecording(), joined by
// audioInputManagerStopRecording().
static std::thread* g_writerThread = nullptr;
static std::atomic<bool> g_writerRunning{false};

bool audioInputManagerRecordingActive(void) {
  return g_writerRunning.load(std::memory_order_acquire);
}
static RecordingWavWriter* g_activeWriter = nullptr;
static int g_activeArmedSlot = -1;
static int g_activeTrackSource = -1; // >= 0: bouncing this track (Phase 7)

static void writerThreadLoop() {
  int16_t chunk[1024 * 2]; // 1024 stereo samples per iteration
  while (g_writerRunning.load(std::memory_order_relaxed)) {
    if (g_activeTrackSource >= 0 && g_activeWriter) {
      // Track bounce: drain the track-bounce ring (Phase 7).
      int popped = trackBouncePop(chunk, 1024);
      if (popped > 0) {
        g_activeWriter->write(chunk, popped);
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    } else if (g_activeArmedSlot >= 0 && g_activeWriter) {
      AudioInputRing* ring = audioInputManager.ring(g_activeArmedSlot);
      if (ring) {
        int popped = ring->pop(chunk, 1024);
        if (popped > 0) {
          g_activeWriter->write(chunk, popped);
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

void audioInputManagerStartRecording(RecordingWavWriter* writer, int armedSlot) {
  audioInputManagerStopRecording();
  if (!writer || armedSlot < 0 || armedSlot >= AUDIO_INPUT_MAX_ARMED) return;
  // Capture is already running while the input is armed. Start each take at
  // the current callback, not with old frames left in its ring.
  audioInputManager.ring(armedSlot)->clear();
  g_activeWriter = writer;
  g_activeArmedSlot = armedSlot;
  g_captureRecordingSlot.store(armedSlot, std::memory_order_release);
  g_writerRunning.store(true, std::memory_order_relaxed);
  g_writerThread = new std::thread(writerThreadLoop);
}

void audioInputManagerStartTrackRecording(RecordingWavWriter* writer, int trackIdx) {
  audioInputManagerStopRecording();
  if (!writer || trackIdx < 0 || trackIdx >= PROJECT_MAX_TRACKS) return;
  g_activeWriter = writer;
  g_activeArmedSlot = -1;
  g_activeTrackSource = trackIdx;
  trackBounceStart(trackIdx);
  g_writerRunning.store(true, std::memory_order_relaxed);
  g_writerThread = new std::thread(writerThreadLoop);
}

void audioInputManagerStopRecording(void) {
  g_captureRecordingSlot.store(-1, std::memory_order_release);
  if (g_writerRunning.exchange(false, std::memory_order_relaxed)) {
    if (g_writerThread) {
      g_writerThread->join();
      delete g_writerThread;
      g_writerThread = nullptr;
    }
  }
  // Drain whatever is still buffered in the ring before closing: the ring
  // holds up to ~170 ms of audio and the writer thread only drains it while
  // running, so skipping this would truncate the take's tail.
  if (g_activeWriter) {
    int16_t chunk[1024 * 2];
    int popped;
    if (g_activeTrackSource >= 0) {
      while ((popped = trackBouncePop(chunk, 1024)) > 0)
        g_activeWriter->write(chunk, popped);
    } else if (g_activeArmedSlot >= 0) {
      AudioInputRing* ring = audioInputManager.ring(g_activeArmedSlot);
      if (ring) {
        while ((popped = ring->pop(chunk, 1024)) > 0)
          g_activeWriter->write(chunk, popped);
      }
    }
  }
  if (g_activeTrackSource >= 0) trackBounceStop();
  g_activeWriter = nullptr;
  g_activeArmedSlot = -1;
  g_activeTrackSource = -1;
}

unsigned int audioInputManagerRecordingDroppedCount(void) {
  if (g_activeArmedSlot < 0) return 0;
  AudioInputRing* ring = audioInputManager.ring(g_activeArmedSlot);
  return ring ? ring->dropped.load(std::memory_order_relaxed) : 0;
}

void AudioInputManager::mixMonitor(int16_t* buffer, int stereoSamples) {
  if (!monitor_.load(std::memory_order_acquire)) return;
  // Route the first armed slot's live capture into the output at -6 dB to
  // avoid feedback. Uses a dedicated monitor ring so monitoring never
  // competes with the recorder's writer thread for ring data.
  for (int slot = 0; slot < AUDIO_INPUT_MAX_ARMED; ++slot) {
    if (armed_[slot] < 0) continue;
    // monitorChunk_ holds 1024 stereo frames; clamp for larger buffer sizes.
    int want = stereoSamples < 1024 ? stereoSamples : 1024;
    int popped = monitorRing_.pop(monitorChunk_, want);
    for (int i = 0; i < popped * 2; ++i) {
      int mixed = buffer[i] + ((monitorChunk_[i] * monitorGain_) >> 15);
      if (mixed > 32767) mixed = 32767;
      if (mixed < -32768) mixed = -32768;
      buffer[i] = (int16_t)mixed;
    }
    break;
  }
}

unsigned int AudioInputManager::droppedCount() const {
  unsigned int total = 0;
  for (int i = 0; i < AUDIO_INPUT_MAX_ARMED; ++i)
    total += rings_[i].dropped.load(std::memory_order_relaxed);
  return total;
}

AudioInputRing* AudioInputManager::ring(int armedSlot) {
  if (armedSlot < 0 || armedSlot >= AUDIO_INPUT_MAX_ARMED) return nullptr;
  return &rings_[armedSlot];
}
