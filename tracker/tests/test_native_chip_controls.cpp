#include "doctest.h"
#include "chipnomad_lib.h"
#include "simple_chip_presets.h"
#include "pitch_table_utils.h"
#include "sid_patch.h"
#include <memory>
#include <vector>
#include <cstring>
#include <cmath>
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
