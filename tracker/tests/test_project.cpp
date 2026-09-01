#include "doctest.h"

#include "project.h"
#include "project_utils.h"

#include <cstring>
#include <cstdlib>

TEST_SUITE("project") {

// Test fixture
struct ProjectFixture {
  Project p;

  ProjectFixture() {
    projectInit(&p);
  }
  ~ProjectFixture() = default;
};

// projectInit tests

TEST_CASE_FIXTURE(ProjectFixture, "projectInit song is empty") {
  for (int r = 0; r < PROJECT_MAX_LENGTH; r++)
    for (int c = 0; c < PROJECT_MAX_TRACKS; c++)
      CHECK(p.song[r][c] == EMPTY_VALUE_16);
}

TEST_CASE_FIXTURE(ProjectFixture, "projectInit default groove") {
  CHECK(p.grooves[0].speed[0] == 6);
  CHECK(p.grooves[0].speed[1] == 6);
  CHECK(p.grooves[0].speed[2] == EMPTY_VALUE_8);
}

TEST_CASE_FIXTURE(ProjectFixture, "projectInit default track tilt") {
  CHECK(p.tiltPivotHz == 1000);
  for (int i = 0; i < PROJECT_MAX_TRACKS; ++i) CHECK(p.trackTilt[i] == 0x80);
}

TEST_CASE("track tilt project settings survive save and load") {
  Project saved, loaded;
  projectInit(&saved);
  projectInit(&loaded);
  saved.chipsCount = 1;
  saved.tracksCount = 1;
  saved.chipType = ChipType::AY;
  std::strcpy(saved.pitchTable.name, "Test");
  saved.pitchTable.length = 1;
  std::strcpy(saved.pitchTable.noteNames[0], "C-4");
  saved.pitchTable.values[0] = 1000;
  saved.trackTilt[0] = 0x00;
  saved.trackTilt[7] = 0xff;
  saved.tiltPivotHz = 2500;
  const char* path = "build/tests/track_tilt_io.cct";
  REQUIRE(projectSave(&saved, path) == 0);
  INFO(projectFileError);
  REQUIRE(projectLoad(&loaded, path) == 0);
  CHECK(loaded.trackTilt[0] == 0x00);
  CHECK(loaded.trackTilt[7] == 0xff);
  CHECK(loaded.tiltPivotHz == 2500);
}

TEST_CASE("new projects initialize the validated period pitch table") {
  Project project;
  projectInitAY(&project);
  CHECK(project.linearPitch == 0);
  CHECK(project.pitchTable.values[0] > project.pitchTable.values[12]);
  CHECK(project.pitchTable.values[12] > project.pitchTable.values[24]);
}

TEST_CASE_FIXTURE(ProjectFixture, "projectInit other grooves empty") {
  for (int g = 1; g < PROJECT_MAX_GROOVES; g++)
    CHECK(grooveIsEmpty(&p, g));
}

TEST_CASE_FIXTURE(ProjectFixture, "projectInit instruments empty") {
  for (int i = 0; i < PROJECT_MAX_INSTRUMENTS; i++)
    CHECK(instrumentIsEmpty(&p, i));
}

TEST_CASE_FIXTURE(ProjectFixture, "projectFree releases instrument sample data") {
  Instrument* instrument = &p.instruments[0];
  getInstrumentFunctions(InstrumentType::Sample).init(instrument);
  instrument->chip.sample.data = static_cast<int16_t*>(std::malloc(sizeof(int16_t)));
  REQUIRE(instrument->chip.sample.data != nullptr);
  projectFree(&p);
  CHECK(instrument->type == InstrumentType::none);
  CHECK(instrument->chip.sample.data == nullptr);
}

TEST_CASE_FIXTURE(ProjectFixture, "projectInit phrases empty") {
  for (int i = 0; i < PROJECT_MAX_PHRASES; i++)
    CHECK(phraseIsEmpty(&p, i));
}

TEST_CASE_FIXTURE(ProjectFixture, "projectInit chains empty") {
  for (int i = 0; i < PROJECT_MAX_CHAINS; i++)
    CHECK(chainIsEmpty(&p, i));
}

TEST_CASE_FIXTURE(ProjectFixture, "projectInit tables empty") {
  for (int i = 0; i < PROJECT_MAX_TABLES; i++)
    CHECK(tableIsEmpty(&p, i));
}

TEST_CASE("modulation destination limits match every engine's routing") {
  CHECK(instrumentModDestinationMax(InstrumentType::AY1) == 22);
  CHECK(instrumentModDestinationMax(InstrumentType::AY2) == 28);
  CHECK(instrumentModDestinationMax(InstrumentType::AYSample) == 24);
  CHECK(instrumentModDestinationMax(InstrumentType::Braids) == 31);
  CHECK(instrumentModDestinationMax(InstrumentType::Sample) == 33);
  CHECK(instrumentModDestinationMax(InstrumentType::SCWF) == 31);
  CHECK(instrumentModDestinationMax(InstrumentType::BYOWTBL) == 33);
  CHECK(instrumentModDestinationMax(InstrumentType::Plaits) == 33);
  CHECK(instrumentModDestinationMax(InstrumentType::PlaitsAlt) == 33);
}

TEST_CASE("voice-post modulation destinations keep their labels") {
  static const char* labels[] = {"ADSR A", "ADSR D", "ADSR S", "ADSR R", "ADSR Shape", "Trig D", "Trig C"};
  int firstGeneric = getInstrumentFunctions(InstrumentType::Plaits).modDestinationsCount + 1;
  for (int i = 0; i < genericModTotalCount - genericModEnvelopeAttack; ++i)
    CHECK(std::strcmp(instrumentModDestinationName(InstrumentType::Plaits,
      firstGeneric + genericModEnvelopeAttack + i), labels[i]) == 0);
}

TEST_CASE("instrument catalogue covers every family and its routable motion FX") {
  for (int rawType = 0; rawType < (int)InstrumentType::totalCount; ++rawType) {
    InstrumentType type = (InstrumentType)rawType;
    const InstrumentDefinition* definition = getInstrumentDefinition(type);
    REQUIRE(definition->uiName[0] != 0);
    // AYSample retains one legacy reserved destination in its serialized
    // numeric range; only declared destinations are exposed by the UI.
    REQUIRE(definition->destinationCount <= getInstrumentFunctions(type).modDestinationsCount + 1);
    for (int destination = 0; destination < definition->destinationCount; ++destination) {
      const InstrumentModDestination* metadata = instrumentModDestination(type, destination);
      REQUIRE(metadata != nullptr);
      CHECK(std::strcmp(metadata->name, instrumentModDestinationName(type, destination)) == 0);
      if (metadata->fx != instrumentNoFX) CHECK(instrumentFXAvailable(type, metadata->fx));
    }
    for (int fx = 0; fx < definition->fxCount; ++fx)
      CHECK(instrumentFXAvailable(type, definition->fxList[fx].fx));
  }

  Instrument instrument;
  uint8_t fx; int base, range; InstrumentMotionValue value;
  getInstrumentFunctions(InstrumentType::Sample).init(&instrument);
  CHECK(instrumentMotionDestination(&instrument, 3, &fx, &base, &range, &value));
  CHECK(fx == fxSST); CHECK(base == 0); CHECK(range == 255);
  CHECK(instrumentMotionDestination(&instrument, 5, &fx, &base, &range, &value));
  CHECK(fx == fxSSP); CHECK(base == 100); CHECK(range == 500);
  CHECK(instrumentMotionDestination(&instrument, 7, &fx, &base, &range, &value));
  CHECK(fx == fxSCF); CHECK(value == InstrumentMotionValue::cutoff);
  getInstrumentFunctions(InstrumentType::SCWF).init(&instrument);
  CHECK(instrumentMotionDestination(&instrument, 3, &fx, &base, &range, &value));
  CHECK(fx == fxSDT);
  getInstrumentFunctions(InstrumentType::BYOWTBL).init(&instrument);
  CHECK(instrumentMotionDestination(&instrument, 5, &fx, &base, &range, &value));
  CHECK(fx == fxBIA);

  static const char* aChChidDestinations[] = {
    "Off", "Volume", "Pitch", "Cutoff", "Reso", "EnvMod", "Decay", "Accent"
  };
  const InstrumentDefinition* aChChid = getInstrumentDefinition(InstrumentType::AChChid);
  REQUIRE(aChChid->destinationCount == 8);
  for (int destination = 0; destination < aChChid->destinationCount; ++destination)
    CHECK(std::strcmp(aChChid->destinations[destination].name, aChChidDestinations[destination]) == 0);
}

TEST_CASE("new instruments use audible synth defaults") {
  Instrument instrument;
  getInstrumentFunctions(InstrumentType::Braids).init(&instrument);
  CHECK(instrument.volume == 255);
  CHECK(instrument.chip.braids.attack == 0);
  CHECK(instrument.chip.braids.decay == 0);
  CHECK(instrument.chip.braids.sustain == 255);
  CHECK(instrument.chip.braids.release == 0);
  CHECK(instrument.chip.braids.filterCharacter == 2);
  CHECK(instrument.chip.braids.filterCutoffHz == 20000);
  CHECK(instrument.chip.braids.timbre == 16384);
  CHECK(instrument.chip.braids.color == 16384);

  getInstrumentFunctions(InstrumentType::Plaits).init(&instrument);
  CHECK(instrument.chip.plaits.harmonics == 16384);
  CHECK(instrument.chip.plaits.timbre == 16384);
  CHECK(instrument.chip.plaits.morph == 16384);
  CHECK(instrument.chip.plaits.decay == 0);
  CHECK(instrument.chip.plaits.auxMix == 0);

  getInstrumentFunctions(InstrumentType::Sample).init(&instrument);
  CHECK(instrument.chip.sample.start == 0);
  CHECK(instrument.chip.sample.end == 255);
  CHECK(instrument.chip.sample.loopMode == 0);
  CHECK(instrument.chip.sample.speedPercent == 100);

  const InstrumentType voiceTypes[] = {InstrumentType::Braids, InstrumentType::Sample,
    InstrumentType::SCWF, InstrumentType::BYOWTBL, InstrumentType::Plaits, InstrumentType::PlaitsAlt};
  for (InstrumentType type : voiceTypes) {
    getInstrumentFunctions(type).init(&instrument);
    const InstrumentVoicePostSettings* post = type == InstrumentType::Braids ?
      static_cast<const InstrumentVoicePostSettings*>(&instrument.chip.braids) :
      type == InstrumentType::Sample ? static_cast<const InstrumentVoicePostSettings*>(&instrument.chip.sample) :
      type == InstrumentType::SCWF ? static_cast<const InstrumentVoicePostSettings*>(&instrument.chip.scwf) :
      type == InstrumentType::BYOWTBL ? static_cast<const InstrumentVoicePostSettings*>(&instrument.chip.byowtbl) :
      static_cast<const InstrumentVoicePostSettings*>(&instrument.chip.plaits);
    CHECK(post->sustain == 255);
    CHECK(post->filterEnabled == 1);
    CHECK(post->filterCharacter == 2);
    CHECK(post->filterCutoffHz == 20000);
  }
}

// instrumentIsEmpty tests

TEST_CASE_FIXTURE(ProjectFixture, "instrumentIsEmpty true for none") {
  CHECK(instrumentIsEmpty(&p, 0));
}

TEST_CASE_FIXTURE(ProjectFixture, "instrumentIsEmpty false for ay") {
  p.instruments[0].type = InstrumentType::AY1;
  CHECK_FALSE(instrumentIsEmpty(&p, 0));
}

// chainIsEmpty tests

TEST_CASE_FIXTURE(ProjectFixture, "chainIsEmpty true after init") {
  CHECK(chainIsEmpty(&p, 0));
}

TEST_CASE_FIXTURE(ProjectFixture, "chainIsEmpty false with phrase") {
  p.chains[0].rows[5].phrase = 1;
  CHECK_FALSE(chainIsEmpty(&p, 0));
}

// phraseIsEmpty tests

TEST_CASE_FIXTURE(ProjectFixture, "phraseIsEmpty true after init") {
  CHECK(phraseIsEmpty(&p, 0));
}

TEST_CASE_FIXTURE(ProjectFixture, "phraseIsEmpty false with note") {
  p.phrases[0].rows[0].note = 42;
  CHECK_FALSE(phraseIsEmpty(&p, 0));
}

TEST_CASE_FIXTURE(ProjectFixture, "phraseIsEmpty false with instrument") {
  p.phrases[0].rows[3].instrument = 1;
  CHECK_FALSE(phraseIsEmpty(&p, 0));
}

TEST_CASE_FIXTURE(ProjectFixture, "phraseIsEmpty false with volume") {
  p.phrases[0].rows[7].volume = 10;
  CHECK_FALSE(phraseIsEmpty(&p, 0));
}

TEST_CASE_FIXTURE(ProjectFixture, "phraseIsEmpty false with fx") {
  p.phrases[0].rows[0].fx[1][0] = fxARP;
  CHECK_FALSE(phraseIsEmpty(&p, 0));
}

TEST_CASE_FIXTURE(ProjectFixture, "phraseIsEmpty false with fx value") {
  p.phrases[0].rows[0].fx[2][1] = 0x50;
  CHECK_FALSE(phraseIsEmpty(&p, 0));
}

// tableIsEmpty tests

TEST_CASE_FIXTURE(ProjectFixture, "tableIsEmpty true after init") {
  CHECK(tableIsEmpty(&p, 0));
}

TEST_CASE_FIXTURE(ProjectFixture, "tableIsEmpty false with pitch flag") {
  p.tables[0].rows[0].pitchFlag = 1;
  CHECK_FALSE(tableIsEmpty(&p, 0));
}

TEST_CASE_FIXTURE(ProjectFixture, "tableIsEmpty false with pitch offset") {
  p.tables[0].rows[0].pitchOffset = 5;
  CHECK_FALSE(tableIsEmpty(&p, 0));
}

TEST_CASE_FIXTURE(ProjectFixture, "tableIsEmpty false with volume") {
  p.tables[0].rows[0].volume = 10;
  CHECK_FALSE(tableIsEmpty(&p, 0));
}

TEST_CASE_FIXTURE(ProjectFixture, "tableIsEmpty false with fx") {
  p.tables[0].rows[0].fx[3][0] = fxVOL;
  CHECK_FALSE(tableIsEmpty(&p, 0));
}

// grooveIsEmpty tests

TEST_CASE_FIXTURE(ProjectFixture, "grooveIsEmpty true for unused") {
  CHECK(grooveIsEmpty(&p, 1));
}

TEST_CASE_FIXTURE(ProjectFixture, "grooveIsEmpty false with speed") {
  p.grooves[1].speed[0] = 8;
  CHECK_FALSE(grooveIsEmpty(&p, 1));
}

// Clear function tests

TEST_CASE_FIXTURE(ProjectFixture, "phraseClear") {
  p.phrases[0].rows[0].note = 42;
  p.phrases[0].rows[5].instrument = 1;
  p.phrases[0].rows[10].fx[0][0] = fxARP;
  p.phrases[0].rows[10].fx[0][1] = 0x37;
  phraseClear(&p.phrases[0]);
  CHECK(phraseIsEmpty(&p, 0));
}

TEST_CASE_FIXTURE(ProjectFixture, "chainClear") {
  p.chains[0].rows[0].phrase = 5;
  p.chains[0].rows[0].transpose = 3;
  chainClear(&p.chains[0]);
  CHECK(chainIsEmpty(&p, 0));
  CHECK(p.chains[0].rows[0].transpose == 0);
}

TEST_CASE_FIXTURE(ProjectFixture, "instrumentClear") {
  p.instruments[0].type = InstrumentType::AY1;
  std::strcpy(p.instruments[0].name, "Test");
  instrumentClear(&p.instruments[0]);
  CHECK(instrumentIsEmpty(&p, 0));
  CHECK(std::strcmp(p.instruments[0].name, "") == 0);
  CHECK(p.instruments[0].volume == 255);
}

TEST_CASE_FIXTURE(ProjectFixture, "tableClear") {
  p.tables[0].rows[0].pitchFlag = 1;
  p.tables[0].rows[0].pitchOffset = 10;
  p.tables[0].rows[0].fx[0][0] = fxVOL;
  tableClear(&p.tables[0]);
  CHECK(tableIsEmpty(&p, 0));
}

// noteName tests

TEST_CASE_FIXTURE(ProjectFixture, "noteName off") {
  CHECK(std::strcmp(noteName(&p, NOTE_OFF), "OFF") == 0);
}

TEST_CASE_FIXTURE(ProjectFixture, "noteName empty") {
  CHECK(std::strcmp(noteName(&p, EMPTY_VALUE_8), "---") == 0);
}

TEST_CASE_FIXTURE(ProjectFixture, "noteName out of range") {
  p.pitchTable.length = 12;
  CHECK(std::strcmp(noteName(&p, 13), "---") == 0);
}

TEST_CASE_FIXTURE(ProjectFixture, "noteName valid") {
  p.pitchTable.length = 12;
  std::strcpy(p.pitchTable.noteNames[0], "C-4");
  CHECK(std::strcmp(noteName(&p, 0), "C-4") == 0);
}

// Track count tests

TEST_CASE_FIXTURE(ProjectFixture, "getChipTracks ay") {
  CHECK(projectGetChipTracks(&p, 0) == 1);
}

TEST_CASE_FIXTURE(ProjectFixture, "getTotalTracks single chip") {
  p.chipsCount = 1;
  CHECK(projectGetTotalTracks(&p) == 1);
}

TEST_CASE_FIXTURE(ProjectFixture, "getTotalTracks multiple chips") {
  p.chipsCount = 3;
  CHECK(projectGetTotalTracks(&p) == 3);
}

// fillFXNames tests

TEST_CASE("fillFXNames common") {
  fillFXNames();
  CHECK(std::strcmp(fxNames[fxARP].name, "ARP") == 0);
  CHECK(std::strcmp(fxNames[fxHOP].name, "HOP") == 0);
  CHECK(std::strcmp(fxNames[fxVOL].name, "VOL") == 0);
  CHECK(std::strcmp(fxNames[fxVSL].name, "VSL") == 0);
}

TEST_CASE("fillFXNames ay") {
  fillFXNames();
  CHECK(std::strcmp(fxNames[fxAYM].name, "AYM") == 0);
  CHECK(std::strcmp(fxNames[fxEPH].name, "EPH") == 0);
}

TEST_CASE("fillFXNames unknown") {
  fillFXNames();
  CHECK(std::strcmp(fxNames[200].name, "---") == 0);
}

} // TEST_SUITE("project")
