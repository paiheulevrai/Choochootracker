#include "pattern_utils.h"
#include "project_utils.h"
#include <string.h>

// ── Helpers ─────────────────────────────────────────────────────────────────

// Find the closest unallocated phrase index to `targetIdx`, scanning outward
// (target, target+1, target-1, target+2, target-2, ...). "Unallocated" means
// the phrase is empty (phraseIsEmpty) AND not referenced by any chain row or
// pattern bar anywhere in the project. This satisfies decision #8's "closest
// unallocated numbers to the pattern number".
int patternFindClosestEmptyPhrase(Project* p, int targetIdx) {
  if (targetIdx < 0) targetIdx = 0;
  if (targetIdx >= PROJECT_MAX_PHRASES) targetIdx = PROJECT_MAX_PHRASES - 1;

  // Build a "used" bitmap of phrases referenced by chains and patterns.
  // A 1 means the phrase is referenced somewhere and should be skipped.
  static uint8_t used[PROJECT_MAX_PHRASES];  // large but on heap of static ok
  // Note: this is static to avoid stack overflow on small platforms (RG353V
  // PortMaster builds). The function is called from the UI thread, never
  // reentrant.
  memset(used, 0, sizeof(used));
  for (int c = 0; c < PROJECT_MAX_CHAINS; c++) {
    if (chainIsEmpty(p, c)) continue;
    for (int r = 0; r < 16; r++) {
      uint16_t ph = p->chains[c].rows[r].phrase;
      if (ph != EMPTY_VALUE_16 && ph < PROJECT_MAX_PHRASES) used[ph] = 1;
    }
  }
  for (int c = 0; c < PROJECT_MAX_PATTERNS; c++) {
    if (patternIsEmpty(p, c)) continue;
    int len = p->patterns[c].length;
    if (len < 1) len = 1;
    if (len > PROJECT_MAX_PATTERN_LENGTH) len = PROJECT_MAX_PATTERN_LENGTH;
    for (int b = 0; b < len; b++) {
      uint16_t ph = p->patterns[c].bars[b].phrase;
      if (ph != EMPTY_VALUE_16 && ph < PROJECT_MAX_PHRASES) used[ph] = 1;
    }
  }

  // Scan outward from targetIdx.
  for (int offset = 0; offset < PROJECT_MAX_PHRASES; offset++) {
    int hi = targetIdx + offset;
    int lo = targetIdx - offset;
    if (hi >= 0 && hi < PROJECT_MAX_PHRASES && !used[hi] && phraseIsEmpty(p, hi)) {
      return hi;
    }
    if (lo >= 0 && lo < PROJECT_MAX_PHRASES && !used[lo] && phraseIsEmpty(p, lo) &&
        lo != hi) {
      return lo;
    }
  }
  return EMPTY_VALUE_16;
}

int findClosestEmptyPattern(Project* p, int startIdx) {
  if (startIdx < 0) startIdx = 0;
  if (startIdx >= PROJECT_MAX_PATTERNS) startIdx = PROJECT_MAX_PATTERNS - 1;
  for (int offset = 0; offset < PROJECT_MAX_PATTERNS; offset++) {
    int hi = startIdx + offset;
    int lo = startIdx - offset;
    if (hi >= 0 && hi < PROJECT_MAX_PATTERNS && patternIsEmpty(p, hi) &&
        p->patternToChainMap[hi] == EMPTY_VALUE_16) {
      return hi;
    }
    if (lo >= 0 && lo < PROJECT_MAX_PATTERNS && patternIsEmpty(p, lo) &&
        p->patternToChainMap[lo] == EMPTY_VALUE_16 && lo != hi) {
      return lo;
    }
  }
  return EMPTY_VALUE_16;
}

int findClosestEmptyChain(Project* p, int startIdx) {
  if (startIdx < 0) startIdx = 0;
  if (startIdx >= PROJECT_MAX_CHAINS) startIdx = PROJECT_MAX_CHAINS - 1;
  for (int offset = 0; offset < PROJECT_MAX_CHAINS; offset++) {
    int hi = startIdx + offset;
    int lo = startIdx - offset;
    if (hi >= 0 && hi < PROJECT_MAX_CHAINS && chainIsEmpty(p, hi) &&
        p->chainToPatternMap[hi] == EMPTY_VALUE_16) {
      return hi;
    }
    if (lo >= 0 && lo < PROJECT_MAX_CHAINS && chainIsEmpty(p, lo) &&
        p->chainToPatternMap[lo] == EMPTY_VALUE_16 && lo != hi) {
      return lo;
    }
  }
  return EMPTY_VALUE_16;
}

// ── Convert chain → pattern (first classic→simplified switch) ──────────────
// For each chain used in the song, allocate a pattern (or reuse an existing
// one via the map), copy the chain's non-empty rows into pattern bars,
// preserve the transpose as a hidden field, and rewrite the song cell.
//
// Edge case (decision #8): an empty chain becomes a 1-bar pattern whose
// single bar references a freshly-allocated empty phrase (closest unallocated
// phrase number to the pattern number).
int convertChainsToPatterns(Project* p) {
  // Walk the song and rewrite each cell from chain idx → pattern idx.
  for (int row = 0; row < PROJECT_MAX_LENGTH; row++) {
    for (int track = 0; track < PROJECT_MAX_TRACKS; track++) {
      uint16_t chainIdx = p->song[row][track];
      if (chainIdx == EMPTY_VALUE_16) continue;
      if (chainIdx >= PROJECT_MAX_CHAINS) continue;  // defensive

      // Find or create the matching pattern.
      uint16_t patternIdx = p->chainToPatternMap[chainIdx];
      if (patternIdx == EMPTY_VALUE_16) {
        // Allocate a new pattern close to the chain index.
        patternIdx = (uint16_t)findClosestEmptyPattern(p, chainIdx);
        if (patternIdx == EMPTY_VALUE_16) return 1;  // patterns full

        Pattern* pat = &p->patterns[patternIdx];
        patternClear(pat);

        // Walk the chain's non-empty rows top-down (mirrors playback's
        // behaviour at playback.cpp:1291 — chain stops at first empty row).
        int barCount = 0;
        for (int r = 0; r < 16; r++) {
          uint16_t phrase = p->chains[chainIdx].rows[r].phrase;
          if (phrase == EMPTY_VALUE_16) break;  // stop at first empty
          if (barCount >= PROJECT_MAX_PATTERN_LENGTH) break;
          PatternBar* bar = &pat->bars[barCount];
          bar->phrase = phrase;
          bar->transpose = p->chains[chainIdx].rows[r].transpose;
          barCount++;
        }

        // Edge case: empty chain (zero non-empty rows).
        // Convert to a 1-bar pattern with a freshly-allocated empty phrase.
        if (barCount == 0) {
          int emptyPhrase = patternFindClosestEmptyPhrase(p, patternIdx);
          if (emptyPhrase == EMPTY_VALUE_16) return 2;  // phrases full
          pat->bars[0].phrase = (uint16_t)emptyPhrase;
          pat->bars[0].transpose = 0;
          barCount = 1;
        }

        pat->length = (uint8_t)barCount;
        p->chainToPatternMap[chainIdx] = patternIdx;
        p->patternToChainMap[patternIdx] = chainIdx;
      }

      p->song[row][track] = patternIdx;
    }
  }
  p->viewMode = ViewMode::simplified;
  return 0;
}

// ── Convert pattern → chain (simplified→classic switch) ────────────────────
// For each pattern used in the song, allocate a chain (or reuse an existing
// one via the map), copy the pattern's bars into chain rows, restore the
// hidden transpose, and rewrite the song cell.
//
// The chain is ALWAYS regenerated from the pattern's current bars when
// switching back to classic (per the user-facing spec: "Phrase values in
// chain should be generated according to the logic from the first prompt").
// This means simplified-mode edits to the pattern are reflected in the
// chain after the switch. The mapping (patternToChainMap /
// chainToPatternMap) is preserved across the regeneration so the same
// chain index is reused — only its content is refreshed. If no mapping
// exists yet (pattern was created in simplified mode with no origin
// chain), a fresh chain is allocated.
int convertPatternsToChains(Project* p) {
  for (int row = 0; row < PROJECT_MAX_LENGTH; row++) {
    for (int track = 0; track < PROJECT_MAX_TRACKS; track++) {
      uint16_t patternIdx = p->song[row][track];
      if (patternIdx == EMPTY_VALUE_16) continue;
      if (patternIdx >= PROJECT_MAX_PATTERNS) continue;

      uint16_t chainIdx = p->patternToChainMap[patternIdx];
      if (chainIdx == EMPTY_VALUE_16) {
        // Pattern was created in simplified mode; allocate a fresh chain.
        chainIdx = (uint16_t)findClosestEmptyChain(p, patternIdx);
        if (chainIdx == EMPTY_VALUE_16) return 1;  // chains full
        p->patternToChainMap[patternIdx] = chainIdx;
        p->chainToPatternMap[chainIdx] = patternIdx;
      }

      // Always sync the chain's content from the pattern's current bars.
      // This reflects any edits made in simplified mode into the chain so
      // the user sees their pattern's phrases after switching back to
      // classic. After the switch, chain and pattern stay independent
      // (chain edits don't propagate back to the pattern).
      Chain* ch = &p->chains[chainIdx];
      chainClear(ch);

      Pattern* pat = &p->patterns[patternIdx];
      int len = pat->length;
      if (len < 1) len = 1;
      if (len > 16) len = 16;  // chain has 16 rows max
      if (len > PROJECT_MAX_PATTERN_LENGTH) len = PROJECT_MAX_PATTERN_LENGTH;

      for (int b = 0; b < len; b++) {
        ch->rows[b].phrase = pat->bars[b].phrase;
        ch->rows[b].transpose = pat->bars[b].transpose;
      }
      // Remaining rows stay EMPTY_VALUE_16 (chainClear).

      p->song[row][track] = chainIdx;
    }
  }
  p->viewMode = ViewMode::classic;
  return 0;
}

// ── Pattern length editing ──────────────────────────────────────────────────

int patternMultiplyLengthByTwo(Project* p, int patternIdx) {
  if (patternIdx < 0 || patternIdx >= PROJECT_MAX_PATTERNS) return 0;
  Pattern* pat = &p->patterns[patternIdx];
  int curLen = pat->length;
  if (curLen < 1) curLen = 1;
  if (curLen > PROJECT_MAX_PATTERN_LENGTH) curLen = PROJECT_MAX_PATTERN_LENGTH;
  int newLen = curLen * 2;
  if (newLen > PROJECT_MAX_PATTERN_LENGTH) return 0;  // would exceed 16

  // Duplicate existing bars (decision #4), but each new bar gets its OWN
  // freshly-allocated phrase — editing a copy must never touch the original
  // bar's phrase. Empty bars stay empty. Each bar is wired and the length
  // grown IMMEDIATELY so the next allocation's used[] bitmap sees the phrase
  // just wired (two bars whose sources hold no notes could otherwise receive
  // the SAME phrase — the "multiplied bars share" bug). If the phrase pool
  // runs out mid-way the operation fails with the length restored to the
  // original: bars beyond it are meaningless until length grows again.
  // 1-bar A → 2-bar A,A' (isolated); 2-bar A,B → 4-bar A,B,A',B'.
  const int origLen = pat->length;
  for (int b = curLen; b < newLen; b++) {
    uint16_t srcPhrase = pat->bars[b - curLen].phrase;
    if (srcPhrase == EMPTY_VALUE_16) {
      pat->bars[b].phrase = EMPTY_VALUE_16;
      pat->length = (uint8_t)(b + 1);
      continue;
    }
    int emptyPhrase = patternFindClosestEmptyPhrase(p, patternIdx);
    if (emptyPhrase == EMPTY_VALUE_16) {
      pat->length = (uint8_t)origLen;  // pool exhausted: no net change
      return 0;
    }
    // Copy content so the clone plays like the original until edited.
    p->phrases[emptyPhrase] = p->phrases[srcPhrase];
    pat->bars[b] = pat->bars[b - curLen];
    pat->bars[b].phrase = (uint16_t)emptyPhrase;
    pat->length = (uint8_t)(b + 1);
  }
  pat->length = (uint8_t)newLen;
  return 1;
}

int patternHalveLengthByTwo(Project* p, int patternIdx) {
  if (patternIdx < 0 || patternIdx >= PROJECT_MAX_PATTERNS) return 0;
  Pattern* pat = &p->patterns[patternIdx];
  int curLen = pat->length;
  if (curLen < 1) return 0;
  if (curLen == 1) return 0;          // can't halve a 1-bar pattern
  if (curLen % 2 != 0) return 0;      // odd length can't be halved cleanly

  // Just drop the second half (decision #4). Bars beyond `length` survive
  // for an undo (a future ×2 will restore them).
  pat->length = (uint8_t)(curLen / 2);
  return 1;
}

int patternAddEmptyBar(Project* p, int patternIdx) {
  if (patternIdx < 0 || patternIdx >= PROJECT_MAX_PATTERNS) return 0;
  Pattern* pat = &p->patterns[patternIdx];
  int curLen = pat->length;
  if (curLen >= PROJECT_MAX_PATTERN_LENGTH) return 0;  // already at max

  // Allocate a fresh empty phrase for the new bar (decision: new bars get
  // closest unallocated phrase numbers, mirroring decision #8's policy for
  // new patterns).
  int emptyPhrase = patternFindClosestEmptyPhrase(p, patternIdx);
  if (emptyPhrase == EMPTY_VALUE_16) return 0;  // phrases full

  pat->bars[curLen].phrase = (uint16_t)emptyPhrase;
  pat->bars[curLen].transpose = 0;
  pat->length = (uint8_t)(curLen + 1);
  return 1;
}

int patternRemoveLastBar(Project* p, int patternIdx) {
  if (patternIdx < 0 || patternIdx >= PROJECT_MAX_PATTERNS) return 0;
  Pattern* pat = &p->patterns[patternIdx];
  int curLen = pat->length;
  if (curLen <= 1) return 0;  // already at minimum

  // Don't free the phrase — it might be referenced elsewhere. Just decrement.
  // The bar's data survives for undo (a future EDIT+Right will reallocate).
  pat->length = (uint8_t)(curLen - 1);
  return 1;
}

// ── ViewMode-aware wrappers ──────────────────────────────────────────────────

int viewModeIsSimplified(Project* p) {
  return p->viewMode == ViewMode::simplified;
}

int8_t viewModeHasNotes(Project* p, int cellIdx) {
  if (p->viewMode == ViewMode::simplified) {
    if (cellIdx < 0 || cellIdx >= PROJECT_MAX_PATTERNS) return 0;
    if (patternIsEmpty(p, cellIdx)) return 0;
    // A pattern has notes if any of its bars (within length) has a phrase that
    // has notes. Mirrors chainHasNotes().
    Pattern* pat = &p->patterns[cellIdx];
    int len = pat->length;
    if (len < 1) len = 1;
    if (len > PROJECT_MAX_PATTERN_LENGTH) len = PROJECT_MAX_PATTERN_LENGTH;
    for (int b = 0; b < len; b++) {
      uint16_t ph = pat->bars[b].phrase;
      if (ph != EMPTY_VALUE_16 && phraseHasNotes(p, ph)) return 1;
    }
    return 0;
  }
  return chainHasNotes(p, cellIdx);
}

int viewModeFindEmpty(Project* p, int startIdx) {
  if (p->viewMode == ViewMode::simplified) {
    return findClosestEmptyPattern(p, startIdx);
  }
  return findEmptyChain(p, startIdx);
}

// NOTE: viewModeCloneToNext and viewModeDeepClone live in pattern_clone_utils.cpp
// because they depend on copy_paste.cpp's cloneChainToNext / deepCloneChain.
// Keeping them separate lets pattern_utils.cpp link without copy_paste.cpp,
// which is required by the beads_lifetime / measure / factory test targets
// that exclude copy_paste.cpp from their source list.

int viewModeIsUsedElsewhere(Project* p, int cellIdx, int excludeTrack, int excludeRow) {
  if (p->viewMode == ViewMode::simplified) {
    // Walk the song; check if any other cell references this pattern.
    for (int row = 0; row < PROJECT_MAX_LENGTH; row++) {
      for (int track = 0; track < PROJECT_MAX_TRACKS; track++) {
        if (row == excludeRow && track == excludeTrack) continue;
        if (p->song[row][track] == cellIdx) return 1;
      }
    }
    return 0;
  }
  return isChainUsedElsewhere(p, cellIdx, excludeTrack, excludeRow);
}

int viewModeIsEmpty(Project* p, int cellIdx) {
  if (p->viewMode == ViewMode::simplified) {
    if (cellIdx < 0 || cellIdx >= PROJECT_MAX_PATTERNS) return 1;
    return patternIsEmpty(p, cellIdx);
  }
  if (cellIdx < 0 || cellIdx >= PROJECT_MAX_CHAINS) return 1;
  return chainIsEmpty(p, cellIdx);
}
