#pragma once

#include <stdint.h>

struct InstrumentSample;

// Spectral-flux transient detection for AUTO slice mode (plan §5).
//
// Detects up to `maxCount` onsets inside the sample's loop region and
// writes their frame positions to outFrames[0..*outCount-1] in ascending
// order. `sensitivity` is 1..99 (higher = more permissive: more onsets
// pass the adaptive threshold). Returns 0 on success, nonzero when the
// sample cannot be analysed (no data, empty region, too short). The
// sample is never modified; the caller owns the bounds array.
//
// The function allocates scratch buffers (FFT plans, window, flux array),
// so it must never run on the audio thread - the editor screen runs it
// under audioManager.pause()/resume().
int sampleDetectTransients(const InstrumentSample* sample,
                           uint32_t loopStart, uint32_t loopEnd,
                           uint8_t maxCount, uint8_t sensitivity,
                           uint32_t* outFrames, uint8_t* outCount);
