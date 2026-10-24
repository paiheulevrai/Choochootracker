#include "doctest.h"
#include "chipnomad_lib.h"
#include "screen_instrument.h"
#include "app_ui_mock.h"
#include <cstring>
#include <initializer_list>

namespace {
struct NativePreviewFixture {
  ChipNomadState* previousState = chipnomadState;
  AppSettings previousSettings = appSettings;
  int previousInstrument = cInstrument;

  NativePreviewFixture() {
    chipnomadState = chipnomadCreate();
    cInstrument = 0;
  }
  ~NativePreviewFixture() {
    chipnomadDestroy(chipnomadState);
    chipnomadState = previousState;
    appSettings = previousSettings;
    cInstrument = previousInstrument;
  }
};
}

TEST_CASE_FIXTURE(NativePreviewFixture, "SID redraw renders without an FM amp and FM keeps its optional overlay") {
  auto& instrument = chipnomadState->project.instruments[0];
  for (bool persistent : {false, true}) {
    appSettings.persistentWaveform = persistent;
    CAPTURE(persistent);
    for (auto type : {InstrumentType::SID, InstrumentType::OPLL}) {
      CAPTURE(int(type));
      getInstrumentFunctions(type).init(&instrument);
      auto* envelope = instrumentFMAmpSettings(&instrument);
      if (type == InstrumentType::SID) REQUIRE(envelope == nullptr);
      else REQUIRE(envelope != nullptr);
      for (bool enabled : {false, true}) {
        if (envelope) envelope->enabled = enabled;
        Instrument before = instrument;
        mockEnvelopePreviewCount = 0;
        mockBitmapDrawCount = 0;
        instrumentFMRefreshStaticWaveform();
        CHECK(mockBitmapDrawCount == 1);
        CHECK(mockBitmapDrawRow == 16);
        CHECK(mockEnvelopePreviewCount == (envelope && enabled ? 1 : 0));
        CHECK(std::memcmp(&before, &instrument, sizeof(Instrument)) == 0);
      }
    }
  }
}
