#ifndef __CHIPNOMAD_LIB_H__
#define __CHIPNOMAD_LIB_H__

#include "chips/chips.h"
#include "project.h"
#include "playback.h"
#include "synth/track_tilt.h"
#include "utils.h"

class BraidsVoice;
class SampleVoice;
class SCWFVoice;
class PlaitsVoice;
class PlaitsAltVoice;
class AChChidVoice;
class AudioCommandQueue;

constexpr int VOICE_MONITOR_SAMPLES = 256;

struct VoiceMonitor {
  float samples[VOICE_MONITOR_SAMPLES];
  float envelope;
  uint8_t active;
};

struct MotionRecordEvent {
  uint16_t phrase;
  uint8_t row;
  uint8_t fx;
  uint8_t value;
  uint8_t erase;
};

// UI-facing copy of the audio state.  The callback never exposes its mutable
// PlaybackState to the UI.
struct PlaybackStatus {
  PlaybackTrackState tracks[PROJECT_MAX_TRACKS];
  uint8_t trackEnabled[PROJECT_MAX_TRACKS];
  uint8_t isPlaying;
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
  // The UI is the sole writer of project. The audio callback only reads
  // audioProject after a tick-boundary snapshot handoff.
  Project project;
  int ownsProjectResources;
  // UI owns project; the audio callback reads only audioProject.
  Project audioProject;
  PlaybackState playbackState;
  SoundChip* chips[PROJECT_MAX_CHIPS];
  int sampleRate;
  float frameSampleCounter;
  float mixVolume;
  int audioOverload;
  int trackClipping[PROJECT_MAX_TRACKS];
  int trackWarnings[PROJECT_MAX_TRACKS];
  TrackTilt trackTilt[PROJECT_MAX_TRACKS];
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
  AChChidVoice* achchidVoices[PROJECT_MAX_TRACKS];
  VoiceMonitor voiceMonitors[PROJECT_MAX_TRACKS];
  MasterEffects* masterEffects;
  AudioCommandQueue* audioCommands;
  PlaybackStatus uiPlaybackStatus;
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

// Reserve render buffers before starting the audio device. Rendering never
// grows these buffers, so this must be called again after reconfiguration.
int chipnomadReserveRenderBuffers(ChipNomadState* state, int frames);
int chipnomadQueueTrackEnabled(ChipNomadState* state, const uint8_t enabled[PROJECT_MAX_TRACKS]);
int chipnomadQueueProjectRefresh(ChipNomadState* state);
void chipnomadQueuePlaybackStop(ChipNomadState* state);
int chipnomadQueuePlaybackStartSong(ChipNomadState* state, int songRow, int chainRow, int loop);
int chipnomadQueuePlaybackStartChain(ChipNomadState* state, int trackIdx, int songRow, int chainRow, int loop);
int chipnomadQueuePlaybackStartPhrase(ChipNomadState* state, int trackIdx, int songRow, int chainRow, int loop);
int chipnomadQueuePlaybackStartPhraseRow(ChipNomadState* state, int trackIdx, const PhraseRow* row);
int chipnomadQueuePlaybackQueuePhrase(ChipNomadState* state, int trackIdx, int songRow, int chainRow);
int chipnomadQueuePlaybackPreviewNote(ChipNomadState* state, int trackIdx, uint8_t note, uint8_t instrument);
int chipnomadQueuePlaybackStopPreview(ChipNomadState* state, int trackIdx);
int chipnomadQueuePlaybackClearTrackFX(ChipNomadState* state, int trackIdx);
void chipnomadQueueLoopRange(ChipNomadState* state, LoopRange range);
void chipnomadQueueClearLoopRange(ChipNomadState* state);
const PlaybackStatus* chipnomadGetPlaybackStatus(ChipNomadState* state);
int chipnomadGetCommandOverflow(ChipNomadState* state);
int chipnomadGetRenderBufferOverflow(ChipNomadState* state);
void chipnomadSetRenderBufferOverflow(ChipNomadState* state);

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

void chipnomadSetLiveStickAxes(float leftVertical, float leftHorizontal,
                               float rightVertical, float rightHorizontal);
void chipnomadSetLiveStickEnabled(int enabled);
void chipnomadSetMotionRecordMode(int record, int erase);
int chipnomadConsumeMotionRecordEvent(MotionRecordEvent* event);
int chipnomadGetMotionRecordOverflow(void);
unsigned int chipnomadGetMotionRecordDroppedCount(void);
void chipnomadSetMotionRecordOverflow(void);

#endif
