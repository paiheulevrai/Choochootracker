#ifndef __CHIPNOMAD_LIB_H__
#define __CHIPNOMAD_LIB_H__

#include "chips/chips.h"
#include "project.h"
#include "playback.h"
#include "utils.h"

class BraidsVoice;
class SampleVoice;
class SCWFVoice;
class PlaitsVoice;
class PlaitsAltVoice;

struct VoiceMonitor {
  float samples[64];
  float envelope;
  uint8_t active;
};
class MasterEffects;

#define AUDIO_OVERLOAD_COOLDOWN_FRAMES 5
#define PITCH_CONFLICT_COOLDOWN_FRAMES 5

/**
* Chip factory function type
* Returns a SoundChip pointer for the given chip index
*/
typedef SoundChip* (*ChipFactory)(int chipIndex, int sampleRate, ChipSetup setup);

/**
* ChipNomad state encapsulating all library state
*/
struct ChipNomadState {
  Project project;
  PlaybackState playbackState;
  SoundChip* chips[PROJECT_MAX_CHIPS];
  int sampleRate;
  float frameSampleCounter;
  float mixVolume;
  int audioOverload;
  int trackClipping[PROJECT_MAX_TRACKS];
  int trackWarnings[PROJECT_MAX_TRACKS];
  float* mixBuffer;
  float* reverbBuffer;
  float* delayBuffer;
  int mixBufferSize;
  int aySampleDithering;
  BraidsVoice* braidsVoices[PROJECT_MAX_TRACKS];
  SampleVoice* sampleVoices[PROJECT_MAX_TRACKS];
  SCWFVoice* scwfVoices[PROJECT_MAX_TRACKS];
  PlaitsVoice* plaitsVoices[PROJECT_MAX_TRACKS];
  PlaitsAltVoice* plaitsAltVoices[PROJECT_MAX_TRACKS];
  VoiceMonitor voiceMonitors[PROJECT_MAX_TRACKS];
  MasterEffects* masterEffects;
};

/**
* Create and initialize ChipNomad state
* @return Pointer to initialized state, or NULL on failure
*/
ChipNomadState* chipnomadCreate(void);

/**
* Destroy ChipNomad state and cleanup resources
* @param state State to destroy
*/
void chipnomadDestroy(ChipNomadState* state);

/**
* Initialize chips with project settings
* @param state ChipNomad state
* @param sampleRate Audio sample rate
* @param factory Chip factory function, or NULL to use default implementations
*/
void chipnomadInitChips(ChipNomadState* state, int sampleRate, ChipFactory factory);

/**
* Render audio with automatic tick rate handling
* @param state ChipNomad state
* @param buffer Interleaved stereo float buffer (left, right, left, right...)
* @param samples Number of stereo sample pairs to render
* @return Number of samples actually rendered (may be less if playback stops)
*/
int chipnomadRender(ChipNomadState* state, float* buffer, int samples);

// Fast offline level balancing over a short render. Returns 0 on success.
int chipnomadAutoMix(ChipNomadState* state, int seconds, uint8_t proposed[PROJECT_MAX_TRACKS]);

/**
* Set emulation quality for all chips
* @param state ChipNomad state
* @param quality Quality level
*/
void chipnomadSetQuality(ChipNomadState* state, ChipNomadQuality quality);
void chipnomadSetBraidsSettings(ChipNomadState* state, uint8_t bits,
                               uint8_t drift, uint8_t signature,
                               uint32_t signatureSeed);

#endif
