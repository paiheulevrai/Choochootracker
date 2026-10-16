#include "doctest.h"
#include "formant_filter.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "screens.h"
#include "mocks/app_ui_mock.h"
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

TEST_SUITE("FORMANT") {
TEST_CASE("exact anchors and log interpolation") {
  const int positions[]={0,32,64,96,127};
  const float f1[]={850,390,240,360,250},f2[]={1610,2300,2400,640,595};
  for(int i=0;i<5;++i) {
    auto f=formantVowelFrequencies(positions[i]);
    CHECK(f.f1==f1[i]);CHECK(f.f2==f2[i]);
    if(i<4) {
      f=formantVowelFrequencies((positions[i]+positions[i+1])*.5f);
      CHECK(f.f1==doctest::Approx(std::sqrt(f1[i]*f1[i+1])));
      CHECK(f.f2==doctest::Approx(std::sqrt(f2[i]*f2[i+1])));
    }
  }
  for(float p=.01f;p<127;p+=.01f) {
    auto a=formantVowelFrequencies(p-.01f),b=formantVowelFrequencies(p);
    CHECK(std::abs(std::log(a.f1/b.f1))<.001f);
    CHECK(std::abs(std::log(a.f2/b.f2))<.001f);
  }
  CHECK(formantCustomFrequency(0,0)==doctest::Approx(200));
  CHECK(formantCustomFrequency(0,255)==doctest::Approx(1200));
  CHECK(formantCustomFrequency(1,0)==doctest::Approx(500));
  CHECK(formantCustomFrequency(1,255)==doctest::Approx(2800));
}
TEST_CASE("inactive controls leave audio identical and CUSTOM controls are independent") {
  for(int mode=0;mode<2;++mode) for(int parameter=1;parameter<4;++parameter) {
    uint8_t v[8]={uint8_t(mode),50,80,170,85,255};
    FormantFilter a,b;a.reset(48000);b.reset(48000);a.set(v);
    v[parameter]+=40;b.set(v);
    float l[2048],r[2048],x[2048],y[2048];
    for(int i=0;i<2048;++i)l[i]=r[i]=x[i]=y[i]=.2f*std::sin(i*.09f);
    a.process(l,r,2048);b.process(x,y,2048);
    double difference=0;
    for(int i=0;i<2048;++i)difference+=std::abs(l[i]-x[i]);
    if(insertParameterActive(insertFormant,parameter,mode))CHECK(difference>.01);
    else CHECK(difference==0);
  }
}
TEST_CASE("fast changes extreme Q low sample rates silence and stereo stay stable") {
  for(int rate:{1000,8000,32000,44100,48000,96000}) {
    FormantFilter f;f.reset(rate);
    uint8_t v[8]={0,0,0,0,255,255};
    float l[97],r[97];double peak=0;
    for(int block=0;block<1500;++block) {
      v[0]=(block/13)%2;v[1]=(block*17)%128;v[2]=block*73;v[3]=block*127;
      f.set(v);
      for(int i=0;i<97;++i) { l[i]=.8f*(2*((block*97+i)%109)/109.f-1);r[i]=0; }
      f.process(l,r,97);
      for(int i=0;i<97;++i) { CHECK(std::isfinite(l[i]));CHECK(std::abs(l[i])<=1.2f);CHECK(r[i]==0);peak=std::max(peak,double(std::abs(l[i]))); }
    }
    CHECK(peak>.001);
    f.reset(rate);f.set(v);memset(l,0,sizeof(l));memset(r,0,sizeof(r));f.process(l,r,97);
    for(int i=0;i<97;++i){CHECK(l[i]==0);CHECK(r[i]==0);}
    v[5]=0;f.reset(rate);f.set(v);
    for(int i=0;i<97;++i)l[i]=r[i]=.01f*i;
    f.process(l,r,97);
    for(int i=0;i<97;++i){CHECK(l[i]==.01f*i);CHECK(r[i]==l[i]);}
  }
}
TEST_CASE("mode changes gate live MOD without losing bases locks or assignments") {
  auto* s=chipnomadCreate();auto& p=s->project;
  p.chipsCount=1;p.tracksCount=1;p.tickRate=50;p.linearPitch=1;calculateLinearPitchTable12TET(&p);
  auto& ins=p.instruments[0];getInstrumentFunctions(InstrumentType::Braids).init(&ins);
  insertSelect(&p.trackInserts[0][0],insertFormant);
  auto& c=p.trackInserts[0][0];c.values[1]=20;c.values[2]=80;c.values[3]=100;c.values[5]=90;
  const int destinations[]={1,2,3,5};
  for(int i=0;i<4;++i) {
    auto& m=ins.modulation[i];m={};m.type=ModulationType::StickLinear;m.amount=32;
    m.destination=getInstrumentFunctions(ins.type).modDestinationsCount+1+genericModFirstInsert+destinations[i];
  }
  chipnomadSetLiveStickEnabled(1);chipnomadSetLiveStickAxes(1,0,0,0);
  chipnomadInitChips(s,48000,nullptr);chipnomadQueuePlaybackPreviewNote(s,0,45,0);
  float audio[1920];
  for(int mode:{0,1,0,1}) {
    insertEdit(&c,0,mode);REQUIRE(chipnomadQueueProjectRefresh(s));
    for(int i=0;i<4;++i)chipnomadRender(s,audio,960);
    CHECK(s->insertValues[0][0][0]==mode);
    for(int i=1;i<4;++i) {
      if(insertParameterActive(insertFormant,i,mode))CHECK(s->insertValues[0][0][i]>c.values[i]);
      else CHECK(s->insertValues[0][0][i]==c.values[i]);
      CHECK(ins.modulation[i-1].destination==getInstrumentFunctions(ins.type).modDestinationsCount+1+genericModFirstInsert+i);
    }
    CHECK(s->insertValues[0][0][5]>90);
    CHECK(c.values[5]==90);
    CHECK(c.values[1]==20);CHECK(c.values[2]==80);CHECK(c.values[3]==100);
  }
  // MODE automation and inactive parameter locks are retained by the insert chain.
  InsertChain chain(48000);InsertAutomation locks{};chain.sync(p.trackInserts[0],&locks);
  locks.valid[0]=(1<<1)|(1<<2);locks.values[0][1]=17;locks.values[0][2]=190;
  insertEdit(&c,0,0);chain.sync(p.trackInserts[0],&locks);
  CHECK(locks.valid[0]==6);CHECK(locks.values[0][1]==17);CHECK(locks.values[0][2]==190);
  fillFXNames();const char* path="build/tests/formant-roundtrip.cct";
  REQUIRE(projectSave(&p,path)==0);
  auto loaded=std::make_unique<Project>();projectInit(loaded.get());REQUIRE(projectLoad(loaded.get(),path)==0);
  CHECK(loaded->trackInserts[0][0].module==insertFormant);
  CHECK(memcmp(loaded->trackInserts[0][0].values,c.values,8)==0);
  for(int i=0;i<4;++i)CHECK(loaded->instruments[0].modulation[i].destination==ins.modulation[i].destination);
  projectFree(loaded.get());chipnomadDestroy(s);chipnomadSetLiveStickEnabled(0);chipnomadSetLiveStickAxes(0,0,0,0);
}
TEST_CASE("two slots and multiple tracks keep independent histories and bypass") {
  InsertConfig c[2]{};uint8_t v[2][8];InsertAutomation automation[3]{};
  for(int i=0;i<2;++i){insertSelect(&c[i],insertFormant);c[i].values[1]=i*64;memcpy(v[i],c[i].values,8);}
  InsertChain a(48000),b(48000),reference(48000);a.sync(c,&automation[0]);b.sync(c,&automation[1]);reference.sync(c,&automation[2]);
  float x[512],y[512],z[512];
  for(int n=0;n<20;++n) {
    for(int i=0;i<512;++i){x[i]=z[i]=.2f*std::sin((i+n*512)*.08f);y[i]=0;}
    a.process(x,256,v);b.process(y,256,v);reference.process(z,256,v);
    for(int i=0;i<512;++i){CHECK(x[i]==z[i]);CHECK(y[i]==0);}
  }
  c[0].bypass=c[1].bypass=1;a.sync(c,&automation[0]);
  for(int n=0;n<5;++n){for(auto& sample:x)sample=.1f;a.process(x,256,v);}
  for(auto sample:x)CHECK(sample==.1f);
}
TEST_CASE("insert page hides inactive controls and keeps hex editing") {
  auto* previous=chipnomadState;auto* previousScreen=currentScreen;
  chipnomadState=chipnomadCreate();screensInitAll();*pSongTrack=0;
  auto& c=chipnomadState->project.trackInserts[0][0];insertSelect(&c,insertFormant);
  screenInsertFXOpenTrack();screenInsertFX.setup(-1);screenInsertFX.fullRedraw();
  auto* d=mockScreenData;REQUIRE(d);
  CHECK(d->isCellValid(0,2));CHECK_FALSE(d->isCellValid(0,3));CHECK_FALSE(d->isCellValid(0,4));
  c.values[1]=127;d->onEdit(0,2,CellEditAction::increase);CHECK(c.values[1]==127);
  d->onEdit(0,1,CellEditAction::increase);
  CHECK(c.values[0]==1);CHECK_FALSE(d->isCellValid(0,2));CHECK(d->isCellValid(0,3));CHECK(d->isCellValid(0,4));
  chipnomadDestroy(chipnomadState);chipnomadState=previous;currentScreen=previousScreen;
}
}
