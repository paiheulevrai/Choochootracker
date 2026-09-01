#ifndef __AUDIOMANAGER_H__
#define __AUDIOMANAGER_H__

#include "common.h"
#include "chipnomad_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void FrameCallback(void* userdata);

struct AudioManager {
  int (*start)(int sampleRate, int audioBufferSize);
  void (*pause)(void);
  void (*resume)(void);
  void (*replaceProject)(Project* replacement);
  void (*reinitializeChips)(void);
  void (*stop)();
  void (*toggleTrackMute)(int trackIdx);
  void (*toggleTrackSolo)(int trackIdx);
  int (*getCpuLoadPercent)(void);
  int (*previewSample)(const char* path);
  void (*stopSamplePreview)(void);
  uint8_t trackStates[PROJECT_MAX_TRACKS];
};

// Singleton AudioManager struct
extern AudioManager audioManager;

// Track state constants
#define TRACK_NORMAL 0
#define TRACK_SOLO 1
#define TRACK_MUTED 2


#ifdef __cplusplus
}
#endif

#endif
