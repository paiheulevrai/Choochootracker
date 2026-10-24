#ifndef __IMPORT_M8S_H__
#define __IMPORT_M8S_H__

#include "chipnomad_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

// Imports a Dirtywave M8 song (.m8s) as a new project. Only the structure
// and the notes are carried over: song rows, chains (with transpose) and
// phrases (note, velocity, instrument number) keep the same indices as in
// the M8 file; notes keep their real pitch
// (M8 note value = MIDI note, index = value - 12). Every
// instrument used by a phrase is converted as far as possible: macrosynth to
// Braids, sampler to Sample (WAV looked up near the file), FM synth to Genesis
// FM, MIDI out to MIDI Out, wavsynth and hypersynth to aChChid; anything else is a default AY
// instrument named after the M8 one. Effects, tables, grooves, mixer and the
// instruments' own parameters are ignored. The tempo is converted to the
// tick rate with the app's default 6-tick groove.
// The destination must be initialized with projectInit/projectInitAY first.
// Success releases its previous instrument data; failure leaves the
// destination unchanged and owned by the caller.
int projectLoadM8S(Project* project, const char* path);

// Same, with an extra folder (the app's sample folder) searched for the WAV
// files that M8 sampler instruments refer to. sampleDir may be NULL.
int projectLoadM8SWithSamples(Project* project, const char* path, const char* sampleDir);

#ifdef __cplusplus
}
#endif

#endif
