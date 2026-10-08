#include <string.h>
#include "project_instruments.h"
#include "project.h"
#include "sid_patch.h"
#include "opll_presets.h"
#include "dx7_patch.h"
#include "opl_patch.h"
#include "four_op_patch.h"
#include "simple_chip_presets.h"
#include "synth/multimode_filter.h"

// Convention: the first modulation destination should be volume

static void initCommon(Instrument* instrument) {
  memset(instrument, 0, sizeof(Instrument));
  instrument->tableSpeed = 1;
  instrument->transposeEnabled = 1;
  instrument->volume = 255;
  instrument->pan = 128;
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
  post->filterCutoffHz = FILTER_CUTOFF_MAX_HZ;
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
  instrument->chip.sample.speedAlgorithm = 0;
  instrument->chip.sample.autoSensitivity = 50;
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

static const char* modNameAChChid(int modIndex) {
  static const char* names[] = {"Off", "Volume", "Pitch", "Cutoff", "Reso", "EnvMod", "Decay", "Accent", "Timbre", "Color"};
  return names[modIndex];
}

static int initAChChidInstrument(Instrument* instrument) {
  initCommon(instrument);
  instrument->type = InstrumentType::AChChid;
  InstrumentAChChid* a = &instrument->chip.achchid;
  a->wave = AChChidWave::saw;
  a->timbre = a->color = 16384;
  a->saturation = 0;
  a->cutoff = 1000;
  a->resonance = 0;
  a->envMod = 25;
  a->decay = 1000;
  a->accent = 100;
  return 0;
}

static int freeAChChidInstrument(Instrument* instrument) { freeCommon(instrument); return 0; }

static const char* modNameDrumSynth(int modIndex) {
  static const char* names[] = {"Off", "Volume", "Pitch", "Decay", "Tone", "Sweep", "Noise", "FM", "Drive", "Cutoff", "Reso"};
  return names[modIndex];
}
int drumSynthMacroUsed(DrumSynthEngine engine, int macro) {
  return (int)engine >= 0 && (int)engine < (int)DrumSynthEngine::totalCount && macro >= 0 && macro < 6;
}
static int initDrumSynthInstrument(Instrument* instrument) {
  initCommon(instrument); instrument->type = InstrumentType::DrumSynth;
  InstrumentDrumSynth* d = &instrument->chip.drumSynth;
  d->engine = DrumSynthEngine::kick; d->decay = 72; d->tone = 128;
  d->sweep = 150; d->noise = 24; d->fm = 32; d->drive = 28;
  initVoicePostSettings(d); return 0;
}
static int freeDrumSynthInstrument(Instrument* instrument) { freeCommon(instrument); return 0; }

static const char* modNameMME(int modIndex) {
  static const char* names[] = {"Off", "Volume", "Pitch", "Waves", "Interval", "Amount", "Flow", "Feedback", "Shaper", "Cutoff", "Reso"};
  return modIndex >= 0 && modIndex < 11 ? names[modIndex] : "Off";
}
static int initMMEInstrument(Instrument* instrument) {
  initCommon(instrument); instrument->type = InstrumentType::MME;
  InstrumentMME* m = &instrument->chip.mme;
  m->model = MMEModel::ring; m->waves = 80; m->interval = 128;
  m->amount = 255; m->feedback = m->shaper = 0; m->flow = 128;
  initVoicePostSettings(m); return 0;
}
static int freeMMEInstrument(Instrument* instrument) { freeCommon(instrument); return 0; }

static const char* modNameOPLL(int index) {
  static const char* names[] = {"Off", "Volume", "Pitch"};
  return index >= 0 && index < 3 ? names[index] : "Off";
}
static int initGenesisInstrument(Instrument* i){initCommon(i);i->type=InstrumentType::GenesisFM;initFourOpPatch(&i->chip.fourOp);strcpy(i->name,"Twin Reed");return 0;}
static int initArcadeInstrument(Instrument* i){initGenesisInstrument(i);i->type=InstrumentType::ArcadeFM;return 0;}
static int initDX7Instrument(Instrument* i) {
  initCommon(i);i->type=InstrumentType::DX7;initDX7Patch(&i->chip.dx7);
  strcpy(i->name,"Tracker Tine");return 0;
}
static int initOPLLInstrument(Instrument* instrument) {
  initCommon(instrument); instrument->type = InstrumentType::OPLL;
  opllApplyPreset(instrument, 3); return 0;
}
static int initVRC7Instrument(Instrument* instrument) {
  initCommon(instrument); instrument->type = InstrumentType::VRC7;
  opllApplyPreset(instrument, 3); return 0;
}
static int initOPL2Instrument(Instrument* instrument) {
  initCommon(instrument); instrument->type=InstrumentType::OPL2;initOPLPatch(&instrument->chip.opl);strcpy(instrument->name,"Soft FM Keys");return 0;
}
static int initOPL3Instrument(Instrument* instrument) {
  initOPL2Instrument(instrument);instrument->type=InstrumentType::OPL3;return 0;
}
static int initSIDInstrument(Instrument* i){initCommon(i);i->type=InstrumentType::SID;initSIDPatch(&i->chip.sid);strcpy(i->name,"Moving Pulse");return 0;}
static int initSegaInstrument(Instrument* i){initCommon(i);i->type=InstrumentType::SegaPSG;simpleChipApplyPreset(i,0);return 0;}
static int initGBPulseInstrument(Instrument* i){initCommon(i);i->type=InstrumentType::GBPulse;simpleChipApplyPreset(i,0);return 0;}
static int initGBNoiseInstrument(Instrument* i){initCommon(i);i->type=InstrumentType::GBNoise;simpleChipApplyPreset(i,0);return 0;}
static const char* modNameSintered(int modIndex) {
  static const char* names[] = {"Off", "Volume", "Pitch", "Decay", "Mod", "A", "B", "Motion", "C", "Cutoff", "Reso"};
  return modIndex >= 0 && modIndex < 11 ? names[modIndex] : "Off";
}
static int initSinteredInstrument(Instrument* instrument) {
  initCommon(instrument); instrument->type = InstrumentType::Sintered;
  InstrumentSintered* s = &instrument->chip.sintered;
  s->model = SinteredModel::knot; s->decay = 82; s->mod = 112; s->a = 128;
  s->b = 96; s->motion = 128; s->c = 64; initVoicePostSettings(s); return 0;
}
static int freeSinteredInstrument(Instrument* instrument) { freeCommon(instrument); return 0; }

static const char* modNameMidi(int modIndex) {
  return "Off";
}
static int initMidiInstrument(Instrument* instrument) {
  initCommon(instrument); instrument->type = InstrumentType::Midi;
  InstrumentMidi* m = &instrument->chip.midi;
  m->channel = 0;
  m->program = EMPTY_VALUE_8;
  m->bankHigh = EMPTY_VALUE_8;
  m->bankLow = EMPTY_VALUE_8;
  for (int i = 0; i < 4; ++i) m->ccNumber[i] = EMPTY_VALUE_8;
  return 0;
}
static int freeMidiInstrument(Instrument* instrument) { freeCommon(instrument); return 0; }

// The one source of truth for family metadata.  Values are accessed through
// typed code below; no union member is addressed by an offset.
#define D(n, f, r, v) {n, (uint8_t)(f), r, v}
#define N D("-", instrumentNoFX, 0, InstrumentMotionValue::raw)
static const InstrumentModDestination destNone[] = {N};
static const InstrumentModDestination destAY1[] = {N, D("Volume", instrumentNoFX, 255, InstrumentMotionValue::raw), D("Pitch", instrumentNoFX, 0, InstrumentMotionValue::raw), D("Noise", instrumentNoFX, 0, InstrumentMotionValue::raw), D("EnvPrd", instrumentNoFX, 0, InstrumentMotionValue::raw)};
static const InstrumentModDestination destAY2[] = {N, D("Volume", instrumentNoFX,255,InstrumentMotionValue::raw), D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw), D("TonePit",instrumentNoFX,0,InstrumentMotionValue::raw), D("Noise",instrumentNoFX,0,InstrumentMotionValue::raw), D("EnvPit",instrumentNoFX,0,InstrumentMotionValue::raw), D("SoftPit",instrumentNoFX,0,InstrumentMotionValue::raw), D("FMDepth",instrumentNoFX,0,InstrumentMotionValue::raw), D("PulseW",instrumentNoFX,0,InstrumentMotionValue::raw), D("PulseL",instrumentNoFX,0,InstrumentMotionValue::raw), D("WavIdx",instrumentNoFX,0,InstrumentMotionValue::raw)};
static const InstrumentModDestination destAYSample[] = {N, D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw), D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw), D("SmplPit",instrumentNoFX,0,InstrumentMotionValue::raw), D("TonePit",instrumentNoFX,0,InstrumentMotionValue::raw), D("Noise",instrumentNoFX,0,InstrumentMotionValue::raw)};
static const InstrumentModDestination destBraids[] = {N,D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw),D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw),D("Timbre",fxBTM,16384,InstrumentMotionValue::raw),D("Color",fxBCL,16384,InstrumentMotionValue::raw),D("Cutoff",fxBCF,FILTER_CUTOFF_MAX_HZ,InstrumentMotionValue::cutoff),D("Reso",fxBRS,255,InstrumentMotionValue::raw)};
static const InstrumentModDestination destSample[] = {N,D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw),D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw),D("Start",fxSST,255,InstrumentMotionValue::raw),D("End",fxSEN,255,InstrumentMotionValue::raw),D("Speed",fxSSP,500,InstrumentMotionValue::speed),D("Loop",fxSLP,2,InstrumentMotionValue::raw),D("Cutoff",fxSCF,FILTER_CUTOFF_MAX_HZ,InstrumentMotionValue::cutoff),D("Reso",fxSRS,255,InstrumentMotionValue::raw)};
static const InstrumentModDestination destPlaits[] = {N,D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw),D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw),D("Harmonic",fxPHA,16384,InstrumentMotionValue::raw),D("Timbre",fxPTM,16384,InstrumentMotionValue::raw),D("Morph",fxPMO,16384,InstrumentMotionValue::raw),D("AuxMix",fxPAX,255,InstrumentMotionValue::raw),D("Cutoff",fxPCF,FILTER_CUTOFF_MAX_HZ,InstrumentMotionValue::cutoff),D("Reso",fxPRS,255,InstrumentMotionValue::raw)};
static const InstrumentModDestination destSCWF[] = {N,D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw),D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw),D("Detune",fxSDT,255,InstrumentMotionValue::raw),D("Mix",fxSMX,255,InstrumentMotionValue::raw),D("Cutoff",fxSCF2,FILTER_CUTOFF_MAX_HZ,InstrumentMotionValue::cutoff),D("Reso",fxSRS2,255,InstrumentMotionValue::raw)};
static const InstrumentModDestination destBYOWTBL[] = {N,D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw),D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw),D("Detune",fxSDT,255,InstrumentMotionValue::raw),D("Mix",fxSMX,255,InstrumentMotionValue::raw),D("Index A",fxBIA,255,InstrumentMotionValue::raw),D("Index B",fxBIB,255,InstrumentMotionValue::raw),D("Cutoff",fxSCF2,FILTER_CUTOFF_MAX_HZ,InstrumentMotionValue::cutoff),D("Reso",fxSRS2,255,InstrumentMotionValue::raw)};
static const InstrumentModDestination destAChChid[] = {N,D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw),D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw),D("Cutoff",fxACF,FILTER_CUTOFF_MAX_HZ,InstrumentMotionValue::cutoff),D("Reso",fxARS,255,InstrumentMotionValue::raw),D("EnvMod",fxAEM,255,InstrumentMotionValue::raw),D("Decay",fxADC,255,InstrumentMotionValue::raw),D("Accent",fxAAC,255,InstrumentMotionValue::raw),D("Timbre",fxATM,16384,InstrumentMotionValue::raw),D("Color",fxACL,16384,InstrumentMotionValue::raw)};
static const InstrumentModDestination destDrumSynth[] = {N,D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw),D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw),D("Decay",fxDDC,255,InstrumentMotionValue::raw),D("Tone",fxDTO,255,InstrumentMotionValue::raw),D("Sweep",fxDSW,255,InstrumentMotionValue::raw),D("Noise",fxDNO,255,InstrumentMotionValue::raw),D("FM",fxDFM,255,InstrumentMotionValue::raw),D("Drive",fxDDR,255,InstrumentMotionValue::raw),D("Cutoff",fxDCF,FILTER_CUTOFF_MAX_HZ,InstrumentMotionValue::cutoff),D("Reso",fxDRS,255,InstrumentMotionValue::raw)};
static const InstrumentModDestination destMME[] = {N,D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw),D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw),D("Waves",fxMWV,255,InstrumentMotionValue::raw),D("Interval",fxMIN,255,InstrumentMotionValue::raw),D("Amount",fxMAM,255,InstrumentMotionValue::raw),D("Flow",fxMFL,255,InstrumentMotionValue::raw),D("Feedback",fxMFB,255,InstrumentMotionValue::raw),D("Shaper",fxMSH,255,InstrumentMotionValue::raw),D("Cutoff",fxMCF,FILTER_CUTOFF_MAX_HZ,InstrumentMotionValue::cutoff),D("Reso",fxMRS,255,InstrumentMotionValue::raw)};
static const InstrumentModDestination destSintered[] = {N,D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw),D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw),D("Decay",fxSDC,255,InstrumentMotionValue::raw),D("Mod",fxSMD,255,InstrumentMotionValue::raw),D("A",fxSA,255,InstrumentMotionValue::raw),D("B",fxSB,255,InstrumentMotionValue::raw),D("Motion",fxSMO,255,InstrumentMotionValue::raw),D("C",fxSC,255,InstrumentMotionValue::raw),D("Cutoff",fxSCF3,FILTER_CUTOFF_MAX_HZ,InstrumentMotionValue::cutoff),D("Reso",fxSRS3,255,InstrumentMotionValue::raw)};
static const InstrumentModDestination destMidi[] = {N};
static const InstrumentModDestination destOPLL[] = {N,D("Volume",instrumentNoFX,255,InstrumentMotionValue::raw),D("Pitch",instrumentNoFX,0,InstrumentMotionValue::raw)};
#undef N
#undef D
#define F(f, n) {(uint8_t)(f), n}
static const InstrumentFX fxAY1[]={F(fxAYM,"AYM"),F(fxNOI,"NOI"),F(fxNOA,"NOA"),F(fxERT,"ERT"),F(fxEAU,"EAU"),F(fxEVB,"EVB"),F(fxEBN,"EBN"),F(fxESL,"ESL"),F(fxENT,"ENT"),F(fxEPT,"EPT"),F(fxEPL,"EPL"),F(fxEPH,"EPH")};
static const InstrumentFX fxAY2[]={F(fxAYM,"AYM"),F(fxNOI,"NOI"),F(fxNOA,"NOA"),F(fxTNN,"TNN"),F(fxTNP,"TNP"),F(fxTNF,"TNF"),F(fxTRT,"TRT"),F(fxEAU,"EAU"),F(fxENN,"ENN"),F(fxENP,"ENP"),F(fxENF,"ENF"),F(fxERT,"ERT"),F(fxSFT,"SFT"),F(fxSFN,"SFN"),F(fxSFP,"SFP"),F(fxSFF,"SFF"),F(fxSRT,"SRT"),F(fxSFM,"SFM"),F(fxPWM,"PWM"),F(fxSPL,"SPL"),F(fxSWT,"SWT")};
static const InstrumentFX fxAYSample[]={F(fxAYM,"AYM"),F(fxNOI,"NOI"),F(fxNOA,"NOA"),F(fxTNN,"TNN"),F(fxTNP,"TNP"),F(fxTNF,"TNF"),F(fxTRT,"TRT"),F(fxSFN,"SFN"),F(fxSFP,"SFP"),F(fxSFF,"SFF"),F(fxSMS,"SMS")};
static const InstrumentFX fxBraids[]={F(fxBMD,"BMD"),F(fxBTM,"BTM"),F(fxBCL,"BCL"),F(fxBCF,"BCF"),F(fxBRS,"BRS")};
static const InstrumentFX fxSample[]={F(fxSLP,"SPL"),F(fxSLI,"SLI"),F(fxSPT,"SPT"),F(fxSST,"SST"),F(fxSTA,"STA"),F(fxSEN,"SEN"),F(fxSVL,"SVL"),F(fxSCF,"SCF"),F(fxSRS,"SRS"),F(fxSSP,"SSP")};
static const InstrumentFX fxSCWF[]={F(fxSDT,"SDT"),F(fxSMX,"SMX"),F(fxSCF2,"SCF"),F(fxSRS2,"SRS")};
static const InstrumentFX fxBYOWTBL[]={F(fxSDT,"SDT"),F(fxSMX,"SMX"),F(fxBIA,"BIA"),F(fxBIB,"BIB"),F(fxSCF2,"SCF"),F(fxSRS2,"SRS")};
static const InstrumentFX fxPlaits[]={F(fxPMD,"PMD"),F(fxPHA,"PHA"),F(fxPTM,"PTM"),F(fxPMO,"PMO"),F(fxPAX,"PAX"),F(fxPCF,"PCF"),F(fxPRS,"PRS")};
static const InstrumentFX fxAChChid[]={F(fxASL,"ASL"),F(fxATY,"ATY"),F(fxADC,"ADC"),F(fxAAC,"AAC"),F(fxATM,"ATM"),F(fxACL,"ACL"),F(fxACF,"ACF"),F(fxARS,"ARS"),F(fxAEM,"AEM")};
static const InstrumentFX fxDrumSynth[]={F(fxDMD,"DMD"),F(fxDDC,"DDC"),F(fxDTO,"DTO"),F(fxDSW,"DSW"),F(fxDNO,"DNO"),F(fxDFM,"DFM"),F(fxDDR,"DDR"),F(fxDCF,"DCF"),F(fxDRS,"DRS")};
static const InstrumentFX fxMME[]={F(fxMMD,"MMD"),F(fxMWV,"MWV"),F(fxMIN,"MIN"),F(fxMAM,"MAM"),F(fxMFL,"MFL"),F(fxMFB,"MFB"),F(fxMSH,"MSH"),F(fxMCF,"MCF"),F(fxMRS,"MRS")};
static const InstrumentFX fxSintered[]={F(fxSMDL,"SMD"),F(fxSDC,"SDC"),F(fxSMD,"SMP"),F(fxSA,"SMA"),F(fxSB,"SMB"),F(fxSMO,"SMO"),F(fxSC,"SMC"),F(fxSCF3,"SCF"),F(fxSRS3,"SRS")};
static const InstrumentFX fxMidi[]={F(fxMC1,"MC1"),F(fxMC2,"MC2"),F(fxMC3,"MC3"),F(fxMC4,"MC4")};
#undef F
#define COUNT(a) (uint8_t)(sizeof(a) / sizeof((a)[0]))
static const InstrumentDefinition instrumentDefinitions[] = {
  {"None",InstrumentCategory::none,InstrumentScreenKind::none,destNone,COUNT(destNone),NULL,0,{0,modNameNone,initNoneInstrument,freeNoneInstrument,0,0}},
  {"AY Classic",InstrumentCategory::chip,InstrumentScreenKind::ay1,destAY1,COUNT(destAY1),fxAY1,COUNT(fxAY1),{4,modNameAY1,initAY1Instrument,freeAY1Instrument,0,0}},
  {"AY Plus",InstrumentCategory::chip,InstrumentScreenKind::ay2,destAY2,COUNT(destAY2),fxAY2,COUNT(fxAY2),{10,modNameAY2,initAY2Instrument,freeAY2Instrument,0,0}},
  {"AY Sample",InstrumentCategory::chip,InstrumentScreenKind::aySample,destAYSample,COUNT(destAYSample),fxAYSample,COUNT(fxAYSample),{6,modNameAYSample,initAYSampleInstrument,freeAYSampleInstrument,0,0}},
  {"Braids",InstrumentCategory::synth,InstrumentScreenKind::braids,destBraids,COUNT(destBraids),fxBraids,COUNT(fxBraids),{6,modNameBraids,initBraidsInstrument,freeBraidsInstrument,1,0}},
  {"Sampler",InstrumentCategory::sample,InstrumentScreenKind::sample,destSample,COUNT(destSample),fxSample,COUNT(fxSample),{8,modNameSample,initSampleInstrument,freeSampleInstrument,1,0}},
  {"Plaits",InstrumentCategory::synth,InstrumentScreenKind::plaits,destPlaits,COUNT(destPlaits),fxPlaits,COUNT(fxPlaits),{8,modNamePlaits,initPlaitsInstrument,freePlaitsInstrument,1,1}},
  {"Plaits-Alt",InstrumentCategory::synth,InstrumentScreenKind::plaits,destPlaits,COUNT(destPlaits),fxPlaits,COUNT(fxPlaits),{8,modNamePlaits,initPlaitsAltInstrument,freePlaitsInstrument,1,1}},
  {"2xSCWF",InstrumentCategory::sample,InstrumentScreenKind::scwf,destSCWF,COUNT(destSCWF),fxSCWF,COUNT(fxSCWF),{6,modNameSCWF,initSCWFInstrument,freeSCWFInstrument,1,0}},
  {"BYOWTBL",InstrumentCategory::sample,InstrumentScreenKind::byowtbl,destBYOWTBL,COUNT(destBYOWTBL),fxBYOWTBL,COUNT(fxBYOWTBL),{8,modNameBYOWTBL,initBYOWTBLInstrument,freeBYOWTBLInstrument,1,0}},
  {"aChChid",InstrumentCategory::synth,InstrumentScreenKind::achchid,destAChChid,COUNT(destAChChid),fxAChChid,COUNT(fxAChChid),{9,modNameAChChid,initAChChidInstrument,freeAChChidInstrument,0,0}},
  {"Bogie",InstrumentCategory::drums,InstrumentScreenKind::drumSynth,destDrumSynth,COUNT(destDrumSynth),fxDrumSynth,COUNT(fxDrumSynth),{10,modNameDrumSynth,initDrumSynthInstrument,freeDrumSynthInstrument,0,0}},
  {"MME",InstrumentCategory::synth,InstrumentScreenKind::mme,destMME,COUNT(destMME),fxMME,COUNT(fxMME),{10,modNameMME,initMMEInstrument,freeMMEInstrument,1,1}},
  {"Sintered",InstrumentCategory::drums,InstrumentScreenKind::sintered,destSintered,COUNT(destSintered),fxSintered,COUNT(fxSintered),{10,modNameSintered,initSinteredInstrument,freeSinteredInstrument,0,0}},
  // Keep the retired PD slots so existing MIDI instruments retain their on-disk type ID.
  {"Retired",InstrumentCategory::none,InstrumentScreenKind::none,destNone,COUNT(destNone),NULL,0,{0,modNameNone,initNoneInstrument,freeNoneInstrument,0,0}},
  {"Retired",InstrumentCategory::none,InstrumentScreenKind::none,destNone,COUNT(destNone),NULL,0,{0,modNameNone,initNoneInstrument,freeNoneInstrument,0,0}},
  {"MIDI Out",InstrumentCategory::midi,InstrumentScreenKind::midi,destMidi,COUNT(destMidi),fxMidi,COUNT(fxMidi),{0,modNameMidi,initMidiInstrument,freeMidiInstrument,0,0}},
  {"OPLL / MSX",InstrumentCategory::fm,InstrumentScreenKind::opl,destOPLL,COUNT(destOPLL),NULL,0,{2,modNameOPLL,initOPLLInstrument,freeNoneInstrument,1,0}},
  {"VRC7",InstrumentCategory::fm,InstrumentScreenKind::opl,destOPLL,COUNT(destOPLL),NULL,0,{2,modNameOPLL,initVRC7Instrument,freeNoneInstrument,1,0}},
  {"AdLib / OPL2",InstrumentCategory::fm,InstrumentScreenKind::opl,destOPLL,COUNT(destOPLL),NULL,0,{2,modNameOPLL,initOPL2Instrument,freeNoneInstrument,1,0}},
  {"OPL3",InstrumentCategory::fm,InstrumentScreenKind::opl,destOPLL,COUNT(destOPLL),NULL,0,{2,modNameOPLL,initOPL3Instrument,freeNoneInstrument,1,0}},
  {"Sega PSG",InstrumentCategory::chip,InstrumentScreenKind::simpleChip,destOPLL,COUNT(destOPLL),NULL,0,{2,modNameOPLL,initSegaInstrument,freeNoneInstrument,1,0}},
  {"GB Pulse",InstrumentCategory::chip,InstrumentScreenKind::simpleChip,destOPLL,COUNT(destOPLL),NULL,0,{2,modNameOPLL,initGBPulseInstrument,freeNoneInstrument,1,0}},
  {"GB Noise",InstrumentCategory::chip,InstrumentScreenKind::simpleChip,destOPLL,COUNT(destOPLL),NULL,0,{2,modNameOPLL,initGBNoiseInstrument,freeNoneInstrument,1,0}},
  {"DX7 FM",InstrumentCategory::fm,InstrumentScreenKind::dx7,destOPLL,COUNT(destOPLL),NULL,0,{2,modNameOPLL,initDX7Instrument,freeNoneInstrument,1,0}},
  {"Genesis FM",InstrumentCategory::fm,InstrumentScreenKind::opl,destOPLL,COUNT(destOPLL),NULL,0,{2,modNameOPLL,initGenesisInstrument,freeNoneInstrument,1,0}},
  {"Arcade FM",InstrumentCategory::fm,InstrumentScreenKind::opl,destOPLL,COUNT(destOPLL),NULL,0,{2,modNameOPLL,initArcadeInstrument,freeNoneInstrument,1,0}},
  {"SID",InstrumentCategory::chip,InstrumentScreenKind::opl,destOPLL,COUNT(destOPLL),NULL,0,{2,modNameOPLL,initSIDInstrument,freeNoneInstrument,0,0}},
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
  if (const auto* native=instrumentNativeModDestination(type,instrumentGenericModDestination(type,destination))) return native;
  const InstrumentDefinition* definition = getInstrumentDefinition(type);
  return destination >= 0 && destination < definition->destinationCount ? &definition->destinations[destination] : NULL;
}

// Phrase/table controls are intentionally smaller than live modulation targets.
static bool trackerDirectFMAvailable(InstrumentType type, int fx) {
  switch(type) {
    case InstrumentType::OPLL: case InstrumentType::VRC7:
    case InstrumentType::OPL2: case InstrumentType::OPL3:
      return fx==fxOAR || fx==fxODR || fx==fxORR || fx==fxOSL || fx==fxOMU;
    case InstrumentType::GenesisFM: case InstrumentType::ArcadeFM:
      return fx==fxOMU || (fx>=fxLFR && fx<=fxLEN);
    default: return false;
  }
}

int instrumentFXAvailable(InstrumentType type, uint8_t fx) {
  if(fx>=fxOAR&&fx<=fxLEN) { if(!trackerDirectFMAvailable(type,fx))return 0; Instrument i{}; getInstrumentFunctions(type).init(&i); NativeFXInfo info{}; return instrumentDirectFMInfo(&i,fx,&info); }
  if(fx==fxFBK)return instrumentNativeModDestination(type,genericModFMFeedback)!=nullptr;
  if(fx>=fxOL1&&fx<=fxOL6) {
    int count=type==InstrumentType::DX7?6:(type==InstrumentType::OPL3||type==InstrumentType::GenesisFM||type==InstrumentType::ArcadeFM)?4:(type==InstrumentType::OPLL||type==InstrumentType::VRC7||type==InstrumentType::OPL2)?2:0;
    return fx-fxOL1<count;
  }
  for(int g=genericModFMBrightness;g<genericModTotalCount;++g)if(const auto* d=instrumentNativeModDestination(type,g))if(d->fx==fx)return 1;
  const InstrumentDefinition* definition = getInstrumentDefinition(type);
  for (int i = 0; i < definition->fxCount; ++i) if (definition->fxList[i].fx == fx) return 1;
  return 0;
}

int instrumentFXAvailableForInstrument(const Instrument* instrument, uint8_t fx) {
  if(instrument && fx>=fxOAR&&fx<=fxLEN) { if(!trackerDirectFMAvailable(instrument->type,fx))return 0; NativeFXInfo info{}; return instrumentDirectFMInfo(instrument,fx,&info); }
  if (!instrument || !instrumentFXAvailable(instrument->type, fx)) return 0;
  if(fx>=fxOL1&&fx<=fxOL6)return fx-fxOL1<instrumentFMOperatorCount(instrument);
  if((instrument->type==InstrumentType::OPL2||instrument->type==InstrumentType::OPL3)&&
      instrument->chip.opl.topology==OPLTopology::twoOperator&&(fx==fxFO3||fx==fxFO4))return 0;
  if (instrument->type != InstrumentType::DrumSynth) return 1;
  int macro = fx == fxDDC ? 0 : fx == fxDTO ? 1 : fx == fxDSW ? 2 :
    fx == fxDNO ? 3 : fx == fxDFM ? 4 : fx == fxDDR ? 5 : -1;
  return macro < 0 || drumSynthMacroUsed(instrument->chip.drumSynth.engine, macro);
}

int instrumentModDestinationAvailable(const Instrument* instrument, int destination) {
  InstrumentType type = instrument ? instrument->type : InstrumentType::none;
  int generic = instrumentGenericModDestination(type, destination);
  if (instrument && isOPL(type) && instrument->chip.opl.topology==OPLTopology::twoOperator && generic>=genericModFMOperator3 && generic<=genericModFMOperator6) return 0;
  int fx,op;
  if(nativeFMModTarget(generic,&fx,&op)) { NativeFXInfo info{};return instrument && instrumentNativeFXInfo(instrument,fx,&info,op); }
  if(generic==genericModFMBrightness||(generic>=genericModFMTime&&generic<=genericModFMLFODepth))return 0;
  if (generic == genericModInstrumentPan || generic == genericModTrackPan)
    return instrument && instrument->type != InstrumentType::Midi;
  if (generic >= genericModFMBrightness) return instrumentNativeModDestination(type,generic)!=nullptr;
  if (generic >= genericModFirstInsert) return 1;
  if (generic >= 0) {
    auto f = getInstrumentFunctions(type);
    return generic < genericModDestinationCount || generic >= genericModFirstP5 ||
      (f.supportsVoicePost && (generic < genericModTriggerDecay || f.supportsTrigger));
  }
  if (!instrumentModDestination(type, destination)) return 0;
  return !instrument || instrument->type != InstrumentType::DrumSynth ||
    destination < 3 || destination > 8 || drumSynthMacroUsed(instrument->chip.drumSynth.engine, destination - 3);
}

InstrumentFMAmp* instrumentFMAmpSettings(Instrument* i) {
  switch (i->type) {
    case InstrumentType::OPLL: case InstrumentType::VRC7: return &i->chip.opll.amp;
    case InstrumentType::OPL2: case InstrumentType::OPL3: return &i->chip.opl.amp;
    case InstrumentType::GenesisFM: case InstrumentType::ArcadeFM: return &i->chip.fourOp.amp;
    case InstrumentType::DX7: return &i->chip.dx7.amp;
    default: return nullptr;
  }
}

InstrumentVoicePostSettings* instrumentVoicePostSettings(Instrument* instrument) {
  if (auto* amp = instrumentFMAmpSettings(instrument)) return amp;
  switch (instrument->type) {
    case InstrumentType::SegaPSG:
    case InstrumentType::GBPulse:
    case InstrumentType::GBNoise: return &instrument->chip.simpleChip;
    case InstrumentType::Braids: return &instrument->chip.braids;
    case InstrumentType::Sample: return &instrument->chip.sample;
    case InstrumentType::SCWF: return &instrument->chip.scwf;
    case InstrumentType::BYOWTBL: return &instrument->chip.byowtbl;
    case InstrumentType::Plaits:
    case InstrumentType::PlaitsAlt: return &instrument->chip.plaits;
    case InstrumentType::DrumSynth: return &instrument->chip.drumSynth;
    case InstrumentType::MME: return &instrument->chip.mme;
    case InstrumentType::Sintered: return &instrument->chip.sintered;
    default: return NULL;
  }
}

int instrumentMotionDestination(const Instrument* instrument, int destination, uint8_t* fx, int* base, int* range, InstrumentMotionValue* value) {
  if(!instrument || !instrumentModDestinationAvailable(instrument,destination))return 0;
  int generic = instrumentGenericModDestination(instrument->type, destination);
  if (generic == genericModInstrumentPan || generic == genericModTrackPan) {
    *fx = generic == genericModInstrumentPan ? fxPAN : fxTPN;
    *base = generic == genericModInstrumentPan ? instrument->pan : 128;
    *range = 255; *value = InstrumentMotionValue::raw;
    return 1;
  }
  const InstrumentModDestination* definition = instrumentModDestination(instrument->type, destination);
  if (!definition || definition->fx == instrumentNoFX) return 0;
  *fx = definition->fx; *range = definition->range; *value = definition->value;
  generic=instrumentGenericModDestination(instrument->type,destination);
  if(generic>=genericModFMBrightness){*base=instrumentNativeControlValue(instrument,generic);return 1;}
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
    case InstrumentType::AChChid:
      *base = destination == 3 ? instrument->chip.achchid.cutoff : destination == 4 ? instrument->chip.achchid.resonance * 255 / 100 : destination == 5 ? instrument->chip.achchid.envMod * 255 / 100 : destination == 6 ? instrument->chip.achchid.decay * 255 / 2000 : destination == 7 ? instrument->chip.achchid.accent * 255 / 100 : destination == 8 ? (instrument->chip.achchid.timbre + 64) / 129 : (instrument->chip.achchid.color + 64) / 129; break;
    case InstrumentType::DrumSynth:
      *base = destination == 3 ? instrument->chip.drumSynth.decay : destination == 4 ? instrument->chip.drumSynth.tone : destination == 5 ? instrument->chip.drumSynth.sweep : destination == 6 ? instrument->chip.drumSynth.noise : destination == 7 ? instrument->chip.drumSynth.fm : destination == 8 ? instrument->chip.drumSynth.drive : destination == 9 ? instrument->chip.drumSynth.filterCutoffHz : instrument->chip.drumSynth.filterResonance; break;
    case InstrumentType::MME:
      *base = destination == 3 ? instrument->chip.mme.waves : destination == 4 ? instrument->chip.mme.interval : destination == 5 ? instrument->chip.mme.amount : destination == 6 ? instrument->chip.mme.flow : destination == 7 ? instrument->chip.mme.feedback : destination == 8 ? instrument->chip.mme.shaper : destination == 9 ? instrument->chip.mme.filterCutoffHz : instrument->chip.mme.filterResonance; break;
    case InstrumentType::Sintered:
      *base = destination == 3 ? instrument->chip.sintered.decay : destination == 4 ? instrument->chip.sintered.mod : destination == 5 ? instrument->chip.sintered.a : destination == 6 ? instrument->chip.sintered.b : destination == 7 ? instrument->chip.sintered.motion : destination == 8 ? instrument->chip.sintered.c : destination == 9 ? instrument->chip.sintered.filterCutoffHz : instrument->chip.sintered.filterResonance; break;
    default: return 0;
  }
  return 1;
}

static int ccScale(uint8_t value, int maximum) {
  return ((int)value * maximum + 63) / 127;
}

static int ccSigned(uint8_t value) {
  return ((int)value * 255 + 63) / 127 - 128;
}

int instrumentCCDestinationAvailable(const Instrument* instrument, int destination) {
  if (!instrument || destination == midiCCDestinationNone) return 0;
  if (destination >= midiCCDestinationAttack && destination <= midiCCDestinationRelease)
    return instrumentVoicePostSettings(const_cast<Instrument*>(instrument)) != NULL;
  if (destination == 1) return 1; // Volume is shared by every instrument.
  if (destination < 3) return 0;
  const InstrumentModDestination* d = instrumentModDestination(instrument->type, destination);
  if (!d || (!d->range && instrument->type != InstrumentType::AY2 &&
             instrument->type != InstrumentType::AYSample)) return 0;
  if (!instrumentModDestinationAvailable(instrument, destination)) return 0;
  return strcmp(d->name, "Loop") && strncmp(d->name, "Index", 5);
}

int instrumentCCDestinationValue(const Instrument* instrument, int destination, uint8_t cc) {
  if (!instrumentCCDestinationAvailable(instrument, destination)) return 0;
  if (destination >= midiCCDestinationAttack && destination <= midiCCDestinationRelease) return ccScale(cc, 255);
  const InstrumentModDestination* d = instrumentModDestination(instrument->type, destination);
  if (d->value == InstrumentMotionValue::cutoff) return (int)filterCutoffFromControl((uint8_t)ccScale(cc, 255));
  if (d->range == 16384) return ccScale(cc, 32767);
  if (d->value == InstrumentMotionValue::speed) return ccScale(cc, 500);
  return ccScale(cc, 255);
}

int instrumentSetCCDestination(Instrument* instrument, int destination, uint8_t cc) {
  if (!instrumentCCDestinationAvailable(instrument, destination)) return 0;
  if (destination == 1) { instrument->volume = (uint8_t)ccScale(cc, 255); return 1; }
  if (destination >= midiCCDestinationAttack && destination <= midiCCDestinationRelease) {
    InstrumentVoicePostSettings* post = instrumentVoicePostSettings(instrument);
    uint8_t value = (uint8_t)ccScale(cc, 255);
    if (destination == midiCCDestinationAttack) post->attack = value;
    else if (destination == midiCCDestinationDecay) post->decay = value;
    else if (destination == midiCCDestinationSustain) post->sustain = value;
    else post->release = value;
    return 1;
  }
  int value = instrumentCCDestinationValue(instrument, destination, cc);
  switch (instrument->type) {
    case InstrumentType::AY2:
      if (destination == 3) instrument->chip.ay2.oscTone.fineTune = ccSigned(cc);
      else if (destination == 4) instrument->chip.ay2.oscNoise.noisePeriod = value;
      else if (destination == 5) instrument->chip.ay2.oscEnvelope.fineTune = ccSigned(cc);
      else if (destination == 6) instrument->chip.ay2.oscSoftware.fineTune = ccSigned(cc);
      else if (destination == 7) instrument->chip.ay2.oscSoftware.fmDepth = value;
      else if (destination == 8) instrument->chip.ay2.oscSoftware.pulseWidth = value;
      else if (destination == 9) instrument->chip.ay2.oscSoftware.pulseLow = ccScale(cc, 15);
      else if (destination == 10) instrument->chip.ay2.oscSoftware.wavetableIndex = value;
      else return 0;
      break;
    case InstrumentType::AYSample:
      if (destination == 3) instrument->chip.aySample.pitchOffset = ccSigned(cc);
      else if (destination == 4) instrument->chip.aySample.oscTone.fineTune = ccSigned(cc);
      else if (destination == 5) instrument->chip.aySample.oscNoise.noisePeriod = value;
      else return 0;
      break;
    case InstrumentType::Sample:
      if (destination == 3) instrument->chip.sample.start = value;
      else if (destination == 4) instrument->chip.sample.end = value;
      else if (destination == 5) instrument->chip.sample.speedPercent = ccScale(cc, 500);
      else if (destination == 7) instrument->chip.sample.filterCutoffHz = value;
      else if (destination == 8) instrument->chip.sample.filterResonance = value;
      else return 0;
      break;
    case InstrumentType::Braids:
      if (destination == 3) instrument->chip.braids.timbre = value;
      else if (destination == 4) instrument->chip.braids.color = value;
      else if (destination == 5) instrument->chip.braids.filterCutoffHz = value;
      else if (destination == 6) instrument->chip.braids.filterResonance = value;
      else return 0;
      break;
    case InstrumentType::Plaits: case InstrumentType::PlaitsAlt:
      if (destination == 3) instrument->chip.plaits.harmonics = value;
      else if (destination == 4) instrument->chip.plaits.timbre = value;
      else if (destination == 5) instrument->chip.plaits.morph = value;
      else if (destination == 6) instrument->chip.plaits.auxMix = value;
      else if (destination == 7) instrument->chip.plaits.filterCutoffHz = value;
      else if (destination == 8) instrument->chip.plaits.filterResonance = value;
      else return 0;
      break;
    case InstrumentType::SCWF:
      if (destination == 3) instrument->chip.scwf.detune = value;
      else if (destination == 4) instrument->chip.scwf.mix = value;
      else if (destination == 5) instrument->chip.scwf.filterCutoffHz = value;
      else if (destination == 6) instrument->chip.scwf.filterResonance = value;
      else return 0;
      break;
    case InstrumentType::BYOWTBL:
      if (destination == 3) instrument->chip.byowtbl.detune = value;
      else if (destination == 4) instrument->chip.byowtbl.mix = value;
      else if (destination == 5) instrument->chip.byowtbl.frameIndex[0] = value;
      else if (destination == 6) instrument->chip.byowtbl.frameIndex[1] = value;
      else if (destination == 7) instrument->chip.byowtbl.filterCutoffHz = value;
      else if (destination == 8) instrument->chip.byowtbl.filterResonance = value;
      else return 0;
      break;
    case InstrumentType::AChChid:
      if (destination == 3) instrument->chip.achchid.cutoff = value;
      else if (destination == 4) instrument->chip.achchid.resonance = ccScale(cc, 100);
      else if (destination == 5) instrument->chip.achchid.envMod = ccScale(cc, 100);
      else if (destination == 6) instrument->chip.achchid.decay = ccScale(cc, 2000);
      else if (destination == 7) instrument->chip.achchid.accent = ccScale(cc, 100);
      else if (destination == 8) instrument->chip.achchid.timbre = value;
      else if (destination == 9) instrument->chip.achchid.color = value;
      else return 0;
      break;
    case InstrumentType::DrumSynth:
      if (destination == 3) instrument->chip.drumSynth.decay = value;
      else if (destination == 4) instrument->chip.drumSynth.tone = value;
      else if (destination == 5) instrument->chip.drumSynth.sweep = value;
      else if (destination == 6) instrument->chip.drumSynth.noise = value;
      else if (destination == 7) instrument->chip.drumSynth.fm = value;
      else if (destination == 8) instrument->chip.drumSynth.drive = value;
      else if (destination == 9) instrument->chip.drumSynth.filterCutoffHz = value;
      else if (destination == 10) instrument->chip.drumSynth.filterResonance = value;
      else return 0;
      break;
    case InstrumentType::MME:
      if (destination == 3) instrument->chip.mme.waves = value;
      else if (destination == 4) instrument->chip.mme.interval = value;
      else if (destination == 5) instrument->chip.mme.amount = value;
      else if (destination == 6) instrument->chip.mme.flow = value;
      else if (destination == 7) instrument->chip.mme.feedback = value;
      else if (destination == 8) instrument->chip.mme.shaper = value;
      else if (destination == 9) instrument->chip.mme.filterCutoffHz = value;
      else if (destination == 10) instrument->chip.mme.filterResonance = value;
      else return 0;
      break;
    case InstrumentType::Sintered:
      if (destination == 3) instrument->chip.sintered.decay = value;
      else if (destination == 4) instrument->chip.sintered.mod = value;
      else if (destination == 5) instrument->chip.sintered.a = value;
      else if (destination == 6) instrument->chip.sintered.b = value;
      else if (destination == 7) instrument->chip.sintered.motion = value;
      else if (destination == 8) instrument->chip.sintered.c = value;
      else if (destination == 9) instrument->chip.sintered.filterCutoffHz = value;
      else if (destination == 10) instrument->chip.sintered.filterResonance = value;
      else return 0;
      break;
    default: return 0;
  }
  return 1;
}

static const char* genericModName(int index) {
  if (index == genericModInstrumentPan) return "PAN";
  if (index == genericModTrackPan) return "Track Pan";
  static const char* names[] = {
    "RevSend", "DlySend",
    "M1 P1", "M1 P2", "M1 P3", "M1 P4",
    "M2 P1", "M2 P2", "M2 P3", "M2 P4",
    "M3 P1", "M3 P2", "M3 P3", "M3 P4",
    "M4 P1", "M4 P2", "M4 P3", "M4 P4",
    "ADSR A", "ADSR D", "ADSR S", "ADSR R", "ADSR Shape", "Trig D", "Trig C",
    "M1 P5", "M2 P5", "M3 P5", "M4 P5",
    "F11", "F12", "F13", "F14", "F15", "F16", "F17", "F18", "F21", "F22", "F23", "F24", "F25", "F26", "F27", "F28"
  };
  return index >= 0 && index < int(sizeof(names)/sizeof(*names)) ? names[index] : "Misc";
}

int instrumentGenericModDestination(InstrumentType type, int destination) {
  int index = destination - getInstrumentFunctions(type).modDestinationsCount - 1;
  return index >= 0 && index < genericModTotalCount ? index : -1;
}

int instrumentModDestinationMax(InstrumentType type) {
  InstrumentFunctions functions = getInstrumentFunctions(type);
  for(int g=genericModTotalCount-1;g>=genericModFMBrightness;--g)
    if(instrumentNativeModDestination(type,g))return functions.modDestinationsCount+1+g;
  return functions.modDestinationsCount + genericModFMBrightness;
}

const char* instrumentModDestinationName(InstrumentType type, int destination) {
  if (destination == midiCCDestinationNone) return "-";
  if (destination == midiCCDestinationAttack) return "Attack";
  if (destination == midiCCDestinationDecay) return "Decay";
  if (destination == midiCCDestinationSustain) return "Sustain";
  if (destination == midiCCDestinationRelease) return "Release";
  const InstrumentModDestination* definition = instrumentModDestination(type, destination);
  if (definition) return definition->name;
  return genericModName(instrumentGenericModDestination(type, destination));
}

const char* instrumentModDestinationNameForInstrument(const Instrument* instrument, int destination) {
  if (instrument && destination >= 3 && destination <= 8) {
    if (instrument->type == InstrumentType::MME) {
      static const char* sync[] = {"Waves", "Interval", "SyncAmt", "Reset", "Feedback", "Shaper"};
      static const char* logic[] = {"Waves", "Interval", "Amount", "Logic", "Feedback", "Shaper"};
      static const char* vocode[] = {"Waves", "Interval", "Analyze", "Formant", "Feedback", "Shaper"};
      static const char* ring[] = {"Waves", "Interval", "Amount", "RingType", "Feedback", "Shaper"};
      const char* const* names = instrument->chip.mme.model == MMEModel::sync ? sync :
        instrument->chip.mme.model == MMEModel::logic ? logic :
        instrument->chip.mme.model == MMEModel::vocode ? vocode :
        instrument->chip.mme.model == MMEModel::ring ? ring : NULL;
      if (names) return names[destination - 3];
    } else if (instrument->type == InstrumentType::Sintered) {
      static const char* names[][6] = {
        {"Decay", "Mod", "Ratio", "Spread", "Motion", "Fold"},
        {"Decay", "Mod", "Ratio", "Feedback", "Motion", "Bite"},
        {"Decay", "Mod", "Noise", "Color", "Motion", "Feedback"},
        {"Decay", "Mod", "Time", "Damping", "Motion", "Regen"},
        {"Decay", "Mod", "Rate", "Pattern", "Motion", "Crush"},
        {"Decay", "Mod", "Ratio", "Chaos", "Motion", "Drive"}
      };
      int model = (int)instrument->chip.sintered.model;
      return names[model >= 0 && model < (int)SinteredModel::totalCount ? model : 0][destination - 3];
    }
  }
  return instrumentModDestinationName(instrument ? instrument->type : InstrumentType::none, destination);
}
