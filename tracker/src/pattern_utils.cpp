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

// ── Shared conversion helpers ───────────────────────────────────────────────
// The two conversion functions below implement the interchange spec:
//
//   classic → simplified:
//     Pattern number = chain number (or closest free pattern slot when
//     occupied). Length = consecutive non-empty chain rows (playback stops
//     at the first empty row, so the pattern must too). Bars reference the
//     chain rows' phrases IN ORDER. A chain-row transpose is BAKED into the
//     phrase's note values: the phrase is cloned into a fresh slot (chains[]
//     stays untouched — it is the stored original structure) and its notes
//     are shifted, clamped to the pitch table range. transpose == 0 bars
//     reference the original phrase directly.
//
//   simplified → classic:
//     Patterns are processed ascending (lowest number first). Mapped
//     patterns still in sync with their chain (neither side edited since
//     the last conversion) only get their song cells rewritten — the
//     saved conversion state is restored. Patterns that changed (edited
//     after the conversion) and unmapped ones allocate/rebuild a chain with
//     EXACTLY their bar count as phrase rows (remaining rows empty —
//     playback stops there, same as an empty pattern bar). Chain phrase
//     slots are allocated LOWEST-FREE-FIRST (scanning upward from 000), so
//     converted phrases pack compactly regardless of the pattern number.
//     Bars whose BAKED content is identical share one phrase slot.

// Note clamp used when baking a transpose into a phrase's notes. Matches
// the phrase editor's note ceiling: the last entry of the pitch table.
static uint8_t conversionMaxNote(Project* p) {
  int max = p->pitchTable.length - 1;
  if (max < 0) max = 0;
  if (max > 255) max = 255;
  return (uint8_t)max;
}

// Phrase-slot state for allocation scans: referenced-by-anything and/or
// non-empty. "referenced" = used by any chain row or pattern bar.
struct ConversionPhraseState {
  uint8_t referenced[PROJECT_MAX_PHRASES];
  uint8_t hasContent[PROJECT_MAX_PHRASES];
};

static void conversionBuildPhraseState(Project* p, ConversionPhraseState* st) {
  memset(st->referenced, 0, sizeof(st->referenced));
  memset(st->hasContent, 0, sizeof(st->hasContent));
  for (int c = 0; c < PROJECT_MAX_CHAINS; c++) {
    if (chainIsEmpty(p, c)) continue;
    for (int r = 0; r < 16; r++) {
      uint16_t ph = p->chains[c].rows[r].phrase;
      if (ph != EMPTY_VALUE_16 && ph < PROJECT_MAX_PHRASES)
        st->referenced[ph] = 1;
    }
  }
  for (int c = 0; c < PROJECT_MAX_PATTERNS; c++) {
    if (patternIsEmpty(p, c)) continue;
    int len = p->patterns[c].length;
    if (len < 1) len = 1;
    if (len > PROJECT_MAX_PATTERN_LENGTH) len = PROJECT_MAX_PATTERN_LENGTH;
    for (int b = 0; b < len; b++) {
      uint16_t ph = p->patterns[c].bars[b].phrase;
      if (ph != EMPTY_VALUE_16 && ph < PROJECT_MAX_PHRASES)
        st->referenced[ph] = 1;
    }
  }
  for (int i = 0; i < PROJECT_MAX_PHRASES; i++) {
    st->hasContent[i] = !phraseIsEmpty(p, i);
  }
}

// Allocate a chain phrase slot for pattern `patternIdx`, bar `bar`:
// prefer pattern+bar when empty AND unreferenced; otherwise the closest
// (outward scan) slot that is empty AND unreferenced; fall back to the
// closest slot that is merely empty (referenced-but-empty phrases are
// fair game — content is what matters). Returns EMPTY_VALUE_16 when no
// empty slot exists at all.
// Allocate a chain phrase slot for the next converted bar: the LOWEST free
// phrase number (scanning upward from 0 — the same convention as
// findEmptyPhrase), preferring slots that are empty AND unreferenced, then
// merely empty. Converted phrases therefore pack compactly from 000 upward
// instead of anchoring to the pattern number (user request: naming starts
// with the lowest possible number). Returns EMPTY_VALUE_16 when no empty
// slot exists at all.
static int conversionAllocatePhraseSlot(Project* p,
  const ConversionPhraseState* st, int patternIdx, int bar) {
  (void)p;
  (void)patternIdx;
  (void)bar;
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < PROJECT_MAX_PHRASES; i++) {
      if (!st->hasContent[i] && (pass == 1 || !st->referenced[i])) return i;
    }
  }
  return EMPTY_VALUE_16;
}

// Mark a phrase slot as taken in the allocator state so later bars of the
// same conversion cannot land on it.
static void conversionClaimSlot(ConversionPhraseState* st, int phrase) {
  if (phrase >= 0 && phrase < PROJECT_MAX_PHRASES) {
    st->referenced[phrase] = 1;
    st->hasContent[phrase] = 1;
  }
}

// Bake `transpose` into a COPY of src and place it in the LOWEST free
// phrase slot (same compact numbering as conversionAllocatePhraseSlot;
// scanning upward from 0). Never modifies src in place: chains[] must stay
// untouched during classic→simplified (it is the stored original). The slot
// is claimed in `st` so later bars cannot reuse it. Returns the new phrase
// index or EMPTY_VALUE_16 when the phrase pool is exhausted.
static int conversionBakeTranspose(Project* p, ConversionPhraseState* st,
  int src, int transpose) {
  int dst = conversionAllocatePhraseSlot(p, st, 0, 0);
  if (dst == EMPTY_VALUE_16) return EMPTY_VALUE_16;
  conversionClaimSlot(st, dst);

  p->phrases[dst] = p->phrases[src];
  const uint8_t maxNote = conversionMaxNote(p);
  for (int r = 0; r < 16; r++) {
    uint8_t note = p->phrases[dst].rows[r].note;
    if (note == EMPTY_VALUE_8 || note == NOTE_OFF) continue;
    int shifted = (int)note + transpose;
    if (shifted < 0) shifted = 0;
    if (shifted > maxNote) shifted = maxNote;
    p->phrases[dst].rows[r].note = (uint8_t)shifted;
  }
  return dst;
}

// Do two phrases hold identical content? Both structs are POD (verified:
// FXCell has no data-bearing helper members), so memcmp is exact. Callers
// must pass valid phrase indices.
static int conversionPhraseContentEqual(Project* p, int a, int b) {
  return memcmp(&p->phrases[a], &p->phrases[b], sizeof(Phrase)) == 0;
}

// ── Content comparison helpers (reconciliation) ─────────────────────────────
// Conversions RECONCILE the counterpart instead of blindly rebuilding it:
// rows/bars that already play the same music keep their phrase number AND
// transpose (the saved conversion state is recalled verbatim); only rows
// whose music changed are re-allocated. This is what makes round trips
// lossless and preserves the user's transposed rows.

// Does the phrase at `target` hold `src`'s content shifted by `transpose`
// (with the same clamping baking uses)? transpose == 0 → verbatim memcmp.
static int conversionShiftedContentEqual(Project* p, int src, int transpose,
  int target) {
  const Phrase* a = &p->phrases[src];
  const Phrase* b = &p->phrases[target];
  const uint8_t maxNote = conversionMaxNote(p);
  for (int n = 0; n < 16; n++) {
    const uint8_t* aRow = reinterpret_cast<const uint8_t*>(&a->rows[n]);
    const uint8_t* bRow = reinterpret_cast<const uint8_t*>(&b->rows[n]);
    uint8_t srcNote = a->rows[n].note;
    uint8_t dstNote = b->rows[n].note;
    if (transpose == 0) {
      if (dstNote != srcNote) return 0;
    } else if (srcNote == EMPTY_VALUE_8 || srcNote == NOTE_OFF) {
      if (dstNote != srcNote) return 0;
    } else {
      int expected = (int)srcNote + transpose;
      if (expected < 0) expected = 0;
      if (expected > maxNote) expected = maxNote;
      if (dstNote != (uint8_t)expected) return 0;
    }
    // Non-note columns are never touched by baking: compare per row
    // (note is the first member of each 16-byte row, so the rest of the
    // row — instrument, volume, FX — is contiguous after it).
    if (memcmp(aRow + 1, bRow + 1, sizeof(PhraseRow) - 1) != 0) return 0;
  }
  return 1;
}

// Two-sided variant: does phrase `a` with transpose `aTranspose` play the
// same music as phrase `b` with transpose `bTranspose`? Each side's played
// note is its stored note clamped after the shift; non-note columns compare
// verbatim.
static int conversionShiftedPairEqual(Project* p, int a, int aTranspose,
  int b, int bTranspose) {
  const Phrase* pa = &p->phrases[a];
  const Phrase* pb = &p->phrases[b];
  const uint8_t maxNote = conversionMaxNote(p);
  for (int n = 0; n < 16; n++) {
    uint8_t aNote = pa->rows[n].note;
    uint8_t bNote = pb->rows[n].note;
    int playedA;
    if (aNote == EMPTY_VALUE_8 || aNote == NOTE_OFF || aTranspose == 0) {
      playedA = aNote;
    } else {
      int v = (int)aNote + aTranspose;
      playedA = v < 0 ? 0 : v > maxNote ? maxNote : v;
    }
    int playedB;
    if (bNote == EMPTY_VALUE_8 || bNote == NOTE_OFF || bTranspose == 0) {
      playedB = bNote;
    } else {
      int v = (int)bNote + bTranspose;
      playedB = v < 0 ? 0 : v > maxNote ? maxNote : v;
    }
    if (playedA != playedB) return 0;
    const uint8_t* aRow = reinterpret_cast<const uint8_t*>(&pa->rows[n]);
    const uint8_t* bRow = reinterpret_cast<const uint8_t*>(&pb->rows[n]);
    if (memcmp(aRow + 1, bRow + 1, sizeof(PhraseRow) - 1) != 0) return 0;
  }
  return 1;
}

// Append ALL of `srcIdx`'s bars to the end of `targetIdx`'s bars.
// Returns the new target length, or -1 when the merged length would exceed
// PROJECT_MAX_PATTERN_LENGTH (no changes made in that case) or src is empty.
// Bars are copied as references (phrase + hidden transpose).
int patternAppendPattern(Project* p, int targetIdx, int srcIdx) {
  Pattern* target = &p->patterns[targetIdx];
  const Pattern* src = &p->patterns[srcIdx];
  int tLen = target->length;
  if (tLen < 1) tLen = 1;
  if (tLen > PROJECT_MAX_PATTERN_LENGTH) tLen = PROJECT_MAX_PATTERN_LENGTH;
  int sLen = src->length;
  if (sLen < 1) sLen = 1;
  if (sLen > PROJECT_MAX_PATTERN_LENGTH) sLen = PROJECT_MAX_PATTERN_LENGTH;
  // An empty source contributes nothing.
  if (patternIsEmpty(p, srcIdx)) return -1;
  int merged = tLen + sLen;
  if (merged > PROJECT_MAX_PATTERN_LENGTH) return -1;  // would overflow
  for (int b = 0; b < sLen; b++) {
    target->bars[tLen + b] = src->bars[b];
  }
  target->length = (uint8_t)merged;
  return merged;
}

// Clone-on-write for simplified-mode bar editing: when the phrase bound to
// `barIdx` of `patternIdx` is ALSO referenced anywhere else (another pattern
// bar, or any chain row), copy it into a fresh lowest-free slot and point
// only THIS bar at the copy — so editing one bar of a multiplied pattern
// never mutates its siblings or the classic view's phrases. Returns the
// (possibly new) phrase index, or EMPTY_VALUE_16 when the pool is exhausted.
// The clone claims the slot in `st` when provided.
int patternEnsureBarExclusive(Project* p, ConversionPhraseState* st,
  int patternIdx, int barIdx) {
  Pattern* pat = &p->patterns[patternIdx];
  uint16_t phrase = pat->bars[barIdx].phrase;
  if (phrase == EMPTY_VALUE_16 || phrase >= PROJECT_MAX_PHRASES) return phrase;

  int otherRefs = 0;
  for (int c = 0; c < PROJECT_MAX_CHAINS && otherRefs == 0; c++) {
    if (chainIsEmpty(p, c)) continue;
    for (int r = 0; r < 16; r++) {
      if (p->chains[c].rows[r].phrase == phrase) { otherRefs = 1; break; }
    }
  }
  for (int c = 0; c < PROJECT_MAX_PATTERNS && otherRefs == 0; c++) {
    if (patternIsEmpty(p, c)) continue;
    int len = p->patterns[c].length;
    if (len < 1) len = 1;
    if (len > PROJECT_MAX_PATTERN_LENGTH) len = PROJECT_MAX_PATTERN_LENGTH;
    for (int b = 0; b < len; b++) {
      if (c == patternIdx && b == barIdx) continue;
      if (p->patterns[c].bars[b].phrase == phrase) { otherRefs = 1; break; }
    }
  }
  if (!otherRefs) return phrase;  // already exclusive

  int fresh = st ? conversionAllocatePhraseSlot(p, st, 0, 0)
                 : patternFindClosestEmptyPhrase(p, patternIdx);
  if (fresh == EMPTY_VALUE_16) return EMPTY_VALUE_16;  // pool exhausted
  p->phrases[fresh] = p->phrases[phrase];
  if (st) conversionClaimSlot(st, fresh);
  pat->bars[barIdx].phrase = (uint16_t)fresh;
  pat->bars[barIdx].transpose = 0;
  return fresh;
}

// Collect the unique cell indices referenced by the song grid into out[]
// (ascending). Returns the count; out must hold PROJECT_MAX_CHAINS entries.
// Used by both conversions to process each converted unit exactly once.
static int conversionCollectSongCells(Project* p, int simplified,
  uint16_t* out) {
  int count = 0;
  for (int row = 0; row < PROJECT_MAX_LENGTH; row++) {
    for (int track = 0; track < PROJECT_MAX_TRACKS; track++) {
      uint16_t cell = p->song[row][track];
      if (cell == EMPTY_VALUE_16) continue;
      int limit = simplified ? PROJECT_MAX_PATTERNS : PROJECT_MAX_CHAINS;
      if (cell >= (uint16_t)limit) continue;  // defensive

      // Linear insert into the ascending list (tiny N, called rarely).
      int pos = 0;
      while (pos < count && out[pos] < cell) pos++;
      if (pos < count && out[pos] == cell) continue;
      if (pos < count) memmove(&out[pos + 1], &out[pos],
        (count - pos) * sizeof(uint16_t));
      out[pos] = cell;
      count++;
    }
  }
  return count;
}

// ── Convert chain → pattern (classic→simplified) ────────────────────────────
// See the block comment above "Shared conversion helpers" for the contract.
// Returns 0 on success; non-zero on a fatal error (slots exhausted) — the
// project is left in an indeterminate state and the caller should reload
// from disk (pre-existing contract; viewMode is flipped LAST so a failed
// call does not silently change the mode).

// Reconcile `pat` with the chain's CURRENT rows: consecutive non-empty rows
// are the pattern's bars. A bar that already plays the row's music (content
// shifted by the row's signed transpose) is left untouched — phrase number
// AND hidden transpose preserved (saved conversion state recalled). Bars
// whose music changed are baked into fresh lowest-free slots. Rows beyond
// the chain's run are dropped from the pattern's active length. Returns 0
// on success.
static int conversionReconcilePatternFromChain(Project* p,
  ConversionPhraseState* st, int chainIdx, int patternIdx) {
  Pattern* pat = &p->patterns[patternIdx];
  const Chain* ch = &p->chains[chainIdx];

  int barCount = 0;
  for (int r = 0; r < 16 && barCount < PROJECT_MAX_PATTERN_LENGTH; r++) {
    uint16_t phrase = ch->rows[r].phrase;
    if (phrase == EMPTY_VALUE_16) break;
    // transpose is a SIGNED value in a uint8_t: FF = -1 (playback casts
    // it to int8_t). Read it as signed or FF bakes as +255.
    int transpose = (int8_t)ch->rows[r].transpose;
    uint16_t curBar = pat->bars[barCount].phrase;
    if (curBar != EMPTY_VALUE_16 && curBar < PROJECT_MAX_PHRASES &&
        conversionShiftedContentEqual(p, phrase, transpose, curBar)) {
      // Same music: keep the bar exactly as saved.
      pat->bars[barCount].transpose = 0;  // bars never carry transpose
    } else if (transpose != 0) {
      // Music changed and the row is transposed: bake into a fresh slot.
      int baked = conversionBakeTranspose(p, st, phrase, transpose);
      if (baked == EMPTY_VALUE_16) return 2;  // phrases full
      pat->bars[barCount].phrase = (uint16_t)baked;
      pat->bars[barCount].transpose = 0;
    } else {
      // Music changed, untransposed: point the bar at the row's phrase.
      pat->bars[barCount].phrase = phrase;
      pat->bars[barCount].transpose = 0;
    }
    barCount++;
  }

  // Edge case: empty chain (zero non-empty rows) → 1-bar pattern with a
  // freshly allocated empty phrase, lowest free slot (spec decision #8).
  if (barCount == 0) {
    uint16_t curBar = pat->bars[0].phrase;
    if (curBar == EMPTY_VALUE_16 ||
        (curBar < PROJECT_MAX_PHRASES && !phraseIsEmpty(p, curBar))) {
      int emptyPhrase = conversionAllocatePhraseSlot(p, st, 0, 0);
      if (emptyPhrase == EMPTY_VALUE_16) return 2;  // phrases full
      conversionClaimSlot(st, emptyPhrase);
      pat->bars[0].phrase = (uint16_t)emptyPhrase;
    }
    pat->bars[0].transpose = 0;
    barCount = 1;
  }

  pat->length = (uint8_t)barCount;
  return 0;
}

int convertChainsToPatterns(Project* p) {
  uint16_t cells[PROJECT_MAX_CHAINS];
  int count = conversionCollectSongCells(p, 0, cells);

  // Shared lowest-free phrase allocator state (baked clones + the
  // empty-chain edge phrase claim slots here).
  ConversionPhraseState st;
  conversionBuildPhraseState(p, &st);

  for (int ci = 0; ci < count; ci++) {
    int chainIdx = cells[ci];
    uint16_t patternIdx = p->chainToPatternMap[chainIdx];
    const int wasMapped = patternIdx != EMPTY_VALUE_16;

    if (!wasMapped) {
      // Pattern number = chain number when free, else closest free slot.
      if (patternIsEmpty(p, chainIdx) &&
          p->patternToChainMap[chainIdx] == EMPTY_VALUE_16) {
        patternIdx = (uint16_t)chainIdx;
      } else {
        patternIdx = (uint16_t)findClosestEmptyPattern(p, chainIdx);
      }
      if (patternIdx == EMPTY_VALUE_16) return 1;  // patterns full
      p->chainToPatternMap[chainIdx] = patternIdx;
      p->patternToChainMap[patternIdx] = chainIdx;
    }

    // Mapped or fresh: reconcile the pattern against the chain's current
    // content — matching bars keep their saved phrase slots, changed ones
    // rebuild (classic edits propagate).
    int result = conversionReconcilePatternFromChain(p, &st, chainIdx,
      patternIdx);
    if (result != 0) return result;
  }

  // Rewrite every song cell: chain index → its pattern.
  for (int row = 0; row < PROJECT_MAX_LENGTH; row++) {
    for (int track = 0; track < PROJECT_MAX_TRACKS; track++) {
      uint16_t chainIdx = p->song[row][track];
      if (chainIdx == EMPTY_VALUE_16) continue;
      if (chainIdx >= PROJECT_MAX_CHAINS) continue;
      uint16_t patternIdx = p->chainToPatternMap[chainIdx];
      if (patternIdx == EMPTY_VALUE_16) return 1;  // unmapped chain: patterns full earlier
      p->song[row][track] = patternIdx;
    }
  }

  p->viewMode = ViewMode::simplified;
  return 0;
}

// ── Convert pattern → chain (simplified→classic) ────────────────────────────
// See the block comment above "Shared conversion helpers" for the contract.
// The leaving simplified mode is the source of truth: mapped patterns ALSO
// get their chains REBUILT from the pattern's current bars (simplified-mode
// edits propagate), exactly like unmapped ones.
// Returns 0 on success; non-zero on a fatal error (slots exhausted).

// Reconcile the chain at `chainIdx` with the pattern's CURRENT bars: the
// chain gets exactly the pattern's bar count as phrase rows (remaining rows
// empty). A row that already plays the bar's music keeps its phrase number
// AND transpose (saved conversion state recalled); rows whose music changed
// are re-allocated (transposed bars bake into fresh lowest-free slots).
// Bars with identical baked content share one phrase. Returns 0 on success.
static int conversionReconcileChainFromPattern(Project* p,
  ConversionPhraseState* st, int patternIdx, int chainIdx) {
  Chain* ch = &p->chains[chainIdx];
  Pattern* pat = &p->patterns[patternIdx];
  int len = pat->length;
  if (len < 1) len = 1;
  if (len > PROJECT_MAX_PATTERN_LENGTH) len = PROJECT_MAX_PATTERN_LENGTH;
  pat->length = (uint8_t)len;

  // Track which chain rows this reconciliation has already consumed so a
  // later bar cannot reuse a row that was kept for an earlier position.
  const Chain saved = *ch;
  bool rowUsed[16];
  for (int r = 0; r < 16; r++) rowUsed[r] = false;

  // Within this pattern: phrase chosen for each distinct baked content.
  // barPhrase[b] = chain phrase index for bar b.
  uint16_t barPhrase[PROJECT_MAX_PATTERN_LENGTH];
  for (int b = 0; b < len; b++) {
    uint16_t srcPhrase = pat->bars[b].phrase;

    // Keep an existing row when it already plays this bar's music (row
    // content shifted by the row transpose == bar content shifted by the
    // bar transpose). Kept rows keep their saved phrase number AND
    // transpose — the conversion state is recalled verbatim.
    bool kept = false;
    if (srcPhrase != EMPTY_VALUE_16 && srcPhrase < PROJECT_MAX_PHRASES) {
      const int barTranspose = (int8_t)pat->bars[b].transpose;
      for (int r = 0; r < 16; r++) {
        if (rowUsed[r]) continue;
        uint16_t rowPhrase = saved.rows[r].phrase;
        if (rowPhrase == EMPTY_VALUE_16 || rowPhrase >= PROJECT_MAX_PHRASES)
          continue;
        const int rowTranspose = (int8_t)saved.rows[r].transpose;
        if (!conversionShiftedPairEqual(p, rowPhrase, rowTranspose,
                                        srcPhrase, barTranspose)) {
          continue;
        }
        ch->rows[b].phrase = rowPhrase;
        ch->rows[b].transpose = saved.rows[r].transpose;
        rowUsed[r] = true;
        barPhrase[b] = rowPhrase;
        kept = true;
        break;
      }
    }
    if (kept) continue;

    // No saved row plays this bar's music: rebuild the row.
    if (srcPhrase == EMPTY_VALUE_16) {
      barPhrase[b] = EMPTY_VALUE_16;
      ch->rows[b].phrase = EMPTY_VALUE_16;
      ch->rows[b].transpose = 0;
      continue;
    }

    int transpose = (int8_t)pat->bars[b].transpose;

    // Dedupe within the pattern: an earlier bar whose baked content is
    // identical shares its phrase slot.
    int reuse = -1;
    for (int prev = 0; prev < b; prev++) {
      if (barPhrase[prev] == EMPTY_VALUE_16) continue;
      int prevSrc = pat->bars[prev].phrase;
      int prevT = (int8_t)pat->bars[prev].transpose;
      if (transpose == prevT && prevSrc == srcPhrase) {
        reuse = prev;  // identical source and transpose
        break;
      }
      if (transpose == 0 && prevT == 0 &&
          conversionPhraseContentEqual(p, srcPhrase, prevSrc)) {
        reuse = prev;  // untransposed bars with identical content
        break;
      }
    }

    if (reuse >= 0) {
      barPhrase[b] = barPhrase[reuse];
      ch->rows[b].phrase = barPhrase[reuse];
      ch->rows[b].transpose = 0;
      continue;
    }

    int slot;
    if (transpose != 0) {
      // Bake into a fresh lowest-free slot.
      int baked = conversionBakeTranspose(p, st, srcPhrase, transpose);
      if (baked == EMPTY_VALUE_16) return 2;  // phrases full
      slot = baked;
    } else {
      slot = conversionAllocatePhraseSlot(p, st, 0, 0);
      if (slot == EMPTY_VALUE_16) return 2;  // phrases full
      // Copy (don't move) the content so the pattern keeps its own phrase
      // data — both structures coexist for the way back.
      p->phrases[slot] = p->phrases[srcPhrase];
    }
    conversionClaimSlot(st, slot);
    barPhrase[b] = (uint16_t)slot;
    ch->rows[b].phrase = (uint16_t)slot;
    ch->rows[b].transpose = 0;
  }

  // Rows beyond the pattern length: empty (playback stops there).
  for (int r = len; r < 16; r++) {
    ch->rows[r].phrase = EMPTY_VALUE_16;
    ch->rows[r].transpose = 0;
  }
  return 0;
}

int convertPatternsToChains(Project* p) {
  uint16_t cells[PROJECT_MAX_PATTERNS];
  int count = conversionCollectSongCells(p, 1, cells);

  // Phrase allocator state: built once, updated as slots are claimed.
  ConversionPhraseState st;
  conversionBuildPhraseState(p, &st);

  for (int pi = 0; pi < count; pi++) {
    int patternIdx = cells[pi];
    uint16_t chainIdx = p->patternToChainMap[patternIdx];
    const int wasMapped = chainIdx != EMPTY_VALUE_16;

    if (!wasMapped) {
      // Unmapped pattern: allocate a chain (closest free slot).
      chainIdx = (uint16_t)findClosestEmptyChain(p, patternIdx);
      if (chainIdx == EMPTY_VALUE_16) return 1;  // chains full
      p->patternToChainMap[patternIdx] = chainIdx;
      p->chainToPatternMap[chainIdx] = patternIdx;
      chainClear(&p->chains[chainIdx]);
    }

    // Mapped or fresh: reconcile the chain against the pattern's current
    // content — matching rows keep their saved phrase slots and transposes,
    // changed ones rebuild (simplified edits propagate).
    int result = conversionReconcileChainFromPattern(p, &st, patternIdx,
      chainIdx);
    if (result != 0) return result;
  }

  // Rewrite every song cell: pattern index → its chain.
  for (int row = 0; row < PROJECT_MAX_LENGTH; row++) {
    for (int track = 0; track < PROJECT_MAX_TRACKS; track++) {
      uint16_t patternIdx = p->song[row][track];
      if (patternIdx == EMPTY_VALUE_16) continue;
      if (patternIdx >= PROJECT_MAX_PATTERNS) continue;
      uint16_t chainIdx = p->patternToChainMap[patternIdx];
      if (chainIdx == EMPTY_VALUE_16) return 1;
      p->song[row][track] = chainIdx;
    }
  }

  p->viewMode = ViewMode::classic;
  return 0;
}