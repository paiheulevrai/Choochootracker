#include "doctest.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "synth/drum_synth_voice.h"
#include "project_utils.h"
#include "synth/master_effects.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

namespace {
Instrument sophieInstrument(int model = 0) {
  Instrument i{};
  getInstrumentFunctions(InstrumentType::DigiSophie).init(&i);
  i.chip.drumSynth.engine = (DrumSynthEngine)model;
  i.chip.drumSynth.filterEnabled = 0;
  return i;
}
double energy(const std::vector<float>& samples) {
  double result = 0;
  for (float s : samples) result += s * s;
  return result;
}
}

TEST_CASE("DigiSophie four models render distinctly at native and device rates") {
  for (float rate : {32000.0f, 44100.0f, 48000.0f, 96000.0f}) {
    std::vector<float> previous;
    for (int model = 0; model < 4; ++model) {
      auto i = sophieInstrument(model);
      DrumSynthVoice v; v.init(rate);
      v.configure(&i.chip.drumSynth, 4800, 1, 20000, 0, true);
      v.noteOn();
      std::vector<float> output(4096);
      v.render(output.data(), output.size());
      CAPTURE(rate); CAPTURE(model);
      CHECK(energy(output) > 0.001);
      for (float s : output) { REQUIRE(std::isfinite(s)); CHECK(std::fabs(s) <= 1.1f); }
      if (!previous.empty()) CHECK(output != previous);
      previous = output;
      v.kill(); v.render(output.data(), output.size());
      CHECK(energy(output) == 0);
    }
  }
}

TEST_CASE("DigiSophie preserves core waveform and centered sweep before tracker envelope") {
  auto i = sophieInstrument();
  i.chip.drumSynth.attack = 0;
  DrumSynthVoice v; v.init(48000);
  v.configure(&i.chip.drumSynth, 6900, 1, 20000, 0, true); v.noteOn();
  ds_voice reference{}; ds_voice_init(&reference);
  ds_params params{(uint16_t)(440.0f * 65536 / 24000), 0, 64, 64, 0, 32, 127, 0};
  int32_t raw[DS_BLOCK_SIZE];
  ds_voice_render(&reference, &params, 1, raw, DS_BLOCK_SIZE);
  float output[DS_BLOCK_SIZE];
  v.render(output, DS_BLOCK_SIZE);
  const float d = i.chip.drumSynth.decay / 255.0f;
  const float coefficient = std::exp(-6.9f / (48000 * (0.018f + d * d * 4)));
  float envelope = 1;
  for (int n = 0; n < DS_BLOCK_SIZE; ++n) {
    CHECK(output[n] == doctest::Approx(raw[n] / 2147483648.0f * envelope).epsilon(0.00001));
    envelope *= coefficient;
  }
}

TEST_CASE("DigiSophie rendering is invariant to odd callback sizes and live reconfiguration") {
  for (float rate : {44100.0f, 48000.0f}) for (int model = 0; model < 4; ++model) {
    auto i = sophieInstrument(model);
    DrumSynthVoice a, b; a.init(rate); b.init(rate);
    a.configure(&i.chip.drumSynth, 4800, 1, 20000, 0, true);
    b.configure(&i.chip.drumSynth, 4800, 1, 20000, 0, true);
    a.noteOn(); b.noteOn();
    std::vector<float> whole(4096), split(4096);
    a.render(whole.data(), whole.size());
    for (size_t offset = 0; offset < split.size();) {
      const size_t frames = std::min<size_t>(37, split.size() - offset);
      b.configure(&i.chip.drumSynth, 4800, 1, 20000, 0, true);
      b.render(split.data() + offset, frames);
      offset += frames;
    }
    CHECK(whole == split);
    b.noteOff();
    std::vector<float> released((size_t)rate);
    b.render(released.data(), released.size());
    CHECK_FALSE(b.active());
  }
}

TEST_CASE("DigiSophie controls independently change every model") {
  for (int model = 0; model < 4; ++model) for (int parameter = 0; parameter < 6; ++parameter) {
    auto low = sophieInstrument(model), high = low;
    auto& l = low.chip.drumSynth; auto& h = high.chip.drumSynth;
    uint8_t* controlsLow[] = {&l.decay, &l.tone, &l.sweep, &l.noise, &l.fm, &l.drive};
    uint8_t* controlsHigh[] = {&h.decay, &h.tone, &h.sweep, &h.noise, &h.fm, &h.drive};
    *controlsLow[parameter] = 0; *controlsHigh[parameter] = 255;
    DrumSynthVoice a, b; a.init(48000); b.init(48000);
    a.configure(&l, 4800, 1, 20000, 0, true); b.configure(&h, 4800, 1, 20000, 0, true);
    a.noteOn(); b.noteOn();
    std::vector<float> x(4096), y(4096);
    a.render(x.data(), x.size()); b.render(y.data(), y.size());
    CAPTURE(model); CAPTURE(parameter);
    double difference = 0;
    for (size_t n = 0; n < x.size(); ++n) difference += std::fabs(x[n] - y[n]);
    CHECK(difference > 0.01);
  }
}

TEST_CASE("DigiSophie and Echoverb round-trip without changing legacy identifiers") {
  auto p = std::make_unique<Project>(), loaded = std::make_unique<Project>();
  projectInitAY(p.get()); projectInit(loaded.get()); fillFXNames();
  p->instruments[0] = sophieInstrument(3);
  auto& d = p->instruments[0].chip.drumSynth;
  d.tone = 203; d.sweep = 17; d.noise = 99; d.fm = 201; d.drive = 155;
  d.filterEnabled = 1; d.filterCutoffHz = 2345;
  p->instruments[0].modulation[0] = {ModulationType::Audio, 7, -64, 1, 16, 80, 64, 0};
  p->delayAlgorithm = 2; p->echoDiffuse = 177; p->echoAge = 213; p->echoHighpassCutoffHz = 345;
  const char* file = "build/tests/digisophie_echoverb.cct";
  REQUIRE(projectSave(p.get(), file) == 0);
  REQUIRE(projectLoad(loaded.get(), file) == 0);
  std::remove(file);
  CHECK(loaded->instruments[0].type == InstrumentType::DigiSophie);
  const auto& r = loaded->instruments[0].chip.drumSynth;
  CHECK((int)r.engine == 3); CHECK(r.tone == 203); CHECK(r.sweep == 17);
  CHECK(r.noise == 99); CHECK(r.fm == 201); CHECK(r.drive == 155); CHECK(r.filterCutoffHz == 2345);
  CHECK(loaded->instruments[0].modulation[0].type == ModulationType::Audio);
  CHECK(loaded->instruments[0].modulation[0].destination == 7);
  CHECK(loaded->delayAlgorithm == 2); CHECK(loaded->echoDiffuse == 177);
  CHECK(loaded->echoAge == 213); CHECK(loaded->echoHighpassCutoffHz == 345);
  CHECK((int)InstrumentType::DrumSynth == 11); CHECK((int)InstrumentType::Layer == 31);
  CHECK((int)InstrumentType::DigiSophie == 32);
  projectFree(p.get()); projectFree(loaded.get());
}

TEST_CASE("DigiSophie Layer shares the prepared drum pool and preserves release ownership") {
  std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)> state{chipnomadCreate(), chipnomadDestroy};
  projectInitAY(&state->project);
  auto& p = state->project;
  p.tracksCount = 8; p.chipsCount = 1; p.linearPitch = 1;
  calculateLinearPitchTable12TET(&p);
  p.instruments[0] = sophieInstrument();
  getInstrumentFunctions(InstrumentType::Layer).init(&p.instruments[1]);
  for (auto& entry : p.instruments[1].chip.layer.entries) { entry.instrument = 0; entry.volume = 255; }
  chipnomadInitChips(state.get(), 48000, nullptr);
  REQUIRE(chipnomadReserveRenderBuffers(state.get(), 512) == 0);
  auto* original = state->drumSynthVoices[0][0];
  REQUIRE(original != nullptr);
  REQUIRE(chipnomadCanSwitchInstrumentLive(InstrumentType::DigiSophie));
  for (int t = 0; t < 8; ++t) REQUIRE(chipnomadQueuePlaybackPreviewNote(state.get(), t, 45, 1));
  std::vector<float> output(1024);
  REQUIRE(chipnomadRender(state.get(), output.data(), 512) == 512);
  CHECK(state->drumSynthVoices[0][0] == original);
  CHECK(energy(output) > 0);
  for (int t = 0; t < 8; ++t) for (int lane = 0; lane < 3; ++lane)
    CHECK(state->playbackState.layerTracks[t][lane].note.instrument == 0);
  const auto begin = std::chrono::steady_clock::now();
  for (int n = 0; n < 100; ++n) REQUIRE(chipnomadRender(state.get(), output.data(), 512) == 512);
  std::printf("DigiSophie 8 tracks x 3 Layer children: %.2f ms / 51200 frames; shared voice bytes=%zu\n",
    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count(), sizeof(DrumSynthVoice));
}

TEST_CASE("DigiSophie saved and legacy projects render immediately after reopening") {
  auto p = std::make_unique<Project>();
  projectInitAY(p.get());
  fillFXNames();
  for (int model = 0; model < 4; ++model) {
    p->instruments[model] = sophieInstrument(model);
    auto& amp = static_cast<InstrumentVoicePostSettings&>(p->instruments[model].chip.drumSynth);
    amp.attack = 3; amp.decay = 17; amp.sustain = 210; amp.release = 28; amp.envelopeShape = 0x91;
  }
  const char* file = "build/tests/digisophie_reopen.cct";
  REQUIRE(projectSave(p.get(), file) == 0);
  std::ifstream saved(file);
  std::ostringstream legacy;
  std::string line;
  while (std::getline(saved, line)) {
    if (line.find("- Attack: ") == 0 || line.find("- Decay: ") == 0 ||
        line.find("- Sustain: ") == 0 || line.find("- Release: ") == 0 ||
        line.find("- Envelope shape: ") == 0) continue;
    legacy << line << '\n';
  }
  saved.close();
  for (bool oldFormat : {false, true}) {
    if (oldFormat) {
      std::ofstream output(file);
      output << legacy.str();
      REQUIRE(output.good());
    }
    for (int model = 0; model < 4; ++model) {
      std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)> state{chipnomadCreate(), chipnomadDestroy};
      REQUIRE(projectLoad(&state->project, file) == 0);
      chipnomadInitChips(state.get(), 48000, nullptr);
      REQUIRE(chipnomadReserveRenderBuffers(state.get(), 512) == 0);
      CAPTURE(oldFormat); CAPTURE(model);
      const auto& amp = static_cast<const InstrumentVoicePostSettings&>(
        state->project.instruments[model].chip.drumSynth);
      CHECK(amp.sustain == (oldFormat ? 255 : 210));
      CHECK(amp.attack == (oldFormat ? 0 : 3));
      CHECK(amp.decay == (oldFormat ? 0 : 17));
      CHECK(amp.release == (oldFormat ? 0 : 28));
      CHECK(amp.envelopeShape == (oldFormat ? 0x80 : 0x91));
      REQUIRE(chipnomadQueuePlaybackPreviewNote(state.get(), model, 45, model));
      std::vector<float> samples(1024);
      REQUIRE(chipnomadRender(state.get(), samples.data(), 512) == 512);
      CHECK(energy(samples) > 0.000001);
    }
  }
  std::remove(file);
  projectFree(p.get());
}

TEST_CASE("DigiSophie provided project renders immediately after opening") {
  const char* file = std::getenv("CCT_DIGISOPHIE_PROJECT");
  if (!file) return;
  auto p = std::make_unique<Project>();
  projectInit(p.get());
  fillFXNames();
  REQUIRE(projectLoad(p.get(), file) == 0);
  int tested = 0;
  for (int instrument = 0; instrument < PROJECT_MAX_INSTRUMENTS; ++instrument) {
    if (p->instruments[instrument].type != InstrumentType::DigiSophie) continue;
    CAPTURE(instrument);
    std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)> state{chipnomadCreate(), chipnomadDestroy};
    REQUIRE(projectLoad(&state->project, file) == 0);
    chipnomadInitChips(state.get(), 48000, nullptr);
    REQUIRE(chipnomadReserveRenderBuffers(state.get(), 512) == 0);
    REQUIRE(chipnomadQueuePlaybackPreviewNote(state.get(), 0, 45, instrument));
    std::vector<float> samples(1024);
    REQUIRE(chipnomadRender(state.get(), samples.data(), 512) == 512);
    CHECK(energy(samples) > 0.000001);
    ++tested;
  }
  CHECK(tested > 0);
  projectFree(p.get());
}

TEST_CASE("Echoverb CPU comparison with existing send algorithms") {
  auto p = std::make_unique<Project>(); projectInit(p.get());
  p->echoDiffuse = 255; p->echoAge = 255; p->delayFeedback = 95;
  std::vector<float> bus(1024), output(1024);
  uint32_t random = 123;
  for (float& s : bus) { random = random * 1664525u + 1013904223u; s = ((random >> 16) / 65535.0f - 0.5f) * 0.2f; }
  for (int algorithm = 0; algorithm < 3; ++algorithm) {
    MasterEffects effects; effects.init(48000);
    p->delayAlgorithm = algorithm;
    const auto begin = std::chrono::steady_clock::now();
    for (int n = 0; n < 100; ++n) {
      std::fill(output.begin(), output.end(), 0);
      effects.process(nullptr, bus.data(), output.data(), 512, p.get());
    }

    std::printf("Delay send algorithm %d: %.2f ms / 51200 frames (48 kHz)\n", algorithm,
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count());
    for (float sample : output) CHECK(std::isfinite(sample));
  }
}

TEST_CASE("Echoverb keeps rendering its shared-send tail after the source track is muted") {
  std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)> state{chipnomadCreate(), chipnomadDestroy};
  projectInitAY(&state->project);
  auto& p = state->project;
  p.tracksCount = 1; p.chipsCount = 1; p.linearPitch = 1;
  calculateLinearPitchTable12TET(&p);
  p.instruments[0] = sophieInstrument();
  p.reverbReturn = 0; p.trackReverbSend[0] = 0;
  p.delayAlgorithm = 2; p.delayTicks = 1; p.delayFeedback = 80; p.trackDelaySend[0] = 100;
  chipnomadInitChips(state.get(), 48000, nullptr);
  REQUIRE(chipnomadReserveRenderBuffers(state.get(), 512) == 0);
  REQUIRE(chipnomadQueuePlaybackPreviewNote(state.get(), 0, 45, 0));
  std::vector<float> output(1024);
  for (int n = 0; n < 4; ++n) REQUIRE(chipnomadRender(state.get(), output.data(), 512) == 512);
  CHECK(state->masterEffects->delayTailActive());
  uint8_t enabled[PROJECT_MAX_TRACKS] = {};
  REQUIRE(chipnomadQueueTrackEnabled(state.get(), enabled));
  for (int n = 0; n < 4; ++n) REQUIRE(chipnomadRender(state.get(), output.data(), 512) == 512);
  CHECK_FALSE(state->playbackState.trackEnabled[0]);
  CHECK(energy(output) > 0.000001);
  CHECK(state->masterEffects->delayTailActive());
}
