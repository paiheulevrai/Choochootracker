// Real Ableton Link backend: wraps <ableton/Link.hpp> behind the
// LinkBackend vtable (see link_backend.h). Compiled only on platforms
// that ship Link (DESKTOP_BUILD / PORTMASTER_BUILD / ANDROID_BUILD);
// every other platform gets link_backend_stub.cpp instead.
//
// Shared tempo and optional transport use Link's App-thread session API.

#include "link_backend.h"
#include "link_backend_internal.h"
#include "host_time_filter.h"

#if defined(DESKTOP_BUILD) || defined(PORTMASTER_BUILD) || defined(ANDROID_BUILD)

#ifdef DESKTOP_BUILD
#ifdef LINK_PLATFORM_WINDOWS
// Windows headers define "interface", which collides with Link Audio identifiers.
#include <ableton/platforms/Config.hpp>
#undef interface
#endif
#include <ableton/LinkAudio.hpp>
#else
#include <ableton/Link.hpp>
#endif

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#ifdef DESKTOP_BUILD
#include <cstring>
#include <memory>
#include <optional>
#endif

#ifdef ANDROID_BUILD
// T28: Android needs a WifiManager MulticastLock before Link's mDNS
// discovery can receive multicast packets. Declared here, implemented in
// tracker/platforms/android/link_backend_android.cpp (JNI).
extern "C" int linkBackendAndroidAcquireMulticastLock(void);
extern "C" int linkBackendAndroidReleaseMulticastLock(void);
#endif

namespace {

// HostTimeFilter lives in host_time_filter.h (header-only so the
// unit test exercises the exact code the backend runs).


struct RealLinkState {
#ifdef DESKTOP_BUILD
  ableton::LinkAudio link{125.0, "ChooChooTracker"};
  std::unique_ptr<ableton::LinkAudioSink> audioSink;
  std::atomic<bool> audioEnabled{false};
  std::optional<ableton::LinkAudio::SessionState> audioSession;
  std::chrono::microseconds audioTime{0};
#else
  ableton::Link link{125.0};
#endif
  HostTimeFilter hostTimeFilter;
  // Latest sample counter reported by the audio thread; the launch-time
  // estimator maps it through the regression to a host time.
  std::atomic<uint64_t> lastSampleCounter{0};
  bool transportSync = true;
  // Headroom added on top of the measured output latency when scheduling
  // a beat-0 alignment (see linkClientRequestBeatAtLaunch). 10 ms covers
  // callback scheduling jitter on the platforms we ship.
  static constexpr uint64_t kSchedulingHeadroomMicros = 10000;
};

void* linkBackendCreate(void*) {
  RealLinkState* state = new RealLinkState();
  return state;
}

void linkBackendDestroy(void*, void* state) {
  delete (RealLinkState*)state;
}

int linkBackendEnable(void*, void* state) {
  RealLinkState* s = (RealLinkState*)state;
#ifdef ANDROID_BUILD
  // MulticastLock must be held before Link opens its sockets, or mDNS
  // discovery stays deaf on Android (T28).
  linkBackendAndroidAcquireMulticastLock();
#endif
  s->hostTimeFilter.reset();
  // Follow the session-wide transport (peers' start/stop propagate to
  // us). Without this, Link ignores start/stop state entirely and the
  // app can never observe session transport changes.
  s->link.enableStartStopSync(s->transportSync);
  s->link.enable(true);
#ifdef DESKTOP_BUILD
  if (s->audioEnabled.load(std::memory_order_acquire)) s->link.enableLinkAudio(true);
#endif
  return 1;
}

void linkBackendDisable(void*, void* state) {
  RealLinkState* s = (RealLinkState*)state;
#ifdef DESKTOP_BUILD
  s->link.enableLinkAudio(false);
#endif
  s->link.enable(false);
  s->link.enableStartStopSync(false);
  s->hostTimeFilter.reset();
#ifdef ANDROID_BUILD
  linkBackendAndroidReleaseMulticastLock();
#endif
}

int linkBackendIsEnabled(void*, void* state) {
  RealLinkState* s = (RealLinkState*)state;
  return s->link.isEnabled() ? 1 : 0;
}

int linkBackendNumPeers(void*, void* state) {
  RealLinkState* s = (RealLinkState*)state;
  return (int)s->link.numPeers();
}

int linkBackendCaptureAudioSessionState(void*, void* state, double* tempo, double* beat,
                                        uint64_t* capturedAtMicros, uint64_t* peers, int* isPlaying) {
  RealLinkState* s = (RealLinkState*)state;
  if (!s->link.isEnabled()) return 0;
  // App-thread variant: thread-safe (mutex-guarded), so this is legal
  // from the UI thread. The realtime-only captureAudioSessionState must
  // only be called from the audio thread.
  const auto sessionState = s->link.captureAppSessionState();
  // Sample Link's phase and the app clock together. The caller can then
  // interpolate that phase back to an audio tick without UI-frame jitter.
  const auto steadyNow = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::steady_clock::now().time_since_epoch());
  const auto rawNow = s->link.clock().micros();
  if (tempo) *tempo = sessionState.tempo();
  if (beat) *beat = sessionState.beatAtTime(rawNow, 4.0);
  if (capturedAtMicros) *capturedAtMicros = (uint64_t)steadyNow.count();
  if (peers) *peers = (uint64_t)s->link.numPeers();
  if (isPlaying) *isPlaying = sessionState.isPlaying() ? 1 : 0;
  return 1;
}

uint64_t linkBackendRequestBeatAtLaunch(void*, void* state, double quantum) {
  RealLinkState* s = (RealLinkState*)state;
  if (!s->link.isEnabled()) return 0;

  // Clock-scale bridge: Link's timeline lives on its platform clock
  // (CLOCK_MONOTONIC_RAW on Linux), while the app schedules playback on
  // steady_clock. Measure both at the same instant and define
  //   raw = steady + skew  <=>  steady = raw - skew
  // so targets can be converted between the two scales.
  const auto steadyNow = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::steady_clock::now().time_since_epoch());
  const auto rawNow = s->link.clock().micros();
  const int64_t skew = rawNow.count() - steadyNow.count();

  // Estimate the host time of "now" on the app's steady_clock scale: the
  // HostTimeFilter maps the audio sample counter to host time, so ask it
  // for the current counter. The audio thread feeds pairs every callback
  // (see linkBackendOnAudioFrame); the latest sample counter is the one
  // it last reported.
  uint64_t steadyEstimate = 0;
  if (!s->hostTimeFilter.estimate(s->lastSampleCounter.load(std::memory_order_relaxed), &steadyEstimate)) {
    // No fit yet (callback hasn't run twice since enable): fall back to
    // the wall clock, which is the same steady_clock scale the filter is
    // fed with.
    steadyEstimate = (uint64_t)steadyNow.count();
  }
  const uint64_t steadyTarget =
    steadyEstimate + outputLatencyMicros() + RealLinkState::kSchedulingHeadroomMicros;

  // App-thread capture/commit: thread-safe, legal from the UI thread.
  auto sessionState = s->link.captureAppSessionState();
  // Quantized launch (LOCAL alignment): requestBeatAtTime maps beat 0 to
  // the next session phase boundary at/after the target time on THIS
  // client's timeline. With peers present it never re-maps the session's
  // beat/time relationship (no tempo push, no transport push); the
  // commit below only publishes our own timeline adjustment.
  const auto rawTarget = std::chrono::microseconds((int64_t)steadyTarget + skew);
  sessionState.requestBeatAtTime(0.0, rawTarget, quantum);
  s->link.commitAppSessionState(sessionState);

  // Return the quantized beat-0 time (NOT the raw input target, which
  // can be up to a quantum earlier than the actual boundary), converted
  // back to the app's steady_clock scale for
  // chipnomadQueuePlaybackStartSongAtTime. Clamp so we never schedule
  // before the target.
  const auto rawLaunch = sessionState.timeAtBeat(0.0, quantum);
  int64_t steadyLaunch = rawLaunch.count() - skew;
  if (steadyLaunch < (int64_t)steadyTarget) steadyLaunch = (int64_t)steadyTarget;
  return (uint64_t)steadyLaunch;
}

int linkBackendSetTempo(void*, void* state, double bpm) {
  auto* s = (RealLinkState*)state;
  auto session = s->link.captureAppSessionState();
  session.setTempo(bpm, s->link.clock().micros());
  s->link.commitAppSessionState(session);
  return 1;
}

void linkBackendSetTransportSync(void*, void* state, int enabled) {
  auto* s = (RealLinkState*)state;
  s->transportSync = enabled != 0;
  s->link.enableStartStopSync(s->transportSync);
}

void linkBackendSetPlaying(void*, void* state, int playing, uint64_t atMicros) {
  auto* s = (RealLinkState*)state;
  if (!s->link.isEnabled() || !s->transportSync) return;
  const auto steadyNow = std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::steady_clock::now().time_since_epoch());
  const auto rawNow = s->link.clock().micros();
  const auto when = atMicros ? std::chrono::microseconds((int64_t)atMicros + rawNow.count() - steadyNow.count()) : rawNow;
  auto session = s->link.captureAppSessionState();
  session.setIsPlaying(playing != 0, when);
  s->link.commitAppSessionState(session);
}

void linkBackendOnAudioFrame(void*, void* state, uint64_t sampleCounter, uint64_t hostTimeMicros) {
  RealLinkState* s = (RealLinkState*)state;
  s->lastSampleCounter.store(sampleCounter, std::memory_order_relaxed);
  s->hostTimeFilter.add(sampleCounter, hostTimeMicros);
#ifdef DESKTOP_BUILD
  s->audioSession.reset();
  if (s->audioEnabled.load(std::memory_order_acquire) && s->link.isEnabled()) {
    s->audioSession.emplace(s->link.captureAudioSessionState());
    s->audioTime = s->link.clock().micros() + std::chrono::microseconds(outputLatencyMicros());
  }
#endif
}

void linkBackendResetHostTimeFilter(void*, void* state) {
  RealLinkState* s = (RealLinkState*)state;
  s->hostTimeFilter.reset();
}

#ifdef DESKTOP_BUILD
int linkBackendSetAudioEnabled(void*, void* state, int enabled) {
  auto* s = (RealLinkState*)state;
  if (enabled && !s->audioSink)
    s->audioSink.reset(new ableton::LinkAudioSink(s->link, "Master", 4096));
  if (!enabled) s->audioEnabled.store(false, std::memory_order_release);
  s->link.enableLinkAudio(enabled && s->link.isEnabled());
  if (enabled) s->audioEnabled.store(true, std::memory_order_release);
  return 1;
}

void linkBackendSendAudio(void*, void* state, const int16_t* samples, int frames, int sampleRate) {
  auto* s = (RealLinkState*)state;
  if (!s->audioEnabled.load(std::memory_order_acquire) || !s->audioSession) return;
  ableton::LinkAudioSink::BufferHandle buffer(*s->audioSink);
  if (!buffer) return;
  const size_t count = static_cast<size_t>(frames) * 2;
  if (count > buffer.maxNumSamples) return;
  std::memcpy(buffer.samples, samples, count * sizeof(*samples));
  constexpr double quantum = 4.0;
  const double beat = s->audioSession->beatAtTime(s->audioTime, quantum);
  buffer.commit(*s->audioSession, beat, quantum, frames, 2, sampleRate);
}
#endif

// outputLatencyMicros() is declared in link_backend_internal.h. The
// weak default definition lives in link_client.cpp; corelib_audio
// overrides it with the measured device latency (T39).

}  // namespace

static const LinkBackend kLinkBackend = {
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
  linkBackendSetTempo,
  linkBackendSetTransportSync,
  linkBackendSetPlaying,
#ifdef DESKTOP_BUILD
  linkBackendSetAudioEnabled,
  linkBackendSendAudio,
#else
  nullptr,
  nullptr,
#endif
};

const LinkBackend* linkBackendRealGet(void) {
  return &kLinkBackend;
}

#else  // !DESKTOP_BUILD && !PORTMASTER_BUILD && !ANDROID_BUILD

// Keep translation units non-empty on stub platforms so pedantic builds
// that dislike TU-less objects stay quiet.
typedef int linkBackendRealUnusedType;

#endif
