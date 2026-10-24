#ifndef __CORELIB_AUDIO_H__
#define __CORELIB_AUDIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>

// 16-bit interleaved stereo buffer
typedef void AudioCallback(int16_t* buffer, int stereoSamples);

// Audio capture callback: receives captured mono/stereo int16 samples.
typedef void AudioCaptureCallback(const int16_t* buffer, int stereoSamples, int channels);

int audioSetup(AudioCallback* audioCallback, int sampleRate, int bufferSize);
void audioPause(int isPaused);
void audioLock(void);
void audioUnlock(void);
void audioCleanup(void);

// Audio input (capture) API. Mirrors the output API above. Availability
// varies per platform: SDL2 backends enumerate real devices; SDL 1.2
// (RG35xx) has no capture support and stubs return failure.
// Enumerates available audio input devices. Returns the device count.
int audioInputDeviceCount(void);
// Copies the name of capture device `index` into buffer. Returns 0 on
// success, non-zero when the index is out of range.
int audioInputDeviceName(int index, char* buffer, int bufferSize);
// Opens a capture device. deviceIndex < 0 opens the system default.
// Returns 0 on success, non-zero on failure (no device, denied permission).
int audioInputSetup(AudioCaptureCallback* callback, int sampleRate,
                    int bufferSize, int channels, int deviceIndex);
void audioInputPause(int isPaused);
void audioInputCleanup(void);
// Re-enumerates capture devices after hot-plug. On SDL2 this re-queries the
// device list (SDL caches it until the audio subsystem re-inits, so this
// performs SDL_QuitSubSystem/SDL_InitSubSystem(SDL_INIT_AUDIO) when needed).
// Returns 0 on success.
int audioInputRefreshDevices(void);


#ifdef __cplusplus
}
#endif

#endif
