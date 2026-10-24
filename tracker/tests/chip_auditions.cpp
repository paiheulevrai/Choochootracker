// Offline audit artifacts and portable demo. Never part of the runtime build.
#include "project.h"
#include "fm_catalog.h"
#include "pitch_table_utils.h"
#include "chipnomad_lib.h"
#include "synth/opll_voice.h"
#include "synth/opl_voice.h"
#include "synth/simple_chip_voice.h"
#include "synth/dx7_voice.h"
#include "synth/four_op_voice.h"
#include "synth/sid_voice.h"
#include "opll_presets.h"
#include "simple_chip_presets.h"
#include "opl_patch.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <memory>
#include <cstring>
#include <cmath>
using Entry=FMPresetEntry;
static void u32(FILE* f,unsigned n){for(int i=0;i<4;++i)fputc((n>>(8*i))&255,f);}
static void u16(FILE* f,unsigned n){fputc(n&255,f);fputc(n>>8,f);}
static void wav(const std::filesystem::path& file,const std::vector<float>& audio){FILE* f=fopen(file.string().c_str(),"wb");if(!f)exit(3);fwrite("RIFF",1,4,f);u32(f,36+audio.size()*2);fwrite("WAVEfmt ",1,8,f);u32(f,16);u16(f,1);u16(f,2);u32(f,48000);u32(f,192000);u16(f,4);u16(f,16);fwrite("data",1,4,f);u32(f,audio.size()*2);for(float x:audio)u16(f,uint16_t(int16_t(std::lround(std::clamp(x,-1.f,1.f)*32767))));fclose(f);}
int main(int argc,char** argv){
 if(argc!=4&&argc!=5)return 2;bool sidOnly=argc==5&&!strcmp(argv[4],"--sid");bool expanded=argc==5&&!strcmp(argv[4],"--expanded");std::filesystem::path folder=argv[1],out=argv[2];std::filesystem::create_directories(out);std::map<int,std::vector<Entry>> banks;
 for(const char* filename:{"catalog.tsv","builtins.tsv"}){std::vector<Entry> entries;if(!loadFMCatalog((folder/filename).string().c_str(),entries))return 3;for(const auto& e:entries)banks[e.bank].push_back(e);}
 auto p=std::make_unique<Project>(),demo=std::make_unique<Project>();projectInit(p.get());projectInit(demo.get());fillFXNames();if(projectLoad(demo.get(),"packaging/common/projects/gm-midi-demo.cct"))return 5;
 for(auto& row:demo->song)for(auto& v:row)v=EMPTY_VALUE_16;for(auto& c:demo->chains)chainClear(&c);for(auto& ph:demo->phrases)phraseClear(&ph);for(auto& i:demo->instruments)instrumentClear(&i);for(auto& t:demo->tables)tableClear(&t);
 strcpy(demo->title,"Native Chip Audition");strcpy(demo->author,"ChooChooTracker");demo->linearPitch=1;calculateLinearPitchTable12TET(demo.get());demo->grooves[0].speed[0]=6;demo->grooves[0].speed[1]=6;
 std::ofstream report(out/"auditions.tsv");report<<"bank\tpreset\tsource_cni\twav\tstart_seconds\tduration_seconds\tpeak\trms\tdc\thuman_listened\n";int section=0;
 for(const auto& bank:banks){if(sidOnly&&bank.first<600)continue;std::vector<float>bankAudio;int pick=0;for(size_t index=0;index<bank.second.size();index+=(expanded&&bank.first>=210?1:std::max(size_t(1),bank.second.size()/4))){
   if(pick++==4&&!(expanded&&bank.first>=210))break;const auto& e=bank.second[index];if(!loadFMPreset(folder.string(),e,p.get(),0))return 6;auto& i=p->instruments[0];
   if(index==0){demo->instruments[section]=i;demo->song[section][0]=section;demo->chains[section].rows[0].phrase=section;demo->chains[section].rows[0].transpose=0;auto& ph=demo->phrases[section];for(int n=0;n<4;++n){ph.rows[n*3].note=48+(n==1?4:n==2?7:n==3?12:0);ph.rows[n*3].instrument=section;ph.rows[n*3].volume=80;}ph.rows[14].note=NOTE_OFF;++section;}
   SIDVoice sid;sid.init(48000);OPLLVoice opll;OPLVoice opl;SimpleChipVoice simple;DX7Part dx7;FourOpVoice four;opll.init(48000);opl.init(48000);simple.init(48000);dx7.init(48000);four.init(48000);
   auto on=[&](float cents){if(i.type==InstrumentType::SID){sid.configure(&i.chip.sid,cents,1);sid.noteOn();}else if(isOPLL(i.type)){opll.configure(&i.chip.opll,cents,1);opll.noteOn();}else if(isOPL(i.type)){opl.configure(i.type,&i.chip.opl,cents,1);opl.noteOn();}else if(isSimpleChip(i.type)){simple.configure(i.type,&i.chip.simpleChip,cents,1);simple.noteOn();}else if(isFourOp(i.type)){four.configure(i.type,&i.chip.fourOp,cents,1);four.noteOn();}else{dx7.voices[0].configure(&i.chip.dx7,cents,1);dx7.voices[0].noteOn();}};
   auto off=[&]{sid.noteOff();opll.noteOff();opl.noteOff();simple.noteOff();four.noteOff();dx7.voices[0].noteOff();};
   std::vector<float> clip(48000*4*2),scratch(1024);double sum=0,squares=0,peak=0;const int pitches[]={6000,6400,6700,7200};
   for(int offset=0;offset<48000*4;){if(offset<48000&&offset%12000==0)on(e.category=="Percussion"?4800:pitches[offset/12000]);if(offset==48000)on(e.category=="Bass"?3600:6000);if(offset==48000*3)off();int boundary=offset<48000?(offset/12000+1)*12000:offset<144000?144000:192000;int frames=std::min(512,boundary-offset);bool stereo=isOPL(i.type)||isFourOp(i.type);
    if(i.type==InstrumentType::SID)sid.render(scratch.data(),frames);else if(isOPLL(i.type))opll.render(scratch.data(),frames);else if(isOPL(i.type))opl.render(scratch.data(),frames);else if(isSimpleChip(i.type))simple.render(scratch.data(),frames);else if(isFourOp(i.type))four.render(scratch.data(),frames);else dx7.render(scratch.data(),frames);
    for(int n=0;n<frames*2;++n){float x=scratch[stereo?n:n/2]*(stereo?1.f:.25f);if(!std::isfinite(x))return 7;clip[2*offset+n]=x;sum+=x;squares+=x*x;peak=std::max(peak,double(std::abs(x)));}offset+=frames;
   }
   if(squares<1e-9||peak>=1){fprintf(stderr,"Bad audio: %s / %s energy=%g peak=%g\n",e.bankName.c_str(),e.name.c_str(),squares,peak);return 8;}std::string name="bank-"+std::to_string(bank.first)+".wav";report<<e.bankName<<'\t'<<e.name<<'\t'<<e.path<<'\t'<<name<<'\t'<<bankAudio.size()/96000<<"\t4\t"<<peak<<'\t'<<std::sqrt(squares/clip.size())<<'\t'<<sum/clip.size()<<"\tfalse\n";bankAudio.insert(bankAudio.end(),clip.begin(),clip.end());
  }wav(out/("bank-"+std::to_string(bank.first)+".wav"),bankAudio);
 }
 if(projectSave(demo.get(),argv[3]))return 9;
 if(sidOnly){
  for(int t=0;t<8;++t){auto type=t<4?InstrumentType::SID:t==4?InstrumentType::DX7:t==5?InstrumentType::OPL3:t==6?InstrumentType::SegaPSG:InstrumentType::GBPulse;getInstrumentFunctions(type).init(&demo->instruments[t]);
   demo->song[0][t]=t;demo->song[1][t]=EMPTY_VALUE_16;demo->chains[t].rows[0].phrase=t;demo->chains[t].rows[0].transpose=0;phraseClear(&demo->phrases[t]);
   for(int r=0;r<16;r+=4){auto& row=demo->phrases[t].rows[r];row.note=36+t*3+r/4;row.instrument=t;row.volume=75;}demo->phrases[t].rows[15].note=NOTE_OFF;
   demo->trackReverbSend[t]=20;demo->trackDelaySend[t]=10;insertSelect(&demo->trackInserts[t][0],t==0?insertCompressor:insertOff);insertSelect(&demo->trackInserts[t][1],insertOff);
  }
  strcpy(demo->title,"SID + FM Evaluation");if(projectSave(demo.get(),(out/"sid-mixed.cct").string().c_str()))return 10;
  for(int t=0;t<4;++t){demo->instruments[t].chip.sid.value[sidRing]=1;demo->instruments[t].chip.sid.value[sidSync]=1;}
  strcpy(demo->title,"SID Ring Sync Budget");if(projectSave(demo.get(),(out/"sid-ring-sync.cct").string().c_str()))return 11;
 }
printf("Auditioned %zu banks; %d-section portable demo. Human listening pending.\n",banks.size(),section);projectFree(p.get());projectFree(demo.get());return 0;
}
