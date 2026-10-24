#include "doctest.h"

#include <import/import_midi.h>
#include <midi/smf_file.h>
#include <chipnomad_lib.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

TEST_SUITE("import_midi") {

TEST_CASE("imports notes on a single channel onto one track, quantized to the row grid") {
  SmfTrackWriter track;
  smfTrackInit(&track);
  smfTrackTempo(&track, 0, 500000); // 120 BPM
  smfTrackNoteOn(&track, 0, 0, 60, 100); // C4, channel 0
  smfTrackNoteOff(&track, 240, 0, 60, 0); // 480 PPQ / 4 rows-per-beat = 120 ticks/row -> 2 rows
  smfTrackEnd(&track, 0);

  const char* path = "test_import_midi_note.mid";
  REQUIRE(smfWriteFile(path, 480, &track, 1) == 0);
  smfTrackFree(&track);

  Project p;
  projectInit(&p);
  REQUIRE(projectLoadMidi(&p, path) == 0);

  CHECK(p.song[0][0] != EMPTY_VALUE_16);
  int chainIdx = p.song[0][0];
  int phraseIdx = p.chains[chainIdx].rows[0].phrase;
  REQUIRE(phraseIdx != EMPTY_VALUE_16);

  // MIDI note 60 -> pitchTable index 48 (60 - 12), matching the 12TET layout
  // export_midi.cpp relies on (index N -> MIDI note 12+N).
  CHECK(p.phrases[phraseIdx].rows[0].note == 48);
  CHECK(p.phrases[phraseIdx].rows[0].instrument == 0);
  CHECK(p.phrases[phraseIdx].rows[1].note == EMPTY_VALUE_8); // held, no retrigger
  CHECK(p.phrases[phraseIdx].rows[2].note == NOTE_OFF);

  projectFree(&p);
  std::remove(path);
}

TEST_CASE("a retrigger on the same channel cuts the previous note without an explicit off") {
  SmfTrackWriter track;
  smfTrackInit(&track);
  smfTrackNoteOn(&track, 0, 0, 60, 100);
  smfTrackNoteOn(&track, 120, 0, 62, 100); // retrigger one row later, no note-off in between
  smfTrackNoteOff(&track, 120, 0, 62, 0);
  smfTrackEnd(&track, 0);

  const char* path = "test_import_midi_retrigger.mid";
  REQUIRE(smfWriteFile(path, 480, &track, 1) == 0);
  smfTrackFree(&track);

  Project p;
  projectInit(&p);
  REQUIRE(projectLoadMidi(&p, path) == 0);

  int chainIdx = p.song[0][0];
  int phraseIdx = p.chains[chainIdx].rows[0].phrase;
  CHECK(p.phrases[phraseIdx].rows[0].note == 48); // MIDI 60
  CHECK(p.phrases[phraseIdx].rows[1].note == 50); // MIDI 62, cuts row 0's note directly
  CHECK(p.phrases[phraseIdx].rows[2].note == NOTE_OFF);

  projectFree(&p);
  std::remove(path);
}

TEST_CASE("a MIDI file with no notes is rejected") {
  SmfTrackWriter track;
  smfTrackInit(&track);
  smfTrackEnd(&track, 0);

  const char* path = "test_import_midi_empty.mid";
  REQUIRE(smfWriteFile(path, 480, &track, 1) == 0);
  smfTrackFree(&track);

  Project p;
  projectInit(&p);
  CHECK(projectLoadMidi(&p, path) != 0);

  projectFree(&p);
  std::remove(path);
}

TEST_CASE("MIDI import replaces an existing sample project and can be repeated") {
  SmfTrackWriter track;
  smfTrackInit(&track);
  smfTrackNoteOn(&track, 0, 0, 60, 100);
  smfTrackNoteOff(&track, 120, 0, 60, 0);
  smfTrackEnd(&track, 0);
  const char* path = "test_import_midi_replace.mid";
  REQUIRE(smfWriteFile(path, 480, &track, 1) == 0);
  smfTrackFree(&track);

  auto p = std::make_unique<Project>();
  projectInit(p.get());
  auto& instrument = p->instruments[PROJECT_MAX_INSTRUMENTS - 1];
  getInstrumentFunctions(InstrumentType::Sample).init(&instrument);
  instrument.chip.sample.data = (int16_t*)calloc(16, sizeof(int16_t));
  REQUIRE(instrument.chip.sample.data != nullptr);
  instrument.chip.sample.frameCount = 16;
  instrument.chip.sample.channels = 1;
  instrument.chip.sample.sampleRate = 44100;
  std::strcpy(p->title, "Previous project");

  for (int attempt = 0; attempt < 2; ++attempt) {
    CAPTURE(attempt);
    REQUIRE(projectLoadMidi(p.get(), path) == 0);
    CHECK(p->instruments[0].type == InstrumentType::AY1);
    CHECK(instrument.type == InstrumentType::none);
    CHECK(p->title[0] == '\0');
    int chain = p->song[0][0];
    REQUIRE(chain >= 0);
    REQUIRE(chain < PROJECT_MAX_CHAINS);
    int phrase = p->chains[chain].rows[0].phrase;
    REQUIRE(phrase >= 0);
    REQUIRE(phrase < PROJECT_MAX_PHRASES);
    CHECK(p->phrases[phrase].rows[0].note == 48);
  }
  projectFree(p.get());
  std::remove(path);
}

TEST_CASE("Failed MIDI imports preserve existing project and sample ownership") {
  const char* path = "test_import_midi_preserve.mid";
  auto p = std::make_unique<Project>();
  projectInit(p.get());
  auto& instrument = p->instruments[0];
  getInstrumentFunctions(InstrumentType::Sample).init(&instrument);
  instrument.chip.sample.data = (int16_t*)calloc(16, sizeof(int16_t));
  REQUIRE(instrument.chip.sample.data != nullptr);
  instrument.chip.sample.frameCount = 16;
  instrument.chip.sample.channels = 1;
  instrument.chip.sample.sampleRate = 44100;
  instrument.chip.sample.data[0] = 1234;
  std::strcpy(p->title, "Keep this project");
  auto before = std::make_unique<Project>(*p);

  SUBCASE("unreadable file") {
    std::remove(path);
  }
  SUBCASE("invalid MIDI data") {
    FILE* file = std::fopen(path, "wb");
    REQUIRE(file != nullptr);
    std::fputs("invalid MIDI", file);
    std::fclose(file);
  }
  SUBCASE("valid MIDI without notes") {
    SmfTrackWriter track;
    smfTrackInit(&track);
    smfTrackEnd(&track, 0);
    REQUIRE(smfWriteFile(path, 480, &track, 1) == 0);
    smfTrackFree(&track);
  }

  CHECK(projectLoadMidi(p.get(), path) != 0);
  CHECK(std::memcmp(p.get(), before.get(), sizeof(Project)) == 0);
  CHECK(instrument.chip.sample.data[0] == 1234);
  projectFree(p.get());
  std::remove(path);
}

}
