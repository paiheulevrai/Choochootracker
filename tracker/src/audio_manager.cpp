#include <stdio.h>
#include <atomic>
#include <chrono>
#include <limits.h>
#include <string.h>
#include "audio_manager.h"
#include "corelib_audio.h"

#include "chipnomad_lib.h"
#include "playback.h"
#include "corelib_file.h"
#include "synth/sample_voice.h"
#include "synth/scwf_voice.h"
#include "synth/sr_wavetable_loader.h"
#include "import/import_wav.h"
#ifdef ANDROID_BUILD
#include "../platforms/android/audio_diagnostics.h"
#endif

static int aSampleRate;
static int aBufferSize;
static std::atomic<int> cpuLoadPercent{0};
static SampleVoice samplePreviewVoice;
static InstrumentSample samplePreview;
static SCWFVoice scwfPreviewVoice;
static InstrumentSCWF scwfPreview;
static InstrumentBYOWTBL byowtblPreview;
static ChipNomadState* ayPreviewState;
static int previewMode;
static int byowtblPreviewOscillator;
static uint64_t byowtblPreviewFrames;
static float* floatBuffer;
static float* samplePreviewBuffer;

enum { PREVIEW_NONE, PREVIEW_PCM, PREVIEW_AY, PREVIEW_SCWF, PREVIEW_BYOWTBL };
static constexpr double previewTwoPi = 6.28318530717958647692;

static void freeSCWFPreview(void) {
  free(scwfPreview.oscillator[0].data);
  free(scwfPreview.oscillator[1].data);
  memset(&scwfPreview, 0, sizeof(scwfPreview));
  free(byowtblPreview.oscillator[0].data);
  free(byowtblPreview.oscillator[1].data);
  memset(&byowtblPreview, 0, sizeof(byowtblPreview));
}

static void stopSamplePreview(void) {
  samplePreviewVoice.kill();
  scwfPreviewVoice.kill();
  free(samplePreview.data);
  memset(&samplePreview, 0, sizeof(samplePreview));
  freeSCWFPreview();
  if (ayPreviewState) chipnomadDestroy(ayPreviewState);
  ayPreviewState = NULL;
  previewMode = PREVIEW_NONE;
  byowtblPreviewFrames = 0;
}

static void configureSCWFPreview(const InstrumentSCWF* instrument, const uint16_t* frameSize,
                                 const uint8_t* frameIndex) {
  scwfPreviewVoice.configure(instrument, 3600.0f, 1.0f,
    scwfDetuneCents(instrument->detune), instrument->mix,
    instrument->filterCutoffHz, instrument->filterResonance,
    frameSize, frameIndex);
}

static void renderPreview(float* buffer, int frames) {
  memset(buffer, 0, frames * 2 * sizeof(*buffer));
  if (previewMode == PREVIEW_PCM) samplePreviewVoice.render(buffer, frames);
  else if (previewMode == PREVIEW_AY && ayPreviewState) chipnomadRender(ayPreviewState, buffer, frames);
  else if (previewMode == PREVIEW_SCWF) scwfPreviewVoice.render(buffer, frames);
  else if (previewMode == PREVIEW_BYOWTBL) {
    uint8_t frameIndex[2] = {byowtblPreview.frameIndex[0], byowtblPreview.frameIndex[1]};
    const double phase = (double)byowtblPreviewFrames / aSampleRate;
    frameIndex[byowtblPreviewOscillator] = (uint8_t)(127.5 + 127.5 * sin(phase * previewTwoPi));
    configureSCWFPreview(&byowtblPreview, byowtblPreview.frameSize, frameIndex);
    scwfPreviewVoice.render(buffer, frames);
    byowtblPreviewFrames += frames;
  }
}

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
#ifdef ANDROID_BUILD
    if (audioDiagnostics::enabled)
      audioDiagnostics::invalidBuffers.fetch_add(1, std::memory_order_relaxed);
#endif
    if (stereoSamples > aBufferSize) chipnomadSetRenderBufferOverflow(chipnomadState);
    memset(buffer, 0, stereoSamples > 0 ? stereoSamples * 2 * sizeof(*buffer) : 0);
    return;
  }

  if (chipnomadRender(chipnomadState, floatBuffer, stereoSamples) != stereoSamples) {
#ifdef ANDROID_BUILD
    if (audioDiagnostics::enabled)
      audioDiagnostics::renderFailures.fetch_add(1, std::memory_order_relaxed);
#endif
    memset(floatBuffer, 0, stereoSamples * 2 * sizeof(*floatBuffer));
  }
  renderPreview(samplePreviewBuffer, stereoSamples);
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
  scwfPreviewVoice.init((float)sampleRate);

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

static void replaceProject(Project* replacement) {
  if (!replacement || !chipnomadState) return;
  pause();
  chipnomadDiscardQueuedProject(chipnomadState);
  projectFree(&chipnomadState->project);
  chipnomadState->project = *replacement;
  memset(replacement, 0, sizeof(*replacement));
  chipnomadInitChips(chipnomadState, aSampleRate, NULL);
  playbackStop(&chipnomadState->playbackState);
  resume();
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
  stopSamplePreview();
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

static void stopSamplePreviewLocked(void) {
  pause();
  stopSamplePreview();
  resume();
}

static int previewSample(const char* path) {
  pause();
  stopSamplePreview();
  char error[64];
  int result = sampleLoadWav16(path, &samplePreview, error, sizeof(error));
  if (!result) {
    samplePreview.end = 255;
    samplePreview.sustain = 255;
    samplePreview.filterCutoffHz = 20000;
    samplePreviewVoice.configure(&samplePreview, 0.0f, 1.0f, 100.0f, 0, 255, 0, 20000, 0);
    samplePreviewVoice.noteOn();
    previewMode = PREVIEW_PCM;
  }
  resume();
  return result;
}

static int previewAYSample(const char* path, const InstrumentAYSample* settings) {
  if (!settings) return 1;
  pause();
  stopSamplePreview();
  uint16_t length, rate;
  WavLoadResult wavResult;
  uint8_t* data = loadWavFile(path, PROJECT_MAX_SAMPLE_SIZE, &length, &rate, &wavResult);
  if (!data) { resume(); return 1; }
  ayPreviewState = chipnomadCreate();
  if (!ayPreviewState) { free(data); resume(); return 1; }
  Project* project = &ayPreviewState->project;
  project->tracksCount = project->chipsCount = 1;
  project->tickRate = chipnomadState->project.tickRate;
  project->chipType = chipnomadState->project.chipType;
  project->chipSetup = chipnomadState->project.chipSetup;
  project->linearPitch = chipnomadState->project.linearPitch;
  project->pitchTable = chipnomadState->project.pitchTable;
  project->instruments[0].type = InstrumentType::AYSample;
  project->instruments[0].volume = 255;
  project->instruments[0].chip.aySample = *settings;
  InstrumentAYSample* sample = &project->instruments[0].chip.aySample;
  sample->sampleData = data;
  sample->fileLength = sample->sampleLength = length;
  sample->sampleRate = rate;
  sample->sampleStart = 0;
  sample->sampleLoopStart = length;
  ayPreviewState->aySampleDithering = chipnomadState->aySampleDithering;
  chipnomadInitChips(ayPreviewState, aSampleRate, NULL);
  if (chipnomadReserveRenderBuffers(ayPreviewState, aBufferSize)) {
    chipnomadDestroy(ayPreviewState);
    ayPreviewState = NULL;
    resume();
    return 1;
  }
  playbackPreviewNote(&ayPreviewState->playbackState, 0, 36, 0);
  previewMode = PREVIEW_AY;
  resume();
  return 0;
}

static int previewSCWFCommon(const char* path, const InstrumentSCWF* settings,
                             int oscillator, int byowtbl) {
  if (!settings || oscillator < 0 || oscillator > 1) return 1;
  pause();
  // SCWF previews are intentionally toggled by START: the second press stops
  // the currently sounding oscillator/table without reloading the WAV.
  if ((byowtbl && previewMode == PREVIEW_BYOWTBL) || (!byowtbl && previewMode == PREVIEW_SCWF)) {
    stopSamplePreview();
    resume();
    return 0;
  }
  stopSamplePreview();
  char error[64];
  InstrumentSCWF* preview = byowtbl ? static_cast<InstrumentSCWF*>(&byowtblPreview) : &scwfPreview;
  *preview = *settings;
  memset(&preview->oscillator[0], 0, sizeof(preview->oscillator[0]));
  memset(&preview->oscillator[1], 0, sizeof(preview->oscillator[1]));
  int loadResult = byowtbl
    ? srWavetableLoadWav(path, &preview->oscillator[oscillator],
        &byowtblPreview.frameSize[oscillator], &byowtblPreview.tableFrames[oscillator], error, sizeof(error))
    : sampleLoadWav16(path, &preview->oscillator[oscillator], error, sizeof(error));
  if (loadResult) {
    freeSCWFPreview();
    resume();
    return 1;
  }
  preview->mix = oscillator ? 255 : 0;
  scwfPreviewVoice.init((float)aSampleRate);
  if (byowtbl) {
    byowtblPreviewOscillator = oscillator;
    byowtblPreviewFrames = 0;
    uint8_t frameIndex[2] = {byowtblPreview.frameIndex[0], byowtblPreview.frameIndex[1]};
    frameIndex[oscillator] = 127;
    configureSCWFPreview(&byowtblPreview, byowtblPreview.frameSize, frameIndex);
    previewMode = PREVIEW_BYOWTBL;
  } else {
    configureSCWFPreview(preview, NULL, NULL);
    previewMode = PREVIEW_SCWF;
  }
  scwfPreviewVoice.noteOn();
  resume();
  return 0;
}

static int previewSCWF(const char* path, const InstrumentSCWF* settings, int oscillator) {
  return previewSCWFCommon(path, settings, oscillator, 0);
}

static int previewBYOWTBL(const char* path, const InstrumentBYOWTBL* settings, int oscillator) {
  return previewSCWFCommon(path, settings, oscillator, 1);
}


// Singleton AudioManager struct
struct AudioManager audioManager = {
  .start = start,
  .pause = pause,
  .resume = resume,
  .replaceProject = replaceProject,
  .reinitializeChips = reinitializeChips,
  .stop = stop,
  .toggleTrackMute = toggleTrackMute,
  .toggleTrackSolo = toggleTrackSolo,
  .getCpuLoadPercent = getCpuLoadPercent,
  .previewSample = previewSample,
  .previewAYSample = previewAYSample,
  .previewSCWF = previewSCWF,
  .previewBYOWTBL = previewBYOWTBL,
  .stopSamplePreview = stopSamplePreviewLocked,
};
