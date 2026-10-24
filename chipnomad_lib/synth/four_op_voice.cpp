#include "native_chip_gain.h"
#include "four_op_voice.h"
#include <cstring>
#include "../native_fm_values.h"
void FourOpVoice::init(float rate){amp_.init(rate);dcCoefficient_=std::exp(-2*3.14159265358979323846*20/rate);opn_.reset();opm_.reset();configured_=false;opnResampler_.init(opn_.sample_rate(7670454),rate);opmResampler_.init(opm_.sample_rate(3579545),rate);kill();}
void FourOpVoice::write(unsigned reg,unsigned value){if(type_==InstrumentType::GenesisFM){opn_.write_address(reg);opn_.write_data(value);}else{opm_.write_address(reg);opm_.write_data(value);}}
void FourOpVoice::configure(InstrumentType type,const InstrumentFourOp* p,float cents,float gain){
  if(!p)return;auto comparable=*p;comparable.fineTune=patch_.fineTune;comparable.amp=patch_.amp;comparable.tone=patch_.tone;
  if(!configured_||type!=type_||memcmp(&comparable,&patch_,sizeof(*p))){bool gate=gated_,active=active_;auto oldAmp=amp_;kill();if(active)amp_=oldAmp;type_=type;patch_=*p;applyPatch();configured_=true;gated_=gate;active_=active;key(gate);}
  patch_.amp=p->amp;patch_.tone=p->tone;patch_.fineTune=p->fineTune;cents_=std::isfinite(cents)?cents+p->fineTune:6000;gain_=std::clamp(gain,0.f,1.f);amp_.configure(patch_.amp,gain_);tone();pitch();
}
void FourOpVoice::applyPatch(){
  macroBrightness_=999;macroFeedback_=-1;
  opn_.reset();opm_.reset();pitchCache_=-1;
  bool genesis=type_==InstrumentType::GenesisFM;
  if(genesis){write(0x22,(patch_.lfoEnabled<<3)|patch_.lfoRate);write(0x27,0);write(0x2b,0);write(0xb0,(patch_.feedback<<3)|patch_.algorithm);write(0xb4,((patch_.pan&1)?128:0)|((patch_.pan&2)?64:0)|(patch_.amplitudeSensitivity<<4)|patch_.pitchSensitivity);}
  else{write(0x0f,0);write(0x18,patch_.lfoRate);write(0x19,patch_.lfoEnabled?patch_.amplitudeDepth:0);write(0x19,128|(patch_.lfoEnabled?patch_.pitchDepth:0));write(0x1b,patch_.lfoWave);write(0x20,(patch_.pan<<6)|(patch_.feedback<<3)|patch_.algorithm);write(0x38,(patch_.pitchSensitivity<<4)|patch_.amplitudeSensitivity);}
  const int order[]={0,2,1,3};
  for(int i=0;i<4;++i){const auto& o=patch_.operators[i];int a=order[i]*(genesis?4:8);
    if(genesis){write(0x30+a,(o.detune<<4)|o.multiplier);write(0x40+a,o.level);write(0x50+a,(o.keyScale<<6)|o.attack);write(0x60+a,(o.amplitudeMod<<7)|o.decay);write(0x70+a,o.sustainRate);write(0x80+a,(o.sustainLevel<<4)|o.release);write(0x90+a,o.ssg);}
    else{write(0x40+a,(o.detune<<4)|o.multiplier);write(0x60+a,o.level);write(0x80+a,(o.keyScale<<6)|o.attack);write(0xa0+a,(o.amplitudeMod<<7)|o.decay);write(0xc0+a,(o.detune2<<6)|o.sustainRate);write(0xe0+a,(o.sustainLevel<<4)|o.release);}
  }
}
void FourOpVoice::tone(){
  macros();
  const int brightness=std::clamp(int(patch_.tone.brightness),-63,63);
  const int feedback=patch_.tone.feedback?std::min(7,int(patch_.tone.feedback)-1):patch_.feedback;
  if(brightness==macroBrightness_&&feedback==macroFeedback_&&!memcmp(macroLevels_,patch_.tone.operatorLevel,6))return;
  const unsigned carriers[]={8,8,8,8,10,14,14,15};const int order[]={0,2,1,3};
  bool genesis=type_==InstrumentType::GenesisFM;
  for(int op=0;op<4;++op){int level=std::clamp((patch_.tone.operatorLevel[op]?128-int(patch_.tone.operatorLevel[op]):int(patch_.operators[op].level))-((carriers[patch_.algorithm]&(1u<<op))?0:brightness),0,127);write((genesis?0x40:0x60)+order[op]*(genesis?4:8),level);}
  write(genesis?0xb0:0x20,(genesis?0:(patch_.pan<<6))|(feedback<<3)|patch_.algorithm);
  macroBrightness_=brightness;macroFeedback_=feedback;memcpy(macroLevels_,patch_.tone.operatorLevel,6);
}
void FourOpVoice::pitch(){
  if(type_==InstrumentType::GenesisFM){
    // Native OPN fnum is 11 bits; phase step (fnum << block)/2, 20-bit phase.
    double fnum=440*std::exp2((std::clamp(double(cents_),0.,14000.)-6900)/1200)*2097152.0/opn_.sample_rate(7670454);int block=0;
    while(fnum>2047&&block<7){fnum*=.5;++block;}int f=std::clamp(int(std::lround(fnum)),1,2047);int value=(block<<11)|f;
    if(value!=pitchCache_){write(0xa4,(block<<3)|(f>>8));write(0xa0,f&255);pitchCache_=value;}
  }else{
    // YM2151 KC=0 is C#0 (MIDI 13); each semitone has 64 key fractions.
    int step=std::clamp(int(std::lround((cents_-1300)*64/100)),0,8*12*64-1);
    if(step!=pitchCache_){const int codes[]={0,1,2,4,5,6,8,9,10,12,13,14};int note=step/64;write(0x28,((note/12)<<4)|codes[note%12]);write(0x30,(step%64)<<2);pitchCache_=step;}
  }
}
void FourOpVoice::key(bool on){unsigned mask=on?patch_.operatorMask:0;
  // Key bits follow logical operator order, unlike parameter register slots.
  write(type_==InstrumentType::GenesisFM?0x28:0x08,mask<<(type_==InstrumentType::GenesisFM?4:3));
}
void FourOpVoice::noteOn(){amp_.noteOn();key(false);pitch();pendingKeyOn_=true;gated_=false;active_=true;silent_=0;}
void FourOpVoice::noteOff(){amp_.noteOff();pendingKeyOn_=false;key(false);gated_=false;}
void FourOpVoice::kill(){amp_.kill();pendingKeyOn_=false;key(false);active_=gated_=false;level_=0;silent_=0;dcInput_[0]=dcInput_[1]=dcOutput_[0]=dcOutput_[1]=0;opnResampler_.reset();opmResampler_.reset();}
void FourOpVoice::native(float& l,float& r){
  // Each chord slot owns a chip and uses channel 0 only. Chip-wide clocks
  // and DAC conversion still advance normally; unused operators do not.
  if(type_==InstrumentType::GenesisFM){ymfm::ym2612::output_data out;opn_.generate(&out,1,1u);// YM2612 ladder models a nonzero idle code; subtract its exact idle
    // baseline before testing silence. The output DC blocker follows the FIR.
    l=(out.data[0]-504)/32768.f;r=(out.data[1]-504)/32768.f;}
  else{ymfm::ym2151::output_data out;opm_.generate(&out,1,1u);l=out.data[0]/32768.f;r=out.data[1]/32768.f;}
  if(pendingKeyOn_){pendingKeyOn_=false;gated_=true;key(true);}
  if(!gated_&&l==0&&r==0)++silent_;else silent_=0;
}
void FourOpVoice::render(float* stereo,size_t frames){
  auto& resampler=type_==InstrumentType::GenesisFM?opnResampler_:opmResampler_;
  for(size_t i=0;i<frames;++i){float l=0,r=0;if(active_)resampler.next([&](float& a,float& b){native(a,b);},l,r);float values[]={l,r};for(int c=0;c<2;++c){float filtered=values[c]-dcInput_[c]+dcCoefficient_*dcOutput_[c];dcInput_[c]=values[c];dcOutput_[c]=filtered;stereo[2*i+c]=filtered*(.25f*nativeChipGain(type_));}amp_.process(stereo[2*i],stereo[2*i+1]);level_=std::max(std::max(std::abs(l),std::abs(r)),level_*.999f);if(silent_>56000)kill();}
}

void FourOpVoice::macros() {
  const auto& t=patch_.tone;
  if(macroBrightness_!=999&&!memcmp(&directCache_,&t.direct,sizeof(directCache_)))return;
  const bool genesis=type_==InstrumentType::GenesisFM;
  const int order[]={0,2,1,3};
  for(int op=0;op<4;++op) {
    const auto& o=patch_.operators[op];const int a=order[op]*(genesis?4:8);
    const int ratio=nativeFMValue(t,op,fxOMU,o.multiplier);
    const int detune=nativeFMValue(t,op,fxODT,o.detune);
    write((genesis?0x30:0x40)+a,(detune<<4)|ratio);
    write((genesis?0x50:0x80)+a,(o.keyScale<<6)|nativeFMValue(t,op,fxOAR,o.attack));
    write((genesis?0x60:0xa0)+a,(o.amplitudeMod<<7)|nativeFMValue(t,op,fxODR,o.decay));
    write((genesis?0x70:0xc0)+a,(genesis?0:o.detune2<<6)|nativeFMValue(t,op,fxOSR,o.sustainRate));
    write((genesis?0x80:0xe0)+a,(nativeFMValue(t,op,fxOSL,o.sustainLevel)<<4)|nativeFMValue(t,op,fxORR,o.release));
  }
  const bool enabled=nativeFMValue(t,0,fxLEN,patch_.lfoEnabled);
  const int rate=nativeFMValue(t,0,fxLFR,patch_.lfoRate);
  if(genesis) {
    write(0x22,(enabled?8:0)|rate);
    write(0xb4,((patch_.pan&1)?128:0)|((patch_.pan&2)?64:0)|
      (nativeFMValue(t,0,fxLAS,patch_.amplitudeSensitivity)<<4)|nativeFMValue(t,0,fxLPS,patch_.pitchSensitivity));
  } else {
    write(0x18,rate);
    write(0x19,enabled?nativeFMValue(t,0,fxLAD,patch_.amplitudeDepth):0);
    write(0x19,128|(enabled?nativeFMValue(t,0,fxLPD,patch_.pitchDepth):0));
    write(0x38,(nativeFMValue(t,0,fxLPS,patch_.pitchSensitivity)<<4)|nativeFMValue(t,0,fxLAS,patch_.amplitudeSensitivity));
  }
  directCache_=t.direct;
}
