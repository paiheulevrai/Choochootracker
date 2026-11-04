#include "doctest.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "project_utils.h"
#include "synth/sample_voice.h"
#include "synth/soundbank_voice.h"
#include "soundbank.h"
#include "soundbank_runtime.h"
#include "midi/midi_router.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>
#include <cstdlib>

namespace {

struct LayerPlayback {
  std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)> state{chipnomadCreate(), chipnomadDestroy};
  float input[1920], output[1920]{};

  explicit LayerPlayback(int tracks = 1) {
    REQUIRE(state);
    auto& p = state->project;
    p.chipsCount = p.tracksCount = tracks;
    p.tickRate = 50;
    p.linearPitch = 1;
    p.chipSetup.ay.clock = 1773400;
    calculateLinearPitchTable12TET(&p);
    getInstrumentFunctions(InstrumentType::Layer).init(&p.instruments[0]);
    getInstrumentFunctions(InstrumentType::AudioIn).init(&p.instruments[1]);
    p.instruments[0].chip.layer.entries[0] = {1, 0x80, 255};
    p.instruments[0].volume = p.instruments[1].volume = 255;
    for (int t = 0; t < tracks; ++t) p.trackVolume[t] = 100;
    for (int i = 0; i < 1920; ++i) input[i] = i & 1 ? -.125f : .25f;
  }

  void init() {
    chipnomadInitChips(state.get(), 48000, nullptr);
    REQUIRE(chipnomadReserveRenderBuffers(state.get(), 960) == 0);
  }

  void note(int track, int instrument) {
    REQUIRE(chipnomadQueuePlaybackPreviewNote(state.get(), track, 48, instrument));
  }

  void render(int frames = 960) {
    REQUIRE(chipnomadRender(state.get(), output, frames, input) == frames);
  }
};

} // namespace

TEST_CASE("Unused Layer children are not copied or visited by the voice engine") {
  LayerPlayback e;
  SUBCASE("no Layer preset") {
    getInstrumentFunctions(InstrumentType::none).init(&e.state->project.instruments[0]);
  }
  SUBCASE("an unused Layer preset is present") {}
  e.init();
  auto& playback = e.state->playbackState;
  playback.layerTracks[0][0].slewCurrent[0] = 1234;
  const auto* children = reinterpret_cast<const uint8_t*>(playback.layerTracks);
  const std::vector<uint8_t> before(children, children + sizeof(playback.layerTracks));
  e.note(0, 1);
  for (int tick = 0; tick < 6; ++tick) e.render();
  CHECK(playback.layerTrackMask == 0);
  CHECK(e.state->layerVoiceTrackMask == 0);
  CHECK(e.state->voiceOwnerCount == 1);
  CHECK(std::memcmp(before.data(), playback.layerTracks, before.size()) == 0);
  CHECK(e.output[400] > .01f);
}

TEST_CASE("Layer owner bounds follow previews, transitions and stops on the last track") {
  LayerPlayback e(8);
  e.init();
  e.note(0, 1);
  e.note(7, 0);
  e.render();
  CHECK(e.state->playbackState.layerTrackMask == 0x80);
  CHECK(e.state->layerVoiceTrackMask == 0x80);
  CHECK(e.state->voiceOwnerCount == VOICE_OWNER_COUNT);
  CHECK(e.state->playbackState.layerTracks[7][0].note.instrument == 1);
  e.note(7, 1);
  e.render();
  CHECK(e.state->playbackState.layerTracks[7][0].note.noteKilled);
  e.render();
  CHECK(e.state->playbackState.layerTrackMask == 0);
  CHECK(e.state->layerVoiceTrackMask == 0);
  CHECK(e.state->voiceOwnerCount == 8);
  e.note(7, 0);
  e.render();
  CHECK(e.state->layerVoiceTrackMask == 0x80);
  REQUIRE(chipnomadQueuePlaybackStopPreview(e.state.get(), 7));
  e.render();
  e.render();
  CHECK(e.state->layerVoiceTrackMask == 0);
  CHECK(e.state->voiceOwnerCount == 8);
  const auto* status = chipnomadGetPlaybackStatus(e.state.get());
  REQUIRE(status);
  CHECK(status->layerTracks[7][0].instrument == EMPTY_VALUE_8);
  CHECK(status->layerTracks[7][0].liveInstrument == EMPTY_VALUE_8);
}

TEST_CASE("Live project snapshots activate and retire Layer children without reinitializing") {
  LayerPlayback e;
  getInstrumentFunctions(InstrumentType::AudioIn).init(&e.state->project.instruments[0]);
  e.init();
  e.note(0, 0);
  e.render();
  CHECK(e.state->voiceOwnerCount == 1);

  auto& instrument = e.state->project.instruments[0];
  getInstrumentFunctions(InstrumentType::Layer).init(&instrument);
  instrument.chip.layer.entries[0] = {1, 0x80, 255};
  REQUIRE(chipnomadQueueProjectRefresh(e.state.get()));
  e.render();
  CHECK(e.state->playbackState.layerTracks[0][0].note.instrument == 1);
  CHECK(e.state->layerVoiceTrackMask == 1);

  SUBCASE("remove the child reference") {
    instrument.chip.layer.entries[0].instrument = EMPTY_VALUE_8;
  }
  SUBCASE("replace the parent engine") {
    getInstrumentFunctions(InstrumentType::AudioIn).init(&instrument);
  }
  REQUIRE(chipnomadQueueProjectRefresh(e.state.get()));
  e.render();
  e.render();
  CHECK(e.state->layerVoiceTrackMask == 0);
  CHECK(e.state->voiceOwnerCount == 1);
}

TEST_CASE("Empty and invalid Layer lanes keep controls without copying child states") {
  LayerPlayback e;
  auto& entries = e.state->project.instruments[0].chip.layer.entries;
  entries[0] = {EMPTY_VALUE_8, 0xff, 42};
  entries[1] = {0, 0x80, 43};
  entries[2] = {127, 0x80, 44};
  e.init();
  e.state->playbackState.layerTracks[0][0].slewCurrent[0] = 1234;
  e.note(0, 0);
  e.render();
  CHECK(e.state->layerVoiceTrackMask == 0);
  CHECK(e.state->voiceOwnerCount == 1);
  CHECK(e.state->playbackState.layerTracks[0][0].slewCurrent[0] == 1234);
  CHECK(e.state->playbackState.tracks[0].liveControls[liveSource1] == 255);
  CHECK(e.state->playbackState.tracks[0].liveControls[liveSource2] == 42);
}

TEST_CASE("Retiring Layer sample voices remain visited until their de-click tail ends") {
  LayerPlayback e;
  auto& p = e.state->project;
  p.tickRate = 1000;
  getInstrumentFunctions(InstrumentType::Sample).init(&p.instruments[2]);
  auto& sample = p.instruments[2].chip.sample;
  sample.frameCount = 4096;
  sample.sampleRate = 48000;
  sample.channels = 1;
  sample.data = static_cast<int16_t*>(std::malloc(sample.frameCount * sizeof(int16_t)));
  REQUIRE(sample.data);
  for (uint32_t i = 0; i < sample.frameCount; ++i) sample.data[i] = 12000;
  sample.start = 0;
  sample.end = 255;
  sample.speedPercent = 100;
  sample.attack = sample.decay = 0;
  sample.sustain = 255;
  sample.filterEnabled = 0;
  p.instruments[2].volume = 255;
  p.instruments[0].chip.layer.entries[0].instrument = 2;
  e.init();
  e.note(0, 0);
  e.render(48);
  auto* voice = e.state->sampleVoices[PROJECT_MAX_TRACKS][0];
  REQUIRE(voice);
  REQUIRE(voice->active());
  e.note(0, 1);
  e.render(48);
  CHECK(voice->active());
  e.render(48);
  CHECK(e.state->playbackState.layerTrackMask == 0);
  CHECK(e.state->layerVoiceTrackMask == 1);
  CHECK(voice->active());
  for (int tick = 0; tick < 8; ++tick) e.render(48);
  CHECK_FALSE(voice->active());
  CHECK(e.state->layerVoiceTrackMask == 0);
  CHECK(e.state->voiceOwnerCount == 1);
}

TEST_CASE("Retiring Layer SoundFont voices keep their output correction") {
  LayerPlayback e;
  auto& p = e.state->project;
  p.tickRate = 1000;
  getInstrumentFunctions(InstrumentType::SoundFont).init(&p.instruments[2]);
  soundbank::SampleCache cache;
  auto bank = soundbank::parseBank({"tests/fixtures/soundbanks/tone.wav", ""});
  REQUIRE_FALSE(bank.presets.empty());
  bank.presets[0].zones[0].loop = soundbank::Loop::continuous;
  p.instruments[2].chip.soundfont.prepared = soundbank::publish(cache.prepare(bank, "sample"));
  p.instruments[2].volume = 255;
  p.instruments[0].chip.layer.entries[0].instrument = 2;
  e.init();
  e.note(0, 0);
  for (int tick = 0; tick < 4; ++tick) e.render(48);
  auto* voice = e.state->soundbankVoices[PROJECT_MAX_TRACKS][0];
  REQUIRE(voice);
  REQUIRE(voice->active());
  e.note(0, 1);
  e.render(48);
  e.render(48);
  CHECK(e.state->playbackState.layerTrackMask == 0);
  CHECK(e.state->layerVoiceTrackMask == 1);
  CHECK(voice->active());
  for (int tick = 0; tick < 8; ++tick) e.render(48);
  CHECK_FALSE(voice->active());
  CHECK(e.state->layerVoiceTrackMask == 0);
  CHECK(e.state->voiceOwnerCount == 1);
  e.state.reset();
  soundbank::collect([](uint32_t) { return false; });
}

TEST_CASE("Layer child FLFO advances only while the child is assigned") {
  LayerPlayback e;
  auto& mod = e.state->project.instruments[1].modulation[0];
  mod.type = ModulationType::FLFO;
  mod.destination = 1;
  mod.amount = 32;
  mod.p3 = 80;
  e.init();
  e.note(0, 0);
  e.render();
  auto& child = e.state->playbackState.layerTracks[0][0];
  REQUIRE(child.note.modulation[0].modulation);
  CHECK(child.note.modulation[0].phase > 0.0f);
  const float firstPhase = child.note.modulation[0].phase;
  e.render();
  CHECK(child.note.modulation[0].phase != firstPhase);
  e.note(0, 1);
  e.render();
  REQUIRE(child.note.modulation[0].modulation == nullptr);
  const float retiredPhase = child.note.modulation[0].phase;
  e.render();
  CHECK(child.note.modulation[0].phase == retiredPhase);
  CHECK(e.state->voiceOwnerCount == 1);
}

TEST_CASE("Layer child MIDI notes are released once before the owner is retired") {
  struct MidiCapture {
    std::vector<MidiEvent> events;
    MidiBackend backend{};
    MidiCapture() {
      backend.userdata = this;
      backend.hasOutput = 1;
      backend.scheduleOutput = [](void* data, const MidiEvent* event, uint64_t) {
        static_cast<MidiCapture*>(data)->events.push_back(*event);
      };
      midiRouterSetBackend(&backend);
    }
    ~MidiCapture() { midiRouterSetBackend(nullptr); }
    int count(uint8_t type) const {
      int total = 0;
      for (const auto& event : events) if (event.type == type) ++total;
      return total;
    }
  } midi;
  LayerPlayback e;
  getInstrumentFunctions(InstrumentType::Midi).init(&e.state->project.instruments[2]);
  e.state->project.instruments[0].chip.layer.entries[0].instrument = 2;
  e.init();
  e.note(0, 0);
  e.render();
  REQUIRE(midi.count(0x90) == 1);
  e.note(0, 1);
  e.render();
  CHECK(midi.count(0x80) == 1);
  e.render();
  e.render();
  CHECK(midi.count(0x80) == 1);
  CHECK(e.state->layerVoiceTrackMask == 0);
  CHECK(e.state->voiceOwnerCount == 1);
}

TEST_CASE("Layer defaults and quadratic transpose are stable") {
  Instrument layer{};
  getInstrumentFunctions(InstrumentType::Layer).init(&layer);
  CHECK((int)InstrumentType::Layer == 31);
  CHECK(getInstrumentDefinition(InstrumentType::Layer)->category == InstrumentCategory::utility);
  const auto* definition = getInstrumentDefinition(InstrumentType::Layer);
  CHECK(definition->functions.modDestinationsCount == 7);
  CHECK(definition->fxCount == 6);
  const char* destinations[] = {"Volume", "L1 Cents", "L1 Vol", "L2 Cents", "L2 Vol", "L3 Cents", "L3 Vol"};
  for (int destination = 1; destination <= 7; ++destination)
    CHECK(std::strcmp(instrumentModDestinationName(InstrumentType::Layer, destination), destinations[destination - 1]) == 0);
  fillFXNames();
  const FX layerFX[] = {fxL1C, fxL1V, fxL2C, fxL2V, fxL3C, fxL3V};
  const char* layerFXNames[] = {"L1C", "L1V", "L2C", "L2V", "L3C", "L3V"};
  for (int i = 0; i < 6; ++i) {
    CHECK(instrumentFXAvailable(InstrumentType::Layer, layerFX[i]));
    CHECK(std::strcmp(fxNames[layerFX[i]].name, layerFXNames[i]) == 0);
  }
  for (const auto& entry : layer.chip.layer.entries) {
    CHECK(entry.instrument == EMPTY_VALUE_8);
    CHECK(entry.transpose == 0x80);
    CHECK(entry.volume == 0x80);
  }
  CHECK(instrumentLayerTransposeCents(0x00) == -2400);
  CHECK(instrumentLayerTransposeCents(0x80) == 0);
  CHECK(instrumentLayerTransposeCents(0xff) == 2400);
  CHECK(instrumentLayerTransposeCents(0x81) < instrumentLayerTransposeCents(0xc0));
  bool positiveNotes[25]{}, negativeNotes[25]{};
  for (int value = 0; value < 256; ++value) {
    int cents = instrumentLayerTransposeCents((uint8_t)value);
    if (std::abs(cents) >= 100) {
      CHECK(cents % 100 == 0);
      (cents > 0 ? positiveNotes : negativeNotes)[std::abs(cents) / 100] = true;
    }
  }
  for (int note = 1; note <= 24; ++note) {
    CHECK(positiveNotes[note]);
    CHECK(negativeNotes[note]);
  }
}

TEST_CASE("Layer project and preset preserve slot references") {
  auto saved = std::make_unique<Project>(), loaded = std::make_unique<Project>();
  projectInit(saved.get()); projectInit(loaded.get());
  saved->chipsCount = saved->tracksCount = 1;
  saved->tickRate = 50;
  saved->chipSetup.ay.clock = 1773400;
  calculateLinearPitchTable12TET(saved.get());
  getInstrumentFunctions(InstrumentType::Layer).init(&saved->instruments[4]);
  auto& entries = saved->instruments[4].chip.layer.entries;
  entries[0] = {2, 0x00, 0x20};
  entries[1] = {7, 0x80, 0x80};
  entries[2] = {127, 0xff, 0xff};
  const char* preset = "build/tests/layer.cni";
  REQUIRE(instrumentSave(saved.get(), preset, 4) == 0);
  REQUIRE(instrumentLoad(loaded.get(), preset, 9) == 0);
  CHECK(loaded->instruments[9].type == InstrumentType::Layer);
  for (int i = 0; i < INSTRUMENT_LAYER_COUNT; ++i) {
    CHECK(loaded->instruments[9].chip.layer.entries[i].instrument == entries[i].instrument);
    CHECK(loaded->instruments[9].chip.layer.entries[i].transpose == entries[i].transpose);
    CHECK(loaded->instruments[9].chip.layer.entries[i].volume == entries[i].volume);
  }

  const char* song = "build/tests/layer.cct";
  REQUIRE(projectSave(saved.get(), song) == 0);
  REQUIRE(projectLoad(loaded.get(), song) == 0);
  CHECK(projectFileVersion == 12);
  CHECK(loaded->instruments[4].type == InstrumentType::Layer);
  for (int i = 0; i < INSTRUMENT_LAYER_COUNT; ++i) {
    CHECK(loaded->instruments[4].chip.layer.entries[i].instrument == entries[i].instrument);
    CHECK(loaded->instruments[4].chip.layer.entries[i].transpose == entries[i].transpose);
    CHECK(loaded->instruments[4].chip.layer.entries[i].volume == entries[i].volume);
  }
  std::remove(preset);
  std::remove(song);
  projectFree(saved.get()); projectFree(loaded.get());
}

TEST_CASE("Layer loader normalizes invalid slot values and rejects invalid controls") {
  auto saved = std::make_unique<Project>(), loaded = std::make_unique<Project>();
  projectInit(saved.get()); projectInit(loaded.get());
  getInstrumentFunctions(InstrumentType::Layer).init(&saved->instruments[0]);
  const char* path = "build/tests/layer-invalid.cni";
  REQUIRE(instrumentSave(saved.get(), path, 0) == 0);
  std::ifstream input(path);
  std::stringstream bytes; bytes << input.rdbuf(); input.close();
  std::string text = bytes.str();
  size_t line = text.find("- Layer: 255,128,128");
  REQUIRE(line != std::string::npos);
  text.replace(line, std::strlen("- Layer: 255,128,128"), "- Layer: 200,128,128");
  std::ofstream(path, std::ios::trunc) << text;
  REQUIRE(instrumentLoad(loaded.get(), path, 1) == 0);
  CHECK(loaded->instruments[1].chip.layer.entries[0].instrument == EMPTY_VALUE_8);

  text.replace(text.find("- Layer: 200,128,128"), std::strlen("- Layer: 200,128,128"),
               "- Layer: 2,256,128");
  std::ofstream(path, std::ios::trunc) << text;
  CHECK(instrumentLoad(loaded.get(), path, 2) != 0);
  std::remove(path);
  projectFree(saved.get()); projectFree(loaded.get());
}

TEST_CASE("Layer references follow swaps, survive cleanup and expose child FX") {
  Project project{};
  projectInit(&project);
  getInstrumentFunctions(InstrumentType::Layer).init(&project.instruments[0]);
  getInstrumentFunctions(InstrumentType::DrumSynth).init(&project.instruments[2]);
  project.instruments[0].chip.layer.entries[0].instrument = 2;
  project.phrases[0].rows[0].instrument = 0;
  project.phrases[0].rows[0].note = 40;
  CHECK(instrumentFXAvailableInProject(&project, 0, fxDDC));
  CHECK_FALSE(instrumentFXAvailableInProject(&project, 0, fxBTM));
  instrumentSwap(&project, 2, 3);
  CHECK(project.instruments[0].chip.layer.entries[0].instrument == 3);
  cleanupUnusedInstruments(&project);
  CHECK(project.instruments[3].type == InstrumentType::DrumSynth);
  projectFree(&project);
}

TEST_CASE("Layer renders an Audio in child on an independent lane") {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(chipnomadCreate(), chipnomadDestroy);
  Project& project = state->project;
  project.chipsCount = project.tracksCount = 1;
  project.tickRate = 50;
  project.linearPitch = 1;
  project.chipSetup.ay.clock = 1773400;
  calculateLinearPitchTable12TET(&project);
  getInstrumentFunctions(InstrumentType::Layer).init(&project.instruments[0]);
  getInstrumentFunctions(InstrumentType::AudioIn).init(&project.instruments[1]);
  project.instruments[0].chip.layer.entries[0].instrument = 1;
  project.instruments[0].chip.layer.entries[0].volume = 0x80;
  project.instruments[0].volume = project.instruments[1].volume = 255;
  project.trackVolume[0] = 100;
  chipnomadInitChips(state.get(), 48000, nullptr);
  REQUIRE(chipnomadReserveRenderBuffers(state.get(), 1024) == 0);
  chipnomadQueuePlaybackPreviewNote(state.get(), 0, 45, 0);
  float input[2048], output[2048]{};
  for (int i = 0; i < 1024; ++i) { input[i * 2] = .25f; input[i * 2 + 1] = -.125f; }
  REQUIRE(chipnomadRender(state.get(), output, 1024, input) == 1024);
  CHECK(output[400] > .05f);
  CHECK(output[401] < -.02f);
  CHECK(state->voiceMonitors[0].active);
  CHECK(state->voiceMonitors[PROJECT_MAX_TRACKS].active);
  const PlaybackStatus* status = chipnomadGetPlaybackStatus(state.get());
  REQUIRE(status != nullptr);
  CHECK(status->layerTracks[0][0].instrument == 1);
}

TEST_CASE("Layer modulation controls each lane's cents and volume") {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(chipnomadCreate(), chipnomadDestroy);
  Project& project = state->project;
  project.chipsCount = project.tracksCount = 1;
  project.tickRate = 50;
  project.linearPitch = 1;
  project.chipSetup.ay.clock = 1773400;
  calculateLinearPitchTable12TET(&project);
  getInstrumentFunctions(InstrumentType::Layer).init(&project.instruments[0]);
  getInstrumentFunctions(InstrumentType::AudioIn).init(&project.instruments[1]);
  Instrument& layer = project.instruments[0];
  layer.chip.layer.entries[0].instrument = 1;
  layer.chip.layer.entries[0].volume = 255;
  layer.modulation[0] = {ModulationType::StickLinear, 2, 127, 0, 0, 0, 0, 0};
  layer.modulation[1] = {ModulationType::StickLinear, 3, 127, 1, 0, 0, 0, 0};
  chipnomadSetLiveStickEnabled(1);
  chipnomadSetLiveStickAxes(1.0f, -1.0f, 0.0f, 0.0f);
  chipnomadInitChips(state.get(), 48000, nullptr);
  REQUIRE(chipnomadReserveRenderBuffers(state.get(), 1024) == 0);
  chipnomadQueuePlaybackPreviewNote(state.get(), 0, 45, 0);
  float input[2048]{}, output[2048]{};
  CHECK(chipnomadRender(state.get(), output, 1024, input) == 1024);
  chipnomadSetLiveStickEnabled(0);
  chipnomadSetLiveStickAxes(0.0f, 0.0f, 0.0f, 0.0f);
  const auto& child = state->playbackState.layerTracks[0][0];
  CHECK(child.note.fineOffset == 2400);
  CHECK(child.note.volume1 == 0);
  CHECK(state->playbackState.tracks[0].liveInstrument == 0);
  CHECK(state->playbackState.tracks[0].liveControls[liveSource1] == 255);
  CHECK(state->playbackState.tracks[0].liveControls[liveSource2] == 0);
}

static std::vector<float> renderLayerMonk(uint8_t transpose) {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(chipnomadCreate(), chipnomadDestroy);
  Project& project = state->project;
  project.chipsCount = project.tracksCount = 1;
  project.tickRate = 50;
  project.linearPitch = 1;
  project.chipSetup.ay.clock = 1773400;
  calculateLinearPitchTable12TET(&project);
  getInstrumentFunctions(InstrumentType::Layer).init(&project.instruments[0]);
  getInstrumentFunctions(InstrumentType::Monk).init(&project.instruments[1]);
  project.instruments[0].chip.layer.entries[0] = {1, transpose, 255};
  project.instruments[0].volume = project.instruments[1].volume = 255;
  project.trackVolume[0] = 100;
  chipnomadInitChips(state.get(), 48000, nullptr);
  REQUIRE(chipnomadReserveRenderBuffers(state.get(), 4096) == 0);
  chipnomadQueuePlaybackPreviewNote(state.get(), 0, 45, 0);
  std::vector<float> output(8192);
  REQUIRE(chipnomadRender(state.get(), output.data(), 4096) == 4096);
  CHECK(state->playbackState.layerTracks[0][0].note.fineOffset == instrumentLayerTransposeCents(transpose));
  return output;
}

TEST_CASE("Layer repitches MonkSynth") {
  const auto center = renderLayerMonk(0x80);
  const auto high = renderLayerMonk(0xff);
  double centerEnergy = 0.0, highEnergy = 0.0, difference = 0.0;
  for (size_t i = 0; i < center.size(); ++i) {
    centerEnergy += center[i] * center[i];
    highEnergy += high[i] * high[i];
    difference += std::fabs(center[i] - high[i]);
  }
  CHECK(centerEnergy > 0.001);
  CHECK(highEnergy > 0.001);
  CHECK(difference > 1.0);
}

TEST_CASE("Layer repitches an active MonkSynth voice") {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(chipnomadCreate(), chipnomadDestroy);
  Project& project = state->project;
  project.chipsCount = project.tracksCount = 1; project.tickRate = 50; project.linearPitch = 1;
  project.chipSetup.ay.clock = 1773400; calculateLinearPitchTable12TET(&project);
  getInstrumentFunctions(InstrumentType::Layer).init(&project.instruments[0]);
  getInstrumentFunctions(InstrumentType::Monk).init(&project.instruments[1]);
  project.instruments[0].chip.layer.entries[0] = {1, 0x80, 255};
  project.instruments[0].volume = project.instruments[1].volume = 255; project.trackVolume[0] = 100;
  chipnomadInitChips(state.get(), 48000, nullptr); REQUIRE(chipnomadReserveRenderBuffers(state.get(), 4096) == 0);
  chipnomadQueuePlaybackPreviewNote(state.get(), 0, 45, 0);
  std::vector<float> center(8192), high(8192);
  REQUIRE(chipnomadRender(state.get(), center.data(), 4096) == 4096);
  project.instruments[0].chip.layer.entries[0].transpose = 0xff;
  REQUIRE(chipnomadQueueProjectRefresh(state.get()));
  REQUIRE(chipnomadRender(state.get(), high.data(), 4096) == 4096);
  CHECK(state->playbackState.layerTracks[0][0].note.fineOffset == 2400);
  double difference = 0.0;
  for (size_t i = 0; i < center.size(); ++i) difference += std::fabs(center[i] - high[i]);
  CHECK(difference > 1.0);
}

TEST_CASE("Layer lane FX override cents and volume") {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(chipnomadCreate(), chipnomadDestroy);
  Project& project = state->project;
  project.chipsCount = project.tracksCount = 1; project.tickRate = 50; project.linearPitch = 1;
  project.chipSetup.ay.clock = 1773400; calculateLinearPitchTable12TET(&project);
  for (auto& speed : project.grooves[0].speed) speed = 50;
  getInstrumentFunctions(InstrumentType::Layer).init(&project.instruments[0]);
  getInstrumentFunctions(InstrumentType::Monk).init(&project.instruments[1]);
  project.instruments[0].chip.layer.entries[0] = {1, 0x80, 255};
  project.song[0][0] = 0; project.chains[0].rows[0].phrase = 0;
  auto& row = project.phrases[0].rows[0];
  row.note = 45; row.instrument = 0; row.volume = PHRASE_VOLUME_MAX;
  row.fx[0] = {fxL1C, 0xff}; row.fx[1] = {fxL1V, 0x00};
  chipnomadInitChips(state.get(), 48000, nullptr); REQUIRE(chipnomadReserveRenderBuffers(state.get(), 1024) == 0);
  REQUIRE(chipnomadQueueProjectRefresh(state.get()));
  REQUIRE(chipnomadQueuePlaybackStartSong(state.get(), 0, 0, 1));
  float output[2048]{}; REQUIRE(chipnomadRender(state.get(), output, 1024) == 1024);
  const auto& child = state->playbackState.layerTracks[0][0];
  CHECK(child.note.fineOffset == 2400);
  CHECK(child.note.volume1 == 0);
}

TEST_CASE("Layer gives legacy AY children a virtual chip and honors zero lane volume") {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(chipnomadCreate(), chipnomadDestroy);
  Project& project = state->project;
  project.chipsCount = project.tracksCount = 1;
  project.tickRate = 50;
  project.linearPitch = 1;
  project.chipSetup.ay.clock = 1773400;
  calculateLinearPitchTable12TET(&project);
  getInstrumentFunctions(InstrumentType::Layer).init(&project.instruments[0]);
  getInstrumentFunctions(InstrumentType::AY1).init(&project.instruments[1]);
  project.instruments[0].chip.layer.entries[0].instrument = 1;
  project.instruments[0].chip.layer.entries[0].volume = 0x80;
  project.trackVolume[0] = 100;
  chipnomadInitChips(state.get(), 48000, nullptr);
  REQUIRE(state->layerAYChips[PROJECT_MAX_TRACKS] != nullptr);
  REQUIRE(chipnomadReserveRenderBuffers(state.get(), 2048) == 0);
  chipnomadQueuePlaybackPreviewNote(state.get(), 0, 48, 0);
  float audible[4096]{};
  REQUIRE(chipnomadRender(state.get(), audible, 2048) == 2048);
  const auto& child = state->playbackState.layerTracks[0][0];
  CHECK(child.note.instrument == 1);
  CHECK(child.note.pitchFinal != EMPTY_VALUE_8);
  CHECK(child.note.chip.ay.outVolume > 0);
  float energy = 0.0f;
  for (float sample : audible) energy += std::fabs(sample);
  CHECK(energy > 0.01f);

  state->project.instruments[0].chip.layer.entries[0].volume = 0;
  chipnomadInitChips(state.get(), 48000, nullptr);
  chipnomadQueuePlaybackPreviewNote(state.get(), 0, 48, 0);
  float silent[4096]{};
  REQUIRE(chipnomadRender(state.get(), silent, 2048) == 2048);
  float silentEnergy = 0.0f;
  for (float sample : silent) silentEnergy += std::fabs(sample);
  CHECK(silentEnergy == doctest::Approx(0.0f).epsilon(0.000001));
}
