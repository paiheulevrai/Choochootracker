#ifndef __CHIPNOMAD_LIB__EXPORT_EXPORT_M8S_H__
#define __CHIPNOMAD_LIB__EXPORT_EXPORT_M8S_H__

#include "../project.h"

// Exports a project's structure and notes as a Dirtywave M8 song (.m8s).
//
// The M8 format holds a lot that this tracker has no equivalent for
// (instruments, tables, mixer, effects...), so the file is built from an
// existing .m8s used as a template: everything outside the song rows,
// chains, phrases, tempo and title is copied from it untouched. The
// template's own song, chains and phrases are fully replaced.
//
// Limitations (v1):
// - Per-row FX are not exported; phrase FX columns are left empty.
// - Instruments are not converted, except Braids (to MacroSynth) and Sample
//   (to Sampler, path /Samples/<file name>, the WAV must be copied to the
//   M8 by hand). Their records are taken from a template instrument of the
//   same type; without one the slot is left as it is. All other instruments
//   keep whatever the template defines in their slots.
// - The tempo is derived from the tick rate assuming the default 6-tick
//   groove; per-track grooves and tempo effects are ignored.
// - The M8 has 255 phrases: a chain referencing a phrase numbered 255 or
//   higher makes the export fail. Notes below the M8's lowest note are
//   clamped.
// - Only version 2.x - 4.x templates are accepted.
//
// Returns 0 on success. On failure returns non-zero and sets
// projectExportM8SError.
extern char projectExportM8SError[48];
int projectExportM8S(Project* project, const char* templatePath, const char* outputPath);

#endif // __CHIPNOMAD_LIB__EXPORT_EXPORT_M8S_H__
