#ifndef __CHIPNOMAD_LIB__LINK_BACKEND_INTERNAL_H__
#define __CHIPNOMAD_LIB__LINK_BACKEND_INTERNAL_H__

#include "link_backend.h"

// Internal plumbing shared between link_client.cpp and the backend
// translation units. Not part of the public engine API.

// The backend currently registered via linkClientSetBackend (nullptr
// until platform init or a test registers one).
const LinkBackend* linkClientCurrentBackend(void);

// Default backend for this platform: the real Link backend where Link
// is compiled in (DESKTOP_BUILD/PORTMASTER_BUILD/ANDROID_BUILD), the
// no-op stub elsewhere. Defined in link_backend_link.cpp and
// link_backend_stub.cpp respectively.
const LinkBackend* linkBackendRealGet(void);

// Monotonic microsecond clock used for the discovery threshold and the
// HostTimeFilter's host-time samples. Same scale as midiIoNowMicros()
// (std::chrono::steady_clock), so timestamps from both clocks can be
// compared directly.
uint64_t linkClientNowMicros(void);

// Output latency of the audio output device in microseconds, used when
// scheduling a beat-0 alignment ahead of "now". The real value is
// provided by the platform audio setup (T39: corelib_audio overrides the
// weak default in link_backend_link.cpp); tests override it too.
uint64_t outputLatencyMicros(void);

#endif // __CHIPNOMAD_LIB__LINK_BACKEND_INTERNAL_H__
