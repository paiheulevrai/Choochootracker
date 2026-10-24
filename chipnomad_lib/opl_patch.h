#pragma once
#include "project_instruments.h"
#include <cstdio>
bool isOPL(InstrumentType type);
bool validOPL(InstrumentType type,const InstrumentOPL& patch);
void initOPLPatch(InstrumentOPL* patch);
int loadOPLData(FILE* file,Instrument* instrument);
int saveOPLData(FILE* file,const Instrument* instrument);
