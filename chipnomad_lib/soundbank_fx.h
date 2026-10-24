#pragma once
#include <algorithm>
#include <cmath>
#include "playback.h"

inline int soundbankFXCutoff(int value) {
  return int(std::lround(20 * std::pow(1000.0, std::clamp(value, 0, 254) / 254.0)));
}
inline int soundbankFXTime(int value) {
  const double x = std::clamp(value, 0, 254) / 254.0;
  return int(std::lround(20000 * x * x));
}
inline SoundbankControls soundbankFXControls(const SoundbankControls& base,
                                            const PlaybackFXState* fx) {
  auto result = base;
  auto active = [&](int id) { return fx[id].isOn && fx[id].fxValue != 255; };
  if (fx[fxFPT].isOn) result.tuneCents = (std::clamp(int(fx[fxFPT].fxValue), 0, 48) - 24) * 100;
  if (active(fxFCF)) result.cutoffHz = soundbankFXCutoff(fx[fxFCF].fxValue);
  if (active(fxFRS)) result.resonanceTenths = (fx[fxFRS].fxValue * 400 + 127) / 254;
  if (active(fxFAT)) result.attackMs = soundbankFXTime(fx[fxFAT].fxValue);
  if (active(fxFDC)) result.decayMs = soundbankFXTime(fx[fxFDC].fxValue);
  if (active(fxFRL)) result.releaseMs = soundbankFXTime(fx[fxFRL].fxValue);
  if (active(fxFHD)) result.holdMs = soundbankFXTime(fx[fxFHD].fxValue);
  if (active(fxFSU)) result.sustainPercent = (fx[fxFSU].fxValue * 100 + 127) / 254;
  return result;
}
