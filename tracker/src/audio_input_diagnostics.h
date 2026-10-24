#ifndef __AUDIO_INPUT_DIAGNOSTICS_H__
#define __AUDIO_INPUT_DIAGNOSTICS_H__

// Lock-free diagnostic counters for the audio input recording pipeline.
// Mirrors tracker/platforms/android/audio_diagnostics.h: atomics only, no
// locks, safe to bump from the SDL capture callback and the writer thread.
#include <atomic>
#include <stdint.h>

namespace audioInputDiagnostics {
static_assert(std::atomic<uint32_t>::is_always_lock_free, "Audio input counters must be lock-free");

inline bool enabled = false; // Only the diagnostics screen flips this on

// Capture side (SDL capture callback / capture thread)
inline std::atomic<uint32_t> captureUnderruns{0};   // Device delivered silence
inline std::atomic<uint32_t> captureOverflow{0};    // Ring full, frames dropped
inline std::atomic<uint32_t> captureDropped{0};     // Frames dropped (any cause)
inline std::atomic<uint32_t> writerFailures{0};     // WAV write errors

inline void reset() {
  captureUnderruns.store(0, std::memory_order_relaxed);
  captureOverflow.store(0, std::memory_order_relaxed);
  captureDropped.store(0, std::memory_order_relaxed);
  writerFailures.store(0, std::memory_order_relaxed);
}
} // namespace audioInputDiagnostics

#endif
