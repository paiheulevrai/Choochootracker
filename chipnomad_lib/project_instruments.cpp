#include <string.h>
#include "project_instruments.h"
#include "project.h"
#include "synth/multimode_filter.h"

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

int modulationIsLiveStick(ModulationType type) {
  return type == ModulationType::StickLinear ||
         type == ModulationType::StickRate;
}

int modulationIsAdditive(ModulationType type) {
  return type == ModulationType::LFO || modulationIsLiveStick(type);
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
  post->filterCharacter = 2;
  post->filterMode = 0;
  post->filterSlope24dB = 0;
  post->filterCutoffHz = 20000;
  post->filterResonance = 0;
  post->attack = 0;
  post->decay = 0;
  post->sustain = 255;
  post->release = 0;
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

static const char* modNameBYOWTBL(int modIndex) {
  static const char *names[] = {"Off", "Volume", "Pitch", "Detune", "Mix", "Index A", "Index B", "Cutoff", "Reso"};
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

// The one source of truth for family metadata.  Values are accessed through
// typed code below; no union member is addressed by an offset.
#define D(n, f, r, v) {n, (uint8_t)(f), r, v}
#define N D("Off", instrumentNoFX, 0, InstrumentMotionValue::raw)
static const InstrumentModDestination destNone[] = {N};
static const InstrumentModDestination destAY1[] = {N, D("Volume", instrumentNoFX, 255, InstrumentMotionValue::raw), D("Pitch", instrumentNoFX, 0, InstrumentMotionValue::raw), D("Noise", instrumentNoFX, 0, InstrumentMotionValue::raw), D("EnvPrd", instrumentNoFX, 0, InstrumentMotionValue::raw)};
static const InstrumentModDestination destAY2[] = {N, D("Volume", instrumentNoFX,255,InstrumentMotionValue::raw), D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw), D("TonePit",instrumentNoFX,0,InstrumentMotionValue::raw), D("Noise",instrumentNoFX,0,InstrumentMotionValue::raw), D("EnvPit",instrumentNoFX,0,InstrumentMotionValue::raw), D("SoftPit",instrumentNoFX,0,InstrumentMotionValue::raw), D("FMDepth",instrumentNoFX,0,InstrumentMotionValue::raw), D("PulseW",instrumentNoFX,0,InstrumentMotionValue::raw), D("PulseL",instrumentNoFX,0,InstrumentMotionValue::raw), D("WavIdx",instrumentNoFX,0,InstrumentMotionValue::raw)};
static const InstrumentModDestination destAYSample[] = {N, D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw), D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw), D("SmplPit",instrumentNoFX,0,InstrumentMotionValue::raw), D("TonePit",instrumentNoFX,0,InstrumentMotionValue::raw), D("Noise",instrumentNoFX,0,InstrumentMotionValue::raw)};
static const InstrumentModDestination destBraids[] = {N,D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw),D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw),D("Timbre",fxBTM,16384,InstrumentMotionValue::raw),D("Color",fxBCL,16384,InstrumentMotionValue::raw),D("Cutoff",fxBCF,20000,InstrumentMotionValue::cutoff),D("Reso",fxBRS,255,InstrumentMotionValue::raw)};
static const InstrumentModDestination destSample[] = {N,D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw),D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw),D("Start",fxSST,255,InstrumentMotionValue::raw),D("End",fxSEN,255,InstrumentMotionValue::raw),D("Speed",fxSSP,500,InstrumentMotionValue::speed),D("Loop",fxSLP,2,InstrumentMotionValue::raw),D("Cutoff",fxSCF,20000,InstrumentMotionValue::cutoff),D("Reso",fxSRS,255,InstrumentMotionValue::raw)};
static const InstrumentModDestination destPlaits[] = {N,D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw),D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw),D("Harmonic",fxPHA,16384,InstrumentMotionValue::raw),D("Timbre",fxPTM,16384,InstrumentMotionValue::raw),D("Morph",fxPMO,16384,InstrumentMotionValue::raw),D("AuxMix",fxPAX,255,InstrumentMotionValue::raw),D("Cutoff",fxPCF,20000,InstrumentMotionValue::cutoff),D("Reso",fxPRS,255,InstrumentMotionValue::raw)};
static const InstrumentModDestination destSCWF[] = {N,D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw),D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw),D("Detune",fxSDT,255,InstrumentMotionValue::raw),D("Mix",fxSMX,255,InstrumentMotionValue::raw),D("Cutoff",fxSCF2,20000,InstrumentMotionValue::cutoff),D("Reso",fxSRS2,255,InstrumentMotionValue::raw)};
static const InstrumentModDestination destBYOWTBL[] = {N,D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw),D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw),D("Detune",fxSDT,255,InstrumentMotionValue::raw),D("Mix",fxSMX,255,InstrumentMotionValue::raw),D("Index A",fxBIA,255,InstrumentMotionValue::raw),D("Index B",fxBIB,255,InstrumentMotionValue::raw),D("Cutoff",fxSCF2,20000,InstrumentMotionValue::cutoff),D("Reso",fxSRS2,255,InstrumentMotionValue::raw)};
#undef N
#undef D
#define F(f, n) {(uint8_t)(f), n}
static const InstrumentFX fxAY1[]={F(fxAYM,"AYM"),F(fxNOI,"NOI"),F(fxNOA,"NOA"),F(fxERT,"ERT"),F(fxEAU,"EAU"),F(fxEVB,"EVB"),F(fxEBN,"EBN"),F(fxESL,"ESL"),F(fxENT,"ENT"),F(fxEPT,"EPT"),F(fxEPL,"EPL"),F(fxEPH,"EPH")};
static const InstrumentFX fxAY2[]={F(fxAYM,"AYM"),F(fxNOI,"NOI"),F(fxNOA,"NOA"),F(fxTNN,"TNN"),F(fxTNP,"TNP"),F(fxTNF,"TNF"),F(fxTRT,"TRT"),F(fxEAU,"EAU"),F(fxENN,"ENN"),F(fxENP,"ENP"),F(fxENF,"ENF"),F(fxERT,"ERT"),F(fxSFT,"SFT"),F(fxSFN,"SFN"),F(fxSFP,"SFP"),F(fxSFF,"SFF"),F(fxSRT,"SRT"),F(fxSFM,"SFM"),F(fxPWM,"PWM"),F(fxSPL,"SPL"),F(fxSWT,"SWT")};
static const InstrumentFX fxAYSample[]={F(fxAYM,"AYM"),F(fxNOI,"NOI"),F(fxNOA,"NOA"),F(fxTNN,"TNN"),F(fxTNP,"TNP"),F(fxTNF,"TNF"),F(fxTRT,"TRT"),F(fxSFN,"SFN"),F(fxSFP,"SFP"),F(fxSFF,"SFF"),F(fxSMS,"SMS")};
static const InstrumentFX fxBraids[]={F(fxBMD,"BMD"),F(fxBTM,"BTM"),F(fxBCL,"BCL"),F(fxBCF,"BCF"),F(fxBRS,"BRS")};
static const InstrumentFX fxSample[]={F(fxSPT,"SPT"),F(fxSST,"SST"),F(fxSEN,"SEN"),F(fxSVL,"SVL"),F(fxSCF,"SCF"),F(fxSRS,"SRS"),F(fxSSP,"SSP"),F(fxSLP,"SLP")};
static const InstrumentFX fxSCWF[]={F(fxSDT,"SDT"),F(fxSMX,"SMX"),F(fxSCF2,"SCF"),F(fxSRS2,"SRS")};
static const InstrumentFX fxBYOWTBL[]={F(fxSDT,"SDT"),F(fxSMX,"SMX"),F(fxBIA,"BIA"),F(fxBIB,"BIB"),F(fxSCF2,"SCF"),F(fxSRS2,"SRS")};
static const InstrumentFX fxPlaits[]={F(fxPMD,"PMD"),F(fxPHA,"PHA"),F(fxPTM,"PTM"),F(fxPMO,"PMO"),F(fxPAX,"PAX"),F(fxPCF,"PCF"),F(fxPRS,"PRS")};
#undef F
#define COUNT(a) (uint8_t)(sizeof(a) / sizeof((a)[0]))
static const InstrumentDefinition instrumentDefinitions[] = {
  {"None",InstrumentCategory::none,InstrumentScreenKind::none,destNone,COUNT(destNone),NULL,0,{0,modNameNone,initNoneInstrument,freeNoneInstrument,0,0}},
  {"AY Classic",InstrumentCategory::chip,InstrumentScreenKind::ay1,destAY1,COUNT(destAY1),fxAY1,COUNT(fxAY1),{4,modNameAY1,initAY1Instrument,freeAY1Instrument,0,0}},
  {"AY Plus",InstrumentCategory::chip,InstrumentScreenKind::ay2,destAY2,COUNT(destAY2),fxAY2,COUNT(fxAY2),{10,modNameAY2,initAY2Instrument,freeAY2Instrument,0,0}},
  {"AY Sample",InstrumentCategory::chip,InstrumentScreenKind::aySample,destAYSample,COUNT(destAYSample),fxAYSample,COUNT(fxAYSample),{6,modNameAYSample,initAYSampleInstrument,freeAYSampleInstrument,0,0}},
  {"Braids",InstrumentCategory::synth,InstrumentScreenKind::braids,destBraids,COUNT(destBraids),fxBraids,COUNT(fxBraids),{6,modNameBraids,initBraidsInstrument,freeBraidsInstrument,1,0}},
  {"PCM Sample",InstrumentCategory::sample,InstrumentScreenKind::sample,destSample,COUNT(destSample),fxSample,COUNT(fxSample),{8,modNameSample,initSampleInstrument,freeSampleInstrument,1,0}},
  {"Plaits",InstrumentCategory::synth,InstrumentScreenKind::plaits,destPlaits,COUNT(destPlaits),fxPlaits,COUNT(fxPlaits),{8,modNamePlaits,initPlaitsInstrument,freePlaitsInstrument,1,1}},
  {"Plaits-Alt",InstrumentCategory::synth,InstrumentScreenKind::plaits,destPlaits,COUNT(destPlaits),fxPlaits,COUNT(fxPlaits),{8,modNamePlaits,initPlaitsAltInstrument,freePlaitsInstrument,1,1}},
  {"2xSCWF",InstrumentCategory::sample,InstrumentScreenKind::scwf,destSCWF,COUNT(destSCWF),fxSCWF,COUNT(fxSCWF),{6,modNameSCWF,initSCWFInstrument,freeSCWFInstrument,1,0}},
  {"BYOWTBL",InstrumentCategory::sample,InstrumentScreenKind::byowtbl,destBYOWTBL,COUNT(destBYOWTBL),fxBYOWTBL,COUNT(fxBYOWTBL),{8,modNameBYOWTBL,initBYOWTBLInstrument,freeBYOWTBLInstrument,1,0}},
};
#undef COUNT

InstrumentFunctions getInstrumentFunctions(InstrumentType type) {
  return getInstrumentDefinition(type)->functions;
}

const InstrumentDefinition* getInstrumentDefinition(InstrumentType type) {
  int index = (int)type;
  if (index < 0 || index >= (int)InstrumentType::totalCount) index = 0;
  return &instrumentDefinitions[index];
}

const InstrumentModDestination* instrumentModDestination(InstrumentType type, int destination) {
  const InstrumentDefinition* definition = getInstrumentDefinition(type);
  return destination >= 0 && destination < definition->destinationCount ? &definition->destinations[destination] : NULL;
}

int instrumentFXAvailable(InstrumentType type, uint8_t fx) {
  const InstrumentDefinition* definition = getInstrumentDefinition(type);
  for (int i = 0; i < definition->fxCount; ++i) if (definition->fxList[i].fx == fx) return 1;
  return 0;
}

InstrumentVoicePostSettings* instrumentVoicePostSettings(Instrument* instrument) {
  switch (instrument->type) {
    case InstrumentType::Braids: return &instrument->chip.braids;
    case InstrumentType::Sample: return &instrument->chip.sample;
    case InstrumentType::SCWF: return &instrument->chip.scwf;
    case InstrumentType::BYOWTBL: return &instrument->chip.byowtbl;
    case InstrumentType::Plaits:
    case InstrumentType::PlaitsAlt: return &instrument->chip.plaits;
    default: return NULL;
  }
}

int instrumentMotionDestination(const Instrument* instrument, int destination, uint8_t* fx, int* base, int* range, InstrumentMotionValue* value) {
  const InstrumentModDestination* definition = instrumentModDestination(instrument->type, destination);
  if (!definition || definition->fx == instrumentNoFX) return 0;
  *fx = definition->fx; *range = definition->range; *value = definition->value;
  switch (instrument->type) {
    case InstrumentType::Braids:
      *base = destination == 3 ? (instrument->chip.braids.timbre + 64) / 129 : destination == 4 ? (instrument->chip.braids.color + 64) / 129 : destination == 5 ? instrument->chip.braids.filterCutoffHz : instrument->chip.braids.filterResonance; break;
    case InstrumentType::Plaits: case InstrumentType::PlaitsAlt:
      *base = destination == 3 ? (instrument->chip.plaits.harmonics + 64) / 129 : destination == 4 ? (instrument->chip.plaits.timbre + 64) / 129 : destination == 5 ? (instrument->chip.plaits.morph + 64) / 129 : destination == 6 ? instrument->chip.plaits.auxMix : destination == 7 ? instrument->chip.plaits.filterCutoffHz : instrument->chip.plaits.filterResonance; break;
    case InstrumentType::Sample:
      *base = destination == 3 ? instrument->chip.sample.start : destination == 4 ? instrument->chip.sample.end : destination == 5 ? instrument->chip.sample.speedPercent : destination == 6 ? instrument->chip.sample.loopMode : destination == 7 ? instrument->chip.sample.filterCutoffHz : instrument->chip.sample.filterResonance; break;
    case InstrumentType::SCWF:
      *base = destination == 3 ? instrument->chip.scwf.detune : destination == 4 ? instrument->chip.scwf.mix : destination == 5 ? instrument->chip.scwf.filterCutoffHz : instrument->chip.scwf.filterResonance; break;
    case InstrumentType::BYOWTBL:
      *base = destination == 3 ? instrument->chip.byowtbl.detune : destination == 4 ? instrument->chip.byowtbl.mix : destination == 5 ? instrument->chip.byowtbl.frameIndex[0] : destination == 6 ? instrument->chip.byowtbl.frameIndex[1] : destination == 7 ? instrument->chip.byowtbl.filterCutoffHz : instrument->chip.byowtbl.filterResonance; break;
    default: return 0;
  }
  return 1;
}

static const char* genericModName(int index) {
  static const char* names[] = {
    "RevSend", "DlySend",
    "M1 P1", "M1 P2", "M1 P3", "M1 P4",
    "M2 P1", "M2 P2", "M2 P3", "M2 P4",
    "M3 P1", "M3 P2", "M3 P3", "M3 P4",
    "M4 P1", "M4 P2", "M4 P3", "M4 P4",
    "ADSR A", "ADSR D", "ADSR S", "ADSR R", "ADSR Shape", "Trig D", "Trig C"
  };
  return index >= 0 && index < genericModTotalCount ? names[index] : "Misc";
}

int instrumentGenericModDestination(InstrumentType type, int destination) {
  int index = destination - getInstrumentFunctions(type).modDestinationsCount - 1;
  return index >= 0 && index < genericModTotalCount ? index : -1;
}

int instrumentModDestinationMax(InstrumentType type) {
  InstrumentFunctions functions = getInstrumentFunctions(type);
  int genericCount = functions.supportsVoicePost ? genericModTotalCount : genericModDestinationCount;
  return functions.modDestinationsCount + genericCount;
}

const char* instrumentModDestinationName(InstrumentType type, int destination) {
  const InstrumentModDestination* definition = instrumentModDestination(type, destination);
  if (definition) return definition->name;
  return genericModName(instrumentGenericModDestination(type, destination));
}
