#include "doctest.h"
#include "project.h"
#include "simple_chip_presets.h"
#include "synth/simple_chip_voice.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
TEST_CASE("Sega tone follows keyboard pitch and live octave changes") {
 for(int rate:{44100,48000,96000}) {
  Instrument instrument{};getInstrumentFunctions(InstrumentType::SegaPSG).init(&instrument);
  REQUIRE(simpleChipApplyPreset(&instrument,2)); // Sustained tone, no native noise.
  SimpleChipVoice voice;voice.init(rate);
  for(int cents:{2400,3600,4800,5700,6900,8100,2400}) {
   CAPTURE(rate);CAPTURE(cents);
   voice.configure(instrument.type,&instrument.chip.simpleChip,cents,1);
   if(!voice.active())voice.noteOn();
   std::vector<float> audio(rate);voice.render(audio.data(),audio.size());
   int crossings=0;
   for(int n=rate/4+1;n<rate;++n)if(audio[n-1]<=0&&audio[n]>0)++crossings;
   double expected=440*std::exp2((cents-6900)/1200.0);
   CHECK(std::abs(crossings/.75-expected)<expected*.015+2);
  }
 }
}
TEST_CASE("Simple chip starter recipes render and survive CNI round trips"){
 auto p=std::make_unique<Project>(),q=std::make_unique<Project>();projectInit(p.get());projectInit(q.get());fillFXNames();
 for(auto type:{InstrumentType::SegaPSG,InstrumentType::GBPulse,InstrumentType::GBNoise})for(int preset=0;preset<simpleChipPresetCount(type);++preset){
   CAPTURE(int(type));CAPTURE(preset);auto& i=p->instruments[0];getInstrumentFunctions(type).init(&i);REQUIRE(simpleChipApplyPreset(&i,preset));
   REQUIRE(instrumentSave(p.get(),"test_simple.cni",0)==0);REQUIRE(instrumentLoad(q.get(),"test_simple.cni",3)==0);
   CHECK(q->instruments[3].type==type);CHECK(memcmp(&q->instruments[3].chip.simpleChip,&i.chip.simpleChip,sizeof(InstrumentSimpleChip))==0);
   for(int rate:{44100,48000,96000}){
     CAPTURE(rate);SimpleChipVoice voice;voice.init(rate);voice.configure(type,&i.chip.simpleChip,6000,1);voice.noteOn();
     std::vector<float> audio(rate);voice.render(audio.data(),audio.size());double energy=0,mean=0;
     for(float v:audio){REQUIRE(std::isfinite(v));CHECK(std::abs(v)<2);energy+=v*v;mean+=v;}
     CHECK(energy>1e-5);CHECK(std::abs(mean/rate)<.1);
     voice.noteOff();voice.render(audio.data(),audio.size());voice.kill();voice.render(audio.data(),audio.size());for(float v:audio)REQUIRE(v==0);
   }
 }
 std::remove("test_simple.cni");projectFree(p.get());projectFree(q.get());
}
TEST_CASE("GB pulse duty and GB noise width produce independent distinct signals"){
 for(auto type:{InstrumentType::GBPulse,InstrumentType::GBNoise}){
   Instrument i{};getInstrumentFunctions(type).init(&i);auto a=i.chip.simpleChip,b=a;b.mode=type==InstrumentType::GBPulse?2:1;
   SimpleChipVoice x,y;x.init(48000);y.init(48000);x.configure(type,&a,6900,1);y.configure(type,&b,6900,1);x.noteOn();y.noteOn();
   std::vector<float>left(12000),right(left.size());x.render(left.data(),left.size());y.render(right.data(),right.size());double delta=0;for(size_t j=0;j<left.size();++j)delta+=std::abs(left[j]-right[j]);CHECK(delta>.1);
 }
}
TEST_CASE("Sega noise follows exact fixed dividers and tone two rising edges"){
 SNG s{};s.clk=3579545;SNG_set_rate(&s,3579545/16);SNG_set_quality(&s,0);SNG_reset(&s);
 for(int rate=0;rate<3;++rate){SNG_reset(&s);SNG_writeIO(&s,0xe0|rate);int changes=0;for(int i=0;i<4096;++i){auto before=s.noise_seed;SNG_calc(&s);changes+=s.noise_seed!=before;}CHECK(changes==4096/(32<<rate));}
 SNG_reset(&s);SNG_writeIO(&s,0xc0|7);SNG_writeIO(&s,2);SNG_writeIO(&s,0xe3);
 for(int i=0;i<8192;++i){auto seed=s.noise_seed,edge=s.edge[2];SNG_calc(&s);CHECK((seed!=s.noise_seed)==(!edge&&s.edge[2]));}
}
TEST_CASE("Simple chips keep sample timing across block sizes"){
 for(auto type:{InstrumentType::SegaPSG,InstrumentType::GBPulse,InstrumentType::GBNoise}){
   Instrument i{};getInstrumentFunctions(type).init(&i);SimpleChipVoice a,b;a.init(48000);b.init(48000);a.configure(type,&i.chip.simpleChip,6900,1);b.configure(type,&i.chip.simpleChip,6900,1);a.noteOn();b.noteOn();
   std::vector<float>x(12000),y(x.size());a.render(x.data(),x.size());for(size_t j=0;j<y.size();++j)b.render(y.data()+j,1);for(size_t j=0;j<x.size();++j)CHECK(x[j]==y[j]);
 }
}

#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
TEST_CASE("Sega composed A-2 G-2 F-2 phrase follows tracker note labels") {
 for(bool linear:{false,true})for(bool extended:{false,true}) {
  CAPTURE(linear);CAPTURE(extended);
  auto s=std::unique_ptr<ChipNomadState,decltype(&chipnomadDestroy)>(chipnomadCreate(),chipnomadDestroy);
  REQUIRE(projectLoad(&s->project,"packaging/common/projects/gm-midi-demo.cct")==0);
  auto& p=s->project;p.tracksCount=1;p.tickRate=50;p.linearPitch=linear;reinitializePitchTable(&p);
  for(auto& groove:p.grooves)for(auto& speed:groove.speed)speed=50;
  p.song[0][0]=0;p.chains[0].rows[0].phrase=0;p.chains[0].rows[0].transpose=0;phraseClear(&p.phrases[0]);
  auto& inst=p.instruments[0];getInstrumentFunctions(InstrumentType::SegaPSG).init(&inst);simpleChipApplyPreset(&inst,2);inst.chip.simpleChip.segaBassExtension=extended;
  for(int row=0;row<3;++row) {auto& r=p.phrases[0].rows[row];r.note=33-row*2;r.instrument=0;r.volume=PHRASE_VOLUME_MAX;}
  REQUIRE(!strcmp(p.pitchTable.noteNames[33],"A-2"));REQUIRE(!strcmp(p.pitchTable.noteNames[29],"F-2"));
  chipnomadInitChips(s.get(),48000,nullptr);chipnomadReserveRenderBuffers(s.get(),480);
  REQUIRE(chipnomadQueueProjectRefresh(s.get()));REQUIRE(chipnomadQueuePlaybackStartSong(s.get(),0,0,1));
  std::vector<float> audio(3*48000*2);for(int block=0;block<300;++block)REQUIRE(chipnomadRender(s.get(),audio.data()+block*960,480));
  for(int row=0;row<3;++row) {
   int crossings=0;for(int f=row*48000+12001;f<(row+1)*48000;++f)if(audio[(f-1)*2]<=0&&audio[f*2]>0)++crossings;
   double expected=440*std::exp2(((33-row*2+12)*100-6900)/1200.0);
   if(!extended)expected=std::max(expected,3579545.0/(32*1023));
   CAPTURE(row);CAPTURE(crossings);CHECK(std::abs(crossings/.75-expected)<3);
  }
 }
}
