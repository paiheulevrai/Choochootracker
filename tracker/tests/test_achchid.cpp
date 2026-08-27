#include "doctest.h"
#include "project_instruments.h"
#include "project.h"

TEST_CASE("aChChid has native controls and dedicated FX") {
  Instrument instrument = {};
  getInstrumentFunctions(InstrumentType::AChChid).init(&instrument);
  CHECK(instrument.type == InstrumentType::AChChid);
  CHECK(instrument.chip.achchid.wave == AChChidWave::saw);
  CHECK(instrument.chip.achchid.decay == 1000);
  CHECK(instrumentVoicePostSettings(&instrument) == nullptr);
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxASL));
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxADC));
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxAAC));
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxATM));
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxACL));
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxACF));
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxARS));
  CHECK(instrumentFXAvailable(InstrumentType::AChChid, fxAEM));
  CHECK_FALSE(instrumentFXAvailable(InstrumentType::AChChid, fxBMD));
}
