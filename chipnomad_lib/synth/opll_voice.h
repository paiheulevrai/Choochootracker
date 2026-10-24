#pragma once
#include "native_fm_amp.h"
#include "../project_instruments.h"
#include "../external/ymfm/ymfm_opl.h"
#include <cstddef>

// One context per bounded tracker chord slot. Construction stays off callback.
class OPLLVoice {
 public:
  OPLLVoice() : chip_(interface_) {}
  void init(float sampleRate);
  void configure(const InstrumentOPLL* patch, float cents, float gain);
  void noteOn();
  void noteOff();
  void kill();
  void render(float* output, size_t frames);
  bool active() const { return active_; }
  float envelopeLevel() const { return active_ ? level_ : 0; }
 private:
  void write(int address, int value);
  void pitch();
  void tone();
  void macros();
  NativeFMValues directCache_{};
  uint8_t macroLevels_[6]{};
  int macroBrightness_=999,macroFeedback_=-1;
  float nextNative();
  ymfm::ymfm_interface interface_;
  ymfm::ym2413 chip_;
  NativeFMAmp amp_;
  InstrumentOPLL patch_{};
  double ratio_ = 1, phase_ = 0;
  float filter_[64][24]{}, history_[32]{};
  int historyPosition_ = 0;
  float cents_ = 6000, gain_ = 1, level_ = 0;
  unsigned silence_ = 0;
  bool active_ = false, gated_ = false, configured_ = false;
  bool pendingKeyOn_=false;
  int lastLow_ = -1, lastHigh_ = -1;
};
