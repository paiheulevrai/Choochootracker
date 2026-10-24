#include "native_chip_gain.h"
#include "simple_chip_voice.h"
#include "../simple_chip_presets.h"
#include <algorithm>
#include <cmath>
#include <cstring>
void SimpleChipVoice::init(float rate){
 rate_=std::max(rate,8000.f);if(apu_)apu_quit(apu_);apu_=apu_init(4194304,rate_);
 if(apu_){apu_reset(apu_,GbApuType_DMG);apu_set_bass(apu_,20);}
 sega_={};sega_.clk=3579545;SNG_set_rate(&sega_,3579545/16);SNG_set_quality(&sega_,0);SNG_reset(&sega_);
 resampler_.init(3579545/16,rate_);post_.init(rate_);dcPole_=std::exp(-2*3.141592653589793*20/rate_);kill();
}
void SimpleChipVoice::configure(InstrumentType type,const InstrumentSimpleChip* p,float cents,float gain){
 if(!p||!validSimpleChip(type,*p))return;
 if(type_!=type){kill();type_=type;}
 bool newProgram=memcmp(&patch_.program,&p->program,sizeof(p->program));
 patch_=*p;if(newProgram){macros_.reset(patch_.program);macroPhase_=0;}
 cents_=std::isfinite(cents)?cents+p->fineTune:6000;
 post_.setGain(gain);post_.setFilter(p->filterEnabled,p->filterCharacter,p->filterMode,p->filterSlope24dB,p->filterCutoffHz,p->filterResonance/255.f);
 post_.setEnvelope(true,p->attack*p->attack/13005.f,p->decay*p->decay/13005.f,p->sustain/255.f,p->release*p->release/13005.f,p->envelopeShape);
 if(active_)registers(false);
}
void SimpleChipVoice::registers(bool trigger){
 int volume=type_==InstrumentType::SegaPSG?std::clamp(macros_.value(0,15),0,15):15;
 macroGain_=volume==15?1.f:volume?std::pow(10.f,(volume-15)/10.f):0.f;
 float pitch=cents_;if(macros_.has(1)){int arp=macros_.value(1);pitch=(uint32_t(arp)&0xc0000000)==0x40000000?(arp^0x40000000)*100.f:pitch+arp*100.f;}
 double hz=440*std::exp2((std::clamp(pitch,0.f,14000.f)-6900)/1200.);
 if(type_==InstrumentType::SegaPSG){
   unsigned mode=patch_.mode,noiseRate=patch_.noiseRate;
   if(macros_.has(2)){int duty=macros_.value(2);mode=(duty&1)?1:2;noiseRate=duty>=2?3:std::clamp(2-(int(pitch/100)%12),0,2);}
   unsigned clock=3579545;
   if(patch_.segaBassExtension && (!mode || noiseRate==3))
     while(clock/(32*hz)>1023 && clock>1)clock/=2;
   if(sega_.clk!=clock){sega_.clk=clock;SNG_set_rate(&sega_,3579545/16);}
   int period=std::clamp(int(std::lround(clock/(32*hz))),2,1023);
   if(trigger||period!=lastFrequency_){SNG_writeIO(&sega_,0x80|(period&15));SNG_writeIO(&sega_,period>>4);SNG_writeIO(&sega_,0xc0|(period&15));SNG_writeIO(&sega_,period>>4);lastFrequency_=period;}
   int noise=(mode==1?4:0)|noiseRate;
   if(trigger||lastMode_!=int(mode)||lastNoise_!=noise){SNG_writeIO(&sega_,0x90|(mode?15:0));SNG_writeIO(&sega_,0xbf);SNG_writeIO(&sega_,0xdf);SNG_writeIO(&sega_,0xe0|noise);SNG_writeIO(&sega_,0xf0|(mode?0:15));lastMode_=mode;lastNoise_=noise;}
   return;
 }
 if(!apu_)return;
 if(trigger){gbWrite(0x26,0x80);gbWrite(0x24,0x77);gbWrite(0x25,type_==InstrumentType::GBPulse?0x11:0x88);gbWrite(0x1a,0);gbWrite(0x17,0);}
 bool softEnvelope=macros_.value(15)==1;
 int envelope=softEnvelope?(std::clamp(macros_.value(0,15),0,15)<<4)|8:(patch_.envelopeInitial<<4)|(patch_.envelopeIncrease<<3)|patch_.envelopePeriod;
 bool envelopeTrigger=trigger||(softEnvelope&&lastEnvelope_!=envelope);
 if(envelopeTrigger){gbWrite(type_==InstrumentType::GBPulse?0x12:0x21,envelope);lastEnvelope_=envelope;}
 if(type_==InstrumentType::GBPulse){
   int frequency=std::clamp(2048-int(std::lround(131072/hz)),0,2047);
   int mode=std::clamp(macros_.value(2,patch_.mode),0,3);
   if(trigger||lastMode_!=mode){gbWrite(0x11,mode<<6);lastMode_=mode;}
   if(trigger){gbWrite(0x10,(patch_.sweepPeriod<<4)|(patch_.sweepNegate<<3)|patch_.sweepShift);}
   if(envelopeTrigger||frequency!=lastFrequency_){gbWrite(0x13,frequency&255);gbWrite(0x14,(frequency>>8)|(envelopeTrigger?0x80:0));lastFrequency_=frequency;}
 }else{
   int noise=(patch_.noiseShift<<4)|(patch_.mode<<3)|patch_.noiseDivisor;
   if(patch_.program.format){int n=std::clamp(int(pitch/100)-60,0,255);int reg=n>=1&&n<=64?((15-(n-1)/4)<<4)|(7-(n-1)%4):n>=65&&n<=68?68-n:0;noise=reg|((macros_.value(2,patch_.mode)?1:0)<<3);}
   if(trigger){gbWrite(0x12,0);}
   if(trigger||noise!=lastNoise_){gbWrite(0x22,noise);lastNoise_=noise;}
   if(envelopeTrigger)gbWrite(0x23,0x80);
 }
}
void SimpleChipVoice::noteOn(){
 if(type_!=InstrumentType::SegaPSG&&!apu_)return;
 if(type_==InstrumentType::SegaPSG)SNG_reset(&sega_);else{apu_reset(apu_,GbApuType_DMG);sequencerRemaining_=8192;}
 macros_.reset(patch_.program);macroPhase_=0;macroRelease_=false;
 resampler_.reset();previous_=dc_=0;registers(true);post_.noteOn(true);active_=true;
}
void SimpleChipVoice::noteOff(){macroRelease_=macros_.releaseTail(patch_.program,0);macros_.release(patch_.program);if(!macroRelease_)post_.noteOff();}
void SimpleChipVoice::kill(){active_=false;post_.kill();lastFrequency_=lastMode_=lastNoise_=lastEnvelope_=-1;previous_=dc_=0;}
float SimpleChipVoice::segaSample(){float left,right;resampler_.next([&](float& l,float& r){l=r=SNG_calc(&sega_)/4096.f;},left,right);float out=left-previous_+dcPole_*dc_;previous_=left;dc_=out;return out;}
float SimpleChipVoice::gbSample(){
 int clocks=apu_clocks_needed(apu_,2);
 if(clocks>0){unsigned time=0,remaining=clocks;
   while(remaining>=sequencerRemaining_){time+=sequencerRemaining_;remaining-=sequencerRemaining_;apu_frame_sequencer_clock(apu_,time);sequencerRemaining_=8192;}
   sequencerRemaining_-=remaining;apu_end_frame(apu_,clocks);apu_update_timestamp(apu_,-clocks);
 }
 short samples[2]{};apu_read_samples(apu_,samples,2);return (samples[0]+samples[1])/65536.f;
}
void SimpleChipVoice::render(float* mono,size_t frames){for(size_t i=0;i<frames;++i){if(!active_){mono[i]=0;continue;}
 if(patch_.program.format){macroPhase_+=patch_.program.rate/double(rate_);if(macroPhase_>=1){macroPhase_-=1;macros_.tick(patch_.program);registers(false);if(macroRelease_&&macros_.done(0))post_.noteOff();}}
 mono[i]=post_.process(type_==InstrumentType::SegaPSG?segaSample():gbSample())*nativeChipGain(type_)*macroGain_;if(!post_.envelopeActive())kill();}}
