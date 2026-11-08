#include "doctest.h"
#include "chipnomad_lib.h"
#include "playback_internal.h"
#include "project_utils.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace {
struct SequencerFXFixture {
  std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)> state{chipnomadCreate(), chipnomadDestroy};

  SequencerFXFixture() {
    projectInitAY(&state->project);
    chipnomadInitChips(state.get(), 44100, nullptr);
    playbackInit(&state->playbackState, &state->project);
    state->playbackState.tracks[0].mode = PlaybackMode::phraseRow;
    state->playbackState.tracks[0].songRow = 0;
  }

  static PhraseRow row() {
    PhraseRow result;
    std::memset(&result, 0xff, sizeof(result));
    return result;
  }

  void read(PhraseRow value) {
    state->playbackState.tracks[0].note.noteTriggered = 0;
    readPhraseRowDirect(&state->playbackState, 0, &value, 0);
  }

  void trigger(uint8_t value) {
    auto r = row();
    r.note = 45;
    r.instrument = 0;
    r.volume = PHRASE_VOLUME_MAX;
    r.fx[0] = {fxEUC, value};
    read(r);
  }
};
}

TEST_CASE_FIXTURE(SequencerFXFixture, "EUC4F loops four hits per sixteen rows until stopped") {
  trigger(0x4f);
  auto& track = state->playbackState.tracks[0];
  std::vector<int> hits{0};
  for (int step = 1; step < 32; ++step) {
    read(row());
    if (track.note.noteTriggered) hits.push_back(step);
    CHECK(track.note.pitchBase == 45);
    CHECK(track.note.instrument == 0);
  }
  CHECK(hits == std::vector<int>{0, 4, 8, 12, 16, 20, 24, 28});
  CHECK(track.euclidean.active);
}

TEST_CASE_FIXTURE(SequencerFXFixture, "EUC evenly distributes all nibble values and clamps excess hits") {
  auto& track = state->playbackState.tracks[0];
  for (int length = 1; length <= 16; ++length) {
    for (int pulses = 0; pulses <= 15; ++pulses) {
      trigger((uint8_t)((pulses << 4) | (length - 1)));
      int hits = pulses ? 1 : 0;
      std::vector<int> gaps;
      int last = 0;
      for (int step = 1; step < length; ++step) {
        read(row());
        if (track.note.noteTriggered) {
          gaps.push_back(step - last);
          last = step;
          ++hits;
        }
      }
      if (pulses) gaps.push_back(length - last);
      CHECK(hits == std::min(pulses, length));
      if (!gaps.empty()) CHECK(*std::max_element(gaps.begin(), gaps.end()) - *std::min_element(gaps.begin(), gaps.end()) <= 1);
      read(row());
      CHECK(bool(track.euclidean.active) == (pulses != 0));
      CHECK(bool(track.note.noteTriggered) == (pulses != 0));
    }
  }
}

TEST_CASE_FIXTURE(SequencerFXFixture, "EUC stops on zero, note off and a replacement note") {
  auto& track = state->playbackState.tracks[0];
  for (int stop = 0; stop < 3; ++stop) {
    trigger(0xff);
    auto r = row();
    if (stop == 0) r.fx[0] = {fxEUC, 0};
    if (stop == 1) r.note = NOTE_OFF;
    if (stop == 2) r.note = 48;
    read(r);
    CHECK_FALSE(track.euclidean.active);
    for (int i = 0; i < 16; ++i) {
      read(row());
      CHECK_FALSE(track.note.noteTriggered);
    }
  }
}

TEST_CASE_FIXTURE(SequencerFXFixture, "EUC follows actual groove rows instead of audio ticks") {
  auto& p = state->project;
  auto r = row(); r.note = 45; r.instrument = 0; r.fx[0] = {fxEUC, 0x4f};
  p.song[0][0] = 0;
  p.chains[0].rows[0].phrase = 0;
  p.phrases[0].rows[0] = r;
  p.grooves[0].speed[0] = 6;
  p.grooves[0].speed[1] = EMPTY_VALUE_8;
  playbackStop(&state->playbackState);
  playbackStartPhrase(&state->playbackState, 0, 0, 0, 0, 0);
  std::vector<int> hits;
  for (int tick = 0; tick < 96; ++tick) {
    auto& track = state->playbackState.tracks[0];
    track.note.noteTriggered = 0;
    playbackNextFrame(state.get());
    if (track.note.noteTriggered) hits.push_back(tick);
  }

  CHECK(hits == std::vector<int>{0, 24, 48, 72});
}

TEST_CASE_FIXTURE(SequencerFXFixture, "EUC loops single-row periods and replacements restart the phase") {
  trigger(0x10);
  auto& track = state->playbackState.tracks[0];
  for (int i = 0; i < 40; ++i) {
    read(row());
    CHECK(track.note.noteTriggered);
    CHECK(track.euclidean.step == 0);
  }
  auto r = row(); r.fx[0] = {fxEUC, 0x23};
  read(r);
  CHECK(track.note.noteTriggered);
  for (int i = 1; i <= 12; ++i) {
    read(row());
    CHECK(bool(track.note.noteTriggered) == (i % 2 == 0));
  }
  r = row(); r.instrument = 0;
  read(r);
  CHECK_FALSE(track.euclidean.active);
}

TEST_CASE_FIXTURE(SequencerFXFixture, "REP columns wrap independently and survive instrument triggers until RTO") {
  auto& track = state->playbackState.tracks[0];
  auto r = row();
  r.note = 45; r.instrument = 0;
  r.fx[0] = {fxPAN, 250};
  r.fx[1] = {fxTPN, 20};
  read(r);
  r = row();
  r.fx[0] = {fxREP, 3}; r.fx[1] = {fxREP, 255};
  read(r);
  CHECK(track.note.fx[fxPAN].fxValue == 253);
  CHECK(track.note.fx[fxTPN].fxValue == 19);
  read(row());
  CHECK(track.note.fx[fxPAN].fxValue == 0);
  CHECK(track.note.fx[fxTPN].fxValue == 18);
  r = row(); r.instrument = 0; r.note = 48;
  read(r);
  CHECK(track.note.fx[fxPAN].fxValue == 3);
  CHECK(track.note.fx[fxTPN].fxValue == 17);
  r = row(); r.fx[0] = {fxRTO, 4};
  read(r);
  CHECK(track.note.fx[fxPAN].fxValue == 4);
  CHECK_FALSE(track.repeat[0].active);
  CHECK(track.note.fx[fxTPN].fxValue == 16);
  r = row(); r.fx[0] = {fxFIN, 8}; r.fx[1] = {fxREP, 0};
  read(r);
  CHECK_FALSE(track.repeat[0].active);
  CHECK_FALSE(track.repeat[1].active);
  read(row());
  CHECK(track.note.fx[fxTPN].fxValue == 16);
}

TEST_CASE_FIXTURE(SequencerFXFixture, "RTO applies the final relative FX once and then stops repeating") {
  auto r = row(); r.fx[0] = {fxFIN, 4};
  read(r);
  r.fx[0] = {fxREP, 1};
  read(r);
  r.fx[0] = {fxRTO, 6};
  read(r);
  auto& track = state->playbackState.tracks[0];
  CHECK(track.note.fx[fxFIN].acc == 15);
  CHECK_FALSE(track.repeat[0].active);
  for (int i = 0; i < 8; ++i) read(row());
  CHECK(track.note.fx[fxFIN].acc == 15);
}

TEST_CASE_FIXTURE(SequencerFXFixture, "REP signed decrement stops at RTO minimum without wrapping") {
  auto r = row();
  r.fx[0] = {fxPAN, 5};
  read(r);
  r.fx[0] = {fxREP, 0xfd};
  read(r);
  CHECK(state->playbackState.tracks[0].repeat[0].last.value == 2);
  r.fx[0] = {fxRTO, 0};
  read(r);
  for (int i = 0; i < 8; ++i) read(row());
  CHECK(state->playbackState.tracks[0].repeat[0].last.value == 0);
}

TEST_CASE_FIXTURE(SequencerFXFixture, "REP resolves before special phrase FX and delayed reads do not increment twice") {
  auto r = row();
  state->project.signedTrackSpeed = 1;
  r.fx[0] = {fxSPD, 0};
  read(r);
  r.fx[0] = {fxREP, 1};
  read(r);
  CHECK(state->playbackState.tracks[0].speedRatio == 1);
  r = row();
  r.fx[1] = {fxDEL, 2};
  read(r);
  auto& repeat = state->playbackState.tracks[0].repeat[0];
  CHECK(repeat.last.value == 2);
  readPhraseRowDirect(&state->playbackState, 0, &r, 1);
  CHECK(repeat.last.value == 2);
  CHECK(state->playbackState.tracks[0].speedRatio == 2);
}

TEST_CASE_FIXTURE(SequencerFXFixture, "REP in tables follows each lane speed and does not increment twice") {
  auto r = row(); r.note = 45; r.instrument = 0;
  read(r);
  auto& table = state->project.tables[3];
  table.rows[0].fx[0] = {fxPAN, 10};
  table.rows[0].fx[1] = {fxTPN, 100};
  table.rows[1].fx[0] = {fxREP, 1};
  table.rows[1].fx[1] = {fxREP, 255};
  table.rows[15].fx[1] = {fxTIC, 3};
  auto& track = state->playbackState.tracks[0];
  tableInit(&state->playbackState, 0, &track.note.instrumentTable, 3, 0, 2);
  playbackNextFrame(state.get());
  CHECK(track.note.fx[fxPAN].fxValue == 10);
  playbackNextFrame(state.get());
  CHECK(track.note.fx[fxPAN].fxValue == 11);
  CHECK(track.note.fx[fxTPN].fxValue == 100);
  playbackNextFrame(state.get());
  CHECK(track.note.fx[fxTPN].fxValue == 99);
  playbackNextFrame(state.get());
  CHECK(track.note.fx[fxPAN].fxValue == 12);
}

TEST_CASE_FIXTURE(SequencerFXFixture, "REP can repeat a probability command before row conditions are evaluated") {
  auto& p = state->project;
  p.song[0][0] = 0;
  p.chains[0].rows[0].phrase = 0;
  auto r = row(); r.note = 45; r.instrument = 0; r.fx[0] = {fxPRO, 0};
  p.phrases[0].rows[0] = r;
  r.note = 47; r.fx[0] = {fxREP, 100};
  p.phrases[0].rows[1] = r;
  auto& track = state->playbackState.tracks[0];
  track.mode = PlaybackMode::phrase;
  readPhraseRow(&state->playbackState, 0, 0);
  CHECK(track.note.instrument == EMPTY_VALUE_8);
  track.phraseRow = 1;
  readPhraseRow(&state->playbackState, 0, 0);
  CHECK(track.repeat[0].last.command == fxPRO);
  CHECK(track.repeat[0].last.value == 100);
  CHECK(track.note.pitchBase == 47);
}

TEST_CASE_FIXTURE(SequencerFXFixture, "Clear Track FX resets EUC and all phrase and table repeats") {
  trigger(0x4f);
  auto& track = state->playbackState.tracks[0];
  track.repeat[1].last = {fxPAN, 128};
  track.repeat[1].active = 1;
  track.note.instrumentTable.repeat[2].last = {fxFIN, 1};
  track.note.instrumentTable.repeat[2].active = 1;
  playbackClearTrackFX(&state->playbackState, 0);
  CHECK_FALSE(track.euclidean.active);
  CHECK_FALSE(track.repeat[1].active);
  CHECK(track.repeat[1].last.command == FX_ID_EMPTY);
  CHECK_FALSE(track.note.instrumentTable.repeat[2].active);
}

TEST_CASE_FIXTURE(SequencerFXFixture, "Conditional phrase HOP resolves REP on its destination row") {
  auto& p = state->project;
  p.song[0][0] = 0;
  p.chains[0].rows[0].phrase = 0;
  auto r = row(); r.fx[0] = {fxPAN, 10};
  p.phrases[0].rows[0] = r;
  r = row(); r.fx[1] = {fxHOP, 0x13};
  p.phrases[0].rows[1] = r;
  r = row(); r.fx[0] = {fxREP, 1};
  p.phrases[0].rows[3] = r;
  auto& track = state->playbackState.tracks[0];
  track.mode = PlaybackMode::phrase;
  readPhraseRow(&state->playbackState, 0, 0);
  track.phraseRow = 1;
  readPhraseRow(&state->playbackState, 0, 0);
  CHECK(track.phraseRow == 3);
  CHECK(track.repeat[0].active);
  CHECK(track.note.fx[fxPAN].fxValue == 11);
}

TEST_CASE_FIXTURE(SequencerFXFixture, "New FX identifiers round-trip in projects without renumbering old commands") {
  fillFXNames();
  auto& p = state->project;
  p.phrases[0].rows[0].fx[0] = {fxEUC, 0x4f};
  p.phrases[0].rows[0].fx[1] = {fxREP, 0xff};
  p.phrases[0].rows[0].fx[2] = {fxRTO, 0x20};
  p.tables[3].rows[2].fx[0] = {fxREP, 2};
  auto loaded = std::make_unique<Project>();
  projectInit(loaded.get());
  const char* file = "build/tests/repeat_euclidean.cct";
  REQUIRE(projectSave(&p, file) == 0);
  REQUIRE(projectLoad(loaded.get(), file) == 0);
  std::remove(file);
  CHECK(loaded->phrases[0].rows[0].fx[0].command == fxEUC);
  CHECK(loaded->phrases[0].rows[0].fx[0].value == 0x4f);
  CHECK(loaded->phrases[0].rows[0].fx[1].command == fxREP);
  CHECK(loaded->phrases[0].rows[0].fx[2].command == fxRTO);
  CHECK(loaded->tables[3].rows[2].fx[0].command == fxREP);
  CHECK(fxEUC > fxL3V);
  projectFree(loaded.get());
}
