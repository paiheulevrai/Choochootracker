#pragma once
#include "project_instruments.h"
bool isSimpleChip(InstrumentType type);
int simpleChipPresetCount(InstrumentType type);
const char* simpleChipPresetName(InstrumentType type,int preset);
bool simpleChipApplyPreset(Instrument* instrument,int preset);
bool validSimpleChip(InstrumentType type,const InstrumentSimpleChip& patch);
