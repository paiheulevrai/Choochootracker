#pragma once
#include <cstdint>

// Instrument-owned overrides. -1 preserves each zone's bank-authored value.
// Keep this POD: Instrument is a serialized, memcpy-able union.
struct SoundbankControls {
  int16_t tuneCents, cutoffHz, resonanceTenths;
  int16_t delayMs, attackMs, holdMs, decayMs, sustainPercent, releaseMs;
  // Zero retains the original project-dependent PBN scale in older songs.
  int16_t bendStepCents;
};
inline SoundbankControls soundbankDefaultControls() {
  return {0, -1, -1, -1, -1, -1, -1, -1, -1, 1};
}
