#pragma once
#include "project_instruments.h"
bool isOPLL(InstrumentType type);
const char* opllPresetName(InstrumentType type, int program);
bool opllApplyPreset(Instrument* instrument, int program);
