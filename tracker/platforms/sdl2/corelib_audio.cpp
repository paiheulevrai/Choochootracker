#include "corelib_audio.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <cstring>
#ifdef PORTMASTER_BUILD
#include <cstdlib>
#include <string>
#endif
#ifdef WEB_BUILD
#include <emscripten.h>
static AudioCallback* webAudioCallback = nullptr;
static int webSampleRate, webBufferSize;
#endif

// Strong override of the weak default in link_client.cpp (T39): the
// measured device output latency used by Link's beat-0 launch scheduler.
// Same C++ linkage as the declaration in link_backend_internal.h.
uint64_t outputLatencyMicros(void);
static uint64_t measuredOutputLatencyMicros = 0;
uint64_t outputLatencyMicros(void) { return measuredOutputLatencyMicros; }
#ifdef ANDROID_BUILD
#include "../android/audio_diagnostics.h"
#include <jni.h>
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
#ifdef WEB_BUILD
  webAudioCallback = audioCallback;
  webSampleRate = sampleRate;
  webBufferSize = bufferSize;
#endif
  // Measured output latency for Ableton Link scheduling (T39): the time
  // audio spends in SDL's buffer before reaching the DAC. PortMaster
  // buffer 512 @ 48 kHz = ~10667 us; desktop 256 @ 48 kHz = ~5333 us.
  // AUDIOENGINE-1 caveat (documented ship exclusion on PortMaster): the
  // true DAC latency of ArkOS/ALSA is unmeasured, so this is a lower
  // bound on the real output latency there.
  measuredOutputLatencyMicros = (uint64_t)((uint64_t)spec.samples * 1000000ULL / (uint64_t)spec.freq);
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

void audioLock(void) { SDL_LockAudio(); }
void audioUnlock(void) { SDL_UnlockAudio(); }

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

#ifdef WEB_BUILD
extern "C" EMSCRIPTEN_KEEPALIVE int webRestartAudio() {
  if (!webAudioCallback) return 1;
  // Reopen only SDL's browser output. The tracker, synth state, project and
  // loaded samples remain alive, including when Safari's old context is stuck.
  audioPause(1);
  audioCleanup();
  const int result = audioSetup(webAudioCallback, webSampleRate, webBufferSize);
  if (!result) audioPause(0);
  return result;
}
#endif

// ---------------------------------------------------------------------------
// Audio input (capture)
// ---------------------------------------------------------------------------

static SDL_AudioDeviceID captureDevice = 0;

#ifdef ANDROID_BUILD
// SDL's Android backend enumerates capture devices by their numeric id: the
// "name" SDL reports IS the id as a decimal string, and opening parses it
// back with SDL_atoi. The numeric string must therefore stay the device's
// identity for opening, but it is useless for display. The tracker asks the
// Java side for a human-readable name for the menu instead.
static JavaVM* audioJavaVm = nullptr;
static jobject audioJavaActivity = nullptr;
static jmethodID audioInputDeviceNameMethod = nullptr;

// The helper methods live on org.libsdl.app.SDLAudioManager as statics, NOT on
// the activity class. GetMethodID on the activity class returns null and ART
// aborts with "JNI DETECTED ERROR IN APPLICATION: mid == null" on the call.
static jclass audioManagerClass = nullptr;

struct AttachedJavaEnv {
  JNIEnv* env = nullptr;
  bool attached = false;
  bool get(void) {
    if (!audioJavaVm) return false;
    if (audioJavaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) return true;
    if (audioJavaVm->AttachCurrentThread(&env, nullptr) != JNI_OK) return false;
    attached = true;
    return true;
  }
  ~AttachedJavaEnv(void) { if (attached) audioJavaVm->DetachCurrentThread(); }
};

static bool ensureAudioJava(void) {
  if (audioInputDeviceNameMethod) return true;
  JNIEnv* env = reinterpret_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
  jobject current = reinterpret_cast<jobject>(SDL_AndroidGetActivity());
  if (!env || !current) return false;
  env->GetJavaVM(&audioJavaVm);
  audioJavaActivity = env->NewGlobalRef(current);
  env->DeleteLocalRef(current);
  jclass cls = env->FindClass("org/libsdl/app/SDLAudioManager");
  if (!cls) {
    env->ExceptionClear();
    return false;
  }
  audioManagerClass = reinterpret_cast<jclass>(env->NewGlobalRef(cls));
  env->DeleteLocalRef(cls);
  if (!audioManagerClass) return false;
  audioInputDeviceNameMethod = env->GetStaticMethodID(audioManagerClass,
    "audioInputDeviceName", "(I)Ljava/lang/String;");
  if (!audioInputDeviceNameMethod) {
    env->ExceptionClear();
    return false;
  }
  return true;
}

static int androidInputName(int deviceId, char* buffer, int bufferSize) {
  AttachedJavaEnv attached;
  if (!ensureAudioJava() || !attached.get()) return 1;
  jstring name = (jstring)attached.env->CallStaticObjectMethod(audioManagerClass,
    audioInputDeviceNameMethod, (jint)deviceId);
  if (attached.env->ExceptionCheck()) {
    attached.env->ExceptionClear();
    return 1;
  }
  if (!name) return 1;
  const char* utf = attached.env->GetStringUTFChars(name, nullptr);
  if (!utf) { attached.env->DeleteLocalRef(name); return 1; }
  strncpy(buffer, utf, bufferSize - 1);
  buffer[bufferSize - 1] = 0;
  attached.env->ReleaseStringUTFChars(name, utf);
  attached.env->DeleteLocalRef(name);
  return 0;
}

// True when the SDL device "name" is just the numeric id (Android backend).
static int isNumericDeviceName(const char* name) {
  if (!name || !*name) return 0;
  for (const char* p = name; *p; ++p) {
    if (*p < '0' || *p > '9') return 0;
  }
  return 1;
}
#endif

int audioInputDeviceCount(void) {
  return SDL_GetNumAudioDevices(SDL_TRUE);
}

int audioInputDeviceName(int index, char* buffer, int bufferSize) {
  const char* name = SDL_GetAudioDeviceName(index, SDL_TRUE);
  if (!name) return 1;
#ifdef ANDROID_BUILD
  // Keep the numeric id as the device identity (audioInputSetup opens by
  // index through SDL_GetAudioDeviceName, which the aaudio backend parses
  // back to an id), but hand the menu a readable name for display.
  if (isNumericDeviceName(name)) {
    if (androidInputName(SDL_atoi(name), buffer, bufferSize) == 0) return 0;
  }
#endif
  strncpy(buffer, name, bufferSize - 1);
  buffer[bufferSize - 1] = 0;
  return 0;
}

static void sdlCaptureCallback(void* userdata, uint8_t* buffer, int bufferBytes) {
  AudioCaptureCallback* cb = (AudioCaptureCallback*)userdata;
  cb((const int16_t*)buffer, bufferBytes / sizeof(int16_t) / 2, 2);
}

int audioInputSetup(AudioCaptureCallback* callback, int sampleRate,
                    int bufferSize, int channels, int deviceIndex) {
  SDL_AudioSpec spec;
  SDL_memset(&spec, 0, sizeof(spec));
  spec.freq = sampleRate;
  spec.format = AUDIO_S16;
  spec.channels = channels;
  spec.samples = bufferSize;
  spec.callback = sdlCaptureCallback;
  spec.userdata = (void*)callback;

  const char* deviceName = deviceIndex >= 0 ? SDL_GetAudioDeviceName(deviceIndex, SDL_TRUE) : NULL;
  captureDevice = SDL_OpenAudioDevice(deviceName, SDL_TRUE, &spec, NULL, 0);
  if (captureDevice == 0) {
    fprintf(stderr, "Failed to open audio input '%s': %s\n",
      deviceName ? deviceName : "default", SDL_GetError());
#ifdef PORTMASTER_BUILD
    // The built-in codec's raw PCM may reject our stereo capture format.
    // SDL's ALSA backend opens AUDIODEV when no device name is supplied;
    // plughw converts to the codec's supported format.
    if (deviceName && SDL_GetCurrentAudioDriver() &&
        SDL_strcmp(SDL_GetCurrentAudioDriver(), "alsa") == 0 &&
        SDL_strstr(deviceName, "audiocodec")) {
      const char* previous = getenv("AUDIODEV");
      const bool hadAudioDev = previous != nullptr;
      const std::string savedAudioDev = hadAudioDev ? previous : "";
      if (setenv("AUDIODEV", "plughw:CARD=audiocodec,DEV=0", 1) == 0) {
        captureDevice = SDL_OpenAudioDevice(NULL, SDL_TRUE, &spec, NULL, 0);
        if (hadAudioDev) setenv("AUDIODEV", savedAudioDev.c_str(), 1);
        else unsetenv("AUDIODEV");
      }
      if (captureDevice == 0)
        fprintf(stderr, "Failed to open built-in microphone via plughw:audiocodec,0: %s\n", SDL_GetError());
      else
        fprintf(stderr, "Opened built-in microphone via plughw:audiocodec,0\n");
    }
#endif
  }
  if (captureDevice == 0) return 1;
  SDL_PauseAudioDevice(captureDevice, 0);
  return 0;
}

int audioInputRefreshDevices(void) {
  // SDL caches the device list until the audio subsystem re-initializes.
  // Re-init it so hot-plugged USB interfaces show up. Capture devices must be
  // closed first (the caller disarms before calling this).
  if (captureDevice) return 1;
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) return 1;
  return 0;
}

void audioInputPause(int isPaused) {
  if (captureDevice) SDL_PauseAudioDevice(captureDevice, isPaused ? 1 : 0);
}

void audioInputCleanup(void) {
  if (captureDevice) {
    SDL_CloseAudioDevice(captureDevice);
    captureDevice = 0;
  }
}
