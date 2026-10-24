#pragma once
#include "native_fm_amp.h"
#include "../four_op_patch.h"
#include "../external/ymfm/ymfm_opn.h"
#include "../external/ymfm/ymfm_opm.h"
#include "native_resampler.h"
// One isolated native chip per bounded tracker chord slot; no instrument-owned DSP.
class FourOpVoice {
 public:
  FourOpVoice():opn_(opnInterface_),opm_(opmInterface_){}
  void init(float rate);
  void configure(InstrumentType type,const InstrumentFourOp* patch,float cents,float gain);
  void noteOn();void noteOff();void kill();void render(float* stereo,size_t frames);
  bool active()const{return active_;}float envelopeLevel()const{return active_?level_:0;}
 private:
  void tone();
  void macros();
  NativeFMValues directCache_{};
  uint8_t macroLevels_[6]{};
  int macroBrightness_=999,macroFeedback_=-1;
  void write(unsigned reg,unsigned value);void applyPatch();void pitch();void key(bool on);void native(float& l,float& r);
  ymfm::ymfm_interface opnInterface_,opmInterface_;
  ymfm::ym2612 opn_;ymfm::ym2151 opm_;
  NativeResampler opnResampler_,opmResampler_;
  NativeFMAmp amp_;
  InstrumentFourOp patch_{};InstrumentType type_=InstrumentType::GenesisFM;
  float cents_=6000,gain_=1,level_=0;bool active_=false,gated_=false,configured_=false;
  float dcCoefficient_=0,dcInput_[2]{},dcOutput_[2]{};
  bool pendingKeyOn_=false;
  int pitchCache_=-1;unsigned silent_=0;
};
