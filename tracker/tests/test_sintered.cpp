#include "doctest.h"
#include "project.h"
#include "synth/sintered_voice.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static InstrumentSintered sinteredFixture(SinteredModel model) {
  InstrumentSintered s = {}; s.model = model; s.decay = 80; s.mod = 150; s.a = 130; s.b = 110; s.motion = 128; s.c = 100;
  s.filterEnabled = 1; s.filterCharacter = 1; s.filterCutoffHz = 20000; return s;
}

TEST_CASE("Sintered renders bounded deterministic one shots") {
  for (int model = 0; model < (int)SinteredModel::totalCount; ++model) {
    InstrumentSintered s = sinteredFixture((SinteredModel)model); SinteredVoice a, b; a.init(48000); b.init(48000);
    a.configure(&s, 6000, 1, 20000, 0); b.configure(&s, 6000, 1, 20000, 0); a.noteOn(); b.noteOn();
    std::vector<float> left(48000 * 3), right(left.size()); a.render(left.data(), left.size()); b.render(right.data(), right.size());
    double energy = 0.0; for (size_t i = 0; i < left.size(); ++i) { CHECK(std::isfinite(left[i])); CHECK(std::fabs(left[i]) <= 1.01f); CHECK(left[i] == right[i]); energy += std::fabs(left[i]); }
    CAPTURE(model); CHECK(energy > .01); CHECK_FALSE(a.active());
  }
}

TEST_CASE("Sintered motion center is static and both envelope sides alter sound") {
  InstrumentSintered base = sinteredFixture(SinteredModel::burst), ad = base, decay = base;
  ad.motion = 16; decay.motion = 240; SinteredVoice a, b, c; a.init(48000); b.init(48000); c.init(48000);
  a.configure(&base, 6000, 1, 20000, 0); b.configure(&ad, 6000, 1, 20000, 0); c.configure(&decay, 6000, 1, 20000, 0);
  a.noteOn(); b.noteOn(); c.noteOn(); std::vector<float> x(4096), y(4096), z(4096); a.render(x.data(), x.size()); b.render(y.data(), y.size()); c.render(z.data(), z.size());
  double adDiff = 0.0, dDiff = 0.0; for (size_t i = 0; i < x.size(); ++i) { adDiff += std::fabs(x[i] - y[i]); dDiff += std::fabs(x[i] - z[i]); }
  CHECK(adDiff > .01); CHECK(dDiff > .01);
}

TEST_CASE("Sintered CNI round trip preserves its controls") {
  Project saved, loaded; fillFXNames(); projectInit(&saved); projectInit(&loaded);
  Instrument* instrument = &saved.instruments[0]; getInstrumentFunctions(InstrumentType::Sintered).init(instrument); std::strcpy(instrument->name, "Sintered Melt");
  InstrumentSintered* s = &instrument->chip.sintered; s->model = SinteredModel::melt; s->decay = 91; s->mod = 144; s->a = 31; s->b = 202; s->motion = 20; s->c = 78;
  const char* path = "test_sintered.cni"; REQUIRE(instrumentSave(&saved, path, 0) == 0); REQUIRE(instrumentLoad(&loaded, path, 0) == 0); std::remove(path);
  const InstrumentSintered& restored = loaded.instruments[0].chip.sintered; CHECK(loaded.instruments[0].type == InstrumentType::Sintered); CHECK(restored.model == SinteredModel::melt);
  CHECK(restored.decay == 91); CHECK(restored.mod == 144); CHECK(restored.a == 31); CHECK(restored.b == 202); CHECK(restored.motion == 20); CHECK(restored.c == 78);
}
