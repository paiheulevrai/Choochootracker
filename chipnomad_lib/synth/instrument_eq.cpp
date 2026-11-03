#include "instrument_eq.h"

#include "project.h"

#include <math.h>
#include <string.h>

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Coefficients {
  double b0, b1, b2, a1, a2;
};

constexpr InstrumentEQBand kDefaultBands[INSTRUMENT_EQ_BANDS] = {
  {EQShape::lowShelf, 100, 0, 42},
  {EQShape::bell, 1000, 0, 48},
  {EQShape::highShelf, 8000, 0, 42},
};

using S = EQShape;
constexpr InstrumentEQBand kOff = {S::off, 1000, 0, 48};
constexpr InstrumentEQ kNewProjectPresets[] = {
  {{{S::lowCut, 25, 0, 42}, kOff, kOff}},                                      // 10 Sub Clean
  {{{S::lowCut, 80, 0, 42}, kOff, kOff}},                                      // 11 Low Cut
  {{{S::lowCut, 180, 0, 42}, kOff, kOff}},                                     // 12 Thin Out
  {{{S::highCut, 8000, 0, 42}, kOff, kOff}},                                   // 13 Darken
  {{{S::bell, 300, -6, 48}, kOff, kOff}},                                     // 14 De-Mud
  {{{S::bell, 650, -6, 54}, kOff, kOff}},                                     // 15 De-Box
  {{{S::bell, 3200, -6, 48}, kOff, kOff}},                                    // 16 De-Harsh
  {{{S::highShelf, 10000, 4, 42}, kOff, kOff}},                                // 17 Air
  {{{S::lowCut, 90, 0, 42}, {S::bell, 300, -4, 48}, {S::bell, 2500, 4, 42}}},  // 18 Vocal Focus
  {{{S::lowCut, 30, 0, 42}, {S::bell, 250, -4, 48}, {S::bell, 900, 4, 42}}},   // 19 Bass Read
  {{{S::bell, 65, 4, 42}, {S::bell, 300, -6, 48}, {S::bell, 3000, 4, 48}}},    // 1A Kick Shape
  {{{S::lowCut, 100, 0, 42}, {S::bell, 200, 4, 48}, {S::bell, 5000, 4, 42}}},  // 1B Snare Shape
  {{{S::lowShelf, 100, 4, 42}, {S::bell, 800, -3, 42}, {S::highShelf, 8000, 4, 42}}}, // 1C Smile
  {{{S::lowShelf, 250, 3, 42}, {S::highShelf, 2500, -3, 42}, kOff}},           // 1D Warm Tilt
  {{{S::lowCut, 250, 0, 42}, {S::bell, 1200, 10, 60}, {S::highCut, 6000, 0, 42}}}, // 1E Nasal
  {{{S::lowCut, 350, 0, 42}, {S::bell, 1500, 4, 48}, {S::highCut, 3500, 0, 42}}}, // 1F Telephone
  {{{S::lowShelf, 120, -6, 42}, kOff, kOff}},                                 // 20 Low Tame
  {{{S::lowShelf, 250, -3, 42}, {S::highShelf, 2500, 3, 42}, kOff}},           // 21 Bright Tilt
  {{{S::highCut, 120, 0, 42}, kOff, kOff}},                                   // 22 Sub Isolate
  {{{S::bell, 1000, -18, 84}, kOff, kOff}},                                   // 23 Notch Search
};
static_assert(sizeof(kNewProjectPresets) / sizeof(kNewProjectPresets[0]) == 20,
              "NEW seeds twenty EQ banks");

bool sameBand(const InstrumentEQBand& a, const InstrumentEQBand& b) {
  return a.shape == b.shape && a.frequency == b.frequency && a.gain == b.gain && a.q == b.q;
}

bool hasGain(EQShape shape) {
  return shape == EQShape::lowShelf || shape == EQShape::bell || shape == EQShape::highShelf;
}

bool bandChangesSound(const InstrumentEQBand& band) {
  if (band.shape == EQShape::off || band.shape >= EQShape::totalCount) return false;
  return !hasGain(band.shape) || band.gain != 0;
}

// RBJ cookbook biquads; false when the band leaves the sound as it is.
bool bandCoefficients(const InstrumentEQBand& band, float sampleRate, Coefficients* c) {
  if (!bandChangesSound(band)) return false;
  const double hz = fmin(fmax((double)band.frequency, kEQFrequencyMin), sampleRate * 0.45);
  const double w = 2.0 * kPi * hz / sampleRate;
  const double cw = cos(w), alpha = sin(w) / (2.0 * instrumentEQQ(band.q));
  const double A = pow(10.0, instrumentEQGainDb(band.gain) / 40.0), rootA = 2.0 * sqrt(A) * alpha;
  double b0, b1, b2, a0, a1, a2;
  switch (band.shape) {
    case EQShape::lowCut:
      b0 = b2 = (1.0 + cw) / 2.0; b1 = -(1.0 + cw);
      a0 = 1.0 + alpha; a1 = -2.0 * cw; a2 = 1.0 - alpha;
      break;
    case EQShape::highCut:
      b0 = b2 = (1.0 - cw) / 2.0; b1 = 1.0 - cw;
      a0 = 1.0 + alpha; a1 = -2.0 * cw; a2 = 1.0 - alpha;
      break;
    case EQShape::lowShelf:
      b0 = A * ((A + 1.0) - (A - 1.0) * cw + rootA);
      b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
      b2 = A * ((A + 1.0) - (A - 1.0) * cw - rootA);
      a0 = (A + 1.0) + (A - 1.0) * cw + rootA;
      a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw);
      a2 = (A + 1.0) + (A - 1.0) * cw - rootA;
      break;
    case EQShape::highShelf:
      b0 = A * ((A + 1.0) + (A - 1.0) * cw + rootA);
      b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
      b2 = A * ((A + 1.0) + (A - 1.0) * cw - rootA);
      a0 = (A + 1.0) - (A - 1.0) * cw + rootA;
      a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw);
      a2 = (A + 1.0) - (A - 1.0) * cw - rootA;
      break;
    default:  // bell
      b0 = 1.0 + alpha * A; b1 = -2.0 * cw; b2 = 1.0 - alpha * A;
      a0 = 1.0 + alpha / A; a1 = -2.0 * cw; a2 = 1.0 - alpha / A;
      break;
  }
  *c = {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
  return true;
}

double responseDb(const Coefficients& c, float hz, float sampleRate) {
  const double w = 2.0 * kPi * hz / sampleRate;
  const double c1 = cos(w), s1 = sin(w), c2 = cos(2.0 * w), s2 = sin(2.0 * w);
  const double nr = c.b0 + c.b1 * c1 + c.b2 * c2, ni = c.b1 * s1 + c.b2 * s2;
  const double dr = 1.0 + c.a1 * c1 + c.a2 * c2, di = c.a1 * s1 + c.a2 * s2;
  const double num = nr * nr + ni * ni, den = dr * dr + di * di;
  return 10.0 * log10(fmax(num, 1e-12) / fmax(den, 1e-12));
}

}  // namespace

void instrumentEQDefaults(InstrumentEQ* eq) {
  memcpy(eq->bands, kDefaultBands, sizeof(kDefaultBands));
}

void instrumentEQInitPresets(Project* project) {
  memcpy(project->eqBanks + 0x10, kNewProjectPresets, sizeof(kNewProjectPresets));
}

bool instrumentEQIsDefault(const InstrumentEQ& eq) {
  InstrumentEQ defaults;
  instrumentEQDefaults(&defaults);
  return instrumentEQSame(eq, defaults);
}

bool instrumentEQSame(const InstrumentEQ& a, const InstrumentEQ& b) {
  for (int i = 0; i < INSTRUMENT_EQ_BANDS; ++i)
    if (!sameBand(a.bands[i], b.bands[i])) return false;
  return true;
}

uint8_t instrumentEQUnusedBank(const Project* project, int ignoreInstrument) {
  bool used[PROJECT_EQ_BANKS] = {};
  for (int i = 0; i < PROJECT_MAX_INSTRUMENTS; ++i) {
    const Instrument& instrument = project->instruments[i];
    if (i != ignoreInstrument && instrument.type != InstrumentType::none && instrument.eq < PROJECT_EQ_BANKS)
      used[instrument.eq] = true;
  }
  for (int pass = 0; pass < 2; ++pass)
    for (int bank = 0; bank < PROJECT_EQ_BANKS; ++bank)
      if (!used[bank] && (pass || instrumentEQIsDefault(project->eqBanks[bank]))) return (uint8_t)bank;
  return EMPTY_VALUE_8;
}

uint8_t instrumentEQAdopt(Project* project, const InstrumentEQ& eq, int instrument) {
  if (!instrumentEQIsDefault(eq))
    for (int bank = 0; bank < PROJECT_EQ_BANKS; ++bank)
      if (instrumentEQSame(project->eqBanks[bank], eq)) return (uint8_t)bank;
  const uint8_t bank = instrumentEQUnusedBank(project, instrument);
  if (bank != EMPTY_VALUE_8) project->eqBanks[bank] = eq;
  return bank;
}

bool instrumentEQAudible(const InstrumentEQ& eq) {
  for (const auto& band : eq.bands)
    if (bandChangesSound(band)) return true;
  return false;
}

const char* instrumentEQShapeName(EQShape shape) {
  static const char* const names[] = {"Off", "Lo cut", "Lo shelf", "Bell", "Hi shelf", "Hi cut"};
  return shape < EQShape::totalCount ? names[(int)shape] : names[0];
}

float instrumentEQQ(uint8_t q) {
  if (q < kEQQMin) q = kEQQMin;
  if (q > kEQQMax) q = kEQQMax;
  return powf(2.0f, (q - 48) / 12.0f);
}

float instrumentEQBandResponseDb(const InstrumentEQBand& band, float hz, float sampleRate) {
  Coefficients c;
  return bandCoefficients(band, sampleRate, &c) ? (float)responseDb(c, hz, sampleRate) : 0.0f;
}

float instrumentEQResponseDb(const InstrumentEQ& eq, float hz, float sampleRate) {
  double db = 0.0;
  for (const auto& band : eq.bands) db += instrumentEQBandResponseDb(band, hz, sampleRate);
  return (float)db;
}

void InstrumentEQFilter::init(float sampleRate) {
  sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
  configured_ = false;
  reset();
}

void InstrumentEQFilter::reset() {
  for (auto& band : bands_) band.z1[0] = band.z1[1] = band.z2[0] = band.z2[1] = 0.0f;
}

void InstrumentEQFilter::configure(const InstrumentEQ& eq) {
  for (int i = 0; configured_ && i < INSTRUMENT_EQ_BANDS; ++i) {
    if (!sameBand(eq.bands[i], settings_.bands[i])) break;
    if (i == INSTRUMENT_EQ_BANDS - 1) return;
  }
  for (int i = 0; i < INSTRUMENT_EQ_BANDS; ++i) {
    Coefficients c;
    const bool active = bandCoefficients(eq.bands[i], sampleRate_, &c);
    Biquad& band = bands_[i];
    // A band that starts working begins from silence, not from old memory.
    if (active && !(configured_ && active_[i])) band.z1[0] = band.z1[1] = band.z2[0] = band.z2[1] = 0.0f;
    if (active) {
      band.b0 = (float)c.b0; band.b1 = (float)c.b1; band.b2 = (float)c.b2;
      band.a1 = (float)c.a1; band.a2 = (float)c.a2;
    }
    active_[i] = active;
  }
  settings_ = eq;
  configured_ = true;
}

void InstrumentEQFilter::process(float* interleaved, int frames) {
  for (int i = 0; i < INSTRUMENT_EQ_BANDS; ++i) {
    if (!active_[i]) continue;
    Biquad& b = bands_[i];
    for (int channel = 0; channel < 2; ++channel) {
      float z1 = b.z1[channel], z2 = b.z2[channel];
      for (int n = 0; n < frames; ++n) {
        float& sample = interleaved[n * 2 + channel];
        const float x = sample;
        const float y = b.b0 * x + z1;
        z1 = b.b1 * x - b.a1 * y + z2;
        z2 = b.b2 * x - b.a2 * y;
        sample = y;
      }
      // Decaying memory would otherwise end in slow denormal numbers.
      b.z1[channel] = fabsf(z1) < 1e-20f ? 0.0f : z1;
      b.z2[channel] = fabsf(z2) < 1e-20f ? 0.0f : z2;
    }
  }
}
