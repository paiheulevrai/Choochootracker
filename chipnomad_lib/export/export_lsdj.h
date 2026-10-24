#ifndef __CHIPNOMAD_LIB__EXPORT_EXPORT_LSDJ_H__
#define __CHIPNOMAD_LIB__EXPORT_EXPORT_LSDJ_H__

#include "../project.h"

// Exports a project's structure and notes as a Little Sound Dj song
// (.lsdsng, one compressed song).
//
// LSDj holds much that this tracker has no equivalent for (tables, grooves,
// wave frames, kit samples, synth settings, commands), so the file is built
// from an existing .lsdsng used as a template: everything outside the song
// rows, chains, phrases, instruments, tempo and title is copied from it
// untouched.
//
// Limitations (v1):
// - Tracks 1-4 become LSDj's PU1, PU2, WAV and NOI; other tracks are dropped.
// - Phrase FX, volumes and note-offs are not exported: LSDj commands are left
//   empty. LSDj has no note volume.
// - Instruments: GB Pulse and GB Noise become LSDj pulse and noise instruments
//   (duty, envelope, stability, panning); any other type becomes a default
//   pulse instrument.
// - The tempo is derived from the tick rate assuming the default 6-tick
//   groove.
// - LSDj has 128 chains and 255 phrases: using a higher number in tracks 1-4
//   makes the export fail. Notes outside LSDj's range are clamped.
//
// Returns 0 on success. On failure returns non-zero and sets
// projectExportLSDJError.
extern char projectExportLSDJError[48];
int projectExportLSDJ(Project* project, const char* templatePath, const char* outputPath);

#endif // __CHIPNOMAD_LIB__EXPORT_EXPORT_LSDJ_H__
