// Tests for the simplified "patterns" view feature.
//
// Covers:
//   - Pattern struct + patternClear / patternIsEmpty
//   - Chain → Pattern conversion (decision #1, #5, #8)
//   - Pattern → Chain conversion (reversibility, decision #5)
//   - Per-bar table override (decision #2, #3, #7)
//   - Length editing: ×2 / ÷2 / +1 / -1 (decision #4)
//   - .cnm round-trip (View mode header + ## Patterns section)
//   - Playback resolver: playbackResolvePhraseAt / Length / Transpose / BarTable
//     for both classic and simplified view modes.

#include "doctest.h"
#include "chipnomad_lib.h"
#include "playback_internal.h"
#include "pattern_utils.h"
#include "project_utils.h"
#include "pitch_table_utils.h"
#include <cstdio>
#include <cstring>
#include <memory>

struct PatternFixture {
  std::unique_ptr<Project> project;
  PatternFixture() : project(std::make_unique<Project>()) {
    projectInit(project.get());
    // Set up a basic AY project so tracksCount etc. are correct.
    projectInitAY(project.get());
    fillFXNames();
  }
};

// Helper: populate a chain with phrases for testing.
// Realistic data: empty rows (beyond `count`) have transpose=0, matching
// what chainClear produces. This lets the round-trip test verify byte-for-byte
// equality after convertChainsToPatterns → convertPatternsToChains (which
// now regenerates the chain from the pattern, see convertPatternsToChains).
static void setupTestChain(Project* p, int chainIdx, int phraseIdx0, int count) {
  for (int i = 0; i < 16; i++) {
    p->chains[chainIdx].rows[i].phrase = (i < count) ? (phraseIdx0 + i) : EMPTY_VALUE_16;
    p->chains[chainIdx].rows[i].transpose = (i < count) ? (uint8_t)(i * 2) : 0;
  }
  // Put some notes in the phrases so phraseHasNotes returns true.
  for (int i = 0; i < count; i++) {
    p->phrases[phraseIdx0 + i].rows[0].note = 48 + i;  // C-4, C#-4, etc.
  }
}

TEST_CASE_FIXTURE(PatternFixture, "patternClear sets length=1 with empty bar") {
  Pattern pat;
  patternClear(&pat);
  CHECK(pat.length == PROJECT_DEFAULT_PATTERN_LENGTH);
  CHECK(pat.length == 1);
  for (int b = 0; b < PROJECT_MAX_PATTERN_LENGTH; b++) {
    CHECK(pat.bars[b].phrase == EMPTY_VALUE_16);
    CHECK(pat.bars[b].transpose == 0);
  }
}

TEST_CASE_FIXTURE(PatternFixture, "patternIsEmpty: fresh pattern is empty") {
  CHECK(patternIsEmpty(project.get(), 0));
  CHECK(patternIsEmpty(project.get(), PROJECT_MAX_PATTERNS - 1));
  // Out of range returns empty.
  CHECK(patternIsEmpty(project.get(), -1));
  CHECK(patternIsEmpty(project.get(), PROJECT_MAX_PATTERNS));
}

TEST_CASE_FIXTURE(PatternFixture, "patternIsEmpty: pattern with a phrase is not empty") {
  project->patterns[5].length = 1;
  project->patterns[5].bars[0].phrase = 10;
  CHECK_FALSE(patternIsEmpty(project.get(), 5));
}

TEST_CASE_FIXTURE(PatternFixture, "patternIsEmpty: respects length") {
  // Length 2 but bars 0,1 empty → empty.
  project->patterns[5].length = 2;
  project->patterns[5].bars[0].phrase = EMPTY_VALUE_16;
  project->patterns[5].bars[1].phrase = EMPTY_VALUE_16;
  project->patterns[5].bars[2].phrase = 10;  // beyond length, shouldn't count
  CHECK(patternIsEmpty(project.get(), 5));
}

TEST_CASE_FIXTURE(PatternFixture, "convertChainsToPatterns: basic conversion preserves phrases and transpose") {
  // Set up a chain at index 3 with 3 phrases (10, 11, 12) and transposes 0, 2, 4.
  setupTestChain(project.get(), 3, 10, 3);
  project->song[0][0] = 3;  // song cell points to chain 3

  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  CHECK(project->viewMode == ViewMode::simplified);

  // The song cell should now point to a pattern index. The pattern index is
  // allocated by findClosestEmptyPattern(p, 3) which prefers index 3 if it's
  // empty (which it is by default). So patternIdx may or may not equal 3 —
  // either way it should be a valid pattern index.
  uint16_t patternIdx = project->song[0][0];
  CHECK(patternIdx < PROJECT_MAX_PATTERNS);

  // The mapping should be recorded.
  CHECK(project->chainToPatternMap[3] == patternIdx);
  CHECK(project->patternToChainMap[patternIdx] == 3);

  // The pattern should have length 3 and matching phrases + transposes.
  Pattern* pat = &project->patterns[patternIdx];
  CHECK(pat->length == 3);
  CHECK(pat->bars[0].phrase == 10);
  CHECK(pat->bars[0].transpose == 0);
  CHECK(pat->bars[1].phrase == 11);
  CHECK(pat->bars[1].transpose == 2);
  CHECK(pat->bars[2].phrase == 12);
  CHECK(pat->bars[2].transpose == 4);
}

TEST_CASE_FIXTURE(PatternFixture, "convertChainsToPatterns: empty chain becomes 1-bar pattern with empty phrase (decision #8)") {
  // Empty chain at index 5 (chainClear already set all rows to EMPTY_VALUE_16).
  project->song[0][0] = 5;

  REQUIRE(convertChainsToPatterns(project.get()) == 0);

  uint16_t patternIdx = project->song[0][0];
  Pattern* pat = &project->patterns[patternIdx];
  CHECK(pat->length == 1);
  // The single bar should reference an empty phrase.
  CHECK(pat->bars[0].phrase != EMPTY_VALUE_16);
  CHECK(pat->bars[0].phrase < PROJECT_MAX_PHRASES);
  CHECK(phraseIsEmpty(project.get(), pat->bars[0].phrase));
}

TEST_CASE_FIXTURE(PatternFixture, "convertChainsToPatterns: chains[] untouched (decision #5)") {
  setupTestChain(project.get(), 3, 10, 3);
  project->song[0][0] = 3;

  // Snapshot the chain's content before conversion.
  Chain original = project->chains[3];

  REQUIRE(convertChainsToPatterns(project.get()) == 0);

  // The chain should be byte-identical after conversion.
  CHECK(std::memcmp(&project->chains[3], &original, sizeof(Chain)) == 0);
}

TEST_CASE_FIXTURE(PatternFixture, "convertPatternsToChains: reverses the first conversion") {
  setupTestChain(project.get(), 3, 10, 3);
  project->song[0][0] = 3;

  // classic → simplified → classic should restore the original chain.
  Chain original = project->chains[3];

  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  REQUIRE(convertPatternsToChains(project.get()) == 0);
  CHECK(project->viewMode == ViewMode::classic);

  // Song cell should point back to chain 3.
  CHECK(project->song[0][0] == 3);
  // Chain 3 should be unchanged.
  CHECK(std::memcmp(&project->chains[3], &original, sizeof(Chain)) == 0);
}

TEST_CASE_FIXTURE(PatternFixture, "convertPatternsToChains: regenerates chain from edited pattern (issue #6)") {
  // Scenario: start classic with an empty chain, switch to simplified,
  // edit the pattern (add bars + notes), switch back to classic — the
  // chain should reflect the pattern's content (not stay empty).
  // Per the user-facing spec: "Phrase values in chain should be generated
  // according to the logic from the first prompt".
  project->song[0][0] = 5;  // chain 5 (empty)
  // chain 5 is empty by default (chainClear sets all rows to EMPTY_VALUE_16).

  // classic → simplified: empty chain becomes 1-bar pattern with empty phrase.
  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  uint16_t patternIdx = project->song[0][0];
  REQUIRE(patternIdx < PROJECT_MAX_PATTERNS);
  Pattern* pat = &project->patterns[patternIdx];
  CHECK(pat->length == 1);
  uint16_t phrase0 = pat->bars[0].phrase;
  CHECK(phrase0 != EMPTY_VALUE_16);

  // Add a second bar to the pattern (creates a fresh empty phrase).
  REQUIRE(patternAddEmptyBar(project.get(), patternIdx));
  CHECK(pat->length == 2);
  uint16_t phrase1 = pat->bars[1].phrase;
  CHECK(phrase1 != EMPTY_VALUE_16);
  CHECK(phrase1 != phrase0);

  // Add some notes to both phrases so they're not empty.
  project->phrases[phrase0].rows[0].note = 48;
  project->phrases[phrase1].rows[0].note = 50;

  // Switch back to classic: chain should be regenerated from the pattern.
  REQUIRE(convertPatternsToChains(project.get()) == 0);
  CHECK(project->viewMode == ViewMode::classic);

  uint16_t chainIdx = project->song[0][0];
  CHECK(chainIdx < PROJECT_MAX_CHAINS);
  Chain* ch = &project->chains[chainIdx];
  // The chain's first 2 rows should match the pattern's bars.
  CHECK(ch->rows[0].phrase == phrase0);
  CHECK(ch->rows[1].phrase == phrase1);
  // Remaining rows should be empty (chainClear).
  CHECK(ch->rows[2].phrase == EMPTY_VALUE_16);
  CHECK(ch->rows[15].phrase == EMPTY_VALUE_16);
}

TEST_CASE_FIXTURE(PatternFixture, "patternMultiplyLengthByTwo: duplicates bars (decision #4)") {
  // 1-bar pattern with phrase 5 → ×2 → 2-bar with phrase 5 in both bars.
  project->patterns[0].length = 1;
  project->patterns[0].bars[0].phrase = 5;
  project->patterns[0].bars[0].transpose = 3;

  REQUIRE(patternMultiplyLengthByTwo(project.get(), 0));
  CHECK(project->patterns[0].length == 2);
  CHECK(project->patterns[0].bars[0].phrase == 5);
  CHECK(project->patterns[0].bars[0].transpose == 3);
  CHECK(project->patterns[0].bars[1].phrase == 5);
  CHECK(project->patterns[0].bars[1].transpose == 3);
}

TEST_CASE_FIXTURE(PatternFixture, "patternMultiplyLengthByTwo: 2-bar A,B → 4-bar A,B,A,B") {
  project->patterns[0].length = 2;
  project->patterns[0].bars[0].phrase = 5;
  project->patterns[0].bars[1].phrase = 7;

  REQUIRE(patternMultiplyLengthByTwo(project.get(), 0));
  CHECK(project->patterns[0].length == 4);
  CHECK(project->patterns[0].bars[0].phrase == 5);
  CHECK(project->patterns[0].bars[1].phrase == 7);
  CHECK(project->patterns[0].bars[2].phrase == 5);
  CHECK(project->patterns[0].bars[3].phrase == 7);
}

TEST_CASE_FIXTURE(PatternFixture, "patternMultiplyLengthByTwo: refuses if would exceed 16") {
  project->patterns[0].length = 9;  // 9 * 2 = 18 > 16
  REQUIRE_FALSE(patternMultiplyLengthByTwo(project.get(), 0));
  CHECK(project->patterns[0].length == 9);  // unchanged

  // 8 * 2 = 16 is OK.
  project->patterns[0].length = 8;
  REQUIRE(patternMultiplyLengthByTwo(project.get(), 0));
  CHECK(project->patterns[0].length == 16);

  // 16 * 2 = 32 > 16, refused.
  REQUIRE_FALSE(patternMultiplyLengthByTwo(project.get(), 0));
  CHECK(project->patterns[0].length == 16);
}

TEST_CASE_FIXTURE(PatternFixture, "patternHalveLengthByTwo: drops second half") {
  project->patterns[0].length = 4;
  project->patterns[0].bars[0].phrase = 5;
  project->patterns[0].bars[1].phrase = 7;
  project->patterns[0].bars[2].phrase = 9;
  project->patterns[0].bars[3].phrase = 11;

  REQUIRE(patternHalveLengthByTwo(project.get(), 0));
  CHECK(project->patterns[0].length == 2);
  // First half survives; second half bars are still in the struct (for undo)
  // but not active.
  CHECK(project->patterns[0].bars[0].phrase == 5);
  CHECK(project->patterns[0].bars[1].phrase == 7);
  CHECK(project->patterns[0].bars[2].phrase == 9);   // not zeroed
  CHECK(project->patterns[0].bars[3].phrase == 11);   // not zeroed
}

TEST_CASE_FIXTURE(PatternFixture, "patternHalveLengthByTwo: refuses odd length and length 1") {
  project->patterns[0].length = 3;
  REQUIRE_FALSE(patternHalveLengthByTwo(project.get(), 0));

  project->patterns[0].length = 1;
  REQUIRE_FALSE(patternHalveLengthByTwo(project.get(), 0));
}

TEST_CASE_FIXTURE(PatternFixture, "patternAddEmptyBar: appends a bar with empty phrase") {
  project->patterns[0].length = 1;
  project->patterns[0].bars[0].phrase = 5;

  REQUIRE(patternAddEmptyBar(project.get(), 0));
  CHECK(project->patterns[0].length == 2);
  // New bar has an empty phrase (allocated by findClosestEmptyPhrase).
  CHECK(project->patterns[0].bars[1].phrase != EMPTY_VALUE_16);
  CHECK(project->patterns[0].bars[1].phrase < PROJECT_MAX_PHRASES);
  CHECK(phraseIsEmpty(project.get(), project->patterns[0].bars[1].phrase));
  // Original bar unchanged.
  CHECK(project->patterns[0].bars[0].phrase == 5);
}

TEST_CASE_FIXTURE(PatternFixture, "patternAddEmptyBar: refuses at max length") {
  project->patterns[0].length = PROJECT_MAX_PATTERN_LENGTH;
  REQUIRE_FALSE(patternAddEmptyBar(project.get(), 0));
  CHECK(project->patterns[0].length == PROJECT_MAX_PATTERN_LENGTH);
}

TEST_CASE_FIXTURE(PatternFixture, "patternRemoveLastBar: decrements length") {
  project->patterns[0].length = 3;
  project->patterns[0].bars[0].phrase = 5;
  project->patterns[0].bars[1].phrase = 7;
  project->patterns[0].bars[2].phrase = 9;

  REQUIRE(patternRemoveLastBar(project.get(), 0));
  CHECK(project->patterns[0].length == 2);
  // Last bar's data survives (for undo).
  CHECK(project->patterns[0].bars[2].phrase == 9);
}

TEST_CASE_FIXTURE(PatternFixture, "patternRemoveLastBar: refuses at length 1") {
  project->patterns[0].length = 1;
  REQUIRE_FALSE(patternRemoveLastBar(project.get(), 0));
}

TEST_CASE_FIXTURE(PatternFixture, "playbackResolvePhraseAt: classic mode reads from chain") {
  // Classic mode: song cell is chain index.
  project->viewMode = ViewMode::classic;
  project->song[0][0] = 5;
  project->chains[5].rows[2].phrase = 42;

  CHECK(playbackResolvePhraseAt(project.get(), 0, 0, 2) == 42);
  // Out-of-range chain row.
  CHECK(playbackResolvePhraseAt(project.get(), 0, 0, 16) == EMPTY_VALUE_16);
}

TEST_CASE_FIXTURE(PatternFixture, "playbackResolvePhraseAt: simplified mode reads from pattern bar") {
  project->viewMode = ViewMode::simplified;
  project->song[0][0] = 5;
  project->patterns[5].length = 3;
  project->patterns[5].bars[1].phrase = 42;

  CHECK(playbackResolvePhraseAt(project.get(), 0, 0, 1) == 42);
  // Beyond pattern length returns empty.
  CHECK(playbackResolvePhraseAt(project.get(), 0, 0, 3) == EMPTY_VALUE_16);
}

TEST_CASE_FIXTURE(PatternFixture, "playbackResolveLengthAt: classic=16, simplified=pattern length") {
  project->viewMode = ViewMode::classic;
  project->song[0][0] = 5;
  CHECK(playbackResolveLengthAt(project.get(), 0, 0) == 16);

  project->viewMode = ViewMode::simplified;
  project->patterns[5].length = 7;
  CHECK(playbackResolveLengthAt(project.get(), 0, 0) == 7);
}

TEST_CASE_FIXTURE(PatternFixture, "playbackResolveTransposeAt: classic from chain, simplified from bar (decision #1)") {
  project->viewMode = ViewMode::classic;
  project->song[0][0] = 5;
  project->chains[5].rows[2].transpose = 7;
  CHECK(playbackResolveTransposeAt(project.get(), 0, 0, 2) == 7);

  project->viewMode = ViewMode::simplified;
  project->patterns[5].length = 3;
  project->patterns[5].bars[1].transpose = 5;
  CHECK(playbackResolveTransposeAt(project.get(), 0, 0, 1) == 5);
}

TEST_CASE_FIXTURE(PatternFixture, ".cnm round-trip: saves and loads viewMode + patterns section") {
  // Set up a project with patterns.
  project->viewMode = ViewMode::simplified;
  project->patterns[3].length = 2;
  project->patterns[3].bars[0].phrase = 10;
  project->patterns[3].bars[0].transpose = 5;
  project->patterns[3].bars[1].phrase = 11;
  project->patternToChainMap[3] = 5;  // origin chain
  project->chainToPatternMap[5] = 3;
  project->song[0][0] = 3;

  // Save.
  const char* path = "build/tests/patterns_simplified_roundtrip.cct";
  REQUIRE(projectSave(project.get(), path) == 0);

  // Load into a fresh project.
  auto loaded = std::make_unique<Project>();
  projectInit(loaded.get());
  projectInitAY(loaded.get());
  int result = projectLoad(loaded.get(), path);
  INFO("projectFileError: ", projectFileError);
  REQUIRE(result == 0);

  // View mode preserved.
  CHECK(loaded->viewMode == ViewMode::simplified);

  // Pattern preserved.
  CHECK(loaded->patterns[3].length == 2);
  CHECK(loaded->patterns[3].bars[0].phrase == 10);
  CHECK(loaded->patterns[3].bars[0].transpose == 5);
  CHECK(loaded->patterns[3].bars[1].phrase == 11);

  // Conversion map preserved.
  CHECK(loaded->patternToChainMap[3] == 5);
  CHECK(loaded->chainToPatternMap[5] == 3);

  // Song cell preserved.
  CHECK(loaded->song[0][0] == 3);

  std::remove(path);
}

TEST_CASE_FIXTURE(PatternFixture, ".cnm round-trip: classic project without patterns loads with empty patterns") {
  // A project saved in classic view (no patterns used) should load with
  // viewMode=classic and empty patterns (projectInit defaults).
  // This simulates loading a pre-patterns file.
  project->viewMode = ViewMode::classic;
  // Make sure no patterns are populated (projectInit already cleared them).
  const char* path = "build/tests/patterns_classic_roundtrip.cct";
  REQUIRE(projectSave(project.get(), path) == 0);

  auto loaded = std::make_unique<Project>();
  projectInit(loaded.get());
  projectInitAY(loaded.get());
  int result = projectLoad(loaded.get(), path);
  INFO("projectFileError: ", projectFileError);
  REQUIRE(result == 0);

  CHECK(loaded->viewMode == ViewMode::classic);
  // Patterns should be cleared (projectInit defaults).
  CHECK(patternIsEmpty(loaded.get(), 0));

  std::remove(path);
}

TEST_CASE_FIXTURE(PatternFixture, ".cnm round-trip: playheadMode preserved") {
  // playheadMode should round-trip through save/load.
  project->viewMode = ViewMode::simplified;
  project->playheadMode = PlayheadMode::loopActiveBar;
  const char* path = "build/tests/patterns_playhead_roundtrip.cct";
  REQUIRE(projectSave(project.get(), path) == 0);

  auto loaded = std::make_unique<Project>();
  projectInit(loaded.get());
  projectInitAY(loaded.get());
  REQUIRE(projectLoad(loaded.get(), path) == 0);

  CHECK(loaded->viewMode == ViewMode::simplified);
  CHECK(loaded->playheadMode == PlayheadMode::loopActiveBar);

  std::remove(path);
}

TEST_CASE_FIXTURE(PatternFixture, "projectInit defaults playheadMode to followPattern") {
  // A freshly-initialized project should default to followPattern (the
  // "advance through the whole pattern" behaviour that matches classic
  // chains).
  Project fresh;
  projectInit(&fresh);
  CHECK(fresh.playheadMode == PlayheadMode::followPattern);
}

TEST_CASE_FIXTURE(PatternFixture, "viewMode wrappers dispatch correctly") {
  // Classic: viewModeHasNotes wraps chainHasNotes.
  project->viewMode = ViewMode::classic;
  project->chains[5].rows[0].phrase = 10;
  project->phrases[10].rows[0].note = 48;
  CHECK(viewModeHasNotes(project.get(), 5) == 1);
  CHECK(viewModeIsEmpty(project.get(), 5) == 0);

  // Simplified: viewModeHasNotes wraps pattern check.
  project->viewMode = ViewMode::simplified;
  project->patterns[5].length = 1;
  project->patterns[5].bars[0].phrase = 10;
  CHECK(viewModeHasNotes(project.get(), 5) == 1);
  CHECK(viewModeIsEmpty(project.get(), 5) == 0);

  // Empty pattern.
  project->patterns[6].length = 1;
  project->patterns[6].bars[0].phrase = EMPTY_VALUE_16;
  CHECK(viewModeHasNotes(project.get(), 6) == 0);
  CHECK(viewModeIsEmpty(project.get(), 6) == 1);
}

TEST_CASE_FIXTURE(PatternFixture, "playbackResolvePhraseAt: out-of-range indices return EMPTY_VALUE_16") {
  // Defensive: out-of-range song/track/chain-row should never crash.
  project->viewMode = ViewMode::classic;
  CHECK(playbackResolvePhraseAt(project.get(), -1, 0, 0) == EMPTY_VALUE_16);
  CHECK(playbackResolvePhraseAt(project.get(), PROJECT_MAX_LENGTH, 0, 0) == EMPTY_VALUE_16);
  CHECK(playbackResolvePhraseAt(project.get(), 0, -1, 0) == EMPTY_VALUE_16);
  CHECK(playbackResolvePhraseAt(project.get(), 0, PROJECT_MAX_TRACKS, 0) == EMPTY_VALUE_16);
  CHECK(playbackResolvePhraseAt(project.get(), 0, 0, -1) == EMPTY_VALUE_16);
  CHECK(playbackResolvePhraseAt(project.get(), 0, 0, 16) == EMPTY_VALUE_16);

  // Empty song cell.
  project->song[0][0] = EMPTY_VALUE_16;
  CHECK(playbackResolvePhraseAt(project.get(), 0, 0, 0) == EMPTY_VALUE_16);
}

TEST_CASE_FIXTURE(PatternFixture, "playbackResolvePhraseAt: simplified pattern bar with EMPTY phrase returns EMPTY_VALUE_16") {
  // A pattern bar that has phrase == EMPTY_VALUE_16 (no phrase assigned)
  // should resolve to EMPTY_VALUE_16, not crash.
  project->viewMode = ViewMode::simplified;
  project->song[0][0] = 5;
  project->patterns[5].length = 2;
  project->patterns[5].bars[0].phrase = 10;           // bar 0 has a phrase
  project->patterns[5].bars[1].phrase = EMPTY_VALUE_16;  // bar 1 has no phrase

  CHECK(playbackResolvePhraseAt(project.get(), 0, 0, 0) == 10);
  CHECK(playbackResolvePhraseAt(project.get(), 0, 0, 1) == EMPTY_VALUE_16);

  // Beyond pattern length returns empty.
  CHECK(playbackResolvePhraseAt(project.get(), 0, 0, 2) == EMPTY_VALUE_16);
}

TEST_CASE_FIXTURE(PatternFixture, "playbackResolveTransposeAt: out-of-range returns 0") {
  project->viewMode = ViewMode::classic;
  CHECK(playbackResolveTransposeAt(project.get(), -1, 0, 0) == 0);
  CHECK(playbackResolveTransposeAt(project.get(), 0, 0, 16) == 0);

  // Empty song cell returns 0.
  project->song[0][0] = EMPTY_VALUE_16;
  CHECK(playbackResolveTransposeAt(project.get(), 0, 0, 0) == 0);
}

TEST_CASE_FIXTURE(PatternFixture, "lookupInstrument: classic mode finds instrument in chain") {
  // Set up a chain with a phrase that has an instrument.
  project->viewMode = ViewMode::classic;
  project->song[0][0] = 5;
  project->chains[5].rows[0].phrase = 10;
  project->chains[5].rows[1].phrase = 11;
  project->phrases[10].rows[0].instrument = 7;
  project->phrases[11].rows[0].instrument = 8;

  // Look up at chain row 1, phrase row 0: no instrument on row 0 of phrase 11,
  // so it should walk back to phrase 10 row 0 and find instrument 7.
  // Wait — lookupInstrument walks the SAME phrase upward, not across phrases.
  // Let me re-check the logic: it finds the phrase at (sr, cr), then scans
  // that phrase's rows from pr upward. If none, moves to previous chain row.
  // So at (0, 1, 0): phrase = 11, scan rows 0..0 of phrase 11 → instrument 8.
  CHECK(lookupInstrument(project.get(), 0, 1, 0, 0) == 8);
  // At (0, 1, 5) with no instrument on rows 0..5 of phrase 11, walks back to
  // chain row 0 (phrase 10), scans its rows 15..0. Only row 0 has instrument 7.
  project->phrases[11].rows[0].instrument = EMPTY_VALUE_8;  // clear it
  CHECK(lookupInstrument(project.get(), 0, 1, 5, 0) == 7);
}

TEST_CASE_FIXTURE(PatternFixture, "lookupInstrument: simplified mode finds instrument in pattern bar") {
  // Set up a pattern with bars that have phrases with instruments.
  project->viewMode = ViewMode::simplified;
  project->song[0][0] = 5;
  project->patterns[5].length = 2;
  project->patterns[5].bars[0].phrase = 10;
  project->patterns[5].bars[1].phrase = 11;
  project->phrases[10].rows[0].instrument = 7;
  project->phrases[11].rows[0].instrument = 8;

  // Look up at bar 1, phrase row 0: phrase 11 row 0 has instrument 8.
  CHECK(lookupInstrument(project.get(), 0, 1, 0, 0) == 8);

  // If bar 1's phrase has no instrument, walk back to bar 0's phrase.
  project->phrases[11].rows[0].instrument = EMPTY_VALUE_8;
  CHECK(lookupInstrument(project.get(), 0, 1, 5, 0) == 7);
}

TEST_CASE_FIXTURE(PatternFixture, "lookupInstrument: empty song cell returns EMPTY_VALUE_8") {
  project->viewMode = ViewMode::classic;
  project->song[0][0] = EMPTY_VALUE_16;
  CHECK(lookupInstrument(project.get(), 0, 0, 0, 0) == EMPTY_VALUE_8);

  // Walks back to previous song row, which is also empty.
  CHECK(lookupInstrument(project.get(), 5, 0, 0, 0) == EMPTY_VALUE_8);
}

// ── Playback behaviour (sandboxed playhead modes) ────────────────────────────
// The playhead mode MUST only change behaviour in simplified view; classic
// chain advance is covered by the main playback suite and must stay
// byte-for-byte unchanged (v7 lesson).

static SoundChip* patternTestChipFactory(int chipIndex, int sampleRate, ChipSetup setup) {
  (void)chipIndex;
  return new SoundChipAY(sampleRate, setup);
}

struct PatternPlaybackFixture {
  ChipNomadState* state;

  PatternPlaybackFixture() {
    state = chipnomadCreate();
    Project* p = &state->project;
    p->tickRate = 50;
    p->chipType = ChipType::AY;
    p->chipsCount = 1;
    p->chipSetup.ay = (ChipSetupAY){ .clock = 1773400, .isYM = 0, .stereoMode = StereoModeAY::ABC, .stereoSeparation = 50, .pwmFullRange = 0 };
    p->tracksCount = projectGetTotalTracks(p);
    p->linearPitch = 0;
    calculatePitchTableAY(p);
    chipnomadInitChips(state, 44100, patternTestChipFactory);
    playbackInit(&state->playbackState, p);
  }

  ~PatternPlaybackFixture() {
    chipnomadDestroy(state);
  }

  void advanceFrames(int n) {
    for (int i = 0; i < n; i++) playbackNextFrame(state);
  }

  // A simplified project: pattern 0 with `bars` bars (phrases 0..bars-1,
  // each with one note row), assigned to song[0][0]. Notes so the bars
  // resolve non-empty.
  void setupSimplified(int bars) {
    Project* p = &state->project;
    p->viewMode = ViewMode::simplified;
    p->playheadMode = PlayheadMode::followPattern;
    patternClear(&p->patterns[0]);
    p->patterns[0].length = (uint8_t)bars;
    for (int b = 0; b < bars; b++) {
      p->patterns[0].bars[b].phrase = (uint16_t)b;
      p->patterns[0].bars[b].transpose = 0;
      p->phrases[b].rows[0].note = 48;
      p->phrases[b].rows[0].instrument = EMPTY_VALUE_8;
      p->phrases[b].rows[0].volume = EMPTY_VALUE_16;
    }
    p->song[0][0] = 0;
  }
};

TEST_CASE_FIXTURE(PatternPlaybackFixture, "simplified chain mode walks pattern bars (followPattern)") {
  setupSimplified(3);
  state->playbackState.tracks[0].loop = 1;
  // Queue pattern playback at bar 0 (like tapping a cell on the Song screen).
  playbackStartChain(&state->playbackState, 0, 0, 0, 1);
  advanceFrames(1);  // frame 1 consumes the queue

  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::chain);
  CHECK(state->playbackState.tracks[0].chainRow == 0);

  // Each bar lasts 16 rows x 6 ticks = 96 frames. Advance through bar 0.
  advanceFrames(96);
  CHECK(state->playbackState.tracks[0].chainRow == 1);
  advanceFrames(96);
  CHECK(state->playbackState.tracks[0].chainRow == 2);
  // End of pattern with followPattern: wraps to bar 0.
  advanceFrames(96);
  CHECK(state->playbackState.tracks[0].chainRow == 0);
}

TEST_CASE_FIXTURE(PatternPlaybackFixture, "simplified loopActiveBar loops the queued bar") {
  setupSimplified(3);
  state->playbackState.tracks[0].loop = 1;
  state->project.playheadMode = PlayheadMode::loopActiveBar;
  // Queue playback starting at bar 1 (the "active" bar).
  playbackStartChain(&state->playbackState, 0, 0, 1, 1);
  advanceFrames(1);

  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::chain);
  CHECK(state->playbackState.tracks[0].chainRow == 1);

  // After one bar (96 frames) the playhead must return to the queued bar 1,
  // not walk to bar 2 or wrap to bar 0.
  advanceFrames(96);
  CHECK(state->playbackState.tracks[0].chainRow == 1);
  advanceFrames(96);
  CHECK(state->playbackState.tracks[0].chainRow == 1);
}

TEST_CASE_FIXTURE(PatternPlaybackFixture, "classic chain advance is unchanged (16 rows, enteredChain at 0)") {
  // Classic mode must behave exactly as before the patterns feature: the
  // chain has 16 rows, playback walks them and loops at chainRow 0.
  Project* p = &state->project;
  p->viewMode = ViewMode::classic;
  chainClear(&p->chains[0]);
  p->chains[0].rows[0].phrase = 0;
  p->chains[0].rows[1].phrase = 1;
  p->phrases[0].rows[0].note = 48;
  p->phrases[1].rows[0].note = 50;
  p->song[0][0] = 0;

  state->playbackState.tracks[0].loop = 1;
  playbackStartChain(&state->playbackState, 0, 0, 0, 1);
  advanceFrames(1);
  CHECK(state->playbackState.tracks[0].chainRow == 0);

  advanceFrames(96);  // one phrase
  CHECK(state->playbackState.tracks[0].chainRow == 1);
  advanceFrames(96);  // second phrase -> wraps to chain row 0 (classic loop)
  CHECK(state->playbackState.tracks[0].chainRow == 0);
  advanceFrames(96);
  CHECK(state->playbackState.tracks[0].chainRow == 1);
}
