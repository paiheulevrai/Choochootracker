#include "doctest.h"
#include "chipnomad_lib.h"
#include "playback.h"
#include "pitch_table_utils.h"
#include "link/link_client.h"
#include "link/link_backend.h"
#include "midi/midi_router.h"
#include "common.h"

#include <chrono>
#include <cstdint>

// End-to-end Link transport follow: the app.cpp UI-tick block (poll ->
// tempo apply -> transport apply) drives song playback from the Link
// session's play state. app.cpp's block and queueLinkAlignedSongStart
// are static, so the test mirrors their logic verbatim and exercises the
// real link client + real playback engine.

namespace {

uint64_t nowMicros() {
  return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();
}

// The audio tick's dueMicros comes from midiRouterNowMicros(), which is 0
// unless a MIDI backend is registered. Register one whose clock is the
// real steady_clock so timed starts (also steady_clock-based) line up
// with the audio thread's time base. scheduleOutput/flush must be
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

// Fake Link backend: session state is whatever the test sets. Capture
// succeeds only while "enabled" (mirrors the real backend, which fails
// when Link is disabled). requestBeatAtLaunch returns "now" when peers
// are present, so a queued timed start is already due on the next
// render tick.
struct TransportFakeState {
  int enabled = 0;
  int peers = 0;
  double sessionTempo = 0.0;
  int sessionIsPlaying = 0;
};
TransportFakeState g_transportFake;

void* fakeCreate(void* userdata) { return userdata; }
void fakeDestroy(void*, void*) {}
int fakeEnable(void* userdata, void*) {
  ((TransportFakeState*)userdata)->enabled = 1;
  return 1;
}
void fakeDisable(void* userdata, void*) {
  ((TransportFakeState*)userdata)->enabled = 0;
}
int fakeIsEnabled(void* userdata, void*) {
  return ((TransportFakeState*)userdata)->enabled;
}
int fakeNumPeers(void* userdata, void*) {
  return ((TransportFakeState*)userdata)->peers;
}
int fakeCapture(void* userdata, void*, double* tempo, double* beat,
                uint64_t* capturedAtMicros, uint64_t* peers, int* isPlaying) {
  TransportFakeState* fake = (TransportFakeState*)userdata;
  if (!fake->enabled) return 0;
  if (tempo) *tempo = fake->sessionTempo;
  if (beat) *beat = 0.0;
  if (capturedAtMicros) *capturedAtMicros = nowMicros();
  if (peers) *peers = (uint64_t)fake->peers;
  if (isPlaying) *isPlaying = fake->sessionIsPlaying;
  return 1;
}
uint64_t fakeRequestBeatAtLaunch(void* userdata, void*, double) {
  TransportFakeState* fake = (TransportFakeState*)userdata;
  if (fake->enabled && fake->peers > 0) return nowMicros();
  return 0;
}
void fakeOnAudioFrame(void*, void*, uint64_t, uint64_t) {}
void fakeResetFilter(void*, void*) {}

const LinkBackend kTransportFakeBackend = {
  &g_transportFake,
  fakeCreate, fakeDestroy,
  fakeEnable, fakeDisable, fakeIsEnabled,
  fakeNumPeers,
  fakeCapture,
  fakeRequestBeatAtLaunch,
  fakeOnAudioFrame,
  fakeResetFilter,
};

// Mirror of app.cpp's queueLinkAlignedSongStart (static there): with Link
// as clock source and a connected session, queue a song start aligned to
// the next downbeat; return 0 when the caller should start immediately.
// (The screenMessage call is app-side presentation and is not mirrored.)
static int queueLinkAlignedSongStart(ChipNomadState* state, int songRow, int chainRow, int loop) {
  if (appSettings.clockSource != ClockSource::link) return 0;
  uint32_t serial = 0;
  if (linkClientGetStatus(state->linkClient, &serial) != LinkStatus::connected) return 0;
  const uint64_t launchTime = linkClientRequestBeatAtLaunch(state->linkClient, 4);
  if (launchTime == 0) return 0;
  chipnomadQueuePlaybackStartSongAtTime(state, songRow, chainRow, loop, launchTime);
  return 1;
}

// Mirror of app.cpp's per-UI-tick Link block: poll, then apply session
// tempo, then apply transport (each exactly once via serial compare).
static void simulateUiTick(ChipNomadState* state, uint32_t* bpmAppliedSerial,
                           uint32_t* transportAppliedSerial) {
  if (appSettings.clockSource == ClockSource::link) {
    linkClientPoll(state->linkClient);
  }
  if (appSettings.clockSource == ClockSource::link) {
    uint32_t linkBpmSerial = 0;
    const float linkBpm = linkClientGetSessionTempo(state->linkClient, &linkBpmSerial);
    if (linkBpm > 0.0f && linkBpmSerial != *bpmAppliedSerial) {
      *bpmAppliedSerial = linkBpmSerial;
      // BPM -> Hz: tickRate = BPM * 0.4 (24 ticks per beat).
      state->project.tickRate = linkBpm * 0.4f;
    }
  }
  if (appSettings.clockSource == ClockSource::link) {
    int linkPlaying = 0;
    uint32_t linkTransportSerial = 0;
    linkClientGetTransport(state->linkClient, &linkPlaying, &linkTransportSerial);
    if (linkTransportSerial != *transportAppliedSerial) {
      *transportAppliedSerial = linkTransportSerial;
      if (linkPlaying && !chipnomadGetPlaybackStatus(state)->isPlaying) {
        if (!queueLinkAlignedSongStart(state, *pSongRow, 0, 1))
          chipnomadQueuePlaybackStartSong(state, *pSongRow, 0, 1);
      } else if (!linkPlaying && chipnomadGetPlaybackStatus(state)->isPlaying) {
        chipnomadQueuePlaybackStop(state);
      }
    }
  }
}

struct TransportFixture {
  ChipNomadState* state;
  int songRow;

  TransportFixture() {
    g_transportFake = TransportFakeState{};
    midiRouterSetBackend(&kRealClockBackend);
    linkClientSetBackend(&kTransportFakeBackend);
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
    // App-side globals the mirrored UI-tick block reads.
    appSettings.clockSource = ClockSource::link;
    chipnomadState = state;
    songRow = 0;
    pSongRow = &songRow;
    linkClientSelect(state->linkClient);
    linkClientConnect(state->linkClient);
  }

  ~TransportFixture() {
    // Destroy the state while the fake backend is still registered
    // (linkClientDestroy calls disable/destroy on it).
    chipnomadDestroy(state);
    appSettings.clockSource = ClockSource::internal;
    chipnomadState = nullptr;
    pSongRow = nullptr;
    linkClientSetBackend(nullptr);
  }
};

}  // namespace

TEST_SUITE("Link transport") {

TEST_CASE("transport follow: joining a playing session starts playback") {
  TransportFixture fixture;
  ChipNomadState* state = fixture.state;
  g_transportFake.peers = 1;
  g_transportFake.sessionTempo = 125.0;
  g_transportFake.sessionIsPlaying = 1;

  uint32_t bpmAppliedSerial = 0;
  uint32_t transportAppliedSerial = 0;
  simulateUiTick(state, &bpmAppliedSerial, &transportAppliedSerial);

  // The connecting -> connected transition captured an initial snapshot,
  // so the transport serial is live on the very first UI tick.
  CHECK(linkClientGetStatus(state->linkClient, nullptr) == LinkStatus::connected);
  CHECK(transportAppliedSerial != 0);

  float buffer[2] = {};
  int started = 0;
  for (int i = 0; i < 200000 && !started; ++i) {
    chipnomadRender(state, buffer, 1);
    if (chipnomadGetPlaybackStatus(state)->isPlaying) started = 1;
  }
  CHECK(started == 1);
}

TEST_CASE("transport follow: idle session does not start playback") {
  TransportFixture fixture;
  ChipNomadState* state = fixture.state;
  g_transportFake.peers = 1;
  g_transportFake.sessionTempo = 125.0;
  g_transportFake.sessionIsPlaying = 0;

  uint32_t bpmAppliedSerial = 0;
  uint32_t transportAppliedSerial = 0;
  simulateUiTick(state, &bpmAppliedSerial, &transportAppliedSerial);

  CHECK(linkClientGetStatus(state->linkClient, nullptr) == LinkStatus::connected);
  // Session never played: no transport change was published, so the
  // serial stays at its initial 0 and the block must not start playback.
  CHECK(transportAppliedSerial == 0);

  float buffer[2] = {};
  for (int i = 0; i < 2000; ++i) chipnomadRender(state, buffer, 1);
  CHECK(chipnomadGetPlaybackStatus(state)->isPlaying == 0);
}

TEST_CASE("transport follow: manual stop stays stopped until session restarts") {
  TransportFixture fixture;
  ChipNomadState* state = fixture.state;
  g_transportFake.peers = 1;
  g_transportFake.sessionTempo = 125.0;
  g_transportFake.sessionIsPlaying = 1;

  uint32_t bpmAppliedSerial = 0;
  uint32_t transportAppliedSerial = 0;
  simulateUiTick(state, &bpmAppliedSerial, &transportAppliedSerial);

  float buffer[2] = {};
  int started = 0;
  for (int i = 0; i < 200000 && !started; ++i) {
    chipnomadRender(state, buffer, 1);
    if (chipnomadGetPlaybackStatus(state)->isPlaying) started = 1;
  }
  REQUIRE(started == 1);

  // Manual local stop while the session keeps playing.
  chipnomadQueuePlaybackStop(state);
  int stopped = 0;
  for (int i = 0; i < 200000 && !stopped; ++i) {
    chipnomadRender(state, buffer, 1);
    if (!chipnomadGetPlaybackStatus(state)->isPlaying) stopped = 1;
  }
  REQUIRE(stopped == 1);

  // Session still playing and the transport serial unchanged: the UI
  // tick must NOT restart playback (exactly-once semantics).
  simulateUiTick(state, &bpmAppliedSerial, &transportAppliedSerial);
  for (int i = 0; i < 20000; ++i) chipnomadRender(state, buffer, 1);
  CHECK(chipnomadGetPlaybackStatus(state)->isPlaying == 0);

  // Session stops (serial bumps, local already stopped: no-op), then
  // starts again (serial bumps again): playback resumes.
  g_transportFake.sessionIsPlaying = 0;
  simulateUiTick(state, &bpmAppliedSerial, &transportAppliedSerial);
  CHECK(chipnomadGetPlaybackStatus(state)->isPlaying == 0);
  g_transportFake.sessionIsPlaying = 1;
  simulateUiTick(state, &bpmAppliedSerial, &transportAppliedSerial);
  started = 0;
  for (int i = 0; i < 200000 && !started; ++i) {
    chipnomadRender(state, buffer, 1);
    if (chipnomadGetPlaybackStatus(state)->isPlaying) started = 1;
  }
  CHECK(started == 1);
}

}  // TEST_SUITE
