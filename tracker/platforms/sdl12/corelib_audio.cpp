#include "corelib_audio.h"
#include <SDL/SDL.h>

static void sdlAudioCallback(void* userdata, uint8_t* buffer, int bufferBytes) {
  AudioCallback* callback = (AudioCallback*)userdata;

  callback((int16_t *)buffer, bufferBytes / sizeof(int16_t) / 2); // Divide by 2 to get number of stereo samples
}

int audioSetup(AudioCallback* audioCallback, int sampleRate, int bufferSize) {
  SDL_AudioSpec spec;
  SDL_memset(&spec, 0, sizeof(spec));
  spec.freq = sampleRate;
  spec.format = AUDIO_S16;
  spec.channels = 2;
  spec.samples = bufferSize;
  spec.callback = sdlAudioCallback;
  spec.userdata = (void*)audioCallback;

  if (SDL_OpenAudio(&spec, NULL) < 0) {
    fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
    return 1;
  }

  SDL_PauseAudio(0);

  return 0;
}

void audioPause(int isPaused) {
  SDL_PauseAudio(isPaused);
}

void audioLock(void) { SDL_LockAudio(); }
void audioUnlock(void) { SDL_UnlockAudio(); }

void audioCleanup(void) {
  SDL_CloseAudio();
}

// ---------------------------------------------------------------------------
// Audio input (capture) - SDL 1.2 has no capture API.
// RG35xx ships with the audio input feature stubbed out: every entry point
// reports "unavailable" so the UI can show a clear message instead of
// silently failing.
// ---------------------------------------------------------------------------

int audioInputDeviceCount(void) {
  return 0;
}

int audioInputDeviceName(int index, char* buffer, int bufferSize) {
  (void)index;
  if (buffer && bufferSize > 0) buffer[0] = 0;
  return 1;
}

int audioInputSetup(AudioCaptureCallback* callback, int sampleRate,
                    int bufferSize, int channels, int deviceIndex) {
  (void)callback; (void)sampleRate; (void)bufferSize; (void)channels; (void)deviceIndex;
  return 1;
}

void audioInputPause(int isPaused) {
  (void)isPaused;
}

int audioInputRefreshDevices(void) { return 1; }

void audioInputCleanup(void) {
}
