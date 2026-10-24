#include "doctest.h"
#include "chipnomad_lib.h"
#include "playback.h"
#include "pitch_table_utils.h"
#include "midi/midi_router.h"

#include <chrono>
#include <cstdint>

// T40: chipnomadQueuePlaybackStartSongAtTime - a queued song start fires on
// the first audio tick frame at or after its target host time, not before.

namespace {

uint64_t nowMicros() {
  return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();
}

// The audio tick's dueMicros comes from midiRouterNowMicros(), which is 0
// unless a MIDI backend is registered. Register one whose clock is the real
// steady_clock so queued start-at-time targets (also steady_clock-based)
// line up with the audio thread's time base. scheduleOutput/flush must be
// non-null: midiRouterPanic sends All Notes Off unconditionally.
uint64_t realNowMicros(void*) { return nowMicros(); }
void noopScheduleOutput(void*, const MidiEvent*, uint64_t) {}
void noopFlushOutputQueue(void*) {}
const MidiBackend kRealClockBackend = {
  nullptr,
  nullptr, nullptr, nullptr, nullptr, // port enumeration - unused
  nullptr, nullptr, nullptr, nullptr, // open/close - unused
  nullptr,                            // pollInput - unused
  noopScheduleOutput, noopFlushOutputQueue, nullptr,
  realNowMicros,
  0, 0, 0, 0,
};

struct StartAtTimeFixture {
  ChipNomadState* state;

  StartAtTimeFixture() {
    midiRouterSetBackend(&kRealClockBackend);
    state = chipnomadCreate();
    Project* p = &state->project;
    p->tickRate = 50;
    p->chipType = ChipType::AY;
    p->chipsCount = 1;
    p->chipSetup.ay = (ChipSetupAY){ .clock = 1773400, .isYM = 0, .stereoMode = StereoModeAY::ABC, .stereoSeparation = 50, .pwmFullRange = 0 };
    p->tracksCount = projectGetTotalTracks(p);
    p->linearPitch = 0;
    calculatePitchTableAY(p);
    // Minimal playable song: chain 0 row 0 -> phrase 0, so
    // playbackStartSong actually puts tracks into song mode.
    p->song[0][0] = 0;
    p->chains[0].rows[0].phrase = 0;
    p->phrases[0].rows[0].note = 60;
    p->phrases[0].rows[0].instrument = 0;
    chipnomadInitChips(state, 44100, nullptr);
    playbackInit(&state->playbackState, p);
  }

  ~StartAtTimeFixture() {
    chipnomadDestroy(state);
  }
};

}  // namespace

TEST_SUITE("timed song start") {

TEST_CASE("start at time fires at the target, not immediately") {
  StartAtTimeFixture fixture;
  ChipNomadState* state = fixture.state;

  // Queue a start 1000 us in the future.
  const uint64_t target = nowMicros() + 1000;
  REQUIRE(chipnomadQueuePlaybackStartSongAtTime(state, 0, 0, 1, target));

  // A single-sample render right away must NOT start playback.
  float buffer[2] = {};
  chipnomadRender(state, buffer, 1);
  CHECK(chipnomadGetPlaybackStatus(state)->isPlaying == 0);

  // Keep rendering in small chunks until playback begins. The start fires
  // on the first tick frame whose dueMicros (wall clock) reaches the
  // target; tick frames arrive every 882 rendered samples, so poll until
  // well past the target in wall time.
  int started = 0;
  uint64_t startedAt = 0;
  for (int i = 0; i < 200000 && !started; ++i) {
    chipnomadRender(state, buffer, 1);
    if (chipnomadGetPlaybackStatus(state)->isPlaying) {
      started = 1;
      startedAt = nowMicros();
    }
  }
  CHECK(started == 1);
  // Started at/after the target (never early).
  CHECK(startedAt >= target);
}

TEST_CASE("start at time 0 fires immediately") {
  StartAtTimeFixture fixture;
  ChipNomadState* state = fixture.state;

  REQUIRE(chipnomadQueuePlaybackStartSongAtTime(state, 0, 0, 1, 0));
  float buffer[2] = {};
  chipnomadRender(state, buffer, 1);
  CHECK(chipnomadGetPlaybackStatus(state)->isPlaying == 1);
}

TEST_CASE("stop request cancels a held timed start") {
  StartAtTimeFixture fixture;
  ChipNomadState* state = fixture.state;

  // Queue a start 50 ms in the future, then stop: the pending start must
  // be dropped, so playback never begins.
  REQUIRE(chipnomadQueuePlaybackStartSongAtTime(state, 0, 0, 1, nowMicros() + 50000));
  chipnomadQueuePlaybackStop(state);
  float buffer[2] = {};
  for (int i = 0; i < 100; ++i) chipnomadRender(state, buffer, 1);
  CHECK(chipnomadGetPlaybackStatus(state)->isPlaying == 0);
}

TEST_CASE("conditional start survives a pending stop") {
  StartAtTimeFixture fixture;
  ChipNomadState* state = fixture.state;
  float buffer[2] = {};
  REQUIRE(chipnomadQueuePlaybackStartSong(state, 0, 0, 1));
  chipnomadRender(state, buffer, 1);
  CHECK(chipnomadGetPlaybackStatus(state)->isPlaying == 1);

  // This mirrors a MIDI Stop followed immediately by Start. The decision to
  // restart belongs to the audio thread, after it has consumed Stop.
  chipnomadQueuePlaybackStop(state);
  REQUIRE(chipnomadQueuePlaybackStartSongIfStopped(state, 0, 0, 1));
  chipnomadRender(state, buffer, 1);
  CHECK(chipnomadGetPlaybackStatus(state)->isPlaying == 1);
}

TEST_CASE("a newer timed start replaces an older pending one") {
  StartAtTimeFixture fixture;
  ChipNomadState* state = fixture.state;

  // Queue a far-future start, then a near-future one: the near one wins.
  REQUIRE(chipnomadQueuePlaybackStartSongAtTime(state, 0, 0, 1, nowMicros() + 100000));
  REQUIRE(chipnomadQueuePlaybackStartSongAtTime(state, 0, 0, 1, nowMicros() + 1000));
  float buffer[2] = {};
  int started = 0;
  for (int i = 0; i < 200000 && !started; ++i) {
    chipnomadRender(state, buffer, 1);
    started = chipnomadGetPlaybackStatus(state)->isPlaying;
  }
  CHECK(started == 1);
}

}
