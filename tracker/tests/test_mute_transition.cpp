#include "doctest.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "soundbank.h"
#include "soundbank_runtime.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace {
struct MuteFixture {
  std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)> state{
    chipnomadCreate(), chipnomadDestroy};
  soundbank::SampleCache cache;

  MuteFixture(InstrumentType type, int rate = 48000, int tickRate = 50) {
    REQUIRE(projectLoad(&state->project, "packaging/common/projects/gm-midi-demo.cct") == 0);
    auto& project = state->project;
    project.tracksCount = 2;
    project.tickRate = tickRate;
    project.linearPitch = 1;
    calculateLinearPitchTable12TET(&project);
    for (auto& groove : project.grooves)
      for (auto& speed : groove.speed) speed = 50;
    for (int t = 0; t < PROJECT_MAX_TRACKS; ++t)
      project.trackReverbSend[t] = project.trackDelaySend[t] = 0;
    auto& instrument = project.instruments[0];
    getInstrumentFunctions(type).init(&instrument);
    if (type == InstrumentType::SoundFont) {
      auto bank = soundbank::parseBank({"tests/fixtures/soundbanks/tone.sf2", ""});
      for (auto& zone : bank.presets[0].zones) {
        zone.loop = soundbank::Loop::continuous;
        zone.envelope.sustain = 1;
      }
      instrument.chip.soundfont.prepared = soundbank::publish(cache.prepare(bank, "0:0"));
    }
    if (auto* amp = instrumentFMAmpSettings(&instrument)) {
      amp->enabled = 1;
      amp->sustain = 255;
    }
    for (int t = 0; t < 2; ++t) {
      project.song[0][t] = t;
      project.chains[t].rows[0].phrase = t;
      project.chains[t].rows[0].transpose = 0;
      phraseClear(&project.phrases[t]);
      auto& row = project.phrases[t].rows[0];
      row.note = 45 + 7 * t;
      row.instrument = 0;
      row.volume = PHRASE_VOLUME_MAX;
      project.trackPan[t] = t ? 220 : 32;
    }
    chipnomadInitChips(state.get(), rate, nullptr);
    chipnomadReserveRenderBuffers(state.get(), 2048);
    chipnomadQueueProjectRefresh(state.get());
    chipnomadQueuePlaybackStartSong(state.get(), 0, 0, 1);
  }

  void queueMask(unsigned mask, bool unusedEnabled = true) {
    uint8_t enabled[PROJECT_MAX_TRACKS];
    std::fill(std::begin(enabled), std::end(enabled), unusedEnabled ? 1 : 0);
    for (int t = 0; t < 2; ++t) enabled[t] = (mask >> t) & 1;
    REQUIRE(chipnomadQueueTrackEnabled(state.get(), enabled));
  }

  std::vector<float> render(int frames, int chunk = 2048) {
    std::vector<float> output(frames * 2);
    for (int offset = 0; offset < frames; offset += chunk) {
      const int count = std::min(chunk, frames - offset);
      REQUIRE(chipnomadRender(state.get(), output.data() + offset * 2, count) == count);
    }
    return output;
  }
};

float peak(const std::vector<float>& output, int firstFrame = 0) {
  float result = 0;
  for (size_t i = firstFrame * 2; i < output.size(); ++i)
    result = std::max(result, std::abs(output[i]));
  return result;
}
}

TEST_CASE("Mute unmute solo and unsolo preserve output continuity across engines") {
  for (auto type : {InstrumentType::SoundFont, InstrumentType::OPLL, InstrumentType::DX7,
                    InstrumentType::Monk, InstrumentType::Braids, InstrumentType::SegaPSG,
                    InstrumentType::AY1}) {
    CAPTURE(int(type));
    MuteFixture fixture(type);
    auto previous = fixture.render(3840);
    REQUIRE(peak(previous) > 0);
    // Mute/unmute, solo, switch solo, unsolo, group mute/unmute.
    for (unsigned mask : {2u, 3u, 1u, 2u, 3u, 0u, 3u}) {
      CAPTURE(mask);
      fixture.queueMask(mask);
      auto output = fixture.render(960, 32); // Split the 144-frame correction across callbacks.
      for (int c = 0; c < 2; ++c)
        CHECK(output[c] == doctest::Approx(previous[previous.size() - 2 + c]).epsilon(0.00001));
      CHECK(playbackIsPlaying(&fixture.state->playbackState));
      for (int t = 0; t < 2; ++t)
        CHECK(fixture.state->playbackState.trackEnabled[t] == ((mask >> t) & 1));
      for (float sample : output) CHECK(std::isfinite(sample));
      if (mask == 0) {
        for (int i = 144 * 2; i < int(output.size()); ++i) CHECK(output[i] == 0);
      } else {
        CHECK(peak(output, 144) > 0);
      }
      previous = std::move(output);
    }
  }
}

TEST_CASE("Mute correction converges to the normal mix and ignores unchanged effective masks") {
  for (int rate : {22050, 44100, 48000, 96000}) {
    CAPTURE(rate);
    MuteFixture smoothed(InstrumentType::OPLL, rate);
    MuteFixture reference(InstrumentType::OPLL, rate);
    const int tickFrames = rate / 50;
    CHECK(smoothed.render(tickFrames * 4) == reference.render(tickFrames * 4));
    for (unsigned mask : {1u, 3u, 0u, 3u}) {
      smoothed.queueMask(mask);
      // The reference gates directly at the same tick, exposing the normal
      // post-change mix without an output correction.
      for (int t = 0; t < 2; ++t) reference.state->playbackState.trackEnabled[t] = (mask >> t) & 1;
      const auto output = smoothed.render(tickFrames, 17);
      const auto raw = reference.render(tickFrames, 17);
      for (int i = rate * 3 / 1000 * 2; i < int(output.size()); ++i) CHECK(output[i] == raw[i]);
      // Re-publishing the same effective mask must leave ordinary playback bit-exact,
      // even when the unused-track bits change.
      smoothed.queueMask(mask, false);
      CHECK(smoothed.render(tickFrames) == reference.render(tickFrames));
    }
  }
}

TEST_CASE("Rapid mute changes retarget the current output without callback-size dependence") {
  // A 1 ms tick lets a second toggle arrive during the 3 ms correction.
  MuteFixture whole(InstrumentType::OPLL, 48000, 1000);
  MuteFixture split(InstrumentType::OPLL, 48000, 1000);
  auto previous = whole.render(480);
  CHECK(previous == split.render(480));
  REQUIRE(peak(previous) > 0);
  for (unsigned mask : {1u, 0u, 3u, 2u, 1u, 3u}) {
    whole.queueMask(mask);
    split.queueMask(mask);
    auto output = whole.render(48);
    CHECK(output == split.render(48, 1));
    for (int c = 0; c < 2; ++c)
      CHECK(output[c] == doctest::Approx(previous[previous.size() - 2 + c]).epsilon(0.00001));
    previous = std::move(output);
  }
  // A redundant publication during the correction must not restart it.
  split.queueMask(3);
  CHECK(whole.render(480) == split.render(480, 1));
}

TEST_CASE("Changing mute settings while idle does not manufacture audio or extend the render") {
  MuteFixture fixture(InstrumentType::OPLL);
  fixture.render(960);
  chipnomadQueuePlaybackStop(fixture.state.get());
  float output[1920]{};
  chipnomadRender(fixture.state.get(), output, 960);
  for (unsigned mask : {0u, 3u, 1u}) {
    fixture.queueMask(mask);
    CHECK(chipnomadRender(fixture.state.get(), output, 960) == 0);
    for (float sample : output) CHECK(sample == 0);
  }
}
