#include "chipnomad_lib.h"
#include "chipnomad_lib_live_stick.h"
#include "playback.h"
#include "synth/braids_voice.h"
#include "synth/sample_voice.h"
#include "synth/scwf_voice.h"
#undef LUT_FM_FREQUENCY_QUANTIZER
#undef LUT_FM_FREQUENCY_QUANTIZER_SIZE
#include "synth/plaits_voice.h"
#include "synth/plaits_alt_voice.h"
#include "synth/achchid_voice.h"
#include "synth/master_effects.h"
#include <math.h>
#include <atomic>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static void detectAYPitchConflicts(ChipNomadState* state);
static void updateBraidsVoices(ChipNomadState* state);
static void updateSampleVoices(ChipNomadState* state);
static void updateSCWFVoices(ChipNomadState* state);
static void updatePlaitsVoices(ChipNomadState* state);
static void updatePlaitsAltVoices(ChipNomadState* state);
static void updateAChChidVoices(ChipNomadState* state);
static void applyVoiceEvents(ChipNomadState* state);
static int hasAudioRateModulation(const ChipNomadState* state);
static void updateAudioRateModulations(ChipNomadState* state);
static void motionRecordFrame(ChipNomadState* state);
static int instrumentFXCutoff(uint8_t value);

class AudioCommandQueue {
 public:
  void requestStop() { stopRequested_.store(1, std::memory_order_release); }

  int takeStopRequest() { return stopRequested_.exchange(0, std::memory_order_acq_rel); }

  int pushProject(const Project& project) {
    return publishProject(project);
  }

  void discardProject() {
    int old = projectPublished_.exchange(-1, std::memory_order_acq_rel);
    if (old >= 0) releasePublished(projectSlots_, old);
  }

  void applyProject(Project* project) {
    int slot = claim(projectSlots_, projectPublished_);
    if (slot < 0) return;
    *project = projectSlots_[slot].value;
    projectSlots_[slot].state.store(kFree, std::memory_order_release);
  }

  int pushTrackEnabled(const uint8_t enabled[PROJECT_MAX_TRACKS]) {
    uint64_t mask = 0;
    for (int i = 0; i < PROJECT_MAX_TRACKS; ++i)
      if (enabled[i]) mask |= UINT64_C(1) << i;
    settingsDraft_.trackMask = mask;
    return publishSettings();
  }

  void pushLoopRange(LoopRange range) {
    settingsDraft_.loopRange = range;
    settingsDraft_.loopDirty = 1;
    publishSettings();
  }

  void clearLoopRange() {
    memset(&settingsDraft_.loopRange, 0, sizeof(settingsDraft_.loopRange));
    settingsDraft_.loopDirty = 1;
    publishSettings();
  }

  int pushCommand(uint8_t type, int a = 0, int b = 0, int c = 0, int d = 0,
                  const PhraseRow* row = NULL) {
    unsigned int head = commandHead_.load(std::memory_order_relaxed);
    unsigned int next = (head + 1) % kCommandCapacity;
    if (next == commandTail_.load(std::memory_order_acquire)) {
      commandOverflow_.fetch_add(1, std::memory_order_relaxed);
      return 0;
    }
    AudioCommand& command = commands_[head];
    command.type = type; command.a = a; command.b = b; command.c = c; command.d = d;
    if (row) command.row = *row;
    commandHead_.store(next, std::memory_order_release);
    return 1;
  }

  void applySettings(PlaybackState* playback) {
    int slot = claim(settingsSlots_, settingsPublished_);
    if (slot < 0) return;
    const Settings& settings = settingsSlots_[slot].value;
    {
      uint64_t mask = settings.trackMask;
      for (int i = 0; i < PROJECT_MAX_TRACKS; ++i)
        playback->trackEnabled[i] = (mask >> i) & 1;
    }
    if (settings.loopDirty) {
      if (settings.loopRange.enabled) playbackSetLoopRange(playback, settings.loopRange);
      else playbackClearLoopRange(playback);
    }
    settingsSlots_[slot].state.store(kFree, std::memory_order_release);
  }

  void applyCommands(PlaybackState* playback) {
    unsigned int tail = commandTail_.load(std::memory_order_relaxed);
    unsigned int head = commandHead_.load(std::memory_order_acquire);
    while (tail != head) {
      const AudioCommand& command = commands_[tail];
      switch (command.type) {
        case kStartSong: playbackStartSong(playback, command.a, command.b, command.c); break;
        case kStartChain: playbackStartChain(playback, command.a, command.b, command.c, command.d); break;
        case kStartPhrase: playbackStartPhrase(playback, command.a, command.b, command.c, command.d); break;
        case kStartPhraseRow: playbackStartPhraseRow(playback, command.a, const_cast<PhraseRow*>(&command.row)); break;
        case kQueuePhrase: playbackQueuePhrase(playback, command.a, command.b, command.c); break;
        case kPreviewNote: playbackPreviewNote(playback, command.a, (uint8_t)command.b, (uint8_t)command.c); break;
        case kStopPreview: playbackStopPreview(playback, command.a); break;
        case kClearTrackFX: memset(playback->tracks[command.a].note.fx, 0, sizeof(playback->tracks[command.a].note.fx)); break;
      }
      tail = (tail + 1) % kCommandCapacity;
    }
    commandTail_.store(tail, std::memory_order_release);
  }

  void publishStatus(const PlaybackState* playback) {
    int old = statusPublished_.exchange(-1, std::memory_order_acq_rel);
    if (old >= 0) releasePublished(statusSlots_, old);
    int slot = findFree(statusSlots_);
    if (slot < 0) return;
    PlaybackStatus& status = statusSlots_[slot].value;
    memcpy(status.tracks, playback->tracks, sizeof(status.tracks));
    memcpy(status.trackEnabled, playback->trackEnabled, sizeof(status.trackEnabled));
    status.isPlaying = playbackIsPlaying(const_cast<PlaybackState*>(playback));
    statusSlots_[slot].state.store(kPublished, std::memory_order_release);
    statusPublished_.store(slot, std::memory_order_release);
  }

  int readStatus(PlaybackStatus* status) {
    int slot = claim(statusSlots_, statusPublished_);
    if (slot < 0) return 0;
    *status = statusSlots_[slot].value;
    statusSlots_[slot].state.store(kFree, std::memory_order_release);
    return 1;
  }

  int commandOverflow() const { return commandOverflow_.load(std::memory_order_relaxed) != 0; }
  void setRenderBufferOverflow() { renderBufferOverflow_.store(1, std::memory_order_relaxed); }
  int renderBufferOverflow() const { return renderBufferOverflow_.load(std::memory_order_relaxed); }

 private:
  enum { kFree, kPublished, kReading };
  template <typename T> struct Slot { T value; std::atomic<int> state{kFree}; };
  struct Settings { uint64_t trackMask = ~UINT64_C(0); LoopRange loopRange{}; uint8_t loopDirty = 0; };
  struct AudioCommand { uint8_t type; int a, b, c, d; PhraseRow row; };
  enum CommandType { kStartSong, kStartChain, kStartPhrase, kStartPhraseRow, kQueuePhrase, kPreviewNote, kStopPreview, kClearTrackFX };
  static constexpr unsigned int kSlotCount = 3;
  static constexpr unsigned int kCommandCapacity = 64;

  template <typename T> static int findFree(Slot<T> slots[kSlotCount]) {
    for (unsigned int i = 0; i < kSlotCount; ++i) {
      int expected = kFree;
      if (slots[i].state.compare_exchange_strong(expected, kReading, std::memory_order_acq_rel)) return (int)i;
    }
    return -1;
  }
  template <typename T> static void releasePublished(Slot<T> slots[kSlotCount], int slot) {
    int expected = kPublished;
    slots[slot].state.compare_exchange_strong(expected, kFree, std::memory_order_acq_rel);
  }
  template <typename T> static int claim(Slot<T> slots[kSlotCount], std::atomic<int>& published) {
    int slot = published.load(std::memory_order_acquire);
    while (slot >= 0) {
      int expected = kPublished;
      if (slots[slot].state.compare_exchange_strong(expected, kReading, std::memory_order_acq_rel)) {
        int publishedSlot = slot;
        published.compare_exchange_strong(publishedSlot, -1, std::memory_order_acq_rel);
        return slot;
      }
      slot = published.load(std::memory_order_acquire);
    }
    return -1;
  }
  int publishProject(const Project& project) {
    int old = projectPublished_.exchange(-1, std::memory_order_acq_rel);
    if (old >= 0) releasePublished(projectSlots_, old);
    int slot = findFree(projectSlots_);
    if (slot < 0) return 0;
    projectSlots_[slot].value = project;
    projectSlots_[slot].state.store(kPublished, std::memory_order_release);
    projectPublished_.store(slot, std::memory_order_release);
    return 1;
  }
  int publishSettings() {
    int old = settingsPublished_.exchange(-1, std::memory_order_acq_rel);
    if (old >= 0) releasePublished(settingsSlots_, old);
    int slot = findFree(settingsSlots_);
    if (slot < 0) return 0;
    settingsSlots_[slot].value = settingsDraft_;
    settingsSlots_[slot].state.store(kPublished, std::memory_order_release);
    settingsPublished_.store(slot, std::memory_order_release);
    return 1;
  }
  Slot<Project> projectSlots_[kSlotCount];
  Slot<Settings> settingsSlots_[kSlotCount];
  Slot<PlaybackStatus> statusSlots_[kSlotCount];
  Settings settingsDraft_;
  std::atomic<int> stopRequested_{0};
  std::atomic<int> projectPublished_{-1};
  std::atomic<int> settingsPublished_{-1};
  std::atomic<int> statusPublished_{-1};
  AudioCommand commands_[kCommandCapacity] = {};
  std::atomic<unsigned int> commandHead_{0};
  std::atomic<unsigned int> commandTail_{0};
  std::atomic<int> commandOverflow_{0};
  std::atomic<int> renderBufferOverflow_{0};
};

static int16_t motionRecordLast[PROJECT_MAX_TRACKS][fxTotalCount];
static int motionRecordLastMode = -1;

static int slewEngineFX(PlaybackTrackState* track, FX fx, int target) {
  int index = (int)fx;
  if (track->slewTarget[index] < 0) {
    track->slewCurrent[index] = target;
    track->slewTarget[index] = target;
  } else if (track->slewTarget[index] != target) {
    track->slewTarget[index] = target;
    track->slewRemaining[index] = track->slewTicks;
  }
  if (!track->slewTicks) {
    track->slewCurrent[index] = target;
    track->slewRemaining[index] = 0;
  } else if (track->slewRemaining[index]) {
    int distance = target - track->slewCurrent[index];
    int step = distance / track->slewRemaining[index];
    if (!step && distance) step = distance < 0 ? -1 : 1;
    track->slewCurrent[index] += step;
    track->slewRemaining[index]--;
  }
  return track->slewCurrent[index];
}


static void resetMotionRecordLast(void) {
  for (int track = 0; track < PROJECT_MAX_TRACKS; ++track)
    for (int fx = 0; fx < fxTotalCount; ++fx)
      motionRecordLast[track][fx] = -1;
}

static int motionDestinationFX(const Instrument* instrument, const PlaybackTrackState* track,
                               int destination, FX* fx, int* base, int* range,
                               InstrumentMotionValue* value) {
  uint8_t rawFX;
  if (!instrumentMotionDestination(instrument, destination, &rawFX, base, range, value)) return 0;
  *fx = (FX)rawFX;
  if (track->note.fx[*fx].isOn)
    *base = *value == InstrumentMotionValue::cutoff
      ? (int)instrumentFXCutoff(track->note.fx[*fx].fxValue)
      : track->note.fx[*fx].fxValue;
  return 1;
}

static int motionFXValue(int value, InstrumentMotionValue kind) {
  if (kind == InstrumentMotionValue::speed) return clampInt(value, 0, 500) * 255 / 500;
  if (kind == InstrumentMotionValue::cutoff) return filterControlFromCutoff((float)clampInt(value, 20, 20000));
  return clampInt(value, 0, 255);
}

static int motionPhraseLocation(ChipNomadState* state, int trackIdx, uint16_t* phrase, uint8_t* row) {
  PlaybackTrackState* track = &state->playbackState.tracks[trackIdx];
  Project* project = &state->audioProject;
  if (track->mode == PlaybackMode::stopped || track->mode == PlaybackMode::phraseRow ||
      track->songRow < 0 || track->songRow >= PROJECT_MAX_LENGTH ||
      track->chainRow < 0 || track->chainRow >= PROJECT_MAX_LENGTH ||
      track->phraseRow < 0 || track->phraseRow >= 16) return 0;
  uint16_t chain = project->song[track->songRow][trackIdx];
  if (chain == EMPTY_VALUE_16 || chain >= PROJECT_MAX_CHAINS) return 0;
  *phrase = project->chains[chain].rows[track->chainRow].phrase;
  if (*phrase == EMPTY_VALUE_16 || *phrase >= PROJECT_MAX_PHRASES) return 0;
  *row = (uint8_t)track->phraseRow;
  return 1;
}

static void rebaseMotionRecordRate(ChipNomadState* state) {
  for (int trackIdx = 0; trackIdx < state->audioProject.tracksCount; ++trackIdx) {
    PlaybackTrackState* track = &state->playbackState.tracks[trackIdx];
    if (track->note.instrument == EMPTY_VALUE_8) continue;
    Instrument* instrument = &state->audioProject.instruments[track->note.instrument];
    for (int slot = 0; slot < 4; ++slot) {
      PlaybackModState* modulation = &track->note.modulation[slot];
      if (!modulation->modulation || modulation->modulation->type != ModulationType::StickRate) continue;
      FX fx;
      int base, range; InstrumentMotionValue value;
      if (!motionDestinationFX(instrument, track, modulation->modulation->destination, &fx, &base, &range, &value)) continue;
      int delta = playbackModScaleToRange(modulation->outValue, range);
      if (range == 16384) delta /= 129;
      track->note.fx[fx].isOn = 1;
      track->note.fx[fx].fxValue = motionFXValue(base + delta, value);
      state->playbackState.liveStickRate[track->note.instrument][slot] = 0;
      modulation->outValue = 0;
    }
  }
}

static void motionRecordFrame(ChipNomadState* state) {
  if (chipnomadMotionTakeRateReset())
    rebaseMotionRecordRate(state);
  int mode = chipnomadMotionMode();
  if (mode != motionRecordLastMode) {
    resetMotionRecordLast();
    motionRecordLastMode = mode;
  }
  if (!mode) return;

  for (int trackIdx = 0; trackIdx < state->audioProject.tracksCount; ++trackIdx) {
    PlaybackTrackState* track = &state->playbackState.tracks[trackIdx];
    if (track->note.instrument == EMPTY_VALUE_8) continue;
    uint16_t phrase;
    uint8_t row;
    if (!motionPhraseLocation(state, trackIdx, &phrase, &row)) continue;
    Instrument* instrument = &state->audioProject.instruments[track->note.instrument];

    FX targets[4];
    int values[4];
    InstrumentMotionValue valueKinds[4];
    int targetCount = 0;
    for (int slot = 0; slot < 4; ++slot) {
      PlaybackModState* modulation = &track->note.modulation[slot];
      if (!modulation->modulation || !modulationIsLiveStick(modulation->modulation->type)) continue;
      FX fx;
      int base, range; InstrumentMotionValue value;
      if (!motionDestinationFX(instrument, track, modulation->modulation->destination, &fx, &base, &range, &value)) continue;
      int target = -1;
      for (int i = 0; i < targetCount; ++i) if (targets[i] == fx) target = i;
      int delta = playbackModScaleToRange(modulation->outValue, range);
      if (range == 16384) delta = delta / 129;
      if (target < 0) {
        target = targetCount++;
        targets[target] = fx;
        values[target] = base;
        valueKinds[target] = value;
      }
      values[target] += delta;
    }

    for (int target = 0; target < targetCount; ++target) {
      FX fx = targets[target];
      int fxValue = motionFXValue(values[target], valueKinds[target]);
      if (mode == 1 && motionRecordLast[trackIdx][fx] == fxValue) continue;
      MotionRecordEvent event = {phrase, row, (uint8_t)fx, (uint8_t)fxValue, (uint8_t)(mode == 2)};
      if (!chipnomadMotionPushEvent(event)) {
        chipnomadMotionSetOverflow();
        continue;
      }
      motionRecordLast[trackIdx][fx] = (int16_t)fxValue;
    }
  }
}

static void captureVoiceMonitor(ChipNomadState* state, int trackIdx,
                                const float* samples, int frames,
                                int channels, float envelope) {
  VoiceMonitor* monitor = &state->voiceMonitors[trackIdx];
  for (int i = 0; i < VOICE_MONITOR_SAMPLES; ++i) {
    int frame = frames > 1 ? (i * (frames - 1)) / (VOICE_MONITOR_SAMPLES - 1) : 0;
    monitor->samples[i] = samples[frame * channels];
  }
  monitor->envelope = envelope;
  monitor->active = 1;
}

static int resizeMixBuffers(ChipNomadState* state, int requiredSize) {
  float* mixBuffer = (float*)malloc(requiredSize * sizeof(float));
  float* reverbBuffer = (float*)malloc(requiredSize * sizeof(float));
  float* delayBuffer = (float*)malloc(requiredSize * sizeof(float));
  if (!mixBuffer || !reverbBuffer || !delayBuffer) {
    free(mixBuffer);
    free(reverbBuffer);
    free(delayBuffer);
    return 0;
  }

  free(state->mixBuffer);
  free(state->reverbBuffer);
  free(state->delayBuffer);
  state->mixBuffer = mixBuffer;
  state->reverbBuffer = reverbBuffer;
  state->delayBuffer = delayBuffer;
  state->mixBufferSize = requiredSize;
  return 1;
}

static int instrumentFXCutoff(uint8_t value) {
  return (int)filterCutoffFromControl(value);
}

static float mixerGain(uint8_t value) {
  if (value == 0) return 0.0f;
  return powf(10.0f, -60.0f * (100 - value) / 2000.0f);
}

static float phraseGain(const PlaybackTrackState* track, const Instrument* instrument) {
  return instrument->volume * track->note.volume / (255.0f * 15.0f);
}

static float effectiveTrackSend(ChipNomadState* state, int trackIdx,
                                bool reverb) {
  PlaybackTrackState* track = &state->playbackState.tracks[trackIdx];
  int value = reverb ? state->audioProject.trackReverbSend[trackIdx]
                     : state->audioProject.trackDelaySend[trackIdx];
  if (track->note.fx[reverb ? fxRSN : fxDSN].isOn)
    value = track->note.fx[reverb ? fxRSN : fxDSN].fxValue * 100 / 255;
  if (track->note.instrument != EMPTY_VALUE_8) {
    InstrumentType type = state->audioProject.instruments[track->note.instrument].type;
    for (int i = 0; i < 4; ++i) {
      PlaybackModState* mod = &track->note.modulation[i];
      if (!mod->modulation) continue;
      int destination = instrumentGenericModDestination(type,
        mod->modulation->destination);
      if (destination != (reverb ? genericModReverbSend : genericModDelaySend)) continue;
      int modulation = playbackModScaleToRange(mod->outValue, 100);
      value = (modulationIsAdditive(mod->modulation->type) ||
               mod->modulation->type == ModulationType::SLFO ||
               mod->modulation->type == ModulationType::FLFO)
        ? value + modulation : modulation;
    }
  }
  value = clampInt(value, 0, 100);
  return state->audioProject.perceptualEffects ? mixerGain((uint8_t)value) : value / 100.0f;
}

static inline void mixTrackSample(ChipNomadState* state, int trackIdx,
                                  float* mix, float* reverb, float* delay,
                                  float sample, int channel, float reverbSend,
                                  float delaySend) {
  sample = state->trackTilt[trackIdx].process(sample, channel,
    state->audioProject.trackTilt[trackIdx], state->audioProject.tiltPivotHz);
  float previous = *mix;
  *mix += sample;
  *reverb += sample * reverbSend;
  *delay += sample * delaySend;
  if (fabsf(*mix) > 1.0f && fabsf(*mix) > fabsf(previous)) {
    state->trackClipping[trackIdx] = AUDIO_OVERLOAD_COOLDOWN_FRAMES;
  }
}

static SoundChip* defaultChipFactory(int chipIndex, int sampleRate, ChipSetup setup) {
  setup.ay.stereoSeparation = 0;
  return new SoundChipAY(sampleRate, setup);
}

ChipNomadState* chipnomadCreate(void) {
  ChipNomadState* state = (ChipNomadState*)malloc(sizeof(ChipNomadState));
  if (!state) return NULL;

  memset(state, 0, sizeof(ChipNomadState));
  state->ownsProjectResources = 1;
  state->audioCommands = new AudioCommandQueue();
  fillFXNames();
  projectInit(&state->project);
  state->audioProject = state->project;
  playbackInit(&state->playbackState, &state->audioProject);
  state->mixVolume = 1.0f;
  state->aySampleDithering = 1; // Default: ON

  // Initialize mix buffer
  state->mixBufferSize = 8192;
  state->mixBuffer = (float*)malloc(state->mixBufferSize * sizeof(float));
  state->reverbBuffer = (float*)malloc(state->mixBufferSize * sizeof(float));
  state->delayBuffer = (float*)malloc(state->mixBufferSize * sizeof(float));
  if (!state->mixBuffer || !state->reverbBuffer || !state->delayBuffer) {
    free(state->mixBuffer);
    free(state->reverbBuffer);
    free(state->delayBuffer);
    free(state);
    return NULL;
  }

  state->masterEffects = new MasterEffects();
  state->masterEffects->init(96000.0f);

  for (int i = 0; i < PROJECT_MAX_TRACKS; i++) {
    state->braidsVoices[i] = new BraidsVoice();
    state->braidsVoices[i]->init();
    state->sampleVoices[i] = new SampleVoice();
    state->sampleVoices[i]->init(96000.0f);
    state->scwfVoices[i] = new SCWFVoice();
    state->scwfVoices[i]->init(96000.0f);
    state->plaitsVoices[i] = new PlaitsVoice();
    state->plaitsVoices[i]->init(96000.0f);
    state->plaitsAltVoices[i] = new PlaitsAltVoice();
    state->plaitsAltVoices[i]->init(96000.0f);
    state->achchidVoices[i] = new AChChidVoice();
    state->achchidVoices[i]->init(96000.0f);
  }

  return state;
}

void chipnomadDestroy(ChipNomadState* state) {
  if (!state) return;

  // Cleanup chips
  for (int i = 0; i < PROJECT_MAX_CHIPS; i++) {
    if (state->chips[i]) {
      delete state->chips[i];
      state->chips[i] = nullptr;
    }
  }

  for (int i = 0; i < PROJECT_MAX_TRACKS; i++) {
    delete state->braidsVoices[i];
    delete state->sampleVoices[i];
    delete state->scwfVoices[i];
    delete state->plaitsVoices[i];
    delete state->plaitsAltVoices[i];
    delete state->achchidVoices[i];
  }

  if (state->ownsProjectResources) projectFree(&state->project);

  // Cleanup mix buffer
  free(state->mixBuffer);
  free(state->reverbBuffer);
  free(state->delayBuffer);
  delete state->masterEffects;
  delete state->audioCommands;

  free(state);
}

void chipnomadInitChips(ChipNomadState* state, int sampleRate, ChipFactory factory) {
  if (!state) return;
  state->audioProject = state->project;
  state->playbackState.p = &state->audioProject;

  // Cleanup existing chips if already initialized
  if (state->sampleRate > 0) {
    for (int i = 0; i < PROJECT_MAX_CHIPS; i++) {
      if (state->chips[i]) {
        delete state->chips[i];
        state->chips[i] = nullptr;
      }
    }
  }

  // Zero the entire chips array for safety
  memset(state->chips, 0, sizeof(state->chips));
  state->sampleRate = sampleRate;
  state->masterEffects->init((float)sampleRate);
  for (int i = 0; i < PROJECT_MAX_TRACKS; i++) {
    state->trackTilt[i].init((float)sampleRate);
    state->sampleVoices[i]->init((float)sampleRate);
    state->scwfVoices[i]->init((float)sampleRate);
    state->plaitsVoices[i]->init((float)sampleRate);
    state->plaitsAltVoices[i]->init((float)sampleRate);
  }

  // Use provided factory or default
  ChipFactory chipFactory = factory ? factory : defaultChipFactory;

  // Initialize chips based on project's chipsCount
  for (int i = 0; i < state->audioProject.chipsCount; i++) {
    state->chips[i] = chipFactory(i, sampleRate, state->audioProject.chipSetup);
  }
}

static int hasAudioRateModulation(const ChipNomadState* state) {
  for (int trackIdx = 0; trackIdx < state->audioProject.tracksCount; ++trackIdx) {
    const PlaybackTrackState* track = &state->playbackState.tracks[trackIdx];
    for (int i = 0; i < 4; ++i) {
      const PlaybackModState* mod = &track->note.modulation[i];
      if (mod->modulation && mod->modulation->type == ModulationType::FLFO &&
          mod->modulation->destination != 0) return 1;
    }
  }
  return 0;
}

int chipnomadReserveRenderBuffers(ChipNomadState* state, int frames) {
  if (!state || frames <= 0 || frames > INT_MAX / 2) return 1;
  int requiredSize = frames * 2;
  return requiredSize <= state->mixBufferSize || resizeMixBuffers(state, requiredSize) ? 0 : 1;
}

int chipnomadQueueTrackEnabled(ChipNomadState* state, const uint8_t enabled[PROJECT_MAX_TRACKS]) {
  return state && state->audioCommands && enabled ? state->audioCommands->pushTrackEnabled(enabled) : 0;
}

int chipnomadQueueProjectRefresh(ChipNomadState* state) {
  return state && state->audioCommands ? state->audioCommands->pushProject(state->project) : 0;
}

void chipnomadDiscardQueuedProject(ChipNomadState* state) {
  if (state && state->audioCommands) state->audioCommands->discardProject();
}

void chipnomadQueuePlaybackStop(ChipNomadState* state) {
  if (state && state->audioCommands) state->audioCommands->requestStop();
}

int chipnomadQueuePlaybackStartSong(ChipNomadState* state, int songRow, int chainRow, int loop) {
  return state && state->audioCommands ? state->audioCommands->pushCommand(0, songRow, chainRow, loop) : 0;
}
int chipnomadQueuePlaybackStartChain(ChipNomadState* state, int trackIdx, int songRow, int chainRow, int loop) {
  return state && state->audioCommands ? state->audioCommands->pushCommand(1, trackIdx, songRow, chainRow, loop) : 0;
}
int chipnomadQueuePlaybackStartPhrase(ChipNomadState* state, int trackIdx, int songRow, int chainRow, int loop) {
  return state && state->audioCommands ? state->audioCommands->pushCommand(2, trackIdx, songRow, chainRow, loop) : 0;
}
int chipnomadQueuePlaybackStartPhraseRow(ChipNomadState* state, int trackIdx, const PhraseRow* row) {
  return state && state->audioCommands && row ? state->audioCommands->pushCommand(3, trackIdx, 0, 0, 0, row) : 0;
}
int chipnomadQueuePlaybackQueuePhrase(ChipNomadState* state, int trackIdx, int songRow, int chainRow) {
  return state && state->audioCommands ? state->audioCommands->pushCommand(4, trackIdx, songRow, chainRow) : 0;
}
int chipnomadQueuePlaybackPreviewNote(ChipNomadState* state, int trackIdx, uint8_t note, uint8_t instrument) {
  return state && state->audioCommands ? state->audioCommands->pushCommand(5, trackIdx, note, instrument) : 0;
}
int chipnomadQueuePlaybackStopPreview(ChipNomadState* state, int trackIdx) {
  return state && state->audioCommands ? state->audioCommands->pushCommand(6, trackIdx) : 0;
}
int chipnomadQueuePlaybackClearTrackFX(ChipNomadState* state, int trackIdx) {
  return state && state->audioCommands ? state->audioCommands->pushCommand(7, trackIdx) : 0;
}
void chipnomadQueueLoopRange(ChipNomadState* state, LoopRange range) {
  if (state && state->audioCommands) state->audioCommands->pushLoopRange(range);
}
void chipnomadQueueClearLoopRange(ChipNomadState* state) {
  if (state && state->audioCommands) state->audioCommands->clearLoopRange();
}
const PlaybackStatus* chipnomadGetPlaybackStatus(ChipNomadState* state) {
  if (!state) return NULL;
  state->audioCommands->readStatus(&state->uiPlaybackStatus);
  return &state->uiPlaybackStatus;
}
int chipnomadGetCommandOverflow(ChipNomadState* state) {
  return state && state->audioCommands ? state->audioCommands->commandOverflow() : 0;
}
int chipnomadGetRenderBufferOverflow(ChipNomadState* state) {
  return state && state->audioCommands ? state->audioCommands->renderBufferOverflow() : 0;
}
void chipnomadSetRenderBufferOverflow(ChipNomadState* state) {
  if (state && state->audioCommands) state->audioCommands->setRenderBufferOverflow();
}

static void applyVoicePostModulations(const PlaybackTrackState* track, InstrumentType type,
                                      int* attack, int* decay, int* sustain, int* release,
                                      int* shape, int* triggerDecay, int* triggerColor) {
  int* values[] = {attack, decay, sustain, release, shape, triggerDecay, triggerColor};
  for (int i = 0; i < 4; ++i) {
    const PlaybackModState* mod = &track->note.modulation[i];
    if (!mod->modulation) continue;
    int destination = instrumentGenericModDestination(type, mod->modulation->destination);
    if (destination < genericModEnvelopeAttack || destination >= genericModTotalCount) continue;
    int index = destination - genericModEnvelopeAttack;
    int value = playbackModScaleToRange(mod->outValue, 255);
    *values[index] = modulationIsAdditive(mod->modulation->type) ? *values[index] + value : value;
  }
  for (int i = 0; i < 7; ++i) *values[i] = clampInt(*values[i], 0, 255);
}

static void updateAudioRateModulations(ChipNomadState* state) {
  for (int trackIdx = 0; trackIdx < state->audioProject.tracksCount; ++trackIdx) {
    PlaybackTrackState* track = &state->playbackState.tracks[trackIdx];
    for (int i = 0; i < 4; ++i)
      playbackModNextAudio(&track->note.modulation[i], (float)state->sampleRate);
  }
  updateSampleVoices(state);
  updateSCWFVoices(state);
  updateBraidsVoices(state);
  updatePlaitsVoices(state);
  updatePlaitsAltVoices(state);
  updateAChChidVoices(state);
}

static int advancePlaybackFrame(ChipNomadState* state) {
  state->audioCommands->applyProject(&state->audioProject);
  state->playbackState.p = &state->audioProject;
  if (state->audioCommands->takeStopRequest()) playbackStop(&state->playbackState);
  state->audioCommands->applySettings(&state->playbackState);
  state->audioCommands->applyCommands(&state->playbackState);
  float axes[4];
  for (int i = 0; i < 4; ++i) axes[i] = chipnomadLiveStickAxis(i);
  int enabled = chipnomadLiveStickIsEnabled();
  if (!enabled && chipnomadMotionRateResetPending()) enabled = 1;
  playbackUpdateLiveStickModulation(&state->playbackState, axes, enabled);
  state->frameSampleCounter += state->sampleRate / state->audioProject.tickRate;
  int allTracksStopped = playbackNextFrame(state);
  motionRecordFrame(state);
  if (allTracksStopped) playbackUpdateLiveStickModulation(&state->playbackState, axes, enabled);
  updateSampleVoices(state); updateSCWFVoices(state); updateBraidsVoices(state);
  updatePlaitsVoices(state); updatePlaitsAltVoices(state); updateAChChidVoices(state); applyVoiceEvents(state);
  if (state->audioOverload > 0) state->audioOverload--;
  for (int i = 0; i < PROJECT_MAX_TRACKS; ++i)
    if (state->trackClipping[i] > 0) state->trackClipping[i]--;
  detectAYPitchConflicts(state);
  state->audioCommands->publishStatus(&state->playbackState);
  return allTracksStopped;
}

static int prepareRenderChunk(ChipNomadState* state, float* output, int frames) {
  for (int i = 0; i < state->audioProject.tracksCount; ++i) state->voiceMonitors[i].active = 0;
  int requiredSize = frames * 2;
  if (requiredSize > state->mixBufferSize) {
    state->audioCommands->setRenderBufferOverflow();
    return 0;
  }
  memset(output, 0, requiredSize * sizeof(float));
  memset(state->reverbBuffer, 0, requiredSize * sizeof(float));
  memset(state->delayBuffer, 0, requiredSize * sizeof(float));
  return 1;
}

static void renderChipTracks(ChipNomadState* state, float* output, int frames) {
  for (int chipIdx = 0; chipIdx < state->audioProject.chipsCount; ++chipIdx) {
    if (chipIdx >= state->audioProject.tracksCount || !state->playbackState.trackEnabled[chipIdx]) continue;
    uint8_t instrumentIdx = state->playbackState.tracks[chipIdx].note.instrument;
    if (instrumentIdx == EMPTY_VALUE_8) continue;
    InstrumentType type = state->audioProject.instruments[instrumentIdx].type;
    if (type != InstrumentType::AY1 && type != InstrumentType::AY2 && type != InstrumentType::AYSample) continue;
    SoundChip* chip = state->chips[chipIdx];
    if (!chip) continue;
    chip->render(state->mixBuffer, frames);
    float gain = state->audioProject.trackVolume[chipIdx] / 100.0f * state->audioProject.instruments[instrumentIdx].volume / 255.0f;
    float reverbSend = effectiveTrackSend(state, chipIdx, true);
    float delaySend = effectiveTrackSend(state, chipIdx, false);
    for (int i = 0; i < frames * 2; ++i)
      mixTrackSample(state, chipIdx, &output[i], &state->reverbBuffer[i], &state->delayBuffer[i],
                     state->mixBuffer[i] * gain, i & 1, reverbSend, delaySend);
  }
}

template <typename Voice>
static void renderMonoVoiceTracks(ChipNomadState* state, Voice* const voices[], float* output, int frames) {
  for (int trackIdx = 0; trackIdx < state->audioProject.tracksCount; ++trackIdx) {
    if (!state->playbackState.trackEnabled[trackIdx] || !voices[trackIdx]->active()) continue;
    Voice* voice = voices[trackIdx];
    voice->render(state->mixBuffer, frames);
    captureVoiceMonitor(state, trackIdx, state->mixBuffer, frames, 1, voice->envelopeLevel());
    float trackGain = state->audioProject.trackVolume[trackIdx] / 100.0f;
    float reverbSend = effectiveTrackSend(state, trackIdx, true);
    float delaySend = effectiveTrackSend(state, trackIdx, false);
    for (int i = 0; i < frames; ++i) {
      float sample = state->mixBuffer[i] * 0.25f * trackGain;
      mixTrackSample(state, trackIdx, &output[i * 2], &state->reverbBuffer[i * 2],
                     &state->delayBuffer[i * 2], sample, 0, reverbSend, delaySend);
      mixTrackSample(state, trackIdx, &output[i * 2 + 1], &state->reverbBuffer[i * 2 + 1],
                     &state->delayBuffer[i * 2 + 1], sample, 1, reverbSend, delaySend);
    }
  }
}

template <typename Voice>
static void renderStereoVoiceTracks(ChipNomadState* state, Voice* const voices[], float* output, int frames) {
  for (int trackIdx = 0; trackIdx < state->audioProject.tracksCount; ++trackIdx) {
    if (!state->playbackState.trackEnabled[trackIdx] || !voices[trackIdx]->active()) continue;
    Voice* voice = voices[trackIdx];
    voice->render(state->mixBuffer, frames);
    captureVoiceMonitor(state, trackIdx, state->mixBuffer, frames, 2, voice->envelopeLevel());
    float gain = state->audioProject.trackVolume[trackIdx] / 100.0f;
    float reverbSend = effectiveTrackSend(state, trackIdx, true);
    float delaySend = effectiveTrackSend(state, trackIdx, false);
    for (int i = 0; i < frames * 2; ++i)
      mixTrackSample(state, trackIdx, &output[i], &state->reverbBuffer[i], &state->delayBuffer[i],
                     state->mixBuffer[i] * gain, i & 1, reverbSend, delaySend);
  }
}

static void processMasterMix(ChipNomadState* state, float* output, int frames) {
  bool hasReverb = false, hasDelay = false;
  for (int i = 0; i < state->audioProject.tracksCount; ++i) {
    hasReverb |= effectiveTrackSend(state, i, true) > 0.0f || state->audioProject.delayReverbSend > 0;
    hasDelay |= effectiveTrackSend(state, i, false) > 0.0f;
  }
  state->masterEffects->process(hasReverb ? state->reverbBuffer : NULL,
                                hasDelay ? state->delayBuffer : NULL, output, frames, &state->audioProject);
  for (int i = 0; i < frames * 2; ++i) {
    output[i] *= state->mixVolume;
    if (output[i] > 1.0f || output[i] < -1.0f) state->audioOverload = AUDIO_OVERLOAD_COOLDOWN_FRAMES;
  }
}

int chipnomadRender(ChipNomadState* state, float* buffer, int samples) {
  if (!state || !buffer || samples <= 0 || samples > INT_MAX / 2) return 0;
  int samplesLeft = samples;
  while (samplesLeft > 0) {
    if ((int)state->frameSampleCounter == 0 && advancePlaybackFrame(state)) break;
    int frames = (int)state->frameSampleCounter < samplesLeft ? (int)state->frameSampleCounter : samplesLeft;
    if (hasAudioRateModulation(state)) { updateAudioRateModulations(state); frames = 1; }
    float* output = buffer + (samples - samplesLeft) * 2;
    if (!prepareRenderChunk(state, output, frames)) return 0;
    renderChipTracks(state, output, frames);
    renderMonoVoiceTracks(state, state->braidsVoices, output, frames);
    renderStereoVoiceTracks(state, state->sampleVoices, output, frames);
    renderStereoVoiceTracks(state, state->scwfVoices, output, frames);
    renderMonoVoiceTracks(state, state->plaitsVoices, output, frames);
    renderMonoVoiceTracks(state, state->plaitsAltVoices, output, frames);
    renderMonoVoiceTracks(state, state->achchidVoices, output, frames);
    processMasterMix(state, output, frames);
    samplesLeft -= frames;
    state->frameSampleCounter -= (float)frames;
  }
  if (samplesLeft > 0) memset(buffer + (samples - samplesLeft) * 2, 0, samplesLeft * 2 * sizeof(float));
  return samples - samplesLeft;
}

static float envelopeTime(uint8_t value) {
  float normalized = value / 255.0f;
  return normalized * normalized * 5.0f;
}

enum { AUTO_MIX_BANDS = 8 };

static float autoMixRender(ChipNomadState* state, int seconds, float* rms,
                           float bandRms[AUTO_MIX_BANDS]) {
  const int frames = 1024;
  float buffer[frames * 2];
  double energy = 0.0;
  double bandEnergy[AUTO_MIX_BANDS] = {};
  float lowPass[AUTO_MIX_BANDS - 1] = {};
  float coefficients[AUTO_MIX_BANDS - 1];
  for (int band = 0; band < AUTO_MIX_BANDS - 1; ++band) {
    float cutoff = 80.0f * (1 << band);
    coefficients[band] = 1.0f - expf(-2.0f * 3.141592653589793f * cutoff / state->sampleRate);
  }
  int rendered = 0;
  float peak = 0.0f;
  playbackStartSong(&state->playbackState, 0, 0, 0);
  while (rendered < seconds * state->sampleRate) {
    int count = chipnomadRender(state, buffer, frames);
    if (count <= 0) break;
    for (int i = 0; i < count * 2; ++i) {
      float value = buffer[i];
      energy += value * value;
      if (fabsf(value) > peak) peak = fabsf(value);
    }
    for (int frame = 0; frame < count; ++frame) {
      float value = (buffer[frame * 2] + buffer[frame * 2 + 1]) * 0.5f;
      float previous = value;
      for (int band = 0; band < AUTO_MIX_BANDS - 1; ++band) {
        lowPass[band] += coefficients[band] * (value - lowPass[band]);
        float filtered = lowPass[band] - (band ? lowPass[band - 1] : 0.0f);
        bandEnergy[band] += filtered * filtered;
        previous = lowPass[band];
      }
      float high = value - previous;
      bandEnergy[AUTO_MIX_BANDS - 1] += high * high;
    }
    rendered += count;
  }
  if (rms) *rms = rendered ? sqrtf((float)(energy / (rendered * 2))) : 0.0f;
  if (bandRms) {
    for (int band = 0; band < AUTO_MIX_BANDS; ++band)
      bandRms[band] = rendered ? sqrtf((float)(bandEnergy[band] / rendered)) : 0.0f;
  }
  return peak;
}

int chipnomadAutoMix(ChipNomadState* state, int seconds, uint8_t proposed[PROJECT_MAX_TRACKS]) {
  if (!state || seconds < 1) return 1;
  ChipNomadState* analysis = chipnomadCreate();
  if (!analysis) return 1;
  analysis->project = state->project; // Sample buffers are read-only during rendering.
  analysis->project.reverbReturn = analysis->project.delayReturn = 0;
  analysis->project.tracksCount = state->project.tracksCount;
  playbackInit(&analysis->playbackState, &analysis->project);
  chipnomadInitChips(analysis, 48000, NULL);

  float rms[PROJECT_MAX_TRACKS] = {};
  float trackBands[PROJECT_MAX_TRACKS][AUTO_MIX_BANDS] = {};
  float sumLog = 0.0f;
  int active = 0;
  for (int track = 0; track < analysis->project.tracksCount; ++track) {
    for (int i = 0; i < PROJECT_MAX_TRACKS; ++i) analysis->playbackState.trackEnabled[i] = i == track;
    autoMixRender(analysis, seconds, &rms[track], trackBands[track]);
    if (rms[track] > 0.0001f) { sumLog += logf(rms[track]); active++; }
  }
  if (!active) { chipnomadDestroy(analysis); return 1; }
  float target = expf(sumLog / active);
  float trackGain[PROJECT_MAX_TRACKS] = {};
  for (int i = 0; i < analysis->project.tracksCount; ++i) {
    if (rms[i] <= 0.0001f) continue;
    float ratio = sqrtf(target / rms[i]); // Keep musical differences while correcting extremes.
    ratio = fminf(2.0f, fmaxf(0.25f, ratio));
    analysis->project.trackVolume[i] = (uint8_t)fminf(100.0f,
      state->project.trackVolume[i] * ratio + 0.5f);
    trackGain[i] = analysis->project.trackVolume[i] /
      (float)state->project.trackVolume[i];
  }
  for (int i = 0; i < PROJECT_MAX_TRACKS; ++i) analysis->playbackState.trackEnabled[i] = 1;
  float mixBands[AUTO_MIX_BANDS] = {};
  autoMixRender(analysis, seconds, NULL, mixBands);

  // Pink noise has comparable energy in each octave. Attenuate only tracks
  // that materially contribute to octaves above the mix's geometric mean.
  float sumLogBands = 0.0f;
  int activeBands = 0;
  for (int band = 0; band < AUTO_MIX_BANDS; ++band) {
    if (mixBands[band] > 0.0001f) { sumLogBands += logf(mixBands[band]); activeBands++; }
  }
  if (activeBands) {
    float pinkTarget = expf(sumLogBands / activeBands);
    for (int track = 0; track < analysis->project.tracksCount; ++track) {
      if (!trackGain[track]) continue;
      float masking = 0.0f;
      for (int band = 0; band < AUTO_MIX_BANDS; ++band) {
        if (mixBands[band] <= pinkTarget) continue;
        float contribution = trackBands[track][band] * trackGain[track] / mixBands[band];
        masking += contribution * contribution * logf(mixBands[band] / pinkTarget);
      }
      float correction = fmaxf(0.85f, expf(-0.5f * masking));
      float volume = analysis->project.trackVolume[track] * correction;
      volume = fmaxf(state->project.trackVolume[track] * 0.25f,
        fminf(state->project.trackVolume[track] * 2.0f, volume));
      analysis->project.trackVolume[track] = (uint8_t)fminf(100.0f, volume + 0.5f);
    }
  }

  float peak = autoMixRender(analysis, seconds, NULL, NULL);
  float safety = peak > 0.89f ? 0.89f / peak : 1.0f;
  for (int i = 0; i < state->project.tracksCount; ++i) {
    proposed[i] = (uint8_t)(analysis->project.trackVolume[i] * safety + 0.5f);
  }
  chipnomadDestroy(analysis);
  return 0;
}

static void applyVoiceEvents(ChipNomadState* state) {
  PlaybackState* playback = &state->playbackState;
  Project* project = &state->audioProject;
  for (int trackIdx = 0; trackIdx < project->tracksCount; ++trackIdx) {
    PlaybackTrackState* track = &playback->tracks[trackIdx];
    if (!track->note.noteTriggered && !track->note.noteReleased && !track->note.noteKilled) continue;
    if (track->note.instrument == EMPTY_VALUE_8) continue;
    switch (project->instruments[track->note.instrument].type) {
      case InstrumentType::Sample:
        if (track->note.noteKilled) state->sampleVoices[trackIdx]->kill();
        else if (track->note.noteTriggered) state->sampleVoices[trackIdx]->noteOn();
        else state->sampleVoices[trackIdx]->noteOff();
        break;
      case InstrumentType::SCWF:
      case InstrumentType::BYOWTBL:
        if (track->note.noteKilled) state->scwfVoices[trackIdx]->kill();
        else if (track->note.noteTriggered) state->scwfVoices[trackIdx]->noteOn();
        else state->scwfVoices[trackIdx]->noteOff();
        break;
      case InstrumentType::Braids:
        if (track->note.noteKilled) state->braidsVoices[trackIdx]->kill();
        else if (track->note.noteTriggered) state->braidsVoices[trackIdx]->noteOn();
        else state->braidsVoices[trackIdx]->noteOff();
        break;
      case InstrumentType::Plaits:
        if (track->note.noteKilled) state->plaitsVoices[trackIdx]->kill();
        else if (track->note.noteTriggered) state->plaitsVoices[trackIdx]->noteOn();
        else state->plaitsVoices[trackIdx]->noteOff();
        break;
      case InstrumentType::PlaitsAlt:
        if (track->note.noteKilled) state->plaitsAltVoices[trackIdx]->kill();
        else if (track->note.noteTriggered) state->plaitsAltVoices[trackIdx]->noteOn();
        else state->plaitsAltVoices[trackIdx]->noteOff();
        break;
      case InstrumentType::AChChid:
        if (track->note.noteKilled) state->achchidVoices[trackIdx]->kill();
        else if (track->note.noteTriggered) {
          PlaybackFXState* slide = &track->note.fx[fxASL];
          state->achchidVoices[trackIdx]->noteOn(track->note.pitchFinal, track->note.accent != 0,
            slide->isOn != 0, slide->fxValue);
        } else state->achchidVoices[trackIdx]->noteOff();
        break;
      default: break;
    }
    track->note.noteTriggered = track->note.noteReleased = track->note.noteKilled = 0;
  }
}

static void updateSampleVoices(ChipNomadState* state) {
  Project* project = &state->audioProject;
  PlaybackState* playback = &state->playbackState;
  for (int trackIdx = 0; trackIdx < project->tracksCount; trackIdx++) {
    PlaybackTrackState* track = &playback->tracks[trackIdx];
    SampleVoice* voice = state->sampleVoices[trackIdx];
    if (track->note.instrument == EMPTY_VALUE_8 ||
        project->instruments[track->note.instrument].type != InstrumentType::Sample) {
      voice->kill();
      continue;
    }

    InstrumentSample* sample = &project->instruments[track->note.instrument].chip.sample;
    int pitchCents = sample->pitch * 100 + track->note.fineOffset;
    int speedPercent = sample->speedPercent;
    int loopMode = sample->loopMode;
    uint8_t start = sample->start;
    uint8_t end = sample->end;
    int cutoff = sample->filterCutoffHz;
    int resonance = sample->filterResonance;
    int attack = sample->attack, decay = sample->decay, sustain = sample->sustain;
    int release = sample->release, shape = sample->envelopeShape;
    int triggerDecay = decay, triggerColor = sustain;
    if (track->note.pitchFinal != EMPTY_VALUE_8) {
      int rootNote = project->pitchTable.octaveSize * 4;
      if (rootNote >= project->pitchTable.length) rootNote = 0;
      int noteCents = project->linearPitch
        ? project->pitchTable.values[track->note.pitchFinal]
        : track->note.pitchFinal * 100;
      int rootCents = project->linearPitch
        ? project->pitchTable.values[rootNote]
        : rootNote * 100;
      pitchCents += noteCents - rootCents;
    }
    float gain = phraseGain(track, &project->instruments[track->note.instrument]);
    if (track->note.fx[fxSPT].isOn) pitchCents = (int8_t)track->note.fx[fxSPT].fxValue * 100 + track->note.fineOffset;
    if (track->note.fx[fxSST].isOn) start = track->note.fx[fxSST].fxValue;
    if (track->note.fx[fxSEN].isOn) end = track->note.fx[fxSEN].fxValue;
    if (track->note.fx[fxSVL].isOn) gain = track->note.fx[fxSVL].fxValue * track->note.volume / (255.0f * 15.0f);
    if (track->note.fx[fxSCF].isOn) cutoff = instrumentFXCutoff(track->note.fx[fxSCF].fxValue);
    if (track->note.fx[fxSRS].isOn) resonance = track->note.fx[fxSRS].fxValue;
    if (track->note.fx[fxSSP].isOn) speedPercent = track->note.fx[fxSSP].fxValue * 500 / 255;
    if (track->note.fx[fxSLP].isOn) loopMode = track->note.fx[fxSLP].fxValue;
    for (int i = 0; i < 4; i++) {
      PlaybackModState* mod = &track->note.modulation[i];
      if (!mod->modulation) continue;
      int value = playbackModScaleToRange(mod->outValue, 255);
      switch (mod->modulation->destination) {
        case 1:
          gain *= value / 255.0f;
          break;
        case 2:
          pitchCents += playbackModScaleToRange(mod->outValue, 1200);
          break;
        case 3:
          start = clampInt(start + value, 0, 255);
          break;
        case 4:
          end = clampInt(end + value, 0, 255);
          break;
        case 5:
          speedPercent += playbackModScaleToRange(mod->outValue, 500);
          break;
        case 6:
          loopMode += playbackModScaleToRange(mod->outValue, 2);
          break;
        case 7:
          cutoff += playbackModScaleToRange(mod->outValue, 20000);
          break;
        case 8:
          resonance += value;
          break;
      }
    }
    speedPercent = clampInt(speedPercent, 0, 500);
    loopMode = clampInt(loopMode, 0, 2);
    start = clampInt(start, 0, 255);
    end = clampInt(end, 0, 255);
    cutoff = clampInt(cutoff, 20, 20000);
    resonance = clampInt(resonance, 0, 255);
    if (track->note.fx[fxEAT].isOn) attack = track->note.fx[fxEAT].fxValue;
    if (track->note.fx[fxEDC].isOn) decay = track->note.fx[fxEDC].fxValue;
    if (track->note.fx[fxESU].isOn) sustain = track->note.fx[fxESU].fxValue;
    if (track->note.fx[fxERL].isOn) release = track->note.fx[fxERL].fxValue;
    if (track->note.fx[fxESH].isOn) shape = track->note.fx[fxESH].fxValue;
    applyVoicePostModulations(track, InstrumentType::Sample, &attack, &decay, &sustain, &release,
                              &shape, &triggerDecay, &triggerColor);
    voice->configure(sample, (float)pitchCents, gain, (float)speedPercent, start, end, (uint8_t)loopMode,
                     (uint16_t)cutoff, (uint8_t)resonance, attack, decay, sustain, release, shape);
  }
}

static void updateSCWFVoices(ChipNomadState* state) {
  Project* project = &state->audioProject;
  PlaybackState* playback = &state->playbackState;
  for (int trackIdx = 0; trackIdx < project->tracksCount; ++trackIdx) {
    PlaybackTrackState* track = &playback->tracks[trackIdx];
    SCWFVoice* voice = state->scwfVoices[trackIdx];
    if (track->note.instrument == EMPTY_VALUE_8 ||
        (project->instruments[track->note.instrument].type != InstrumentType::SCWF &&
         project->instruments[track->note.instrument].type != InstrumentType::BYOWTBL)) {
      voice->kill();
      continue;
    }
    Instrument* instrument = &project->instruments[track->note.instrument];
    InstrumentSCWF* scwf = &instrument->chip.scwf;
    InstrumentBYOWTBL* byowtbl = &instrument->chip.byowtbl;
    int detune = scwf->detune;
    int mix = scwf->mix;
    int cutoff = scwf->filterCutoffHz;
    int resonance = scwf->filterResonance;
    int attack = scwf->attack, decay = scwf->decay, sustain = scwf->sustain;
    int release = scwf->release, shape = scwf->envelopeShape;
    int triggerDecay = decay, triggerColor = sustain;
    uint8_t frameIndex[2] = {byowtbl->frameIndex[0], byowtbl->frameIndex[1]};
    int pitchModulation = 0;
    float gain = phraseGain(track, instrument);
    if (track->note.fx[fxSDT].isOn) detune = track->note.fx[fxSDT].fxValue;
    if (track->note.fx[fxSMX].isOn) mix = track->note.fx[fxSMX].fxValue;
    if (track->note.fx[fxSCF2].isOn) cutoff = instrumentFXCutoff(track->note.fx[fxSCF2].fxValue);
    if (track->note.fx[fxSRS2].isOn) resonance = track->note.fx[fxSRS2].fxValue;
    if (instrument->type == InstrumentType::BYOWTBL) {
      if (track->note.fx[fxBIA].isOn) frameIndex[0] = track->note.fx[fxBIA].fxValue;
      if (track->note.fx[fxBIB].isOn) frameIndex[1] = track->note.fx[fxBIB].fxValue;
    }
    for (int i = 0; i < 4; ++i) {
      PlaybackModState* mod = &track->note.modulation[i];
      if (!mod->modulation) continue;
      int value = playbackModScaleToRange(mod->outValue, 255);
      switch (mod->modulation->destination) {
        case 1: gain = modulationIsAdditive(mod->modulation->type) ? gain + value / 255.0f : value * track->note.volume / (255.0f * 15.0f); break;
        case 2: pitchModulation += playbackModScaleToRange(mod->outValue, 1200); break;
        case 3: detune += value; break;
        case 4: mix += value; break;
        case 5:
          if (instrument->type == InstrumentType::BYOWTBL) frameIndex[0] = (uint8_t)clampInt(frameIndex[0] + value, 0, 255);
          else cutoff += playbackModScaleToRange(mod->outValue, 20000);
          break;
        case 6:
          if (instrument->type == InstrumentType::BYOWTBL) frameIndex[1] = (uint8_t)clampInt(frameIndex[1] + value, 0, 255);
          else resonance += value;
          break;
        case 7: cutoff += playbackModScaleToRange(mod->outValue, 20000); break;
        case 8: resonance += value; break;
      }
    }
    int cents = track->note.pitchFinal == EMPTY_VALUE_8 ? 6000 :
      (project->linearPitch ? project->pitchTable.values[track->note.pitchFinal]
                            : (track->note.pitchFinal + 12) * 100) +
      track->note.fineOffset + pitchModulation;
    detune = clampInt(detune, 0, SCWF_DETUNE_MAX);
    mix = clampInt(mix, 0, 255);
    cutoff = clampInt(cutoff, 20, 20000);
    resonance = clampInt(resonance, 0, 255);
    if (track->note.fx[fxEAT].isOn) attack = track->note.fx[fxEAT].fxValue;
    if (track->note.fx[fxEDC].isOn) decay = track->note.fx[fxEDC].fxValue;
    if (track->note.fx[fxESU].isOn) sustain = track->note.fx[fxESU].fxValue;
    if (track->note.fx[fxERL].isOn) release = track->note.fx[fxERL].fxValue;
    if (track->note.fx[fxESH].isOn) shape = track->note.fx[fxESH].fxValue;
    applyVoicePostModulations(track, instrument->type, &attack, &decay, &sustain, &release,
                              &shape, &triggerDecay, &triggerColor);
    voice->configure(scwf, (float)cents, gain, scwfDetuneCents((uint8_t)detune), (uint8_t)mix,
                     (uint16_t)cutoff, (uint8_t)resonance,
                     instrument->type == InstrumentType::BYOWTBL ? byowtbl->frameSize : NULL,
                     instrument->type == InstrumentType::BYOWTBL ? frameIndex : NULL,
                     attack, decay, sustain, release, shape);
  }
}

static void updateAChChidVoices(ChipNomadState* state) {
  Project* project = &state->audioProject;
  PlaybackState* playback = &state->playbackState;
  for (int trackIdx = 0; trackIdx < project->tracksCount; ++trackIdx) {
    PlaybackTrackState* track = &playback->tracks[trackIdx];
    AChChidVoice* voice = state->achchidVoices[trackIdx];
    if (track->note.instrument == EMPTY_VALUE_8 ||
        project->instruments[track->note.instrument].type != InstrumentType::AChChid) {
      voice->kill();
      continue;
    }
    InstrumentAChChid* a = &project->instruments[track->note.instrument].chip.achchid;
    int cutoff = a->cutoff, resonance = a->resonance, envMod = a->envMod;
    int decay = a->decay, accent = a->accent;
    int timbre = a->timbre, color = a->color;
    float gain = phraseGain(track, &project->instruments[track->note.instrument]);
    if (track->note.fx[fxACF].isOn) cutoff = instrumentFXCutoff(track->note.fx[fxACF].fxValue);
    if (track->note.fx[fxARS].isOn) resonance = track->note.fx[fxARS].fxValue * 100 / 255;
    if (track->note.fx[fxAEM].isOn) envMod = track->note.fx[fxAEM].fxValue * 100 / 255;
    if (track->note.fx[fxADC].isOn) decay = 200 + track->note.fx[fxADC].fxValue * 1800 / 255;
    if (track->note.fx[fxAAC].isOn) accent = track->note.fx[fxAAC].fxValue * 100 / 255;
    if (a->wave == AChChidWave::braids) {
      timbre = slewEngineFX(track, fxATM, track->note.fx[fxATM].isOn ? track->note.fx[fxATM].fxValue : timbre / 129) * 129;
      color = slewEngineFX(track, fxACL, track->note.fx[fxACL].isOn ? track->note.fx[fxACL].fxValue : color / 129) * 129;
    }
    for (int i = 0; i < 4; ++i) {
      PlaybackModState* mod = &track->note.modulation[i];
      if (!mod->modulation) continue;
      switch (mod->modulation->destination) {
        case 1: { int value = playbackModScaleToRange(mod->outValue, 255); gain = modulationIsAdditive(mod->modulation->type) ? gain + value / 255.0f : value / 255.0f; break; }
        case 3: cutoff += playbackModScaleToRange(mod->outValue, 20000); break;
        case 4: resonance += playbackModScaleToRange(mod->outValue, 100); break;
        case 5: envMod += playbackModScaleToRange(mod->outValue, 100); break;
        case 6: decay += playbackModScaleToRange(mod->outValue, 1800); break;
        case 7: accent += playbackModScaleToRange(mod->outValue, 100); break;
        case 8: if (a->wave == AChChidWave::braids) timbre += playbackModScaleToRange(mod->outValue, 16384); break;
        case 9: if (a->wave == AChChidWave::braids) color += playbackModScaleToRange(mod->outValue, 16384); break;
      }
    }
    voice->configure((uint8_t)a->wave, a->fineTune, a->model, (uint16_t)clampInt(timbre, 0, 32767), (uint16_t)clampInt(color, 0, 32767),
      (uint16_t)clampInt(cutoff, 200, 20000), (uint8_t)clampInt(resonance, 0, 100),
      (uint8_t)clampInt(envMod, 0, 100), (uint16_t)clampInt(decay, 200, 2000),
      (uint8_t)clampInt(accent, 0, 100), gain < 0.0f ? 0.0f : gain);
  }
}

static void updateBraidsVoices(ChipNomadState* state) {
  Project* project = &state->audioProject;
  PlaybackState* playback = &state->playbackState;

  for (int trackIdx = 0; trackIdx < project->tracksCount; trackIdx++) {
    PlaybackTrackState* track = &playback->tracks[trackIdx];
    BraidsVoice* voice = state->braidsVoices[trackIdx];

    if (track->note.instrument == EMPTY_VALUE_8 ||
        project->instruments[track->note.instrument].type != InstrumentType::Braids) {
      voice->kill();
      continue;
    }

    InstrumentBraids* instrument = &project->instruments[track->note.instrument].chip.braids;
    int timbre = instrument->timbre;
    int color = instrument->color;
    int cutoff = instrument->filterCutoffHz;
    int resonance = instrument->filterResonance;
    int model = instrument->model;
    int pitchModulation = 0;
    int attack = instrument->attack, decay = instrument->decay, sustain = instrument->sustain;
    int release = instrument->release, shape = instrument->envelopeShape;
    int triggerDecay = decay, triggerColor = sustain;
    float gain = phraseGain(track, &project->instruments[track->note.instrument]);

    if (track->note.fx[fxBMD].isOn) {
      model = clampInt(track->note.fx[fxBMD].fxValue, 0,
        braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);
    }
    timbre = slewEngineFX(track, fxBTM, track->note.fx[fxBTM].isOn ? track->note.fx[fxBTM].fxValue : timbre / 129) * 129;
    color = slewEngineFX(track, fxBCL, track->note.fx[fxBCL].isOn ? track->note.fx[fxBCL].fxValue : color / 129) * 129;
    cutoff = instrumentFXCutoff(slewEngineFX(track, fxBCF, track->note.fx[fxBCF].isOn ? track->note.fx[fxBCF].fxValue : filterControlFromCutoff(cutoff)));
    resonance = slewEngineFX(track, fxBRS, track->note.fx[fxBRS].isOn ? track->note.fx[fxBRS].fxValue : resonance);

    for (int i = 0; i < 4; i++) {
      PlaybackModState* mod = &track->note.modulation[i];
      if (!mod->modulation) continue;
      switch (mod->modulation->destination) {
        case 1: {
          int value = playbackModScaleToRange(mod->outValue, 255);
          gain = modulationIsAdditive(mod->modulation->type)
            ? gain + value / 255.0f : value * track->note.volume / (255.0f * 15.0f);
          break;
        }
        case 2: pitchModulation += playbackModScaleToRange(mod->outValue, 1200); break;
        case 3: timbre += playbackModScaleToRange(mod->outValue, 16384); break;
        case 4: color += playbackModScaleToRange(mod->outValue, 16384); break;
        case 5: cutoff += playbackModScaleToRange(mod->outValue, 20000); break;
        case 6: resonance += playbackModScaleToRange(mod->outValue, 255); break;
      }
    }

    timbre = clampInt(timbre, 0, 32767);
    color = clampInt(color, 0, 32767);
    cutoff = clampInt(cutoff, 20, 20000);
    resonance = clampInt(resonance, 0, 255);
    if (track->note.fx[fxEAT].isOn) attack = track->note.fx[fxEAT].fxValue;
    if (track->note.fx[fxEDC].isOn) decay = track->note.fx[fxEDC].fxValue;
    if (track->note.fx[fxESU].isOn) sustain = track->note.fx[fxESU].fxValue;
    if (track->note.fx[fxERL].isOn) release = track->note.fx[fxERL].fxValue;
    if (track->note.fx[fxESH].isOn) shape = track->note.fx[fxESH].fxValue;
    applyVoicePostModulations(track, InstrumentType::Braids, &attack, &decay, &sustain, &release,
                              &shape, &triggerDecay, &triggerColor);

    voice->setModel(model);
    voice->setParameters(timbre, color);
    voice->setGain(gain);
    voice->setFilter(
      instrument->filterEnabled != 0,
      instrument->filterCharacter,
      static_cast<BraidsFilterMode>(instrument->filterMode > 2 ? 0 : instrument->filterMode),
      instrument->filterSlope24dB != 0,
      cutoff,
      resonance / 255.0f);

    voice->setEnvelope(true,
      envelopeTime(attack), envelopeTime(decay), sustain / 255.0f, envelopeTime(release), shape);

    if (track->note.pitchFinal != EMPTY_VALUE_8) {
      int cents = (project->linearPitch
        ? project->pitchTable.values[track->note.pitchFinal]
        : (track->note.pitchFinal + 12) * 100)
        + track->note.fineOffset + pitchModulation;
      voice->setPitch(static_cast<int16_t>((cents * 128) / 100));
    }

  }
}

static void updatePlaitsVoices(ChipNomadState* state) {
  Project* project = &state->audioProject;
  PlaybackState* playback = &state->playbackState;
  for (int trackIdx = 0; trackIdx < project->tracksCount; ++trackIdx) {
    PlaybackTrackState* track = &playback->tracks[trackIdx];
    PlaitsVoice* voice = state->plaitsVoices[trackIdx];
    if (track->note.instrument == EMPTY_VALUE_8 ||
        project->instruments[track->note.instrument].type != InstrumentType::Plaits) {
      voice->kill();
      continue;
    }

    InstrumentPlaits* p = &project->instruments[track->note.instrument].chip.plaits;
    int engine = p->engine;
    int harmonics = p->harmonics;
    int timbre = p->timbre;
    int morph = p->morph;
    int auxMix = p->auxMix;
    int cutoff = p->filterCutoffHz;
    int resonance = p->filterResonance;
    int pitchModulation = 0;
    int attack = p->attack, decay = p->decay, sustain = p->sustain;
    int release = p->release, shape = p->envelopeShape;
    int triggerDecay = decay, triggerColor = sustain;
    float gain = phraseGain(track, &project->instruments[track->note.instrument]);

    if (track->note.fx[fxPMD].isOn) engine = track->note.fx[fxPMD].fxValue;
    harmonics = slewEngineFX(track, fxPHA, track->note.fx[fxPHA].isOn ? track->note.fx[fxPHA].fxValue : harmonics / 129) * 129;
    timbre = slewEngineFX(track, fxPTM, track->note.fx[fxPTM].isOn ? track->note.fx[fxPTM].fxValue : timbre / 129) * 129;
    morph = slewEngineFX(track, fxPMO, track->note.fx[fxPMO].isOn ? track->note.fx[fxPMO].fxValue : morph / 129) * 129;
    auxMix = slewEngineFX(track, fxPAX, track->note.fx[fxPAX].isOn ? track->note.fx[fxPAX].fxValue : auxMix);
    cutoff = instrumentFXCutoff(slewEngineFX(track, fxPCF, track->note.fx[fxPCF].isOn ? track->note.fx[fxPCF].fxValue : filterControlFromCutoff(cutoff)));
    resonance = slewEngineFX(track, fxPRS, track->note.fx[fxPRS].isOn ? track->note.fx[fxPRS].fxValue : resonance);

    for (int i = 0; i < 4; ++i) {
      PlaybackModState* mod = &track->note.modulation[i];
      if (!mod->modulation) continue;
      int value = playbackModScaleToRange(mod->outValue, 255);
      switch (mod->modulation->destination) {
        case 1: gain = modulationIsAdditive(mod->modulation->type) ? gain + value / 255.0f : value * track->note.volume / (255.0f * 15.0f); break;
        case 2: pitchModulation += playbackModScaleToRange(mod->outValue, 1200); break;
        case 3: harmonics += playbackModScaleToRange(mod->outValue, 16384); break;
        case 4: timbre += playbackModScaleToRange(mod->outValue, 16384); break;
        case 5: morph += playbackModScaleToRange(mod->outValue, 16384); break;
        case 6: auxMix += value; break;
        case 7: cutoff += playbackModScaleToRange(mod->outValue, 20000); break;
        case 8: resonance += value; break;
      }
    }

    engine = clampInt(engine, 0, 23);
    harmonics = clampInt(harmonics, 0, 32767);
    timbre = clampInt(timbre, 0, 32767);
    morph = clampInt(morph, 0, 32767);
    auxMix = clampInt(auxMix, 0, 255);
    cutoff = clampInt(cutoff, 20, 20000);
    resonance = clampInt(resonance, 0, 255);
    if (track->note.fx[fxEAT].isOn) attack = track->note.fx[fxEAT].fxValue;
    if (track->note.fx[fxEDC].isOn) decay = track->note.fx[fxEDC].fxValue;
    if (track->note.fx[fxESU].isOn) sustain = track->note.fx[fxESU].fxValue;
    if (track->note.fx[fxERL].isOn) release = track->note.fx[fxERL].fxValue;
    if (track->note.fx[fxESH].isOn) shape = track->note.fx[fxESH].fxValue;
    if (p->envelopeMode == 0) {
      if (track->note.fx[fxTDC].isOn) triggerDecay = track->note.fx[fxTDC].fxValue;
      if (track->note.fx[fxTCL].isOn) triggerColor = track->note.fx[fxTCL].fxValue;
    }
    applyVoicePostModulations(track, InstrumentType::Plaits, &attack, &decay, &sustain, &release,
                              &shape, &triggerDecay, &triggerColor);
    int cents = track->note.pitchFinal == EMPTY_VALUE_8 ? 6000 :
      (project->linearPitch ? project->pitchTable.values[track->note.pitchFinal]
                            : (track->note.pitchFinal + 12) * 100) +
      track->note.fineOffset + pitchModulation;

    voice->configure((uint8_t)engine, (uint16_t)harmonics, (uint16_t)timbre,
                     (uint16_t)morph, (uint8_t)auxMix, p->envelopeMode,
                     triggerDecay, triggerColor,
                     cents / 100.0f, gain);
    voice->setFilter(p->filterEnabled != 0, p->filterCharacter, p->filterMode, p->filterSlope24dB != 0,
                     cutoff, resonance / 255.0f);
    voice->setEnvelope(envelopeTime(attack), envelopeTime(decay),
                       sustain / 255.0f, envelopeTime(release), shape);
  }
}

static void updatePlaitsAltVoices(ChipNomadState* state) {
  Project* project = &state->audioProject;
  PlaybackState* playback = &state->playbackState;
  for (int trackIdx = 0; trackIdx < project->tracksCount; ++trackIdx) {
    PlaybackTrackState* track = &playback->tracks[trackIdx];
    PlaitsAltVoice* voice = state->plaitsAltVoices[trackIdx];
    if (track->note.instrument == EMPTY_VALUE_8 ||
        project->instruments[track->note.instrument].type != InstrumentType::PlaitsAlt) {
      voice->kill();
      continue;
    }

    InstrumentPlaits* p = &project->instruments[track->note.instrument].chip.plaits;
    int engine = p->engine, harmonics = p->harmonics, timbre = p->timbre;
    int morph = p->morph, auxMix = p->auxMix, cutoff = p->filterCutoffHz;
    int resonance = p->filterResonance, pitchModulation = 0;
    int attack = p->attack, decay = p->decay, sustain = p->sustain;
    int release = p->release, shape = p->envelopeShape;
    int triggerDecay = decay, triggerColor = sustain;
    float gain = phraseGain(track, &project->instruments[track->note.instrument]);
    if (track->note.fx[fxPMD].isOn) engine = track->note.fx[fxPMD].fxValue;
    harmonics = slewEngineFX(track, fxPHA, track->note.fx[fxPHA].isOn ? track->note.fx[fxPHA].fxValue : harmonics / 129) * 129;
    timbre = slewEngineFX(track, fxPTM, track->note.fx[fxPTM].isOn ? track->note.fx[fxPTM].fxValue : timbre / 129) * 129;
    morph = slewEngineFX(track, fxPMO, track->note.fx[fxPMO].isOn ? track->note.fx[fxPMO].fxValue : morph / 129) * 129;
    auxMix = slewEngineFX(track, fxPAX, track->note.fx[fxPAX].isOn ? track->note.fx[fxPAX].fxValue : auxMix);
    cutoff = instrumentFXCutoff(slewEngineFX(track, fxPCF, track->note.fx[fxPCF].isOn ? track->note.fx[fxPCF].fxValue : filterControlFromCutoff(cutoff)));
    resonance = slewEngineFX(track, fxPRS, track->note.fx[fxPRS].isOn ? track->note.fx[fxPRS].fxValue : resonance);
    for (int i = 0; i < 4; ++i) {
      PlaybackModState* mod = &track->note.modulation[i];
      if (!mod->modulation) continue;
      int value = playbackModScaleToRange(mod->outValue, 255);
      switch (mod->modulation->destination) {
        case 1: gain = modulationIsAdditive(mod->modulation->type) ? gain + value / 255.0f : value * track->note.volume / (255.0f * 15.0f); break;
        case 2: pitchModulation += playbackModScaleToRange(mod->outValue, 1200); break;
        case 3: harmonics += playbackModScaleToRange(mod->outValue, 16384); break;
        case 4: timbre += playbackModScaleToRange(mod->outValue, 16384); break;
        case 5: morph += playbackModScaleToRange(mod->outValue, 16384); break;
        case 6: auxMix += value; break;
        case 7: cutoff += playbackModScaleToRange(mod->outValue, 20000); break;
        case 8: resonance += value; break;
      }
    }
    engine = clampInt(engine, 0, 23); harmonics = clampInt(harmonics, 0, 32767);
    timbre = clampInt(timbre, 0, 32767); morph = clampInt(morph, 0, 32767);
    auxMix = clampInt(auxMix, 0, 255); cutoff = clampInt(cutoff, 20, 20000);
    resonance = clampInt(resonance, 0, 255);
    if (track->note.fx[fxEAT].isOn) attack = track->note.fx[fxEAT].fxValue;
    if (track->note.fx[fxEDC].isOn) decay = track->note.fx[fxEDC].fxValue;
    if (track->note.fx[fxESU].isOn) sustain = track->note.fx[fxESU].fxValue;
    if (track->note.fx[fxERL].isOn) release = track->note.fx[fxERL].fxValue;
    if (track->note.fx[fxESH].isOn) shape = track->note.fx[fxESH].fxValue;
    if (p->envelopeMode == 0) {
      if (track->note.fx[fxTDC].isOn) triggerDecay = track->note.fx[fxTDC].fxValue;
      if (track->note.fx[fxTCL].isOn) triggerColor = track->note.fx[fxTCL].fxValue;
    }
    applyVoicePostModulations(track, InstrumentType::PlaitsAlt, &attack, &decay, &sustain, &release,
                              &shape, &triggerDecay, &triggerColor);
    int cents = track->note.pitchFinal == EMPTY_VALUE_8 ? 6000 :
      (project->linearPitch ? project->pitchTable.values[track->note.pitchFinal]
                            : (track->note.pitchFinal + 12) * 100) +
      track->note.fineOffset + pitchModulation;
    voice->configure((uint8_t)engine, (uint16_t)harmonics, (uint16_t)timbre,
      (uint16_t)morph, (uint8_t)auxMix, p->envelopeMode, triggerDecay, triggerColor,
      cents / 100.0f, gain);
    voice->setFilter(p->filterEnabled != 0, p->filterCharacter, p->filterMode, p->filterSlope24dB != 0,
      cutoff, resonance / 255.0f);
    voice->setEnvelope(envelopeTime(attack), envelopeTime(decay),
      sustain / 255.0f, envelopeTime(release), shape);
  }
}

static void detectAYPitchConflicts(ChipNomadState* state) {
  // Independent AY instances cannot fight over a shared tone generator.
  for (int i = 0; i < PROJECT_MAX_TRACKS; i++) state->trackWarnings[i] = 0;
}

void chipnomadSetQuality(ChipNomadState* state, ChipNomadQuality quality) {
  for (int i = 0; i < PROJECT_MAX_CHIPS; i++) {
    if (state->chips[i]) {
      state->chips[i]->setQuality(quality);
    }
  }
}

void chipnomadSetBraidsSettings(ChipNomadState* state, uint8_t bits,
                               uint8_t drift, uint8_t signature,
                               uint32_t signatureSeed) {
  if (!state) return;
  for (int i = 0; i < PROJECT_MAX_TRACKS; ++i) {
    state->braidsVoices[i]->setGlobalSettings(bits, drift, signature,
      signatureSeed);
  }
}
