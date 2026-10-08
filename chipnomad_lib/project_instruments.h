#ifndef __CHIPNOMAD_LIB__PROJECT_INSTRUMENTS_H__
#define __CHIPNOMAD_LIB__PROJECT_INSTRUMENTS_H__

#include <stdlib.h>
#include <stdint.h>
#include "project_constants.h"

// Forward declarations
struct Project;

// Instruments

enum class InstrumentType : uint8_t {
  none = 0,
  AY1 = 1,
  AY2 = 2,
  AYSample = 3,
  Braids = 4,
  Sample = 5,
  Plaits = 6,
  PlaitsAlt = 7,
  SCWF = 8,
  BYOWTBL = 9,
  AChChid = 10,
  DrumSynth = 11,
  MME = 12,
  Sintered = 13,
  Midi = 16,
  OPLL = 17,
  VRC7 = 18,
  OPL2 = 19,
  OPL3 = 20,
  SegaPSG = 21,
  GBPulse = 22,
  GBNoise = 23,
  DX7 = 24,
  GenesisFM = 25,
  ArcadeFM = 26,
  SID = 27,
  totalCount,
};

enum class ModulationType : uint8_t {
  ADSR = 0,
  AHD = 1,
  LFO = 2,
  SLFO = 3,
  FLFO = 4,
  StickLinear = 5,
  StickVelocity = 6, // Legacy project value, no longer exposed.
  StickRate = 7,
  totalCount,
};

enum class StickAxis : uint8_t {
  leftVertical = 0,
  leftHorizontal = 1,
  rightVertical = 2,
  rightHorizontal = 3,
  totalCount,
};

enum class LFOShape : uint8_t {
  tri = 0,
  sin = 1,
  uniTri = 2,
  uniSin = 3,
  rampDown = 4,
  rampUp = 5,
  expDown = 6,
  expUp = 7,
  square = 8,
  random = 9,
  wavetable = 10,
  totalCount,
};

enum class LFOTrigger : uint8_t {
  free = 0,
  retrig = 1,
  hold = 2,
  once = 3,
  phrase = 4,
  chain = 5,
  totalCount,
};

struct Modulation {
  ModulationType type;
  uint8_t destination;
  int8_t amount;
  uint8_t p1; // ADSR: A, AHD: A, LFO: Shape
  uint8_t p2; // ADSR: D, AHD: H, LFO: Trig
  uint8_t p3; // ADSR: S, AHD: D, LFO: Period
  uint8_t p4; // ADSR: R, AHD: -, LFO: -, SLFO: multiplier
  uint8_t p5; // LFO/SLFO/FLFO Wavetable: AY wavetable index
};

// AY Instruments

struct InstrumentAY1 {
  Modulation volumeEnvelope;  // ADSR envelope as modulation
  uint8_t autoEnvN; // 0 - no auto-env
  uint8_t autoEnvD;
  uint8_t defaultMixer; // Low nibble: mixer, high nibble: envelope shape
};

struct InstrumentAYOscTone {
  uint8_t isOn;
  uint8_t pitchFlag;
  int8_t pitchOffset;
  int8_t fineTune;
};

struct InstrumentAYOscNoise {
  uint8_t isOn;
  uint8_t noisePeriod;
};

struct InstrumentAYOscEnvelope {
  uint8_t shape;
  uint8_t autoEnvN;
  uint8_t autoEnvD;
  uint8_t pitchFlag;
  int8_t pitchOffset;
  int8_t fineTune;
};

enum class AYSoftwareOscType : uint8_t {
  none = 0,
  pulse = 1,
  syncTone = 2,
  syncEnvelope = 3,
  wavetable = 4,
  toneFM = 5,
  envFM = 6,
  sample = 7, // Needs to be last for various conditions for AY2 instrument
  totalCount,
};

struct InstrumentAYOscSoftware {
  AYSoftwareOscType type;
  uint8_t pitchFlag;
  int8_t pitchOffset;
  int8_t fineTune;
  uint8_t pulseWidth;
  uint8_t pulseLow;
  uint8_t wavetableIndex;
  uint8_t fmDepth;
  uint8_t envShapePair; // For SyncEnv: high nibble = shape 1, low nibble = shape 2, default 0x00
};

struct InstrumentAY2 {
  InstrumentAYOscTone oscTone;
  InstrumentAYOscNoise oscNoise;
  InstrumentAYOscEnvelope oscEnvelope;
  InstrumentAYOscSoftware oscSoftware;
};

struct InstrumentAYSample {
  InstrumentAYOscTone oscTone;
  InstrumentAYOscNoise oscNoise;
  char sampleName[PROJECT_INSTRUMENT_NAME_LENGTH + 1];
  uint16_t fileLength;
  uint16_t sampleRate;
  uint16_t sampleStart;
  uint16_t sampleLength;
  uint16_t sampleLoopStart;
  uint8_t *sampleData;  // 8-bit unsigned PCM data
  int8_t pitchOffset;
  int8_t fineTune;
};

struct InstrumentVoicePostSettings {
  uint8_t filterEnabled;
  uint8_t filterCharacter; // 0: off, 1: clean, 2: classic, 3: aggressive, 4: acid
  uint8_t filterMode;
  uint8_t filterSlope24dB;
  uint16_t filterCutoffHz;
  uint8_t filterResonance;
  uint8_t attack;
  uint8_t decay;
  uint8_t sustain;
  uint8_t release;
  uint8_t envelopeShape;
};

struct InstrumentBraids : InstrumentVoicePostSettings {
  uint8_t model;
  uint16_t timbre;
  uint16_t color;
};

enum class AChChidWave : uint8_t { square, saw, braids };
struct InstrumentAChChid {
  AChChidWave wave;
  int8_t fineTune;
  uint8_t model;
  uint16_t timbre;
  uint16_t color;
  uint8_t saturation;
  uint16_t cutoff;
  uint8_t resonance;
  uint8_t envMod;
  uint16_t decay;
  uint8_t accent;
};

enum class DrumSynthEngine : uint8_t { kick, snare, hat, clap, tom, rim, fm, noise, cowbell, cymbal, shaker, clave, totalCount };

struct InstrumentDrumSynth : InstrumentVoicePostSettings {
  DrumSynthEngine engine;
  uint8_t decay;
  uint8_t tone;
  uint8_t sweep;
  uint8_t noise;
  uint8_t fm;
  uint8_t drive;
};

// Multi Modulation Engine: two internal oscillators coupled by a selectable
// cross-modulation algorithm.  The six controls deliberately have a stable
// position in the UI even though their musical interpretation varies by model.
enum class MMEModel : uint8_t { ring, fold, cross, vpm, sync, logic, vocode, totalCount };
struct InstrumentMME : InstrumentVoicePostSettings {
  MMEModel model;
  uint8_t waves;
  uint8_t interval;
  uint8_t amount;
  uint8_t flow;
  uint8_t feedback;
  uint8_t shaper;
};

enum class SinteredModel : uint8_t { knot, shard, burst, comb, logic, melt, totalCount };
struct InstrumentSintered : InstrumentVoicePostSettings {
  SinteredModel model;
  uint8_t decay;
  uint8_t mod;
  uint8_t a;
  uint8_t b;
  uint8_t motion;
  uint8_t c;
};

struct InstrumentPlaits : InstrumentVoicePostSettings {
  uint8_t engine;
  uint16_t harmonics;
  uint16_t timbre;
  uint16_t morph;
  uint8_t auxMix;
  uint8_t envelopeMode; // 0: TRIG/LPG, 2: post-VCA ADSR (1 loads as legacy VCA)
};

#define PROJECT_SAMPLE_PATH_LENGTH 255

// Maximum number of slices per sample (all slice modes, plan decision D7).
#define PROJECT_SAMPLE_MAX_SLICES 64

struct InstrumentSample : InstrumentVoicePostSettings {
  char path[PROJECT_SAMPLE_PATH_LENGTH + 1];
  uint32_t sampleRate;
  uint32_t frameCount;
  uint8_t channels;
  int16_t* data;
  int8_t pitch;
  uint16_t speedPercent;
  uint8_t start;
  uint8_t end;
  uint8_t loopMode; // 0: off, 1: loop, 2: ping-pong
  // Slice sentinel (see sample_voice.h): 0 off, 1..64 EQUAL, 65..128 AUTO,
  // 129..192 LAZY (value - 64/128 is the slice count).
  uint8_t slice;
  uint8_t stretchMode; // 0: off, 1: 1 beat, 2: 2 beats, 3: 1 bar, 4: 2 bars, 5: 4 bars, 6: 8 bars
  uint8_t speedAlgorithm; // 0: dirty granular playback, 1: clean Signalsmith stretch
  uint8_t autoSensitivity; // AUTO transient detection strength, 1..99 (50 = default)
  // Manual slice boundaries in frames (start of each slice; the last slice
  // ends at the loop end marker). Empty (all zero) = even/auto placement.
  uint32_t sliceBounds[PROJECT_SAMPLE_MAX_SLICES];
};

// Playback window mapping: Start/End are stored as 0-255 normalized values;
// these convert them to absolute frame positions. Shared by the sample
// voice, the stretch processor and the sample editor screen - do not fork
// the formulas.
static inline uint32_t sampleMarkerToStartFrame(uint32_t frameCount, uint8_t start) {
  return frameCount ? (uint32_t)((uint64_t)start * (frameCount - 1) / 255) : 0;
}

static inline uint32_t sampleMarkerToEndFrame(uint32_t frameCount, uint8_t end) {
  return end == 255 ? frameCount : (uint32_t)((uint64_t)(end + 1) * frameCount / 256);
}

// 2xSCWF is a pair of forward-looping, one-cycle PCM waveforms.  It shares
// the sample loader and post-processing settings, but is a synthesizer voice,
// not a sample-playback mode.
struct InstrumentSCWF : InstrumentVoicePostSettings {
  InstrumentSample oscillator[2];
  uint8_t detune;
  uint8_t mix;
};

struct InstrumentBYOWTBL : InstrumentSCWF {
  uint16_t frameSize[2];
  uint16_t tableFrames[2];
  uint8_t frameIndex[2];
};

// Drives an external MIDI device instead of synthesizing audio: triggering a
// note sends a MIDI Note On/Off on this channel (see chipnomad_lib/midi_io.h).
// Program/bank select are sent once, whenever they're about to differ from
// what that channel was last told (see applyVoiceEvents in chipnomad_lib.cpp) -
// not before every note, which would needlessly retrigger the receiving
// device's own envelopes.
struct InstrumentMidi {
  uint8_t channel;  // 0-15 (shown to the user as 1-16)
  uint8_t program;  // 0-127, or EMPTY_VALUE_8 to not send Program Change
  uint8_t bankHigh; // CC0 (Bank Select MSB), 0-127 or EMPTY_VALUE_8 for none
  uint8_t bankLow;  // CC32 (Bank Select LSB), 0-127 or EMPTY_VALUE_8 for none
  // Which CC number each of the 4 generic MC1-MC4 row FX sends to (0-127,
  // or EMPTY_VALUE_8 to leave that FX slot unconfigured/inert). A single FX
  // byte only carries one 0-255 value, not a CC number and a value, so the
  // number is fixed per instrument here and the per-row FX just carries the
  // value (0-255, rescaled to 0-127 on send) - same idea as Braids' BTM/BCL.
  uint8_t ccNumber[4];
};

// Optional tracker VCA around a native FM patch. Native operator envelopes stay
// intact. The inherited filter fields remain zero/reserved in this amp-only UI.
struct InstrumentFMAmp : InstrumentVoicePostSettings {
  uint8_t enabled;
};
// Runtime overrides store native value + 1, so native zero is distinct from unset.
struct NativeFMValues {
  uint16_t operators[6][12];
  uint16_t global[6];
};

struct InstrumentFMTone {
  NativeFMValues direct;
  int8_t brightness; // Modulator output-level offset, -63..63; zero preserves preset.
  uint8_t feedback; // 0 preserves preset; 1..8 select feedback 0..7.
  uint8_t operatorLevel[6]; // Runtime absolute level + 1; zero uses saved patch.
};

struct InstrumentOPLL {
  uint8_t schema;
  uint8_t program; // 1..15 ROM identity, zero is a custom tone
  int8_t fineTune; // cents
  uint8_t patch[8]; // complete tone, portable with the song
  uint16_t bankId;
  char presetName[64];
  InstrumentFMAmp amp;
  InstrumentFMTone tone;
};

enum class OPLTopology : uint8_t { twoOperator, fourOperator, dualVoice };
struct OPLOperator {
  uint8_t multiplier, level, attack, decay, sustain, release, waveform, keyScale;
  uint8_t vibrato, tremolo, sustained, rateScale;
};
struct InstrumentOPL {
  uint8_t schema;
  OPLTopology topology;
  OPLOperator operators[4]; // Native order: mod1, carrier1, mod2, carrier2.
  uint8_t feedback[2], connection[2], pan[2]; // pan: 1 left, 2 right, 3 both
  uint8_t deepVibrato, deepTremolo, percussion, fixedNote, drumKey, volumeModel;
  int16_t noteOffset[2];
  int8_t secondDetune, velocityOffset;
  uint16_t keyOnDuration, keyOffDuration; // Source estimates, never tail cutoffs.
  int8_t fineTune;
  uint16_t bankId, sourceBank, sourceProgram;
  char presetName[64];
  InstrumentFMAmp amp;
  InstrumentFMTone tone;
};

// Bounded, owned source sequences. Zero format preserves legacy patch behavior.
// Format 1: canonical Furnace sequence macros; format 2: GoatTracker GTI5.
struct ChipProgram {
  uint8_t format, rate;
  uint16_t size;
  uint8_t data[512];
};

struct InstrumentSimpleChip : InstrumentVoicePostSettings {
  uint8_t schema, preset;
  uint8_t mode; // Sega: tone/white/periodic. Pulse: 4 duties. Noise: 15/7 bits.
  uint8_t noiseRate, noiseDivisor, noiseShift;
  uint8_t envelopeInitial, envelopePeriod, envelopeIncrease;
  uint8_t sweepPeriod, sweepShift, sweepNegate;
  int8_t fineTune;
  uint8_t segaBassExtension; // Lower the virtual clock for notes below the 10-bit divider range.
  ChipProgram program;
};

// Four-operator Yamaha native order: S1, S2, S3, S4 (M1,C1,M2,C2).
struct FourOpOperator {
  uint8_t multiplier, detune, level, keyScale, attack, decay, sustainRate;
  uint8_t release, sustainLevel, ssg, detune2, amplitudeMod;
};
struct InstrumentFourOp {
  uint8_t schema, algorithm, feedback, pan, amplitudeSensitivity, pitchSensitivity;
  uint8_t lfoEnabled, lfoRate, lfoWave, amplitudeDepth, pitchDepth, operatorMask;
  FourOpOperator operators[4];
  int8_t fineTune;
  uint16_t bankId, sourceProgram;
  char presetName[64];
  InstrumentFMAmp amp;
  InstrumentFMTone tone;
};

struct InstrumentDX7 {
  uint8_t schema;
  uint8_t voice[155]; // Canonical Yamaha VCED: OP6..OP1, global parameters, name.
  int8_t fineTune;
  uint8_t velocity; // Native velocity; software tracker volume remains separate.
  uint16_t bankId, sourceProgram;
  char presetName[64];
  InstrumentFMAmp amp;
  InstrumentFMTone tone;
};

struct InstrumentSID {
  uint8_t schema;
  uint16_t value[25];
  uint16_t bankId;
  char presetName[64];
  ChipProgram program;
};

union InstrumentChipData {
  InstrumentSID sid;
  InstrumentFourOp fourOp;
  InstrumentDX7 dx7;
  InstrumentSimpleChip simpleChip;

  InstrumentOPL opl;

  InstrumentOPLL opll;
  InstrumentAY1 ay;
  InstrumentAY2 ay2;
  InstrumentAYSample aySample;
  InstrumentBraids braids;
  InstrumentSample sample;
  InstrumentSCWF scwf;
  InstrumentBYOWTBL byowtbl;
  InstrumentPlaits plaits;
  InstrumentAChChid achchid;
  InstrumentDrumSynth drumSynth;
  InstrumentMME mme;
  InstrumentSintered sintered;
  InstrumentMidi midi;
};

struct Instrument {
  InstrumentType type;
  char name[PROJECT_INSTRUMENT_NAME_LENGTH + 1];
  uint8_t tableSpeed;
  uint8_t transposeEnabled;
  uint8_t volume;
  uint8_t pan;
  Modulation modulation[4];
  InstrumentChipData chip;
};

struct InstrumentFunctions {
  int modDestinationsCount;
  const char* (*modName)(int modIndex);
  int (*init)(Instrument* instrument);
  int (*free)(Instrument* instrument);
  uint8_t supportsVoicePost;
  uint8_t supportsTrigger;
};

// This is metadata, not an audio abstraction: renderers keep their typed
// paths while screens, validation and motion routing share this one catalogue.
enum class InstrumentCategory : uint8_t { none, chip, sample, synth, drums, midi, fm };
enum class InstrumentScreenKind : uint8_t { none, ay1, ay2, aySample, braids, sample, scwf, byowtbl, plaits, achchid, drumSynth, mme, sintered, midi, opll, opl, simpleChip, dx7 };
enum class InstrumentMotionValue : uint8_t { raw, speed, cutoff };

static constexpr uint8_t instrumentNoFX = 0xff;

// Reserved destinations shared by all instruments for MIDI CC mappings.
// 0 is the explicit "no destination" option; keep the other values below
// the runtime MIDI CC destination table size.
enum MidiCCDestination : uint8_t {
  midiCCDestinationNone = 0,
  midiCCDestinationAttack = 11,
  midiCCDestinationDecay = 12,
  midiCCDestinationSustain = 13,
  midiCCDestinationRelease = 14,
  midiCCDestinationSongPlayStop = 15,
  midiCCDestinationTrackMute = 16,
  midiCCDestinationTrackSolo = 17,
  midiCCDestinationTrackVolume = 18,
  midiCCDestinationTrackReverbSend = 19,
  midiCCDestinationTrackDelaySend = 20,
};

struct InstrumentModDestination {
  const char* name;
  uint8_t fx;                 // FX enum value, or instrumentNoFX.
  uint16_t range;             // Native modulation range.
  InstrumentMotionValue value;
};

struct InstrumentFX {
  uint8_t fx;                 // FX enum value (kept byte-sized to avoid a project.h cycle).
  const char* name;
};

struct InstrumentDefinition {
  const char* uiName;
  InstrumentCategory category;
  InstrumentScreenKind screen;
  const InstrumentModDestination* destinations;
  uint8_t destinationCount;
  const InstrumentFX* fxList;
  uint8_t fxCount;
  InstrumentFunctions functions;
};

InstrumentFunctions getInstrumentFunctions(InstrumentType type);
const InstrumentDefinition* getInstrumentDefinition(InstrumentType type);
const InstrumentModDestination* instrumentModDestination(InstrumentType type, int destination);
int instrumentMotionDestination(const Instrument* instrument, int destination,
                                uint8_t* fx, int* base, int* range,
                                InstrumentMotionValue* value);
int instrumentCCDestinationAvailable(const Instrument* instrument, int destination);
int instrumentCCDestinationValue(const Instrument* instrument, int destination, uint8_t cc);
int instrumentSetCCDestination(Instrument* instrument, int destination, uint8_t cc);
int instrumentFXAvailable(InstrumentType type, uint8_t fx);
int instrumentFXAvailableForInstrument(const Instrument* instrument, uint8_t fx);
int instrumentModDestinationAvailable(const Instrument* instrument, int destination);
int drumSynthMacroUsed(DrumSynthEngine engine, int macro);
InstrumentVoicePostSettings* instrumentVoicePostSettings(Instrument* instrument);
InstrumentFMAmp* instrumentFMAmpSettings(Instrument* instrument);
InstrumentFMTone* instrumentFMToneSettings(Instrument* instrument);
const InstrumentModDestination* instrumentNativeModDestination(InstrumentType type, int generic);
// Piecewise mapping gives an exact 80 neutral and preserves all old 00-7E steps.
inline int fmBrightnessFromByte(int v) { return v<=128 ? ((v*63+64)/128)-63 : ((v-128)*63+63)/127; }
inline int fmBrightnessToByte(int v) { return v<=0 ? ((v+63)*128+31)/63 : 128+(v*127+31)/63; }
int instrumentNativeControlValue(const Instrument* instrument, int generic);
const char* instrumentModDestinationName(InstrumentType type, int destination);
const char* instrumentModDestinationNameForInstrument(const Instrument* instrument, int destination);
int instrumentModDestinationMax(InstrumentType type);
int instrumentGenericModDestination(InstrumentType type, int destination);
int modulationIsLiveStick(ModulationType type);
int modulationIsAdditive(ModulationType type);

enum GenericModDestination {
  genericModReverbSend = 0,
  genericModDelaySend,
  genericModFirstParameter,
  genericModDestinationCount = genericModFirstParameter + 16,
  genericModEnvelopeAttack = genericModDestinationCount,
  genericModEnvelopeDecay,
  genericModEnvelopeSustain,
  genericModEnvelopeRelease,
  genericModEnvelopeShape,
  genericModTriggerDecay,
  genericModTriggerColor,
  genericModFirstP5,
  genericModFirstInsert = genericModFirstP5 + 4,
  genericModFMBrightness = genericModFirstInsert + 16,
  genericModFMFeedback,
  genericModChipMode,
  genericModChipNoiseRate,
  genericModChipNoiseDivisor,
  genericModChipNoiseShift,
  genericModChipSweepPeriod,
  genericModChipSweepShift,
  genericModChipSweepDirection,
  genericModChipEnvelopeInitial,
  genericModChipEnvelopePeriod,
  genericModChipEnvelopeDirection,
  genericModFMOperator1, genericModFMOperator2, genericModFMOperator3,
  genericModFMOperator4, genericModFMOperator5, genericModFMOperator6,
  genericModSIDPulse, genericModSIDCutoff, genericModSIDResonance, genericModSIDWave,
  genericModSIDFilterMode, genericModSIDMacroRate, genericModSIDRing, genericModSIDSync,
  genericModFMTime, genericModFMDecay, genericModFMDetune, genericModFMRatio,
  genericModFMLFORate, genericModFMLFODepth,
  genericModSIDAttack, genericModSIDDecay, genericModSIDSustain, genericModSIDRelease,
  genericModSIDPartner,
  genericModFirstDirectFM,
  // Appended to preserve the numeric destinations stored by existing songs.
  genericModInstrumentPan = genericModFirstDirectFM + 78,
  genericModTrackPan,
  genericModTotalCount,
};

bool nativeFMModTarget(int generic,int* fx,int* op);

#endif // __CHIPNOMAD_LIB__PROJECT_INSTRUMENTS_H__
