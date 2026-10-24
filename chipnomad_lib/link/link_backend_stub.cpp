// No-op Ableton Link backend for platforms without Link support (Web,
// Miyoo, RG35xx). Compiled when none of DESKTOP_BUILD / PORTMASTER_BUILD
// / ANDROID_BUILD is defined; the real backend (link_backend_link.cpp)
// covers the rest. Every function is a harmless no-op so the client
// layer (link_client.cpp) compiles and runs unchanged.

#include "link_backend.h"

#if !defined(DESKTOP_BUILD) && !defined(PORTMASTER_BUILD) && !defined(ANDROID_BUILD)

namespace {

struct StubState {
  int unused;
};

void* linkBackendCreate(void*) {
  StubState* state = new StubState();
  state->unused = 0;
  return state;
}

void linkBackendDestroy(void*, void* state) {
  delete (StubState*)state;
}

int linkBackendEnable(void*, void*) { return 0; }
void linkBackendDisable(void*, void*) {}
int linkBackendIsEnabled(void*, void*) { return 0; }
int linkBackendNumPeers(void*, void*) { return 0; }

int linkBackendCaptureAudioSessionState(void*, void*, double* tempo, double* beat,
                                        uint64_t* capturedAtMicros, uint64_t* peers, int* isPlaying) {
  if (tempo) *tempo = 0.0;
  if (beat) *beat = 0.0;
  if (capturedAtMicros) *capturedAtMicros = 0;
  if (peers) *peers = 0;
  if (isPlaying) *isPlaying = 0;
  return 0;
}

uint64_t linkBackendRequestBeatAtLaunch(void*, void*, double) { return 0; }
void linkBackendOnAudioFrame(void*, void*, uint64_t, uint64_t) {}
void linkBackendResetHostTimeFilter(void*, void*) {}

}  // namespace

static const LinkBackend kLinkStubBackend = {
  nullptr,  // userdata
  linkBackendCreate,
  linkBackendDestroy,
  linkBackendEnable,
  linkBackendDisable,
  linkBackendIsEnabled,
  linkBackendNumPeers,
  linkBackendCaptureAudioSessionState,
  linkBackendRequestBeatAtLaunch,
  linkBackendOnAudioFrame,
  linkBackendResetHostTimeFilter,
};

const LinkBackend* linkBackendRealGet(void) {
  return &kLinkStubBackend;
}

#endif  // !DESKTOP_BUILD && !PORTMASTER_BUILD && !ANDROID_BUILD
