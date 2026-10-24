#pragma once
#include "../project_instruments.h"
// Fixed voice-output calibration from the public 1,196-preset inventory.
// Baseline: personal/r36h 9aba4f0, native wrappers before this calibration.
// 96 kHz, MIDI 48/60, gain .9, native envelopes; README_MEASURE.md policy:
// median K-weighted loudness vs PCM, [-12,+6] dB, 1 dB p95 peak headroom.
// OPLL/VRC7 share a voice adapter and pooled calibration. This is not AGC.
inline constexpr float nativeChipGain(InstrumentType type) {
  switch(type) {
    case InstrumentType::OPLL: return 1.995262315f;
    case InstrumentType::VRC7: return 1.995262315f;
    case InstrumentType::OPL2: return 1.995262315f;
    case InstrumentType::OPL3: return 1.995262315f;
    case InstrumentType::GenesisFM: return 1.995262315f;
    case InstrumentType::ArcadeFM: return 1.995262315f;
    case InstrumentType::DX7: return 1.995262315f;
    case InstrumentType::SID: return 1.995262315f;
    case InstrumentType::SegaPSG: return 0.997248011f;
    case InstrumentType::GBPulse: return 1.995262315f;
    case InstrumentType::GBNoise: return 1.995262315f;
    default: return 1.f;
  }
}
