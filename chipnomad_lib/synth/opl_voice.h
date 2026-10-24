#pragma once
#include "native_fm_amp.h"
#include "../project_instruments.h"
#include "../external/ymfm/ymfm_opl.h"
#include "native_resampler.h"
class OPLVoice {
 public:
  OPLVoice():opl2_(interface2_),opl3_(interface3_){}
  void init(float rate);
  void configure(InstrumentType type,const InstrumentOPL* patch,float cents,float gain);
  void noteOn(); void noteOff(); void kill();
  void render(float* stereo,size_t frames);
  bool active()const{return active_;} float envelopeLevel()const{return active_?level_:0;}
 private:
  void tone();
  void macros();
  NativeFMValues directCache_{};
  uint8_t macroLevels_[6]{};
  int macroBrightness_=999,macroFeedback_=-1;
  void write(unsigned reg,unsigned value);void applyPatch();void pitch();void native(float& l,float& r);
  ymfm::ymfm_interface interface2_,interface3_;
  ymfm::ym3812 opl2_;ymfm::ymf262 opl3_;
  NativeResampler resampler_;
  NativeFMAmp amp_;
  InstrumentOPL patch_{};
  InstrumentType type_=InstrumentType::OPL2;
  float rate_=48000,cents_=6000,gain_=1,level_=0;
  bool active_=false,gated_=false,configured_=false;
  int low_[2]={-1,-1},high_[2]={-1,-1};
  bool pendingKeyOn_=false;
  unsigned silent_=0;
};
