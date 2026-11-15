#include "doctest.h"
#include "edit_history.h"
#include "app.h"
#include "project_utils.h"
#include "screens.h"
#include "screen_instrument.h"
#include "chipnomad_lib_live_stick.h"
#include "pattern_utils.h"
#include "copy_paste.h"
#include "soundbank_runtime.h"
#include <cstdlib>
#include <cstring>
#include <array>

namespace {
struct HistoryFixture {
  ChipNomadState* previous = chipnomadState;
  HistoryFixture() {
    chipnomadState = chipnomadCreate();
    REQUIRE(chipnomadState);
    projectFree(&chipnomadState->project);
    projectInitAY(&chipnomadState->project);
    initDefaultAppSettings();
    screensInitAll();
    currentScreen = &screenSong;
    inputRawCallback = nullptr;
    chipnomadSetMotionRecordMode(0, 0);
    editHistoryClear();
  }
  ~HistoryFixture() {
    editHistoryClear();
    chipnomadDestroy(chipnomadState);
    chipnomadState = previous;
  }
  Project& project() { return chipnomadState->project; }
  void engine(int index, InstrumentType type) {
    auto& i = project().instruments[index];
    getInstrumentFunctions(i.type).free(&i);
    i = {};
    i.type = type;
    getInstrumentFunctions(type).init(&i);
  }
  void note(uint8_t value) {
    EditHistoryScope edit("Note");
    REQUIRE(editHistoryCapture(EditObject::phrase, 0));
    project().phrases[0].rows[0].note = value;
  }
};
}

TEST_CASE_FIXTURE(HistoryFixture, "Undo combines nested editor changes and redoes exact bytes") {
  auto& p = project();
  std::array<uint8_t, sizeof(p.song)> oldSong;
  std::memcpy(oldSong.data(), p.song, sizeof(p.song));
  const Phrase oldPhrase = p.phrases[0];
  const Chain oldChain = p.chains[0];
  const Table oldTable = p.tables[7];
  {
    EditHistoryScope edit("Compound");
    REQUIRE(editHistoryCapture(EditObject::song));
    p.song[2][1] = 0;
    p.songHighlight[2][1] = 1;
    {
      EditHistoryScope nested("Nested");
      REQUIRE(editHistoryCapture(EditObject::phrase, 0));
      REQUIRE(editHistoryCapture(EditObject::chain, 0));
      REQUIRE(editHistoryCapture(EditObject::table, 7));
      p.phrases[0].rows[3].note = 67;
      p.chains[0].rows[2].phrase = 0;
      p.tables[7].rows[4].volume = 93;
    }
  }
  const Phrase newPhrase = p.phrases[0];
  REQUIRE(editHistoryUndo());
  CHECK(std::memcmp(p.song, oldSong.data(), sizeof(p.song)) == 0);
  CHECK(p.songHighlight[2][1] == 0);
  CHECK(std::memcmp(&p.phrases[0], &oldPhrase, sizeof(Phrase)) == 0);
  CHECK(std::memcmp(&p.chains[0], &oldChain, sizeof(Chain)) == 0);
  CHECK(std::memcmp(&p.tables[7], &oldTable, sizeof(Table)) == 0);
  CHECK_FALSE(editHistoryCanUndo());
  REQUIRE(editHistoryRedo());
  CHECK(std::memcmp(&p.phrases[0], &newPhrase, sizeof(Phrase)) == 0);
  CHECK(p.song[2][1] == 0);
  CHECK(p.songHighlight[2][1] == 1);
  CHECK(p.tables[7].rows[4].volume == 93);
}

TEST_CASE_FIXTURE(HistoryFixture, "Undo preserves redo for no-ops and branches on a real edit") {
  note(60);
  note(61);
  REQUIRE(editHistoryUndo());
  note(60);
  CHECK(editHistoryCanRedo());
  REQUIRE(editHistoryRedo());
  CHECK(project().phrases[0].rows[0].note == 61);
  REQUIRE(editHistoryUndo());
  note(72);
  CHECK_FALSE(editHistoryCanRedo());
  REQUIRE(editHistoryUndo());
  CHECK(project().phrases[0].rows[0].note == 60);
}

TEST_CASE_FIXTURE(HistoryFixture, "Undo groups held repeats and release reconciliation") {
  const uint8_t before = project().phrases[0].rows[0].note;
  editHistoryGesture(true); note(60);
  editHistoryGesture(true); note(61);
  editHistoryGesture(false);
  {
    EditHistoryScope release("Release");
    REQUIRE(editHistoryCapture(EditObject::chain, 0));
    project().chains[0].rows[0].phrase = 0;
  }

  REQUIRE(editHistoryUndo());
  CHECK(project().phrases[0].rows[0].note == before);
  CHECK(project().chains[0].rows[0].phrase == EMPTY_VALUE_16);
  CHECK_FALSE(editHistoryCanUndo());
  REQUIRE(editHistoryRedo());
  CHECK(project().phrases[0].rows[0].note == 61);
  CHECK(project().chains[0].rows[0].phrase == 0);
}

TEST_CASE_FIXTURE(HistoryFixture, "Undo does not merge an earlier action after a no-op press") {
  note(60);
  editHistoryGesture(true);
  { EditHistoryScope press("Press only"); }
  editHistoryGesture(true); note(61);
  editHistoryGesture(false);
  { EditHistoryScope release("Release"); }
  REQUIRE(editHistoryUndo());
  CHECK(project().phrases[0].rows[0].note == 60);
  CHECK(editHistoryCanUndo());
}

TEST_CASE_FIXTURE(HistoryFixture, "Undo restores clone-on-write phrase references atomically") {
  auto& p = project();
  p.chains[0].rows[0].phrase = 0;
  p.chains[0].rows[1].phrase = 0;
  p.phrases[0].rows[0].note = 60;
  int fresh;
  {
    EditHistoryScope edit("Shared phrase");
    fresh = chainEnsureBarExclusive(&p, 0, 0);
    REQUIRE(fresh != EMPTY_VALUE_16);
    REQUIRE(fresh != 0);
    REQUIRE(editHistoryCapture(EditObject::phrase, fresh));
    p.phrases[fresh].rows[0].note = 72;
  }
  REQUIRE(editHistoryUndo());
  CHECK(p.chains[0].rows[0].phrase == 0);
  CHECK(p.phrases[fresh].rows[0].note == EMPTY_VALUE_8);
  CHECK(p.phrases[0].rows[0].note == 60);
  REQUIRE(editHistoryRedo());
  CHECK(p.chains[0].rows[0].phrase == fresh);
  CHECK(p.chains[0].rows[1].phrase == 0);
  CHECK(p.phrases[fresh].rows[0].note == 72);
}

TEST_CASE_FIXTURE(HistoryFixture, "Undo bounds actions and rolls back an oversized transaction") {
  for (int i = 0; i < 80; ++i) note(uint8_t(i));
  int undone = 0;
  while (editHistoryCanUndo()) { REQUIRE(editHistoryUndo()); ++undone; }
  CHECK(undone == 64);
  CHECK(editHistoryBytes() <= 256 * 1024);
  editHistoryClear();
  const uint8_t before = project().phrases[0].rows[0].note;
  bool rejected = false;
  {
    EditHistoryScope edit("Too large");
    for (int i = 0; i < PROJECT_MAX_PHRASES; ++i) {
      if (!editHistoryCapture(EditObject::phrase, i)) { rejected = true; break; }
      project().phrases[i].rows[0].note = 100;
    }
  }
  CHECK(rejected);
  CHECK(project().phrases[0].rows[0].note == before);
  for (int i = 1; i < PROJECT_MAX_PHRASES; ++i)
    CHECK(project().phrases[i].rows[0].note == EMPTY_VALUE_8);
  CHECK_FALSE(editHistoryCanUndo());
  CHECK(editHistoryBytes() <= 256 * 1024);
}

TEST_CASE_FIXTURE(HistoryFixture, "Undo owns sampler PCM and refuses an over-budget engine change") {
  engine(10, InstrumentType::Sample);
  auto& sample = project().instruments[10].chip.sample;
  sample.frameCount = 4; sample.channels = 2;
  sample.data = static_cast<int16_t*>(malloc(8 * sizeof(int16_t)));
  REQUIRE(sample.data);
  for (int i = 0; i < 8; ++i) sample.data[i] = int16_t(i * 213 - 700);
  {
    EditHistoryScope edit("Engine");
    REQUIRE(editHistoryCaptureInstrument(10));
    engine(10, InstrumentType::Braids);
  }
  CHECK(editHistoryResourceBytes() == 16);
  REQUIRE(editHistoryUndo());
  CHECK(project().instruments[10].type == InstrumentType::Sample);
  for (int i = 0; i < 8; ++i) CHECK(project().instruments[10].chip.sample.data[i] == i * 213 - 700);
  REQUIRE(editHistoryRedo());
  CHECK(project().instruments[10].type == InstrumentType::Braids);
  editHistoryClear();
  engine(10, InstrumentType::Sample);
  auto& large = project().instruments[10].chip.sample;
  large.frameCount = 4 * 1024 * 1024 + 1; large.channels = 1;
  large.data = static_cast<int16_t*>(malloc(size_t(large.frameCount) * sizeof(int16_t)));
  REQUIRE(large.data);
  auto* original = large.data;
  projectModified = 0;
  {
    EditHistoryScope edit("Too much PCM");
    projectModified = 1;
    CHECK_FALSE(editHistoryCaptureInstrument(10));
  }
  CHECK(projectModified == 0);
  CHECK(project().instruments[10].type == InstrumentType::Sample);
  CHECK(project().instruments[10].chip.sample.data == original);
  CHECK_FALSE(editHistoryCanUndo());
  --large.frameCount;
  large.data[0] = 99;
  large.data[large.frameCount - 1] = 44;
  {
    EditHistoryScope edit("Exact resource limit");
    REQUIRE(editHistoryCaptureInstrument(10));
    engine(10, InstrumentType::Braids);
  }
  CHECK(editHistoryResourceBytes() == 8 * 1024 * 1024);
  REQUIRE(editHistoryUndo());
  CHECK(project().instruments[10].chip.sample.data[0] == 99);
  CHECK(project().instruments[10].chip.sample.data[4 * 1024 * 1024 - 1] == 44);
}

TEST_CASE_FIXTURE(HistoryFixture, "Undo clones wavetable resources without aliasing the source") {
  engine(10, InstrumentType::BYOWTBL);
  for (int n = 0; n < 2; ++n) {
    auto& s = project().instruments[10].chip.scwf.oscillator[n];
    s.frameCount = 2; s.channels = 1;
    s.data = static_cast<int16_t*>(malloc(4));
    REQUIRE(s.data);
    s.data[0] = int16_t(700 + n); s.data[1] = -400;
  }
  REQUIRE(cloneInstrument(10, 11));
  for (int n = 0; n < 2; ++n) {
    CHECK(project().instruments[11].chip.scwf.oscillator[n].data != project().instruments[10].chip.scwf.oscillator[n].data);
    CHECK(project().instruments[11].chip.scwf.oscillator[n].data[0] == 700 + n);
  }
  REQUIRE(editHistoryUndo());
  CHECK(project().instruments[11].type == InstrumentType::none);
  CHECK(project().instruments[10].chip.scwf.oscillator[0].data[0] == 700);
  REQUIRE(editHistoryRedo());
  CHECK(project().instruments[11].chip.scwf.oscillator[1].data[0] == 701);
}

TEST_CASE_FIXTURE(HistoryFixture, "Undo retains SoundFont handles until history eviction") {
  engine(10, InstrumentType::SoundFont);
  auto prepared = std::make_shared<soundbank::PreparedPreset>();
  prepared->bytes = 16;
  std::weak_ptr<const soundbank::PreparedPreset> weak = prepared;
  const auto handle = soundbank::publish(prepared);
  prepared.reset();
  project().instruments[10].chip.soundfont.prepared = handle;
  {
    EditHistoryScope edit("SoundFont engine");
    REQUIRE(editHistoryCaptureInstrument(10));
    engine(10, InstrumentType::Braids);
  }
  soundbank::collect([](uint32_t) { return false; });
  REQUIRE(soundbank::prepared(handle));
  REQUIRE(editHistoryUndo());
  CHECK(project().instruments[10].chip.soundfont.prepared == handle);
  soundbank::collect([&](uint32_t h) { return h == handle; });
  REQUIRE(editHistoryRedo());
  editHistoryClear();
  soundbank::collect([](uint32_t) { return false; });
  CHECK_FALSE(soundbank::prepared(handle));
  CHECK(weak.expired());
}

TEST_CASE_FIXTURE(HistoryFixture, "Undo records model side effects and barriers unrelated parameter edits") {
  engine(10, InstrumentType::Braids);
  auto& b = project().instruments[10].chip.braids;
  const auto original = b;
  {
    EditHistoryScope edit("Model");
    REQUIRE(editHistoryCaptureModel(10));
    b.model = 3; b.timbre = 1234;
  }
  REQUIRE(editHistoryUndo());
  CHECK(b.model == original.model);
  CHECK(b.timbre == original.timbre);
  REQUIRE(editHistoryRedo());
  {
    EditHistoryScope edit("Parameter outside initial scope");
    REQUIRE(editHistoryCaptureModel(10));
    b.color = 999;
  }
  CHECK_FALSE(editHistoryCanUndo());
  CHECK_FALSE(editHistoryCanRedo());
  CHECK(b.color == 999);
}

TEST_CASE_FIXTURE(HistoryFixture, "Undo remapping consumes repeats and releases after a mapping change") {
  note(60); note(61);
  const InputCode input{InputDeviceType::gamepad, 12345};
  appSettings.keyMapping.keyUndo[0] = input;
  CHECK(editHistoryInput(input, true));
  CHECK(project().phrases[0].rows[0].note == 60);
  CHECK(editHistoryInput(input, true));
  CHECK(project().phrases[0].rows[0].note == 60);
  appSettings.keyMapping.keyUndo[0] = {InputDeviceType::none, 0};
  CHECK(editHistoryInput(input, false));
  CHECK_FALSE(editHistoryInput(input, true));
  CHECK(editHistoryInput({InputDeviceType::logical, keyRedo}, true));
  CHECK(editHistoryInput({InputDeviceType::logical, keyRedo}, false));
  CHECK(project().phrases[0].rows[0].note == 61);
}
