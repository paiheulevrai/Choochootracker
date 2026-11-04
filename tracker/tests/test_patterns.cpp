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

TEST_CASE_FIXTURE(PatternFixture, "convertChainsToPatterns: basic conversion preserves phrases, bakes transpose") {
  // Set up a chain at index 3 with 3 phrases (10, 11, 12) and transposes 0, 2,
  // 4. The transposed rows are baked into cloned phrases; transpose == 0
  // bars reference the original phrase directly.
  setupTestChain(project.get(), 3, 10, 3);
  project->song[0][0] = 3;  // song cell points to chain 3

  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  CHECK(project->viewMode == ViewMode::simplified);

  // Pattern 3 is free, so the pattern takes the chain's own number.
  uint16_t patternIdx = project->song[0][0];
  CHECK(patternIdx == 3);

  // The mapping should be recorded.
  CHECK(project->chainToPatternMap[3] == patternIdx);
  CHECK(project->patternToChainMap[patternIdx] == 3);

  // The pattern should have length 3. Bar 0 (transpose 0) references the
  // original phrase 10; bars 1/2 (transposes 2/4) reference clones with
  // baked notes.
  Pattern* pat = &project->patterns[patternIdx];
  CHECK(pat->length == 3);
  CHECK(pat->bars[0].phrase == 10);
  CHECK(pat->bars[0].transpose == 0);

  // Bar 1: clone of phrase 11 with notes shifted by +2.
  uint16_t baked1 = pat->bars[1].phrase;
  CHECK(baked1 != 11);
  CHECK(project->phrases[11].rows[0].note == 49);  // original untouched
  CHECK(project->phrases[baked1].rows[0].note == 51);

  // Bar 2: clone of phrase 12 with notes shifted by +4.
  uint16_t baked2 = pat->bars[2].phrase;
  CHECK(baked2 != 12);
  CHECK(baked2 != baked1);
  CHECK(project->phrases[12].rows[0].note == 50);  // original untouched
  CHECK(project->phrases[baked2].rows[0].note == 54);
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

TEST_CASE_FIXTURE(PatternFixture, "convertPatternsToChains: in-sync round trip restores the saved chain verbatim") {
  // Untouched pairs (neither side edited since the conversion) are restored
  // exactly: the chain keeps its original phrase slots and content.
  setupTestChain(project.get(), 3, 10, 3);
  project->song[0][0] = 3;
  Chain original = project->chains[3];

  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  REQUIRE(convertPatternsToChains(project.get()) == 0);
  CHECK(project->viewMode == ViewMode::classic);
  CHECK(project->song[0][0] == 3);
  // Byte-identical: slot layout AND content restored (in-sync pair skipped).
  CHECK(std::memcmp(&project->chains[3], &original, sizeof(Chain)) == 0);
}

TEST_CASE_FIXTURE(PatternFixture, "convertPatternsToChains: mapped pattern rebuilds chain from CURRENT pattern content") {
  // Live-mode-wins: after classic->simplified, editing the pattern in
  // simplified mode and converting back must carry the edits into the
  // chain (the leaving simplified mode is the source of truth).
  project->song[0][0] = 5;  // chain 5 (empty)

  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  uint16_t patternIdx = project->song[0][0];
  REQUIRE(patternIdx == 5);
  Pattern* pat = &project->patterns[patternIdx];
  REQUIRE(pat->length == 1);
  uint16_t phrase0 = pat->bars[0].phrase;
  REQUIRE(phrase0 != EMPTY_VALUE_16);

  // Edit the pattern in simplified mode.
  REQUIRE(patternAddEmptyBar(project.get(), patternIdx));
  CHECK(pat->length == 2);
  project->phrases[phrase0].rows[0].note = 48;
  project->phrases[pat->bars[1].phrase].rows[0].note = 50;

  // Convert back: the chain is rebuilt from the pattern's current bars.
  REQUIRE(convertPatternsToChains(project.get()) == 0);
  CHECK(project->viewMode == ViewMode::classic);
  CHECK(project->song[0][0] == 5);
  Chain* ch = &project->chains[5];
  CHECK_FALSE(chainIsEmpty(project.get(), 5));
  // Bar 0 wants phrase 5 (pattern number; free at conversion time).
  CHECK(ch->rows[0].phrase != EMPTY_VALUE_16);
  CHECK(project->phrases[ch->rows[0].phrase].rows[0].note == 48);
  CHECK(ch->rows[1].phrase != EMPTY_VALUE_16);
  CHECK(project->phrases[ch->rows[1].phrase].rows[0].note == 50);
  CHECK(ch->rows[2].phrase == EMPTY_VALUE_16);
  // The pattern was not modified by the conversion.
  CHECK(pat->length == 2);
  CHECK(project->phrases[phrase0].rows[0].note == 48);
}

TEST_CASE_FIXTURE(PatternFixture, "convertPatternsToChains: pattern added in simplified mode builds a fresh chain") {
  // A pattern with NO mapping (created entirely in simplified mode, placed
  // in the song) converts to a brand-new chain with per-bar phrase slots:
  // bar k wants phrase (pattern + k); free slots take the content.
  project->viewMode = ViewMode::simplified;
  project->patterns[5].length = 2;
  project->patterns[5].bars[0].phrase = 70;
  project->patterns[5].bars[1].phrase = 71;
  project->phrases[70].rows[0].note = 48;
  project->phrases[71].rows[0].note = 50;
  project->song[0][0] = 5;

  REQUIRE(convertPatternsToChains(project.get()) == 0);
  CHECK(project->viewMode == ViewMode::classic);

  uint16_t chainIdx = project->song[0][0];
  CHECK(chainIdx < PROJECT_MAX_CHAINS);
  CHECK(project->patternToChainMap[5] == chainIdx);
  Chain* ch = &project->chains[chainIdx];
  // Lowest-free-first: bars pack into slots 0 and 1.
  CHECK(ch->rows[0].phrase == 0);
  CHECK(project->phrases[0].rows[0].note == 48);
  CHECK(ch->rows[1].phrase == 1);
  CHECK(project->phrases[1].rows[0].note == 50);
  // Remaining rows empty (playback stops at the first empty row).
  CHECK(ch->rows[2].phrase == EMPTY_VALUE_16);
  CHECK(ch->rows[15].phrase == EMPTY_VALUE_16);
}

TEST_CASE_FIXTURE(PatternFixture, "convertPatternsToChains: occupied desired slot falls back to closest free") {
  // Pattern 11, 4 bars, but phrase 12 is occupied by content → the sequence
  // becomes 11, 13, 14, 15 (per-phrase closest free; gaps allowed).
  // Build the pattern directly in simplified mode.
  project->viewMode = ViewMode::simplified;
  project->patterns[11].length = 4;
  for (int b = 0; b < 4; b++) {
    project->patterns[11].bars[b].phrase = 100 + b;
    project->patterns[11].bars[b].transpose = 0;
    project->phrases[100 + b].rows[0].note = 40 + b;
  }
  // Phrase 12 is occupied (has content).
  project->phrases[12].rows[0].note = 30;
  project->song[0][0] = 11;

  REQUIRE(convertPatternsToChains(project.get()) == 0);
  Chain* ch = &project->chains[project->song[0][0]];
  // Lowest-free-first naming: slots 0..3 regardless of the pattern number
  // (phrase 12's occupancy no longer matters — naming packs from 000).
  CHECK(ch->rows[0].phrase == 0);
  CHECK(ch->rows[1].phrase == 1);
  CHECK(ch->rows[2].phrase == 2);
  CHECK(ch->rows[3].phrase == 3);
  // Contents were copied in order.
  CHECK(project->phrases[0].rows[0].note == 40);
  CHECK(project->phrases[1].rows[0].note == 41);
  CHECK(project->phrases[2].rows[0].note == 42);
  CHECK(project->phrases[3].rows[0].note == 43);
  // Original pattern phrases remain untouched.
  CHECK(project->phrases[101].rows[0].note == 41);
}

TEST_CASE_FIXTURE(PatternFixture, "convertPatternsToChains: identical bars share a phrase slot") {
  // Bars 0 and 2 reference the same phrase; bar 1 references a different
  // phrase with IDENTICAL content. All three untransposed bars share one
  // slot in the chain.
  project->viewMode = ViewMode::simplified;
  project->patterns[7].length = 3;
  project->patterns[7].bars[0].phrase = 20;
  project->patterns[7].bars[1].phrase = 21;
  project->patterns[7].bars[2].phrase = 20;
  project->phrases[20].rows[0].note = 55;
  project->phrases[21] = project->phrases[20];  // identical content
  project->song[0][0] = 7;

  REQUIRE(convertPatternsToChains(project.get()) == 0);
  Chain* ch = &project->chains[project->song[0][0]];
  CHECK(ch->rows[0].phrase == ch->rows[1].phrase);
  CHECK(ch->rows[0].phrase == ch->rows[2].phrase);
  CHECK(ch->rows[0].phrase == 0);  // lowest free slot
  CHECK(project->phrases[0].rows[0].note == 55);
}

TEST_CASE_FIXTURE(PatternFixture, "convertPatternsToChains: transposed bar bakes into fresh slot") {
  project->viewMode = ViewMode::simplified;
  project->patterns[4].length = 1;
  project->patterns[4].bars[0].phrase = 30;
  project->patterns[4].bars[0].transpose = 3;
  project->phrases[30].rows[0].note = 48;
  project->song[0][0] = 4;

  REQUIRE(convertPatternsToChains(project.get()) == 0);
  Chain* ch = &project->chains[project->song[0][0]];
  // The chain row holds a baked clone: note 48 + 3 = 51.
  uint16_t baked = ch->rows[0].phrase;
  CHECK(baked != 30);
  CHECK(ch->rows[0].transpose == 0);
  CHECK(project->phrases[baked].rows[0].note == 51);
  // Original phrase untouched (pattern keeps referencing it).
  CHECK(project->phrases[30].rows[0].note == 48);
  CHECK(project->patterns[4].bars[0].phrase == 30);
  CHECK(project->patterns[4].bars[0].transpose == 3);
}

TEST_CASE_FIXTURE(PatternFixture, "conversion round trip: edits carry both ways") {
  // Live-mode-wins semantics: whichever structure the user just edited is
  // the source of truth; conversion syncs its counterpart from it.
  setupTestChain(project.get(), 3, 10, 3);
  project->song[0][0] = 3;
  Chain original = project->chains[3];

  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  CHECK(project->song[0][0] == 3);  // pattern 3

  // Edit the PATTERN in simplified mode: change bar 1 to a different phrase.
  project->viewMode = ViewMode::simplified;
  project->patterns[3].bars[1].phrase = 44;
  project->phrases[44].rows[0].note = 60;

  // Convert back: the chain is rebuilt from the edited pattern.
  REQUIRE(convertPatternsToChains(project.get()) == 0);
  CHECK(project->viewMode == ViewMode::classic);
  CHECK(project->song[0][0] == 3);
  Chain* ch = &project->chains[3];
  CHECK(ch->rows[1].phrase != original.rows[1].phrase);
  CHECK(project->phrases[ch->rows[1].phrase].rows[0].note == 60);
  // Untouched rows still mirror the original content.
  CHECK(ch->rows[0].phrase != EMPTY_VALUE_16);
  CHECK(project->phrases[ch->rows[0].phrase].rows[0].note ==
        project->phrases[original.rows[0].phrase].rows[0].note);

  // Add a new simplified-mode pattern at song row 1: pattern 20, 2 bars.
  project->viewMode = ViewMode::simplified;
  project->patterns[20].length = 2;
  project->patterns[20].bars[0].phrase = 60;
  project->patterns[20].bars[1].phrase = 61;
  project->phrases[60].rows[0].note = 44;
  project->phrases[61].rows[0].note = 45;
  project->song[1][0] = 20;

  // Convert to classic: chain 3 rebuilt from pattern 3 (same content as
  // after the edit), new chain 20 built from pattern 20.
  REQUIRE(convertPatternsToChains(project.get()) == 0);
  CHECK(project->song[0][0] == 3);
  CHECK(project->song[1][0] == 20);
  CHECK(project->chains[20].rows[0].phrase != EMPTY_VALUE_16);
  CHECK(project->chains[20].rows[1].phrase != EMPTY_VALUE_16);
  CHECK(project->phrases[project->chains[20].rows[0].phrase].rows[0].note == 44);
  CHECK(project->phrases[project->chains[20].rows[1].phrase].rows[0].note == 45);

  // Edit the CHAIN in classic mode: swap chain 20 row 0 to a new phrase.
  project->chains[20].rows[0].phrase = 70;
  project->phrases[70].rows[0].note = 80;

  // Convert to simplified: pattern 20 is rebuilt from the EDITED chain.
  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  CHECK(project->viewMode == ViewMode::simplified);
  CHECK(project->song[1][0] == 20);
  Pattern* pat20 = &project->patterns[20];
  CHECK(pat20->bars[0].phrase != 60);  // rebuilt, not restored
  CHECK(project->phrases[pat20->bars[0].phrase].rows[0].note == 80);
  CHECK(pat20->bars[1].phrase != EMPTY_VALUE_16);
  CHECK(project->phrases[pat20->bars[1].phrase].rows[0].note == 45);
}

TEST_CASE_FIXTURE(PatternFixture, "convertChainsToPatterns: classic edits propagate into mapped pattern") {
  // The reported bug: convert, edit the CHAIN in classic, convert back —
  // the pattern must reflect the edits, not restore pre-conversion content.
  project->chains[6].rows[0].phrase = 12;
  project->chains[6].rows[1].phrase = 13;
  project->phrases[12].rows[0].note = 40;
  project->phrases[13].rows[0].note = 41;
  project->song[0][0] = 6;

  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  CHECK(project->song[0][0] == 6);
  REQUIRE(convertPatternsToChains(project.get()) == 0);  // back to classic

  // Edit the chain in classic: change row 1 to phrase 14 with new notes.
  project->chains[6].rows[1].phrase = 14;
  project->phrases[14].rows[0].note = 42;

  // Convert to simplified: the pattern is rebuilt from the EDITED chain.
  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  Pattern* pat = &project->patterns[6];
  REQUIRE(pat->length == 2);
  // Content is what matters: bar 0 keeps the original note (40), bar 1
  // carries the edited phrase's note (42). Phrase NUMBERS may shift across
  // a round trip (slots get re-allocated); content must not.
  CHECK(project->phrases[pat->bars[0].phrase].rows[0].note == 40);
  CHECK(project->phrases[pat->bars[1].phrase].rows[0].note == 42);
}

TEST_CASE_FIXTURE(PatternFixture, "conversion: unchanged pairs keep slot numbers, edited pairs rebuild") {
  // Two chains convert. Only chain 20 gets edited in classic afterwards.
  // Converting to simplified: pattern 3 keeps its exact bars (in sync),
  // pattern 20 is rebuilt from the edited chain.
  setupTestChain(project.get(), 3, 10, 3);
  project->chains[20].rows[0].phrase = 50;
  project->chains[20].rows[1].phrase = 51;
  project->phrases[50].rows[0].note = 33;
  project->phrases[51].rows[0].note = 34;
  project->song[0][0] = 3;
  project->song[1][0] = 20;

  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  CHECK(project->song[0][0] == 3);
  CHECK(project->song[1][0] == 20);
  Pattern snapshot3 = project->patterns[3];
  Pattern snapshot20 = project->patterns[20];

  // Round trip to classic: both pairs in sync → everything restored.
  REQUIRE(convertPatternsToChains(project.get()) == 0);
  CHECK(std::memcmp(&project->chains[3], &snapshot3, 0) == 0);  // smoke
  // (chains rebuilt to the same slots as before — in sync)

  // Edit ONLY chain 20 in classic: change row 1 to phrase 52.
  project->chains[20].rows[1].phrase = 52;
  project->phrases[52].rows[0].note = 35;

  // Convert to simplified: pattern 3 untouched (in sync), pattern 20 rebuilt.
  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  CHECK(std::memcmp(&project->patterns[3], &snapshot3, sizeof(Pattern)) == 0);
  CHECK(project->patterns[20].bars[1].phrase == 52);  // edit carried
  CHECK(project->patterns[20].bars[0].phrase == snapshot20.bars[0].phrase);
}

TEST_CASE_FIXTURE(PatternFixture, "conversion: repeated edit cycles leave untouched pairs byte-identical") {
  // Mirrors the app workflow: alternate conversions, editing the LIVE mode's
  // structure each cycle. Every conversion rebuilds only the edited pair;
  // all other pairs stay byte-identical throughout.
  setupTestChain(project.get(), 1, 30, 2);
  setupTestChain(project.get(), 2, 40, 2);
  project->song[0][0] = 1;
  project->song[1][0] = 2;

  // classic -> simplified
  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  Pattern snap1 = project->patterns[1];
  Pattern snap2 = project->patterns[2];
  Chain chainSnap2 = project->chains[2];

  // Cycle 1 (in simplified): edit PATTERN 1's bar 0 to a new phrase.
  project->patterns[1].bars[0].phrase = 60;
  project->phrases[60].rows[0].note = 55;

  // simplified -> classic: chain 1 rebuilt (edit carried), chain 2 restored.
  REQUIRE(convertPatternsToChains(project.get()) == 0);
  CHECK(std::memcmp(&project->chains[2], &chainSnap2, sizeof(Chain)) == 0);
  const bool cycle1Carried =
    project->phrases[project->chains[1].rows[0].phrase].rows[0].note == 55;
  CHECK(cycle1Carried);

  // Cycle 2 (in classic): edit CHAIN 2's row 1 to a new phrase (and clear
  // the row's old transpose — a fresh untransposed phrase).
  project->chains[2].rows[1].phrase = 62;
  project->chains[2].rows[1].transpose = 0;
  project->phrases[62].rows[0].note = 66;
  Pattern snap1b = project->patterns[1];
  // classic -> simplified: pattern 2 rebuilt (edit carried), pattern 1
  // untouched.
  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  CHECK(std::memcmp(&project->patterns[1], &snap1b, sizeof(Pattern)) == 0);
  const bool cycle2Carried =
    project->phrases[project->patterns[2].bars[1].phrase].rows[0].note == 66;
  CHECK(cycle2Carried);
  // Chain 2 keeps the edited phrase (conversion never touches chains).
  CHECK(project->chains[2].rows[1].phrase == 62);

  // Cycle 3 (in simplified): edit PATTERN 2 bar 0; pattern 1 must stay put.
  project->patterns[2].bars[0].phrase = 70;
  project->phrases[70].rows[0].note = 77;
  REQUIRE(convertPatternsToChains(project.get()) == 0);
  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  CHECK(project->phrases[project->patterns[2].bars[0].phrase].rows[0].note == 77);
  CHECK(project->patterns[1].bars[0].phrase == 60);  // untouched through cycles
  CHECK(project->phrases[project->chains[2].rows[0].phrase].rows[0].note == 77);
}

TEST_CASE_FIXTURE(PatternFixture, "patternEnsureBarExclusive: multiplied bars isolate on edit") {
  // User flow: chain rows 001,002,001,002 → pattern bars 1,2,1,2 (shared).
  // Editing bar 2 must clone the phrase for bar 2 only; bars 0/2 keep the
  // original, so converting back to classic leaves phrase 1/2 untouched.
  project->chains[1].rows[0].phrase = 1;
  project->chains[1].rows[1].phrase = 2;
  project->chains[1].rows[2].phrase = 1;
  project->chains[1].rows[3].phrase = 2;
  project->phrases[1].rows[0].note = 48;
  project->phrases[2].rows[0].note = 50;
  project->song[0][0] = 1;

  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  Pattern* pat = &project->patterns[1];
  REQUIRE(pat->length == 4);
  CHECK(pat->bars[0].phrase == 1);
  CHECK(pat->bars[1].phrase == 2);
  CHECK(pat->bars[2].phrase == 1);
  CHECK(pat->bars[3].phrase == 2);

  // Edit bar 1 (the second 002): clone-on-write isolates it.
  int edited = patternEnsureBarExclusive(project.get(), NULL, 1, 1);
  REQUIRE(edited != EMPTY_VALUE_16);
  CHECK(edited != 2);                       // new slot
  CHECK(pat->bars[1].phrase == edited);
  CHECK(pat->bars[3].phrase == 2);          // sibling untouched
  CHECK(project->chains[1].rows[1].phrase == 2);  // classic view untouched
  // The clone starts identical; the user's edit lands only in it.
  project->phrases[edited].rows[1].note = 60;
  CHECK(project->phrases[2].rows[1].note == EMPTY_VALUE_8);
  CHECK(project->phrases[edited].rows[1].note == 60);

  // Exclusive phrase: a second call is a no-op.
  CHECK(patternEnsureBarExclusive(project.get(), NULL, 1, 1) == edited);

  // Converting back: the chain keeps the original phrases; only the edited
  // row rebuilds from the pattern.
  REQUIRE(convertPatternsToChains(project.get()) == 0);
  Chain* ch = &project->chains[1];
  CHECK(ch->rows[0].phrase == 1);           // original preserved
  CHECK(project->phrases[1].rows[0].note == 48);
  CHECK(ch->rows[2].phrase == 1);
  CHECK(ch->rows[1].phrase != 2);           // edited row rebuilt
  CHECK(project->phrases[ch->rows[1].phrase].rows[1].note == 60);
  CHECK(project->phrases[ch->rows[1].phrase].rows[0].note == 50);
  // Bar 3 is unchanged: restored VERBATIM (reconcile keeps saved slots
  // for every row that plays the same music).
  CHECK(ch->rows[3].phrase == 2);
  CHECK(project->phrases[ch->rows[3].phrase].rows[0].note == 50);
  CHECK(ch->rows[3].transpose == 0);
}

TEST_CASE_FIXTURE(PatternFixture, "patternAppendPattern: merges and refuses overflow") {
  // Two 2-bar patterns merge into a 4-bar one; a third 2-bar one would
  // make 6 (fits), but a 14-bar + 3-bar source refuses (17 > 16).
  project->patterns[1].length = 2;
  project->patterns[1].bars[0].phrase = 10;
  project->patterns[1].bars[1].phrase = 11;
  project->patterns[2].length = 2;
  project->patterns[2].bars[0].phrase = 12;
  project->patterns[2].bars[1].phrase = 13;
  project->phrases[10].rows[0].note = 40;
  project->phrases[11].rows[0].note = 41;
  project->phrases[12].rows[0].note = 42;
  project->phrases[13].rows[0].note = 43;

  CHECK(patternAppendPattern(project.get(), 1, 2) == 4);
  CHECK(project->patterns[1].length == 4);
  CHECK(project->patterns[1].bars[2].phrase == 12);
  CHECK(project->patterns[1].bars[3].phrase == 13);
  // Source untouched by append (caller clears it).
  CHECK(project->patterns[2].bars[0].phrase == 12);

  // Overflow refusal: 14-bar target + 3-bar source > 16, no change.
  project->patterns[3].length = 14;
  project->patterns[4].length = 3;
  CHECK(patternAppendPattern(project.get(), 3, 4) == -1);
  CHECK(project->patterns[3].length == 14);

  // Empty source refuses.
  CHECK(patternAppendPattern(project.get(), 1, 5) == -1);
}

TEST_CASE_FIXTURE(PatternFixture, "convertChainsToPatterns: pattern number = chain number, occupied falls back") {
  // Chain 9 converts while pattern 9 is already occupied by an unrelated
  // pattern → the pattern takes the closest free slot instead.
  setupTestChain(project.get(), 9, 40, 2);
  project->song[0][0] = 9;
  // Occupy pattern 9 with content.
  project->patterns[9].length = 1;
  project->patterns[9].bars[0].phrase = 50;
  project->phrases[50].rows[0].note = 33;

  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  uint16_t patternIdx = project->song[0][0];
  CHECK(patternIdx != 9);
  CHECK(patternIdx != EMPTY_VALUE_16);
  CHECK(project->patterns[patternIdx].length == 2);
  CHECK(project->chainToPatternMap[9] == patternIdx);
}

TEST_CASE_FIXTURE(PatternFixture, "convertChainsToPatterns: negative transpose bakes down, positive up (user scenario)") {
  // Chain 01, phrase 001 with four C-4 (48) notes. Chain rows: 001 @ 00,
  // 001 @ FF (= -1 SIGNED), 001 @ 01. Converted bars must play C-4, B-3
  // (47), C#-4 (49) — FF is a signed transpose, not +255.
  project->chains[1].rows[0].phrase = 1;
  project->chains[1].rows[1].phrase = 1;
  project->chains[1].rows[2].phrase = 1;
  project->chains[1].rows[0].transpose = 0x00;
  project->chains[1].rows[1].transpose = 0xFF;  // -1
  project->chains[1].rows[2].transpose = 0x01;  // +1
  for (int r = 0; r < 4; r++) project->phrases[1].rows[r].note = 48;  // C-4
  project->song[0][0] = 1;

  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  Pattern* pat = &project->patterns[1];
  REQUIRE(pat->length == 3);
  // Bar 0: transpose 0 → references the original phrase directly.
  CHECK(pat->bars[0].phrase == 1);
  // Bar 1: baked clone with C-4 shifted DOWN one semitone → B-3 (47).
  uint16_t bakedDown = pat->bars[1].phrase;
  CHECK(bakedDown != 1);
  CHECK(project->phrases[bakedDown].rows[0].note == 47);
  CHECK(project->phrases[bakedDown].rows[3].note == 47);
  // Bar 2: baked clone shifted UP one semitone → C#-4 (49).
  uint16_t bakedUp = pat->bars[2].phrase;
  CHECK(bakedUp != 1);
  CHECK(bakedUp != bakedDown);
  CHECK(project->phrases[bakedUp].rows[0].note == 49);
  // Original phrase untouched.
  CHECK(project->phrases[1].rows[0].note == 48);
  // Bar transposes are zeroed (all baked into the notes).
  CHECK(pat->bars[0].transpose == 0);
  CHECK(pat->bars[1].transpose == 0);
  CHECK(pat->bars[2].transpose == 0);
}

TEST_CASE_FIXTURE(PatternFixture, "convertChainsToPatterns: note clamped at pitch table edges") {
  // Transpose is SIGNED (FF = -1). Upward clamp: transpose +100 pushes
  // notes past the table end → clamped to the ceiling. Downward clamp:
  // transpose -100 on a low note → clamped to 0.
  setupTestChain(project.get(), 2, 15, 1);
  project->chains[2].rows[0].transpose = 100;  // +100 (still positive signed)
  project->phrases[15].rows[0].note = 100;
  project->song[0][0] = 2;

  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  Pattern* pat = &project->patterns[project->song[0][0]];
  uint16_t baked = pat->bars[0].phrase;
  CHECK(baked != 15);
  uint8_t maxNote = (uint8_t)(project->pitchTable.length - 1);
  CHECK(project->phrases[baked].rows[0].note == maxNote);

  // Downward clamp: fresh project state via a second chain/phrase pair.
  project->chains[3].rows[0].phrase = 16;
  project->chains[3].rows[0].transpose = 0x9C;  // -100 signed
  project->phrases[16].rows[0].note = 30;
  project->song[1][0] = 3;

  REQUIRE(convertChainsToPatterns(project.get()) == 0);
  Pattern* pat3 = &project->patterns[3];
  REQUIRE(pat3->length == 1);
  uint16_t bakedDown = pat3->bars[0].phrase;
  CHECK(bakedDown != 16);
  CHECK(project->phrases[bakedDown].rows[0].note == 0);  // clamped at the floor

  // NOTE_OFF and empty notes are not shifted.
  project->phrases[15].rows[1].note = NOTE_OFF;
  project->phrases[15].rows[2].note = EMPTY_VALUE_8;
}

TEST_CASE_FIXTURE(PatternFixture, "convertChainsToPatterns: empty chain becomes 1-bar pattern with empty phrase (decision #8)") {
  // Empty chain at index 5 (chainClear already set all rows to EMPTY_VALUE_16).
  project->song[0][0] = 5;

  REQUIRE(convertChainsToPatterns(project.get()) == 0);

  uint16_t patternIdx = project->song[0][0];
  CHECK(patternIdx == 5);
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

TEST_CASE_FIXTURE(PatternFixture, "patternMultiplyLengthByTwo: duplicates bars, isolated phrases") {
  // 1-bar pattern with phrase 5 → ×2 → 2 bars where bar 1 gets its OWN
  // freshly-allocated phrase (a copy of phrase 5's content). Editing the
  // copy must never affect phrase 5.
  project->patterns[0].length = 1;
  project->patterns[0].bars[0].phrase = 5;
  project->patterns[0].bars[0].transpose = 3;
  project->phrases[5].rows[0].note = 48;

  REQUIRE(patternMultiplyLengthByTwo(project.get(), 0));
  CHECK(project->patterns[0].length == 2);
  CHECK(project->patterns[0].bars[0].phrase == 5);
  CHECK(project->patterns[0].bars[0].transpose == 3);
  // New bar: a DIFFERENT phrase, same content.
  CHECK(project->patterns[0].bars[1].phrase != 5);
  CHECK(project->patterns[0].bars[1].transpose == 3);
  CHECK(project->phrases[project->patterns[0].bars[1].phrase].rows[0].note == 48);
  // Isolation: editing the copy leaves the original untouched.
  project->phrases[project->patterns[0].bars[1].phrase].rows[0].note = 60;
  CHECK(project->phrases[5].rows[0].note == 48);
}

TEST_CASE_FIXTURE(PatternFixture, "patternMultiplyLengthByTwo: 2-bar A,B → 4-bar A,B,A',B' (isolated)") {
  project->patterns[0].length = 2;
  project->patterns[0].bars[0].phrase = 5;
  project->patterns[0].bars[1].phrase = 7;
  project->phrases[5].rows[0].note = 48;
  project->phrases[7].rows[0].note = 50;

  REQUIRE(patternMultiplyLengthByTwo(project.get(), 0));
  CHECK(project->patterns[0].length == 4);
  CHECK(project->patterns[0].bars[0].phrase == 5);
  CHECK(project->patterns[0].bars[1].phrase == 7);
  uint16_t p2 = project->patterns[0].bars[2].phrase;
  uint16_t p3 = project->patterns[0].bars[3].phrase;
  // Fresh, distinct phrases holding copies of the source content.
  CHECK(p2 != 5);
  CHECK(p2 != 7);
  CHECK(p3 != 5);
  CHECK(p3 != 7);
  CHECK(p2 != p3);
  CHECK(project->phrases[p2].rows[0].note == 48);
  CHECK(project->phrases[p3].rows[0].note == 50);
  // Isolation: editing a copy never touches its source.
  project->phrases[p2].rows[0].note = 60;
  CHECK(project->phrases[5].rows[0].note == 48);
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
