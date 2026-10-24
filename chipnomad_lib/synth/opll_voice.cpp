#include "native_chip_gain.h"
#include "opll_voice.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "../native_fm_values.h"

void OPLLVoice::init(float sampleRate) {
  constexpr double pi = 3.14159265358979323846264338327950288;
  const double rate = sampleRate >= 8000 ? sampleRate : 48000;
  ratio_ = double(chip_.sample_rate(3579545)) / rate;
  // Causal, 24-tap windowed-sinc low pass; retains history and fractional phase.
  // 64 fractional phases; coefficients are prepared off the audio callback.
  const double cutoff = .46 * std::min(1.0, 1.0 / ratio_);
  for (int phase = 0; phase < 64; ++phase) {
    double sum = 0;
    for (int tap = 0; tap < 24; ++tap) {
      const double x = tap - 11.0 + phase / 64.0;
      const double sinc = std::abs(x) < 1e-9 ? 2 * cutoff : std::sin(2 * pi * cutoff * x) / (pi * x);
      filter_[phase][tap] = float(sinc * (.5 + .5 * std::cos(pi * x / 12.0)));
      sum += filter_[phase][tap];
    }
    for (float& c : filter_[phase]) c /= sum;
  }
  amp_.init(rate); chip_.reset(); configured_ = false; kill();
}
void OPLLVoice::write(int address, int value) { chip_.write_address(address); chip_.write_data(value); }
void OPLLVoice::configure(const InstrumentOPLL* patch, float cents, float gain) {
  if (!patch) return;
  if (!configured_ || std::memcmp(patch_.patch, patch->patch, 8)) {
    // Program zero reproduces the complete saved tone, independently of the
    // installed preset library. OPLL and VRC7 use their own pinned tone bytes.
    for (int i = 0; i < 8; ++i) write(i, patch->patch[i]);
    write(0x30, 0); configured_ = true;macroBrightness_=999;macroFeedback_=-1;
  }
  patch_ = *patch;
  cents_ = std::isfinite(cents) ? cents + patch_.fineTune : 6000;
  gain_ = std::clamp(gain, 0.0f, 1.0f);
  tone();
  amp_.configure(patch_.amp, gain_);
  pitch();
}
void OPLLVoice::tone() {
  macros();
  const int brightness=std::clamp(int(patch_.tone.brightness),-63,63);
  const int feedback=patch_.tone.feedback?std::min(7,int(patch_.tone.feedback)-1):(patch_.patch[3]&7);
  if(brightness!=macroBrightness_||memcmp(macroLevels_,patch_.tone.operatorLevel,6)){
    write(2,(patch_.patch[2]&0xc0)|std::clamp((patch_.tone.operatorLevel[0]?64-patch_.tone.operatorLevel[0]:(patch_.patch[2]&63))-brightness,0,63));
    write(0x30,std::clamp((patch_.tone.operatorLevel[1]?16-int(patch_.tone.operatorLevel[1]):0),0,15));
    memcpy(macroLevels_,patch_.tone.operatorLevel,6);macroBrightness_=brightness;
  }
  if(feedback!=macroFeedback_){write(3,(patch_.patch[3]&0xf8)|feedback);macroFeedback_=feedback;}
}
void OPLLVoice::pitch() {
  const double hz = 440 * std::exp2((std::clamp(cents_, 0.0f, 14000.0f) - 6900) / 1200.0);
  double fnum = hz * 524288.0 / chip_.sample_rate(3579545);
  int block = 0;
  while (fnum > 511 && block < 7) { fnum *= .5; ++block; }
  int f = std::clamp(int(std::lround(fnum)), 1, 511);
  int low = f & 255, high = (f >> 8) | (block << 1) | (gated_ ? 0x10 : 0);
  if (low != lastLow_) { write(0x10, low); lastLow_ = low; }
  if (high != lastHigh_) { write(0x20, high); lastHigh_ = high; }
}
void OPLLVoice::noteOn() {
  amp_.noteOn(); gated_ = false; pitch(); pendingKeyOn_ = true; active_ = true; silence_ = 0;
}
void OPLLVoice::noteOff() { amp_.noteOff(); pendingKeyOn_ = false; gated_ = false; pitch(); }
void OPLLVoice::kill() {
  amp_.kill();
  pendingKeyOn_ = false; gated_ = false; active_ = false; lastLow_ = lastHigh_ = -1;
  write(0x20, 0); phase_ = 0; silence_ = 0; level_ = 0;
  std::memset(history_, 0, sizeof(history_)); historyPosition_ = 0;
}
float OPLLVoice::nextNative() {
  ymfm::ym2413::output_data output;
  // Channel 0 carries this isolated chord voice; rhythm mode stays disabled.
  chip_.generate(&output, 1, 1u);
  // Clock key-off before asserting a retrigger: ymfm samples key state at native ticks.
  if (pendingKeyOn_) { pendingKeyOn_ = false; gated_ = true; pitch(); }
  const float sample = (output.data[0] + output.data[1]) / 32768.0f;
  // Only retire after the native digital output has been exactly silent for
  // a full second following key-off, never based on the user's volume.
  if (!gated_ && sample == 0) ++silence_; else silence_ = 0;
  return sample;
}
void OPLLVoice::render(float* output, size_t frames) {
  if (!output) return;
  for (size_t i = 0; i < frames; ++i) {
    if (!active_) { output[i] = 0; continue; }
    phase_ += ratio_;
    while (phase_ >= 1) {
      phase_ -= 1;
      historyPosition_ = (historyPosition_ + 1) & 31;
      history_[historyPosition_] = nextNative();
    }
    const int phase = std::min(63, int(phase_ * 64));
    float sample = 0;
    for (int tap = 0; tap < 24; ++tap) sample += history_[(historyPosition_ - tap) & 31] * filter_[phase][tap];
    output[i] = amp_.process(sample * nativeChipGain(InstrumentType::OPLL));
    level_ = std::max(std::abs(output[i]), level_ * .999f);
    if (silence_ > chip_.sample_rate(3579545)) kill();
  }
}

void OPLLVoice::macros() {
  const auto& t=patch_.tone;
  if(macroBrightness_!=999&&!memcmp(&directCache_,&t.direct,sizeof(directCache_)))return;
  for(int op=0;op<2;++op) {

    const int ratio=nativeFMValue(t,op,fxOMU,patch_.patch[op]&15);
    write(op,(patch_.patch[op]&0xf0)|ratio);
    write(4+op,(nativeFMValue(t,op,fxOAR,patch_.patch[4+op]>>4)<<4)|nativeFMValue(t,op,fxODR,patch_.patch[4+op]&15));
    write(6+op,(nativeFMValue(t,op,fxOSL,patch_.patch[6+op]>>4)<<4)|nativeFMValue(t,op,fxORR,patch_.patch[6+op]&15));
  }
  directCache_=t.direct;
}
