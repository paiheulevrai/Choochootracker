// The instrument EQ: value helpers shared by the EQ screen and the engine,
// and the filter that runs on the track an instrument plays on.
#ifndef CHOOCHOO_INSTRUMENT_EQ_H
#define CHOOCHOO_INSTRUMENT_EQ_H

#include "project_instruments.h"

struct Project;

constexpr int kEQGainMin = -30, kEQGainMax = 30;  // +-15 dB in half decibels
constexpr uint8_t kEQQMin = 24, kEQQMax = 96;      // Q 0.25 .. 16
constexpr uint16_t kEQFrequencyMin = 20, kEQFrequencyMax = 20000;

void instrumentEQDefaults(InstrumentEQ* eq);
// Seeds editable banks 01-14 for NEW only; loaders keep their flat defaults.
void instrumentEQInitPresets(Project* project);
bool instrumentEQIsDefault(const InstrumentEQ& eq);
bool instrumentEQSame(const InstrumentEQ& a, const InstrumentEQ& b);
// The first EQ bank no instrument uses (other than ignoreInstrument), flat
// ones first; EMPTY_VALUE_8 when every bank is in use.
uint8_t instrumentEQUnusedBank(const Project* project, int ignoreInstrument);
// The bank for an EQ that comes with a loaded instrument file: a bank that
// already holds the same changed EQ, or else an unused bank set to it.
uint8_t instrumentEQAdopt(Project* project, const InstrumentEQ& eq, int instrument);
// At least one band changes the sound.
bool instrumentEQAudible(const InstrumentEQ& eq);
const char* instrumentEQShapeName(EQShape shape);
inline float instrumentEQGainDb(int8_t gain) { return gain * 0.5f; }
float instrumentEQQ(uint8_t q);
// Response in decibels at hz, for one band or for the whole EQ.
float instrumentEQBandResponseDb(const InstrumentEQBand& band, float hz, float sampleRate);
float instrumentEQResponseDb(const InstrumentEQ& eq, float hz, float sampleRate);

class InstrumentEQFilter {
 public:
  void init(float sampleRate);
  void reset();
  // Takes new settings and keeps the filter memory, so edits don't click.
  void configure(const InstrumentEQ& eq);
  void process(float* interleaved, int frames);

 private:
  struct Biquad {
    float b0, b1, b2, a1, a2;
    float z1[2], z2[2];
  };
  Biquad bands_[INSTRUMENT_EQ_BANDS];
  bool active_[INSTRUMENT_EQ_BANDS];
  InstrumentEQ settings_;
  bool configured_;
  float sampleRate_;
};

#endif
