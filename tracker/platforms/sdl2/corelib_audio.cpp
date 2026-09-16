#include "corelib_audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#ifdef ANDROID_BUILD
#include "../android/audio_diagnostics.h"
#include <android/log.h>
#include <math.h>

static SDL_Thread* diagnosticThread;
static SDL_atomic_t diagnosticStop;
static std::atomic<uint32_t> callbackCount{0}, maxRenderUs{0}, maxGapUs{0};
static std::atomic<uint32_t> lastFrames{0}, overBudget{0};
static Uint64 previousCallback, diagnosticFrequency;
static int diagnosticRate;
static bool diagnosticTone;
static double tonePhase;

static void recordMaximum(std::atomic<uint32_t>& maximum, uint32_t value) {
  uint32_t old = maximum.load(std::memory_order_relaxed);
  while (old < value && !maximum.compare_exchange_weak(old, value, std::memory_order_relaxed)) {}
}

static int reportAudioDiagnostics(void*) {
  unsigned ticks = 0;
  while (!SDL_AtomicGet(&diagnosticStop)) {
    SDL_Delay(100);
    if (++ticks % 10) continue;
    __android_log_print(ANDROID_LOG_INFO, "CCTAudio",
      "callbacks=%u frames=%u render_max_us=%u gap_max_us=%u over_budget=%u invalid_buffers=%u render_failures=%u",
      callbackCount.load(std::memory_order_relaxed), lastFrames.load(std::memory_order_relaxed),
      maxRenderUs.exchange(0, std::memory_order_relaxed), maxGapUs.exchange(0, std::memory_order_relaxed),
      overBudget.load(std::memory_order_relaxed),
      audioDiagnostics::invalidBuffers.load(std::memory_order_relaxed),
      audioDiagnostics::renderFailures.load(std::memory_order_relaxed));
  }
  return 0;
}
#endif

static void sdlAudioCallback(void* userdata, uint8_t* buffer, int bufferBytes) {
  AudioCallback* callback = (AudioCallback *)userdata;
#ifdef ANDROID_BUILD
  if (audioDiagnostics::enabled) {
    const Uint64 started = SDL_GetPerformanceCounter();
    if (previousCallback)
      recordMaximum(maxGapUs, (started - previousCallback) * 1000000 / diagnosticFrequency);
    previousCallback = started;
    const int frames = bufferBytes / sizeof(int16_t) / 2;
    if (diagnosticTone) {
      int16_t* samples = (int16_t*)buffer;
      const double step = 6.283185307179586 * 440.0 / diagnosticRate;
      for (int i = 0; i < frames; ++i) {
        samples[2 * i] = samples[2 * i + 1] = (int16_t)(sin(tonePhase) * 2048.0);
        tonePhase += step;
        if (tonePhase >= 6.283185307179586) tonePhase -= 6.283185307179586;
      }
    } else {
      callback((int16_t*)buffer, frames);
    }
    const Uint64 elapsed = SDL_GetPerformanceCounter() - started;
    recordMaximum(maxRenderUs, elapsed * 1000000 / diagnosticFrequency);
    if (elapsed * diagnosticRate > (Uint64)frames * diagnosticFrequency)
      overBudget.fetch_add(1, std::memory_order_relaxed);
    lastFrames.store(frames, std::memory_order_relaxed);
    callbackCount.fetch_add(1, std::memory_order_relaxed);
    return;
  }
#endif
  callback((int16_t *)buffer, bufferBytes / sizeof(int16_t) / 2); // Divide by 2 to get number of stereo samples
}

int audioSetup(AudioCallback* audioCallback, int sampleRate, int bufferSize) {
#ifdef ANDROID_BUILD
  const char* diagnostic = SDL_getenv("CCT_AUDIO_DIAG");
  audioDiagnostics::enabled = diagnostic && SDL_strcmp(diagnostic, "1") == 0;
  if (audioDiagnostics::enabled) {
    const char* tone = SDL_getenv("CCT_AUDIO_TONE");
    diagnosticTone = tone && SDL_strcmp(tone, "1") == 0;
    diagnosticRate = sampleRate;
    diagnosticFrequency = SDL_GetPerformanceFrequency();
    previousCallback = 0;
    tonePhase = 0;
    callbackCount = maxRenderUs = maxGapUs = lastFrames = overBudget = 0;
    audioDiagnostics::invalidBuffers = audioDiagnostics::renderFailures = 0;
  }
#endif
  SDL_AudioSpec spec;
  SDL_memset(&spec, 0, sizeof(spec));
  spec.freq = sampleRate;
  spec.format = AUDIO_S16;
  spec.channels = 2;
  spec.samples = bufferSize;
  spec.callback = sdlAudioCallback;
  spec.userdata = (void *)audioCallback;

  if (SDL_OpenAudio(&spec, NULL) < 0) {
    fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
    return 1;
  }
#ifdef ANDROID_BUILD
  if (audioDiagnostics::enabled) {
    __android_log_print(ANDROID_LOG_INFO, "CCTAudio",
      "backend=%s rate=%d requested_frames=%d callback_bytes=%u format=%u channels=%u tone=%d",
      SDL_GetCurrentAudioDriver(), sampleRate, bufferSize, spec.size,
      spec.format, spec.channels, diagnosticTone);
    SDL_AtomicSet(&diagnosticStop, 0);
    diagnosticThread = SDL_CreateThread(reportAudioDiagnostics, "CCTAudioStats", NULL);
    if (!diagnosticThread)
      __android_log_print(ANDROID_LOG_ERROR, "CCTAudio", "Statistics thread: %s", SDL_GetError());
  }
#endif
  return 0;
}

void audioPause(int isPaused) {
#ifdef ANDROID_BUILD
  // Reset only while SDL holds the callback paused, so intentional pauses do
  // not appear as scheduling gaps and the callback owns its timing state.
  if (!isPaused && SDL_GetAudioStatus() == SDL_AUDIO_PAUSED) previousCallback = 0;
#endif
  SDL_PauseAudio(isPaused);
}

void audioCleanup(void) {
  SDL_CloseAudio();
#ifdef ANDROID_BUILD
  if (diagnosticThread) {
    SDL_AtomicSet(&diagnosticStop, 1);
    SDL_WaitThread(diagnosticThread, NULL);
    diagnosticThread = NULL;
  }
#endif
}
