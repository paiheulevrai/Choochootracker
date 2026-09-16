#pragma once

#include <atomic>
#include <stdint.h>

// Android-only diagnostic counters. The callback never logs or waits on a lock.
namespace audioDiagnostics {
static_assert(std::atomic<uint32_t>::is_always_lock_free, "Audio counters must be lock-free");
inline bool enabled = false;
inline std::atomic<uint32_t> invalidBuffers{0};
inline std::atomic<uint32_t> renderFailures{0};
}
