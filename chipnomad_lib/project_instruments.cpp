#include <string.h>
#include "project_instruments.h"
#include "project.h"

// Convention: the first modulation destination should be volume

static void initCommon(Instrument* instrument) {
  memset(instrument, 0, sizeof(Instrument));
  instrument->tableSpeed = 1;
  instrument->transposeEnabled = 1;
  instrument->volume = 255;
  instrument->modulation[0].type = ModulationType::ADSR;
  instrument->modulation[1].type = ModulationType::AHD;
  instrument->modulation[2].type = ModulationType::LFO;
  instrument->modulation[3].type = ModulationType::LFO;
}

static void freeCommon(Instrument* instrument) {
  memset(instrument, 0, sizeof(Instrument));
}

// Instrument type: None
static const char* modNameNone(int modIndex) {
  return "Off";
}

static int initNoneInstrument(Instrument* instrument) {
  initCommon(instrument);
  instrument->type = InstrumentType::none;
  return 0;
}

static int freeNoneInstrument(Instrument* instrument) {
  freeCommon(instrument);
  return 0;
}

// Instrument type: AY1
static const char* modNameAY1(int modIndex) {
  static const char *names[] = {"Off", "Volume", "Pitch", "Noise", "EnvPrd"};
  return names[modIndex];
}

static int initAY1Instrument(Instrument* instrument) {
  initCommon(instrument);
  instrument->type = InstrumentType::AY1;
  instrument->chip.ay.defaultMixer = 0x01; // Tone on, noise off, envelope shape 0
  instrument->chip.ay.volumeEnvelope = (Modulation){
    .type = ModulationType::ADSR, .destination = 1, .amount = 127, .p1 = 0, .p2 = 0, .p3 = 15, .p4 = 0
  };
  return 0;
}

static int freeAY1Instrument(Instrument* instrument) {
  freeCommon(instrument);
  return 0;
}

// Instrument type: AY2
static const char* modNameAY2(int modIndex) {
  static const char *names[] = {
    "Off", "Volume", "Pitch", "TonePit", "Noise", "EnvPit", "SoftPit", "FMDepth", "PulseW", "PulseL", "WavIdx"
  };
  return names[modIndex];
}

static int initAY2Instrument(Instrument* instrument) {
  initCommon(instrument);
  instrument->type = InstrumentType::AY2;
  instrument->chip.ay2.oscTone.isOn = 1;
  instrument->chip.ay2.oscEnvelope.pitchOffset = 48; // +4 octaves because envelope is lower
  instrument->chip.ay2.oscSoftware.pulseWidth = 0x80; // 50% duty cycle
  return 0;
}

static int freeAY2Instrument(Instrument* instrument) {
  freeCommon(instrument);
  return 0;
}

// Instrument type: AY Sample
static const char* modNameAYSample(int modIndex) {
  static const char *names[] = {"Off", "Volume", "Pitch", "SmplPit", "TonePit", "Noise"};
  return names[modIndex];
}

static int initAYSampleInstrument(Instrument* instrument) {
  initCommon(instrument);
  instrument->type = InstrumentType::AYSample;

  return 0;
}

static int freeAYSampleInstrument(Instrument* instrument) {
  if (instrument->chip.aySample.sampleData != NULL) {
    free(instrument->chip.aySample.sampleData);
  }
  freeCommon(instrument);
  return 0;
}

static const char* modNameBraids(int modIndex) {
  static const char *names[] = {"Off", "Volume", "Pitch", "Timbre", "Color", "Cutoff", "Reso"};
  return names[modIndex];
}

static void initVoicePostSettings(InstrumentVoicePostSettings* post) {
  post->filterEnabled = 1;
  post->filterMode = 0;
  post->filterSlope24dB = 0;
  post->filterCutoffHz = 20000;
  post->filterResonance = 0;
  post->attack = 1;
  post->decay = 0;
  post->sustain = 255;
  post->release = 1;
  post->envelopeShape = 0x80;
}

static int initBraidsInstrument(Instrument* instrument) {
  initCommon(instrument);
  instrument->type = InstrumentType::Braids;
  instrument->chip.braids.model = 0;
  instrument->chip.braids.timbre = 16384;
  instrument->chip.braids.color = 16384;
  initVoicePostSettings(&instrument->chip.braids);
  return 0;
}

static int freeBraidsInstrument(Instrument* instrument) {
  freeCommon(instrument);
  return 0;
}

static const char* modNamePlaits(int modIndex) {
  static const char *names[] = {
    "Off", "Volume", "Pitch", "Harmonic", "Timbre", "Morph", "AuxMix", "Cutoff", "Reso"
  };
  return names[modIndex];
}

static int initPlaitsInstrument(Instrument* instrument) {
  initCommon(instrument);
  instrument->type = InstrumentType::Plaits;
  InstrumentPlaits* plaits = &instrument->chip.plaits;
  plaits->harmonics = 16384;
  plaits->timbre = 16384;
  plaits->morph = 16384;
  initVoicePostSettings(plaits);
  return 0;
}

static int freePlaitsInstrument(Instrument* instrument) {
  freeCommon(instrument);
  return 0;
}

static int initPlaitsAltInstrument(Instrument* instrument) {
  initPlaitsInstrument(instrument);
  instrument->type = InstrumentType::PlaitsAlt;
  return 0;
}

static const char* modNameSample(int modIndex) {
  static const char *names[] = {
    "Off", "Volume", "Pitch", "Start", "End", "Speed", "Loop", "Cutoff", "Reso"
  };
  return names[modIndex];
}

static int initSampleInstrument(Instrument* instrument) {
  initCommon(instrument);
  instrument->type = InstrumentType::Sample;
  instrument->chip.sample.end = 255;
  instrument->chip.sample.speedPercent = 100;
  initVoicePostSettings(&instrument->chip.sample);
  return 0;
}

static int freeSampleInstrument(Instrument* instrument) {
  free(instrument->chip.sample.data);
  freeCommon(instrument);
  return 0;
}

static const char* modNameSCWF(int modIndex) {
  static const char *names[] = {"Off", "Volume", "Pitch", "Detune", "Mix", "Cutoff", "Reso"};
  return names[modIndex];
}

static int initSCWFInstrument(Instrument* instrument) {
  initCommon(instrument);
  instrument->type = InstrumentType::SCWF;
  instrument->chip.scwf.mix = 128;
  initVoicePostSettings(&instrument->chip.scwf);
  return 0;
}

static int freeSCWFInstrument(Instrument* instrument) {
  free(instrument->chip.scwf.oscillator[0].data);
  free(instrument->chip.scwf.oscillator[1].data);
  freeCommon(instrument);
  return 0;
}

static int initBYOWTBLInstrument(Instrument* instrument) {
  initSCWFInstrument(instrument);
  instrument->type = InstrumentType::BYOWTBL;
  return 0;
}

static int freeBYOWTBLInstrument(Instrument* instrument) {
  return freeSCWFInstrument(instrument);
}

// Get function pointers for instrument type
InstrumentFunctions getInstrumentFunctions(InstrumentType type) {
  switch (type) {
    case InstrumentType::AY1:
      return (InstrumentFunctions){
        .modDestinationsCount = 4,
        .modName = modNameAY1,
        .init = initAY1Instrument,
        .free = freeAY1Instrument
      };
    case InstrumentType::AY2:
      return (InstrumentFunctions){
        .modDestinationsCount = 10,
        .modName = modNameAY2,
        .init = initAY2Instrument,
        .free = freeAY2Instrument
      };
    case InstrumentType::AYSample:
      return (InstrumentFunctions){
        .modDestinationsCount = 6,
        .modName = modNameAYSample,
        .init = initAYSampleInstrument,
        .free = freeAYSampleInstrument
      };
    case InstrumentType::Braids:
      return (InstrumentFunctions){
        .modDestinationsCount = 6,
        .modName = modNameBraids,
        .init = initBraidsInstrument,
        .free = freeBraidsInstrument
      };
    case InstrumentType::Sample:
      return (InstrumentFunctions){
        .modDestinationsCount = 8,
        .modName = modNameSample,
        .init = initSampleInstrument,
        .free = freeSampleInstrument
      };
    case InstrumentType::SCWF:
      return (InstrumentFunctions){
        .modDestinationsCount = 6,
        .modName = modNameSCWF,
        .init = initSCWFInstrument,
        .free = freeSCWFInstrument
      };
    case InstrumentType::BYOWTBL:
      return (InstrumentFunctions){
        .modDestinationsCount = 6,
        .modName = modNameSCWF,
        .init = initBYOWTBLInstrument,
        .free = freeBYOWTBLInstrument
      };
    case InstrumentType::Plaits:
      return (InstrumentFunctions){
        .modDestinationsCount = 8,
        .modName = modNamePlaits,
        .init = initPlaitsInstrument,
        .free = freePlaitsInstrument
      };
    case InstrumentType::PlaitsAlt:
      return (InstrumentFunctions){
        .modDestinationsCount = 8,
        .modName = modNamePlaits,
        .init = initPlaitsAltInstrument,
        .free = freePlaitsInstrument
      };
    default:
      return (InstrumentFunctions){
        .modDestinationsCount = 0,
        .modName = modNameNone,
        .init = initNoneInstrument,
        .free = freeNoneInstrument
      };
  }
}

static const char* genericModName(int index) {
  static const char* names[] = {
    "RevSend", "DlySend",
    "M1 P1", "M1 P2", "M1 P3", "M1 P4",
    "M2 P1", "M2 P2", "M2 P3", "M2 P4",
    "M3 P1", "M3 P2", "M3 P3", "M3 P4",
    "M4 P1", "M4 P2", "M4 P3", "M4 P4"
  };
  return index >= 0 && index < genericModDestinationCount ? names[index] : "Misc";
}

int instrumentGenericModDestination(InstrumentType type, int destination) {
  int index = destination - getInstrumentFunctions(type).modDestinationsCount - 1;
  return index >= 0 && index < genericModDestinationCount ? index : -1;
}

int instrumentModDestinationMax(InstrumentType type) {
  return getInstrumentFunctions(type).modDestinationsCount + genericModDestinationCount;
}

const char* instrumentModDestinationName(InstrumentType type, int destination) {
  InstrumentFunctions functions = getInstrumentFunctions(type);
  if (destination >= 0 && destination <= functions.modDestinationsCount) {
    return functions.modName(destination);
  }
  return genericModName(instrumentGenericModDestination(type, destination));
}
