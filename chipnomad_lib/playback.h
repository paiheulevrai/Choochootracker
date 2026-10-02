#ifndef __CHIPNOMAD_LIB__PLAYBACK_H__
#define __CHIPNOMAD_LIB__PLAYBACK_H__

#include "project.h"
#include "chips/chips.h"
#include "playback_fx.h"
#include "playback_chips.h"
#include "playback_modulation.h"
#include "chord.h"

struct ChipNomadState;

enum class PlaybackMode {
  none, // For queue
  stopped,
  song,
  chain,
  phrase,
  phraseRow,
  loop,
  live,
};

enum class LiveQueueAction : uint8_t {
  none,
  normal,
  urgent,
  stopNormal,
  stopUrgent,
};

struct PlaybackTableState {
  uint8_t tableIdx;
  uint8_t baseSpeed;
  uint8_t rows[4];
  uint8_t counters[4];
  uint8_t speed[4];
  uint8_t fxAuxState[16][4]; // Used for stateful effects like HOP
};

struct PlaybackNoteState {
  uint8_t pitchBase;
  uint8_t instrument;
  uint8_t volume;
  uint8_t noteTriggered;
  uint8_t noteReleased;
  uint8_t noteKilled;
  uint8_t accent;

  uint8_t pitchFinal; // Calculated pitch value
  int8_t pitchOffset; // Pitch offset (semitones)
  int16_t fineOffset; // Fine pitch offset (cents or periods, depending on linearPitch setting)
  int16_t periodOffset; // Period offset
  uint8_t volume1; // Instrument volume
  uint8_t volume2; // Instrument table volume
  uint8_t volume3; // Aux table volume
  int8_t volumeOffset; // Volume offset

  PlaybackTableState instrumentTable;
  PlaybackTableState auxTable;
  PlaybackFXState fx[256]; // Active FX on this note, indexed by FX enum

  PlaybackModState modulation[4]; // Modulation states

  PlaybackChipNoteState chip;
};

struct PlaybackTrackQueue {
  PlaybackMode mode;
  int songRow;
  int chainRow;
  int phraseRow;
  int loop;
  LiveQueueAction liveAction;
};

struct PlaybackTrackState {
  PlaybackTrackQueue queue;

  PlaybackMode mode;
  // Position in the song
  int songRow;
  int chainRow;
  int phraseRow;
  int loop;

  // Groove
  uint8_t grooveIdx;
  int grooveRow;
  uint8_t pendingGrooveIdx; // For GGR synchronization

  int frameCounter;

  // Persistent sequencer FX state
  uint8_t speedRatio;
  uint32_t speedPhase;
  uint8_t slewTicks;
  uint8_t achchidGateTicks;
  uint8_t achchidGateCounter;
  int16_t slewCurrent[fxTotalCount];
  int16_t slewTarget[fxTotalCount];
  uint8_t slewRemaining[fxTotalCount];
  uint32_t conditionVisits[16];
  uint32_t conditionRandom;
  uint16_t conditionPhrase;

  // Currently playing note
  PlaybackNoteState note;
  uint8_t chordVoiceCount;
  uint8_t chordPitchBase[CHORD_MAX_VOICES];
  uint8_t chordPitchFinal[CHORD_MAX_VOICES];
  // Cached phrase row data
  PhraseRow currentPhraseRow;
  // FX auxillary state data for the phrase (used by HOP)
  uint8_t fxAuxState[16][3];

  // One-shot signal for a MIDI Out instrument's MC1-MC4 row FX (see
  // playback_fx_midi.cpp and chipnomad_lib.cpp's applyVoiceEvents): set when
  // that FX is freshly read from a row, consumed and cleared by the audio
  // engine, which is the only layer that knows about real MIDI I/O.
  uint8_t midiCCPending[4];
  uint8_t midiCCValue[4];
};

struct PlaybackAYChipState {
  uint8_t envShape;
};

union PlaybackChipState {
  PlaybackAYChipState ay;
};

struct LoopRange {
  int enabled;
  int level; // 0 = song, 1 = chain, 2 = phrase
  int startSongRow;
  int startChainRow;
  int startPhraseRow;
  int endSongRow;
  int endChainRow;
  int endPhraseRow;
};

// Stop boundary for offline rendering (bounce/export of a selection).
// Unlike LoopRange, reaching the end STOPS the track (resetTrack) instead of
// wrapping back to the start. Checked at the same three checkpoints in
// moveToNextPhraseRow. Not gated by track->loop. SNG/HOP commands that would
// jump outside the range also stop the track.
struct StopRange {
  int enabled;
  int level; // 0 = song, 1 = chain, 2 = phrase
  int startSongRow;
  int startChainRow;
  int startPhraseRow;
  int endSongRow;
  int endChainRow;
  int endPhraseRow;
};

struct PlaybackState {
  Project* p;
  PlaybackTrackState tracks[PROJECT_MAX_TRACKS];
  PlaybackChipState chips[PROJECT_MAX_CHIPS];
  uint8_t trackEnabled[PROJECT_MAX_TRACKS];
  LoopRange loopRange;
  StopRange stopRange;
  float liveStickAxes[4];
  int16_t liveStickRate[PROJECT_MAX_INSTRUMENTS][4];
  uint8_t liveStickWasPlaying;
  uint8_t scaleRoot;
  ScalePreset scalePreset;
  uint8_t scaleFXCommandSeen;
};

// FX typedefs
typedef void (*PlaybackFXInitFunc)(
  PlaybackState* state,
  PlaybackTrackState* track,
  int trackIdx,
  PlaybackFXState* fx,
  PlaybackTableState* tableState,
  int tableFXColumn
);
typedef void (*PlaybackFXRestartFunc)(
  PlaybackState* state,
  PlaybackTrackState* track,
  int trackIdx,
  PlaybackFXState* fx
);
typedef void (*PlaybackFXHandleFunc)(
  PlaybackState* state,
  PlaybackTrackState* track,
  int trackIdx,
  int chipIdx,
  PlaybackFXState* fx
);

struct PlaybackFXHandler {
  PlaybackFXInitFunc init;
  PlaybackFXHandleFunc handle;
  PlaybackFXRestartFunc restart;
};

/**
 * Initializes the playback state with the given project
 *
 * @param state Pointer to the playback state to initialize
 * @param project Pointer to the project data to use for playback
 */
void playbackInit(PlaybackState* state, Project* project);

/**
 * Checks if any track is currently playing
 *
 * @param state Pointer to the playback state
 * @return 1 if any track is playing, 0 if all tracks are stopped
 */
int playbackIsPlaying(PlaybackState* state);

/**
 * Starts song playback from the specified position
 *
 * @param state Pointer to the playback state
 * @param songRow Starting row position in the song
 * @param chainRow Starting row position in the chain
 * @param loop Whether to loop when reaching the end
 */
void playbackStartSong(PlaybackState* state, int songRow, int chainRow, int loop);

/**
 * Starts chain playback for a specific track
 *
 * @param state Pointer to the playback state
 * @param trackIdx Index of the track to play
 * @param songRow Row position in the song containing the chain
 * @param chainRow Starting row position in the chain
 * @param loop Whether to loop when reaching the end
 */
void playbackStartChain(PlaybackState* state, int trackIdx, int songRow, int chainRow, int loop);

/**
 * Starts phrase playback for a specific track
 *
 * @param state Pointer to the playback state
 * @param trackIdx Index of the track to play
 * @param songRow Row position in the song containing the phrase
 * @param chainRow Row position in the chain containing the phrase
 * @param loop Whether to loop when reaching the end
 * @param startPhraseRow Phrase row to start from (0 for the top)
 */
void playbackStartPhrase(PlaybackState* state, int trackIdx, int songRow, int chainRow, int loop, int startPhraseRow = 0);

/**
 * Starts playback of a phrase row
 *
 * @param state Pointer to the playback state
 * @param trackIdx Index of the track to play
 * @param phraseRow Phrase row data to play
 */
void playbackStartPhraseRow(PlaybackState* state, int trackIdx, PhraseRow* phraseRow);

/**
 * Queues a phrase for playback on a specific track
 * Only works if the track is currently in phrase playback mode
 *
 * @param state Pointer to the playback state
 * @param trackIdx Index of the track to queue the phrase on
 * @param songRow Row position in the song containing the phrase
 * @param chainRow Row position in the chain containing the phrase
 */
void playbackQueuePhrase(PlaybackState* state, int trackIdx, int songRow, int chainRow);
void playbackStartLiveChain(PlaybackState* state, int trackIdx, int songRow);
void playbackQueueLiveChain(PlaybackState* state, int trackIdx, int songRow, int urgent);

// Phrase volume, VOL effects, and the active instrument/aux table volume columns.
float playbackVolumeGain(const PlaybackState* state, const PlaybackTrackState* track);

/**
 * Stops playback on all tracks
 *
 * @param state Pointer to the playback state
 */
void playbackStop(PlaybackState* state);

/**
 * Plays a single note with an instrument for preview
 *
 * @param state Pointer to the playback state
 * @param trackIdx Index of the track to use for preview
 * @param note Note value to play
 * @param instrument Instrument to use
 */
void playbackPreviewNote(PlaybackState* state, int trackIdx, uint8_t note, uint8_t instrument);

/**
 * Stops preview playback on a specific track
 *
 * @param state Pointer to the playback state
 * @param trackIdx Index of the track to stop preview on
 */
void playbackStopPreview(PlaybackState* state, int trackIdx);

/**
 * Sets a loop range for playback
 *
 * @param state Pointer to the playback state
 * @param range Loop range configuration
 */
void playbackSetLoopRange(PlaybackState* state, LoopRange range);

/**
 * Sets a stop boundary for offline rendering (bounce). When a playing track
 * reaches the end of the range it is stopped (notes killed) instead of
 * looping or continuing.
 *
 * @param state Pointer to the playback state
 * @param range Stop range configuration
 */
void playbackSetStopRange(PlaybackState* state, StopRange range);

/**
 * Clears the loop range, disabling ranged loop
 *
 * @param state Pointer to the playback state
 */
void playbackClearLoopRange(PlaybackState* state);

/**
 * Advances playback by one frame
 *
 * @param state Pointer to the ChipNomad state
 * @return 1 if all tracks have finished playing, 0 if any track is still active
 */
int playbackNextFrame(struct ChipNomadState* state);
void playbackUpdateLiveStickModulation(PlaybackState* state, const float axes[4], int enabled);
int16_t playbackLiveStickOutput(const PlaybackState* state, uint8_t instrument,
                                int slot, const Modulation* mod);

#endif // __CHIPNOMAD_LIB__PLAYBACK_H__
