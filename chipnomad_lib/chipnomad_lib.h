#ifndef __CHIPNOMAD_LIB_H__
#define __CHIPNOMAD_LIB_H__

#include "chips/chips.h"
#include "project.h"
#include "playback.h"
#include "synth/track_tilt.h"
#include "synth/instrument_eq.h"
#include "utils.h"
#include "stop_transition.h"

class BraidsVoice;
class SampleVoice;
class SoundbankVoice;
class SCWFVoice;
class PlaitsVoice;
class PlaitsAltVoice;
class AChChidVoice;
class DrumSynthVoice;
class AudioInputVoice;
class MMEVoice;
class MonkSynthVoice;
class SinteredVoice;
class PDVoice;
class OPLLVoice;
class SIDVoice;
class DX7Voice;
class DX7Part;
class FourOpVoice;
class OPLVoice;
class SimpleChipVoice;
class AudioCommandQueue;
struct PreparedInstrumentVoices;
struct MidiRouterState;
struct LinkClientState;
class AudioMonitor;

constexpr int VOICE_OWNER_COUNT = PROJECT_MAX_VOICE_OWNERS;

constexpr int VOICE_MONITOR_SAMPLES = 256;

struct VoiceMonitor {
  float samples[VOICE_MONITOR_SAMPLES];
  float envelope;
  uint8_t active;
};

struct MotionRecordEvent {
  uint16_t phrase;
  uint8_t row;
  FXId fx;
  uint8_t value;
  uint8_t erase;
};

// UI-facing copy of the audio state.  The callback never exposes its mutable
// PlaybackState to the UI.
struct PlaybackLayerStatus {
  uint8_t instrument;
  uint8_t liveInstrument;
  uint8_t liveControls[liveControlCount];
};

struct PlaybackStatus {
  PlaybackTrackState tracks[PROJECT_MAX_TRACKS];
  PlaybackLayerStatus layerTracks[PROJECT_MAX_TRACKS][INSTRUMENT_LAYER_COUNT];
  uint8_t trackEnabled[PROJECT_MAX_TRACKS];
  uint8_t isPlaying;
  // Audio-thread tick counter and the host timestamp of that tick. They are
  // published with the status snapshot so the UI can follow Link phase
  // without racing the audio callback.
  uint64_t linkPhaseTicks;
  uint64_t linkPhaseTickMicros;
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
  // Dedicated AY instances for Layer lanes. Normal owners remain null.
  SoundChip* layerAYChips[VOICE_OWNER_COUNT];
  int sampleRate;
  float frameSampleCounter;
  StopTransition stopTransition;
  float mixVolume;
  int audioOverload;
  int trackClipping[PROJECT_MAX_TRACKS];
  int trackWarnings[PROJECT_MAX_TRACKS];
  TrackTilt trackTilt[PROJECT_MAX_TRACKS];
  // The EQ of the instrument each track is playing, run on the track's insert buffer.
  InstrumentEQFilter trackEQ[PROJECT_MAX_TRACKS];
  uint8_t trackEQActive[PROJECT_MAX_TRACKS];
  InstrumentEQFilter layerEQ[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  uint8_t layerEQActive[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  InstrumentEQFilter masterEQ;
  uint8_t masterEQActive;
  InsertChain* insertChains[PROJECT_MAX_TRACKS];
  InsertChain* masterInsertChain;
  InsertAutomation masterInsertAutomation;
  uint32_t insertResetSeen[PROJECT_MAX_TRACKS];
  uint8_t insertValues[PROJECT_MAX_TRACKS][2][8];
  uint8_t insertActive[PROJECT_MAX_TRACKS];
  uint8_t audioInputRoutedTrackMask;
  float* insertBuffer;
  float* audioInputTrackCurrent;
  float* audioInputTrackPrevious;
  int audioInputTrackPreviousFrames;
  float* sidechainBuffer;
  uint8_t sidechainSourceMask, masterSidechainExcludeMask;
  float* mixBuffer;
  float* reverbBuffer;
  float* delayBuffer;
  int mixBufferSize;
  int aySampleDithering;
  BraidsVoice* braidsVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  uint8_t braidsBits, braidsDrift, braidsSignature;
  uint32_t braidsSignatureSeed;
  SoundbankVoice* soundbankVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  AudioInputVoice* audioInputVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  SampleVoice* sampleVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  SCWFVoice* scwfVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  PlaitsVoice* plaitsVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  PlaitsAltVoice* plaitsAltVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  AChChidVoice* achchidVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  DrumSynthVoice* drumSynthVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  MMEVoice* mmeVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  MonkSynthVoice* monkPreview;
  MonkSynthVoice* monkVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  OPLVoice* oplVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  OPLVoice* oplPreview;
  FourOpVoice* fourOpPreview;
  FourOpVoice* fourOpVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  SimpleChipVoice* simpleChipVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  SimpleChipVoice* simpleChipPreview;
  InstrumentType chipPreviewType;
  uint8_t nativePreviewReady;
  DX7Part* dx7Parts[VOICE_OWNER_COUNT];
  DX7Voice* dx7Voices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  DX7Part* dx7Preview;
  SIDVoice* sidPreview;
  SIDVoice* sidVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  OPLLVoice* opllPreview;
  int opllPreviewTrack;
  OPLLVoice* opllVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  SinteredVoice* sinteredVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  PDVoice* pdVoices[VOICE_OWNER_COUNT][CHORD_MAX_VOICES];
  // MIDI-Out active-note tracking, Program/Bank cache, and MIDI-In routing
  // state now live behind the generic MIDI router (see midi/midi_router.h) -
  // one instance per ChipNomadState, so independent engine states (e.g. one
  // per test fixture) never share bookkeeping through a hidden global.
  MidiRouterState* midiRouter;
  // Receiver-only Ableton Link client (see link/link_client.h) - same
  // one-instance-per-state ownership as the MIDI router. The UI polls it
  // with serial-bumped getters; the audio thread captures session state
  // through it at tick boundaries.
  LinkClientState* linkClient;
  VoiceMonitor voiceMonitors[VOICE_OWNER_COUNT];
  MasterEffects* masterEffects;
  AudioCommandQueue* audioCommands;
  AudioMonitor* audioMonitor;
  int benchmarkBypassPan;
  PlaybackStatus uiPlaybackStatus;
  // Audio-thread-only state, copied into PlaybackStatus each tick.
  uint64_t linkPhaseTicks;
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
bool chipnomadCanSwitchInstrumentLive(InstrumentType type);
PreparedInstrumentVoices* chipnomadPrepareInstrumentVoices(ChipNomadState* state, InstrumentType type);
// Install a prepared pool while the audio callback is locked. Takes ownership.
void chipnomadInstallInstrumentVoices(ChipNomadState* state, PreparedInstrumentVoices* prepared);

// Reserve render buffers before starting the audio device. Rendering never
// grows these buffers, so this must be called again after reconfiguration.
int chipnomadReserveRenderBuffers(ChipNomadState* state, int frames);
// Configure persistent workers for real-time rendering while audio is stopped.
// Returns the number of workers actually enabled; 0 is the serial fallback.
int chipnomadQueueTrackEnabled(ChipNomadState* state, const uint8_t enabled[PROJECT_MAX_TRACKS]);
// Offline only, with the audio device paused.
void chipnomadCollectSoundbanks(ChipNomadState* state);
int chipnomadQueueProjectRefresh(ChipNomadState* state);
int chipnomadQueuePlaybackScale(ChipNomadState* state, uint8_t root, ScalePreset preset);
void chipnomadDiscardQueuedProject(ChipNomadState* state);
void chipnomadQueuePlaybackStop(ChipNomadState* state);
// Sends Note Off for every currently active MIDI Out note, plus a blanket
// CC123 (All Notes Off) / CC120 (All Sound Off) on every channel as a final
// fallback. Call before closing/switching the MIDI output port and on
// application shutdown, in addition to the automatic call on playback stop.
void chipnomadMidiPanic(ChipNomadState* state);
int chipnomadQueuePlaybackStartSong(ChipNomadState* state, int songRow, int chainRow, int loop);
// Audio-thread conditional start. Unlike checking a UI playback snapshot,
// this remains correct when a MIDI Stop and the following Start cross the
// UI/audio thread boundary in the same render block.
int chipnomadQueuePlaybackStartSongIfStopped(ChipNomadState* state, int songRow, int chainRow, int loop);
// Timed start (T40): playback begins on the first audio tick frame at or
// after waitUntilMicros on the monotonic host clock (same scale as
// midiRouterNowMicros / linkClientNowMicros). waitUntilMicros == 0 fires
// immediately. Used by the Ableton Link quantized launch to align song
// start to a session beat boundary.
int chipnomadQueuePlaybackStartSongAtTime(ChipNomadState* state, int songRow, int chainRow, int loop, uint64_t waitUntilMicros);
int chipnomadQueuePlaybackStartChain(ChipNomadState* state, int trackIdx, int songRow, int chainRow, int loop, uint64_t waitUntilMicros = 0);
int chipnomadQueuePlaybackStartPhrase(ChipNomadState* state, int trackIdx, int songRow, int chainRow, int loop, uint64_t waitUntilMicros = 0);
int chipnomadQueuePlaybackStartPhraseRow(ChipNomadState* state, int trackIdx, const PhraseRow* row);
int chipnomadQueuePlaybackStartPhraseRowFull(ChipNomadState* state, int trackIdx, const PhraseRow* row);
int chipnomadQueuePlaybackQueuePhrase(ChipNomadState* state, int trackIdx, int songRow, int chainRow);
int chipnomadQueuePlaybackStartLiveChain(ChipNomadState* state, int trackIdx, int songRow);
int chipnomadQueuePlaybackQueueLiveChain(ChipNomadState* state, int trackIdx, int songRow, int urgent);
int chipnomadQueuePlaybackPreviewNote(ChipNomadState* state, int trackIdx, uint8_t note, uint8_t instrument);
// Copies the bounded native patch into the existing audio command queue.
int chipnomadQueueSimpleChipPreview(ChipNomadState* state,int track,InstrumentType type,const InstrumentSimpleChip* patch);
int chipnomadQueueFourOpPreview(ChipNomadState* state,int track,InstrumentType type,const InstrumentFourOp* patch);
int chipnomadQueueOPLPreview(ChipNomadState* state, int track, InstrumentType type, const InstrumentOPL* patch);
int chipnomadQueueDX7Preview(ChipNomadState* state,int track,const InstrumentDX7* patch);
int chipnomadQueueMonkPreview(ChipNomadState* state, int track, const InstrumentMonk* patch);
int chipnomadQueueSIDPreview(ChipNomadState* state,int track,const InstrumentSID* patch);
// Native preview starts lazily initialize DSP on the calling thread. Hosts
// must hold their audio lock (or stop rendering) when submitting a patch.
int chipnomadQueueOPLLPreview(ChipNomadState* state, int track, const InstrumentOPLL* patch);
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
// Optional live stereo input has exactly samples frames at the output rate.
// Omitted for offline export: Audio in then receives silence.
int chipnomadRender(ChipNomadState* state, float* buffer, int samples, const float* liveInput = nullptr);

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
