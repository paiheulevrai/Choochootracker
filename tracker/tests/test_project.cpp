#include "doctest.h"

#include "project.h"
#include "project_io_common.h"
#include "project_utils.h"
#include "import/import_vt2.h"
#include "synth/multimode_filter.h"

#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>
#include <cstdio>

TEST_SUITE("project") {

TEST_CASE("MIDI CC maps continuous engine destinations to native values") {
  Project p;
  projectInit(&p);
  Instrument* plaits = &p.instruments[0];
  plaits->type = InstrumentType::Plaits;
  CHECK(instrumentCCDestinationAvailable(plaits, 1));
  CHECK(instrumentSetCCDestination(plaits, 1, 0));
  CHECK(plaits->volume == 0);
  CHECK(instrumentSetCCDestination(plaits, 1, 127));
  CHECK(plaits->volume == 255);
  CHECK(instrumentCCDestinationAvailable(plaits, 4));
  CHECK(instrumentSetCCDestination(plaits, 4, 0));
  CHECK(plaits->chip.plaits.timbre == 0);
  CHECK(instrumentSetCCDestination(plaits, 4, 127));
  CHECK(plaits->chip.plaits.timbre == 32767);
  CHECK(instrumentSetCCDestination(plaits, 7, 0));
  CHECK(plaits->chip.plaits.filterCutoffHz == FILTER_CUTOFF_MIN_HZ);
  CHECK(instrumentSetCCDestination(plaits, 7, 127));
  CHECK(plaits->chip.plaits.filterCutoffHz == FILTER_CUTOFF_MAX_HZ);
  projectFree(&p);
}

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

TEST_CASE_FIXTURE(ProjectFixture, "projectInit enables MIDI CC mapping rows") {
  for (int i = 0; i < PROJECT_MAX_MIDI_CC_MAPPINGS; ++i)
    CHECK(p.midiCCMappings[i].enabled == 1);
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

TEST_CASE("MIDI CC mappings survive save and load") {
  Project saved, loaded;
  projectInit(&saved); projectInit(&loaded);
  saved.chipsCount = loaded.chipsCount = 1;
  saved.tracksCount = loaded.tracksCount = 1;
  saved.chipType = loaded.chipType = ChipType::AY;
  std::strcpy(saved.title, "MIDI CC");
  std::strcpy(saved.pitchTable.name, "MIDI CC");
  saved.midiCCMappings[0] = {1, 2, 74, 3, 4};
  const char* path = "build/tests/midi_cc_io.cct";
  REQUIRE(projectSave(&saved, path) == 0);
  REQUIRE(projectLoad(&loaded, path) == 0);
  CHECK(loaded.midiCCMappings[0].enabled == 1);
  CHECK(loaded.midiCCMappings[0].channel == 2);
  CHECK(loaded.midiCCMappings[0].cc == 74);
  CHECK(loaded.midiCCMappings[0].instrument == 3);
  CHECK(loaded.midiCCMappings[0].destination == 4);
}

TEST_CASE("scale project settings survive save and load") {
  Project saved, loaded;
  projectInit(&saved);
  projectInit(&loaded);
  saved.chipsCount = 1;
  saved.tracksCount = 1;
  saved.chipType = ChipType::AY;
  saved.tickRate = 50;
  saved.pitchTable.length = 1;
  saved.pitchTable.octaveSize = 12;
  std::strcpy(saved.pitchTable.name, "Test");
  std::strcpy(saved.pitchTable.noteNames[0], "C-4");
  saved.pitchTable.values[0] = 1000;
  saved.scaleApply = 0;
  saved.scaleMode = 1;
  saved.scaleTracksMask = 0xa5;
  saved.scaleRoot = 9;
  saved.scalePreset = scaleCustom;
  saved.scaleCustomMask = 0x0491;
  const char* path = "build/tests/scale_io.cct";
  REQUIRE(projectSave(&saved, path) == 0);
  REQUIRE(projectLoad(&loaded, path) == 0);
  CHECK(loaded.scaleApply == 0);
  CHECK(loaded.scaleMode == 1);
  CHECK(loaded.scaleTracksMask == 0xa5);
  CHECK(loaded.scaleRoot == 9);
  CHECK(loaded.scalePreset == scaleCustom);
  CHECK(loaded.scaleCustomMask == 0x0491);
}

TEST_CASE("legacy scale line without mode field loads as Quantizer") {
  Project saved, loaded;
  projectInit(&saved);
  projectInit(&loaded);
  saved.chipsCount = 1;
  saved.tracksCount = 1;
  saved.chipType = ChipType::AY;
  saved.tickRate = 50;
  saved.pitchTable.length = 1;
  saved.pitchTable.octaveSize = 12;
  std::strcpy(saved.pitchTable.name, "Test");
  std::strcpy(saved.pitchTable.noteNames[0], "C-4");
  saved.pitchTable.values[0] = 1000;
  const char* path = "build/tests/scale_io_legacy.cct";
  REQUIRE(projectSave(&saved, path) == 0);
  // Rewrite the current 6-field Scale line into the pre-Note-Lock layout:
  // apply,root,preset,customMask,tracksMask (no mode field).
  {
    std::ifstream in(path);
    REQUIRE(in.is_open());
    std::string text, lineBuf;
    while (std::getline(in, lineBuf)) {
      if (lineBuf.rfind("- Scale: ", 0) == 0) lineBuf = "- Scale: 1,5,3,2741,165";
      text += lineBuf;
      text += '\n';
    }
    in.close();
    std::ofstream out(path);
    REQUIRE(out.is_open());
    out << text;
  }
  REQUIRE(projectLoad(&loaded, path) == 0);
  CHECK(loaded.scaleApply == 1);
  CHECK(loaded.scaleMode == 0);
  CHECK(loaded.scaleRoot == 5);
  CHECK(loaded.scalePreset == scaleDorian);
  CHECK(loaded.scaleCustomMask == 0x0ab5);
  CHECK(loaded.scaleTracksMask == 0xa5);
}

TEST_CASE("a project with fewer than 8 tracks survives save and load") {
  // Regression test: projectLoadInternal used to force chipsCount back to
  // PROJECT_MAX_TRACKS after reading it from the file, so any project saved
  // with fewer tracks (e.g. a VT2 import, which is 3 tracks) would then be
  // read back expecting a wider Song section than what was actually written,
  // and fail to load at all.
  Project saved, loaded;
  projectInitAY(&saved);
  projectInitAY(&loaded);
  saved.tracksCount = saved.chipsCount = 3;

  saved.song[0][2] = 0;
  saved.chains[0].rows[0].phrase = 0;
  saved.phrases[0].rows[0].note = 40;
  saved.phrases[0].rows[0].instrument = 0;
  saved.phrases[0].rows[0].volume = PHRASE_VOLUME_MAX;

  const char* path = "build/tests/reduced_tracks_io.cct";
  REQUIRE(projectSave(&saved, path) == 0);
  INFO(projectFileError);
  REQUIRE(projectLoad(&loaded, path) == 0);
  INFO(projectFileError);
  CHECK(loaded.tracksCount == 3);
  CHECK(loaded.chipsCount == 3);
  CHECK(loaded.song[0][2] == 0);
  CHECK(loaded.phrases[0].rows[0].note == 40);
}

TEST_CASE("sample slice survives save and load; missing field is Off") {
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
  getInstrumentFunctions(InstrumentType::Sample).init(&saved.instruments[0]);
  saved.instruments[0].chip.sample.slice = 16;
  const char* path = "build/tests/sample_slice_io.cct";
  REQUIRE(projectSave(&saved, path) == 0);
  INFO(projectFileError);
  REQUIRE(projectLoad(&loaded, path) == 0);
  CHECK(loaded.instruments[0].type == InstrumentType::Sample);
  CHECK(loaded.instruments[0].chip.sample.slice == 16);

  FILE* in = std::fopen(path, "r");
  REQUIRE(in != nullptr);
  const char* stripped = "build/tests/sample_slice_missing.cct";
  FILE* out = std::fopen(stripped, "w");
  REQUIRE(out != nullptr);
  char line[512];
  while (std::fgets(line, sizeof(line), in)) {
    if (std::strncmp(line, "- Sample slice:", 15) == 0) continue;
    std::fputs(line, out);
  }
  std::fclose(in);
  std::fclose(out);

  Project missing;
  projectInit(&missing);
  REQUIRE(projectLoad(&missing, stripped) == 0);
  CHECK(missing.instruments[0].type == InstrumentType::Sample);
  CHECK(missing.instruments[0].chip.sample.slice == 0);
}

TEST_CASE("projects embed loaded samples in a ZIP container") {
  Project saved, loaded;
  projectInitAY(&saved);
  projectInitAY(&loaded);
  getInstrumentFunctions(InstrumentType::Sample).init(&saved.instruments[0]);
  InstrumentSample& sample = saved.instruments[0].chip.sample;
  std::strcpy(sample.path, "samples/original.wav");
  sample.sampleRate = 8000;
  sample.frameCount = 4;
  sample.channels = 1;
  sample.data = static_cast<int16_t*>(std::malloc(4 * sizeof(int16_t)));
  REQUIRE(sample.data != nullptr);
  sample.data[0] = -1000; sample.data[1] = 2000; sample.data[2] = -3000; sample.data[3] = 4000;

  const char* path = "build/tests/sample_archive.cct";
  REQUIRE(projectSave(&saved, path) == 0);
  FILE* archive = std::fopen(path, "rb");
  REQUIRE(archive != nullptr);
  CHECK(std::fgetc(archive) == 'P');
  CHECK(std::fgetc(archive) == 'K');
  std::fclose(archive);

  REQUIRE(projectLoad(&loaded, path) == 0);
  const InstrumentSample& result = loaded.instruments[0].chip.sample;
  REQUIRE(result.data != nullptr);
  CHECK(result.frameCount == 4);
  CHECK(result.sampleRate == 8000);
  CHECK(result.data[0] == -1000);
  CHECK(result.data[3] == 4000);
  CHECK(std::strcmp(result.path, "samples/original.wav") == 0);
}

TEST_CASE("archives preserve BYOWTBL frame layout") {
  Project saved, loaded;
  projectInitAY(&saved);
  projectInitAY(&loaded);
  getInstrumentFunctions(InstrumentType::BYOWTBL).init(&saved.instruments[0]);
  InstrumentBYOWTBL& table = saved.instruments[0].chip.byowtbl;
  std::strcpy(table.oscillator[0].path, "samples/wavetable.wav");
  table.oscillator[0].sampleRate = 8000;
  table.oscillator[0].frameCount = 8;
  table.oscillator[0].channels = 1;
  table.oscillator[0].data = static_cast<int16_t*>(std::malloc(8 * sizeof(int16_t)));
  REQUIRE(table.oscillator[0].data != nullptr);
  table.frameSize[0] = 4;
  table.tableFrames[0] = 2;

  const char* path = "build/tests/byowtbl_archive.cct";
  REQUIRE(projectSave(&saved, path) == 0);
  REQUIRE(projectLoad(&loaded, path) == 0);

  const InstrumentBYOWTBL& result = loaded.instruments[0].chip.byowtbl;
  CHECK(result.frameSize[0] == 4);
  CHECK(result.tableFrames[0] == 2);
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
  CHECK(instrumentModDestinationMax(InstrumentType::AY1) == 49);
  CHECK(instrumentModDestinationMax(InstrumentType::AY2) == 55);
  CHECK(instrumentModDestinationMax(InstrumentType::AYSample) == 51);
  CHECK(instrumentModDestinationMax(InstrumentType::Braids) == 51);
  CHECK(instrumentModDestinationMax(InstrumentType::Sample) == 53);
  CHECK(instrumentModDestinationMax(InstrumentType::SCWF) == 51);
  CHECK(instrumentModDestinationMax(InstrumentType::BYOWTBL) == 53);
  CHECK(instrumentModDestinationMax(InstrumentType::Plaits) == 53);
  CHECK(instrumentModDestinationMax(InstrumentType::PlaitsAlt) == 53);
  CHECK(instrumentModDestinationMax(InstrumentType::AChChid) == 54);
  CHECK(instrumentModDestinationMax(InstrumentType::DrumSynth) == 55);
}

TEST_CASE("voice-post modulation destinations keep their labels") {
  static const char* labels[] = {"ADSR A", "ADSR D", "ADSR S", "ADSR R", "ADSR Shape", "Trig D", "Trig C",
                                 "M1 P5", "M2 P5", "M3 P5", "M4 P5"};
  int firstGeneric = getInstrumentFunctions(InstrumentType::Plaits).modDestinationsCount + 1;
  for (int i = 0; i < genericModFirstInsert - genericModEnvelopeAttack; ++i)
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
      const char* expectedName = destination == midiCCDestinationNone ? "-" : instrumentModDestinationName(type, destination);
      CHECK(std::strcmp(metadata->name, expectedName) == 0);
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
    "-", "Volume", "Pitch", "Cutoff", "Reso", "EnvMod", "Decay", "Accent", "Timbre", "Color"
  };
  const InstrumentDefinition* aChChid = getInstrumentDefinition(InstrumentType::AChChid);
  REQUIRE(aChChid->destinationCount == 10);
  for (int destination = 0; destination < aChChid->destinationCount; ++destination)
    CHECK(std::strcmp(aChChid->destinations[destination].name, aChChidDestinations[destination]) == 0);
  Instrument achchid;
  getInstrumentFunctions(InstrumentType::AChChid).init(&achchid);
  CHECK(instrumentMotionDestination(&achchid, 8, &fx, &base, &range, &value));
  CHECK(fx == fxATM); CHECK(range == 16384);
  CHECK(instrumentMotionDestination(&achchid, 9, &fx, &base, &range, &value));
  CHECK(fx == fxACL); CHECK(range == 16384);

  static const char* drumSynthDestinations[] = {
    "-", "Volume", "Pitch", "Decay", "Tone", "Sweep", "Noise", "FM", "Drive", "Cutoff", "Reso"
  };
  const InstrumentDefinition* drumSynth = getInstrumentDefinition(InstrumentType::DrumSynth);
  CHECK(std::strcmp(drumSynth->uiName, "Bogie") == 0);
  CHECK(drumSynth->category == InstrumentCategory::drums);
  REQUIRE(drumSynth->destinationCount == 11);
  for (int destination = 0; destination < drumSynth->destinationCount; ++destination)
    CHECK(std::strcmp(drumSynth->destinations[destination].name, drumSynthDestinations[destination]) == 0);
  getInstrumentFunctions(InstrumentType::DrumSynth).init(&instrument);
  uint8_t expectedFX[] = {fxDDC, fxDTO, fxDSW, fxDNO, fxDFM, fxDDR, fxDCF, fxDRS};
  int expectedBase[] = {72, 128, 150, 24, 32, 28, 20000, 0};
  int expectedRange[] = {255, 255, 255, 255, 255, 255, 20000, 255};
  for (int destination = 3; destination <= 10; ++destination) {
    CHECK(instrumentMotionDestination(&instrument, destination, &fx, &base, &range, &value));
    CHECK(fx == expectedFX[destination - 3]);
    CHECK(base == expectedBase[destination - 3]);
    CHECK(range == expectedRange[destination - 3]);
  }
  CHECK(instrumentMotionDestination(&instrument, 9, &fx, &base, &range, &value));
  CHECK(base == 20000); CHECK(range == 20000); CHECK(value == InstrumentMotionValue::cutoff);
  CHECK(instrumentMotionDestination(&instrument, 10, &fx, &base, &range, &value));
  CHECK(base == 0); CHECK(range == 255); CHECK(value == InstrumentMotionValue::raw);
  instrument.chip.drumSynth.engine = DrumSynthEngine::clap;
  CHECK(instrumentFXAvailableForInstrument(&instrument, fxDSW));
  CHECK(instrumentFXAvailableForInstrument(&instrument, fxDFM));
  CHECK(instrumentModDestinationAvailable(&instrument, 5));
  CHECK(instrumentModDestinationAvailable(&instrument, 7));
  CHECK(instrumentFXAvailableForInstrument(&instrument, fxDNO));
  CHECK(instrumentFXAvailableForInstrument(&instrument, fxDCF));
  CHECK(instrumentModDestinationAvailable(&instrument, 6));
  CHECK(instrumentModDestinationAvailable(&instrument, 9));
}

TEST_CASE("v4 projects preserve LFO wavetable settings") {
  Project saved, loaded;
  projectInitAY(&saved);
  projectInitAY(&loaded);
  getInstrumentFunctions(InstrumentType::AY2).init(&saved.instruments[0]);
  Modulation& mod = saved.instruments[0].modulation[2];
  mod.type = ModulationType::SLFO;
  mod.p1 = static_cast<uint8_t>(LFOShape::wavetable);
  mod.p2 = static_cast<uint8_t>(LFOTrigger::chain);
  mod.p3 = 12;
  mod.p4 = 8;
  mod.p5 = 42;
  const char* path = "build/tests/lfo_wavetable_v4.cct";
  REQUIRE(projectSave(&saved, path) == 0);
  INFO(projectFileError);
  REQUIRE(projectLoad(&loaded, path) == 0);
  CHECK(projectFileVersion == 7);
  const Modulation& reloaded = loaded.instruments[0].modulation[2];
  CHECK(reloaded.p1 == static_cast<uint8_t>(LFOShape::wavetable));
  CHECK(reloaded.p2 == static_cast<uint8_t>(LFOTrigger::chain));
  CHECK(reloaded.p5 == 42);
}

TEST_CASE("v3 projects default the LFO wavetable index to zero") {
  Project project;
  projectInit(&project);
  INFO(projectFileError);
  REQUIRE(projectLoad(&project, "packaging/common/projects/alf dance.cct") == 0);
  CHECK(projectFileVersion == 3);
  for (int instrument = 0; instrument < PROJECT_MAX_INSTRUMENTS; ++instrument)
    for (int mod = 0; mod < 4; ++mod)
      CHECK(project.instruments[instrument].modulation[mod].p5 == 0);
}

TEST_CASE("phrase FX groups put the active engine after Track FX") {
  CHECK(std::strcmp(fxGroups[0].name, "Sequencer FX") == 0);
  CHECK(std::strcmp(fxGroups[1].name, "Track FX") == 0);
  CHECK(fxGroups[1].columns == 4);
  CHECK(fxGroups[1].fxList[3].fx == fxCRD);
  // Merged sample FX order keeps SPL/SLI first (our fork's saved-project
  // numeric values); SST is the sample-start control, STA a legacy alias.
  CHECK(getInstrumentDefinition(InstrumentType::Sample)->fxList[3].fx == fxSST);
  CHECK(getInstrumentDefinition(InstrumentType::Sample)->fxList[4].fx == fxSTA);
  CHECK(fxGroups[2].instType == InstrumentType::AY1);
  CHECK(fxGroups[11].instType == InstrumentType::AChChid);
  CHECK(fxGroups[12].instType == InstrumentType::DrumSynth);
  CHECK(fxGroups[13].instType == InstrumentType::MME);
  CHECK(fxGroups[14].instType == InstrumentType::OPLL);
  CHECK(fxGroups[24].instType == InstrumentType::SID);
  CHECK(std::strcmp(fxGroups[26].name, "ADSR / Trigger FX") == 0);
  CHECK(std::strcmp(fxGroups[27].name, "Modulation FX") == 0);
}

TEST_CASE_FIXTURE(ProjectFixture, "failed VT2 import leaves its destination unchanged") {
  std::strcpy(p.title, "Keep this project");
  CHECK(projectLoadVT2(&p, "tests/test_empty_title.cnm") != 0);
  CHECK(std::strcmp(p.title, "Keep this project") == 0);
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
  CHECK(instrument.chip.sample.slice == 0);

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
  CHECK(p.tables[0].retriggerMode == TableRetriggerMode::instrument);
}

TEST_CASE_FIXTURE(ProjectFixture, "an empty structural table is saved") {
  p.tables[0].retriggerMode = TableRetriggerMode::free;
  CHECK_FALSE(tableIsEmpty(&p, 0));
  FILE* file = tmpfile();
  REQUIRE(file != nullptr);
  CHECK(saveTable(file, 0, &p.tables[0]) == 0);
  rewind(file);
  char header[64];
  fgets(header, sizeof(header), file);
  fgets(header, sizeof(header), file);
  CHECK(std::strstr(header, "Retrig: Free") != nullptr);
  fclose(file);
  tableClear(&p.tables[0]);
  CHECK(p.tables[0].retriggerMode == TableRetriggerMode::instrument);
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
  CHECK(std::strcmp(fxNames[255].name, "---") == 0);
}

} // TEST_SUITE("project")
