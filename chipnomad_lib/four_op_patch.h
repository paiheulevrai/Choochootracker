#pragma once
#include "project_instruments.h"
#include <cstdio>
bool isFourOp(InstrumentType type);
bool validFourOp(InstrumentType type,const InstrumentFourOp& patch);
void initFourOpPatch(InstrumentFourOp* patch);
int loadFourOpData(FILE* file,Instrument* instrument);
int saveFourOpData(FILE* file,const Instrument* instrument);
