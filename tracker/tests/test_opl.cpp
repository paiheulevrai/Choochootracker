#include "packaged_presets.h"
#include "doctest.h"
#include "project.h"
#include "opl_patch.h"
#include "synth/opl_voice.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
TEST_CASE("OPL topology and native range validation"){
  InstrumentOPL p{};initOPLPatch(&p);CHECK(validOPL(InstrumentType::OPL2,p));
  p.topology=OPLTopology::fourOperator;CHECK_FALSE(validOPL(InstrumentType::OPL2,p));CHECK(validOPL(InstrumentType::OPL3,p));
  p.operators[2].waveform=8;CHECK_FALSE(validOPL(InstrumentType::OPL3,p));p.operators[2].waveform=0;p.schema=2;CHECK_FALSE(validOPL(InstrumentType::OPL3,p));
  CHECK(sizeof(InstrumentOPL)<=sizeof(InstrumentChipData));
}
TEST_CASE("OPL renders two, four and dual voice configurations with stable blocks"){
  for(auto type:{InstrumentType::OPL2,InstrumentType::OPL3})for(int topology=0;topology<(type==InstrumentType::OPL2?1:3);++topology)for(int rate:{44100,48000,96000}){
    CAPTURE(int(type));CAPTURE(topology);CAPTURE(rate);
    InstrumentOPL p{};initOPLPatch(&p);p.topology=(OPLTopology)topology;p.operators[2]=p.operators[0];p.operators[3]=p.operators[1];p.connection[1]=1;p.secondDetune=4;
    OPLVoice a,b;a.init(rate);b.init(rate);a.configure(type,&p,6000,1);b.configure(type,&p,6000,1);a.noteOn();b.noteOn();
    std::vector<float>x(rate/2),y(x.size());a.render(x.data(),x.size()/2);
    for(size_t i=0;i<x.size()/2;){size_t n=std::min(size_t(113),x.size()/2-i);b.render(y.data()+i*2,n);i+=n;}
    double energy=0;for(size_t i=0;i<x.size();++i){REQUIRE(std::isfinite(x[i]));CHECK(x[i]==y[i]);CHECK(std::abs(x[i])<1);energy+=x[i]*x[i];}CHECK(energy>.001);
    a.kill();a.render(x.data(),x.size()/2);for(float v:x)REQUIRE(v==0);
  }
}
TEST_CASE("Every packaged OPL preset reloads and renders finite audible native audio"){
  auto p=std::make_unique<Project>();projectInit(p.get());fillFXNames();
  const std::filesystem::path folder="packaging/common/instruments/FACTORY";
  REQUIRE(std::filesystem::exists(folder/"catalog.tsv"));
  int count=0,silent=0;double peak=0;std::vector<float> audio(24000);
  for(const auto& entry:packagedPresets()){
    if(false)continue;
    REQUIRE((loadFMPreset("packaging/common/instruments/FACTORY",entry,p.get(),0)?0:1)==0);
    auto& instrument=p->instruments[0];if(!isOPL(instrument.type))continue;
    CAPTURE(entry.path);REQUIRE(validOPL(instrument.type,instrument.chip.opl));
    OPLVoice voice;voice.init(48000);voice.configure(instrument.type,&instrument.chip.opl,6000,1);voice.noteOn();voice.render(audio.data(),audio.size()/2);
    double energy=0;for(float v:audio){REQUIRE(std::isfinite(v));peak=std::max(peak,double(std::abs(v)));energy+=v*v;}
    // Very slow source envelopes (e.g. Seashore AT=1) need a held audition.
    for(int held=0;energy<1e-9 && held<48;++held){voice.render(audio.data(),audio.size()/2);for(float v:audio){REQUIRE(std::isfinite(v));energy+=v*v;}}
    if(energy<1e-9)++silent;
    CHECK(energy>1e-9);++count;
  }
  CHECK(count==697);CHECK(silent==0);CHECK(peak<1);projectFree(p.get());
}
TEST_CASE("OPL stereo layers stay distinct and changing topology clears stale pairs"){
  InstrumentOPL p{};initOPLPatch(&p);p.topology=OPLTopology::dualVoice;p.pan[0]=1;p.pan[1]=2;p.noteOffset[1]=7;p.operators[2]=p.operators[0];p.operators[3]=p.operators[1];
  OPLVoice a;a.init(48000);a.configure(InstrumentType::OPL3,&p,6000,1);a.noteOn();std::vector<float>out(4096);a.render(out.data(),2048);
  double delta=0;for(size_t i=0;i<out.size();i+=2)delta+=std::abs(out[i]-out[i+1]);CHECK(delta>1);
  p.topology=OPLTopology::twoOperator;p.pan[0]=3;a.configure(InstrumentType::OPL3,&p,6000,1);a.noteOn();a.render(out.data(),2048);
  // The 3 ms transition bridges the previous stereo output; pairing must
  // be fully mono once that bounded transition has finished.
  for(size_t i=2*144;i<out.size();i+=2)CHECK(out[i]==out[i+1]);
}
