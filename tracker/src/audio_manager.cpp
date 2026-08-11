#include <stdio.h>
#include <atomic>
#include <chrono>
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
static float* samplePreviewBuffer;
static int samplePreviewBufferSize;

int pendingReinitChips = 0;

static void updatePlaybackMuteFlags(void) {
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
      chipnomadState->playbackState.trackEnabled[i] = (audioManager.trackStates[i] == TRACK_SOLO) ? 1 : 0;
    } else {
      // Mute mode: muted tracks are disabled, others enabled
      chipnomadState->playbackState.trackEnabled[i] = (audioManager.trackStates[i] == TRACK_MUTED) ? 0 : 1;
    }
  }
}

static void audioCallback(int16_t* buffer, int stereoSamples) {
  const auto startedAt = std::chrono::steady_clock::now();

  if (pendingReinitChips) {
    chipnomadInitChips(chipnomadState, aSampleRate, NULL);
    pendingReinitChips = 0;
  }

  static float* floatBuffer = NULL;
  static int floatBufferSize = 0;

  if (floatBufferSize < stereoSamples * 2) {
    floatBuffer = (float*)realloc(floatBuffer, stereoSamples * 2 * sizeof(float));
    floatBufferSize = stereoSamples * 2;
  }

  chipnomadRender(chipnomadState, floatBuffer, stereoSamples);
  if (samplePreviewBufferSize < stereoSamples * 2) {
    samplePreviewBuffer = (float*)realloc(samplePreviewBuffer,
      stereoSamples * 2 * sizeof(float));
    samplePreviewBufferSize = stereoSamples * 2;
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
  aSampleRate = sampleRate;
  aBufferSize = bufferSize;
  cpuLoadPercent.store(0, std::memory_order_relaxed);
  memset(&samplePreview, 0, sizeof(samplePreview));
  samplePreviewVoice.init((float)sampleRate);

  audioSetup(audioCallback, sampleRate, bufferSize);

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

static void stop() {
  audioCleanup();
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
    samplePreviewVoice.configure(&samplePreview, 0.0f, 1.0f, 0, 255, 20000, 0);
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
  .stop = stop,
  .toggleTrackMute = toggleTrackMute,
  .toggleTrackSolo = toggleTrackSolo,
  .getCpuLoadPercent = getCpuLoadPercent,
  .previewSample = previewSample,
  .stopSamplePreview = stopSamplePreview,
};
