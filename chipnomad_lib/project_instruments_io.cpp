#include "project.h"
#include "project_io_common.h"
#include "synth/sample_voice.h"
#include "synth/sr_wavetable_loader.h"
#include <stdio.h>
#include <string.h>

// Load AY1 instrument data (legacy format - version 1.0)
static int loadInstrumentAY1Legacy(FILE* file, Instrument* instrument) {
  while (1) {
    char* line = peekLine(file);
    if (line == NULL) return 1;
    if (line[0] == '#') return 0;

    if (strncmp(line, "- Volume envelope: ", 19) == 0) {
      // Read ADSR values into modulation struct
      Modulation* ve = &instrument->chip.ay.volumeEnvelope;
      ve->type = ModulationType::ADSR;
      ve->destination = 1;
      ve->amount = 127;
      sscanf(line, "- Volume envelope: %hhu,%hhu,%hhu,%hhu",
        &ve->p1, &ve->p2, &ve->p3, &ve->p4);  // A, D, S, R
    } else if (strncmp(line, "- Auto envelope: ", 17) == 0) {
      sscanf(line, "- Auto envelope: %hhu,%hhu",
        &instrument->chip.ay.autoEnvN, &instrument->chip.ay.autoEnvD);
    } else if (strncmp(line, "- Default mixer: ", 17) == 0) {
      sscanf(line, "- Default mixer: %hhX", &instrument->chip.ay.defaultMixer);
    }
    consumeLine(file);
  }
}

// Load AY1 instrument data (new format - version 2.0)
static int loadInstrumentAY1(FILE* file, Instrument* instrument) {
  while (1) {
    char* line = peekLine(file);
    if (line == NULL) return 1;
    if (line[0] == '#') return 0;

    if (strncmp(line, "- Volume envelope: ", 19) == 0) {
      // Read ADSR values into modulation struct
      Modulation* ve = &instrument->chip.ay.volumeEnvelope;
      ve->type = ModulationType::ADSR;
      ve->destination = 1;
      ve->amount = 127;
      sscanf(line, "- Volume envelope: %hhu,%hhu,%hhu,%hhu",
        &ve->p1, &ve->p2, &ve->p3, &ve->p4);  // A, D, S, R
    } else if (strncmp(line, "- Auto envelope: ", 17) == 0) {
      sscanf(line, "- Auto envelope: %hhu,%hhu",
        &instrument->chip.ay.autoEnvN, &instrument->chip.ay.autoEnvD);
    } else if (strncmp(line, "- Default mixer: ", 17) == 0) {
      sscanf(line, "- Default mixer: %hhX", &instrument->chip.ay.defaultMixer);
    }
    consumeLine(file);
  }
}

// Load AY2 instrument data
static int loadInstrumentAY2(FILE* file, Instrument* instrument) {
  while (1) {
    char* line = peekLine(file);
    if (line == NULL) return 1;
    if (line[0] == '#') return 0;

    // Tone oscillator
    if (strncmp(line, "- Tone on: ", 11) == 0) {
      sscanf(line, "- Tone on: %hhu", &instrument->chip.ay2.oscTone.isOn);
    } else if (strncmp(line, "- Tone pitch flag: ", 19) == 0) {
      sscanf(line, "- Tone pitch flag: %hhu", &instrument->chip.ay2.oscTone.pitchFlag);
    } else if (strncmp(line, "- Tone pitch offset: ", 21) == 0) {
      sscanf(line, "- Tone pitch offset: %hhd", &instrument->chip.ay2.oscTone.pitchOffset);
    } else if (strncmp(line, "- Tone fine tune: ", 18) == 0) {
      sscanf(line, "- Tone fine tune: %hhd", &instrument->chip.ay2.oscTone.fineTune);
    }
    // Noise oscillator
    else if (strncmp(line, "- Noise on: ", 12) == 0) {
      sscanf(line, "- Noise on: %hhu", &instrument->chip.ay2.oscNoise.isOn);
    } else if (strncmp(line, "- Noise period: ", 16) == 0) {
      sscanf(line, "- Noise period: %hhu", &instrument->chip.ay2.oscNoise.noisePeriod);
    }
    // Envelope oscillator
    else if (strncmp(line, "- Envelope shape: ", 18) == 0) {
      sscanf(line, "- Envelope shape: %hhu", &instrument->chip.ay2.oscEnvelope.shape);
    } else if (strncmp(line, "- Envelope auto N: ", 19) == 0) {
      sscanf(line, "- Envelope auto N: %hhu", &instrument->chip.ay2.oscEnvelope.autoEnvN);
    } else if (strncmp(line, "- Envelope auto D: ", 19) == 0) {
      sscanf(line, "- Envelope auto D: %hhu", &instrument->chip.ay2.oscEnvelope.autoEnvD);
    } else if (strncmp(line, "- Envelope pitch flag: ", 23) == 0) {
      sscanf(line, "- Envelope pitch flag: %hhu", &instrument->chip.ay2.oscEnvelope.pitchFlag);
    } else if (strncmp(line, "- Envelope pitch offset: ", 25) == 0) {
      sscanf(line, "- Envelope pitch offset: %hhd", &instrument->chip.ay2.oscEnvelope.pitchOffset);
    } else if (strncmp(line, "- Envelope fine tune: ", 22) == 0) {
      sscanf(line, "- Envelope fine tune: %hhd", &instrument->chip.ay2.oscEnvelope.fineTune);
    }
    // Software oscillator
    else if (strncmp(line, "- Software type: ", 17) == 0) {
      sscanf(line, "- Software type: %hhu", (uint8_t*)&instrument->chip.ay2.oscSoftware.type);
    } else if (strncmp(line, "- Software pitch flag: ", 23) == 0) {
      sscanf(line, "- Software pitch flag: %hhu", &instrument->chip.ay2.oscSoftware.pitchFlag);
    } else if (strncmp(line, "- Software pitch offset: ", 25) == 0) {
      sscanf(line, "- Software pitch offset: %hhd", &instrument->chip.ay2.oscSoftware.pitchOffset);
    } else if (strncmp(line, "- Software fine tune: ", 22) == 0) {
      sscanf(line, "- Software fine tune: %hhd", &instrument->chip.ay2.oscSoftware.fineTune);
    } else if (strncmp(line, "- Pulse Width: ", 15) == 0) {
      sscanf(line, "- Pulse Width: %hhu", &instrument->chip.ay2.oscSoftware.pulseWidth);
    } else if (strncmp(line, "- Pulse Low: ", 13) == 0) {
      sscanf(line, "- Pulse Low: %hhu", &instrument->chip.ay2.oscSoftware.pulseLow);
    } else if (strncmp(line, "- Wavetable Index: ", 19) == 0) {
      sscanf(line, "- Wavetable Index: %hhu", &instrument->chip.ay2.oscSoftware.wavetableIndex);
    } else if (strncmp(line, "- FM depth: ", 12) == 0) {
      sscanf(line, "- FM depth: %hhu", &instrument->chip.ay2.oscSoftware.fmDepth);
    } else if (strncmp(line, "- Env Shape Pair: ", 18) == 0) {
      sscanf(line, "- Env Shape Pair: %hhu", &instrument->chip.ay2.oscSoftware.envShapePair);
    }
    consumeLine(file);
  }
}

// Load AYSample instrument data
static int loadInstrumentAYSample(FILE* file, Instrument* instrument) {
  while (1) {
    char* line = peekLine(file);
    if (line == NULL) break;
    if (line[0] == '#') break;

    // Tone oscillator
    if (strncmp(line, "- Tone on: ", 11) == 0) {
      sscanf(line, "- Tone on: %hhu", &instrument->chip.aySample.oscTone.isOn);
    } else if (strncmp(line, "- Tone pitch flag: ", 19) == 0) {
      sscanf(line, "- Tone pitch flag: %hhu", &instrument->chip.aySample.oscTone.pitchFlag);
    } else if (strncmp(line, "- Tone pitch offset: ", 21) == 0) {
      sscanf(line, "- Tone pitch offset: %hhd", &instrument->chip.aySample.oscTone.pitchOffset);
    } else if (strncmp(line, "- Tone fine tune: ", 18) == 0) {
      sscanf(line, "- Tone fine tune: %hhd", &instrument->chip.aySample.oscTone.fineTune);
    }
    // Noise oscillator
    else if (strncmp(line, "- Noise on: ", 12) == 0) {
      sscanf(line, "- Noise on: %hhu", &instrument->chip.aySample.oscNoise.isOn);
    } else if (strncmp(line, "- Noise period: ", 16) == 0) {
      sscanf(line, "- Noise period: %hhu", &instrument->chip.aySample.oscNoise.noisePeriod);
    }
    // Sample parameters
    else if (strncmp(line, "- Sample name: ", 15) == 0) {
      sscanf(line, "- Sample name: %[^\n]", instrument->chip.aySample.sampleName);
    } else if (strncmp(line, "- Sample rate: ", 15) == 0) {
      sscanf(line, "- Sample rate: %hu", &instrument->chip.aySample.sampleRate);
    } else if (strncmp(line, "- Sample start: ", 16) == 0) {
      sscanf(line, "- Sample start: %hX", &instrument->chip.aySample.sampleStart);
    } else if (strncmp(line, "- Sample length: ", 17) == 0) {
      sscanf(line, "- Sample length: %hX", &instrument->chip.aySample.sampleLength);
    } else if (strncmp(line, "- Sample loop start: ", 21) == 0) {
      sscanf(line, "- Sample loop start: %hX", &instrument->chip.aySample.sampleLoopStart);
    } else if (strncmp(line, "- Sample pitch offset: ", 23) == 0) {
      sscanf(line, "- Sample pitch offset: %hhd", &instrument->chip.aySample.pitchOffset);
    } else if (strncmp(line, "- Sample fine tune: ", 20) == 0) {
      sscanf(line, "- Sample fine tune: %hhd", &instrument->chip.aySample.fineTune);
    }

    consumeLine(file);
  }

  // Check for #### Sample Data section
  char* line = peekLine(file);
  if (line != NULL && strncmp(line, "#### Sample Data", 16) == 0) {
    consumeLine(file);  // Consume "#### Sample Data"

    // Read "- Length:" line
    line = peekLine(file);
    if (line != NULL && strncmp(line, "- Length: ", 10) == 0) {
      uint16_t dataLen = 0;
      sscanf(line, "- Length: %hX", &dataLen);
      consumeLine(file);

      // Validate length
      if (dataLen > PROJECT_MAX_SAMPLE_SIZE) {
        dataLen = PROJECT_MAX_SAMPLE_SIZE;
      }
      instrument->chip.aySample.fileLength = dataLen;

      // Read "- Data:" line
      line = peekLine(file);
      if (line != NULL && strncmp(line, "- Data:", 7) == 0) {
        consumeLine(file);

        // Load binary data
        if (dataLen > 0) {
          if (loadBinaryData(file, &instrument->chip.aySample.sampleData, &dataLen, PROJECT_MAX_SAMPLE_SIZE)) {
            return 1;
          }
        }
      }
    }
  }

  return 0;
}

static int loadVoicePostSetting(const char* line, InstrumentVoicePostSettings* post) {
  if (strncmp(line, "- Filter enabled: ", 18) == 0) {
    sscanf(line, "- Filter enabled: %hhu", &post->filterEnabled);
    if (!post->filterEnabled) post->filterCharacter = 0;
  }
  else if (strncmp(line, "- Filter character: ", 20) == 0) sscanf(line, "- Filter character: %hhu", &post->filterCharacter);
  else if (strncmp(line, "- Filter mode: ", 15) == 0) sscanf(line, "- Filter mode: %hhu", &post->filterMode);
  else if (strncmp(line, "- Filter slope: ", 16) == 0) sscanf(line, "- Filter slope: %hhu", &post->filterSlope24dB);
  else if (strncmp(line, "- Filter cutoff: ", 17) == 0) sscanf(line, "- Filter cutoff: %hu", &post->filterCutoffHz);
  else if (strncmp(line, "- Filter resonance: ", 20) == 0) sscanf(line, "- Filter resonance: %hhu", &post->filterResonance);
  else if (strncmp(line, "- Attack: ", 10) == 0) sscanf(line, "- Attack: %hhu", &post->attack);
  else if (strncmp(line, "- Decay: ", 9) == 0) sscanf(line, "- Decay: %hhu", &post->decay);
  else if (strncmp(line, "- Sustain: ", 11) == 0) sscanf(line, "- Sustain: %hhu", &post->sustain);
  else if (strncmp(line, "- Release: ", 11) == 0) sscanf(line, "- Release: %hhu", &post->release);
  else if (strncmp(line, "- Envelope shape: ", 18) == 0) sscanf(line, "- Envelope shape: %hhu", &post->envelopeShape);
  else return 0;
  return 1;
}

static void saveVoicePostSettings(FILE* file, const InstrumentVoicePostSettings* post) {
  fprintf(file, "- Filter enabled: %hhu\n", post->filterEnabled);
  fprintf(file, "- Filter character: %hhu\n", post->filterCharacter);
  fprintf(file, "- Filter mode: %hhu\n", post->filterMode);
  fprintf(file, "- Filter slope: %hhu\n", post->filterSlope24dB);
  fprintf(file, "- Filter cutoff: %hu\n", post->filterCutoffHz);
  fprintf(file, "- Filter resonance: %hhu\n", post->filterResonance);
  fprintf(file, "- Attack: %hhu\n", post->attack);
  fprintf(file, "- Decay: %hhu\n", post->decay);
  fprintf(file, "- Sustain: %hhu\n", post->sustain);
  fprintf(file, "- Release: %hhu\n", post->release);
  fprintf(file, "- Envelope shape: %hhu\n", post->envelopeShape);
}

static int loadInstrumentBraids(FILE* file, Instrument* instrument) {
  InstrumentBraids* braids = &instrument->chip.braids;
  while (1) {
    char* line = peekLine(file);
    if (line == NULL || line[0] == '#') return 0;

    if (strncmp(line, "- Model: ", 9) == 0) sscanf(line, "- Model: %hhu", &braids->model);
    else if (strncmp(line, "- Timbre: ", 10) == 0) sscanf(line, "- Timbre: %hu", &braids->timbre);
    else if (strncmp(line, "- Color: ", 9) == 0) sscanf(line, "- Color: %hu", &braids->color);
    else loadVoicePostSetting(line, braids);
    consumeLine(file);
  }
}

static int loadInstrumentSample(FILE* file, Instrument* instrument) {
  InstrumentSample* sample = &instrument->chip.sample;
  while (1) {
    char* line = peekLine(file);
    if (line == NULL || line[0] == '#') break;
    if (strncmp(line, "- Sample path: ", 15) == 0) sscanf(line, "- Sample path: %255[^\n]", sample->path);
    else if (strncmp(line, "- Sample pitch: ", 16) == 0) sscanf(line, "- Sample pitch: %hhd", &sample->pitch);
    else if (strncmp(line, "- Sample speed: ", 16) == 0) sscanf(line, "- Sample speed: %hu", &sample->speedPercent);
    else if (strncmp(line, "- Sample start: ", 16) == 0) sscanf(line, "- Sample start: %hhu", &sample->start);
    else if (strncmp(line, "- Sample end: ", 14) == 0) sscanf(line, "- Sample end: %hhu", &sample->end);
    else if (strncmp(line, "- Sample loop: ", 15) == 0) sscanf(line, "- Sample loop: %hhu", &sample->loopMode);
    else if (strncmp(line, "- Sample volume: ", 17) == 0) sscanf(line, "- Sample volume: %hhu", &instrument->volume);
    else loadVoicePostSetting(line, sample);
    consumeLine(file);
  }
  if (sample->path[0]) {
    char error[64];
    sampleLoadWav16(sample->path, sample, error, sizeof(error));
  }
  if (sample->loopMode > 2) sample->loopMode = 0;
  return 0;
}

static int loadInstrumentSCWF(FILE* file, Instrument* instrument) {
  InstrumentSCWF* scwf = &instrument->chip.scwf;
  while (1) {
    char* line = peekLine(file);
    if (line == NULL || line[0] == '#') break;
    if (strncmp(line, "- Oscillator A path: ", 21) == 0) sscanf(line, "- Oscillator A path: %255[^\n]", scwf->oscillator[0].path);
    else if (strncmp(line, "- Oscillator B path: ", 21) == 0) sscanf(line, "- Oscillator B path: %255[^\n]", scwf->oscillator[1].path);
    else if (strncmp(line, "- Detune: ", 10) == 0) sscanf(line, "- Detune: %hhu", &scwf->detune);
    else if (strncmp(line, "- Mix: ", 7) == 0) sscanf(line, "- Mix: %hhu", &scwf->mix);
    else loadVoicePostSetting(line, scwf);
    consumeLine(file);
  }
  for (int i = 0; i < 2; ++i) {
    if (!scwf->oscillator[i].path[0]) continue;
    char error[64];
    sampleLoadWav16(scwf->oscillator[i].path, &scwf->oscillator[i], error, sizeof(error));
  }
  return 0;
}

static int loadInstrumentBYOWTBL(FILE* file, Instrument* instrument) {
  InstrumentBYOWTBL* table = &instrument->chip.byowtbl;
  while (1) {
    char* line = peekLine(file);
    if (line == NULL || line[0] == '#') break;
    if (strncmp(line, "- Oscillator A path: ", 21) == 0) sscanf(line, "- Oscillator A path: %255[^\n]", table->oscillator[0].path);
    else if (strncmp(line, "- Oscillator B path: ", 21) == 0) sscanf(line, "- Oscillator B path: %255[^\n]", table->oscillator[1].path);
    else if (strncmp(line, "- Position A: ", 14) == 0) sscanf(line, "- Position A: %hhu", &table->frameIndex[0]);
    else if (strncmp(line, "- Position B: ", 14) == 0) sscanf(line, "- Position B: %hhu", &table->frameIndex[1]);
    else if (strncmp(line, "- Detune: ", 10) == 0) sscanf(line, "- Detune: %hhu", &table->detune);
    else if (strncmp(line, "- Mix: ", 7) == 0) sscanf(line, "- Mix: %hhu", &table->mix);
    else loadVoicePostSetting(line, table);
    consumeLine(file);
  }
  for (int i = 0; i < 2; ++i) {
    if (!table->oscillator[i].path[0]) continue;
    char error[64];
    if (srWavetableLoadWav(table->oscillator[i].path, &table->oscillator[i],
                         &table->frameSize[i], &table->tableFrames[i], error, sizeof(error))) return 1;
  }
  return 0;
}

static int loadInstrumentPlaits(FILE* file, Instrument* instrument) {
  InstrumentPlaits* p = &instrument->chip.plaits;
  while (1) {
    char* line = peekLine(file);
    if (line == NULL || line[0] == '#') return 0;
    if (strncmp(line, "- Engine: ", 10) == 0) sscanf(line, "- Engine: %hhu", &p->engine);
    else if (strncmp(line, "- Harmonics: ", 13) == 0) sscanf(line, "- Harmonics: %hu", &p->harmonics);
    else if (strncmp(line, "- Timbre: ", 10) == 0) sscanf(line, "- Timbre: %hu", &p->timbre);
    else if (strncmp(line, "- Morph: ", 9) == 0) sscanf(line, "- Morph: %hu", &p->morph);
    else if (strncmp(line, "- Aux mix: ", 11) == 0) sscanf(line, "- Aux mix: %hhu", &p->auxMix);
    else if (strncmp(line, "- Envelope mode: ", 17) == 0) sscanf(line, "- Envelope mode: %hhu", &p->envelopeMode);
    else loadVoicePostSetting(line, p);
    consumeLine(file);
  }
}

// Load modulation data
static int loadModulation(FILE* file, Instrument* instrument) {
  for (int i = 0; i < 4; i++) {
    char* line = peekLine(file);
    if (line == NULL) return 1;
    if (line[0] == '#') return 0;

    char modPrefix[32];
    snprintf(modPrefix, 32, "- Mod%d: ", i + 1);

    if (strncmp(line, modPrefix, strlen(modPrefix)) == 0) {
      sscanf(line + strlen(modPrefix), "%hhu,%hhu,%hhd,%hhu,%hhu,%hhu,%hhu",
        (uint8_t*)&instrument->modulation[i].type,
        &instrument->modulation[i].destination,
        &instrument->modulation[i].amount,
        &instrument->modulation[i].p1,
        &instrument->modulation[i].p2,
        &instrument->modulation[i].p3,
        &instrument->modulation[i].p4);
      if (projectFileVersion < 3 && instrument->type == InstrumentType::Sample &&
          instrument->modulation[i].destination >= 5) {
        instrument->modulation[i].destination += 2;
      }
    }
    consumeLine(file);
  }
  return 0;
}

// Main load function
int instrumentLoadData(FILE* file, Instrument* instrument, Project* p) {
  instrumentClear(instrument);

  // Read common fields first
  while (1) {
    char* line = peekLine(file);
    if (line == NULL) return 1;
    if (line[0] == '#') return 0;

    if (strncmp(line, "- Name: ", 8) == 0) {
      sscanf(line, "- Name: %[^\n]", instrument->name);
    } else if (strncmp(line, "- Type: ", 8) == 0) {
      sscanf(line, "- Type: %hhd", reinterpret_cast<uint8_t*>(&instrument->type));
    } else if (strncmp(line, "- Table speed: ", 15) == 0) {
      sscanf(line, "- Table speed: %hhu", &instrument->tableSpeed);
    } else if (strncmp(line, "- Volume: ", 10) == 0) {
      sscanf(line, "- Volume: %hhu", &instrument->volume);
    } else if (strncmp(line, "- Transpose: ", 13) == 0) {
      sscanf(line, "- Transpose: %hhu", &instrument->transposeEnabled);
      consumeLine(file);
      // After reading transpose, check what comes next
      break;
    }
    consumeLine(file);
  }

  // Check version to determine format
  if (projectFileVersion == 1) {
    // Legacy format (version 1.0): no modulation, no "Chip data:" separator
    // Only AY1 instruments existed in version 1.0
    if (instrument->type == InstrumentType::AY1) {
      if (loadInstrumentAY1Legacy(file, instrument)) return 1;
    }
    return 0;
  }

  // New format (version 2.0): read modulation and chip data sections
  char* line = peekLine(file);
  if (line == NULL) return 1;

  if (strncmp(line, "- Modulation:", 13) == 0) {
    consumeLine(file);
    if (loadModulation(file, instrument)) return 1;
    line = peekLine(file);  // Read next line after modulation
    if (line == NULL) return 1;
  }

  if (strncmp(line, "- Chip data:", 12) == 0) {
    consumeLine(file);
    // Load chip-specific data based on instrument type
    switch (instrument->type) {
      case InstrumentType::AY1:
        if (loadInstrumentAY1(file, instrument)) return 1;
        break;
      case InstrumentType::AY2:
        if (loadInstrumentAY2(file, instrument)) return 1;
        break;
      case InstrumentType::AYSample:
        if (loadInstrumentAYSample(file, instrument)) return 1;
        break;
      case InstrumentType::Braids:
        if (loadInstrumentBraids(file, instrument)) return 1;
        break;
      case InstrumentType::Sample:
        if (loadInstrumentSample(file, instrument)) return 1;
        break;
      case InstrumentType::SCWF:
        if (loadInstrumentSCWF(file, instrument)) return 1;
        break;
      case InstrumentType::BYOWTBL:
        if (loadInstrumentBYOWTBL(file, instrument)) return 1;
        break;
      case InstrumentType::Plaits:
      case InstrumentType::PlaitsAlt:
        if (loadInstrumentPlaits(file, instrument)) return 1;
        break;
      default:
        break;
    }
  }

  if (instrument->type == InstrumentType::Braids &&
      instrument->chip.braids.filterCutoffHz > 20000) {
    instrument->chip.braids.filterCutoffHz = 20000;
  } else if ((instrument->type == InstrumentType::Plaits || instrument->type == InstrumentType::PlaitsAlt) &&
             instrument->chip.plaits.filterCutoffHz > 20000) {
    instrument->chip.plaits.filterCutoffHz = 20000;
  } else if (instrument->type == InstrumentType::Sample &&
             instrument->chip.sample.filterCutoffHz > 20000) {
    instrument->chip.sample.filterCutoffHz = 20000;
  }

  return 0;
}

// Save AY1 instrument data
static int saveInstrumentAY1(FILE* file, Instrument* instrument) {
  // Save volume envelope as ADSR values (for backward compatibility in file format)
  Modulation* ve = &instrument->chip.ay.volumeEnvelope;
  fprintf(file, "- Volume envelope: %hhu,%hhu,%hhu,%hhu\n",
    ve->p1, ve->p2, ve->p3, ve->p4);  // A, D, S, R
  fprintf(file, "- Auto envelope: %hhd,%hhd\n",
    instrument->chip.ay.autoEnvN, instrument->chip.ay.autoEnvD);
  fprintf(file, "- Default mixer: %02X\n", instrument->chip.ay.defaultMixer);
  return 0;
}

// Save AY2 instrument data
static int saveInstrumentAY2(FILE* file, Instrument* instrument) {
  // Tone oscillator
  fprintf(file, "- Tone on: %hhu\n", instrument->chip.ay2.oscTone.isOn);
  fprintf(file, "- Tone pitch flag: %hhu\n", instrument->chip.ay2.oscTone.pitchFlag);
  fprintf(file, "- Tone pitch offset: %hhd\n", instrument->chip.ay2.oscTone.pitchOffset);
  fprintf(file, "- Tone fine tune: %hhd\n", instrument->chip.ay2.oscTone.fineTune);

  // Noise oscillator
  fprintf(file, "- Noise on: %hhu\n", instrument->chip.ay2.oscNoise.isOn);
  fprintf(file, "- Noise period: %hhu\n", instrument->chip.ay2.oscNoise.noisePeriod);

  // Envelope oscillator
  fprintf(file, "- Envelope shape: %hhu\n", instrument->chip.ay2.oscEnvelope.shape);
  fprintf(file, "- Envelope auto N: %hhu\n", instrument->chip.ay2.oscEnvelope.autoEnvN);
  fprintf(file, "- Envelope auto D: %hhu\n", instrument->chip.ay2.oscEnvelope.autoEnvD);
  fprintf(file, "- Envelope pitch flag: %hhu\n", instrument->chip.ay2.oscEnvelope.pitchFlag);
  fprintf(file, "- Envelope pitch offset: %hhd\n", instrument->chip.ay2.oscEnvelope.pitchOffset);
  fprintf(file, "- Envelope fine tune: %hhd\n", instrument->chip.ay2.oscEnvelope.fineTune);

  // Software oscillator
  fprintf(file, "- Software type: %hhu\n", instrument->chip.ay2.oscSoftware.type);
  fprintf(file, "- Software pitch flag: %hhu\n", instrument->chip.ay2.oscSoftware.pitchFlag);
  fprintf(file, "- Software pitch offset: %hhd\n", instrument->chip.ay2.oscSoftware.pitchOffset);
  fprintf(file, "- Software fine tune: %hhd\n", instrument->chip.ay2.oscSoftware.fineTune);
  fprintf(file, "- Pulse Width: %hhu\n", instrument->chip.ay2.oscSoftware.pulseWidth);
  fprintf(file, "- Pulse Low: %hhu\n", instrument->chip.ay2.oscSoftware.pulseLow);
  fprintf(file, "- Wavetable Index: %hhu\n", instrument->chip.ay2.oscSoftware.wavetableIndex);
  fprintf(file, "- FM depth: %hhu\n", instrument->chip.ay2.oscSoftware.fmDepth);
  fprintf(file, "- Env Shape Pair: %hhu\n", instrument->chip.ay2.oscSoftware.envShapePair);

  return 0;
}

// Save AYSample instrument data
static int saveInstrumentAYSample(FILE* file, Instrument* instrument) {
  // Tone oscillator
  fprintf(file, "- Tone on: %hhu\n", instrument->chip.aySample.oscTone.isOn);
  fprintf(file, "- Tone pitch flag: %hhu\n", instrument->chip.aySample.oscTone.pitchFlag);
  fprintf(file, "- Tone pitch offset: %hhd\n", instrument->chip.aySample.oscTone.pitchOffset);
  fprintf(file, "- Tone fine tune: %hhd\n", instrument->chip.aySample.oscTone.fineTune);

  // Noise oscillator
  fprintf(file, "- Noise on: %hhu\n", instrument->chip.aySample.oscNoise.isOn);
  fprintf(file, "- Noise period: %hhu\n", instrument->chip.aySample.oscNoise.noisePeriod);

  // Sample parameters
  fprintf(file, "- Sample name: %s\n", instrument->chip.aySample.sampleName);
  fprintf(file, "- Sample rate: %hu\n", instrument->chip.aySample.sampleRate);
  fprintf(file, "- Sample start: %04X\n", instrument->chip.aySample.sampleStart);
  fprintf(file, "- Sample length: %04X\n", instrument->chip.aySample.sampleLength);
  fprintf(file, "- Sample loop start: %04X\n", instrument->chip.aySample.sampleLoopStart);
  fprintf(file, "- Sample pitch offset: %hhd\n", instrument->chip.aySample.pitchOffset);
  fprintf(file, "- Sample fine tune: %hhd\n", instrument->chip.aySample.fineTune);

  // Save sample data as a separate #### section
  if (instrument->chip.aySample.fileLength > 0 && instrument->chip.aySample.sampleData != NULL) {
    fprintf(file, "\n#### Sample Data\n\n");
    saveBinaryData(file,
                  instrument->chip.aySample.sampleData,
                  instrument->chip.aySample.fileLength);
  }

  return 0;
}

static int saveInstrumentBraids(FILE* file, Instrument* instrument) {
  InstrumentBraids* braids = &instrument->chip.braids;
  fprintf(file, "- Model: %hhu\n", braids->model);
  fprintf(file, "- Timbre: %hu\n", braids->timbre);
  fprintf(file, "- Color: %hu\n", braids->color);
  saveVoicePostSettings(file, braids);
  return 0;
}

static int saveInstrumentSample(FILE* file, Instrument* instrument) {
  InstrumentSample* sample = &instrument->chip.sample;
  fprintf(file, "- Sample path: %s\n", sample->path);
  fprintf(file, "- Sample pitch: %hhd\n", sample->pitch);
  fprintf(file, "- Sample speed: %hu\n", sample->speedPercent);
  fprintf(file, "- Sample start: %hhu\n", sample->start);
  fprintf(file, "- Sample end: %hhu\n", sample->end);
  fprintf(file, "- Sample loop: %hhu\n", sample->loopMode);
  saveVoicePostSettings(file, sample);
  return 0;
}

static int saveInstrumentSCWF(FILE* file, Instrument* instrument) {
  InstrumentSCWF* scwf = &instrument->chip.scwf;
  fprintf(file, "- Oscillator A path: %s\n", scwf->oscillator[0].path);
  fprintf(file, "- Oscillator B path: %s\n", scwf->oscillator[1].path);
  fprintf(file, "- Detune: %hhu\n", scwf->detune);
  fprintf(file, "- Mix: %hhu\n", scwf->mix);
  saveVoicePostSettings(file, scwf);
  return 0;
}

static int saveInstrumentBYOWTBL(FILE* file, Instrument* instrument) {
  InstrumentBYOWTBL* table = &instrument->chip.byowtbl;
  fprintf(file, "- Oscillator A path: %s\n", table->oscillator[0].path);
  fprintf(file, "- Oscillator B path: %s\n", table->oscillator[1].path);
  fprintf(file, "- Position A: %hhu\n", table->frameIndex[0]);
  fprintf(file, "- Position B: %hhu\n", table->frameIndex[1]);
  fprintf(file, "- Detune: %hhu\n", table->detune);
  fprintf(file, "- Mix: %hhu\n", table->mix);
  saveVoicePostSettings(file, table);
  return 0;
}

static int saveInstrumentPlaits(FILE* file, Instrument* instrument) {
  InstrumentPlaits* p = &instrument->chip.plaits;
  fprintf(file, "- Engine: %hhu\n", p->engine);
  fprintf(file, "- Harmonics: %hu\n", p->harmonics);
  fprintf(file, "- Timbre: %hu\n", p->timbre);
  fprintf(file, "- Morph: %hu\n", p->morph);
  fprintf(file, "- Aux mix: %hhu\n", p->auxMix);
  fprintf(file, "- Envelope mode: %hhu\n", p->envelopeMode);
  saveVoicePostSettings(file, p);
  return 0;
}

// Save modulation data
static int saveModulation(FILE* file, Instrument* instrument) {
  fprintf(file, "- Modulation:\n");
  for (int i = 0; i < 4; i++) {
    fprintf(file, "- Mod%d: %hhu,%hhu,%hhd,%hhu,%hhu,%hhu,%hhu\n",
      i + 1,
      instrument->modulation[i].type,
      instrument->modulation[i].destination,
      instrument->modulation[i].amount,
      instrument->modulation[i].p1,
      instrument->modulation[i].p2,
      instrument->modulation[i].p3,
      instrument->modulation[i].p4);
  }
  return 0;
}

// Main save function
int instrumentSaveData(FILE* file, int idx, Instrument* instrument) {
  fprintf(file, "\n### Instrument %X\n\n", idx);
  fprintf(file, "- Name: %s\n", instrument->name);
  fprintf(file, "- Type: %hhd\n", static_cast<uint8_t>(instrument->type));
  fprintf(file, "- Table speed: %hhu\n", instrument->tableSpeed);
  fprintf(file, "- Volume: %hhu\n", instrument->volume);
  fprintf(file, "- Transpose: %hhu\n", instrument->transposeEnabled);

  // Save modulation data
  saveModulation(file, instrument);

  // Save chip-specific data
  fprintf(file, "- Chip data:\n");
  switch (instrument->type) {
    case InstrumentType::AY1:
      saveInstrumentAY1(file, instrument);
      break;
    case InstrumentType::AY2:
      saveInstrumentAY2(file, instrument);
      break;
    case InstrumentType::AYSample:
      saveInstrumentAYSample(file, instrument);
      break;
    case InstrumentType::Braids:
      saveInstrumentBraids(file, instrument);
      break;
    case InstrumentType::Sample:
      saveInstrumentSample(file, instrument);
      break;
    case InstrumentType::SCWF:
      saveInstrumentSCWF(file, instrument);
      break;
    case InstrumentType::BYOWTBL:
      saveInstrumentBYOWTBL(file, instrument);
      break;
    case InstrumentType::Plaits:
    case InstrumentType::PlaitsAlt:
      saveInstrumentPlaits(file, instrument);
      break;
    default:
      break;
  }

  return 0;
}
