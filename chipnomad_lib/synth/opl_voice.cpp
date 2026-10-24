#include "native_chip_gain.h"
#include "opl_voice.h"
#include <cstring>
#include "../native_fm_values.h"
void OPLVoice::init(float rate){rate_=rate;amp_.init(rate);opl2_.reset();opl3_.reset();configured_=false;resampler_.init(opl2_.sample_rate(3579545),rate_);kill();}
void OPLVoice::write(unsigned reg,unsigned value){
  if(type_==InstrumentType::OPL3){if(reg&0x100)opl3_.write_address_hi(reg);else opl3_.write_address(reg);opl3_.write_data(value);}
  else{opl2_.write_address(reg);opl2_.write_data(value);}
}
void OPLVoice::configure(InstrumentType type,const InstrumentOPL* patch,float cents,float gain){
  if(!patch)return;
  InstrumentOPL comparable=*patch;comparable.fineTune=patch_.fineTune;comparable.amp=patch_.amp;comparable.tone=patch_.tone;
  bool changed=!configured_||type_!=type||memcmp(&patch_,&comparable,sizeof(*patch));
  if(changed){bool gate=gated_,wasActive=active_;auto oldAmp=amp_;kill();if(wasActive)amp_=oldAmp;type_=type;patch_=*patch;applyPatch();configured_=true;gated_=gate;active_=wasActive;}
  patch_.amp=patch->amp;patch_.tone=patch->tone;patch_.fineTune=patch->fineTune;
  cents_=std::isfinite(cents)?cents+patch_.fineTune:6000;gain_=std::clamp(gain,0.f,1.f);amp_.configure(patch_.amp,gain_);tone();pitch();
}
void OPLVoice::applyPatch(){
  macroBrightness_=999;macroFeedback_=-1;
  // Clear pairing, routes and key state before topology changes.
  opl2_.reset();opl3_.reset();
  write(1,0x20);
  if(type_==InstrumentType::OPL3){write(0x105,1);write(0x104,patch_.topology==OPLTopology::fourOperator?1:0);}
  write(0xbd,(patch_.deepTremolo?0x80:0)|(patch_.deepVibrato?0x40:0));
  // Channel 0 + channel 3 form the first native OPL3 four-operator pair.
  const int addresses[]={0,3,8,11};
  int count=patch_.topology==OPLTopology::twoOperator?2:4;
  for(int i=0;i<count;++i){auto& o=patch_.operators[i];int a=addresses[i];
    write(0x20+a,(o.tremolo<<7)|(o.vibrato<<6)|(o.sustained<<5)|(o.rateScale<<4)|o.multiplier);
    write(0x40+a,(o.keyScale<<6)|o.level);write(0x60+a,(o.attack<<4)|o.decay);
    write(0x80+a,(o.sustain<<4)|o.release);write(0xe0+a,o.waveform);
  }
  for(int i=0;i<2;++i)write(0xc0+(i?3:0),(patch_.feedback[i]<<1)|patch_.connection[i]|(type_==InstrumentType::OPL3?patch_.pan[i]<<4:0));
}
void OPLVoice::tone(){
  macros();
  const int brightness=std::clamp(int(patch_.tone.brightness),-63,63);
  const int feedback=std::min(8,int(patch_.tone.feedback));
  if(brightness==macroBrightness_&&feedback==macroFeedback_&&!memcmp(macroLevels_,patch_.tone.operatorLevel,6))return;
  const int addresses[]={0,3,8,11};
  unsigned carriers;
  int count=patch_.topology==OPLTopology::twoOperator?2:4;
  if(patch_.topology==OPLTopology::fourOperator){const unsigned masks[]={8,9,10,13};carriers=masks[patch_.connection[0]|(patch_.connection[1]<<1)];}
  else carriers=(patch_.connection[0]?3:2)|(count==4?(patch_.connection[1]?12:8):0);
  for(int op=0;op<count;++op){const auto& o=patch_.operators[op];int level=std::clamp((patch_.tone.operatorLevel[op]?64-int(patch_.tone.operatorLevel[op]):int(o.level))-((carriers&(1u<<op))?0:brightness),0,63);write(0x40+addresses[op],(o.keyScale<<6)|level);}
  for(int i=0;i<(count==4?2:1);++i)write(0xc0+(i?3:0),((feedback?feedback-1:patch_.feedback[i])<<1)|patch_.connection[i]|(type_==InstrumentType::OPL3?patch_.pan[i]<<4:0));
  macroBrightness_=brightness;macroFeedback_=feedback;memcpy(macroLevels_,patch_.tone.operatorLevel,6);
}
void OPLVoice::pitch(){
  for(int i=0;i<(patch_.topology==OPLTopology::twoOperator?1:2);++i){
    float base=(patch_.percussion||patch_.fixedNote)?patch_.drumKey*100.f:cents_;
    double cents=base+patch_.noteOffset[i]*100.0;
    // WOPL second-voice detune quantizes signed units to 1/32 semitone.
    if(i&&patch_.topology==OPLTopology::dualVoice)cents+=((int(patch_.secondDetune)+128)/2-64)*100.0/32;
    double fnum=440*std::exp2((std::clamp(cents,0.,14000.)-6900)/1200)*1048576.0/opl2_.sample_rate(3579545);
    int block=0;while(fnum>1023&&block<7){fnum*=.5;++block;}
    int f=std::clamp(int(std::lround(fnum)),1,1023),lo=f&255,hi=(f>>8)|(block<<2)|(gated_?0x20:0),ch=i?3:0;
    if(lo!=low_[i]){write(0xa0+ch,lo);low_[i]=lo;}if(hi!=high_[i]){write(0xb0+ch,hi);high_[i]=hi;}
  }
}
void OPLVoice::noteOn(){amp_.noteOn();gated_=false;pitch();pendingKeyOn_=true;active_=true;silent_=0;}
void OPLVoice::noteOff(){amp_.noteOff();pendingKeyOn_=false;gated_=false;pitch();}
void OPLVoice::kill(){amp_.kill();pendingKeyOn_=false;write(0xb0,0);write(0xb3,0);active_=gated_=false;low_[0]=low_[1]=high_[0]=high_[1]=-1;silent_=0;level_=0;resampler_.reset();}
void OPLVoice::native(float& l,float& r){
  // Each voice owns a chip but only uses channel 0, plus channel 3 for
  // four-operator/dual patches. applyPatch resets before topology changes.
  // Preserve the chip-wide LFO/envelope clocks without advancing unused voices.
  if(type_==InstrumentType::OPL3){ymfm::ymf262::output_data out;opl3_.generate(&out,1,patch_.topology==OPLTopology::twoOperator?1u:9u);l=(out.data[0]+out.data[2])/32768.f;r=(out.data[1]+out.data[3])/32768.f;}
  else{ymfm::ym3812::output_data out;opl2_.generate(&out,1,1u);l=r=out.data[0]/32768.f;}
  if(pendingKeyOn_){pendingKeyOn_=false;gated_=true;pitch();}
  if(!gated_&&l==0&&r==0)++silent_;else silent_=0;
}
void OPLVoice::render(float* stereo,size_t frames){
  for(size_t i=0;i<frames;++i){float l=0,r=0;if(active_)resampler_.next([&](float& a,float& b){native(a,b);},l,r);
    // Native OPL full scale, conservative instrument gain; normal track mixer follows.
    float outL=l*(.25f*nativeChipGain(type_)),outR=r*(.25f*nativeChipGain(type_));amp_.process(outL,outR);stereo[2*i]=outL;stereo[2*i+1]=outR;
    level_=std::max(std::max(std::abs(l),std::abs(r)),level_*.999f);
    if(silent_>49715)kill();
  }
}

void OPLVoice::macros() {
  const auto& t=patch_.tone;
  if(macroBrightness_!=999&&!memcmp(&directCache_,&t.direct,sizeof(directCache_)))return;
  const int addresses[]={0,3,8,11};
  const int count=patch_.topology==OPLTopology::twoOperator?2:4;
  for(int op=0;op<count;++op) {
    const auto& o=patch_.operators[op];const int a=addresses[op];
    const int ratio=nativeFMValue(t,op,fxOMU,o.multiplier);
    write(0x20+a,(o.tremolo<<7)|(o.vibrato<<6)|(o.sustained<<5)|(o.rateScale<<4)|ratio);
    write(0x60+a,(nativeFMValue(t,op,fxOAR,o.attack)<<4)|nativeFMValue(t,op,fxODR,o.decay));
    write(0x80+a,(nativeFMValue(t,op,fxOSL,o.sustain)<<4)|nativeFMValue(t,op,fxORR,o.release));
  }
  directCache_=t.direct;
}
