#ifndef __PATTERN_CLONE_UTILS_H__
#define __PATTERN_CLONE_UTILS_H__

#include "chipnomad_lib.h"

enum class PatternCopyResult { success, noFreePhrase, invalidPhrase, historyFailure };

#ifdef __cplusplus
extern "C" {
#endif

// ── ViewMode-aware clone wrappers ───────────────────────────────────────────
// These wrappers dispatch to cloneChainToNext / deepCloneChain (in copy_paste.cpp)
// for classic view, or to pattern equivalents for simplified view. They are split
// into their own file (pattern_clone_utils.cpp) so that pattern_utils.cpp stays
// linkable without copy_paste.cpp — needed by beads_lifetime / measure / factory
// test targets that exclude copy_paste.cpp.
//
// The app build (Makefile.common wildcard) compiles both pattern_utils.cpp and
// pattern_clone_utils.cpp. The test build (Makefile.test) includes both in
// COMMON_SOURCES; beads_lifetime / measure / factory / chip_bench / chip_auditions
// exclude copy_paste.cpp AND pattern_clone_utils.cpp together.

// Clone the song cell (chain or pattern) into a free slot in the track bank.
// Returns the new index, or EMPTY_VALUE_16 if no empty slot is available.
int viewModeCloneToNext(Project* p, int srcIdx, int track);

PatternCopyResult patternCopyIndependent(Project* p, int srcIdx, int dstIdx, int track = -1);

// Replace the cell's phrases with independent clones. Returns 1 on success.
int viewModeDeepClone(Project* p, int srcIdx, int track);

#ifdef __cplusplus
}
#endif

#endif // __PATTERN_CLONE_UTILS_H__
