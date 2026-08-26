#include <stdio.h>
#include <atomic>
#include <chrono>
#include <limits.h>
#include <string.h>
#include "audio_manager.h"
#include "corelib_audio.h"

#include "chipnomad_lib.h"
#include "corelib_file.h"
#include "synth/sample_voice.h"

static int aSampleRate;
static int aBufferSize;
static std::atomic<int> cpuLoadPercent{0};
static SampleVoice samplePreviewVoice;
static InstrumentSample samplePreview;
static float* floatBuffer;
static float* samplePreviewBuffer;

static void updatePlaybackMuteFlags(void) {
  uint8_t trackEnabled[PROJECT_MAX_TRACKS];
  // Check if any tracks are solo
  int hasSolo = 0;
  for (int i = 0; i < PROJECT_MAX_TRACKS; i++) {
    if (audioManager.trackStates[i] == TRACK_SOLO) {
      hasSolo = 1;
      break;
    }
  }

  for (int i = 0; i < PROJECT_MAX_TRACKS; i++) {
    if (hasSolo) {
      // Solo mode: only solo tracks are enabled
      trackEnabled[i] = (audioManager.trackStates[i] == TRACK_SOLO) ? 1 : 0;
    } else {
      // Mute mode: muted tracks are disabled, others enabled
      trackEnabled[i] = (audioManager.trackStates[i] == TRACK_MUTED) ? 0 : 1;
    }
  }
  chipnomadQueueTrackEnabled(chipnomadState, trackEnabled);
}

static void audioCallback(int16_t* buffer, int stereoSamples) {
  const auto startedAt = std::chrono::steady_clock::now();

  if (stereoSamples <= 0 || stereoSamples > aBufferSize ||
      !floatBuffer || !samplePreviewBuffer) {
    if (stereoSamples > aBufferSize) chipnomadSetRenderBufferOverflow(chipnomadState);
    memset(buffer, 0, stereoSamples > 0 ? stereoSamples * 2 * sizeof(*buffer) : 0);
    return;
  }

  if (chipnomadRender(chipnomadState, floatBuffer, stereoSamples) != stereoSamples) {
    memset(floatBuffer, 0, stereoSamples * 2 * sizeof(*floatBuffer));
  }
  samplePreviewVoice.render(samplePreviewBuffer, stereoSamples);
  for (int i = 0; i < stereoSamples * 2; ++i) floatBuffer[i] += samplePreviewBuffer[i];

  // Convert float to int16_t
  for (int i = 0; i < stereoSamples * 2; i++) {
    int sample = floatBuffer[i] * 32767;
    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;
    buffer[i] = sample;
  }

  const auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::steady_clock::now() - startedAt).count();
  int load = (stereoSamples > 0 && aSampleRate > 0)
    ? (int)(elapsedNs * aSampleRate / (stereoSamples * 10000000LL))
    : 0;
  if (load > 999) load = 999;
  int previous = cpuLoadPercent.load(std::memory_order_relaxed);
  cpuLoadPercent.store((previous * 7 + load) / 8, std::memory_order_relaxed);
}

static int start(int sampleRate, int bufferSize) {
  if (sampleRate <= 0 || bufferSize <= 0 || bufferSize > INT_MAX / 2) return 1;

  aSampleRate = sampleRate;
  aBufferSize = bufferSize;
  cpuLoadPercent.store(0, std::memory_order_relaxed);
  memset(&samplePreview, 0, sizeof(samplePreview));
  samplePreviewVoice.init((float)sampleRate);

  free(floatBuffer);
  free(samplePreviewBuffer);
  floatBuffer = (float*)malloc(bufferSize * 2 * sizeof(float));
  samplePreviewBuffer = (float*)malloc(bufferSize * 2 * sizeof(float));
  if (!floatBuffer || !samplePreviewBuffer) {
    free(floatBuffer);
    free(samplePreviewBuffer);
    floatBuffer = NULL;
    samplePreviewBuffer = NULL;
    return 1;
  }
  if (chipnomadReserveRenderBuffers(chipnomadState, bufferSize)) {
    free(floatBuffer);
    free(samplePreviewBuffer);
    floatBuffer = NULL;
    samplePreviewBuffer = NULL;
    return 1;
  }

  if (audioSetup(audioCallback, sampleRate, bufferSize)) {
    free(floatBuffer);
    free(samplePreviewBuffer);
    floatBuffer = NULL;
    samplePreviewBuffer = NULL;
    return 1;
  }

  // Initialize track states
  for (int i = 0; i < PROJECT_MAX_TRACKS; i++) {
    audioManager.trackStates[i] = TRACK_NORMAL;
  }
  updatePlaybackMuteFlags();

  return 0;
}

static void pause(void) {
  audioPause(1);
}

static void resume(void) {
  audioPause(0);
}

static void reinitializeChips(void) {
  pause();
  chipnomadInitChips(chipnomadState, aSampleRate, NULL);
  resume();
}

static void stop() {
  audioCleanup();
  free(floatBuffer);
  free(samplePreviewBuffer);
  floatBuffer = NULL;
  samplePreviewBuffer = NULL;
  samplePreviewVoice.kill();
  free(samplePreview.data);
  samplePreview.data = NULL;
}

static void toggleTrackMute(int trackIdx) {
  if (trackIdx >= 0 && trackIdx < PROJECT_MAX_TRACKS) {
    // Clear all solos when switching to mute mode
    for (int i = 0; i < PROJECT_MAX_TRACKS; i++) {
      if (audioManager.trackStates[i] == TRACK_SOLO) {
        audioManager.trackStates[i] = TRACK_NORMAL;
      }
    }

    if (audioManager.trackStates[trackIdx] == TRACK_MUTED) {
      audioManager.trackStates[trackIdx] = TRACK_NORMAL;
    } else {
      audioManager.trackStates[trackIdx] = TRACK_MUTED;
    }
  }

  updatePlaybackMuteFlags();
}

static void toggleTrackSolo(int trackIdx) {
  if (trackIdx >= 0 && trackIdx < PROJECT_MAX_TRACKS) {
    // Clear all mutes when switching to solo mode
    for (int i = 0; i < PROJECT_MAX_TRACKS; i++) {
      if (audioManager.trackStates[i] == TRACK_MUTED) {
        audioManager.trackStates[i] = TRACK_NORMAL;
      }
    }

    if (audioManager.trackStates[trackIdx] == TRACK_SOLO) {
      audioManager.trackStates[trackIdx] = TRACK_NORMAL;
    } else {
      audioManager.trackStates[trackIdx] = TRACK_SOLO;
    }
  }

  updatePlaybackMuteFlags();
}

static int getCpuLoadPercent(void) {
  return cpuLoadPercent.load(std::memory_order_relaxed);
}

static void stopSamplePreview(void) {
  pause();
  samplePreviewVoice.kill();
  free(samplePreview.data);
  memset(&samplePreview, 0, sizeof(samplePreview));
  resume();
}

static int previewSample(const char* path) {
  pause();
  samplePreviewVoice.kill();
  char error[64];
  int result = sampleLoadWav16(path, &samplePreview, error, sizeof(error));
  if (!result) {
    samplePreview.end = 255;
    samplePreview.sustain = 255;
    samplePreview.filterCutoffHz = 20000;
    samplePreviewVoice.configure(&samplePreview, 0.0f, 1.0f, 100.0f, 0, 255, 0, 20000, 0);
    samplePreviewVoice.noteOn();
  }
  resume();
  return result;
}


// Singleton AudioManager struct
struct AudioManager audioManager = {
  .start = start,
  .pause = pause,
  .resume = resume,
  .reinitializeChips = reinitializeChips,
  .stop = stop,
  .toggleTrackMute = toggleTrackMute,
  .toggleTrackSolo = toggleTrackSolo,
  .getCpuLoadPercent = getCpuLoadPercent,
  .previewSample = previewSample,
  .stopSamplePreview = stopSamplePreview,
};
