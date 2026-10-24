#include "packaged_presets.h"
#include "doctest.h"
#include "chipnomad_lib.h"
#include "sid_patch.h"
#include "synth/sid_voice.h"
#include <memory>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cstring>

TEST_CASE("SID factory programs load render multiple sample rates and release") {
 auto p=std::make_unique<Project>();projectInit(p.get());fillFXNames();int count=0;
 for(const auto& file:packagedPresets()) {
  if(file.path.find("sid-")!=0)continue;
  CAPTURE(file.path);int result=(loadFMPreset("packaging/common/instruments/FACTORY",file,p.get(),0)?0:1);INFO(projectFileError);REQUIRE(result==0);
  REQUIRE(p->instruments[0].type==InstrumentType::SID);auto patch=p->instruments[0].chip.sid;REQUIRE(validSID(patch));
  for(int rate:{44100,48000}){SIDVoice voice;voice.init(rate);voice.configure(&patch,6000,.25f);voice.noteOn();
   std::vector<float> audio(rate);voice.render(audio.data(),audio.size());double sum=0,squares=0;
   for(float v:audio){REQUIRE(std::isfinite(v));REQUIRE(std::abs(v)<1);sum+=v;squares+=v*v;}
   CHECK(squares-sum*sum/audio.size()>1e-7);
   voice.noteOff();for(int sec=0;sec<30&&voice.active();++sec)voice.render(audio.data(),audio.size());CHECK_FALSE(voice.active());
  }++count;
 }
 CHECK(count==56);projectFree(p.get());
}
TEST_CASE("SID render is independent of chunking and adjacent voices") {
 for(int rate:{22050,48000,96000})for(int wave:{1,2,3,4,5,6,7,8}) {
  CAPTURE(rate);CAPTURE(wave);InstrumentSID p;initSIDPatch(&p);p.value[sidWave]=wave;p.value[sidSync]=wave%2;p.value[sidRing]=1;
  SIDVoice a,b,other;a.init(rate);b.init(rate);other.init(rate);a.configure(&p,6900,.25);b.configure(&p,6900,.25);other.configure(&p,4700,.25);a.noteOn();b.noteOn();other.noteOn();
  std::vector<float>x(rate/4),y(x.size()),z(x.size());a.render(x.data(),x.size());other.render(z.data(),z.size());for(size_t n=0;n<y.size();){int k=std::min(size_t(113),y.size()-n);b.render(y.data()+n,k);n+=k;}CHECK(x==y);
  p.value[sidCutoff]=1900;a.configure(&p,7300,.25);b.configure(&p,7300,.25);a.render(x.data(),x.size());b.render(y.data(),y.size());CHECK(x==y);
  a.kill();a.render(x.data(),x.size());for(float v:x)REQUIRE(v==0);
 }
}
TEST_CASE("SID portable save roundtrip and invalid instrument load are transactional") {
 auto a=std::make_unique<Project>(),b=std::make_unique<Project>();projectInit(a.get());projectInit(b.get());fillFXNames();REQUIRE(!projectLoad(a.get(),"packaging/common/projects/gm-midi-demo.cct"));
 getInstrumentFunctions(InstrumentType::SID).init(&a->instruments[5]);auto& p=a->instruments[5].chip.sid;p.value[sidPulse]=1731;p.value[sidCutoff]=937;
 REQUIRE(!instrumentSave(a.get(),"build/tests/sid.cni",5));REQUIRE(!instrumentLoad(b.get(),"build/tests/sid.cni",8));CHECK(!memcmp(&p,&b->instruments[8].chip.sid,sizeof(p)));
 for(const char* name:{"build/tests/sid.cct","build/tests/sid.zip"}){REQUIRE(!projectSave(a.get(),name));REQUIRE(!projectLoad(b.get(),name));CHECK(!memcmp(&p,&b->instruments[5].chip.sid,sizeof(p)));}
 p.value[sidWave]=9;REQUIRE(!instrumentSave(a.get(),"build/tests/sid.cni",5));auto before=std::make_unique<Project>(*b);CHECK(instrumentLoad(b.get(),"build/tests/sid.cni",8)!=0);CHECK(!memcmp(before.get(),b.get(),sizeof(Project)));
 projectFree(a.get());projectFree(b.get());
}
TEST_CASE("SID queued preview owns recipe and stops without changing project") {
 auto* s=chipnomadCreate();s->project.chipsCount=1;s->project.tracksCount=3;s->project.tickRate=50;chipnomadInitChips(s,48000,nullptr);auto before=std::make_unique<Project>(s->project);InstrumentSID patch;initSIDPatch(&patch);REQUIRE(chipnomadQueueSIDPreview(s,0,&patch));memset(&patch,0,sizeof(patch));std::vector<float>a(2048);double energy=0;
 for(int n=0;n<20;++n){chipnomadRender(s,a.data(),1024);for(float v:a)energy+=v*v;}CHECK(energy>.001);CHECK(!memcmp(before.get(),&s->project,sizeof(Project)));
 REQUIRE(chipnomadQueueSIDPreview(s,0,nullptr));chipnomadRender(s,a.data(),1024);chipnomadRender(s,a.data(),1024);for(float v:a)CHECK(v==0);chipnomadDestroy(s);
}
TEST_CASE("FM brightness byte scaling preserves every legacy native value") {
 for(int v=-63;v<=63;++v)CHECK(fmBrightnessFromByte(fmBrightnessToByte(v))==v);
 CHECK(fmBrightnessFromByte(0)==-63);CHECK(fmBrightnessFromByte(128)==0);CHECK(fmBrightnessFromByte(255)==63);
}

TEST_CASE("SID voice budget accounts for silent sync partners and retains fresh notes") {
 InstrumentSID p;initSIDPatch(&p);SIDVoice v[5];SIDVoice* ptr[5];float audio[512];
 for(int i=0;i<5;++i){ptr[i]=&v[i];v[i].init(48000);v[i].configure(&p,6000,.25);v[i].noteOn();}
 CHECK(limitSIDVoices(ptr,5,4)==1);CHECK_FALSE(v[4].active());
 for(int i=0;i<4;++i)v[i].render(audio,512);v[4].noteOn();CHECK(limitSIDVoices(ptr,5,4)==1);CHECK(v[4].active());
 p.value[sidRing]=1;for(auto& voice:v){voice.configure(&p,6000,.25);voice.noteOn();}
 CHECK(limitSIDVoices(ptr,5,4)==3);int count=0;for(auto& voice:v)count+=voice.active();CHECK(count==2);
}
