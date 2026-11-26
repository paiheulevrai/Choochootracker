#include "doctest.h"
#include "chipnomad_lib.h"
#include "playback_internal.h"
#include "project_utils.h"
#include <chrono>
#include <cstdio>
#include <memory>

namespace {
struct BlendFixture {
  std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)> state{chipnomadCreate(), chipnomadDestroy};
  PreparedBlends prepared;
  Project& p = state->project;
  PlaybackState& playback = state->playbackState;

  BlendFixture() {
    projectInitAY(&p);
    p.song[0][0] = 0;
    p.chains[0].rows[0].phrase = 0;
    p.phrases[0].rows[0].note = 45;
    p.phrases[0].rows[0].instrument = 0;
    p.grooves[0].speed[0] = 4;
    p.grooves[0].speed[1] = EMPTY_VALUE_8;
    chipnomadInitChips(state.get(), 44100, nullptr);
  }
  void start() {
    playbackInit(&playback, &p);
    prepared.prepare(p);
    playback.blends = &prepared;
    playbackStartSong(&playback, 0, 0, 0);
    tick();
  }
  void tick(int count = 1) {
    for (int i = 0; i < count; ++i) playbackNextFrame(state.get());
  }
  int value(FXId command = fxPAN) { return playback.tracks[0].note.fx[command].fxValue; }
  void segment(int begin = 1, int end = 5, uint8_t target = 200, uint8_t curve = 128) {
    p.phrases[0].rows[0].fx[0] = {fxPAN, 0};
    p.phrases[0].rows[begin].fx[0] = {fxBEG, curve};
    p.phrases[0].rows[end].fx[0] = {fxEND, target};
  }
};
}

TEST_CASE("BEG curve is continuous bounded monotonic and exactly linear at 80") {
  for (int curve = 0; curve < 256; ++curve) {
    int previous = 0;
    for (int position = 0; position <= 65536; position += 256) {
      const int value = blendCurve(position, uint8_t(curve));
      REQUIRE(value >= previous);
      REQUIRE(value <= 65536);
      if (curve == 128) REQUIRE(value == position);
      previous = value;
    }
    CHECK(blendCurve(0, uint8_t(curve)) == 0);
    CHECK(blendCurve(65536, uint8_t(curve)) == 65536);
  }
  CHECK(blendCurve(32768, 0) == 49152);
  CHECK(blendCurve(32768, 255) == 16384);
}

TEST_CASE_FIXTURE(BlendFixture, "BEG interpolates at tick resolution and reaches END exactly through instrument triggers") {
  segment();
  p.phrases[0].rows[3].note = 48;
  p.phrases[0].rows[3].instrument = 0;
  start();
  CHECK(prepared.segments.size() == 1);
  tick(4);
  CHECK(value() == 0);
  tick(1);
  CHECK(value() > 0);
  CHECK(value() < 50);
  tick(7);
  CHECK(value() == 100);
  tick(8);
  CHECK(value() == 200);
  CHECK_FALSE(playback.tracks[0].blend[0].active);
  CHECK(playback.blendErrorCount == 0);
}

TEST_CASE_FIXTURE(BlendFixture, "BEG curves affect progression but not target and signed targets do not wrap") {
  for (uint8_t curve : {uint8_t(0), uint8_t(128), uint8_t(255)}) {
    segment(1, 5, 200, curve);
    start();
    tick(12);
    CHECK(value() == (curve == 0 ? 150 : curve == 128 ? 100 : 50));
    tick(8);
    CHECK(value() == 200);
  }
  p.phrases[0].rows[0].fx[0] = {fxSPT, uint8_t(-24)};
  p.phrases[0].rows[1].fx[0] = {fxBEG, 128};
  p.phrases[0].rows[5].fx[0] = {fxEND, 24};
  start();
  tick(12);
  CHECK(value(fxSPT) == 0);
  tick(8);
  CHECK(value(fxSPT) == 24);
}

TEST_CASE_FIXTURE(BlendFixture, "BEG and END reject missing endpoints and relative or discrete commands explicitly") {
  for (FXId command : {FXId(fxVOL), FXId(fxPIT), FXId(fxSLP), FX_ID_EMPTY}) {
    segment();
    p.phrases[0].rows[0].fx[0] = {command, 0};
    start();
    tick(4);
    CHECK(playback.blendError == BlendError::destination);
    CHECK_FALSE(playback.tracks[0].blend[0].active);
  }
  segment();
  p.phrases[0].rows[5].fx[0] = {FX_ID_EMPTY, 0};
  start();
  tick(4);
  CHECK(playback.blendError == BlendError::endpoint);
  segment();
  p.phrases[0].rows[3].fx[0] = {fxPAN, 32};
  start();
  tick(4);
  CHECK(playback.blendError == BlendError::endpoint);
  tick(8);
  CHECK(value() == 32);
  tick(8);
  CHECK(value() == 200);
}

TEST_CASE_FIXTURE(BlendFixture, "BEG lookup follows occurrences across sixteen bars in either view") {
  for (ViewMode view : {ViewMode::classic, ViewMode::simplified}) {
    p.viewMode = view;
    p.patterns[0].length = 16;
    for (int bar = 0; bar < 16; ++bar) {
      p.chains[0].rows[bar].phrase = bar;
      p.patterns[0].bars[bar].phrase = bar;
    }
    p.phrases[0].rows[0].fx[0] = {fxPAN, 0};
    p.phrases[0].rows[1].fx[0] = {fxBEG, 128};
    p.phrases[15].rows[15].fx[0] = {fxEND, 254};
    start();
    tick(4 * 128);
    CHECK(value() == 127);
    tick(4 * 127);
    CHECK(value() == 254);
    CHECK(playback.blendErrorCount == 0);
  }
}

TEST_CASE_FIXTURE(BlendFixture, "BEG delay and zero grooves do not advance a segment twice") {
  segment();
  p.phrases[0].rows[2].fx[1] = {fxDEL, 2};
  p.phrases[0].rows[2].note = 47;
  start();
  tick(12);
  CHECK(value() == 100);
  CHECK(playback.tracks[0].blend[0].elapsed == 2);
  tick(8);
  CHECK(value() == 200);
  p.phrases[0].rows[2].fx[1].command = FX_ID_EMPTY;
  p.grooves[0].speed[0] = 4;
  p.grooves[0].speed[1] = 0;
  p.grooves[0].speed[2] = 4;
  p.grooves[0].speed[3] = EMPTY_VALUE_8;
  start();
  tick(16);
  CHECK(value() == 200);
}

TEST_CASE_FIXTURE(BlendFixture, "BEG crosses chain or pattern and song row boundaries in both views") {
  for (ViewMode view : {ViewMode::classic, ViewMode::simplified}) {
    p.viewMode = view;
    p.song[1][0] = 1;
    p.chains[1].rows[0].phrase = 1;
    p.patterns[0].length = p.patterns[1].length = 1;
    p.patterns[0].bars[0].phrase = 0;
    p.patterns[1].bars[0].phrase = 1;
    p.phrases[0].rows[0].fx[0] = {fxPAN, 0};
    p.phrases[0].rows[15].fx[0] = {fxBEG, 128};
    p.phrases[1].rows[1].fx[0] = {fxEND, 200};
    start();
    tick(64);
    CHECK(value() == 100);
    tick(4);
    CHECK(value() == 200);
    CHECK(playback.blendErrorCount == 0);
  }
}

TEST_CASE_FIXTURE(BlendFixture, "BEG does not pair across an entirely empty chain") {
  p.phrases[0].rows[0].fx[0] = {fxPAN, 0};
  p.phrases[0].rows[1].fx[0] = {fxBEG, 128};
  p.song[1][0] = 1;
  p.song[2][0] = 2;
  p.chains[2].rows[0].phrase = 1;
  p.phrases[1].rows[0].fx[0] = {fxEND, 200};
  start();
  tick(4);
  CHECK(playback.blendError == BlendError::endpoint);
  CHECK_FALSE(playback.tracks[0].blend[0].active);
}
TEST_CASE_FIXTURE(BlendFixture, "BEG target edits rebase continuously and removed endpoints cancel") {
  segment();
  start();
  tick(12);
  REQUIRE(value() == 100);
  p.phrases[0].rows[5].fx[0].value = 240;
  prepared.prepare(p);
  tick();
  CHECK(value() == 100);
  tick(7);
  CHECK(value() == 240);
  start();
  tick(8);
  p.phrases[0].rows[5].fx[0].command = FX_ID_EMPTY;
  prepared.prepare(p);
  tick();
  CHECK_FALSE(playback.tracks[0].blend[0].active);
  CHECK(playback.blendError == BlendError::endpoint);
}

TEST_CASE_FIXTURE(BlendFixture, "BEG table columns use their own clocks and never pair across a wrap") {
  auto& table = p.tables[0];
  for (int lane = 0; lane < 2; ++lane) {
    table.rows[0].fx[lane] = {lane ? fxTPN : fxPAN, 0};
    table.rows[1].fx[lane] = {fxBEG, 128};
    table.rows[5].fx[lane] = {fxEND, 200};
  }
  start();
  auto& runtime = playback.tracks[0].note.instrumentTable;
  runtime.speed[0] = 2;
  runtime.speed[1] = 4;
  tick(6);
  CHECK(value() == 100);
  CHECK(value(fxTPN) == 25);
  tick(14);
  CHECK(value(fxTPN) == 200);
  CHECK_FALSE(runtime.blend[1].active);
  table.rows[5].fx[0] = {FX_ID_EMPTY, 0};
  table.rows[14].fx[0] = {fxBEG, 128};
  table.rows[0].fx[0] = {fxEND, 99};
  prepared.prepare(p);
  CHECK(prepared.find(blendTableKey(0, 0, 14)) == nullptr);
}

TEST_CASE_FIXTURE(BlendFixture, "BEG tables do not overwrite a phrase END on its own row") {
  segment();
  p.instruments[0].tableSpeed = 2;
  p.tables[0].rows[0].fx[0] = {fxPAN, 0};
  p.tables[0].rows[1].fx[0] = {fxBEG, 128};
  p.tables[0].rows[15].fx[0] = {fxEND, 100};
  start();
  tick(20);
  CHECK(value() == 200);
  tick();
  CHECK(value() < 100);
}
TEST_CASE_FIXTURE(BlendFixture, "BEG index keeps distinct endpoints for reused phrases and reports its bounded footprint") {
  p.chains[0].rows[0].phrase = 0;
  p.chains[0].rows[1].phrase = 1;
  p.chains[0].rows[2].phrase = 0;
  p.chains[0].rows[3].phrase = 2;
  p.phrases[0].rows[0].fx[0] = {fxPAN, 0};
  p.phrases[0].rows[1].fx[0] = {fxBEG, 128};
  p.phrases[1].rows[0].fx[0] = {fxEND, 64};
  p.phrases[2].rows[0].fx[0] = {fxEND, 192};
  prepared.prepare(p);
  const auto* first = prepared.find(blendPhraseKey(0, 0, 0, 0, 1));
  const auto* second = prepared.find(blendPhraseKey(0, 0, 0, 2, 1));
  REQUIRE(first);
  REQUIRE(second);
  CHECK(first->target == 64);
  CHECK(second->target == 192);
  CHECK(prepared.segments.size() == 2);
}

TEST_CASE_FIXTURE(BlendFixture, "BEG decreases without wrap and follows unequal groove and SPD clocks") {
  segment(1, 5, 0);
  p.phrases[0].rows[0].fx[0].value = 200;
  p.grooves[0].speed[0] = 2;
  p.grooves[0].speed[1] = 6;
  p.grooves[0].speed[2] = EMPTY_VALUE_8;
  p.signedTrackSpeed = 1;
  p.phrases[0].rows[0].fx[1] = {fxSPD, uint8_t(-1)};
  start();
  tick(4);
  CHECK(value() == 200);
  tick(16);
  CHECK(value() == 100);
  tick(16);
  CHECK(value() == 0);
  CHECK(playback.blendErrorCount == 0);
}

TEST_CASE_FIXTURE(BlendFixture, "BEG is cancelled by jumps and Clear Track FX but END still uses column context") {
  segment();
  p.phrases[0].rows[3].fx[1] = {fxHOP, 0x04};
  start();
  tick(12);
  CHECK_FALSE(playback.tracks[0].blend[0].active);
  p.phrases[0].rows[3].fx[1].command = FX_ID_EMPTY;
  start();
  tick(8);
  REQUIRE(playback.tracks[0].blend[0].active);
  playbackClearTrackFX(&playback, 0);
  CHECK_FALSE(playback.tracks[0].blend[0].active);
}

TEST_CASE_FIXTURE(BlendFixture, "BEG prepared snapshots survive unpublished replacement and discard") {
  segment(1, 15, 200);
  REQUIRE(chipnomadQueueProjectRefresh(state.get()));
  REQUIRE(chipnomadQueuePlaybackStartSong(state.get(), 0, 0, 0));
  float output[1764]{};
  REQUIRE(chipnomadReserveRenderBuffers(state.get(), 882) == 0);
  for (int tick = 0; tick < 10; ++tick) REQUIRE(chipnomadRender(state.get(), output, 882) == 882);
  REQUIRE(playback.tracks[0].blend[0].active);
  for (int edit = 0; edit < 12; ++edit) {
    p.phrases[0].rows[15].fx[0].value = 220 + edit;
    REQUIRE(chipnomadQueueProjectRefresh(state.get()));
    REQUIRE(chipnomadQueueProjectRefresh(state.get()));
    chipnomadDiscardQueuedProject(state.get());
    REQUIRE(chipnomadRender(state.get(), output, 882) == 882);
    CHECK(playback.blendErrorCount == 0);
    REQUIRE(chipnomadQueueProjectRefresh(state.get()));
    REQUIRE(chipnomadRender(state.get(), output, 882) == 882);
  }
  CHECK(playback.tracks[0].blend[0].target == 231);
}

TEST_CASE_FIXTURE(BlendFixture, "MRT and MKL act once on selected slots without retriggering or killing the note") {
  for (auto& mod : p.instruments[0].modulation)
    mod = {ModulationType::LFO, 2, 127, 0, uint8_t(LFOTrigger::free), 32, 0};
  start();
  tick(3);
  const uint32_t untouched = playback.tracks[0].note.modulation[1].counter;
  auto& track = playback.tracks[0];
  const FXCell kill{fxMKL, 5};
  initFX(&playback, 0, &kill, nullptr, -1);
  CHECK(track.note.modulation[0].modulation == nullptr);
  CHECK(track.note.modulation[2].modulation == nullptr);
  CHECK(track.note.modulation[1].modulation != nullptr);
  CHECK(track.note.modulation[3].modulation != nullptr);
  CHECK_FALSE(track.note.fx[fxMKL].isOn);
  tick();
  CHECK(track.note.modulation[0].outValue == 0);
  CHECK(track.note.modulation[1].counter > untouched);
  CHECK(track.note.pitchBase == 45);
  const FXCell retrigger{fxMRT, 1};
  initFX(&playback, 0, &retrigger, nullptr, -1);
  CHECK(track.note.modulation[0].modulation != nullptr);
  CHECK(track.note.modulation[0].counter == 0);
  CHECK(track.note.modulation[2].modulation == nullptr);
  CHECK_FALSE(track.note.fx[fxMRT].isOn);
  tick(2);
  CHECK(track.note.modulation[0].counter == 2);
  CHECK(track.note.modulation[1].counter > untouched + 1);
  FXCell ignored{fxMKL, 0xf0};
  initFX(&playback, 0, &ignored, nullptr, -1);
  ignored = {fxMRT, 0};
  initFX(&playback, 0, &ignored, nullptr, -1);
  CHECK(track.note.modulation[0].modulation != nullptr);
  CHECK(track.note.modulation[0].counter == 2);
  FXCell all{fxMKL, 15};
  initFX(&playback, 0, &all, nullptr, -1);
  for (auto& mod : track.note.modulation) CHECK(mod.modulation == nullptr);
  all.command = fxMRT;
  initFX(&playback, 0, &all, nullptr, -1);
  for (auto& mod : track.note.modulation) CHECK(mod.modulation != nullptr);
}

TEST_CASE_FIXTURE(BlendFixture, "MKL on structural LFOs ends at their own phrase trigger but not for free LFOs") {
  p.chains[0].rows[1].phrase = 1;
  p.instruments[0].modulation[0] =
    {ModulationType::LFO, 2, 127, 0, uint8_t(LFOTrigger::phrase), 32, 0};
  p.instruments[0].modulation[1] =
    {ModulationType::LFO, 2, 127, 0, uint8_t(LFOTrigger::free), 32, 0};
  p.phrases[0].rows[1].fx[0] = {fxMKL, 3};
  start();
  tick(4);
  CHECK(playback.tracks[0].note.modulation[0].modulation == nullptr);
  tick(60);
  CHECK(playback.tracks[0].note.modulation[0].modulation != nullptr);
  CHECK(playback.tracks[0].note.modulation[0].counter == 1);
  CHECK(playback.tracks[0].note.modulation[1].modulation == nullptr);
}
TEST_CASE_FIXTURE(BlendFixture, "MKL lasts until the selected modulator normal trigger and also works from tables") {
  p.instruments[0].modulation[0] =
    {ModulationType::LFO, 2, 127, 0, uint8_t(LFOTrigger::retrig), 32, 0};
  p.instruments[0].modulation[1] =
    {ModulationType::LFO, 2, 127, 0, uint8_t(LFOTrigger::free), 32, 0};
  p.tables[0].rows[1].fx[0] = {fxMKL, 3};
  p.tables[0].rows[3].fx[0] = {fxMRT, 2};
  p.phrases[0].rows[1].note = 46;
  p.phrases[0].rows[1].instrument = 0;
  start();
  tick();
  CHECK(playback.tracks[0].note.modulation[0].modulation == nullptr);
  CHECK(playback.tracks[0].note.modulation[1].modulation == nullptr);
  tick(2);
  CHECK(playback.tracks[0].note.modulation[1].modulation != nullptr);
  tick();
  CHECK(playback.tracks[0].note.modulation[0].modulation != nullptr);
}

TEST_CASE_FIXTURE(BlendFixture, "BEG END MRT and MKL round trip in phrase and table data") {
  fillFXNames();
  const FXId commands[] = {fxBEG, fxEND, fxMRT, fxMKL};
  for (int i = 0; i < 4; ++i) {
    p.phrases[0].rows[i].fx[0] = {commands[i], uint8_t(128 + i)};
    p.tables[0].rows[i].fx[0] = {commands[i], uint8_t(i)};
  }
  auto loaded = std::make_unique<Project>();
  projectInit(loaded.get());
  const char* file = "build/tests/blend.cct";
  REQUIRE(projectSave(&p, file) == 0);
  REQUIRE(projectLoad(loaded.get(), file) == 0);
  std::remove(file);
  for (int i = 0; i < 4; ++i) {
    CHECK(loaded->phrases[0].rows[i].fx[0].command == commands[i]);
    CHECK(loaded->phrases[0].rows[i].fx[0].value == 128 + i);
    CHECK(loaded->tables[0].rows[i].fx[0].command == commands[i]);
  }
  projectFree(loaded.get());
}

TEST_CASE_FIXTURE(BlendFixture, "BEG index footprint and preparation cost at maximum arrangement density") {
  p.tracksCount = PROJECT_MAX_TRACKS;
  for (auto& row : p.song) for (auto& cell : row) cell = 0;
  for (auto& bar : p.chains[0].rows) bar.phrase = 0;
  for (int row = 0; row < 16; ++row)
    for (auto& fx : p.phrases[0].rows[row].fx)
      fx = {row % 2 ? fxEND : fxBEG, 128};
  const auto begin = std::chrono::steady_clock::now();
  prepared.prepare(p);
  const double milliseconds = std::chrono::duration<double, std::milli>(
    std::chrono::steady_clock::now() - begin).count();
  CHECK(prepared.segments.size() == size_t(PROJECT_MAX_TRACKS) * PROJECT_MAX_LENGTH * 16 * 8 * 3);
  std::printf("BEG worst-density index: %zu records, %zu bytes capacity, %.2f ms producer preparation\n",
    prepared.segments.size(), prepared.segments.capacity() * sizeof(BlendSegment), milliseconds);
}
