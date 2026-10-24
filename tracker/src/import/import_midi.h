#ifndef __IMPORT_MIDI_H__
#define __IMPORT_MIDI_H__

#include "chipnomad_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

// Imports a Standard MIDI File as a new project. Notes are grouped by MIDI
// channel (one channel -> one tracker track, up to PROJECT_MAX_TRACKS; extra
// channels beyond that are dropped), quantized onto a fixed grid (4 rows per
// beat), and placed on a single default AY instrument (slot 0) - MIDI
// program numbers have no chiptune equivalent, so the user is expected to
// pick real instruments afterward. Only the first tempo found in the file is
// used (single global tickRate/groove, no per-section tempo changes).
// The destination must be initialized with projectInit/projectInitAY first,
// even for an empty project. Success releases its previous instrument data;
// failure leaves the destination unchanged and owned by the caller.
int projectLoadMidi(Project* project, const char* path);

#ifdef __cplusplus
}
#endif

#endif
