#include "corelib_audio.h"
#include "chipnomad_lib.h"

int audioSetup(AudioCallback* audioCallback, int sampleRate, int bufferSize) { return 0; }
void audioPause(int isPaused) {}
void audioLock(void) {}
void audioUnlock(void) {}
void audioCleanup(void) {}

int audioInputDeviceCount(void) { return 0; }
int audioInputDeviceName(int index, char* buffer, int bufferSize) {
  (void)index;
  if (buffer && bufferSize > 0) buffer[0] = 0;
  return 1;
}
int audioInputSetup(AudioCaptureCallback* callback, int sampleRate, int bufferSize, int channels, int deviceIndex) {
  (void)callback; (void)sampleRate; (void)bufferSize; (void)channels; (void)deviceIndex;
  return 1;
}
void audioInputPause(int isPaused) { (void)isPaused; }
int audioInputRefreshDevices(void) { return 1; }

void audioInputCleanup(void) {}
