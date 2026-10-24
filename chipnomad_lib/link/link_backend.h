#ifndef __CHIPNOMAD_LIB__LINK_BACKEND_H__
#define __CHIPNOMAD_LIB__LINK_BACKEND_H__

#include <stdint.h>
#include "link_client.h"

// Platform-neutral Ableton Link backend vtable, mirroring MidiBackend in
// midi/midi_router.h: the client layer (link_client.cpp) owns musical
// semantics (status state machine, serial-bumped getters, receiver-only
// contract); a backend only talks to the Link library (or stubs it out).
// No ableton:: type appears anywhere in this header, so the client layer
// compiles unchanged on platforms where Link is stubbed out.
//
// The real backend (link_backend_link.cpp) is compiled when
// DESKTOP_BUILD / PORTMASTER_BUILD / ANDROID_BUILD is defined; the stub
// (link_backend_stub.cpp) covers every other platform (Web, Miyoo,
// RG35xx). Exactly one of the two defines linkBackendGet().

struct LinkBackend {
  void* userdata;
  // Lifecycle. create returns an opaque backend state (the real backend
  // holds its ableton::Link and HostTimeFilter there); destroy tears it
  // down. Both are called on the UI thread only.
  void* (*create)(void* userdata);
  void (*destroy)(void* userdata, void* state);
  // Network opt-in / opt-out (UI thread). enable maps to link.enable(),
  // disable to link.disable(). Both are NOT realtime-safe.
  int (*enable)(void* userdata, void* state);   // 1 on success, 0 on failure
  void (*disable)(void* userdata, void* state);
  int (*isEnabled)(void* userdata, void* state);
  // Polling (UI thread, cheap).
  int (*numPeers)(void* userdata, void* state);
  // Session snapshot (App/UI thread; the real backend uses Link's
  // thread-safe captureAppSessionState, NOT the realtime-only audio
  // variant). Fills tempo/beat/peers/isPlaying from the latest session state;
  // returns 1 on success, 0 if Link is not enabled. isPlaying reports the
  // session-wide transport state (start/stop sync must be enabled for
  // peers' transport to propagate).
  // beat is sampled at capturedAtMicros, both on the app steady_clock scale.
  int (*captureAudioSessionState)(void* userdata, void* state, double* tempo, double* beat,
                                  uint64_t* capturedAtMicros, uint64_t* peers, int* isPlaying);
  // Requests a local beat-0 alignment at (now + outputLatencyMicros +
  // schedulingHeadroomMicros) and returns the absolute host time in
  // microseconds at which playback should begin (the quantized beat-0
  // time on the app's steady_clock scale), or 0 when there are no peers
  // (immediate start). quantum is in beats (4.0 for 4/4).
  uint64_t (*requestBeatAtLaunch)(void* userdata, void* state, double quantum);
  // Feeds the backend's HostTimeFilter with one (sampleCounter,
  // hostTimeMicros) pair from the audio callback entry. Stub ignores it.
  void (*onAudioFrame)(void* userdata, void* state, uint64_t sampleCounter, uint64_t hostTimeMicros);
  // Resets the HostTimeFilter window (enable, sample-rate or buffer-size
  // change). Stub ignores it.
  void (*resetHostTimeFilter)(void* userdata, void* state);
};

// Registers the active backend. Global rather than per-client-state:
// there is one Link session per process regardless of how many
// LinkClientState/ChipNomadState instances exist (mirrors
// midiRouterSetBackend). Platform init calls this once at startup; tests
// call it with a fake backend per test case. Passing nullptr restores
// the built-in default (real backend where compiled in, stub elsewhere).
void linkClientSetBackend(const LinkBackend* backend);

#endif // __CHIPNOMAD_LIB__LINK_BACKEND_H__
