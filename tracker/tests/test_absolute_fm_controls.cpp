#include "doctest.h"
#include "chipnomad_lib.h"
#include "playback_internal.h"
#include "pitch_table_utils.h"
#include "project_utils.h"
#include "sid_patch.h"
#include "chipnomad_lib_live_stick.h"
#include "synth/opll_voice.h"
#include "synth/opl_voice.h"
#include "synth/four_op_voice.h"
#include "synth/dx7_voice.h"
#include <memory>
#include <vector>
#include <cstring>

static void setNative(Instrument& i,int fx,int op,int v) {
  if(fx==fxFBK){instrumentFMToneSettings(&i)->feedback=v+1;return;}
  if(i.type==InstrumentType::DX7) {
    if(fx>=fxLFR) {
      const int fields[]={137,140,139,-1,143,-1};i.chip.dx7.voice[fields[fx-fxLFR]]=v;
    } else {const int fields[]={0,1,2,3,6,20,18,19,17,4,5,7};i.chip.dx7.voice[(5-op)*21+fields[fx-fxOAR]]=v;}
  } else if(i.type==InstrumentType::GenesisFM||i.type==InstrumentType::ArcadeFM) {
    auto& p=i.chip.fourOp;auto& o=p.operators[op];
    switch(fx) {
      case fxOAR:o.attack=v;break;case fxODR:o.decay=v;break;case fxOSR:o.sustainRate=v;break;
      case fxORR:o.release=v;break;case fxOSL:o.sustainLevel=v;break;case fxODT:o.detune=v;break;case fxOMU:o.multiplier=v;break;
      case fxLFR:p.lfoRate=v;break;case fxLAD:p.amplitudeDepth=v;break;case fxLPD:p.pitchDepth=v;break;
      case fxLAS:p.amplitudeSensitivity=v;break;case fxLPS:p.pitchSensitivity=v;break;case fxLEN:p.lfoEnabled=v;break;
    }
  } else if(i.type==InstrumentType::OPL2||i.type==InstrumentType::OPL3) {
    auto& o=i.chip.opl.operators[op];
    switch(fx){case fxOAR:o.attack=v;break;case fxODR:o.decay=v;break;case fxORR:o.release=v;break;case fxOSL:o.sustain=v;break;case fxOMU:o.multiplier=v;break;}
  } else {
    auto* p=i.chip.opll.patch;
    switch(fx){case fxOAR:p[4+op]=(p[4+op]&15)|(v<<4);break;case fxODR:p[4+op]=(p[4+op]&240)|v;break;case fxORR:p[6+op]=(p[6+op]&240)|v;break;case fxOSL:p[6+op]=(p[6+op]&15)|(v<<4);break;case fxOMU:p[op]=(p[op]&240)|v;break;}
  }
}

TEST_CASE("Direct FM metadata round trips every legal native value on every operator") {
  for(auto type:{InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7}) {
    Instrument i{};getInstrumentFunctions(type).init(&i);
    if(type==InstrumentType::OPL3)i.chip.opl.topology=OPLTopology::fourOperator;
    for(int op=0;op<instrumentFMOperatorCount(&i);++op)for(int fx=fxOAR;fx<=fxLEN;++fx) {
      NativeFXInfo info{};if(!instrumentNativeFXInfo(&i,fx,&info,op))continue;
      CAPTURE(int(type));CAPTURE(op);CAPTURE(fx);CHECK_FALSE(info.relative);REQUIRE(info.label);
      for(int v=info.minimum;v<=info.maximum;++v) {
        setNative(i,fx,op,v);NativeFXInfo actual{};REQUIRE(instrumentNativeFXInfo(&i,fx,&actual,op));CHECK(actual.preset==v);
      }
    }
  }
}

static std::vector<float> renderNative(InstrumentType type,int fx,int op,int value,bool command,bool modulate=false) {
  auto s=std::unique_ptr<ChipNomadState,decltype(&chipnomadDestroy)>(chipnomadCreate(),chipnomadDestroy);
  auto& p=s->project;REQUIRE(projectLoad(&p,"packaging/common/projects/gm-midi-demo.cct")==0);
  p.tracksCount=1;p.tickRate=50;p.linearPitch=1;calculateLinearPitchTable12TET(&p);
  for(auto& g:p.grooves)for(auto& speed:g.speed)speed=50;
  p.song[0][0]=0;p.chains[0].rows[0].phrase=0;p.chains[0].rows[0].transpose=0;
  phraseClear(&p.phrases[0]);auto& i=p.instruments[0];getInstrumentFunctions(type).init(&i);
  if(type==InstrumentType::OPL3)i.chip.opl.topology=OPLTopology::fourOperator;
  auto& row=p.phrases[0].rows[0];row.note=45;row.instrument=0;row.volume=PHRASE_VOLUME_MAX;
  if(modulate) {
    auto& m=i.modulation[0];m={};m.type=ModulationType::StickLinear;m.amount=127;
    int g=fx==fxFBK?genericModFMFeedback:genericModFirstDirectFM+(fx>=fxLFR?72+fx-fxLFR:op*12+fx-fxOAR);
    m.destination=getInstrumentFunctions(type).modDestinationsCount+1+g;
    chipnomadSetLiveStickEnabled(1);chipnomadSetLiveStickAxes(1,0,0,0);
    if(fx==fxFBK){row.fx[1][0]=fxFBK;row.fx[1][1]=0;}
  } else if(command){row.fx[0][0]=fx;row.fx[0][1]=value;}
  else setNative(i,fx,op,value);
  auto before=std::make_unique<Instrument>(i);
  chipnomadInitChips(s.get(),48000,nullptr);chipnomadReserveRenderBuffers(s.get(),480);
  REQUIRE(chipnomadQueueProjectRefresh(s.get()));REQUIRE(chipnomadQueuePlaybackStartSong(s.get(),0,0,1));
  std::vector<float> out(1920);for(int b=0;b<2;++b)REQUIRE(chipnomadRender(s.get(),out.data()+b*960,480)==480);
  CHECK(!memcmp(before.get(),&i,sizeof(i)));
  if(modulate){chipnomadSetLiveStickEnabled(0);chipnomadSetLiveStickAxes(0,0,0,0);}
  return out;
}

TEST_CASE("Native modulation bindings address fixed operators and clamp to native maxima") {
  for(auto type:{InstrumentType::OPLL,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7}) {
    Instrument i{};getInstrumentFunctions(type).init(&i);
    if(type==InstrumentType::OPL3)i.chip.opl.topology=OPLTopology::fourOperator;
    int op=instrumentFMOperatorCount(&i)-1;
    NativeFXInfo info{};REQUIRE(instrumentNativeFXInfo(&i,fxOMU,&info,op));
    CAPTURE(int(type));
    CHECK(renderNative(type,fxOMU,op,info.maximum,false,true)==renderNative(type,fxOMU,op,info.maximum,false));
    CHECK(renderNative(type,fxFBK,0,7,false,true)==renderNative(type,fxFBK,0,7,false));
  }
}

static std::vector<float> renderAdapter(InstrumentType type,int fx,int op,int value,bool direct) {
  Instrument i{};getInstrumentFunctions(type).init(&i);
  if(type==InstrumentType::OPL3)i.chip.opl.topology=OPLTopology::fourOperator;
  if(direct) {
    auto& d=instrumentFMToneSettings(&i)->direct;
    if(fx>=fxLFR)d.global[fx-fxLFR]=value+1;else d.operators[op][fx-fxOAR]=value+1;
  } else setNative(i,fx,op,value);
  std::vector<float> out(1920);
  if(type==InstrumentType::DX7) {
    DX7Part v;v.init(48000);v.voices[0].configure(&i.chip.dx7,5700,1);v.voices[0].noteOn();v.render(out.data(),960);
  } else if(type==InstrumentType::GenesisFM||type==InstrumentType::ArcadeFM) {
    FourOpVoice v;v.init(48000);v.configure(type,&i.chip.fourOp,5700,1);v.noteOn();v.render(out.data(),960);
  } else if(type==InstrumentType::OPL2||type==InstrumentType::OPL3) {
    OPLVoice v;v.init(48000);v.configure(type,&i.chip.opl,5700,1);v.noteOn();v.render(out.data(),960);
  } else {
    OPLLVoice v;v.init(48000);v.configure(&i.chip.opll,5700,1);v.noteOn();v.render(out.data(),960);
  }
  return out;
}

TEST_CASE("Direct FM voice controls render identically to editing the exact preset field") {
  for(auto type:{InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7}) {
    Instrument i{};getInstrumentFunctions(type).init(&i);if(type==InstrumentType::OPL3)i.chip.opl.topology=OPLTopology::fourOperator;
    for(int op:{0,instrumentFMOperatorCount(&i)-1})for(int fx=fxOAR;fx<=fxLEN;++fx) {
      NativeFXInfo info{};if(!instrumentNativeFXInfo(&i,fx,&info,op))continue;
      for(int value:{info.minimum,info.preset,info.maximum}) {
        CAPTURE(int(type));CAPTURE(op);CAPTURE(fx);CAPTURE(value);CHECK(renderAdapter(type,fx,op,value,true)==renderAdapter(type,fx,op,value,false));
      }
    }
  }
}

TEST_CASE("Simplified tracker FM controls match the per-engine list") {
  for(auto type:{InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7}) {
    Instrument i{};getInstrumentFunctions(type).init(&i);
    const bool opl=type==InstrumentType::OPLL||type==InstrumentType::VRC7||type==InstrumentType::OPL2||type==InstrumentType::OPL3;
    const bool four=type==InstrumentType::GenesisFM||type==InstrumentType::ArcadeFM;
    CHECK_FALSE(instrumentFXAvailableForInstrument(&i,fxFBK+1)); // Removed selector ID.
    for(int fx=fxOAR;fx<=fxLEN;++fx) {
      const bool expected=opl?(fx==fxOAR||fx==fxODR||fx==fxORR||fx==fxOSL||fx==fxOMU):
        four?(fx==fxOMU||fx==fxLFR||fx==fxLAS||fx==fxLPS||fx==fxLEN||(type==InstrumentType::ArcadeFM&&(fx==fxLAD||fx==fxLPD))):false;
      CAPTURE(int(type));CAPTURE(fx);
      CHECK(bool(instrumentFXAvailableForInstrument(&i,fx))==expected);
    }
  }
}

TEST_CASE("Retained multiplier commands address operator one") {
  for(auto type:{InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM}) {
    CAPTURE(int(type));
    CHECK(renderNative(type,fxOMU,0,7,true)==renderNative(type,fxOMU,0,7,false));
  }
}

TEST_CASE("SID direct values use native numbering including nonzero minima") {
  Instrument i{};getInstrumentFunctions(InstrumentType::SID).init(&i);
  for(auto item:{std::pair<int,int>{fxSWV,sidWave},{fxSMR,sidMacroRate},{fxSPR,sidPartnerRatio}}) {
    NativeFXInfo info{};REQUIRE(instrumentNativeFXInfo(&i,item.first,&info));CHECK(info.minimum==1);
    for(int value=1;value<=info.maximum;++value){i.chip.sid.value[item.second]=value;REQUIRE(instrumentNativeFXInfo(&i,item.first,&info));CHECK(info.preset==value);}
  }
}
