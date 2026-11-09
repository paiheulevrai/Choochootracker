#include "doctest.h"
#include "project.h"
#include "chipnomad_lib.h"
#include "playback.h"
#include "pitch_table_utils.h"
#include "project_utils.h"
#include "synth/drum_synth_voice.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
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

TEST_CASE("DrumSynth render is stable across callback block sizes") {
  for (int engine = 0; engine < (int)DrumSynthEngine::totalCount; ++engine) {
    InstrumentDrumSynth instrument = {};
    instrument.engine = (DrumSynthEngine)engine; instrument.decay = 220;
    instrument.tone = 180; instrument.sweep = 170; instrument.noise = 150;
    instrument.fm = 140; instrument.drive = 190;
    DrumSynthVoice whole, chunked; whole.init(48000); chunked.init(48000);
    whole.configure(&instrument, 6000, 1, 20000, 0);
    chunked.configure(&instrument, 6000, 1, 20000, 0);
    whole.noteOn(); chunked.noteOn();
    std::vector<float> a(4096), b(4096);
    whole.render(a.data(), a.size());
    for (size_t offset = 0; offset < b.size(); offset += 128)
      chunked.render(b.data() + offset, 128);
    float maximumDifference = 0.0f;
    for (size_t i = 0; i < a.size(); ++i)
      maximumDifference = std::max(maximumDifference, std::fabs(a[i] - b[i]));
    CAPTURE(engine); CAPTURE(maximumDifference);
    CHECK(maximumDifference < 0.001f);
  }
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
  d->filterColor = 209;
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
  CHECK(restored.filterColor == 209);
}

TEST_CASE("DrumSynth clap Slaps maps the full control range to one through eight hands") {
  CHECK(drumSynthClapSlaps(0) == 1);
  CHECK(drumSynthClapSlaps(128) == 4);
  CHECK(drumSynthClapSlaps(255) == 8);
  int previous = 1;
  for (int control = 0; control <= 255; ++control) {
    const int slaps = drumSynthClapSlaps(control);
    CHECK(slaps >= previous);
    CHECK(slaps <= previous + 1);
    previous = slaps;
  }
  for (int slaps = 1; slaps <= 8; ++slaps)
    CHECK(drumSynthClapSlaps(drumSynthClapSlapsControl(slaps)) == slaps);

  Instrument instrument = {};
  instrument.type = InstrumentType::DrumSynth;
  instrument.chip.drumSynth.engine = DrumSynthEngine::clap;
  CHECK(std::strcmp(instrumentModDestinationNameForInstrument(&instrument, 5), "Spacing") == 0);
  CHECK(std::strcmp(instrumentModDestinationNameForInstrument(&instrument, 6), "Width") == 0);
  CHECK(std::strcmp(instrumentCCDestinationName(&instrument, 7), "Slaps") == 0);
  instrument.chip.drumSynth.engine = DrumSynthEngine::snare;
  CHECK(std::strcmp(instrumentModDestinationNameForInstrument(&instrument, 7), "FM") == 0);
  instrument.type = InstrumentType::DigiSophie;
  CHECK(std::strcmp(instrumentModDestinationNameForInstrument(&instrument, 7), "Feedback") == 0);
}

TEST_CASE("DrumSynth clap retriggers every hand even at minimum Decay") {
  for (int rate : {44100, 48000, 96000}) {
    for (int slaps = 1; slaps <= 8; ++slaps) {
      CAPTURE(rate); CAPTURE(slaps);
      InstrumentDrumSynth instrument = {};
      instrument.engine = DrumSynthEngine::clap;
      instrument.tone = instrument.noise = 128;
      instrument.sweep = 255;
      instrument.fm = drumSynthClapSlapsControl(slaps);
      DrumSynthVoice voice;
      voice.init(rate);
      voice.configure(&instrument, 6000, 1, 20000, 0);
      voice.noteOn();
      const int spacing = (int)(rate * 0.030f + 0.5f);
      std::vector<float> gap(spacing - 1);
      for (int hand = 0; hand < slaps; ++hand) {
        CHECK(voice.active());
        float sample;
        voice.render(&sample, 1);
        CHECK(std::isfinite(sample));
        if (hand == slaps - 1) {
          CHECK(voice.envelopeLevel() > 0.99f);
          CHECK(voice.envelopeLevel() < 1.001f);
        } else {
          CHECK(voice.envelopeLevel() > 0.64f);
          CHECK(voice.envelopeLevel() < 0.66f);
        }
        voice.render(gap.data(), gap.size());
      }
      std::vector<float> rest(rate / 10);
      voice.render(rest.data(), rest.size());
      CHECK_FALSE(voice.active());
      for (float sample : rest) CHECK(sample == 0.0f);
    }
  }
}

TEST_CASE("DrumSynth clap Decay affects only the tail starting at the last hand") {
  InstrumentDrumSynth instrument = {};
  instrument.engine = DrumSynthEngine::clap;
  instrument.tone = instrument.noise = 128;
  instrument.sweep = 255; instrument.fm = 128;
  DrumSynthVoice shortTail, longTail;
  shortTail.init(48000); longTail.init(48000);
  shortTail.configure(&instrument, 6000, 1, 20000, 0);
  instrument.decay = 255;
  longTail.configure(&instrument, 6000, 1, 20000, 0);
  shortTail.noteOn(); longTail.noteOn();
  std::vector<float> a(24000), b(24000);
  shortTail.render(a.data(), a.size()); longTail.render(b.data(), b.size());
  for (size_t i = 0; i < 3 * 1440; ++i) CHECK(a[i] == b[i]);
  CHECK_FALSE(shortTail.active());
  CHECK(longTail.active());
  double tailEnergy = 0.0;
  for (size_t i = 6000; i < b.size(); ++i) tailEnergy += b[i] * b[i];
  CHECK(tailEnergy > 1.0);
}

TEST_CASE("DrumSynth clap is noise only and Width keeps a usable level") {
  InstrumentDrumSynth instrument = {};
  instrument.engine = DrumSynthEngine::clap;
  instrument.decay = 200; instrument.tone = 128; instrument.fm = 0;
  double minimumRms = 1.0, maximumRms = 0.0, narrowBrightness = 0.0, wideBrightness = 0.0;
  for (uint8_t width : {0, 128, 255}) {
    CAPTURE(width);
    instrument.noise = width;
    DrumSynthVoice lowPitch, highPitch;
    lowPitch.init(48000); highPitch.init(48000);
    lowPitch.configure(&instrument, 2400, 1, 20000, 0);
    highPitch.configure(&instrument, 9600, 1, 20000, 0);
    lowPitch.noteOn(); highPitch.noteOn();
    std::vector<float> a(4096), b(4096);
    lowPitch.render(a.data(), a.size()); highPitch.render(b.data(), b.size());
    double energy = 0.0, differenceEnergy = 0.0;
    for (size_t i = 1; i < a.size(); ++i) {
      CHECK(a[i] == b[i]);
      CHECK(std::isfinite(a[i]));
      CHECK(std::fabs(a[i]) < 1.36f);
      energy += a[i] * a[i];
      differenceEnergy += (a[i] - a[i - 1]) * (a[i] - a[i - 1]);
    }
    const double rms = std::sqrt(energy / (a.size() - 1));
    CHECK(rms > 0.06);
    minimumRms = std::min(minimumRms, rms);
    maximumRms = std::max(maximumRms, rms);
    if (width == 0) narrowBrightness = differenceEnergy / energy;
    if (width == 255) wideBrightness = differenceEnergy / energy;
  }
  CHECK(maximumRms / minimumRms < 1.6);
  CHECK(wideBrightness > narrowBrightness * 1.5);
}

TEST_CASE("DrumSynth clap captures Slaps per trigger and refreshes parameters after a model change") {
  InstrumentDrumSynth instrument = {};
  instrument.engine = DrumSynthEngine::clap;
  instrument.decay = 80; instrument.tone = instrument.noise = instrument.fm = 128;
  instrument.sweep = 255;
  DrumSynthVoice voice, reference;
  voice.init(48000); reference.init(48000);
  voice.configure(&instrument, 6000, 1, 20000, 0);
  reference.configure(&instrument, 6000, 1, 20000, 0);
  voice.noteOn(); reference.noteOn();
  float a, b;
  voice.render(&a, 1); reference.render(&b, 1);
  instrument.fm = 0;
  voice.configure(&instrument, 6000, 1, 20000, 0);
  std::vector<float> changed(10000), original(10000);
  voice.render(changed.data(), changed.size()); reference.render(original.data(), original.size());
  CHECK(changed == original);
  voice.noteOn(); voice.render(&a, 1);
  CHECK(voice.envelopeLevel() == doctest::Approx(1.0f));

  voice.kill();
  instrument.engine = DrumSynthEngine::kick;
  instrument.tone = 255; instrument.noise = 0; instrument.decay = 200;
  voice.configure(&instrument, 6000, 1, 20000, 0);
  instrument.engine = DrumSynthEngine::clap;
  voice.configure(&instrument, 6000, 1, 20000, 0);
  voice.noteOn();
  voice.render(changed.data(), changed.size());
  CHECK(voice.active());
  CHECK(voice.envelopeLevel() > 0.01f);
}

TEST_CASE("DrumSynth clap Layer reuses the drum pool with four and eight hands") {
  for (uint8_t control : {128, 255}) {
    std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)> state{chipnomadCreate(), chipnomadDestroy};
    REQUIRE(state != nullptr);
    projectInitAY(&state->project);
    auto& p = state->project;
    p.tracksCount = 8; p.chipsCount = 1; p.linearPitch = 1;
    calculateLinearPitchTable12TET(&p);
    getInstrumentFunctions(InstrumentType::DrumSynth).init(&p.instruments[0]);
    auto& d = p.instruments[0].chip.drumSynth;
    d.engine = DrumSynthEngine::clap;
    d.tone = d.noise = d.sweep = 128; d.decay = 200; d.fm = control;
    d.filterEnabled = 0;
    getInstrumentFunctions(InstrumentType::Layer).init(&p.instruments[1]);
    for (auto& entry : p.instruments[1].chip.layer.entries) {
      entry.instrument = 0; entry.volume = 255;
    }
    chipnomadInitChips(state.get(), 48000, nullptr);
    REQUIRE(chipnomadReserveRenderBuffers(state.get(), 512) == 0);
    std::vector<DrumSynthVoice*> prepared;
    for (auto& owner : state->drumSynthVoices)
      for (auto* voice : owner) prepared.push_back(voice);
    float output[1024];
    std::vector<double> timings;
    double energy = 0.0;
    for (int round = 0; round < 5; ++round) {
      const auto start = std::chrono::steady_clock::now();
      for (int block = 0; block < 100; ++block) {
        if (block % 24 == 0)
          for (int track = 0; track < 8; ++track)
            REQUIRE(chipnomadQueuePlaybackPreviewNote(state.get(), track, 45, 1));
        REQUIRE(chipnomadRender(state.get(), output, 512) == 512);
      }
      timings.push_back(std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count());
      for (float sample : output) {
        CHECK(std::isfinite(sample));
        energy += sample * sample;
      }
    }
    std::sort(timings.begin(), timings.end());
    std::printf("Bogie Clap %d hands, 8 tracks x 3 Layer children: median %.2f ms / 51200 frames; voice bytes=%zu\n",
      drumSynthClapSlaps(control), timings[2], sizeof(DrumSynthVoice));
    CHECK(energy > 1.0);
    size_t index = 0;
    for (auto& owner : state->drumSynthVoices)
      for (auto* voice : owner) CHECK(voice == prepared[index++]);
    for (int track = 0; track < 8; ++track)
      for (int lane = 0; lane < 3; ++lane)
        CHECK(state->playbackState.layerTracks[track][lane].note.instrument == 0);
  }
}
