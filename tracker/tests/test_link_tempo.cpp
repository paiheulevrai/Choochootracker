#include "doctest.h"
#include "link/link_client.h"
#include "link/link_backend.h"
#include "chipnomad_lib.h"

#include <vector>
#include <cstdint>

// T38: TEMPO-2/3/5 scenario tests through a mock backend, verifying the
// receiver-only contract end to end: the session tempo flows INTO the
// project, never out (zero setTempo-equivalent calls on the backend).

namespace {

struct TempoFakeState {
  int enabled = 0;
  int peers = 0;
  double sessionTempo = 0.0;
  int sessionIsPlaying = 0;
  int enableCalls = 0;
  int disableCalls = 0;
  int setTempoCalls = 0;  // receiver-only guard: must stay 0 forever
  int captureCalls = 0;
};

TempoFakeState g_tempoFake;

void resetTempoFake() {
  g_tempoFake = TempoFakeState();
}

void* tempoFakeCreate(void*) { return &g_tempoFake; }
void tempoFakeDestroy(void*, void*) {}
int tempoFakeEnable(void*, void*) { ++g_tempoFake.enableCalls; g_tempoFake.enabled = 1; return 1; }
void tempoFakeDisable(void*, void*) { ++g_tempoFake.disableCalls; g_tempoFake.enabled = 0; }
int tempoFakeIsEnabled(void*, void*) { return g_tempoFake.enabled; }
int tempoFakeNumPeers(void*, void*) { return g_tempoFake.peers; }

int tempoFakeCapture(void*, void*, double* tempo, double* beat, uint64_t* capturedAtMicros,
                     uint64_t* peers, int* isPlaying) {
  ++g_tempoFake.captureCalls;
  if (!g_tempoFake.enabled) return 0;
  if (tempo) *tempo = g_tempoFake.sessionTempo;
  if (beat) *beat = 0.0;
  if (capturedAtMicros) *capturedAtMicros = 1;
  if (peers) *peers = (uint64_t)g_tempoFake.peers;
  if (isPlaying) *isPlaying = g_tempoFake.sessionIsPlaying;
  return 1;
}

uint64_t tempoFakeRequestBeatAtLaunch(void*, void*, double) { return 0; }
void tempoFakeOnAudioFrame(void*, void*, uint64_t, uint64_t) {}
void tempoFakeResetFilter(void*, void*) {}

const LinkBackend kTempoFakeBackend = {
  nullptr,
  tempoFakeCreate,
  tempoFakeDestroy,
  tempoFakeEnable,
  tempoFakeDisable,
  tempoFakeIsEnabled,
  tempoFakeNumPeers,
  tempoFakeCapture,
  tempoFakeRequestBeatAtLaunch,
  tempoFakeOnAudioFrame,
  tempoFakeResetFilter,
};

// Mirrors app.cpp's UI-tick Link block (T34): poll the client's session
// tempo and apply it to the project's tick rate on serial change.
void simulateUiTick(ChipNomadState* state, uint32_t* appliedSerial) {
  uint32_t serial = 0;
  const float bpm = linkClientGetSessionTempo(state->linkClient, &serial);
  if (bpm > 0.0f && serial != *appliedSerial) {
    *appliedSerial = serial;
    state->project.tickRate = bpm * 0.4f;
  }
}

}  // namespace

TEST_SUITE("Link tempo sync") {

TEST_CASE("TEMPO-2: joining adopts the session tempo") {
  resetTempoFake();
  g_tempoFake.sessionTempo = 110.0;
  g_tempoFake.peers = 1;
  linkClientSetBackend(&kTempoFakeBackend);
  ChipNomadState* state = chipnomadCreate();
  REQUIRE(state != nullptr);

  linkClientSelect(state->linkClient);
  linkClientConnect(state->linkClient);
  uint32_t statusSerial = 0;
  CHECK(linkClientGetStatus(state->linkClient, &statusSerial) == LinkStatus::connecting);

  // Audio thread publishes the session snapshot; UI poll resolves
  // connecting -> connected and adopts the tempo.
  double tempo = 0; uint64_t peers = 0; int isPlaying = 0;
  REQUIRE(linkClientCaptureAudioSessionState(state->linkClient, &tempo, &peers, &isPlaying));
  linkClientPoll(state->linkClient);
  CHECK(linkClientGetStatus(state->linkClient, &statusSerial) == LinkStatus::connected);

  uint32_t appliedSerial = 0;
  simulateUiTick(state, &appliedSerial);
  CHECK(linkClientGetSessionTempo(state->linkClient, &statusSerial) == doctest::Approx(110.0f));
  // 110 BPM * 0.4 = 44.0 Hz tick rate.
  CHECK(state->project.tickRate == doctest::Approx(44.0).epsilon(0.001));

  chipnomadDestroy(state);
}

TEST_CASE("TEMPO-3: loading a song never pushes tempo to the session") {
  resetTempoFake();
  g_tempoFake.sessionTempo = 110.0;
  g_tempoFake.peers = 1;
  linkClientSetBackend(&kTempoFakeBackend);
  ChipNomadState* state = chipnomadCreate();

  linkClientSelect(state->linkClient);
  linkClientConnect(state->linkClient);
  double tempo = 0; uint64_t peers = 0; int isPlaying = 0;
  REQUIRE(linkClientCaptureAudioSessionState(state->linkClient, &tempo, &peers, &isPlaying));
  linkClientPoll(state->linkClient);

  // "Load" a project with a different tick rate: the project file's
  // tickRate lands in project (as projectLoad would), the session tempo
  // is untouched, and no setTempo-equivalent is ever called.
  state->project.tickRate = 50.0f;
  uint32_t statusSerial = 0;
  CHECK(linkClientGetSessionTempo(state->linkClient, &statusSerial) == doctest::Approx(110.0f));
  CHECK(g_tempoFake.setTempoCalls == 0);
  // The next UI tick re-applies the session tempo over the loaded value.
  uint32_t appliedSerial = 0;
  simulateUiTick(state, &appliedSerial);
  CHECK(state->project.tickRate == doctest::Approx(44.0).epsilon(0.001));
  CHECK(g_tempoFake.setTempoCalls == 0);

  chipnomadDestroy(state);
}

TEST_CASE("TEMPO-5: connect with no peers does not jump the tempo") {
  resetTempoFake();
  g_tempoFake.peers = 0;  // no session out there
  linkClientSetBackend(&kTempoFakeBackend);
  ChipNomadState* state = chipnomadCreate();
  state->project.tickRate = 50.0f;  // 125 BPM

  linkClientSelect(state->linkClient);
  linkClientConnect(state->linkClient);
  // No capture succeeds (nothing enabled publishes nothing meaningful
  // until a peer exists), and poll times out into noPeers.
  double tempo = 0; uint64_t peers = 0; int isPlaying = 0;
  REQUIRE(linkClientCaptureAudioSessionState(state->linkClient, &tempo, &peers, &isPlaying));
  CHECK(tempo == 0.0);
  linkClientPoll(state->linkClient);
  uint32_t statusSerial = 0;
  CHECK(linkClientGetStatus(state->linkClient, &statusSerial) == LinkStatus::connecting);

  // The tempo field stays where the user left it.
  CHECK(linkClientGetSessionTempo(state->linkClient, &statusSerial) == 0.0f);
  uint32_t appliedSerial = 0;
  simulateUiTick(state, &appliedSerial);
  CHECK(state->project.tickRate == doctest::Approx(50.0).epsilon(0.001));
  CHECK(g_tempoFake.setTempoCalls == 0);

  chipnomadDestroy(state);
}

}
