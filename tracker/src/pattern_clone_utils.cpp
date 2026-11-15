#include "pattern_clone_utils.h"
#include "edit_history.h"
#include "pattern_utils.h"
#include "copy_paste.h"
#include "project_utils.h"
#include <string.h>

// ── Pattern clone helpers (used by viewModeCloneToNext / viewModeDeepClone) ──

PatternCopyResult patternCopyIndependent(Project* p, int srcIdx, int dstIdx) {
  if (!p || srcIdx < 0 || srcIdx >= PROJECT_MAX_PATTERNS ||
      dstIdx < 0 || dstIdx >= PROJECT_MAX_PATTERNS || srcIdx == dstIdx)
    return PatternCopyResult::invalidPhrase;

  static uint8_t used[PROJECT_MAX_PHRASES]; // UI-only scratch avoids a large handheld stack.
  memset(used, 0, sizeof(used));
  for (const auto& chain : p->chains)
    for (const auto& row : chain.rows)
      if (row.phrase < PROJECT_MAX_PHRASES) used[row.phrase] = 1;
  for (const auto& pattern : p->patterns)
    for (const auto& bar : pattern.bars)
      if (bar.phrase < PROJECT_MAX_PHRASES) used[bar.phrase] = 1;

  uint16_t source[PROJECT_MAX_PATTERN_LENGTH], destination[PROJECT_MAX_PATTERN_LENGTH];
  int count = 0;
  for (const auto& bar : p->patterns[srcIdx].bars) {
    if (bar.phrase == EMPTY_VALUE_16) continue;
    if (bar.phrase >= PROJECT_MAX_PHRASES) return PatternCopyResult::invalidPhrase;
    int i = 0;
    while (i < count && source[i] != bar.phrase) ++i;
    if (i < count) continue;
    int fresh = 0;
    while (fresh < PROJECT_MAX_PHRASES && (used[fresh] || !phraseIsEmpty(p, fresh))) ++fresh;
    if (fresh == PROJECT_MAX_PHRASES) return PatternCopyResult::noFreePhrase;
    source[count] = bar.phrase;
    destination[count++] = (uint16_t)fresh;
    used[fresh] = 1;
  }

  if (!editHistoryCapture(p, EditObject::pattern, dstIdx)) return PatternCopyResult::historyFailure;
  for (int i = 0; i < count; ++i)
    if (!editHistoryCapture(p, EditObject::phrase, destination[i])) return PatternCopyResult::historyFailure;
  p->patterns[dstIdx] = p->patterns[srcIdx];
  for (int i = 0; i < count; ++i) {
    p->phrases[destination[i]] = p->phrases[source[i]];
    for (auto& bar : p->patterns[dstIdx].bars)
      if (bar.phrase == source[i]) bar.phrase = destination[i];
  }
  return PatternCopyResult::success;
}

// Clone a pattern to the next empty slot. Returns the new pattern index, or
// EMPTY_VALUE_16 if no empty slot is available.
static int clonePatternToNext(Project* p, int srcIdx) {
  if (srcIdx < 0 || srcIdx >= PROJECT_MAX_PATTERNS) return EMPTY_VALUE_16;
  if (patternIsEmpty(p, srcIdx)) return EMPTY_VALUE_16;
  int dst = findClosestEmptyPattern(p, srcIdx + 1);
  if (dst == EMPTY_VALUE_16) return EMPTY_VALUE_16;
  if (!editHistoryCapture(p, EditObject::pattern, dst)) return EMPTY_VALUE_16;
  p->patterns[dst] = p->patterns[srcIdx];
  // The clone does NOT inherit the original's chain mapping (it's a new
  // independent pattern). Pattern-to-chain map stays EMPTY_VALUE_16.
  p->patternToChainMap[dst] = EMPTY_VALUE_16;
  return dst;
}

// Deep-clone a pattern: clone the pattern AND each phrase referenced by its
// bars (mirrors deepCloneChain's behaviour for chains). Returns 1 on success,
// 0 on failure (out of phrase slots). Operates IN-PLACE on the source pattern
// (replaces its bar phrases with fresh clones), matching deepCloneChain's
// in-place semantics.
static int deepClonePattern(Project* p, int patternIdx) {
  if (!editHistoryCapture(p, EditObject::pattern, patternIdx)) return 0;
  if (patternIdx < 0 || patternIdx >= PROJECT_MAX_PATTERNS) return 0;
  if (patternIsEmpty(p, patternIdx)) return 0;
  Pattern* pat = &p->patterns[patternIdx];
  int len = pat->length;
  if (len < 1) len = 1;
  if (len > PROJECT_MAX_PATTERN_LENGTH) len = PROJECT_MAX_PATTERN_LENGTH;

  // Find distinct phrases and clone each once.
  uint16_t usedPhrases[PROJECT_MAX_PATTERN_LENGTH];
  uint16_t phraseMapping[PROJECT_MAX_PATTERN_LENGTH];
  int distinctCount = 0;
  for (int b = 0; b < len; b++) {
    uint16_t ph = pat->bars[b].phrase;
    if (ph == EMPTY_VALUE_16) {
      phraseMapping[b] = EMPTY_VALUE_16;
      continue;
    }
    int found = -1;
    for (int i = 0; i < distinctCount; i++) {
      if (usedPhrases[i] == ph) { found = i; break; }
    }
    if (found == -1) {
      // New phrase, find empty slot. Use the global findEmptyPhrase (which
      // scans from 0). For patterns we'd ideally scan from the pattern idx,
      // but findEmptyPhrase is the established helper.
      int newPhraseIdx = findEmptyPhrase(p, 0);
      if (newPhraseIdx == EMPTY_VALUE_16) return 0;
      clonePhrase(ph, newPhraseIdx);
      usedPhrases[distinctCount] = ph;
      phraseMapping[b] = (uint16_t)newPhraseIdx;
      distinctCount++;
    } else {
      // Reuse mapping from earlier bar with the same phrase.
      for (int i = 0; i < b; i++) {
        if (pat->bars[i].phrase == ph) {
          phraseMapping[b] = phraseMapping[i];
          break;
        }
      }
    }
  }
  for (int b = 0; b < len; b++) {
    if (phraseMapping[b] != EMPTY_VALUE_16) {
      pat->bars[b].phrase = phraseMapping[b];
    }
  }
  return 1;
}

// ── ViewMode-aware clone wrappers ────────────────────────────────────────────

int viewModeCloneToNext(Project* p, int srcIdx) {
  if (p->viewMode == ViewMode::simplified) {
    return clonePatternToNext(p, srcIdx);
  }
  // Classic-mode wrappers use the global chipnomadState->project (mirrors
  // copy_paste.cpp's cloneChainToNext). Caller is expected to pass the same
  // global.
  return cloneChainToNext(srcIdx);
}

int viewModeDeepClone(Project* p, int srcIdx) {
  if (p->viewMode == ViewMode::simplified) {
    return deepClonePattern(p, srcIdx);
  }
  return deepCloneChain(srcIdx);
}
