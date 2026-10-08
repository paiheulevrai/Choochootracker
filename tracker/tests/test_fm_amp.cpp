#include "doctest.h"
#include "project.h"
#include "fm_amp.h"
#include "opl_patch.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "synth/native_fm_amp.h"
#include <cmath>
#include <memory>
#include <cstring>
#include <vector>

TEST_CASE("FM event smoothing preserves steady signal and bridges retriggers") {
 for(float rate:{44100.f,48000.f,96000.f}) {
  CAPTURE(rate);NativeFMAmp amp;amp.init(rate);InstrumentFMAmp settings{};
  amp.configure(settings,1);amp.noteOn();
  CHECK(amp.process(1)==0);
  float previous=0;
  for(int n=1;n<int(rate*.003f);++n) {
   float value=amp.process(1);CHECK(value>=previous);CHECK(value-previous<.012f);previous=value;
  }
  CHECK(amp.process(1)==1);CHECK(amp.process(.25f)==.25f);
  amp.noteOn();CHECK(amp.process(-1)==.25f);
  previous=.25f;
  for(int n=1;n<int(rate*.003f);++n) {
   float value=amp.process(-1);CHECK(std::abs(value-previous)<.015f);previous=value;
  }
  CHECK(amp.process(-1)==-1);
 }
}
TEST_CASE("FM optional amp ADSR shapes and releases independently of its native envelope") {
 NativeFMAmp amp;amp.init(48000);InstrumentFMAmp p{};
 p.enabled=1;p.attack=36;p.decay=36;p.sustain=128;p.release=36;p.envelopeShape=128;
 amp.configure(p,1);amp.noteOn();
 CHECK(amp.process(1)==0);
 float atTen=0,held=0;
 for(int n=1;n<15000;++n){float x=amp.process(1);if(n==480)atTen=x;held=x;}
 CHECK(atTen<.12f);CHECK(held==doctest::Approx(128.f/255));
 amp.noteOff();for(int n=0;n<6000;++n)held=amp.process(1);CHECK(held==0);
 p.enabled=0;amp.configure(p,1);CHECK(amp.process(1)==1);
}
TEST_CASE("FM amp extension is strict and optional") {
 InstrumentFMAmp p{};bool seen=false;
 CHECK(loadFMAmpSetting("- FM amp: 1,12,34,56,78,128",p,seen)==1);
 CHECK(p.enabled==1);CHECK(p.attack==12);CHECK(p.sustain==56);
 CHECK(loadFMAmpSetting("- FM amp: 1,12,34,56,78,128",p,seen)==-1);
 for(const char* bad:{"- FM amp: 2,0,0,0,0,0","- FM amp: 1,-1,0,0,0,0","- FM amp: 1,0,0,256,0,0","- FM amp: 1,0,0,0,0,0junk"}) {
  seen=false;CHECK(loadFMAmpSetting(bad,p,seen)==-1);
 }
}
TEST_CASE("All native FM types save reopen their independent amp and expose ADSR modulation") {
 auto p=std::make_unique<Project>(),q=std::make_unique<Project>();projectInit(p.get());projectInit(q.get());fillFXNames();
 REQUIRE(projectLoad(p.get(),"packaging/common/projects/gm-midi-demo.cct")==0);
 for(auto type:{InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7}) {
  CAPTURE(int(type));getInstrumentFunctions(type).init(&p->instruments[0]);
  auto* a=instrumentFMAmpSettings(&p->instruments[0]);REQUIRE(a);CHECK(a->enabled==0);
  a->enabled=1;a->attack=17;a->decay=91;a->sustain=123;a->release=47;a->envelopeShape=99;
  auto* tone=instrumentFMToneSettings(&p->instruments[0]);tone->brightness=-27;tone->feedback=6;
  auto before=p->instruments[0];
  REQUIRE(instrumentSave(p.get(),"test_fm_amp.cni",0)==0);
  REQUIRE(instrumentLoad(q.get(),"test_fm_amp.cni",2)==0);
  CHECK(!memcmp(a,instrumentFMAmpSettings(&q->instruments[2]),sizeof(*a)));
  CHECK(!memcmp(tone,instrumentFMToneSettings(&q->instruments[2]),sizeof(*tone)));
  CHECK(!memcmp(&before,&p->instruments[0],sizeof(before)));
  const int first=getInstrumentFunctions(type).modDestinationsCount+1;
  CHECK(instrumentModDestinationAvailable(&p->instruments[0],first+genericModEnvelopeAttack));
  CHECK_FALSE(instrumentModDestinationAvailable(&p->instruments[0],first+genericModTriggerDecay));
  REQUIRE(projectSave(p.get(),"test_fm_amp.cct")==0);
  REQUIRE(projectLoad(q.get(),"test_fm_amp.cct")==0);
  CHECK(!memcmp(a,instrumentFMAmpSettings(&q->instruments[0]),sizeof(*a)));
  CHECK(!memcmp(tone,instrumentFMToneSettings(&q->instruments[0]),sizeof(*tone)));
 }
 std::remove("test_fm_amp.cni");std::remove("test_fm_amp.cct");projectFree(p.get());projectFree(q.get());
}

TEST_CASE("FM tone extension rejects out of range and duplicate controls") {
 InstrumentFMTone t{};bool seen=false;
 CHECK(loadFMToneSetting("- FM tone: -63,8",t,seen)==1);
 CHECK(t.brightness==-63);CHECK(t.feedback==8);
 CHECK(loadFMToneSetting("- FM tone: 0,0",t,seen)==-1);
 for(const char* bad:{"- FM tone: -64,0","- FM tone: 64,0","- FM tone: 0,9","- FM tone: 0,-1","- FM tone: 0,0junk"}) {
  seen=false;CHECK(loadFMToneSetting(bad,t,seen)==-1);
 }
}
TEST_CASE("Native macros append IDs and expose only supported controls") {
 CHECK(genericModFirstInsert==29);CHECK(genericModFMBrightness==45);
 CHECK(fxSLI==fxF28+1);CHECK(fxFBR==fxSLI+1);CHECK(fxTotalCount<255); /* merged: our fxSLI keeps its pre-merge ID between fxF28 and upstream's native FX block */fillFXNames();
 for(int type=0;type<int(InstrumentType::totalCount);++type) {
  Instrument i{};getInstrumentFunctions(InstrumentType(type)).init(&i);
  for(int g=genericModFMBrightness;g<genericModInstrumentPan;++g) {
   int dest=getInstrumentFunctions(i.type).modDestinationsCount+1+g;
   const auto* d=instrumentNativeModDestination(i.type,g);
   const int directIndex=g-genericModFirstDirectFM;
   const bool unusedOPL=isOPL(i.type)&&i.chip.opl.topology==OPLTopology::twoOperator&&
     ((g>=genericModFMOperator3&&g<=genericModFMOperator6)||(directIndex>=24&&directIndex<72));
      CHECK(bool(instrumentModDestinationAvailable(&i,dest))==(bool(d)&&!unusedOPL));
   REQUIRE(instrumentModDestinationName(i.type,dest));
   CAPTURE(type);CAPTURE(g);
   if(d&&!unusedOPL){
    // Live modulation retains native operator targets that were deliberately
    // removed from the compact phrase/table FX list. Check their native
    // metadata here; tracker exposure has a separate per-engine contract.
    int nativeFX,op;
    if(nativeFMModTarget(g,&nativeFX,&op)) {
     NativeFXInfo info{};
     REQUIRE(instrumentNativeFXInfo(&i,nativeFX,&info,op));
     CHECK(d->fx==nativeFX);CHECK(d->range==info.maximum);
    }
    if(d->fx<fxOAR||d->fx>fxLEN)CHECK(instrumentFXAvailableForInstrument(&i,d->fx));
    if(instrumentFXAvailableForInstrument(&i,d->fx))CHECK(strcmp(fxNames[d->fx].name,"---"));
    uint8_t fx;int base,range;InstrumentMotionValue encoding;
    REQUIRE(instrumentMotionDestination(&i,dest,&fx,&base,&range,&encoding));
    CHECK(fx==d->fx);CHECK(base>=0);CHECK(base<=range);
   }
  }
 }
}

TEST_CASE("Pan modulation destinations map to pan FX") {
 for(int type=0;type<int(InstrumentType::totalCount);++type) {
  Instrument i{};getInstrumentFunctions(InstrumentType(type)).init(&i);
  int first=getInstrumentFunctions(i.type).modDestinationsCount+1;
  for(int g : {genericModInstrumentPan,genericModTrackPan}) {
   int dest=first+g;
   const bool available=i.type!=InstrumentType::Midi;
   CHECK(bool(instrumentModDestinationAvailable(&i,dest))==available);
   if(!available)continue;
   uint8_t fx;int base,range;InstrumentMotionValue encoding;
   REQUIRE(instrumentMotionDestination(&i,dest,&fx,&base,&range,&encoding));
   CHECK(fx==(g==genericModInstrumentPan?fxPAN:fxTPN));
   CHECK(base==128);CHECK(range==255);CHECK(encoding==InstrumentMotionValue::raw);
  }
 }
}

TEST_CASE("OPL3 control availability follows topology changes on the actual instrument") {
  Instrument instrument{};
  getInstrumentFunctions(InstrumentType::OPL3).init(&instrument);
  fillFXNames();
  const int first = getInstrumentFunctions(instrument.type).modDestinationsCount + 1;
  // Exercise the same cache and instrument in both directions.
  for (auto topology : {OPLTopology::twoOperator, OPLTopology::fourOperator,
                        OPLTopology::dualVoice, OPLTopology::twoOperator}) {
    instrument.chip.opl.topology = topology;
    const int count = topology == OPLTopology::twoOperator ? 2 : 4;
    CAPTURE(int(topology));
    CHECK(instrumentFMOperatorCount(&instrument) == count);
    for (int op = 0; op < 6; ++op) {
      CAPTURE(op);
      const bool available = op < count;
      const int level = first + genericModFMOperator1 + op;
      CHECK(bool(instrumentModDestinationAvailable(&instrument, level)) == available);
      CHECK(bool(instrumentFXAvailableForInstrument(&instrument, fxOL1 + op)) == available);
      NativeFXInfo info{};
      CHECK(instrumentNativeFXInfo(&instrument, fxOL1 + op, &info) == available);
      for (int fx : {fxOAR, fxODR, fxORR, fxOSL, fxOMU}) {
        const int dest = first + genericModFirstDirectFM + op * 12 + fx - fxOAR;
        CHECK(bool(instrumentModDestinationAvailable(&instrument, dest)) == available);
        CHECK(instrumentNativeFXInfo(&instrument, fx, &info, op) == available);
        if (available) {
          REQUIRE(instrumentModDestinationName(instrument.type, dest));
          CHECK(std::strstr(instrumentModDestinationName(instrument.type, dest), "OP") != nullptr);
        }
      }
    }
  }
}

TEST_CASE("OPL3 topology operator settings and modulation survive CNI and CCT reload") {
  auto saved = std::make_unique<Project>(), loaded = std::make_unique<Project>();
  projectInit(saved.get());
  projectInit(loaded.get());
  fillFXNames();
  REQUIRE(projectLoad(saved.get(), "packaging/common/projects/gm-midi-demo.cct") == 0);
  auto& instrument = saved->instruments[0];
  getInstrumentFunctions(InstrumentType::OPL3).init(&instrument);
  const int first = getInstrumentFunctions(instrument.type).modDestinationsCount + 1;
  for (auto topology : {OPLTopology::twoOperator, OPLTopology::fourOperator, OPLTopology::dualVoice}) {
    CAPTURE(int(topology));
    instrument.chip.opl.topology = topology;
    const int count = topology == OPLTopology::twoOperator ? 2 : 4;
    for (int op = 0; op < count; ++op) {
      instrument.chip.opl.operators[op].attack = 7 + op;
      instrument.chip.opl.operators[op].multiplier = 2 + op;
      instrument.chip.opl.operators[op].level = 10 + op;
    }
    instrument.modulation[0].destination = first + genericModFirstDirectFM + (count - 1) * 12 + fxOMU - fxOAR;
    instrument.modulation[0].amount = 43;
    REQUIRE(instrumentSave(saved.get(), "build/tests/opl3-review.cni", 0) == 0);
    REQUIRE(instrumentLoad(loaded.get(), "build/tests/opl3-review.cni", 0) == 0);
    auto checkReload = [&] {
      const auto& actual = loaded->instruments[0];
      CHECK(actual.type == InstrumentType::OPL3);
      CHECK(std::memcmp(&actual.chip.opl, &instrument.chip.opl, sizeof(InstrumentOPL)) == 0);
      CHECK(std::memcmp(actual.modulation, instrument.modulation, sizeof(instrument.modulation)) == 0);
      CHECK(instrumentModDestinationAvailable(&actual, actual.modulation[0].destination));
      CHECK(bool(instrumentFXAvailableForInstrument(&actual, fxOL3)) == (count == 4));
    };
    checkReload();
    REQUIRE(projectSave(saved.get(), "build/tests/opl3-review.cct") == 0);
    REQUIRE(projectLoad(loaded.get(), "build/tests/opl3-review.cct") == 0);
    checkReload();
  }
  projectFree(saved.get());
  projectFree(loaded.get());
  std::remove("build/tests/opl3-review.cni");
  std::remove("build/tests/opl3-review.cct");
}
