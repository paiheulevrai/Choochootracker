#ifndef __PATTERN_UTILS_H__
#define __PATTERN_UTILS_H__

#include "chipnomad_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Pattern / chain conversion ──────────────────────────────────────────────
// These functions implement the bidirectional classic ↔ simplified conversion
// described in the patterns spec (decisions #1, #5, #8).
//
// Behaviour contract:
//
//   convertChainsToPatterns(p):
//     - Walks p->song[row][track]; for each non-empty chain cell, finds or
//       creates the matching pattern (via chainToPatternMap / patternToChainMap)
//       and rewrites the cell to point at the pattern.
//     - Existing patterns are NOT regenerated (decision #5: chains[] and
//       patterns[] stay independent after first generation). Only chains
//       without a mapping get a fresh pattern.
//     - chains[] is left untouched.
//     - Sets p->viewMode = ViewMode::simplified.
//
//   convertPatternsToChains(p):
//     - Walks p->song[row][track]; for each non-empty pattern cell, finds or
//       creates the matching chain (via patternToChainMap / chainToPatternMap)
//       and rewrites the cell to point at the chain.
//     - Existing chains are NOT regenerated — original classic content is
//       preserved (decision #5's "so it can be reversed" guarantee).
//     - patterns[] is left untouched.
//     - Sets p->viewMode = ViewMode::classic.
//
//   Both functions return 0 on success, non-zero on a fatal error (out of
//   pattern or chain slots). On failure the project is left in an indeterminate
//   state — caller should reload from disk.
//
// Edge cases (decision #8):
//   - An empty chain (all rows have EMPTY_VALUE_16 phrase) is converted to a
//     1-bar pattern whose single bar references a freshly-allocated empty
//     phrase. The empty phrase gets the closest unallocated phrase number to
//     the pattern number (see findClosestEmptyPhrase()).
//   - Pattern bar allocation when bars are added (handled by the UI's
//     EDIT+Right): the new bar gets the closest unallocated phrase number to
//     the pattern number. See patternAllocateEmptyPhrase().

int convertChainsToPatterns(Project* p);
int convertPatternsToChains(Project* p);

// Find the closest unallocated phrase index to `patternIdx`, scanning
// outward from `patternIdx`. Returns EMPTY_VALUE_16 if phrases are full.
// "Unallocated" means the phrase is empty AND not referenced by any chain
// or pattern bar in the project.
int patternFindClosestEmptyPhrase(Project* p, int patternIdx);

// Find the closest unallocated pattern index to `startIdx`, scanning outward.
// Returns EMPTY_VALUE_16 if patterns are full.
int findClosestEmptyPattern(Project* p, int startIdx);

// Find the closest unallocated chain index to `startIdx`, scanning outward.
// Returns EMPTY_VALUE_16 if chains are full.
int findClosestEmptyChain(Project* p, int startIdx);

// Multiply a pattern's length by 2 by duplicating its existing bars.
// Returns 1 if successful, 0 if doubling would exceed PROJECT_MAX_PATTERN_LENGTH.
// Bars beyond the original length are filled with copies of the existing bars
// (1-bar A → 2-bar A,A; 2-bar A,B → 4-bar A,B,A,B; per decision #4).
int patternMultiplyLengthByTwo(Project* p, int patternIdx);

// Halve a pattern's length by 2 by dropping the second half.
// Returns 1 if successful, 0 if the current length is odd or already 1.
// Bars are not zeroed — only `length` is changed, so the dropped bars survive
// for an undo (a future ×2 will restore them).
int patternHalveLengthByTwo(Project* p, int patternIdx);

// Add one empty bar to the end of a pattern. The new bar references a freshly
// allocated empty phrase (closest unallocated to the pattern index).
// Returns 1 if successful, 0 if the pattern is already at max length.
int patternAddEmptyBar(Project* p, int patternIdx);

// Remove the last bar from a pattern. The phrase that was in the last bar is
// NOT freed (it might be referenced elsewhere). Only `length` decreases.
// Returns 1 if successful, 0 if the pattern is already at length 1.
int patternRemoveLastBar(Project* p, int patternIdx);

// ── ViewMode-aware wrappers (for screen_song.cpp / screen_phrase.cpp) ────────
// These wrap the chain equivalents (chainHasNotes, cloneChainToNext, etc.)
// and dispatch based on p->viewMode. The screen code can use these without
// branching on view mode everywhere.
//
// The pure wrappers (no copy_paste dependency) live here:
//   viewModeIsSimplified(p)              — quick check
//   viewModeHasNotes(p, cellIdx)         — chainHasNotes or patternHasNotes
//   viewModeFindEmpty(p, startIdx)      — findEmptyChain or findEmptyPattern
//   viewModeIsUsedElsewhere(p, cellIdx, excludeTrack, excludeRow)
//                                        — isChainUsedElsewhere or pattern equiv
//   viewModeIsEmpty(p, cellIdx)          — chainIsEmpty or patternIsEmpty
//
// The clone wrappers (viewModeCloneToNext / viewModeDeepClone) depend on
// copy_paste.cpp's cloneChainToNext / deepCloneChain, so they live in
// pattern_clone_utils.h to keep pattern_utils.cpp linkable without
// copy_paste (needed by beads_lifetime / measure / factory test targets
// that exclude copy_paste.cpp).

int viewModeIsSimplified(Project* p);
int8_t viewModeHasNotes(Project* p, int cellIdx);
int viewModeFindEmpty(Project* p, int startIdx);
int viewModeIsUsedElsewhere(Project* p, int cellIdx, int excludeTrack, int excludeRow);

// Returns 1 if the song cell (chain or pattern) is empty (no phrases in any
// row/bar). Wraps chainIsEmpty / patternIsEmpty.
int viewModeIsEmpty(Project* p, int cellIdx);

#ifdef __cplusplus
}
#endif

#endif // __PATTERN_UTILS_H__
