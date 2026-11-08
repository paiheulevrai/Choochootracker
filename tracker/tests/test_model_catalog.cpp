#include "doctest.h"
#include "screens/model_catalog.h"
#include <string.h>

TEST_CASE("every Braids and Plaits model belongs to exactly one category") {
  CHECK(modelCatalogsValid());
}

TEST_CASE("Bogie models are split evenly across useful sound families") {
  REQUIRE(drumSynthCategoryCount == 3);
  for (int category = 0; category < drumSynthCategoryCount; ++category)
    CHECK(drumSynthCategories[category].childCount == 4);
}

TEST_CASE("Plaits-Alt contains exactly its 24 supplemental engines") {
  int count = 0;
  bool seen[24] = {};
  for (int category = 0; category < plaitsAltCategoryCount; ++category) {
    for (int item = 0; item < plaitsAltCategories[category].childCount; ++item) {
      int value = plaitsAltCategories[category].children[item].value;
      CHECK(value >= 0);
      CHECK(value < 24);
      CHECK_FALSE(seen[value]);
      seen[value] = true;
      ++count;
    }
  }
  CHECK(count == 24);
}

TEST_CASE("model names come from the catalog") {
  CHECK((int)SinteredModel::shard == 1);
  CHECK(strcmp(modelCatalogName(InstrumentType::Sintered, 1), "CREASE") == 0);
  CHECK(strcmp(digiSophieModelName(3), "SHARD") == 0);
  CHECK(strcmp(modelCatalogName(InstrumentType::Braids, 46), "DIGI-MOD") == 0);
  CHECK(strcmp(modelCatalogName(InstrumentType::Plaits, 5), "WAVE TERRAIN") == 0);
  CHECK(strcmp(modelCatalogName(InstrumentType::PlaitsAlt, 23), "PHASE FLOCK") == 0);
  CHECK(strcmp(modelCatalogName(InstrumentType::Sintered, 5), "MELT") == 0);
  CHECK(strcmp(modelCatalogName(InstrumentType::Sintered, 6), "SQUALL") == 0);
  CHECK(strcmp(modelCatalogName(InstrumentType::Plaits, 24), "UNKNOWN") == 0);
}

TEST_CASE("model-specific modulation labels and classic modulation defaults") {
  Instrument instrument = {};
  getInstrumentFunctions(InstrumentType::Sintered).init(&instrument);
  instrument.chip.sintered.model = SinteredModel::melt;
  CHECK(strcmp(instrumentModDestinationNameForInstrument(&instrument, 5), "Ratio") == 0);
  CHECK(strcmp(instrumentModDestinationNameForInstrument(&instrument, 6), "Chaos") == 0);
  CHECK(strcmp(instrumentModDestinationNameForInstrument(&instrument, 8), "Drive") == 0);
  instrument.chip.sintered.model = SinteredModel::squall;
  CHECK(strcmp(instrumentModDestinationNameForInstrument(&instrument, 4), "FM") == 0);
  CHECK(strcmp(instrumentModDestinationNameForInstrument(&instrument, 6), "Voices") == 0);
  CHECK(strcmp(instrumentModDestinationNameForInstrument(&instrument, 8), "Clip") == 0);
  const ModulationType defaultTypes[] = {
    ModulationType::ADSR, ModulationType::AHD,
    ModulationType::LFO, ModulationType::LFO
  };
  for (int slot = 0; slot < 4; ++slot) {
    CHECK(instrument.modulation[slot].type == defaultTypes[slot]);
    CHECK(instrument.modulation[slot].destination == 0);
    CHECK(instrument.modulation[slot].amount == 0);
  }

  getInstrumentFunctions(InstrumentType::MME).init(&instrument);
  instrument.chip.mme.model = MMEModel::sync;
  CHECK(instrument.chip.mme.waves == 80);
  CHECK(instrument.chip.mme.amount == 255);
  CHECK(strcmp(instrumentModDestinationNameForInstrument(&instrument, 5), "SyncAmt") == 0);
  instrument.chip.mme.model = MMEModel::ring;
  CHECK(strcmp(instrumentModDestinationNameForInstrument(&instrument, 6), "RingType") == 0);
  for (int slot = 0; slot < 4; ++slot) {
    CHECK(instrument.modulation[slot].type == defaultTypes[slot]);
    CHECK(instrument.modulation[slot].destination == 0);
    CHECK(instrument.modulation[slot].amount == 0);
  }

  getInstrumentFunctions(InstrumentType::Sample).init(&instrument);
  for (int slot = 0; slot < 4; ++slot) {
    CHECK(instrument.modulation[slot].type == defaultTypes[slot]);
    CHECK(instrument.modulation[slot].destination == 0);
    CHECK(instrument.modulation[slot].amount == 0);
  }

  getInstrumentFunctions(InstrumentType::AChChid).init(&instrument);
  for (int slot = 0; slot < 4; ++slot) {
    CHECK(instrument.modulation[slot].type == defaultTypes[slot]);
    CHECK(instrument.modulation[slot].destination == 0);
    CHECK(instrument.modulation[slot].amount == 0);
  }
}
