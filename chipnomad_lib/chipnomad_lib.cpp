#include "chipnomad_lib.h"
#include "playback.h"
#include "synth/braids_voice.h"
#include <stdlib.h>
#include <string.h>

static void detectAYPitchConflicts(ChipNomadState* state);
static void updateBraidsVoices(ChipNomadState* state);

static SoundChip* defaultChipFactory(int chipIndex, int sampleRate, ChipSetup setup) {
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
  if (!state->mixBuffer) {
    free(state);
    return NULL;
  }

  for (int i = 0; i < PROJECT_MAX_TRACKS; i++) {
    state->braidsVoices[i] = new BraidsVoice();
    state->braidsVoices[i]->init();
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
  }

  // Cleanup mix buffer
  free(state->mixBuffer);

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

  // Use provided factory or default
  ChipFactory chipFactory = factory ? factory : defaultChipFactory;

  // Initialize chips based on project's chipsCount
  for (int i = 0; i < state->project.chipsCount; i++) {
    state->chips[i] = chipFactory(i, sampleRate, state->project.chipSetup);
  }
}

int chipnomadRender(ChipNomadState* state, float* buffer, int samples) {
  if (!state) return 0;

  int samplesLeft = samples;
  int allTracksStopped = 0;

  while (samplesLeft > 0 && !allTracksStopped) {
    if ((int)state->frameSampleCounter == 0) {
      state->frameSampleCounter += state->sampleRate / state->project.tickRate;
      allTracksStopped = playbackNextFrame(state);
      updateBraidsVoices(state);
      // Decrease audio overload cooldown each frame
      if (state->audioOverload > 0) {
        state->audioOverload--;
      }
      // Detect AY pitch conflicts each frame
      detectAYPitchConflicts(state);
    }

    if (allTracksStopped) break;

    int samplesToRender = ((int)state->frameSampleCounter < samplesLeft) ?
    (int)state->frameSampleCounter : samplesLeft;
    int bufferOffset = (samples - samplesLeft) * 2;

    // Clear buffer section
    for (int i = 0; i < samplesToRender * 2; i++) {
      buffer[bufferOffset + i] = 0.0f;
    }

    // Ensure mix buffer is large enough
    int requiredSize = samplesToRender * 2;
    if (requiredSize > state->mixBufferSize) {
      state->mixBufferSize = requiredSize;
      state->mixBuffer = (float*)realloc(state->mixBuffer, state->mixBufferSize * sizeof(float));
      if (!state->mixBuffer) return 0; // Out of memory
    }

    // Mix all chips
    for (int chipIdx = 0; chipIdx < state->project.chipsCount; chipIdx++) {
      SoundChip* chip = state->chips[chipIdx];
      if (chip) {
        // Render chip to mix buffer
        chip->render(state->mixBuffer, samplesToRender);

        // Mix into main buffer
        for (int i = 0; i < samplesToRender * 2; i++) {
          buffer[bufferOffset + i] += state->mixBuffer[i];
        }
      }
    }

    // A Braids instrument owns one monophonic voice on its tracker track.
    for (int trackIdx = 0; trackIdx < state->project.tracksCount; trackIdx++) {
      BraidsVoice* voice = state->braidsVoices[trackIdx];
      if (!voice->active()) continue;
      voice->render(state->mixBuffer, samplesToRender);
      for (int i = 0; i < samplesToRender; i++) {
        float sample = state->mixBuffer[i] * 0.25f;
        buffer[bufferOffset + i * 2] += sample;
        buffer[bufferOffset + i * 2 + 1] += sample;
      }
    }

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

static bool braidsUsesInternalDecay(uint8_t model) {
  return model >= braids::MACRO_OSC_SHAPE_STRUCK_BELL &&
    model <= braids::MACRO_OSC_SHAPE_SNARE;
}

static float envelopeTime(uint8_t value) {
  float normalized = value / 255.0f;
  return normalized * normalized * 5.0f;
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
      track->note.noteTriggered = 0;
      track->note.noteReleased = 0;
      continue;
    }

    InstrumentBraids* instrument = &project->instruments[track->note.instrument].chip.braids;
    int timbre = instrument->timbre;
    int color = instrument->color;
    int cutoff = instrument->filterCutoffHz;
    int resonance = instrument->filterResonance;
    int pitchModulation = 0;
    float gain = 1.0f;

    for (int i = 0; i < 4; i++) {
      PlaybackModState* mod = &track->note.modulation[i];
      if (!mod->modulation) continue;
      switch (mod->modulation->destination) {
        case 1: {
          int value = playbackModScaleToRange(mod->outValue, 255);
          gain = mod->modulation->type == ModulationType::LFO
            ? gain + value / 255.0f : value / 255.0f;
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
    cutoff = clampInt(cutoff, 20, 43200);
    resonance = clampInt(resonance, 0, 255);

    voice->setModel(instrument->model);
    voice->setParameters(timbre, color);
    voice->setGain(gain);
    voice->setFilter(
      instrument->filterEnabled != 0,
      static_cast<BraidsFilterMode>(instrument->filterMode > 2 ? 0 : instrument->filterMode),
      instrument->filterSlope24dB != 0,
      cutoff,
      resonance / 255.0f);

    bool useEnvelope = !braidsUsesInternalDecay(instrument->model);
    voice->setEnvelope(useEnvelope,
      envelopeTime(instrument->attack), envelopeTime(instrument->decay),
      instrument->sustain / 255.0f, envelopeTime(instrument->release));

    if (track->note.pitchFinal != EMPTY_VALUE_8) {
      int cents = (project->linearPitch
        ? project->pitchTable.values[track->note.pitchFinal] + track->note.fineOffset
        : track->note.pitchFinal * 100) + pitchModulation;
      voice->setPitch(static_cast<int16_t>((cents * 128) / 100));
    }

    if (track->note.noteTriggered) {
      voice->noteOn();
      track->note.noteTriggered = 0;
    }
    if (track->note.noteReleased) {
      voice->noteOff();
      track->note.noteReleased = 0;
    }
  }
}

static void detectAYPitchConflicts(ChipNomadState* state) {
  if (!state || state->project.chipType != ChipType::AY) return;

  // Decrease existing warning cooldowns
  for (int i = 0; i < state->project.tracksCount; i++) {
    if (state->trackWarnings[i] > 0) {
      state->trackWarnings[i]--;
    }
  }

  // Collect tone periods for all tracks (0xFFFF = not using tone)
  uint16_t trackPeriods[PROJECT_MAX_TRACKS];
  for (int chipIdx = 0; chipIdx < state->project.chipsCount; chipIdx++) {
    SoundChipAY* chip = static_cast<SoundChipAY*>(state->chips[chipIdx]);
    if (!chip) continue;
    int trackOffset = chipIdx * 3;
    uint8_t mixer = chip->getRegister(7);

    for (int i = 0; i < 3; i++) {
      uint16_t period = chip->getRegister(i * 2) | (chip->getRegister(i * 2 + 1) << 8);
      uint8_t volume = chip->getRegister(8 + i);
      int toneEnabled = ((mixer >> i) & 1) == 0;
      int noiseEnabled = ((mixer >> (i + 3)) & 1) == 0;
      int envelopeMode = (volume & 16) != 0;

      // Check if track uses tone generation
      int isPureNoise = !toneEnabled && noiseEnabled && !envelopeMode;
      int isPureEnvelope = !toneEnabled && !noiseEnabled && envelopeMode;
      int isZeroVolume = (volume & 0xf) == 0 && !envelopeMode;
      int usesTone = !(isPureNoise || isPureEnvelope || isZeroVolume);

      trackPeriods[trackOffset + i] = (usesTone && period != 0) ? period : 0xFFFF;
    }
  }

  // Find conflicts by comparing all track periods
  for (int i = 0; i < state->project.tracksCount; i++) {
    for (int j = i + 1; j < state->project.tracksCount; j++) {
      if (trackPeriods[i] != 0xFFFF && trackPeriods[i] == trackPeriods[j]) {
        state->trackWarnings[i] = PITCH_CONFLICT_COOLDOWN_FRAMES;
        state->trackWarnings[j] = PITCH_CONFLICT_COOLDOWN_FRAMES;
      }
    }
  }
}

void chipnomadSetQuality(ChipNomadState* state, ChipNomadQuality quality) {
  for (int i = 0; i < PROJECT_MAX_CHIPS; i++) {
    if (state->chips[i]) {
      state->chips[i]->setQuality(quality);
    }
  }
}
