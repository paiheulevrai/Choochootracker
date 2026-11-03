#include "doctest.h"
#include "project.h"
#include "chipnomad_lib.h"
#include "synth/sintered_voice.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
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

TEST_CASE("Sintered motion extremes strongly animate every model") {
  for (int model = 0; model < (int)SinteredModel::totalCount; ++model) {
    InstrumentSintered still = sinteredFixture((SinteredModel)model), attack = still, decay = still;
    attack.motion = 0; decay.motion = 255;
    SinteredVoice a, b, c; a.init(48000); b.init(48000); c.init(48000);
    a.configure(&still, 6000, 1, 20000, 0); b.configure(&attack, 6000, 1, 20000, 0); c.configure(&decay, 6000, 1, 20000, 0);
    a.noteOn(); b.noteOn(); c.noteOn(); std::vector<float> x(4096), y(4096), z(4096);
    a.render(x.data(), x.size()); b.render(y.data(), y.size()); c.render(z.data(), z.size());
    double attackDiff = 0.0, decayDiff = 0.0;
    for (size_t i = 0; i < x.size(); ++i) { attackDiff += std::fabs(x[i] - y[i]); decayDiff += std::fabs(x[i] - z[i]); }
    CAPTURE(model); CHECK(attackDiff > 1.0); CHECK(decayDiff > 1.0);
  }
}

TEST_CASE("Sintered block-rate constants are stable across callback block sizes") {
  for (int model = 0; model < (int)SinteredModel::totalCount; ++model) {
    InstrumentSintered s = sinteredFixture((SinteredModel)model);
    s.motion = 220;
    SinteredVoice whole, chunked; whole.init(48000); chunked.init(48000);
    whole.configure(&s, 6000, 1, 20000, 0); chunked.configure(&s, 6000, 1, 20000, 0);
    whole.noteOn(); chunked.noteOn();
    std::vector<float> a(4096), b(4096);
    whole.render(a.data(), a.size());
    for (size_t offset = 0; offset < b.size(); offset += 128)
      chunked.render(b.data() + offset, 128);
    float maximumDifference = 0.0f;
    for (size_t i = 0; i < a.size(); ++i)
      maximumDifference = std::max(maximumDifference, std::fabs(a[i] - b[i]));
    CAPTURE(model); CAPTURE(maximumDifference);
    CHECK(maximumDifference < 0.001f);
  }
}

TEST_CASE("Sintered CNI round trip preserves its controls") {
  Project saved, loaded; fillFXNames(); projectInit(&saved); projectInit(&loaded);
  Instrument* instrument = &saved.instruments[0]; getInstrumentFunctions(InstrumentType::Sintered).init(instrument); std::strcpy(instrument->name, "Sintered Melt");
  InstrumentSintered* s = &instrument->chip.sintered; s->model = SinteredModel::squall; s->decay = 91; s->mod = 144; s->a = 31; s->b = 202; s->motion = 20; s->c = 78; s->gate = 1;
  const char* path = "test_sintered.cni"; REQUIRE(instrumentSave(&saved, path, 0) == 0); REQUIRE(instrumentLoad(&loaded, path, 0) == 0); std::remove(path);
  const InstrumentSintered& restored = loaded.instruments[0].chip.sintered; CHECK(loaded.instruments[0].type == InstrumentType::Sintered); CHECK(restored.model == SinteredModel::squall);
  CHECK(restored.decay == 91); CHECK(restored.mod == 144); CHECK(restored.a == 31); CHECK(restored.b == 202); CHECK(restored.motion == 20); CHECK(restored.c == 78);
  CHECK(restored.gate == 1);
}

TEST_CASE("Sintered gate sustains all models and releases with decay") {
  for (float rate : {32000.0f, 48000.0f}) {
    for (int model = 0; model < (int)SinteredModel::totalCount; ++model) {
      CAPTURE(rate); CAPTURE(model);
      InstrumentSintered s = sinteredFixture((SinteredModel)model);
      s.gate = 1;
      SinteredVoice voice; voice.init(rate);
      voice.configure(&s, 6000, 1, 20000, 0); voice.noteOn();
      std::vector<float> held((size_t)(rate * 2));
      voice.render(held.data(), held.size());
      CHECK(voice.active());
      CHECK(voice.envelopeLevel() == 1.0f);
      double energy = 0;
      for (size_t i = held.size() - 1024; i < held.size(); ++i) {
        REQUIRE(std::isfinite(held[i]));
        CHECK(std::fabs(held[i]) <= 1.01f);
        energy += held[i] * held[i];
      }
      CHECK(energy > .001);
      voice.noteOff();
      std::vector<float> release(128);
      voice.render(release.data(), release.size());
      CHECK(voice.active());
      CHECK(voice.envelopeLevel() > 0);
      CHECK(voice.envelopeLevel() < 1);
      voice.noteOff(); // Repeated releases must not restart the tail.
      voice.render(held.data(), held.size());
      CHECK_FALSE(voice.active());
      CHECK(held.back() == 0);
      voice.noteOn();
      voice.render(release.data(), release.size());
      CHECK(voice.envelopeLevel() == 1);
      voice.kill();
      CHECK_FALSE(voice.active());
    }
  }
}

TEST_CASE("Sintered default one shots ignore note off") {
  for (int model = 0; model < (int)SinteredModel::totalCount; ++model) {
    InstrumentSintered s = sinteredFixture((SinteredModel)model);
    SinteredVoice a, b; a.init(48000); b.init(48000);
    a.configure(&s, 6000, 1, 20000, 0); b.configure(&s, 6000, 1, 20000, 0);
    a.noteOn(); b.noteOn();
    std::vector<float> x(256), y(256);
    a.render(x.data(), x.size()); b.render(y.data(), y.size());
    a.noteOff();
    x.resize(48000); y.resize(x.size());
    a.render(x.data(), x.size()); b.render(y.data(), y.size());
    CHECK(x == y);
    CHECK_FALSE(a.active());
  }
}

TEST_CASE("Sintered legacy instruments default to one shot after loading") {
  Project saved, loaded; projectInit(&saved); projectInit(&loaded);
  auto& instrument = saved.instruments[0];
  getInstrumentFunctions(InstrumentType::Sintered).init(&instrument);
  CHECK(instrument.chip.sintered.gate == 0);
  const char* current = "test_sintered_current.cni";
  const char* legacy = "test_sintered_legacy.cni";
  REQUIRE(instrumentSave(&saved, current, 0) == 0);
  {
    std::ifstream input(current);
    std::ofstream output(legacy);
    REQUIRE(input.good()); REQUIRE(output.good());
    std::string line;
    while (std::getline(input, line))
      if (line.rfind("- Gate: ", 0) != 0) output << line << '\n';
  }
  getInstrumentFunctions(InstrumentType::Sintered).init(&loaded.instruments[0]);
  loaded.instruments[0].chip.sintered.gate = 1;
  REQUIRE(instrumentLoad(&loaded, legacy, 0) == 0);
  CHECK(loaded.instruments[0].chip.sintered.gate == 0);
  std::remove(current); std::remove(legacy);
}

TEST_CASE("Sintered sequencer note off releases sustained chord voices") {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(
    chipnomadCreate(), chipnomadDestroy);
  REQUIRE(projectLoad(&state->project, "packaging/common/projects/gm-midi-demo.cct") == 0);
  auto& project = state->project;
  project.tracksCount = 1; project.tickRate = 50;
  for (auto& groove : project.grooves)
    for (auto& speed : groove.speed) speed = 50;
  project.song[0][0] = 0;
  chainClear(&project.chains[0]);
  project.chains[0].rows[0].phrase = 0;
  phraseClear(&project.phrases[0]);
  getInstrumentFunctions(InstrumentType::Sintered).init(&project.instruments[0]);
  project.instruments[0].chip.sintered.gate = 1;
  auto& row = project.phrases[0].rows[0];
  row.note = 36; row.instrument = 0; row.volume = PHRASE_VOLUME_MAX;
  row.fx[0][0] = fxCRD; row.fx[0][1] = 7;
  project.phrases[0].rows[2].note = NOTE_OFF;
  chipnomadInitChips(state.get(), 48000, nullptr);
  chipnomadReserveRenderBuffers(state.get(), 480);
  REQUIRE(chipnomadQueueProjectRefresh(state.get()));
  REQUIRE(chipnomadQueuePlaybackStartSong(state.get(), 0, 0, 1));
  std::vector<float> audio(960);
  for (int block = 0; block < 150; ++block)
    REQUIRE(chipnomadRender(state.get(), audio.data(), 480) == 480);
  REQUIRE(state->playbackState.tracks[0].chordVoiceCount == 4);
  for (int slot = 0; slot < 4; ++slot) {
    REQUIRE(state->sinteredVoices[0][slot]);
    CHECK(state->sinteredVoices[0][slot]->active());
    CHECK(state->sinteredVoices[0][slot]->envelopeLevel() == 1);
  }
  for (int block = 0; block < 100; ++block)
    REQUIRE(chipnomadRender(state.get(), audio.data(), 480) == 480);
  for (int slot = 0; slot < 4; ++slot)
    CHECK_FALSE(state->sinteredVoices[0][slot]->active());
}
