#include "synth/native_chip_gain.h"
#include "packaged_presets.h"
#include "doctest.h"
#include "project.h"
#include "four_op_patch.h"
#include "synth/four_op_voice.h"
#include "chipnomad_lib.h"
#include <memory>
#include <vector>
#include <cstring>
#include <filesystem>
TEST_CASE("FourOp native pitch blocks release isolation and pan"){
 for(auto type:{InstrumentType::GenesisFM,InstrumentType::ArcadeFM})for(int rate:{22050,44100,48000,96000}){
  CAPTURE(int(type));CAPTURE(rate);InstrumentFourOp p{};initFourOpPatch(&p);p.algorithm=7;p.operatorMask=8;
  for(auto& o:p.operators)o={1,0,0,0,31,0,0,15,0,0,0,0};
  FourOpVoice a,b,other;a.init(rate);b.init(rate);other.init(rate);a.configure(type,&p,6900,1);b.configure(type,&p,6900,1);a.noteOn();b.noteOn();
  std::vector<float>x(rate*2),y(x.size());a.render(x.data(),rate);for(int i=0;i<rate;){int n=std::min(113,rate-i);b.render(y.data()+2*i,n);i+=n;}
  CHECK(x==y);double energy=0;int crossings=0;for(int i=rate/4+1;i<rate;++i){REQUIRE(std::isfinite(x[2*i]));energy+=x[2*i]*x[2*i];if(x[2*i-2]<=0&&x[2*i]>0)++crossings;}CHECK(energy>.001);CHECK(std::abs(crossings/.75-440)<3);
  other.configure(type,&p,4700,1);other.noteOn();other.render(x.data(),rate);a.configure(type,&p,7300,.8);b.configure(type,&p,7300,.8);a.render(x.data(),rate);b.render(y.data(),rate);CHECK(x==y);
  a.noteOff();for(int n=0;n<6&&a.active();++n)a.render(x.data(),rate);CHECK_FALSE(a.active());a.kill();a.render(x.data(),rate);for(float v:x)REQUIRE(v==0);
  p.pan=1;a.configure(type,&p,6900,1);a.noteOn();a.render(x.data(),rate);for(int i=0;i<rate;++i)REQUIRE(x[2*i+1]==0);
 }
}
TEST_CASE("FourOp patch file ownership and malformed load are transactional"){
 auto a=std::make_unique<Project>(),b=std::make_unique<Project>();fillFXNames();projectInit(a.get());projectInit(b.get());REQUIRE(!projectLoad(a.get(),"packaging/common/projects/gm-midi-demo.cct"));
 for(auto type:{InstrumentType::GenesisFM,InstrumentType::ArcadeFM}){
  getInstrumentFunctions(type).init(&a->instruments[5]);auto& p=a->instruments[5].chip.fourOp;p.fineTune=-29;p.bankId=523;p.sourceProgram=91;p.operators[0].detune=6;
  REQUIRE(validFourOp(type,p));REQUIRE(!instrumentSave(a.get(),"test_four_op.cni",5));REQUIRE(!instrumentLoad(b.get(),"test_four_op.cni",8));CHECK(!memcmp(&p,&b->instruments[8].chip.fourOp,sizeof(p)));
  REQUIRE(!projectSave(a.get(),"test_four_op.cct"));REQUIRE(!projectLoad(b.get(),"test_four_op.cct"));CHECK(!memcmp(&p,&b->instruments[5].chip.fourOp,sizeof(p)));
  p.algorithm=8;CHECK_FALSE(validFourOp(type,p));REQUIRE(!instrumentSave(a.get(),"test_four_op.cni",5));auto before=std::make_unique<Project>(*b);CHECK(instrumentLoad(b.get(),"test_four_op.cni",8)!=0);CHECK(!memcmp(before.get(),b.get(),sizeof(Project)));
 }
 CHECK(sizeof(Instrument)==1208) /* merged: our InstrumentSample (autoSensitivity + 64 sliceBounds) grows the union; includes upstream sample stretch/speed fields */;CHECK(int(InstrumentType::GenesisFM)==25);CHECK(int(InstrumentType::ArcadeFM)==26);std::remove("test_four_op.cni");std::remove("test_four_op.cct");projectFree(a.get());projectFree(b.get());
}
TEST_CASE("FourOp all original factory patches load and render"){
 auto p=std::make_unique<Project>();projectInit(p.get());int count=0;std::vector<float> audio(96000);
 for(const auto& f:packagedPresets()){
  if(f.path.find("four-op-")!=0)continue;CAPTURE(f.path);REQUIRE(!(loadFMPreset("packaging/common/instruments/FACTORY",f,p.get(),0)?0:1));auto& i=p->instruments[0];REQUIRE(validFourOp(i.type,i.chip.fourOp));FourOpVoice voice;voice.init(48000);voice.configure(i.type,&i.chip.fourOp,6000,1);voice.noteOn();voice.render(audio.data(),48000);double energy=0;
  for(float x:audio){REQUIRE(std::isfinite(x));REQUIRE(std::abs(x)<1);energy+=x*x;}CHECK(energy>1e-6);++count;
 }
 CHECK(count==48);projectFree(p.get());
}
TEST_CASE("FourOp queued preview owns the patch and stops cleanly"){
 auto* s=chipnomadCreate();s->project.chipsCount=1;s->project.tracksCount=3;s->project.tickRate=50;chipnomadInitChips(s,48000,nullptr);auto before=std::make_unique<Project>(s->project);
 for(auto type:{InstrumentType::GenesisFM,InstrumentType::ArcadeFM}){InstrumentFourOp p{};initFourOpPatch(&p);REQUIRE(chipnomadQueueFourOpPreview(s,0,type,&p));memset(&p,0,sizeof(p));std::vector<float>a(2048);double energy=0;for(int n=0;n<20;++n){chipnomadRender(s,a.data(),1024);for(float x:a)energy+=x*x;}CHECK(energy>.001);CHECK(!memcmp(before.get(),&s->project,sizeof(Project)));REQUIRE(chipnomadQueueFourOpPreview(s,0,type,nullptr));chipnomadRender(s,a.data(),1024);chipnomadRender(s,a.data(),1024);for(float x:a)CHECK(x==0);}
 chipnomadDestroy(s);
}
#include "synth/opl_voice.h"
#include "synth/opll_voice.h"
#include "opl_patch.h"
TEST_CASE("Yamaha retriggers are sampled as separate native key transitions"){
 for(auto type:{InstrumentType::OPLL,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM}){
  CAPTURE(int(type));Instrument p{};getInstrumentFunctions(type).init(&p);
  auto verify=[&](auto& a,auto& b){a.noteOn();b.noteOn();std::vector<float>x(96000),y(96000);a.render(x.data(),48000);b.render(y.data(),48000);REQUIRE(x==y);a.noteOn();a.render(x.data(),48000);b.render(y.data(),48000);CHECK(x!=y);};
  if(type==InstrumentType::OPLL){OPLLVoice a,b;a.init(48000);b.init(48000);a.configure(&p.chip.opll,6900,1);b.configure(&p.chip.opll,6900,1);verify(a,b);}
  else if(isOPL(type)){OPLVoice a,b;a.init(48000);b.init(48000);a.configure(type,&p.chip.opl,6900,1);b.configure(type,&p.chip.opl,6900,1);verify(a,b);}
  else{FourOpVoice a,b;a.init(48000);b.init(48000);a.configure(type,&p.chip.fourOp,6900,1);b.configure(type,&p.chip.fourOp,6900,1);verify(a,b);}
 }
}
TEST_CASE("FourOp default patch matches directly clocked ymfm register reference"){
 for(auto type:{InstrumentType::GenesisFM,InstrumentType::ArcadeFM}){
  ymfm::ymfm_interface io;ymfm::ym2612 opn(io);ymfm::ym2151 opm(io);opn.reset();opm.reset();bool genesis=type==InstrumentType::GenesisFM;
  auto write=[&](int reg,int value){if(genesis){opn.write_address(reg);opn.write_data(value);}else{opm.write_address(reg);opm.write_data(value);}};
  // Literal register fixture for Twin Reed, A4, all operators enabled.
  const int opnRegs[][2]={{0x22,0},{0x27,0},{0x2b,0},{0xb0,4},{0xb4,192},{0x30,1},{0x40,30},{0x50,95},{0x60,10},{0x70,3},{0x80,87},{0x90,0},{0x38,1},{0x48,8},{0x58,95},{0x68,8},{0x78,2},{0x88,71},{0x98,0},{0x34,3},{0x44,40},{0x54,95},{0x64,12},{0x74,4},{0x84,103},{0x94,0},{0x3c,1},{0x4c,12},{0x5c,95},{0x6c,8},{0x7c,2},{0x8c,71},{0x9c,0},{0xa4,0x24},{0xa0,0x3b}};
  const int opmRegs[][2]={{0x0f,0},{0x18,0},{0x19,0},{0x19,128},{0x1b,0},{0x20,196},{0x38,0},{0x40,1},{0x60,30},{0x80,95},{0xa0,10},{0xc0,3},{0xe0,87},{0x50,1},{0x70,8},{0x90,95},{0xb0,8},{0xd0,2},{0xf0,71},{0x48,3},{0x68,40},{0x88,95},{0xa8,12},{0xc8,4},{0xe8,103},{0x58,1},{0x78,12},{0x98,95},{0xb8,8},{0xd8,2},{0xf8,71},{0x28,0x4a},{0x30,0}};
  if(genesis)for(const auto& r:opnRegs)write(r[0],r[1]);else for(const auto& r:opmRegs)write(r[0],r[1]);
  InstrumentFourOp patch{};initFourOpPatch(&patch);FourOpVoice voice;voice.init(48000);voice.configure(type,&patch,6900,1);voice.noteOn();std::vector<float>actual(96000);voice.render(actual.data(),48000);
  NativeResampler resampler;resampler.init(genesis?opn.sample_rate(7670454):opm.sample_rate(3579545),48000);bool first=true;float previous[2]{},filtered[2]{};float dc=std::exp(-2*3.14159265358979323846*20/48000);
  for(int frame=0;frame<48000;++frame){float l,r;resampler.next([&](float& left,float& right){if(genesis){ymfm::ym2612::output_data out;opn.generate(&out);left=(out.data[0]-504)/32768.f;right=(out.data[1]-504)/32768.f;}else{ymfm::ym2151::output_data out;opm.generate(&out);left=out.data[0]/32768.f;right=out.data[1]/32768.f;}if(first){first=false;write(genesis?0x28:0x08,genesis?0xf0:0x78);}},l,r);float v[]={l,r};for(int ch=0;ch<2;++ch){float output=v[ch]-previous[ch]+dc*filtered[ch];previous[ch]=v[ch];filtered[ch]=output;if(frame>=144)CHECK(actual[frame*2+ch]==output*(.25f*nativeChipGain(type)));}}
 }
}
