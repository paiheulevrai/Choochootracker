#include "doctest.h"
#include "project.h"
#include "synth/drum_synth_voice.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

TEST_CASE("DrumSynth renders every engine and decays without note off") {
  DrumSynthVoice voice;
  InstrumentDrumSynth instrument = {};
  instrument.decay = 100; instrument.tone = 128; instrument.sweep = 128;
  instrument.noise = 128; instrument.fm = 128; instrument.drive = 32;
  instrument.filterEnabled = 1; instrument.filterCharacter = 1;
  instrument.filterMode = 0; instrument.filterCutoffHz = 20000;
  voice.init(48000.0f);
  std::vector<float> output(48000 * 3);
  for (int engine = 0; engine < (int)DrumSynthEngine::totalCount; ++engine) {
    instrument.engine = (DrumSynthEngine)engine;
    voice.configure(&instrument, 6000.0f, 1.0f, 20000, 0);
    voice.noteOn(); voice.render(output.data(), output.size());
    double energy = 0.0;
    for (float sample : output) { CHECK(std::isfinite(sample)); energy += std::fabs(sample); }
    CAPTURE(engine); CHECK(energy > 0.01); CHECK_FALSE(voice.active());
  }
}

TEST_CASE("DrumSynth retriggers and uses the shared filter") {
  DrumSynthVoice voice;
  InstrumentDrumSynth instrument = {};
  instrument.engine = DrumSynthEngine::cowbell; instrument.decay = 100;
  instrument.tone = 128; instrument.sweep = 128; instrument.noise = 32;
  instrument.fm = 128; instrument.drive = 40; instrument.filterEnabled = 1;
  instrument.filterCharacter = 2; instrument.filterMode = 0; instrument.filterSlope24dB = 1;
  voice.init(48000.0f); voice.configure(&instrument, 6000.0f, 1.0f, 800, 180); voice.noteOn();
  std::vector<float> first(1024), second(1024); voice.render(first.data(), first.size());
  voice.noteOn(); voice.render(second.data(), second.size());
  double energy = 0.0; for (float sample : second) { CHECK(std::isfinite(sample)); energy += std::fabs(sample); }
  CHECK(energy > 0.01);
}

TEST_CASE("DrumSynth cowbell FM changes the generated waveform") {
  InstrumentDrumSynth instrument = {};
  instrument.engine = DrumSynthEngine::cowbell; instrument.decay = 100; instrument.tone = 128;
  instrument.sweep = 128; instrument.noise = 0; instrument.drive = 0;
  DrumSynthVoice dry, modulated;
  dry.init(48000.0f); modulated.init(48000.0f);
  instrument.fm = 0; dry.configure(&instrument, 6000.0f, 1.0f, 20000, 0); dry.noteOn();
  instrument.fm = 255; modulated.configure(&instrument, 6000.0f, 1.0f, 20000, 0); modulated.noteOn();
  std::vector<float> plain(2048), fm(2048);
  dry.render(plain.data(), plain.size()); modulated.render(fm.data(), fm.size());
  double difference = 0.0;
  for (size_t i = 0; i < plain.size(); ++i) difference += std::fabs(plain[i] - fm[i]);
  CHECK(difference > 1.0);
}

TEST_CASE("DrumSynth every macro changes every model") {
  for (int engine = 0; engine < (int)DrumSynthEngine::totalCount; ++engine) {
    for (int macro = 0; macro < 6; ++macro) {
      InstrumentDrumSynth low = {};
      low.engine = (DrumSynthEngine)engine; low.decay = low.tone = low.sweep = 128;
      low.noise = low.fm = low.drive = 128;
      uint8_t* values[] = {&low.decay, &low.tone, &low.sweep, &low.noise, &low.fm, &low.drive};
      InstrumentDrumSynth high = low; *values[macro] = 0;
      uint8_t* highValues[] = {&high.decay, &high.tone, &high.sweep, &high.noise, &high.fm, &high.drive};
      *highValues[macro] = 255;
      DrumSynthVoice a, b; a.init(48000.0f); b.init(48000.0f);
      a.configure(&low, 6000.0f, 1.0f, 20000, 0); b.configure(&high, 6000.0f, 1.0f, 20000, 0);
      a.noteOn(); b.noteOn(); std::vector<float> left(4096), right(4096);
      a.render(left.data(), left.size()); b.render(right.data(), right.size());
      double difference = 0.0;
      for (size_t i = 0; i < left.size(); ++i) difference += std::fabs(left[i] - right[i]);
      CAPTURE(engine); CAPTURE(macro); CHECK(difference > .01);
    }
  }
}

TEST_CASE("DrumSynth CNI round trip preserves engine macros and filter") {
  Project saved, loaded;
  fillFXNames();
  projectInit(&saved); projectInit(&loaded);
  Instrument* instrument = &saved.instruments[0];
  getInstrumentFunctions(InstrumentType::DrumSynth).init(instrument);
  std::strcpy(instrument->name, "Copper Cowbell");
  InstrumentDrumSynth* d = &instrument->chip.drumSynth;
  d->engine = DrumSynthEngine::cowbell; d->decay = 91; d->tone = 44; d->sweep = 201;
  d->noise = 12; d->fm = 188; d->drive = 67; d->filterEnabled = 1;
  d->filterMode = 2; d->filterSlope24dB = 1; d->filterCutoffHz = 4321; d->filterResonance = 111;
  const char* path = "test_drum_synth.cni";
  REQUIRE(instrumentSave(&saved, path, 0) == 0);
  REQUIRE(instrumentLoad(&loaded, path, 0) == 0);
  std::remove(path);
  const InstrumentDrumSynth& restored = loaded.instruments[0].chip.drumSynth;
  CHECK(loaded.instruments[0].type == InstrumentType::DrumSynth);
  CHECK(std::strcmp(loaded.instruments[0].name, "Copper Cowbell") == 0);
  CHECK(restored.engine == DrumSynthEngine::cowbell);
  CHECK(restored.decay == 91); CHECK(restored.tone == 44); CHECK(restored.sweep == 201);
  CHECK(restored.noise == 12); CHECK(restored.fm == 188); CHECK(restored.drive == 67);
  CHECK(restored.filterEnabled == 1); CHECK(restored.filterMode == 2);
  CHECK(restored.filterSlope24dB == 1); CHECK(restored.filterCutoffHz == 4321);
  CHECK(restored.filterResonance == 111);
}
