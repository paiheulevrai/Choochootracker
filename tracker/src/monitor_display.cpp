#include "monitor_display.h"
#include "audio_monitor.h"
#include "common.h"
#include <algorithm>
#include <cstring>

static AudioMonitorSnapshot snapshot;
static float meterPeaks[PROJECT_MAX_TRACKS][2];
static bool active;

void monitorDisplayInit(void) {
  snapshot = {};
  memset(meterPeaks, 0, sizeof(meterPeaks));
  active = false;
}

void monitorDisplaySetActive(bool value) {
  if (active == value) return;
  active = value;
  snapshot = {};
  memset(meterPeaks, 0, sizeof(meterPeaks));
  if (chipnomadState && chipnomadState->audioMonitor)
    chipnomadState->audioMonitor->setEnabled(value);
}

bool monitorDisplayActive(void) { return active; }

void monitorDisplayUpdate(void) {
  if (!active || !chipnomadState || !chipnomadState->audioMonitor) return;
  const bool fresh = chipnomadState->audioMonitor->receive(snapshot);
  for (int t = 0; t < PROJECT_MAX_TRACKS; ++t)
    for (int c = 0; c < 2; ++c)
      meterPeaks[t][c] = std::max(meterPeaks[t][c] * 0.88f, fresh ? snapshot.peaks[t][c] : 0.0f);
}

const float* monitorDisplayTrackSamples(int track) {
  return track >= 0 && track < PROJECT_MAX_TRACKS ? snapshot.tracks[track] : nullptr;
}

const float* monitorDisplayMixSamples(void) { return snapshot.mix; }
const float (*monitorDisplayMixStereoSamples(void))[2] { return snapshot.mixStereo; }
float monitorDisplayTrackPeak(int track, int channel) {
  return track >= 0 && track < PROJECT_MAX_TRACKS && channel >= 0 && channel < 2 ? meterPeaks[track][channel] : 0;
}
