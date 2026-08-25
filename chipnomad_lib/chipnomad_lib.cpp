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
#include "synth/master_effects.h"
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static void detectAYPitchConflicts(ChipNomadState* state);
static void updateBraidsVoices(ChipNomadState* state);
static void updateSampleVoices(ChipNomadState* state);
static void updateSCWFVoices(ChipNomadState* state);
static void updatePlaitsVoices(ChipNomadState* state);
static void updatePlaitsAltVoices(ChipNomadState* state);
static void applyVoiceEvents(ChipNomadState* state);
static int hasAudioRateModulation(const ChipNomadState* state);
static void updateAudioRateModulations(ChipNomadState* state);
static void motionRecordFrame(ChipNomadState* state);

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
                               int destination, FX* fx, int* base, int* range) {
  switch (instrument->type) {
    case InstrumentType::Braids:
      if (destination == 3) { *fx = fxBTM; *base = (instrument->chip.braids.timbre + 64) / 129; *range = 16384; }
      else if (destination == 4) { *fx = fxBCL; *base = (instrument->chip.braids.color + 64) / 129; *range = 16384; }
      else return 0;
      break;
    case InstrumentType::Plaits:
    case InstrumentType::PlaitsAlt:
      if (destination == 3) { *fx = fxPHA; *base = (instrument->chip.plaits.harmonics + 64) / 129; *range = 16384; }
      else if (destination == 4) { *fx = fxPTM; *base = (instrument->chip.plaits.timbre + 64) / 129; *range = 16384; }
      else if (destination == 5) { *fx = fxPMO; *base = (instrument->chip.plaits.morph + 64) / 129; *range = 16384; }
      else if (destination == 6) { *fx = fxPAX; *base = instrument->chip.plaits.auxMix; *range = 255; }
      else return 0;
      break;
    default:
      return 0;
  }
  if (track->note.fx[*fx].isOn) *base = track->note.fx[*fx].fxValue;
  return 1;
}

static PhraseRow* motionPhraseRow(ChipNomadState* state, int trackIdx) {
  PlaybackTrackState* track = &state->playbackState.tracks[trackIdx];
  Project* project = &state->project;
  if (track->mode == PlaybackMode::stopped || track->mode == PlaybackMode::phraseRow ||
      track->songRow < 0 || track->songRow >= PROJECT_MAX_LENGTH ||
      track->chainRow < 0 || track->chainRow >= PROJECT_MAX_LENGTH ||
      track->phraseRow < 0 || track->phraseRow >= 16) return NULL;
  uint16_t chain = project->song[track->songRow][trackIdx];
  if (chain == EMPTY_VALUE_16 || chain >= PROJECT_MAX_CHAINS) return NULL;
  uint16_t phrase = project->chains[chain].rows[track->chainRow].phrase;
  if (phrase == EMPTY_VALUE_16 || phrase >= PROJECT_MAX_PHRASES) return NULL;
  return &project->phrases[phrase].rows[track->phraseRow];
}

static void rebaseMotionRecordRate(ChipNomadState* state) {
  for (int trackIdx = 0; trackIdx < state->project.tracksCount; ++trackIdx) {
    PlaybackTrackState* track = &state->playbackState.tracks[trackIdx];
    if (track->note.instrument == EMPTY_VALUE_8) continue;
    Instrument* instrument = &state->project.instruments[track->note.instrument];
    for (int slot = 0; slot < 4; ++slot) {
      PlaybackModState* modulation = &track->note.modulation[slot];
      if (!modulation->modulation || modulation->modulation->type != ModulationType::StickRate) continue;
      FX fx;
      int base, range;
      if (!motionDestinationFX(instrument, track, modulation->modulation->destination, &fx, &base, &range)) continue;
      int delta = playbackModScaleToRange(modulation->outValue, range);
      if (range == 16384) delta /= 129;
      track->note.fx[fx].isOn = 1;
      track->note.fx[fx].fxValue = clampInt(base + delta, 0, 255);
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
  chipnomadMotionClearOverflow();
  if (!mode) return;

  for (int trackIdx = 0; trackIdx < state->project.tracksCount; ++trackIdx) {
    PlaybackTrackState* track = &state->playbackState.tracks[trackIdx];
    if (track->note.instrument == EMPTY_VALUE_8) continue;
    PhraseRow* row = motionPhraseRow(state, trackIdx);
    if (!row) continue;
    Instrument* instrument = &state->project.instruments[track->note.instrument];

    FX targets[4];
    int values[4];
    int targetCount = 0;
    for (int slot = 0; slot < 4; ++slot) {
      PlaybackModState* modulation = &track->note.modulation[slot];
      if (!modulation->modulation || !modulationIsLiveStick(modulation->modulation->type)) continue;
      FX fx;
      int base, range;
      if (!motionDestinationFX(instrument, track, modulation->modulation->destination, &fx, &base, &range)) continue;
      int target = -1;
      for (int i = 0; i < targetCount; ++i) if (targets[i] == fx) target = i;
      int delta = playbackModScaleToRange(modulation->outValue, range);
      if (range == 16384) delta = delta / 129;
      if (target < 0) {
        target = targetCount++;
        targets[target] = fx;
        values[target] = base;
      }
      values[target] += delta;
    }

    for (int target = 0; target < targetCount; ++target) {
      FX fx = targets[target];
      int value = clampInt(values[target], 0, 255);
      int column = -1;
      for (int i = 2; i >= 0; --i) if (row->fx[i][0] == fx) { column = i; break; }
      if (mode == 2) {
        if (column >= 0) {
          row->fx[column][0] = EMPTY_VALUE_8;
          row->fx[column][1] = 0;
          motionRecordLast[trackIdx][fx] = -1;
          chipnomadMotionSetDirty();
        }
        continue;
      }
      if (column < 0 && motionRecordLast[trackIdx][fx] == value) continue;
      if (column < 0) {
        for (int i = 2; i >= 0; --i) if (row->fx[i][0] == EMPTY_VALUE_8) { column = i; break; }
      }
      if (column < 0) {
        chipnomadMotionSetOverflow();
        continue;
      }
      if (row->fx[column][0] != fx || row->fx[column][1] != value) {
        row->fx[column][0] = fx;
        row->fx[column][1] = (uint8_t)value;
        chipnomadMotionSetDirty();
      }
      motionRecordLast[trackIdx][fx] = (int16_t)value;
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
  int value = reverb ? state->project.trackReverbSend[trackIdx]
                     : state->project.trackDelaySend[trackIdx];
  if (track->note.fx[reverb ? fxRSN : fxDSN].isOn)
    value = track->note.fx[reverb ? fxRSN : fxDSN].fxValue * 100 / 255;
  if (track->note.instrument != EMPTY_VALUE_8) {
    InstrumentType type = state->project.instruments[track->note.instrument].type;
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
  return state->project.perceptualEffects ? mixerGain((uint8_t)value) : value / 100.0f;
}

static inline void mixTrackSample(ChipNomadState* state, int trackIdx,
                                  float* mix, float* reverb, float* delay,
                                  float sample, float reverbSend,
                                  float delaySend) {
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
  fillFXNames();
  projectInit(&state->project);
  playbackInit(&state->playbackState, &state->project);
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
  }

  // Cleanup mix buffer
  free(state->mixBuffer);
  free(state->reverbBuffer);
  free(state->delayBuffer);
  delete state->masterEffects;

  free(state);
}

void chipnomadInitChips(ChipNomadState* state, int sampleRate, ChipFactory factory) {
  if (!state) return;

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
    state->sampleVoices[i]->init((float)sampleRate);
    state->scwfVoices[i]->init((float)sampleRate);
    state->plaitsVoices[i]->init((float)sampleRate);
    state->plaitsAltVoices[i]->init((float)sampleRate);
  }

  // Use provided factory or default
  ChipFactory chipFactory = factory ? factory : defaultChipFactory;

  // Initialize chips based on project's chipsCount
  for (int i = 0; i < state->project.chipsCount; i++) {
    state->chips[i] = chipFactory(i, sampleRate, state->project.chipSetup);
  }
}

static int hasAudioRateModulation(const ChipNomadState* state) {
  for (int trackIdx = 0; trackIdx < state->project.tracksCount; ++trackIdx) {
    const PlaybackTrackState* track = &state->playbackState.tracks[trackIdx];
    for (int i = 0; i < 4; ++i) {
      const PlaybackModState* mod = &track->note.modulation[i];
      if (mod->modulation && mod->modulation->type == ModulationType::FLFO &&
          mod->modulation->destination != 0) return 1;
    }
  }
  return 0;
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
  for (int trackIdx = 0; trackIdx < state->project.tracksCount; ++trackIdx) {
    PlaybackTrackState* track = &state->playbackState.tracks[trackIdx];
    for (int i = 0; i < 4; ++i)
      playbackModNextAudio(&track->note.modulation[i], (float)state->sampleRate);
  }
  updateSampleVoices(state);
  updateSCWFVoices(state);
  updateBraidsVoices(state);
  updatePlaitsVoices(state);
  updatePlaitsAltVoices(state);
}

int chipnomadRender(ChipNomadState* state, float* buffer, int samples) {
  if (!state || !buffer || samples <= 0 || samples > INT_MAX / 2) return 0;

  int samplesLeft = samples;
  int allTracksStopped = 0;

  while (samplesLeft > 0 && !allTracksStopped) {
    if ((int)state->frameSampleCounter == 0) {
      float axes[4];
      for (int i = 0; i < 4; ++i)
        axes[i] = chipnomadLiveStickAxis(i);
      int enabled = chipnomadLiveStickIsEnabled();
      if (!enabled && chipnomadMotionRateResetPending()) enabled = 1;
      playbackUpdateLiveStickModulation(&state->playbackState, axes, enabled);
      state->frameSampleCounter += state->sampleRate / state->project.tickRate;
      allTracksStopped = playbackNextFrame(state);
      motionRecordFrame(state);
      if (allTracksStopped) playbackUpdateLiveStickModulation(&state->playbackState, axes, enabled);
      updateSampleVoices(state);
      updateSCWFVoices(state);
      updateBraidsVoices(state);
      updatePlaitsVoices(state);
      updatePlaitsAltVoices(state);
      applyVoiceEvents(state);
      // Decrease audio overload cooldown each frame
      if (state->audioOverload > 0) {
        state->audioOverload--;
      }
      for (int i = 0; i < PROJECT_MAX_TRACKS; ++i) {
        if (state->trackClipping[i] > 0) state->trackClipping[i]--;
      }
      // Detect AY pitch conflicts each frame
      detectAYPitchConflicts(state);
    }

    if (allTracksStopped) break;

    int samplesToRender = ((int)state->frameSampleCounter < samplesLeft) ?
    (int)state->frameSampleCounter : samplesLeft;
    if (hasAudioRateModulation(state)) {
      updateAudioRateModulations(state);
      samplesToRender = 1;
    }
    int bufferOffset = (samples - samplesLeft) * 2;
    for (int i = 0; i < state->project.tracksCount; ++i) state->voiceMonitors[i].active = 0;

    // Clear buffer section
    for (int i = 0; i < samplesToRender * 2; i++) {
      buffer[bufferOffset + i] = 0.0f;
    }

    // Ensure mix buffer is large enough
    int requiredSize = samplesToRender * 2;
    if (requiredSize > state->mixBufferSize) {
      if (!resizeMixBuffers(state, requiredSize)) return 0;
    }
    memset(state->reverbBuffer, 0, requiredSize * sizeof(float));
    memset(state->delayBuffer, 0, requiredSize * sizeof(float));

    // Mix all chips
    for (int chipIdx = 0; chipIdx < state->project.chipsCount; chipIdx++) {
      if (chipIdx >= state->project.tracksCount ||
          !state->playbackState.trackEnabled[chipIdx]) continue;
      uint8_t instrumentIdx = state->playbackState.tracks[chipIdx].note.instrument;
      if (instrumentIdx == EMPTY_VALUE_8) continue;
      InstrumentType type = state->project.instruments[instrumentIdx].type;
      if (type != InstrumentType::AY1 && type != InstrumentType::AY2 &&
          type != InstrumentType::AYSample) continue;
      SoundChip* chip = state->chips[chipIdx];
      if (chip) {
        // Render chip to mix buffer
        chip->render(state->mixBuffer, samplesToRender);

        // Mix into main buffer
        float trackGain = state->project.trackVolume[chipIdx] / 100.0f *
                          state->project.instruments[instrumentIdx].volume / 255.0f;
        float reverbSend = effectiveTrackSend(state, chipIdx, true);
        float delaySend = effectiveTrackSend(state, chipIdx, false);
        for (int i = 0; i < samplesToRender * 2; i++) {
          float sample = state->mixBuffer[i] * trackGain;
          mixTrackSample(state, chipIdx, &buffer[bufferOffset + i],
            &state->reverbBuffer[i], &state->delayBuffer[i], sample,
            reverbSend, delaySend);
        }
      }
    }

    // A Braids instrument owns one monophonic voice on its tracker track.
    for (int trackIdx = 0; trackIdx < state->project.tracksCount; trackIdx++) {
      if (!state->playbackState.trackEnabled[trackIdx]) continue;
      BraidsVoice* voice = state->braidsVoices[trackIdx];
      if (!voice->active()) continue;
      voice->render(state->mixBuffer, samplesToRender);
      captureVoiceMonitor(state, trackIdx, state->mixBuffer, samplesToRender, 1,
                          voice->envelopeLevel());
      float trackGain = state->project.trackVolume[trackIdx] / 100.0f;
      float reverbSend = effectiveTrackSend(state, trackIdx, true);
      float delaySend = effectiveTrackSend(state, trackIdx, false);
      for (int i = 0; i < samplesToRender; i++) {
        float sample = state->mixBuffer[i] * 0.25f * trackGain;
        mixTrackSample(state, trackIdx, &buffer[bufferOffset + i * 2],
          &state->reverbBuffer[i * 2], &state->delayBuffer[i * 2], sample,
          reverbSend, delaySend);
        mixTrackSample(state, trackIdx, &buffer[bufferOffset + i * 2 + 1],
          &state->reverbBuffer[i * 2 + 1], &state->delayBuffer[i * 2 + 1], sample,
          reverbSend, delaySend);
      }
    }

    // Sample voices render directly as clean interleaved stereo PCM.
    for (int trackIdx = 0; trackIdx < state->project.tracksCount; trackIdx++) {
      if (!state->playbackState.trackEnabled[trackIdx]) continue;
      SampleVoice* voice = state->sampleVoices[trackIdx];
      if (!voice->active()) continue;
      voice->render(state->mixBuffer, samplesToRender);
      captureVoiceMonitor(state, trackIdx, state->mixBuffer, samplesToRender, 2,
                          voice->envelopeLevel());
      float trackGain = state->project.trackVolume[trackIdx] / 100.0f;
      float reverbSend = effectiveTrackSend(state, trackIdx, true);
      float delaySend = effectiveTrackSend(state, trackIdx, false);
      for (int i = 0; i < samplesToRender * 2; i++) {
        float sample = state->mixBuffer[i] * trackGain;
        mixTrackSample(state, trackIdx, &buffer[bufferOffset + i],
          &state->reverbBuffer[i], &state->delayBuffer[i], sample,
          reverbSend, delaySend);
      }
    }

    // Plaits runs at its native 48 kHz and is interpolated to the output rate.
    // 2xSCWF is a clean stereo oscillator voice with the same post path as Sample.
    for (int trackIdx = 0; trackIdx < state->project.tracksCount; trackIdx++) {
      if (!state->playbackState.trackEnabled[trackIdx]) continue;
      SCWFVoice* voice = state->scwfVoices[trackIdx];
      if (!voice->active()) continue;
      voice->render(state->mixBuffer, samplesToRender);
      captureVoiceMonitor(state, trackIdx, state->mixBuffer, samplesToRender, 2,
                          voice->envelopeLevel());
      float trackGain = state->project.trackVolume[trackIdx] / 100.0f;
      float reverbSend = effectiveTrackSend(state, trackIdx, true);
      float delaySend = effectiveTrackSend(state, trackIdx, false);
      for (int i = 0; i < samplesToRender * 2; ++i) {
        float sample = state->mixBuffer[i] * trackGain;
        mixTrackSample(state, trackIdx, &buffer[bufferOffset + i],
          &state->reverbBuffer[i], &state->delayBuffer[i], sample, reverbSend, delaySend);
      }
    }

    // Plaits runs at its native 48 kHz and is interpolated to the output rate.
    for (int trackIdx = 0; trackIdx < state->project.tracksCount; trackIdx++) {
      if (!state->playbackState.trackEnabled[trackIdx]) continue;
      PlaitsVoice* voice = state->plaitsVoices[trackIdx];
      if (!voice->active()) continue;
      voice->render(state->mixBuffer, samplesToRender);
      captureVoiceMonitor(state, trackIdx, state->mixBuffer, samplesToRender, 1,
                          voice->envelopeLevel());
      float trackGain = state->project.trackVolume[trackIdx] / 100.0f;
      float reverbSend = effectiveTrackSend(state, trackIdx, true);
      float delaySend = effectiveTrackSend(state, trackIdx, false);
      for (int i = 0; i < samplesToRender; i++) {
        float sample = state->mixBuffer[i] * 0.25f * trackGain;
        mixTrackSample(state, trackIdx, &buffer[bufferOffset + i * 2],
          &state->reverbBuffer[i * 2], &state->delayBuffer[i * 2], sample,
          reverbSend, delaySend);
        mixTrackSample(state, trackIdx, &buffer[bufferOffset + i * 2 + 1],
          &state->reverbBuffer[i * 2 + 1], &state->delayBuffer[i * 2 + 1], sample,
          reverbSend, delaySend);
      }
    }

    // Plaits-Alt has the same controls, but its engines are the supplemental
    // catalogue selected by the instrument type.
    for (int trackIdx = 0; trackIdx < state->project.tracksCount; trackIdx++) {
      if (!state->playbackState.trackEnabled[trackIdx]) continue;
      PlaitsAltVoice* voice = state->plaitsAltVoices[trackIdx];
      if (!voice->active()) continue;
      voice->render(state->mixBuffer, samplesToRender);
      captureVoiceMonitor(state, trackIdx, state->mixBuffer, samplesToRender, 1,
                          voice->envelopeLevel());
      float trackGain = state->project.trackVolume[trackIdx] / 100.0f;
      float reverbSend = effectiveTrackSend(state, trackIdx, true);
      float delaySend = effectiveTrackSend(state, trackIdx, false);
      for (int i = 0; i < samplesToRender; i++) {
        float sample = state->mixBuffer[i] * 0.25f * trackGain;
        mixTrackSample(state, trackIdx, &buffer[bufferOffset + i * 2],
          &state->reverbBuffer[i * 2], &state->delayBuffer[i * 2], sample,
          reverbSend, delaySend);
        mixTrackSample(state, trackIdx, &buffer[bufferOffset + i * 2 + 1],
          &state->reverbBuffer[i * 2 + 1], &state->delayBuffer[i * 2 + 1], sample,
          reverbSend, delaySend);
      }
    }

    bool hasReverb = false;
    bool hasDelay = false;
    for (int i = 0; i < state->project.tracksCount; ++i) {
      hasReverb |= effectiveTrackSend(state, i, true) > 0.0f || state->project.delayReverbSend > 0;
      hasDelay |= effectiveTrackSend(state, i, false) > 0.0f;
    }
    state->masterEffects->process(hasReverb ? state->reverbBuffer : NULL,
                                  hasDelay ? state->delayBuffer : NULL,
                                  buffer + bufferOffset, samplesToRender, &state->project);

    // Apply mix volume and detect overload
    for (int i = 0; i < samplesToRender * 2; i++) {
      buffer[bufferOffset + i] *= state->mixVolume;
      // Check for audio overload (values beyond -1.0 to 1.0 range)
      if (buffer[bufferOffset + i] > 1.0f || buffer[bufferOffset + i] < -1.0f) {
        state->audioOverload = AUDIO_OVERLOAD_COOLDOWN_FRAMES;
      }
    }

    samplesLeft -= samplesToRender;
    state->frameSampleCounter -= (float)samplesToRender;
  }

  // Fill remaining buffer with silence if playback stopped early
  if (samplesLeft > 0) {
    int bufferOffset = (samples - samplesLeft) * 2;
    for (int i = 0; i < samplesLeft * 2; i++) {
      buffer[bufferOffset + i] = 0.0f;
    }
  }

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
  Project* project = &state->project;
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
      default: break;
    }
    track->note.noteTriggered = track->note.noteReleased = track->note.noteKilled = 0;
  }
}

static void updateSampleVoices(ChipNomadState* state) {
  Project* project = &state->project;
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
  Project* project = &state->project;
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

static void updateBraidsVoices(ChipNomadState* state) {
  Project* project = &state->project;
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
  Project* project = &state->project;
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
  Project* project = &state->project;
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
