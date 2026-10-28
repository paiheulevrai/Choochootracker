#include "corelib_audio.h"
#include "chipnomad_lib.h"
#include <cstring>
#include <string>
#include <vector>
int mockAudioOutputDeviceCount = 0;
char mockSelectedAudioOutput[128] = {};
std::vector<std::string> mockAudioOutputNames;
int audioOutputDeviceCount(void) {
  return mockAudioOutputNames.empty() ? mockAudioOutputDeviceCount : (int)mockAudioOutputNames.size();
}
int audioOutputDeviceName(int index, char* buffer, int size) {
  if (index < 0 || index >= audioOutputDeviceCount() || !buffer || size <= 0) return 1;
  const char* name = mockAudioOutputNames.empty() ? "ROCCAT Juke" : mockAudioOutputNames[index].c_str();
  if (!*name) return 1; // Device disappeared during enumeration.
  std::strncpy(buffer, name, size - 1);
  buffer[size - 1] = 0;
  return 0;
}
void audioOutputSelect(const char* name) {
  std::strncpy(mockSelectedAudioOutput, name ? name : "", sizeof(mockSelectedAudioOutput) - 1);
  mockSelectedAudioOutput[sizeof(mockSelectedAudioOutput) - 1] = 0;
}

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
