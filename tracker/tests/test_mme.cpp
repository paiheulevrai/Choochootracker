#include "doctest.h"
#include "project.h"
#include "synth/mme_voice.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static InstrumentMME mmeInstrument(MMEModel model) {
  InstrumentMME m = {};
  m.model = model; m.waves = 96; m.interval = 128; m.amount = 128; m.flow = 128;
  m.feedback = 64; m.shaper = 32; m.filterEnabled = 1; m.filterCharacter = 1;
  m.filterMode = 0; m.filterCutoffHz = 20000; m.sustain = 255; return m;
}

TEST_CASE("MME renders every model safely") {
  std::vector<float> output(4096);
  for (int model = 0; model < (int)MMEModel::totalCount; ++model) {
    MMEVoice voice; InstrumentMME m = mmeInstrument((MMEModel)model);
    voice.init(48000.0f); voice.configure(&m, 6000.0f, 1.0f, 20000, 0); voice.noteOn(); voice.render(output.data(), output.size());
    double energy = 0.0; for (float sample : output) { CHECK(std::isfinite(sample)); energy += std::fabs(sample); }
    CAPTURE(model); CHECK(energy > .01);
  }
}

TEST_CASE("MME macros and feedback alter the voice without instability") {
  for (int macro = 0; macro < 6; ++macro) {
    InstrumentMME low = mmeInstrument(MMEModel::cross), high = low;
    uint8_t* lo[] = {&low.waves, &low.interval, &low.amount, &low.flow, &low.feedback, &low.shaper};
    uint8_t* hi[] = {&high.waves, &high.interval, &high.amount, &high.flow, &high.feedback, &high.shaper};
    *lo[macro] = 0; *hi[macro] = 255;
    MMEVoice a, b; a.init(48000.0f); b.init(48000.0f);
    a.configure(&low, 6000, 1, 20000, 0); b.configure(&high, 6000, 1, 20000, 0); a.noteOn(); b.noteOn();
    std::vector<float> left(4096), right(4096); a.render(left.data(), left.size()); b.render(right.data(), right.size());
    double difference = 0.0; for (size_t i = 0; i < left.size(); ++i) { CHECK(std::isfinite(right[i])); difference += std::fabs(left[i] - right[i]); }
    CAPTURE(macro); CHECK(difference > .01);
  }
}

TEST_CASE("MME Sync performs an audible master reset") {
  InstrumentMME freeRunning = mmeInstrument(MMEModel::sync), synced = freeRunning;
  freeRunning.amount = 0; synced.amount = 255; freeRunning.interval = synced.interval = 190;
  freeRunning.flow = synced.flow = 0;
  MMEVoice a, b; a.init(48000.0f); b.init(48000.0f);
  a.configure(&freeRunning, 6000, 1, 20000, 0); b.configure(&synced, 6000, 1, 20000, 0); a.noteOn(); b.noteOn();
  std::vector<float> left(8192), right(8192); a.render(left.data(), left.size()); b.render(right.data(), right.size());
  double difference = 0.0; for (size_t i = 0; i < left.size(); ++i) { CHECK(std::isfinite(right[i])); difference += std::fabs(left[i] - right[i]); }
  CHECK(difference > 10.0);
}

TEST_CASE("MME maximum feedback stays animated") {
  for (int model = 0; model < (int)MMEModel::totalCount; ++model) {
    InstrumentMME m = mmeInstrument((MMEModel)model);
    m.feedback = 255; m.amount = 210; m.flow = 180; m.shaper = 96;
    MMEVoice voice; voice.init(48000.0f); voice.configure(&m, 6000, 1, 20000, 0); voice.noteOn();
    std::vector<float> output(48000); voice.render(output.data(), output.size());
    float lo = 1.0f, hi = -1.0f;
    for (size_t i = output.size() - 4096; i < output.size(); ++i) {
      CHECK(std::isfinite(output[i])); lo = std::fmin(lo, output[i]); hi = std::fmax(hi, output[i]);
    }
    CAPTURE(model); CHECK(hi - lo > .02f);
  }
}

TEST_CASE("MME CNI round trip preserves model macros and filter") {
  Project saved, loaded; fillFXNames(); projectInit(&saved); projectInit(&loaded);
  Instrument* instrument = &saved.instruments[0]; getInstrumentFunctions(InstrumentType::MME).init(instrument);
  std::strcpy(instrument->name, "Warp Yard"); InstrumentMME* m = &instrument->chip.mme;
  m->model = MMEModel::vocode; m->waves = 20; m->interval = 210; m->amount = 40; m->flow = 201; m->feedback = 175; m->shaper = 123;
  m->filterEnabled = 1; m->filterMode = 2; m->filterSlope24dB = 1; m->filterCutoffHz = 4321; m->filterResonance = 111;
  const char* path = "test_mme.cni"; REQUIRE(instrumentSave(&saved, path, 0) == 0); REQUIRE(instrumentLoad(&loaded, path, 0) == 0); std::remove(path);
  const InstrumentMME& restored = loaded.instruments[0].chip.mme;
  CHECK(loaded.instruments[0].type == InstrumentType::MME); CHECK(restored.model == MMEModel::vocode);
  CHECK(restored.waves == 20); CHECK(restored.interval == 210); CHECK(restored.amount == 40); CHECK(restored.flow == 201);
  CHECK(restored.feedback == 175); CHECK(restored.shaper == 123); CHECK(restored.filterCutoffHz == 4321);
}
