#include "doctest.h"
#include "chipnomad_lib.h"
#include <algorithm>
#include <memory>
#include <vector>

TEST_CASE("Project voice pools follow the instruments while playback remains safe") {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(chipnomadCreate(), chipnomadDestroy);
  REQUIRE(state);
  chipnomadInitChips(state.get(), 48000, nullptr);
  REQUIRE(chipnomadReserveRenderBuffers(state.get(), 128) == 0);
  CHECK(state->sampleVoices[0][0] == nullptr);
  CHECK(state->dx7Parts[0] == nullptr);
  CHECK(state->braidsVoices[0][0] == nullptr);
  std::vector<float> audio(256);
  std::fill(audio.begin(), audio.end(), 1.0f);
  chipnomadRender(state.get(), audio.data(), 128);
  CHECK(std::all_of(audio.begin(), audio.end(), [](float sample) { return sample == 0.0f; }));

  auto& sample = state->project.instruments[0];
  getInstrumentFunctions(InstrumentType::Sample).init(&sample);
  auto& dx7 = state->project.instruments[1];
  getInstrumentFunctions(InstrumentType::DX7).init(&dx7);
  chipnomadInitChips(state.get(), 48000, nullptr);
  CHECK(state->sampleVoices[0][0] != nullptr);
  CHECK(state->dx7Parts[0] != nullptr);
  CHECK(state->dx7Voices[0][0] != nullptr);
  CHECK(state->braidsVoices[0][0] == nullptr);
  std::fill(audio.begin(), audio.end(), 1.0f);
  chipnomadRender(state.get(), audio.data(), 128);
  CHECK(std::all_of(audio.begin(), audio.end(), [](float sample) { return sample == 0.0f; }));

  instrumentClear(&sample);
  instrumentClear(&dx7);
  chipnomadInitChips(state.get(), 48000, nullptr);
  CHECK(state->sampleVoices[0][0] == nullptr);
  CHECK(state->dx7Parts[0] == nullptr);
  CHECK(state->dx7Voices[0][0] == nullptr);
  std::fill(audio.begin(), audio.end(), 1.0f);
  chipnomadRender(state.get(), audio.data(), 128);
  CHECK(std::all_of(audio.begin(), audio.end(), [](float sample) { return sample == 0.0f; }));
}

TEST_CASE("A new synth pool can be prepared without changing the playing project") {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(chipnomadCreate(), chipnomadDestroy);
  REQUIRE(state);
  getInstrumentFunctions(InstrumentType::Braids).init(&state->project.instruments[0]);
  chipnomadInitChips(state.get(), 48000, nullptr);
  auto* playingVoice = state->braidsVoices[0][0];
  REQUIRE(playingVoice);
  CHECK(chipnomadCanSwitchInstrumentLive(InstrumentType::Plaits));
  CHECK_FALSE(chipnomadCanSwitchInstrumentLive(InstrumentType::Sample));
  REQUIRE(state->plaitsVoices[0][0] == nullptr);
  auto* prepared = chipnomadPrepareInstrumentVoices(state.get(), InstrumentType::Plaits);
  REQUIRE(prepared);
  CHECK(state->plaitsVoices[0][0] == nullptr);
  chipnomadInstallInstrumentVoices(state.get(), prepared);
  CHECK(state->plaitsVoices[0][0] != nullptr);
  CHECK(state->braidsVoices[0][0] == playingVoice);
  CHECK(state->audioProject.instruments[0].type == InstrumentType::Braids);
  CHECK(chipnomadPrepareInstrumentVoices(state.get(), InstrumentType::Plaits) == nullptr);
}
