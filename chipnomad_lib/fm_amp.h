#pragma once
#include "project_instruments.h"
#include <cstdio>

// Optional extension: old native instruments have a bypassed, zeroed amp.
// Return 1 for a consumed setting, 0 for another key, -1 for malformed data.
int loadFMAmpSetting(const char* line, InstrumentFMAmp& amp, bool& seen);
void saveFMAmpSetting(FILE* file, const InstrumentFMAmp& amp);
int loadFMToneSetting(const char* line, InstrumentFMTone& tone, bool& seen);
void saveFMToneSetting(FILE* file, const InstrumentFMTone& tone);

inline bool validFMControls(const InstrumentFMAmp& amp, const InstrumentFMTone& tone) {
  return amp.enabled<=1 && tone.brightness>=-63 && tone.brightness<=63 && tone.feedback<=8;
}
