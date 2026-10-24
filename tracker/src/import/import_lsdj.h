#ifndef __IMPORT_LSDJ_H__
#define __IMPORT_LSDJ_H__

#include "chipnomad_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

// Imports a Little Sound Dj song (.lsdsng, one compressed 32 KB song) as a new
// project. Only the structure and the notes are carried over: song rows,
// chains (with transpose) and phrases (note, instrument number) keep the same
// indices as in the LSDj file. LSDj channels PU1, PU2, WAV and NOI become
// tracks 1-4. Pulse and noise instruments become GB Pulse and GB Noise with
// their duty, envelope, stability and panning (the pulse sweep is not
// imported); every note gets full volume. Wave instruments become the GB Pulse
// Hollow Lead preset and kits a default GB Noise. Phrase commands C (chord),
// D (delay), E (pulse and noise envelope), G (groove), K (kill) and V
// (vibrato) become ARP, DEL, CEI/CEP/CED, GGR, KIL and PVB, and the 32 grooves
// are imported. Other commands, tables, wave frames, kit samples and synth
// settings are ignored. The tempo is converted to the tick rate with a 6-tick
// groove.
// The destination must be initialized with projectInit/projectInitAY first.
// Success releases its previous instrument data; failure leaves the
// destination unchanged and owned by the caller.
int projectLoadLSDJ(Project* project, const char* path);

// Songs of an LSDj save file (.sav), for a picker: slot 0-31, name, version
// and size in 512 byte blocks. Fills at most maxSongs entries and stores their
// number in *count. Returns 0 when there is at least one song; otherwise sets
// projectFileError.
struct LsdjSavSong {
  int slot;
  char name[9];
  uint8_t version;
  int blocks;
};
int lsdjListSav(const char* savPath, LsdjSavSong* songs, int maxSongs, int* count);

// Same as projectLoadLSDJ for the song in the given slot of a .sav.
int projectLoadLSDJSav(Project* project, const char* savPath, int slot);

// Extracts every song of an LSDj save file (.sav, up to 32 songs) as an
// .lsdsng file in outDir (created if needed), named <name>_v<version>.lsdsng.
// The song being edited when the cartridge was saved is not extracted.
// Returns 0 on success and stores the number of files in *count; on failure
// sets projectFileError.
int lsdjExtractSav(const char* savPath, const char* outDir, int* count);

#ifdef __cplusplus
}
#endif

#endif
