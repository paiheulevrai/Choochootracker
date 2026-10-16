// Headless real-engine trial, shared by host tests and the native R36H build.
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "formant_filter.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#ifndef PORTMASTER_BUILD
#include "screens.h"
void getSelectionBounds(ScreenData*, int*, int*, int*, int*) {}
#endif
static void require(bool ok,const char* reason) {if(!ok){std::fprintf(stderr,"FAIL: %s\n",reason);std::exit(1);}}
static void wav(const std::string& path,const std::vector<float>& samples) {
  FILE* f=std::fopen(path.c_str(),"wb");require(f,"open WAV");
  auto u16=[&](uint16_t x){unsigned char b[]={uint8_t(x),uint8_t(x>>8)};std::fwrite(b,1,2,f);};
  auto u32=[&](uint32_t x){u16(x);u16(x>>16);};
  std::fwrite("RIFF",1,4,f);u32(36+samples.size()*2);std::fwrite("WAVEfmt ",1,8,f);
  u32(16);u16(1);u16(2);u32(48000);u32(192000);u16(4);u16(16);
  std::fwrite("data",1,4,f);u32(samples.size()*2);
  for(float x:samples)u16(int16_t(std::clamp(x,-1.f,1.f)*32767));
  require(std::fclose(f)==0,"write WAV");
}
static void setup(Project& p,int tracks,int slots,int mode,bool modulate) {
  p.chipsCount=tracks;p.tracksCount=tracks;p.tickRate=50;p.linearPitch=1;
  calculateLinearPitchTable12TET(&p);p.reverbReturn=p.delayReturn=0;
  for(int t=0;t<tracks;++t) {
    auto& i=p.instruments[t];getInstrumentFunctions(InstrumentType::AChChid).init(&i);
    i.chip.achchid.wave=AChChidWave::saw;i.chip.achchid.cutoff=16000;i.chip.achchid.envMod=0;
    i.volume=160;p.trackVolume[t]=70/tracks;p.trackReverbSend[t]=p.trackDelaySend[t]=0;
    p.song[0][t]=t;p.chains[t].rows[0].phrase=t;
    p.phrases[t].rows[0].note=36+t*3;p.phrases[t].rows[0].instrument=t;p.phrases[t].rows[0].volume=PHRASE_VOLUME_MAX;
    for(int slot=0;slot<slots;++slot) {
      auto& c=p.trackInserts[t][slot];insertSelect(&c,insertFormant);
      c.values[0]=mode;c.values[1]=64;c.values[2]=100;c.values[3]=160;c.values[4]=255;
      if(modulate) {
        auto& m=i.modulation[slot*2];m={};m.type=ModulationType::LFO;
        m.destination=getInstrumentFunctions(i.type).modDestinationsCount+1+genericModFirstInsert+slot*8+(mode?2:1);
        m.amount=64;m.p1=uint8_t(LFOShape::sin);m.p3=mode?7:4;
        auto& n=i.modulation[slot*2+1];n=m;n.destination+=mode?1:3;n.p3=5;
      }
    }
  }
}
int main(int argc,char** argv) {
  require(argc==2,"usage: formant-trial EXISTING_OUTPUT_DIRECTORY");const std::string dir=argv[1];fillFXNames();
  // Listening reference: five one-second anchors, then a five-second continuous sweep.
  std::vector<float> reference(48000*10*2);FormantFilter f;f.reset(48000);
  uint8_t v[8]={0,0,206,173,85,255};const int anchors[]={0,32,64,96,127};
  for(int offset=0;offset<480000;offset+=96) {
    v[1]=offset<240000?anchors[offset/48000]:int((offset-240000)*127.f/240000);f.set(v);
    float l[96],r[96];for(int j=0;j<96;++j)l[j]=r[j]=.25f*(2*std::fmod((offset+j)*110.f/48000,1.f)-1);
    f.process(l,r,96);for(int j=0;j<96;++j){reference[(offset+j)*2]=l[j];reference[(offset+j)*2+1]=r[j];}
  }
  wav(dir+"/formant-anchors-sweep.wav",reference);
  for(int tracks:{1,4,8})for(int slots:{1,2})for(int mode:{0,1}) {
    auto* s=chipnomadCreate();setup(s->project,tracks,slots,mode,true);
    const std::string name="formant-"+std::to_string(tracks)+"t-"+std::to_string(slots)+"s-"+(mode?"custom":"vow");
    const auto path=dir+"/"+name+".cct";require(projectSave(&s->project,path.c_str())==0,"save trial");
    // Destroy the original engine and reopen from disk, as a fresh application would.
    chipnomadDestroy(s);s=chipnomadCreate();require(projectLoad(&s->project,path.c_str())==0,"reopen trial");
    for(int t=0;t<tracks;++t)for(int slot=0;slot<slots;++slot) {
      auto& c=s->project.trackInserts[t][slot];require(c.module==insertFormant && c.values[0]==mode && c.values[4]==255,"persist insert");
      require(s->project.instruments[t].modulation[slot*2].type==ModulationType::LFO,"persist MOD");
    }
    chipnomadInitChips(s,48000,nullptr);chipnomadReserveRenderBuffers(s,512);chipnomadQueuePlaybackStartSong(s,0,0,1);
    constexpr int frames=512,blocks=188;
    float out[frames*2];for(int n=0;n<10;++n)require(chipnomadRender(s,out,frames)==frames,"warmup");
    int lo[6]={255,255,255,255,255,255},hi[6]={};double energy=0,worst=0;float peak=0;
    auto start=std::chrono::steady_clock::now();
    for(int block=0;block<blocks;++block) {
      auto before=std::chrono::steady_clock::now();require(chipnomadRender(s,out,frames)==frames,"render");
      worst=std::max(worst,std::chrono::duration<double>(std::chrono::steady_clock::now()-before).count());
      for(int p=0;p<6;++p){int x=s->insertValues[0][0][p];lo[p]=std::min(lo[p],x);hi[p]=std::max(hi[p],x);}
      for(float x:out){require(std::isfinite(x),"finite audio");peak=std::max(peak,std::abs(x));energy+=x*x;}
    }
    double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    require(energy>.0001,"non-silent audio");require(hi[mode?2:1]>lo[mode?2:1],"first live LFO");require(hi[mode?3:4]>lo[mode?3:4],"second independent LFO");
    require(mode?hi[1]==64:hi[2]==100&&hi[3]==160,"inactive MOD/base unaffected");
    std::printf("%s render_cpu=%.2f%% worst_callback=%.2fms budget=10.67ms peak=%.5f MOD=pass reopen=pass\n",name.c_str(),seconds*100/(blocks*frames/48000.0),worst*1000,peak);std::fflush(stdout);
    chipnomadDestroy(s);
  }
}
