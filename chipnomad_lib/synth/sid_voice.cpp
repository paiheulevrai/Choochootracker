#include "native_chip_gain.h"
// Uses the pinned zlib m6581 digital and filter helpers; see external/chips/NOTICE.txt.
#define CHIPS_IMPL
#include "sid_voice.h"
#include <cstring>
void SIDVoice::init(float rate){
 rate_=std::max(8000.f,rate);
 // Initialize the core's shared cutoff table once, off the audio callback.
 // Concurrent song/export contexts clone this immutable starting state.
 static const m6581_t initial=[](){m6581_t c;m6581_desc_t d{985248,48000,1};m6581_init(&c,&d);return c;}();
 chip_=initial;chip_.sound_hz=int(rate_);chip_.sample_period=(985248*M6581_FIXEDPOINT_SCALE)/int(rate_);m6581_reset(&chip_);
 dcPole_=std::exp(-2*3.141592653589793*20/rate_);amp_.init(rate_);configured_=false;kill();
}
void SIDVoice::write(int reg,int value){
 value&=255;if(registers_[reg]==value)return;registers_[reg]=value;
 _m6581_write(&chip_,M6581_CS|uint64_t(reg)|(uint64_t(value)<<16));
}
void SIDVoice::configure(const InstrumentSID* p,float cents,float gain){
 if(!p||!validSID(*p))return;
 bool changed=!configured_||memcmp(&patch_,p,sizeof(*p));
 bool newProgram=!configured_||memcmp(&patch_.program,&p->program,sizeof(p->program));
 patch_=*p;configured_=true;if(newProgram){macros_.reset(patch_.program);goat_.reset(patch_.program);macroPhase_=0;}
 cents_=std::isfinite(cents)?std::clamp(cents,0.f,14000.f):6000;
 InstrumentFMAmp amp{};amp_.configure(amp,gain);
 // Parameter changes update registers without resetting oscillators/envelopes.
 if(active_){if(changed)program();else if(!patch_.value[sidFixedFrequency])program();}
}
void SIDVoice::program(){
 const auto* v=patch_.value;double seconds=double(macroFrame_)/v[sidMacroRate];
 if(patch_.program.format==2){
  float cents=goat_.absolute?goat_.note*100.f:cents_+goat_.note*100.f;
  int freq=std::clamp(int(std::lround(std::clamp(440*std::exp2((cents-6900)/1200.)*16777216/985248,1.,65535.))),1,65535);
  write(0,freq&255);write(1,freq>>8);write(2,goat_.pulse&255);write(3,goat_.pulse>>8);
  write(5,goat_.ad);write(6,goat_.sr);write(21,0);write(22,goat_.cutoff);
  write(23,(goat_.resonance<<4)|(goat_.filter?1:0));write(24,(goat_.filter<<4)|15);
  write(4,goat_.control&(gated_?255:254));return;
 }
 double frequency=v[sidFixedFrequency]?v[sidFixedFrequency]:440*std::exp2((cents_-6900)/1200.)*16777216/985248;
 if(macros_.has(1)){int arp=macros_.value(1);frequency=440*std::exp2((((uint32_t(arp)&0xc0000000)==0x40000000?(arp^0x40000000)*100.:cents_+arp*100.)-6900)/1200.)*16777216/985248;}
 if(v[sidSweepTarget]){double x=std::min(1.,double(macroFrame_)/std::max(1,int(v[sidSweepFrames])));
  frequency=v[sidSweepMode]?frequency*std::pow(v[sidSweepTarget]/std::max(1.,frequency),x):frequency+(v[sidSweepTarget]-frequency)*x;
 }
 if(v[sidArpeggio]){static const int arp[][3]={{0,0,0},{0,4,7},{0,3,7},{0,7,12},{0,5,9}};frequency*=std::exp2(arp[v[sidArpeggio]][macroFrame_%3]/12.);}
 frequency+=v[sidVibratoDepth]*std::sin(2*3.141592653589793*v[sidVibratoRate]/10.*seconds);
 int f=std::clamp(int(std::lround(std::clamp(frequency,1.,65535.))),1,65535);write(0,f&255);write(1,f>>8);
 int partner=std::clamp(int(std::lround(std::clamp(frequency*v[sidPartnerRatio],1.,65535.))),1,65535);write(14,partner&255);write(15,partner>>8);
 int pulse=std::clamp(int(v[sidPulse])+int(std::lround(v[sidPulseDepth]*std::sin(2*3.141592653589793*v[sidPulseRate]/10.*seconds))),0,4095);
 pulse=std::clamp(macros_.value(2,pulse),0,4095);
 write(2,pulse&255);write(3,pulse>>8);write(5,(v[sidAttack]<<4)|v[sidDecay]);write(6,(v[sidSustain]<<4)|v[sidRelease]);
 double cutoff=v[sidCutoff];if(v[sidCutoffTarget]!=65535)cutoff+=(int(v[sidCutoffTarget])-cutoff)*std::min(1.,double(macroFrame_)/std::max(1,int(v[sidGateFrames]?v[sidGateFrames]:75)));
 cutoff=macros_.value(5,int(cutoff));
 int fc=std::clamp(int(std::lround(cutoff)),0,2047);write(21,fc&7);write(22,fc>>3);
 write(23,(v[sidResonance]<<4)|(v[sidFilterMode]?1:0));write(24,(v[sidFilterMode]<<4)|15);
 if(gated_&&v[sidGateFrames]&&macroFrame_>=v[sidGateFrames])noteOff();
 write(4,(std::clamp(macros_.value(3,v[sidWave]),0,8)<<4)|(v[sidRing]<<2)|(v[sidSync]<<1)|(gated_?1:0));
}
void SIDVoice::noteOn(){
 if(!configured_)return;
 m6581_reset(&chip_);for(auto& r:registers_)r=-1;
 cyclePhase_=macroPhase_=0;macroFrame_=silent_=0;previous_=dc_=0;active_=gated_=fresh_=true;
 macros_.reset(patch_.program);goat_.reset(patch_.program);program();amp_.noteOn();
}
void SIDVoice::noteOff(){if(active_){gated_=false;macros_.release(patch_.program);write(4,registers_[4]&~1);amp_.noteOff();}}
void SIDVoice::kill(){active_=gated_=fresh_=false;level_=0;amp_.kill();}
float SIDVoice::clock(){
 // The other two envelopes are permanently zero in this isolated note engine.
 // Skip their digital work, keeping the native per-cycle envelope/filter path.
 _m6581_voice_tick(&chip_,0);
 if(patch_.value[sidRing]||patch_.value[sidSync]){
  _m6581_voice_tick(&chip_,2);
  if(patch_.value[sidSync]&&chip_.voice[2].sync)chip_.voice[0].wav_accum=0;
 }
 int voice=int(chip_.voice[0].wav_output)*int(chip_.voice[0].env_cur_level);
 int filtered=_m6581_filter_output(&chip_.filter,chip_.filter.voices?voice:0);
 return ((chip_.filter.voices?0:voice)+filtered)*(15.f/67108864.f);
}
void SIDVoice::render(float* out,size_t frames){
 if(frames)fresh_=false;
 for(size_t i=0;i<frames;++i){
  if(!active_){out[i]=0;continue;}
  macroPhase_+=(patch_.program.format?patch_.program.rate:patch_.value[sidMacroRate])/double(rate_);
  if(macroPhase_>=1){macroPhase_-=1;++macroFrame_;macros_.tick(patch_.program);goat_.tick(patch_.program);program();}
  cyclePhase_+=985248./rate_;int cycles=int(cyclePhase_);cyclePhase_-=cycles;
  float sample=0;for(int c=0;c<cycles;++c)sample+=clock();sample/=std::max(1,cycles);
  float blocked=sample-previous_+dcPole_*dc_;previous_=sample;dc_=blocked;
  const float sourceGain=macros_.has(0)?std::clamp(macros_.value(0),0,15)/15.f:1.f;
  out[i]=amp_.process(blocked*nativeChipGain(InstrumentType::SID))*sourceGain;level_=std::max(std::abs(out[i]),level_*.999f);
  if(!gated_&&!chip_.voice[0].env_cur_level){if(++silent_>unsigned(rate_*.05))kill();}else silent_=0;
 }
}
unsigned limitSIDVoices(SIDVoice* const* voices,size_t count,unsigned limit){
 unsigned active=0,stolen=0;for(size_t i=0;i<count;++i)if(voices[i]->active())active+=voices[i]->cost();
 while(active>limit){SIDVoice* victim=nullptr;
  for(size_t i=count;i-->0;){auto* v=voices[i];if(!v->active())continue;
   if(!victim||(v->releasing()&&!victim->releasing())||(v->releasing()==victim->releasing()&& ((!v->fresh()&&victim->fresh()) || (v->fresh()==victim->fresh()&&v->envelopeLevel()<victim->envelopeLevel()))))victim=v;
  }
  if(!victim)break;active-=victim->cost();victim->kill();++stolen;
 }return stolen;
}
