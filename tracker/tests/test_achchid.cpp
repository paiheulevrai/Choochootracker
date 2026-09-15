#include "doctest.h"
#include "project_instruments.h"
#include "project.h"
#include "synth/achchid_voice.h"
#include <cmath>
#include <vector>

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

TEST_CASE("aChChid output is softly bounded before the mixer") {
  AChChidVoice voice;
  voice.init(48000.0f);
  voice.configure(1, 0, 0, 16384, 16384, 400, 100, 100, 2000, 100, 4.0f);
  voice.noteOn(48, true, false, 0);
  std::vector<float> output(4096);
  voice.render(output.data(), (int)output.size());
  for (float sample : output) {
    CHECK(std::isfinite(sample));
    CHECK(std::fabs(sample) <= 0.85001f);
  }
}
