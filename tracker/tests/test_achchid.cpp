#include "doctest.h"
#include "project_instruments.h"
#include "project.h"

TEST_CASE("aChChid has native controls and only ASL") {
  Instrument instrument = {};
  getInstrumentFunctions(InstrumentType::AChChid).init(&instrument);
  CHECK(instrument.type == InstrumentType::AChChid);
  CHECK(instrument.chip.achchid.wave == AChChidWave::saw);
  CHECK(instrument.chip.achchid.decay == 1000);
  CHECK(instrumentVoicePostSettings(&instrument) == nullptr);
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxASL));
  CHECK_FALSE(instrumentFXAvailable(InstrumentType::AChChid, fxBMD));
}
