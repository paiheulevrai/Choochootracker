#include "doctest.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "synth/opl_voice.h"
#include "synth/opll_voice.h"
#include "synth/four_op_voice.h"
#include "synth/dx7_voice.h"
#include "opl_patch.h"
#include <cmath>
#include <memory>
#include <vector>

TEST_CASE("Native FM CRD chords retain four independently rendered notes") {
  const InstrumentType types[] = {InstrumentType::OPL2, InstrumentType::OPL3,
    InstrumentType::OPL3, InstrumentType::OPL3, InstrumentType::OPLL,
    InstrumentType::VRC7, InstrumentType::GenesisFM, InstrumentType::ArcadeFM,
    InstrumentType::DX7};
  for (int mode = 0; mode < 9; ++mode) {
    const auto type = types[mode];
    CAPTURE(mode);
    auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(
      chipnomadCreate(), chipnomadDestroy);
    REQUIRE(projectLoad(&state->project, "packaging/common/projects/gm-midi-demo.cct") == 0);
    auto& project = state->project;
    project.tracksCount = 1;
    project.tickRate = 50;
    project.linearPitch = 1;
    project.scaleApply = 0;
    calculateLinearPitchTable12TET(&project);
    for (auto& groove : project.grooves)
      for (auto& speed : groove.speed) speed = 50;
    project.song[0][0] = 0;
    project.chains[0].rows[0].phrase = 0;
    project.chains[0].rows[0].transpose = 0;
    phraseClear(&project.phrases[0]);
    auto& instrument = project.instruments[0];
    getInstrumentFunctions(type).init(&instrument);
    if (isOPL(type)) {
      auto& patch = instrument.chip.opl;
      patch.topology = static_cast<OPLTopology>(mode == 0 ? 0 : mode - 1);
      patch.operators[2] = patch.operators[0];
      patch.operators[3] = patch.operators[1];
    }
    auto& row = project.phrases[0].rows[0];
    row.note = 36;
    row.instrument = 0;
    row.volume = PHRASE_VOLUME_MAX;
    row.fx[0][0] = fxCRD;
    row.fx[0][1] = 7; // Four-note major seventh.

    chipnomadInitChips(state.get(), 48000, nullptr);
    chipnomadReserveRenderBuffers(state.get(), 512);
    REQUIRE(chipnomadQueueProjectRefresh(state.get()));
    REQUIRE(chipnomadQueuePlaybackStartSong(state.get(), 0, 0, 1));
    std::vector<float> mix(1024);
    for (int block = 0; block < 8; ++block)
      REQUIRE(chipnomadRender(state.get(), mix.data(), 512) == 512);
    const auto& track = state->playbackState.tracks[0];
    REQUIRE(track.chordVoiceCount == 4);
    const int notes[] = {36, 40, 43, 47};
    std::vector<float> outputs[4];
    for (int note = 0; note < 4; ++note) {
      CAPTURE(note);
      CHECK(track.chordPitchFinal[note] == notes[note]);
      if (type == InstrumentType::DX7) {
        REQUIRE(state->dx7Voices[0][note]->active());
        CHECK(state->dx7Voices[0][note]->envelopeLevel() > 0);
        continue;
      }
      auto renderVoice = [&](auto* voice, bool stereo) {
        REQUIRE(voice->active());
        outputs[note].resize(stereo ? 1024 : 512);
        voice->render(outputs[note].data(), 512);
      };
      if (isOPL(type)) renderVoice(state->oplVoices[0][note], true);
      else if (type == InstrumentType::OPLL || type == InstrumentType::VRC7)
        renderVoice(state->opllVoices[0][note], false);
      else renderVoice(state->fourOpVoices[0][note], true);
      double energy = 0;
      for (float sample : outputs[note]) {
        REQUIRE(std::isfinite(sample));
        energy += sample * sample;
      }
      CHECK(energy > 1e-9);
      for (int earlier = 0; earlier < note; ++earlier)
        CHECK(outputs[note] != outputs[earlier]);
    }
  }
}
