#ifndef IMPORT_VTS_H
#define IMPORT_VTS_H

#include "chipnomad_lib.h"

int instrumentLoadVTS(const char* path, int instrumentIdx);
int instrumentLoadVTSFromMemory(Project* project, char** lines, int lineCount,
                                int instrumentIdx, const char* instrumentName);

#endif
