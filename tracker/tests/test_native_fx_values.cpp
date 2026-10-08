#include "doctest.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "external/msfa/env.h"
#include <memory>
#include <vector>
#include <cstring>
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
  REQUIRE(original.find("Module 9.0")!=std::string::npos);
  auto q=std::make_unique<Project>();projectInit(q.get());
  for(char version:{'6','7','8','9'}) {
    auto text=original;text[text.find("Module ")+7]=version;
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
  CHECK(projectFileVersion==7); /* merged: this fork writes plain projects as 7.0 (SLP redefinition + 6-field Scale) */
  CHECK(q->phrases[0].rows[0].volume==15);
  CHECK(q->phrases[0].rows[3].volume==75);CHECK(q->phrases[0].rows[4].volume==EMPTY_VALUE_16);
  projectFree(p.get());projectFree(q.get());std::filesystem::remove(path);
}
