#include "synth/dx7_voice.h"
#include "synth/sample_voice.h"
#include "dx7_patch.h"
#include "synth/opll_voice.h"
#include "synth/opl_voice.h"
#include "synth/simple_chip_voice.h"
#include "synth/four_op_voice.h"
#include "opll_presets.h"
#include "simple_chip_presets.h"
#include "opl_patch.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include <cstring>
#include <thread>
#include <atomic>
#include <sys/resource.h>
#include "fm_catalog.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
static void stats(const char* kind,int rate,int frames,int count,int seconds,std::vector<double>& times,size_t bytes) {
 double sum=0;int misses=0;for(double t:times){sum+=t;misses+=t>frames*1e6/rate;}std::sort(times.begin(),times.end());
 printf("%s,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%zu\n",kind,rate,frames,count,seconds,sum/times.size(),times[times.size()/2],times[size_t(times.size()*.95)],times[size_t(times.size()*.99)],times.back(),misses,bytes);fflush(stdout);
}
template<class Voice,class Configure>static void family(const char* name,int count,int seconds,Configure configure,volatile double& checksum){
 auto voices=std::make_unique<Voice[]>(count);for(int i=0;i<count;++i){voices[i].init(48000);configure(voices[i],i);voices[i].noteOn();}std::vector<float>audio(1024);std::vector<double>times;times.reserve(seconds*48000/512);
 for(int b=0;b<seconds*48000/512+100;++b){auto start=std::chrono::steady_clock::now();if(b%73==0)voices[(b/73)%count].noteOff();if(b%73==7){configure(voices[(b/73)%count],(b/73)%count);voices[(b/73)%count].noteOn();}for(int i=0;i<count;++i){voices[i].render(audio.data(),512);checksum+=audio[0];}if(b>=100)times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());}
 stats(name,48000,512,count,seconds,times,sizeof(Voice));
}
int main(int argc,char** argv) {
  bool soak=argc>1&&!strcmp(argv[1],"--soak");int seconds=soak?600:argc>1?std::atoi(argv[1]):30;if(seconds<1||seconds>600)return 1;
  bool bounded=false,songsOnly=false,sampleMix=false,songFMAmp=false; int insertTracks=1,songTracks=8;
  for(int arg=2;arg<argc;++arg) {
    if(!strcmp(argv[arg],"--bounded"))bounded=true;
    else if(!strcmp(argv[arg],"--songs-only"))songsOnly=true;
    else if(!strcmp(argv[arg],"--all-inserts"))insertTracks=8;
    else if(!strcmp(argv[arg],"--four-inserts"))insertTracks=2;
    else if(!strcmp(argv[arg],"--no-inserts"))insertTracks=0;
    else if(!strcmp(argv[arg],"--four-tracks"))songTracks=4;
    else if(!strcmp(argv[arg],"--sample-mix"))sampleMix=true;
    else if(!strcmp(argv[arg],"--song-fm-amp"))songFMAmp=true;
    else return 2;
  }
  volatile double checksum=0;
  if(songFMAmp)fprintf(stderr,"Song fixtures enable FM amp ADSR: 16,32,192,48; shape128.\n");
  printf("kind,rate,frames,notes,seconds,mean_us,p50_us,p95_us,p99_us,worst_us,deadline_misses,part_bytes\n");
  if(!soak&&!songsOnly)for(int rate:{44100,48000,96000})for(int frames:{128,512})for(int count:{1,4,8,16,32}) {
    auto parts=std::make_unique<DX7Part[]>(8);InstrumentDX7 patches[8]{};
    for(int i=0;i<8;++i){parts[i].init(rate);initDX7Patch(&patches[i]);patches[i].voice[134]=i?21:4;patches[i].voice[135]=(i%4)*2;patches[i].voice[139]=i*10;patches[i].voice[143]=3;}
    for(int i=0;i<count;++i){auto& v=parts[i/4].voices[i%4];v.configure(&patches[i/4],4800+(i%7)*300,1.f/4);v.noteOn();}
    std::vector<float> audio(frames);std::vector<double> times;times.reserve(seconds*rate/frames+1);
    for(int block=0;block<seconds*rate/frames+100;++block) {
      auto start=std::chrono::steady_clock::now();
      if(block%73==0){int i=(block/73)%count;parts[i/4].voices[i%4].noteOff();}
      if(block%73==7){int i=(block/73)%count;parts[i/4].voices[i%4].configure(&patches[i/4],5000+((block/73)%12)*100,1.f/4);parts[i/4].voices[i%4].noteOn();}
      for(int part=0;part<(count+3)/4;++part){parts[part].render(audio.data(),frames);checksum+=audio[0];}
      double micros=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();if(block>=100)times.push_back(micros);
    }
    stats("DX7",rate,frames,count,seconds,times,sizeof(DX7Part));
  }

  if(!soak&&!songsOnly)for(int count:{1,8,32}) {
    if(bounded&&count>8)continue; // Optional 32-voice non-DX7 overload stress.
    for(auto type:{InstrumentType::OPLL,InstrumentType::VRC7}){Instrument i{};getInstrumentFunctions(type).init(&i);family<OPLLVoice>(getInstrumentDefinition(type)->uiName,count,seconds,[&](auto& v,int n){v.configure(&i.chip.opll,4800+(n%12)*100,.5);},checksum);}
    for(auto type:{InstrumentType::SegaPSG,InstrumentType::GBPulse,InstrumentType::GBNoise}){Instrument i{};getInstrumentFunctions(type).init(&i);family<SimpleChipVoice>(getInstrumentDefinition(type)->uiName,count,seconds,[&](auto& v,int n){v.configure(type,&i.chip.simpleChip,4800+(n%12)*100,.5);},checksum);}
    for(auto type:{InstrumentType::GenesisFM,InstrumentType::ArcadeFM}){Instrument i{};getInstrumentFunctions(type).init(&i);family<FourOpVoice>(getInstrumentDefinition(type)->uiName,count,seconds,[&](auto& v,int n){v.configure(type,&i.chip.fourOp,4800+(n%12)*100,.5);},checksum);}
    for(int topology=0;topology<3;++topology){InstrumentOPL p{};initOPLPatch(&p);p.topology=(OPLTopology)topology;p.operators[2]=p.operators[0];p.operators[3]=p.operators[1];p.connection[1]=1;p.secondDetune=4;family<OPLVoice>(topology==0?"OPL2":topology==1?"OPL3 four":"OPL3 dual",count,seconds,[&](auto& v,int n){v.configure(topology?InstrumentType::OPL3:InstrumentType::OPL2,&p,4800+(n%12)*100,.5);},checksum);}
  }
  // Actual eight-track sequencer with pre-existing and new instruments, sends
  // and two representative inserts; --four-inserts and --all-inserts retain
  // four/sixteen-slot stress. Playback/modulation overhead stays inside blocks.
  for(int scene=0;scene<3;++scene) {
    const bool heavy=scene>0,chords=scene==2;
    if((soak&&!chords)||(sampleMix&&scene==1))continue;
    auto* state=chipnomadCreate();if(projectLoad(&state->project,"packaging/common/projects/gm-midi-demo.cct"))return 3;
    const InstrumentType mixed[]={InstrumentType::AY1,InstrumentType::Plaits,InstrumentType::OPL3,InstrumentType::DX7,InstrumentType::GenesisFM,InstrumentType::ArcadeFM,InstrumentType::SegaPSG,InstrumentType::GBPulse};
    const InstrumentType balanced[]={InstrumentType::Sample,InstrumentType::Sample,InstrumentType::Sample,InstrumentType::Sample,InstrumentType::SegaPSG,InstrumentType::GBPulse,InstrumentType::DX7,InstrumentType::OPL3};
    const char* waves[]={"01-kik.wav","02-hat.wav","03-sn1.wav","06-csh.wav"};
    for(int t=0;t<8;++t){auto type=sampleMix?balanced[t]:heavy?(t%3==0?InstrumentType::OPL3:t%3==1?InstrumentType::DX7:InstrumentType::GenesisFM):mixed[t];getInstrumentFunctions(type).init(&state->project.instruments[t]);
      if(songFMAmp)if(auto* amp=instrumentFMAmpSettings(&state->project.instruments[t])) {
        amp->enabled=1; amp->attack=16; amp->decay=32; amp->sustain=192;
        amp->release=48; amp->envelopeShape=128;
      }
      if(type==InstrumentType::Sample){char path[256],error[128];snprintf(path,sizeof(path),"packaging/common/samples/ChocolateAmen/%s",waves[t]);auto& sample=state->project.instruments[t].chip.sample;if(sampleLoadWav16(path,&sample,error,sizeof(error))){fprintf(stderr,"%s: %s\n",path,error);chipnomadDestroy(state);return 4;}sample.loopMode=1;}
      if(type==InstrumentType::OPL3){auto& p=state->project.instruments[t].chip.opl;p.topology=(t&1)?OPLTopology::dualVoice:OPLTopology::fourOperator;p.operators[2]=p.operators[0];p.operators[3]=p.operators[1];p.connection[1]=1;}
      state->project.song[0][t]=t<songTracks?t:EMPTY_VALUE_8;state->project.chains[t].rows[0].phrase=t;state->project.chains[t].rows[0].transpose=0;phraseClear(&state->project.phrases[t]);for(int r=0;r<16;r+=4){auto& row=state->project.phrases[t].rows[r];row.note=36+t*3+r/4;row.instrument=t;row.volume=75;if(chords&&t==(sampleMix?6:1)){row.fx[0][0]=fxCRD;row.fx[0][1]=7;}}state->project.phrases[t].rows[15].note=NOTE_OFF;state->project.trackReverbSend[t]=30;state->project.trackDelaySend[t]=20;insertSelect(&state->project.trackInserts[t][0],t<insertTracks?insertCompressor:insertOff);insertSelect(&state->project.trackInserts[t][1],t<insertTracks?((t&1)?insertTape:insertDoubler):insertOff);
    }
    if(sampleMix&&chords&&getenv("CHOOCHOO_BENCH_PROJECT")&&projectSave(&state->project,getenv("CHOOCHOO_BENCH_PROJECT"))){chipnomadDestroy(state);return 5;}
    chipnomadInitChips(state,48000,nullptr);chipnomadReserveRenderBuffers(state,512);chipnomadQueueProjectRefresh(state);chipnomadQueuePlaybackStartSong(state,0,0,1);std::vector<float>audio(1024);std::vector<double>times;times.reserve(seconds*48000/512);
    std::atomic<bool>stop{false};std::atomic<unsigned>scans{0};std::thread browser;
    if(soak)browser=std::thread([&]{while(!stop){std::vector<FMPresetEntry>entries;if(!loadFMCatalog("../.tmp/chip-audit/scale-catalog.tsv",entries)||entries.size()!=10000)std::abort();++scans;std::this_thread::sleep_for(std::chrono::milliseconds(50));}});
    auto paced=std::chrono::steady_clock::now();for(int b=0;b<seconds*48000/512+100;++b){auto start=std::chrono::steady_clock::now();chipnomadRender(state,audio.data(),512);checksum+=audio[0];if(b>=100)times.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());if(soak)std::this_thread::sleep_until(paced+std::chrono::microseconds((b+1)*int64_t(512)*1000000/48000));}stop=true;if(browser.joinable())browser.join();if(soak){struct rusage usage{};getrusage(RUSAGE_SELF,&usage);fprintf(stderr,"Paced host soak: %d seconds plus warmup; 10k metadata scans=%u maxrss_native_units=%ld\n",seconds,unsigned(scans),usage.ru_maxrss);}char label[96];snprintf(label,sizeof(label),"%s (%d tracks)+%d inserts+sends",sampleMix?(chords?"4WAV+2chip+2FM chord song":"4WAV+2chip+2FM song"):(chords?"FM-heavy chord song":heavy?"FM-heavy song":"Mixed song"),songTracks,std::min(songTracks,insertTracks)*2);stats(label,48000,512,songTracks+(chords?3:0),seconds,times,sizeof(ChipNomadState));chipnomadDestroy(state);
  }
  fprintf(stderr,"Offline render, not device callbacks; checksum %.9f; no hardware underrun/thermal observation\n",double(checksum));
}
