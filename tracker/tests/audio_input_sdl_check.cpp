// Standalone SDL/ALSA fallback check; the main test suite uses a platform mock.
#include "corelib_audio.h"
#include <SDL2/SDL.h>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static const char* inputName = "audiocodec, ";
static const char* driver = "alsa";
static std::vector<std::string> attempts;
static int successAttempt;
static int paused;
static int closed;
static int failSetCall;
static int setCalls;
static bool failUnset;

extern "C" int __real_setenv(const char*, const char*, int);
extern "C" int __real_unsetenv(const char*);

extern "C" const char* __wrap_SDL_GetAudioDeviceName(int, int) {
  return inputName;
}

extern "C" const char* __wrap_SDL_GetCurrentAudioDriver() {
  return driver;
}

extern "C" SDL_AudioDeviceID __wrap_SDL_OpenAudioDevice(
    const char* device, int capture, const SDL_AudioSpec* spec, SDL_AudioSpec*, int) {
  assert(capture == SDL_TRUE);
  assert(spec->freq == 48000 && spec->format == AUDIO_S16);
  assert(spec->channels == 2 && spec->samples == 512);
  const char* audioDev = getenv("AUDIODEV");
  attempts.emplace_back(device ? device : (audioDev ? audioDev : "<unset>"));
  if ((int)attempts.size() == successAttempt) return 42;
  SDL_SetError("simulated failure");
  return 0;
}

extern "C" void __wrap_SDL_PauseAudioDevice(SDL_AudioDeviceID device, int pause) {
  assert(device == 42 && pause == 0);
  ++paused;
}

extern "C" void __wrap_SDL_CloseAudioDevice(SDL_AudioDeviceID device) {
  assert(device == 42);
  ++closed;
}

extern "C" int __wrap_setenv(const char* name, const char* value, int overwrite) {
  if (++setCalls == failSetCall) {
    errno = ENOMEM;
    return -1;
  }
  return __real_setenv(name, value, overwrite);
}

extern "C" int __wrap_unsetenv(const char* name) {
  if (failUnset) {
    errno = EINVAL;
    return -1;
  }
  return __real_unsetenv(name);
}

static void capture(const int16_t*, int, int) {}

static void reset(const char* previous = nullptr) {
  audioInputCleanup();
  if (previous) assert(__real_setenv("AUDIODEV", previous, 1) == 0);
  else assert(__real_unsetenv("AUDIODEV") == 0);
  inputName = "audiocodec, ";
  driver = "alsa";
  attempts.clear();
  successAttempt = paused = closed = failSetCall = setCalls = 0;
  failUnset = false;
}

static int open() {
  return audioInputSetup(capture, 48000, 512, 2, 0);
}

int main() {
  const std::string named = "audiocodec, ";
  const std::string raw = "hw:CARD=audiocodec,DEV=0";
  const std::string plug = "plughw:CARD=audiocodec,DEV=0";

  reset("original");
  successAttempt = 1;
  assert(open() == 0 && attempts == std::vector<std::string>{named});
  assert(std::strcmp(getenv("AUDIODEV"), "original") == 0 && paused == 1);

  reset("original");
  successAttempt = 2;
  assert(open() == 0 && (attempts == std::vector<std::string>{named, raw}));
  assert(std::strcmp(getenv("AUDIODEV"), "original") == 0 && paused == 1);

  reset();
  successAttempt = 3;
  assert(open() == 0 && (attempts == std::vector<std::string>{named, raw, plug}));
  assert(getenv("AUDIODEV") == nullptr && paused == 1);

  reset("");
  assert(open() != 0 && (attempts == std::vector<std::string>{named, raw, plug}));
  assert(getenv("AUDIODEV") && !*getenv("AUDIODEV") && paused == 0);

  reset("original");
  inputName = "USB audiocodec, ";
  assert(open() != 0 && attempts == std::vector<std::string>{inputName});
  assert(std::strcmp(getenv("AUDIODEV"), "original") == 0);

  reset();
  driver = "pulseaudio";
  assert(open() != 0 && attempts == std::vector<std::string>{named});
  assert(getenv("AUDIODEV") == nullptr);

  reset("original");
  inputName = nullptr;
  assert(open() != 0 && attempts == std::vector<std::string>{"original"});
  assert(std::strcmp(getenv("AUDIODEV"), "original") == 0);

  reset("original");
  failSetCall = 1;
  assert(open() != 0 && attempts == std::vector<std::string>{named});
  assert(std::strcmp(getenv("AUDIODEV"), "original") == 0);

  reset("original");
  failSetCall = 3;
  assert(open() != 0 && (attempts == std::vector<std::string>{named, raw}));
  assert(std::strcmp(getenv("AUDIODEV"), "original") == 0);

  reset("original");
  successAttempt = 2;
  failSetCall = 2;
  assert(open() != 0 && (attempts == std::vector<std::string>{named, raw}));
  assert(closed == 1 && paused == 0);

  reset();
  successAttempt = 2;
  failUnset = true;
  assert(open() != 0 && (attempts == std::vector<std::string>{named, raw}));
  assert(closed == 1 && paused == 0);

  reset();
  return 0;
}
