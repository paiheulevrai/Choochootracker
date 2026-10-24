#pragma once
#include "project_instruments.h"
#include <cstdio>
// Stable field order for versioned native SID programs, never raw struct dumps.
enum SIDParameter {
 sidReserved, sidWave, sidAttack, sidDecay, sidSustain, sidRelease,
 sidPulse, sidCutoff, sidResonance, sidFilterMode, sidRing, sidSync, sidPartnerRatio,
 sidMacroRate, sidGateFrames, sidFixedFrequency, sidSweepTarget, sidSweepFrames,
 sidSweepMode, sidPulseDepth, sidPulseRate, sidVibratoDepth, sidVibratoRate,
 sidCutoffTarget, sidArpeggio, sidParameterCount
};
static_assert(sidParameterCount==25,"SID serialization field count");
void initSIDPatch(InstrumentSID* patch);
bool validSID(const InstrumentSID& patch);
// A non-null reason identifies a base control owned by an embedded program.
const char* sidImportedControlReason(const InstrumentSID& patch, int parameter);
int loadSIDData(FILE* file,Instrument* instrument);
void saveSIDData(FILE* file,const Instrument* instrument);
