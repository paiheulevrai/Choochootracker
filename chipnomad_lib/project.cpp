#include <stdio.h>
#include <string.h>
#include "project.h"
#include "project_instruments.h"
#include "utils.h"

FXName fxNames[256];
static FXName instrumentGroupNames[(int)InstrumentType::totalCount][fxTotalCount];

// FX Names organized by groups (in the order as they appear in FX select screen)

// Sequencer FX
FXName fxNamesSequencer[] = {
  {fxARP, "ARP"}, {fxARC, "ARC"}, {fxPVB, "PVB"}, {fxPBN, "PBN"}, {fxPSL, "PSL"},
  {fxPIT, "PIT"}, {fxFIN, "FIN"}, {fxPRD, "PRD"}, {fxVOL, "VOL"}, {fxVSL, "VSL"},
  {fxRET, "RET"}, {fxDEL, "DEL"}, {fxOFF, "OFF"}, {fxKIL, "KIL"}, {fxTIC, "TIC"},
  {fxTBL, "TBL"}, {fxTBX, "TBX"}, {fxTHO, "THO"}, {fxTXH, "TXH"}, {fxGRV, "GRV"},
  {fxGGR, "GGR"}, {fxHOP, "HOP"}, {fxSNG, "SNG"}, {fxPRO, "PRO"},
  {fxMOD, "MOD"}, {fxSPD, "SPD"}, {fxSLE, "SLE"}
};
int fxSequencerCount = sizeof(fxNamesSequencer) / sizeof(FXName);

FXName fxNamesTrack[] = {{fxRSN, "RSN"}, {fxDSN, "DSN"}};
int fxTrackCount = sizeof(fxNamesTrack) / sizeof(FXName);

FXName fxNamesEnvelope[] = {
  {fxEAT, "EAT"}, {fxEDC, "EDC"}, {fxESU, "ESU"}, {fxERL, "ERL"}, {fxESH, "ESH"},
  {fxTDC, "TDC"}, {fxTCL, "TCL"}
};
int fxEnvelopeCount = sizeof(fxNamesEnvelope) / sizeof(FXName);

// Modulation FX
FXName fxNamesModulation[] = {
  {fxM1A, "M1A"}, {fxM11, "M11"}, {fxM12, "M12"}, {fxM13, "M13"}, {fxM14, "M14"},
  {fxM2A, "M2A"}, {fxM21, "M21"}, {fxM22, "M22"}, {fxM23, "M23"}, {fxM24, "M24"},
  {fxM3A, "M3A"}, {fxM31, "M31"}, {fxM32, "M32"}, {fxM33, "M33"}, {fxM34, "M34"},
  {fxM4A, "M4A"}, {fxM41, "M41"}, {fxM42, "M42"}, {fxM43, "M43"}, {fxM44, "M44"}
};
int fxModulationCount = sizeof(fxNamesModulation) / sizeof(FXName);

// FX Groups array. FX counts are filled in fillFXNames()
FXGroup fxGroups[] = {
  {"Sequencer FX", fxNamesSequencer, 0, 8, InstrumentType::none},
  {"Track FX", fxNamesTrack, 0, 2, InstrumentType::none},
  {"ADSR / Trigger FX", fxNamesEnvelope, 0, 7, InstrumentType::none},
  {"Modulation FX", fxNamesModulation, 0, 5, InstrumentType::none},
  {"AY Classic FX", NULL, 0, 8, InstrumentType::AY1},
  {"AY Plus FX", NULL, 0, 8, InstrumentType::AY2},
  {"AYSample FX", NULL, 0, 8, InstrumentType::AYSample},
  {"Braids FX", NULL, 0, 5, InstrumentType::Braids},
  {"Sample FX", NULL, 0, 6, InstrumentType::Sample},
  {"2xSCWF FX", NULL, 0, 4, InstrumentType::SCWF},
  {"BYOWTBL FX", NULL, 0, 6, InstrumentType::BYOWTBL},
  {"Plaits FX", NULL, 0, 7, InstrumentType::Plaits},
  {"Plaits-Alt FX", NULL, 0, 7, InstrumentType::PlaitsAlt},
};
int fxGroupCount = sizeof(fxGroups) / sizeof(FXGroup);

// Fill FX names
void fillFXNames() {
  // Initialize all FX names to "---"
  for (int c = 0; c < 256; c++) {
    strcpy(fxNames[c].name, "---");
    fxNames[c].fx = (enum FX)c;
  }

  // Fill counts in fxGroups array
  fxGroups[0].count = fxSequencerCount;
  fxGroups[1].count = fxTrackCount;
  fxGroups[2].count = fxEnvelopeCount;
  fxGroups[3].count = fxModulationCount;
  // Instrument groups are materialized from the declarative catalogue.  The
  // editor still receives its established FXName view, without duplicating
  // family availability or labels here.
  for (int group = 4; group < fxGroupCount; ++group) {
    InstrumentType type = fxGroups[group].instType;
    const InstrumentDefinition* definition = getInstrumentDefinition(type);
    FXName* names = instrumentGroupNames[(int)type];
    fxGroups[group].fxList = names;
    fxGroups[group].count = definition->fxCount;
    for (int i = 0; i < definition->fxCount; ++i) {
      names[i].fx = (FX)definition->fxList[i].fx;
      strcpy(names[i].name, definition->fxList[i].name);
    }
  }

  // Fill FX names from all groups
  for (int g = 0; g < fxGroupCount; g++) {
    for (int i = 0; i < fxGroups[g].count; i++) {
      enum FX fx = fxGroups[g].fxList[i].fx;
      strcpy(fxNames[fx].name, fxGroups[g].fxList[i].name);
      fxNames[fx].fx = fx;
    }
  }
}

// Initialize project
void projectInit(Project* p) {
  memset(p, 0, sizeof(Project));

  // Title
  strcpy(p->title, "");
  strcpy(p->author, "");
  // Period pitch is the hardware-validated default for AY and modern engines.
  p->linearPitch = 0;
  p->signedTrackSpeed = 1;
  p->perceptualEffects = 1;
  for (int i = 0; i < PROJECT_MAX_TRACKS; i++) p->trackVolume[i] = 100;
  p->reverbReturn = 100;
  p->reverbTime = 180;
  p->reverbDamping = 160;
  p->reverbFilterCutoffHz = 16000;
  p->delayReturn = 100;
  p->delayReverbSend = 0;
  p->delayTicks = 9;
  p->delayFeedback = 50;
  p->delayFilterCutoffHz = 12000;

  // Clean song structure
  for (int c = 0; c < PROJECT_MAX_LENGTH; c++) {
    for (int d = 0; d < PROJECT_MAX_TRACKS; d++) {
      p->song[c][d] = EMPTY_VALUE_16;
      p->songHighlight[c][d] = 0;
    }
  }

  // Clean chains
  for (int c = 0; c < PROJECT_MAX_CHAINS; c++) {
    chainClear(&p->chains[c]);
  }

  // Clean grooves
  for (int c = 0; c < PROJECT_MAX_GROOVES; c++) {
    for (int d = 0; d < 16; d++) {
      p->grooves[c].speed[d] = EMPTY_VALUE_8;
    }
  }

  // Default groove
  p->grooves[0].speed[0] = 6;
  p->grooves[0].speed[1] = 6;

  // Clean phrases
  for (int c = 0; c < PROJECT_MAX_PHRASES; c++) {
    phraseClear(&p->phrases[c]);
  }

  // Clean instruments
  for (int c = 0; c < PROJECT_MAX_INSTRUMENTS; c++) {
    instrumentClear(&p->instruments[c]);
  }

  // Clean tables
  for (int c = 0; c < PROJECT_MAX_TABLES; c++) {
    tableClear(&p->tables[c]);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Convenience functions

// Is chain empty?
int8_t chainIsEmpty(Project* project, int chain) {
  for (int c = 0; c < 16; c++) {
    if (project->chains[chain].rows[c].phrase != EMPTY_VALUE_16) return 0;
  }
  return 1;
}

// Is phrase empty?
int8_t phraseIsEmpty(Project* project, int phrase) {
  for (int c = 0; c < 16; c++) {
    if (project->phrases[phrase].rows[c].note != EMPTY_VALUE_8) return 0;
    if (project->phrases[phrase].rows[c].instrument != EMPTY_VALUE_8) return 0;
    if (project->phrases[phrase].rows[c].volume != EMPTY_VALUE_8) return 0;
    for (int d = 0; d < 3; d++) {
      if (project->phrases[phrase].rows[c].fx[d][0] != EMPTY_VALUE_8) return 0;
      if (project->phrases[phrase].rows[c].fx[d][1] != 0) return 0;
    }
  }

  return 1;
}

// Is instrument empty?
int8_t instrumentIsEmpty(Project* project, int instrument) {
  return project->instruments[instrument].type == InstrumentType::none;
}

// Is table empty?
int8_t tableIsEmpty(Project* project, int table) {
  for (int c = 0; c < 16; c++) {
    if (project->tables[table].rows[c].pitchFlag != 0) return 0;
    if (project->tables[table].rows[c].pitchOffset != 0) return 0;
    if (project->tables[table].rows[c].volume != EMPTY_VALUE_8) return 0;
    for (int d = 0; d < 4; d++) {
      if (project->tables[table].rows[c].fx[d][0] != EMPTY_VALUE_8) return 0;
      if (project->tables[table].rows[c].fx[d][1] != 0) return 0;
    }
  }

  return 1;
}

// Is groove empty?
int8_t grooveIsEmpty(Project* project, int groove) {
  for (int c = 0; c < 16; c++) {
    if (project->grooves[groove].speed[c] != EMPTY_VALUE_8) return 0;
  }
  return 1;
}

// Is wavetable empty?
int8_t wavetableIsEmpty(Project* project, int wavetable) {
  for (int c = 0; c < 32; c++) {
    if (project->ayWavetables[wavetable][c] != 0) return 0;
  }
  return 1;
}

// Note name in phrase
const char* noteName(Project* project, uint8_t note) {
  if (note == NOTE_OFF) {
    return "OFF";
  } else if (note < project->pitchTable.length) {
    return project->pitchTable.noteNames[note];
  } else {
    return "---";
  }
}

// Get number of tracks for a chip at index
int projectGetChipTracks(Project* p, int chipIndex) {
  return 1;
}

// Get total number of tracks for the project
int projectGetTotalTracks(Project* p) {
  int totalTracks = 0;
  for (int i = 0; i < p->chipsCount; i++) {
    totalTracks += projectGetChipTracks(p, i);
  }
  return totalTracks;
}

// Clear a single phrase with proper initialization
void phraseClear(Phrase* phrase) {
  for (int d = 0; d < 16; d++) {
    phrase->rows[d].note = EMPTY_VALUE_8;
    phrase->rows[d].instrument = EMPTY_VALUE_8;
    phrase->rows[d].volume = EMPTY_VALUE_8;
    for (int e = 0; e < 3; e++) {
      phrase->rows[d].fx[e][0] = EMPTY_VALUE_8;
      phrase->rows[d].fx[e][1] = 0;
    }
  }
}

// Clear a single chain with proper initialization
void chainClear(Chain* chain) {
  for (int d = 0; d < 16; d++) {
    chain->rows[d].phrase = EMPTY_VALUE_16;
    chain->rows[d].transpose = 0;
  }
}

// Clear a single instrument with proper initialization
void instrumentClear(Instrument* instrument) {
  getInstrumentFunctions(instrument->type).free(instrument);
  getInstrumentFunctions(InstrumentType::none).init(instrument);
}

// Clear a single table with proper initialization
void tableClear(Table* table) {
  for (int d = 0; d < 16; d++) {
    table->rows[d].pitchFlag = 0;
    table->rows[d].pitchOffset = 0;
    table->rows[d].volume = EMPTY_VALUE_8;
    for (int e = 0; e < 4; e++) {
      table->rows[d].fx[e][0] = EMPTY_VALUE_8;
      table->rows[d].fx[e][1] = 0;
    }
  }
}
