#include "model_catalog.h"

#define LEAF(label, value) {label, value, NULL, 0}
#define CATEGORY(label, items) {label, -1, items, (int)(sizeof(items) / sizeof(items[0]))}

static const SelectionItem plaitsAnalog[] = {
  LEAF("VA VCF", 0), LEAF("PHASE DIST", 1), LEAF("WAVE TERRAIN", 5),
  LEAF("STRING MACH", 6), LEAF("VIRTUAL ANALOG", 8), LEAF("WAVESHAPING", 9),
  LEAF("FORMANT", 11), LEAF("HARMONIC", 12), LEAF("WAVETABLE", 13), LEAF("CHORD", 14)
};
static const SelectionItem plaitsFm[] = {
  LEAF("6-OP FM 1", 2), LEAF("6-OP FM 2", 3), LEAF("6-OP FM 3", 4), LEAF("2-OP FM", 10)
};
static const SelectionItem plaitsDigital[] = {LEAF("CHIPTUNE", 7), LEAF("SPEECH", 15)};
static const SelectionItem plaitsTexture[] = {LEAF("SWARM", 16), LEAF("NOISE", 17), LEAF("PARTICLE", 18)};
static const SelectionItem plaitsPhysical[] = {LEAF("STRING", 19), LEAF("MODAL", 20)};
static const SelectionItem plaitsDrums[] = {LEAF("BASS DRUM", 21), LEAF("SNARE DRUM", 22), LEAF("HI-HAT", 23)};

const SelectionItem plaitsCategories[] = {
  CATEGORY("ANALOG / WAVES", plaitsAnalog), CATEGORY("FM", plaitsFm),
  CATEGORY("DIGITAL", plaitsDigital), CATEGORY("TEXTURE / NOISE", plaitsTexture),
  CATEGORY("PHYSICAL", plaitsPhysical), CATEGORY("DRUMS", plaitsDrums)
};
const int plaitsCategoryCount = sizeof(plaitsCategories) / sizeof(plaitsCategories[0]);

static const SelectionItem plaitsAltGranular[] = {
  LEAF("GLISSON", 0), LEAF("PULSAR", 1), LEAF("GENDY", 2),
  LEAF("SCANNED", 3), LEAF("LOOPBACK", 4)
};
static const SelectionItem plaitsAltPhase[] = {
  LEAF("PHASE WEAVE", 5), LEAF("SIDEBAND BANK", 6), LEAF("UNDERTOW", 7),
  LEAF("ATTRACTOR", 8), LEAF("LOCKSTEP", 9)
};
static const SelectionItem plaitsAltAcoustic[] = {
  LEAF("REED PIPE", 10), LEAF("BRASS", 11), LEAF("SHAKERS", 12),
  LEAF("CLAPS", 13), LEAF("FRESHETS FORMANT", 14)
};
static const SelectionItem plaitsAltHarmony[] = {
  LEAF("DIATONIC CHORD", 15), LEAF("SCALE STACK", 16),
  LEAF("WT DIATONIC CHORD", 17), LEAF("WT SCALE STACK", 18), LEAF("HELIX", 19)
};
static const SelectionItem plaitsAltDigital[] = {
  LEAF("BYTEBEAT", 20), LEAF("RULEFIELD", 21), LEAF("SPECTRAL SPIRAL", 22),
  LEAF("PHASE FLOCK", 23)
};
const SelectionItem plaitsAltCategories[] = {
  CATEGORY("GRANULAR / MICRO", plaitsAltGranular),
  CATEGORY("PHASE / HARMONIC", plaitsAltPhase),
  CATEGORY("ACOUSTIC / PHYSICAL", plaitsAltAcoustic),
  CATEGORY("POLYPHONY / HARMONY", plaitsAltHarmony),
  CATEGORY("DIGITAL / WEIRD", plaitsAltDigital)
};
const int plaitsAltCategoryCount = sizeof(plaitsAltCategories) / sizeof(plaitsAltCategories[0]);

static const SelectionItem braidsAnalog[] = {
  LEAF("CSAW", 0), LEAF("MORPH", 1), LEAF("SAW-SQUARE", 2), LEAF("SINE-TRI", 3),
  LEAF("BUZZ", 4), LEAF("SQUARE-SUB", 5), LEAF("SAW-SUB", 6),
  LEAF("SQUARE-SYNC", 7), LEAF("SAW-SYNC", 8)
};
static const SelectionItem braidsMulti[] = {
  LEAF("TRIPLE-SAW", 9), LEAF("TRIPLE-SQR", 10), LEAF("TRIPLE-TRI", 11),
  LEAF("TRIPLE-SINE", 12), LEAF("TRIPLE-RING", 13), LEAF("SAW-SWARM", 14),
  LEAF("SAW-COMB", 15), LEAF("TOY", 16)
};
static const SelectionItem braidsFormant[] = {
  LEAF("FILTER-LP", 17), LEAF("FILTER-PEAK", 18), LEAF("FILTER-BP", 19),
  LEAF("FILTER-HP", 20), LEAF("VOSIM", 21), LEAF("VOWEL", 22),
  LEAF("VOWEL-FOF", 23), LEAF("HARMONICS", 24)
};
static const SelectionItem braidsFm[] = {LEAF("FM", 25), LEAF("FEEDBACK-FM", 26), LEAF("CHAOTIC-FM", 27)};
static const SelectionItem braidsPhysical[] = {
  LEAF("PLUCKED", 28), LEAF("BOWED", 29), LEAF("BLOWN", 30),
  LEAF("FLUTED", 31), LEAF("STRUCK-BELL", 32), LEAF("STRUCK-DRUM", 33)
};
static const SelectionItem braidsDrums[] = {LEAF("KICK", 34), LEAF("CYMBAL", 35), LEAF("SNARE", 36)};
static const SelectionItem braidsWavetables[] = {LEAF("WAVETABLES", 37), LEAF("WAVE-MAP", 38), LEAF("WAVE-LINE", 39), LEAF("WAVE-PARA", 40)};
static const SelectionItem braidsNoise[] = {
  LEAF("FILTER-NOISE", 41), LEAF("TWIN-PEAKS", 42), LEAF("CLOCK-NOISE", 43),
  LEAF("GRAN-CLOUD", 44), LEAF("PARTICLE", 45), LEAF("DIGI-MOD", 46)
};

const SelectionItem braidsCategories[] = {
  CATEGORY("ANALOG", braidsAnalog), CATEGORY("MULTI OSC", braidsMulti),
  CATEGORY("FILTER / VOICE", braidsFormant), CATEGORY("FM / CHAOS", braidsFm),
  CATEGORY("PHYSICAL", braidsPhysical), CATEGORY("DRUMS", braidsDrums),
  CATEGORY("WAVETABLES", braidsWavetables), CATEGORY("NOISE / GRANULAR", braidsNoise)
};
const int braidsCategoryCount = sizeof(braidsCategories) / sizeof(braidsCategories[0]);

static bool catalogValid(const SelectionItem* categories, int count, int models) {
  bool seen[64] = {};
  for (int i = 0; i < count; ++i) {
    for (int j = 0; j < categories[i].childCount; ++j) {
      int value = categories[i].children[j].value;
      if (value < 0 || value >= models || seen[value]) return false;
      seen[value] = true;
    }
  }
  for (int i = 0; i < models; ++i) if (!seen[i]) return false;
  return true;
}

bool modelCatalogsValid() {
  return catalogValid(plaitsCategories, plaitsCategoryCount, 24) &&
         catalogValid(plaitsAltCategories, plaitsAltCategoryCount, 24) &&
         catalogValid(braidsCategories, braidsCategoryCount, 47);
}
