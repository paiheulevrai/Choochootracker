#include "doctest.h"

#include <export/export_midi.h>
#include <midi/smf_file.h>
#include <chipnomad_lib.h>
#include <project_utils.h>

#include <cstdio>
#include <cstdlib>

TEST_SUITE("export_midi") {

static void setNote(Project* p, int track, int songRow, int chainRow, int phraseRowIdx,
                     uint8_t note, uint8_t instrument, uint16_t volume) {
  int chainIdx = songRow; // one dedicated chain per song row is plenty for tests
  p->song[songRow][track] = chainIdx;
  p->chains[chainIdx].rows[chainRow].phrase = chainIdx; // one dedicated phrase, same index
  p->chains[chainIdx].rows[chainRow].transpose = 0;
  PhraseRow* row = &p->phrases[chainIdx].rows[phraseRowIdx];
  row->note = note;
  row->instrument = instrument;
  row->volume = volume;
}

TEST_CASE("exports a note and its note-off, quantized to the row grid") {
  Project p;
  projectInitAY(&p);
  p.tracksCount = 1;

  // C-0 is pitch table index 0 -> MIDI note 12. Full volume (7F) -> velocity 127.
  setNote(&p, 0, 0, 0, 0, 0, 0, PHRASE_VOLUME_MAX);
  p.phrases[0].rows[2].note = NOTE_OFF;

  const char* path = "test_export_midi_note.mid";
  REQUIRE(projectExportMidi(&p, path) == 0);

  SmfReadResult result;
  REQUIRE(smfReadFile(path, &result) == 0);

  REQUIRE(result.channels[0].count == 2);
  CHECK(result.channels[0].events[0].type == SmfEventType::noteOn);
  CHECK(result.channels[0].events[0].note == 12);
  CHECK(result.channels[0].events[0].velocity == 127);
  CHECK(result.channels[0].events[0].absoluteTick == 0);

  CHECK(result.channels[0].events[1].type == SmfEventType::noteOff);
  CHECK(result.channels[0].events[1].absoluteTick == 2 * 120); // 480 PPQ / 4 rows-per-beat

  smfFreeReadResult(&result);
  std::remove(path);
}

TEST_CASE("a chain transpose shifts the exported MIDI note") {
  Project p;
  projectInitAY(&p);
  p.tracksCount = 1;

  setNote(&p, 0, 0, 0, 0, 0, 0, PHRASE_VOLUME_MAX);
  p.chains[0].rows[0].transpose = 5; // +5 semitones

  const char* path = "test_export_midi_transpose.mid";
  REQUIRE(projectExportMidi(&p, path) == 0);

  SmfReadResult result;
  REQUIRE(smfReadFile(path, &result) == 0);
  REQUIRE(result.channels[0].count >= 1);
  CHECK(result.channels[0].events[0].note == 17); // 12 + 5

  smfFreeReadResult(&result);
  std::remove(path);
}

TEST_CASE("an empty song refuses to export") {
  Project p;
  projectInitAY(&p);

  CHECK(projectExportMidi(&p, "test_export_midi_empty.mid") != 0);
}

TEST_CASE("a chain ending early advances to the next song row immediately, not after a full 256-row block") {
  Project p;
  projectInitAY(&p);
  p.tracksCount = 1;

  // Song row 0's chain only fills chain row 0 (rows 1-15 stay "---"): real
  // playback ends the chain there and jumps straight to song row 1, rather
  // than padding out the remaining 15 chain rows as silence.
  setNote(&p, 0, 0, 0, 0, 0, 0, PHRASE_VOLUME_MAX);
  setNote(&p, 0, 1, 0, 0, 1, 0, PHRASE_VOLUME_MAX); // second song row, distinct note

  const char* path = "test_export_midi_early_chain_end.mid";
  REQUIRE(projectExportMidi(&p, path) == 0);

  SmfReadResult result;
  REQUIRE(smfReadFile(path, &result) == 0);

  // noteOn(12), then noteOff(12)+noteOn(13) back-to-back where the chain
  // ends and the song row advances, then a final noteOff(13) at track end.
  REQUIRE(result.channels[0].count == 4);
  CHECK(result.channels[0].events[0].type == SmfEventType::noteOn);
  CHECK(result.channels[0].events[0].note == 12); // song row 0's note
  CHECK(result.channels[0].events[0].absoluteTick == 0);
  CHECK(result.channels[0].events[2].type == SmfEventType::noteOn);
  CHECK(result.channels[0].events[2].note == 13); // song row 1's note
  // One full 16-row phrase (not 256 rows) after the first note.
  CHECK(result.channels[0].events[2].absoluteTick == 16 * 120);

  smfFreeReadResult(&result);
  std::remove(path);
}

}
