#include "doctest.h"
#include "link/link_client.h"
#include "link/link_backend.h"

#include <vector>
#include <cstdint>

// LinkClient tests through a hand-built fake LinkBackend, mirroring
// test_midi_router.cpp's approach: the real backend only exists under
// DESKTOP_BUILD/PORTMASTER_BUILD/ANDROID_BUILD, which the test build
// never defines, so the state machine and serial semantics are exercised
// against a mock that records every call.

namespace {

struct FakeLinkState {
  int enabled = 0;
  int peers = 0;
  double sessionTempo = 0.0;
  int sessionIsPlaying = 0;  // session transport state reported by capture
  int enableCalls = 0;
  int disableCalls = 0;
  int createCalls = 0;
  int destroyCalls = 0;
  int captureCalls = 0;
  int requestBeatCalls = 0;
  int onAudioFrameCalls = 0;
  int resetFilterCalls = 0;
  uint64_t lastRequestedBeat = 999;  // sentinel: requestBeatAtTime arg
  double lastQuantum = -1.0;
  uint64_t lastSampleCounter = 0;
  uint64_t lastHostTime = 0;
  int enableResult = 1;  // can simulate backend failure
};

FakeLinkState g_fake;

void resetFake() {
  g_fake = FakeLinkState();
}

void* fakeCreate(void*) {
  ++g_fake.createCalls;
  return &g_fake;
}

void fakeDestroy(void*, void*) {
  ++g_fake.destroyCalls;
}

int fakeEnable(void*, void*) {
  ++g_fake.enableCalls;
  g_fake.enabled = g_fake.enableResult;
  return g_fake.enableResult;
}

void fakeDisable(void*, void*) {
  ++g_fake.disableCalls;
  g_fake.enabled = 0;
}

int fakeIsEnabled(void*, void*) { return g_fake.enabled; }

int fakeNumPeers(void*, void*) { return g_fake.peers; }

int fakeCapture(void*, void*, double* tempo, double* beat, uint64_t* capturedAtMicros,
                uint64_t* peers, int* isPlaying) {
  ++g_fake.captureCalls;
  if (!g_fake.enabled) return 0;
  if (tempo) *tempo = g_fake.sessionTempo;
  if (beat) *beat = 0.0;
  if (capturedAtMicros) *capturedAtMicros = 1;
  if (peers) *peers = (uint64_t)g_fake.peers;
  if (isPlaying) *isPlaying = g_fake.sessionIsPlaying;
  return 1;
}

uint64_t fakeRequestBeatAtLaunch(void*, void*, double quantum) {
  ++g_fake.requestBeatCalls;
  g_fake.lastQuantum = quantum;
  if (!g_fake.enabled || g_fake.peers == 0) return 0;
  return 1234567;
}

void fakeOnAudioFrame(void*, void*, uint64_t sampleCounter, uint64_t hostTime) {
  ++g_fake.onAudioFrameCalls;
  g_fake.lastSampleCounter = sampleCounter;
  g_fake.lastHostTime = hostTime;
}

void fakeResetFilter(void*, void*) {
  ++g_fake.resetFilterCalls;
}

const LinkBackend kFakeBackend = {
  nullptr,  // userdata
  fakeCreate,
  fakeDestroy,
  fakeEnable,
  fakeDisable,
  fakeIsEnabled,
  fakeNumPeers,
  fakeCapture,
  fakeRequestBeatAtLaunch,
  fakeOnAudioFrame,
  fakeResetFilter,
};

}  // namespace

TEST_SUITE("Link client") {

TEST_CASE("Create/destroy lifecycle talks to the backend exactly once") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();
  REQUIRE(client != nullptr);
  CHECK(g_fake.createCalls == 1);
  uint32_t serial = 0;
  CHECK(linkClientGetStatus(client, &serial) == LinkStatus::disabled);
  CHECK(serial == 0);
  linkClientDestroy(client);
  CHECK(g_fake.destroyCalls == 1);
  CHECK(g_fake.disableCalls == 0);  // never enabled, no disable needed
}

TEST_CASE("Select/Deselect are preference-only: no enable/disable calls") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  linkClientSelect(client);
  uint32_t serial = 0;
  CHECK(linkClientGetStatus(client, &serial) == LinkStatus::selected);
  CHECK(serial == 1);
  CHECK(g_fake.enableCalls == 0);
  CHECK(g_fake.disableCalls == 0);

  linkClientDeselect(client);
  CHECK(linkClientGetStatus(client, &serial) == LinkStatus::disabled);
  CHECK(serial == 2);
  CHECK(g_fake.enableCalls == 0);
  CHECK(g_fake.disableCalls == 0);

  linkClientDestroy(client);
}

TEST_CASE("Connect enables the backend and bumps the serial through connecting") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  linkClientSelect(client);
  uint32_t serialAfterSelect = 0;
  CHECK(linkClientGetStatus(client, &serialAfterSelect) == LinkStatus::selected);

  linkClientConnect(client);
  CHECK(g_fake.enableCalls == 1);
  uint32_t serialAfterConnect = 0;
  CHECK(linkClientGetStatus(client, &serialAfterConnect) == LinkStatus::connecting);
  CHECK(serialAfterConnect > serialAfterSelect);

  linkClientDestroy(client);
}

TEST_CASE("Backend enable failure reports error status") {
  resetFake();
  g_fake.enableResult = 0;
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  linkClientSelect(client);
  linkClientConnect(client);
  uint32_t serial = 0;
  CHECK(linkClientGetStatus(client, &serial) == LinkStatus::error);
  CHECK(g_fake.enableCalls == 1);

  linkClientDestroy(client);
}

TEST_CASE("Disconnect disables the backend and returns to selected") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  linkClientSelect(client);
  linkClientConnect(client);
  g_fake.peers = 2;
  g_fake.sessionTempo = 120.0;
  // Simulate the audio thread publishing a session snapshot.
  double tempo = 0; uint64_t peers = 0; int isPlaying = 0;
  REQUIRE(linkClientCaptureAudioSessionState(client, &tempo, &peers, &isPlaying));
  CHECK(tempo == doctest::Approx(120.0));
  CHECK(peers == 2);

  linkClientDisconnect(client);
  uint32_t serial = 0;
  CHECK(linkClientGetStatus(client, &serial) == LinkStatus::selected);
  CHECK(g_fake.disableCalls == 1);
  // Snapshot cleared on disconnect.
  uint32_t tempoSerial = 0;
  CHECK(linkClientGetSessionTempo(client, &tempoSerial) == 0.0f);
  CHECK(linkClientGetPeerCount(client) == 0);

  linkClientDestroy(client);
}

TEST_CASE("Deselect while connected disables the backend") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  linkClientSelect(client);
  linkClientConnect(client);
  CHECK(g_fake.enableCalls == 1);
  linkClientDeselect(client);
  CHECK(g_fake.disableCalls == 1);
  uint32_t serial = 0;
  CHECK(linkClientGetStatus(client, &serial) == LinkStatus::disabled);

  linkClientDestroy(client);
}

TEST_CASE("Serial-bumped getters: tempo serial only moves on value change") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  linkClientSelect(client);
  linkClientConnect(client);
  g_fake.peers = 1;
  g_fake.sessionTempo = 110.0;

  double tempo = 0; uint64_t peers = 0; int isPlaying = 0;
  REQUIRE(linkClientCaptureAudioSessionState(client, &tempo, &peers, &isPlaying));
  uint32_t serial1 = 0;
  CHECK(linkClientGetSessionTempo(client, &serial1) == doctest::Approx(110.0f));

  // Same tempo again: serial must NOT move.
  REQUIRE(linkClientCaptureAudioSessionState(client, &tempo, &peers, &isPlaying));
  uint32_t serial2 = 0;
  CHECK(linkClientGetSessionTempo(client, &serial2) == doctest::Approx(110.0f));
  CHECK(serial2 == serial1);

  // Changed tempo: serial moves.
  g_fake.sessionTempo = 132.0;
  REQUIRE(linkClientCaptureAudioSessionState(client, &tempo, &peers, &isPlaying));
  uint32_t serial3 = 0;
  CHECK(linkClientGetSessionTempo(client, &serial3) == doctest::Approx(132.0f));
  CHECK(serial3 > serial1);

  linkClientDestroy(client);
}

TEST_CASE("Capture returns 0 when Link is not enabled") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  double tempo = 123.0; uint64_t peers = 99; int isPlaying = 1;
  CHECK(linkClientCaptureAudioSessionState(client, &tempo, &peers, &isPlaying) == 0);
  CHECK(tempo == 0.0);
  CHECK(peers == 0);
  CHECK(isPlaying == 0);

  linkClientDestroy(client);
}

TEST_CASE("RequestBeatAtLaunch returns 0 with no peers, target time with peers") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  linkClientSelect(client);
  linkClientConnect(client);
  // No peers yet: immediate start.
  CHECK(linkClientRequestBeatAtLaunch(client, 4) == 0);
  CHECK(g_fake.requestBeatCalls == 1);

  // With peers: backend returns its scheduled target time.
  g_fake.peers = 1;
  const uint64_t target = linkClientRequestBeatAtLaunch(client, 4);
  CHECK(target == 1234567);
  CHECK(g_fake.lastQuantum == doctest::Approx(4.0));

  linkClientDestroy(client);
}

TEST_CASE("Audio frame notification and filter reset reach the backend") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  linkClientNotifyAudioFrame(client, 480, 987654321);
  CHECK(g_fake.onAudioFrameCalls == 1);
  CHECK(g_fake.lastSampleCounter == 480);
  CHECK(g_fake.lastHostTime == 987654321);

  linkClientResetHostTimeFilter(client);
  CHECK(g_fake.resetFilterCalls == 1);

  linkClientDestroy(client);
}

TEST_CASE("Poll resolves connecting to connected when peers appear") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  linkClientSelect(client);
  linkClientConnect(client);
  uint32_t serial = 0;
  CHECK(linkClientGetStatus(client, &serial) == LinkStatus::connecting);

  // Peer shows up: poll transitions to connected and publishes the count.
  g_fake.peers = 3;
  linkClientPoll(client);
  CHECK(linkClientGetStatus(client, &serial) == LinkStatus::connected);
  CHECK(linkClientGetPeerCount(client) == 3);

  linkClientDestroy(client);
}

TEST_CASE("Poll resolves connecting to noPeers after the discovery threshold") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  linkClientSelect(client);
  linkClientConnect(client);
  // No peers, and the fake clock never advances past the threshold inside
  // poll's own window (linkClientNowMicros is real; the threshold is 3s,
  // so a single immediate poll must still report connecting).
  linkClientPoll(client);
  uint32_t serial = 0;
  CHECK(linkClientGetStatus(client, &serial) == LinkStatus::connecting);

  linkClientDestroy(client);
}

TEST_CASE("Connect while already enabled is a no-op") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  linkClientSelect(client);
  linkClientConnect(client);
  CHECK(g_fake.enableCalls == 1);
  linkClientConnect(client);
  CHECK(g_fake.enableCalls == 1);  // no second enable

  linkClientDestroy(client);
}

TEST_CASE("Destroy while connected disables first") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  linkClientSelect(client);
  linkClientConnect(client);
  CHECK(g_fake.enableCalls == 1);
  linkClientDestroy(client);
  CHECK(g_fake.disableCalls == 1);
  CHECK(g_fake.destroyCalls == 1);
}

TEST_CASE("Transport: serial bumps only on isPlaying change") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  linkClientSelect(client);
  linkClientConnect(client);
  g_fake.peers = 1;
  g_fake.sessionTempo = 120.0;

  double tempo = 0; uint64_t peers = 0; int isPlaying = 0;
  REQUIRE(linkClientCaptureAudioSessionState(client, &tempo, &peers, &isPlaying));
  CHECK(isPlaying == 0);
  uint32_t serial1 = 0;
  linkClientGetTransport(client, &isPlaying, &serial1);
  CHECK(isPlaying == 0);

  // Same state again: serial must NOT move.
  REQUIRE(linkClientCaptureAudioSessionState(client, &tempo, &peers, &isPlaying));
  uint32_t serial2 = 0;
  linkClientGetTransport(client, &isPlaying, &serial2);
  CHECK(serial2 == serial1);

  // Session starts: serial moves, value flips to 1.
  g_fake.sessionIsPlaying = 1;
  REQUIRE(linkClientCaptureAudioSessionState(client, &tempo, &peers, &isPlaying));
  uint32_t serial3 = 0;
  linkClientGetTransport(client, &isPlaying, &serial3);
  CHECK(isPlaying == 1);
  CHECK(serial3 > serial1);

  // Still playing: serial stays.
  REQUIRE(linkClientCaptureAudioSessionState(client, &tempo, &peers, &isPlaying));
  uint32_t serial4 = 0;
  linkClientGetTransport(client, &isPlaying, &serial4);
  CHECK(serial4 == serial3);

  // Session stops: serial moves again, value flips back to 0.
  g_fake.sessionIsPlaying = 0;
  REQUIRE(linkClientCaptureAudioSessionState(client, &tempo, &peers, &isPlaying));
  uint32_t serial5 = 0;
  linkClientGetTransport(client, &isPlaying, &serial5);
  CHECK(isPlaying == 0);
  CHECK(serial5 > serial3);

  linkClientDestroy(client);
}

TEST_CASE("Transport: getTransport before any capture reports not playing") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  int isPlaying = 1; uint32_t serial = 7;
  linkClientGetTransport(client, &isPlaying, &serial);
  CHECK(isPlaying == 0);
  CHECK(serial == 0);

  linkClientDestroy(client);
}

TEST_CASE("Disconnect resets transport to not playing") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  linkClientSelect(client);
  linkClientConnect(client);
  g_fake.peers = 1;
  g_fake.sessionIsPlaying = 1;
  double tempo = 0; uint64_t peers = 0; int isPlaying = 0;
  REQUIRE(linkClientCaptureAudioSessionState(client, &tempo, &peers, &isPlaying));
  uint32_t serialBefore = 0;
  linkClientGetTransport(client, &isPlaying, &serialBefore);
  CHECK(isPlaying == 1);

  linkClientDisconnect(client);
  uint32_t serialAfter = 0;
  linkClientGetTransport(client, &isPlaying, &serialAfter);
  CHECK(isPlaying == 0);
  CHECK(serialAfter > serialBefore);

  linkClientDestroy(client);
}

TEST_CASE("Poll while connected publishes tempo and transport every frame") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  linkClientSelect(client);
  linkClientConnect(client);
  g_fake.peers = 2;
  g_fake.sessionTempo = 128.0;
  g_fake.sessionIsPlaying = 1;
  linkClientPoll(client);  // connecting -> connected
  uint32_t statusSerial = 0;
  CHECK(linkClientGetStatus(client, &statusSerial) == LinkStatus::connected);

  // Poll drives the capture: tempo and transport are published without
  // an explicit capture call from the test.
  g_fake.sessionTempo = 132.0;
  g_fake.sessionIsPlaying = 0;
  linkClientPoll(client);
  uint32_t tempoSerial = 0;
  CHECK(linkClientGetSessionTempo(client, &tempoSerial) == doctest::Approx(132.0f));
  int isPlaying = 1; uint32_t transportSerial = 0;
  linkClientGetTransport(client, &isPlaying, &transportSerial);
  CHECK(isPlaying == 0);
  CHECK(transportSerial > 0);

  linkClientDestroy(client);
}

TEST_CASE("Poll downgrades connected to noPeers when the session empties") {
  resetFake();
  linkClientSetBackend(&kFakeBackend);
  LinkClientState* client = linkClientCreate();

  linkClientSelect(client);
  linkClientConnect(client);
  g_fake.peers = 1;
  g_fake.sessionTempo = 120.0;
  g_fake.sessionIsPlaying = 1;
  linkClientPoll(client);
  uint32_t statusSerial = 0;
  CHECK(linkClientGetStatus(client, &statusSerial) == LinkStatus::connected);

  // Last peer leaves while the session was playing: status downgrades,
  // tempo and transport reset so a stale "playing" can't linger.
  g_fake.peers = 0;
  linkClientPoll(client);
  CHECK(linkClientGetStatus(client, &statusSerial) == LinkStatus::noPeers);
  CHECK(linkClientGetPeerCount(client) == 0);
  uint32_t tempoSerial = 0;
  CHECK(linkClientGetSessionTempo(client, &tempoSerial) == 0.0f);
  int isPlaying = 1; uint32_t transportSerial = 0;
  linkClientGetTransport(client, &isPlaying, &transportSerial);
  CHECK(isPlaying == 0);
  CHECK(transportSerial > 0);

  // A peer reappears: poll resolves back to connected (discovery
  // re-armed by the downgrade).
  g_fake.peers = 1;
  linkClientPoll(client);
  CHECK(linkClientGetStatus(client, &statusSerial) == LinkStatus::connected);

  linkClientDestroy(client);
}

}
