#pragma once

// UI-owned telemetry; update once before drawing the current frame.
void monitorDisplayInit(void);
void monitorDisplaySetActive(bool active);
bool monitorDisplayActive(void);
void monitorDisplayUpdate(void);
const float* monitorDisplayTrackSamples(int track);
const float* monitorDisplayMixSamples(void);
const float (*monitorDisplayMixStereoSamples(void))[2];
float monitorDisplayTrackPeak(int track, int channel);

// Horizontal 1-row meter for the audio input settings screen (Phase 4).
// Reads from the AudioInputManager levels snapshot for the given armed slot.
void monitorDisplayDrawHorizontalMeter(int armedSlot, int col, int row, int width);

// Fetches the latest audio-input levels snapshot. The first call each frame
// consumes the published snapshot; later calls return the cached copy.
// *fresh is set to 1 when a new snapshot arrived this frame, else 0.
// Returns true when data is available.
struct AudioInputLevels;
bool monitorDisplayInputLevels(AudioInputLevels* out, int* fresh);
