#include "doctest.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "simple_chip_presets.h"
#include "sid_patch.h"
#include "external/msfa/env.h"
#include <memory>
#include <vector>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

TEST_CASE("Native FX limits and preset values follow the selected instrument") {
  for(auto type:{InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7}) {
    Instrument i{};getInstrumentFunctions(type).init(&i);
    for(int op=0;op<6;++op) {
      NativeFXInfo info{};bool present=instrumentNativeFXInfo(&i,fxOL1+op,&info);
      CHECK(present==(op<instrumentFMOperatorCount(&i)));
      if(!present)continue;
      int max=type==InstrumentType::DX7?99:(type==InstrumentType::GenesisFM||type==InstrumentType::ArcadeFM)?127:((type==InstrumentType::OPLL||type==InstrumentType::VRC7)&&op==1)?15:63;
      CHECK(info.maximum==max);CHECK(info.preset>=0);CHECK(info.preset<=max);CHECK_FALSE(info.relative);
    }
  }
  Instrument i{};NativeFXInfo info{};
  getInstrumentFunctions(InstrumentType::GBPulse).init(&i);
  REQUIRE(instrumentNativeFXInfo(&i,fxCMD,&info));CHECK(info.maximum==3);CHECK(info.preset==1);
  i.chip.simpleChip.mode=2;
  REQUIRE(instrumentNativeFXInfo(&i,fxCMD,&info));CHECK(info.preset==2);
  REQUIRE(instrumentNativeFXInfo(&i,fxCSS,&info));CHECK(info.maximum==7);
  REQUIRE(instrumentNativeFXInfo(&i,fxCED,&info));CHECK(info.maximum==1);
  getInstrumentFunctions(InstrumentType::DX7).init(&i);
  i.chip.dx7.voice[5*21+16]=42;i.chip.dx7.voice[4*21+16]=91;
  REQUIRE(instrumentNativeFXInfo(&i,fxOL1,&info));CHECK(info.preset==42);
  REQUIRE(instrumentNativeFXInfo(&i,fxOL2,&info));CHECK(info.preset==91);
  CHECK_FALSE(instrumentNativeFXInfo(&i,fxFET,&info));
  REQUIRE(instrumentNativeFXInfo(&i,fxOAR,&info));CHECK_FALSE(info.relative);CHECK(info.preset==i.chip.dx7.voice[105]);
  i.chip.dx7.voice[135]=5;
  REQUIRE(instrumentNativeFXInfo(&i,fxFBK,&info));CHECK(info.maximum==7);CHECK(info.preset==5);
}

namespace {
std::vector<float> renderAbsolute(InstrumentType type,int value,bool fx) {
  auto state=std::unique_ptr<ChipNomadState,decltype(&chipnomadDestroy)>(chipnomadCreate(),chipnomadDestroy);
  auto& p=state->project;
  REQUIRE(projectLoad(&p,"packaging/common/projects/gm-midi-demo.cct")==0);
  p.tracksCount=1;p.tickRate=50;p.linearPitch=1;calculateLinearPitchTable12TET(&p);
  for(auto& g:p.grooves)for(auto& speed:g.speed)speed=50;
  p.song[0][0]=0;p.chains[0].rows[0].phrase=0;p.chains[0].rows[0].transpose=0;
  phraseClear(&p.phrases[0]);auto& i=p.instruments[0];getInstrumentFunctions(type).init(&i);
  auto& row=p.phrases[0].rows[0];row.note=45;row.instrument=0;row.volume=PHRASE_VOLUME_MAX;
  if(fx){row.fx[0][0]=fxOL1;row.fx[0][1]=value;}
  else if(type==InstrumentType::DX7)i.chip.dx7.voice[5*21+16]=value;
  else if(type==InstrumentType::OPLL||type==InstrumentType::VRC7)i.chip.opll.patch[2]=(i.chip.opll.patch[2]&0xc0)|(63-value);
  else if(type==InstrumentType::OPL2||type==InstrumentType::OPL3)i.chip.opl.operators[0].level=63-value;
  else i.chip.fourOp.operators[0].level=127-value;
  auto saved=std::make_unique<Project>(p);
  chipnomadInitChips(state.get(),48000,nullptr);chipnomadReserveRenderBuffers(state.get(),480);
  REQUIRE(chipnomadQueueProjectRefresh(state.get()));REQUIRE(chipnomadQueuePlaybackStartSong(state.get(),0,0,1));
  std::vector<float> out(9600);
  for(int b=0;b<10;++b)REQUIRE(chipnomadRender(state.get(),out.data()+b*960,480)==480);
  CHECK(!memcmp(saved.get(),&p,sizeof(Project)));
  return out;
}
}

TEST_CASE("Absolute operator FX reproduce native preset edits at endpoints and midrange") {
  for(auto type:{InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7}) {
    Instrument i{};getInstrumentFunctions(type).init(&i);NativeFXInfo info{};
    REQUIRE(instrumentNativeFXInfo(&i,fxOL1,&info));
    CAPTURE(int(type));
    for(int value:{0,info.maximum/2,info.maximum}) {
      CAPTURE(value);CHECK(renderAbsolute(type,value,true)==renderAbsolute(type,value,false));
    }
    CHECK(renderAbsolute(type,0,true)!=renderAbsolute(type,info.maximum,true));
  }
}

TEST_CASE("Direct native commands survive song and instrument saves") {
  fillFXNames();
  auto p=std::make_unique<Project>();projectInit(p.get());
  REQUIRE(projectLoad(p.get(),"packaging/common/projects/gm-midi-demo.cct")==0);
  getInstrumentFunctions(InstrumentType::DX7).init(&p->instruments[0]);
  p->phrases[0].rows[0].fx[0][0]=fxOL1;p->phrases[0].rows[0].fx[0][1]=42;
  p->phrases[0].rows[0].fx[1][0]=fxOAR;p->phrases[0].rows[0].fx[1][1]=128;
  p->phrases[0].rows[0].fx[2][0]=fxFBK;p->phrases[0].rows[0].fx[2][1]=6;
  p->tables[0].rows[0].fx[0][0]=fxOL2;p->tables[0].rows[0].fx[0][1]=63;
  p->tables[0].rows[0].fx[1][0]=fxLFR;p->tables[0].rows[0].fx[1][1]=8;
  auto dir=std::filesystem::temp_directory_path()/"cct-native-absolute-fx";
  std::filesystem::create_directories(dir);
  auto song=(dir/"values.cct").string(),inst=(dir/"values.cni").string();
  REQUIRE(projectSave(p.get(),song.c_str())==0);
  auto q=std::make_unique<Project>();projectInit(q.get());
  int loaded=projectLoad(q.get(),song.c_str());INFO(projectFileError);REQUIRE(loaded==0);
  CHECK(q->phrases[0].rows[0].fx[0][0]==fxOL1);CHECK(q->phrases[0].rows[0].fx[0][1]==42);
  CHECK(q->phrases[0].rows[0].fx[1][0]==fxOAR);CHECK(q->phrases[0].rows[0].fx[1][1]==128);
  CHECK(q->phrases[0].rows[0].fx[2][0]==fxFBK);CHECK(q->phrases[0].rows[0].fx[2][1]==6);
  REQUIRE(projectSave(p.get(),(dir/"values.zip").string().c_str())==0);
  REQUIRE(projectLoad(q.get(),(dir/"values.zip").string().c_str())==0);
  CHECK(q->phrases[0].rows[0].fx[0][0]==fxOL1);CHECK(q->phrases[0].rows[0].fx[1][0]==fxOAR);
  REQUIRE(instrumentSave(p.get(),inst.c_str(),0)==0);
  REQUIRE(instrumentLoad(q.get(),inst.c_str(),1)==0);
  CHECK(q->tables[1].rows[0].fx[0][0]==fxOL2);CHECK(q->tables[1].rows[0].fx[0][1]==63);
  CHECK(q->tables[1].rows[0].fx[1][0]==fxLFR);CHECK(q->tables[1].rows[0].fx[1][1]==8);
  projectFree(p.get());projectFree(q.get());std::filesystem::remove_all(dir);
}

TEST_CASE("DX7 absolute level changes retain the running envelope stage") {
  choochoo_msfa::Env env;choochoo_msfa::Env::init_sr(44100);
  int rates[]={99,60,50,40},levels[]={99,80,60,0};env.init(rates,levels,3500,0);
  for(int n=0;n<30;++n)env.getsample();
  char before,after;env.getPosition(&before);env.setOutputLevel(3600);env.getPosition(&after);CHECK(before==after);
  env.keydown(false);env.getPosition(&before);env.setOutputLevel(3200);env.getPosition(&after);CHECK(before==after);
}

TEST_CASE("Native songs use the same direct volume scale as upstream format 6") {
  auto p=std::make_unique<Project>();projectInit(p.get());
  REQUIRE(projectLoad(p.get(),"packaging/common/projects/gm-midi-demo.cct")==0);
  getInstrumentFunctions(InstrumentType::DX7).init(&p->instruments[0]);
  phraseClear(&p->phrases[0]);
  for(int row=0;row<4;++row){p->phrases[0].rows[row].note=45;p->phrases[0].rows[row].instrument=0;}
  p->phrases[0].rows[0].volume=15;p->phrases[0].rows[1].volume=7;
  p->phrases[0].rows[2].volume=0;p->phrases[0].rows[3].volume=75;
  auto path=std::filesystem::temp_directory_path()/"cct-native-volume-migration.cct";
  REQUIRE(projectSave(p.get(),path.string().c_str())==0);
  std::ifstream input(path);std::stringstream buffer;buffer<<input.rdbuf();input.close();auto original=buffer.str();
  REQUIRE(original.find("Module 12.0")!=std::string::npos);
  auto q=std::make_unique<Project>();projectInit(q.get());
  for(char version:{'6','7','8','9'}) {
    auto text=original;text.replace(text.find("Module 12.0"),std::strlen("Module 12.0"),std::string("Module ")+version+".0");
    std::ofstream(path)<<text;
    REQUIRE(projectLoad(q.get(),path.string().c_str())==0);
    CHECK(q->phrases[0].rows[0].volume==15);
    CHECK(q->phrases[0].rows[1].volume==7);
    CHECK(q->phrases[0].rows[2].volume==0);
    CHECK(q->phrases[0].rows[3].volume==75);
    CHECK(q->phrases[0].rows[4].volume==EMPTY_VALUE_16);
  }
  getInstrumentFunctions(InstrumentType::AY1).init(&p->instruments[0]);
  REQUIRE(projectSave(p.get(),path.string().c_str())==0);
  REQUIRE(projectLoad(q.get(),path.string().c_str())==0);
  CHECK(projectFileVersion==12);
  CHECK(q->phrases[0].rows[0].volume==15);
  CHECK(q->phrases[0].rows[3].volume==75);CHECK(q->phrases[0].rows[4].volume==EMPTY_VALUE_16);
  projectFree(p.get());projectFree(q.get());std::filesystem::remove(path);
}

namespace {
void setControl(Instrument& i,int g,int value) {
 if(g>=genericModFMOperator1&&g<=genericModFMOperator6) {
  int op=g-genericModFMOperator1;
  if(i.type==InstrumentType::DX7)i.chip.dx7.voice[(5-op)*21+16]=value;
  else if(i.type==InstrumentType::OPLL||i.type==InstrumentType::VRC7) {
   if(op==0)i.chip.opll.patch[2]=(i.chip.opll.patch[2]&192)|(63-value);
   else i.chip.opll.tone.operatorLevel[op]=value+1;
  } else if(i.type==InstrumentType::OPL2||i.type==InstrumentType::OPL3)i.chip.opl.operators[op].level=63-value;
  else i.chip.fourOp.operators[op].level=127-value;
  return;
 }
 if(auto* t=instrumentFMToneSettings(&i)){if(g==genericModFMFeedback)t->feedback=value+1;return;}
 if(i.type==InstrumentType::SID){auto* p=i.chip.sid.value;switch(g){
 case genericModSIDPulse:p[sidPulse]=(value*4095+127)/255;break;
 case genericModSIDCutoff:p[sidCutoff]=(value*2047+127)/255;break;
 case genericModSIDResonance:p[sidResonance]=value;break;
 case genericModSIDWave:p[sidWave]=std::max(1,value);break;
 case genericModSIDFilterMode:p[sidFilterMode]=value;break;
 case genericModSIDMacroRate:p[sidMacroRate]=std::max(1,value);break;
 case genericModSIDRing:p[sidRing]=value;break;
 case genericModSIDSync:p[sidSync]=value;break;
 case genericModSIDAttack:p[sidAttack]=value;break;
 case genericModSIDDecay:p[sidDecay]=value;break;
 case genericModSIDSustain:p[sidSustain]=value;break;
 case genericModSIDRelease:p[sidRelease]=value;break;
 case genericModSIDPartner:p[sidPartnerRatio]=std::max(1,value);break;}return;}
 auto& p=i.chip.simpleChip;
 switch(g){
 case genericModChipMode:p.mode=value;break;
 case genericModChipNoiseRate:p.noiseRate=value;break;
 case genericModChipNoiseDivisor:p.noiseDivisor=value;break;
 case genericModChipNoiseShift:p.noiseShift=value;break;
 case genericModChipSweepPeriod:p.sweepPeriod=value;break;
 case genericModChipSweepShift:p.sweepShift=value;break;
 case genericModChipSweepDirection:p.sweepNegate=value;break;
 case genericModChipEnvelopeInitial:p.envelopeInitial=value;break;
 case genericModChipEnvelopePeriod:p.envelopePeriod=value;break;
 case genericModChipEnvelopeDirection:p.envelopeIncrease=value;break;
 }
}
std::vector<float> render(InstrumentType type,int generic,int value,bool fx) {
 auto s=std::unique_ptr<ChipNomadState,decltype(&chipnomadDestroy)>(chipnomadCreate(),chipnomadDestroy);
 REQUIRE(projectLoad(&s->project,"packaging/common/projects/gm-midi-demo.cct")==0);
 auto& p=s->project;p.tracksCount=1;p.tickRate=50;p.linearPitch=1;calculateLinearPitchTable12TET(&p);
 for(auto& groove:p.grooves)for(auto& speed:groove.speed)speed=50;
 p.song[0][0]=0;p.chains[0].rows[0].phrase=0;p.chains[0].rows[0].transpose=0;phraseClear(&p.phrases[0]);
 auto& i=p.instruments[0];getInstrumentFunctions(type).init(&i);
 if(type==InstrumentType::SegaPSG&&generic==genericModChipNoiseRate)simpleChipApplyPreset(&i,7);
 if(auto* a=instrumentFMAmpSettings(&i)){a->enabled=1;a->sustain=255;a->release=35;}
 auto& r=p.phrases[0].rows[0];r.note=45;r.instrument=0;r.volume=PHRASE_VOLUME_MAX;
 if(generic==genericModEnvelopeAttack){if(fx){r.fx[0][0]=fxEAT;r.fx[0][1]=value;}else instrumentFMAmpSettings(&i)->attack=value;}
 else if(fx){r.fx[0][0]=instrumentNativeModDestination(type,generic)->fx;r.fx[0][1]=value;}
 else setControl(i,generic,value);
 auto before=std::make_unique<Project>(p);
 chipnomadInitChips(s.get(),48000,nullptr);chipnomadReserveRenderBuffers(s.get(),480);
 REQUIRE(chipnomadQueueProjectRefresh(s.get()));REQUIRE(chipnomadQueuePlaybackStartSong(s.get(),0,0,1));
 std::vector<float> out(48000);for(int block=0;block<50;++block)REQUIRE(chipnomadRender(s.get(),out.data()+block*960,480)==480);
 CHECK(!memcmp(before.get(),&p,sizeof(Project)));return out;
}
}
TEST_CASE("Native phrase macros reach the real voice without modifying saved parameters") {
 for(auto t:{InstrumentType::SID,InstrumentType::OPLL,InstrumentType::VRC7,InstrumentType::OPL2,InstrumentType::OPL3,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::DX7,InstrumentType::SegaPSG,InstrumentType::GBPulse,InstrumentType::GBNoise}) {
  CAPTURE(int(t));
  for(int g=genericModFMBrightness;g<genericModFirstDirectFM;++g)if(auto* d=instrumentNativeModDestination(t,g)) {
   if(g==genericModFMOperator2&&(t==InstrumentType::OPLL||t==InstrumentType::VRC7))continue; // Carrier level is a channel register, not a saved tone byte.
   CAPTURE(g);int value=d->range==1?1:d->range/2;
   if(g==genericModFMBrightness)value=30;
   if(g>=genericModFMTime&&g<=genericModFMLFODepth)value=160;
   auto fx=render(t,g,value,true),saved=render(t,g,value,false);
   REQUIRE(fx.size()==saved.size());CHECK(fx==saved);
   double energy=0;for(float x:fx){REQUIRE(std::isfinite(x));energy+=x*x;}CHECK(energy>1e-8);
  }
  Instrument i{};getInstrumentFunctions(t).init(&i);
  if(instrumentFMAmpSettings(&i))CHECK(render(t,genericModEnvelopeAttack,80,true)==render(t,genericModEnvelopeAttack,80,false));
 }
}
