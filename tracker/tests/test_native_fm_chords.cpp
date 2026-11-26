#include "doctest.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "synth/opl_voice.h"
#include "synth/opll_voice.h"
#include "synth/four_op_voice.h"
#include "synth/dx7_voice.h"
#include "opl_patch.h"
#include "opll_presets.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace {
std::vector<float> renderTiltChord(InstrumentType type, uint8_t tilt, int blockSize,
                                   int sampleRate = 48000, int tickRate = 8, bool chord = true,
                                   int sinteredFilter = -1) {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(
    chipnomadCreate(), chipnomadDestroy);
  REQUIRE(state);
  auto& p = state->project;
  p.chipsCount = p.tracksCount = 1;
  p.tickRate = tickRate;
  p.linearPitch = 1;
  p.scaleApply = 0;
  calculateLinearPitchTable12TET(&p);
  for (auto& groove : p.grooves)
    for (auto& speed : groove.speed) speed = 2;
  getInstrumentFunctions(type).init(&p.instruments[0]);
  if (type == InstrumentType::Layer) {
    const InstrumentType children[] = {
      sinteredFilter >= 0 ? InstrumentType::Sintered : InstrumentType::VRC7,
      InstrumentType::OPL2, InstrumentType::AudioIn};
    for (int child = 0; child < 3; ++child) {
      auto& instrument = p.instruments[child + 1];
      getInstrumentFunctions(children[child]).init(&instrument);
      instrument.pan = 64 + child * 64;
      p.instruments[0].chip.layer.entries[child] = {uint8_t(child + 1), 0x80, 128};
    }
    p.instruments[3].chip.audioIn.filterEnabled = 0;
  } else if (type == InstrumentType::VRC7) {
    REQUIRE(opllApplyPreset(&p.instruments[0], 2));
    auto& fm = p.instruments[0].chip.opll;
    fm.amp.enabled = 1;
    fm.amp.attack = 95;
    fm.amp.decay = 144;
    fm.amp.sustain = 0;
    fm.amp.release = 80;
    fm.amp.envelopeShape = 255;
    fm.tone.brightness = 63;
    fm.tone.feedback = 8;
  } else if (type == InstrumentType::AudioIn) {
    auto& input = p.instruments[0].chip.audioIn;
    input.filterEnabled = 0;
    input.attack = input.decay = input.release = 0;
    input.sustain = 255;
  }
  if (sinteredFilter >= 0) {
    auto& sintered = p.instruments[type == InstrumentType::Layer ? 1 : 0].chip.sintered;
    sintered.gate = 1;
    sintered.filterEnabled = sinteredFilter != 0;
    sintered.filterCharacter = sinteredFilter;
    sintered.filterCutoffHz = 2000;
    sintered.filterResonance = 100;
  }
  p.trackTilt[0] = tilt;
  p.trackVolume[0] = 100;
  p.trackReverbSend[0] = p.trackDelaySend[0] = 0;
  p.song[0][0] = 0;
  p.chains[0].rows[0].phrase = 0;
  p.chains[0].rows[0].transpose = 0;
  phraseClear(&p.phrases[0]);
  for (int row = 0; row < 2; ++row) {
    auto& note = p.phrases[0].rows[row];
    note.note = row ? 34 : 36;
    note.instrument = 0;
    note.volume = PHRASE_VOLUME_MAX;
    if (chord) {
      note.fx[0][0] = fxCRD;
      note.fx[0][1] = row ? 0x6c : 0x5c;
    }
  }
  p.phrases[0].rows[2].note = NOTE_OFF;
  chipnomadInitChips(state.get(), sampleRate, nullptr);
  REQUIRE(chipnomadReserveRenderBuffers(state.get(), blockSize) == 0);
  float* const trackBuffer = state->insertBuffer;
  REQUIRE(chipnomadQueuePlaybackStartSong(state.get(), 0, 0, 1));
  std::vector<float> input(sampleRate * 2), output(sampleRate * 2);
  for (int i = 0; i < sampleRate; ++i) {
    input[i * 2] = .3f * std::sin(i * .037f);
    input[i * 2 + 1] = .2f * std::cos(i * .053f);
  }
  for (int offset = 0; offset < sampleRate;) {
    const int frames = std::min(blockSize, sampleRate - offset);
    REQUIRE(chipnomadRender(state.get(), output.data() + offset * 2,
                           frames, input.data() + offset * 2) == frames);
    offset += frames;
  }
  CHECK(state->insertBuffer == trackBuffer);
  return output;
}

float maxDifference(const std::vector<float>& a, const std::vector<float>& b) {
  REQUIRE(a.size() == b.size());
  float maximum = 0;
  bool finite = true;
  for (size_t i = 0; i < a.size(); ++i) {
    finite &= std::isfinite(a[i]) && std::isfinite(b[i]);
    maximum = std::max(maximum, std::fabs(a[i] - b[i]));
  }
  REQUIRE(finite);
  return maximum;
}
}

TEST_CASE("Track tilt on mono, stereo and Layer chords is independent of render blocks") {
  for (auto type : {InstrumentType::VRC7, InstrumentType::OPL2, InstrumentType::AudioIn, InstrumentType::Layer})
    for (int rate : {32000, 48000})
      for (uint8_t tilt : {0, 0x60, 128, 0xa0, 255}) {
        const int tickRate = rate == 48000 ? 8 : 50;
        CAPTURE(type);
        CAPTURE(rate);
        CAPTURE(tickRate);
        CAPTURE(tilt);
        const auto reference = renderTiltChord(type, tilt, 32, rate, tickRate);
        for (int frames : {137, 512, 6000}) {
          CAPTURE(frames);
          CHECK(maxDifference(reference, renderTiltChord(type, tilt, frames, rate, tickRate)) < 1e-7f);
        }
      }
}

TEST_CASE("Sintered filtered chords and Layer retriggers are independent of render blocks") {
  for (auto type : {InstrumentType::Sintered, InstrumentType::Layer}) {
    for (int filter = 0; filter <= 6; ++filter) {
      CAPTURE(type); CAPTURE(filter);
      const auto reference = renderTiltChord(type, 128, 32, 48000, 8, true, filter);
      for (int frames : {137, 512})
        CHECK(maxDifference(reference, renderTiltChord(type, 128, frames, 48000, 8, true, filter)) < 1e-7f);
    }
  }
}

TEST_CASE("Layer track tilt has no gain discontinuity around 40") {
  const auto peak = [](const std::vector<float>& samples) {
    float result = 0.0f;
    for (float sample : samples) result = std::max(result, std::fabs(sample));
    return result;
  };
  const float below = peak(renderTiltChord(InstrumentType::Layer, 0x3f, 512));
  const float at = peak(renderTiltChord(InstrumentType::Layer, 0x40, 512));
  CHECK(below > at);
  CHECK(below / at < 1.02f);
}

TEST_CASE("Track tilt filters the summed signal once including Layer children") {
  for (auto type : {InstrumentType::VRC7, InstrumentType::OPL2, InstrumentType::AudioIn, InstrumentType::Layer})
    for (bool chord : {false, true}) {
      const auto dry = renderTiltChord(type, 128, 512, 48000, 8, chord);
      for (uint8_t value : {0, 0x60, 0xa0, 255}) {
        CAPTURE(type);
        CAPTURE(chord);
        CAPTURE(value);
        TrackTilt tilt;
        tilt.init(48000);
        auto expected = dry;
        for (size_t i = 0; i < expected.size(); ++i)
          expected[i] = tilt.process(expected[i], i & 1, value, 1000);
        CHECK(maxDifference(expected, renderTiltChord(type, value, 512, 48000, 8, chord)) < 1e-7f);
      }
    }
}

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
