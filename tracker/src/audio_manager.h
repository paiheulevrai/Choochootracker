#ifndef __AUDIOMANAGER_H__
#define __AUDIOMANAGER_H__

#include "common.h"
#include "chipnomad_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void FrameCallback(void* userdata);
typedef int ProjectMutation(void* userdata);

struct AudioManager {
  int (*start)(int sampleRate, int audioBufferSize);
  int (*reconfigure)(int sampleRate, int audioBufferSize);
  // Reopens the output device at the current rate/size (device lost, driver
  // reset). Buffers and engine state are preserved; returns 0 on success.
  int (*restart)(void);
  void (*pause)(void);
  void (*resume)(void);
  void (*replaceProject)(Project* replacement);
  void (*reinitializeChips)(void);
  void (*ensureInstrumentVoices)(InstrumentType type);
  int (*mutateProject)(ProjectMutation* mutation, void* userdata);
  void (*stop)();
  void (*toggleTrackMute)(int trackIdx);
  void (*toggleTrackSolo)(int trackIdx);
  int (*getCpuLoadPercent)(void);
  int (*previewSample)(const char* path);
  int (*previewAYSample)(const char* path, const InstrumentAYSample* settings);
  int (*previewSCWF)(const char* path, const InstrumentSCWF* settings, int oscillator);
  int (*previewBYOWTBL)(const char* path, const InstrumentBYOWTBL* settings, int oscillator);
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
