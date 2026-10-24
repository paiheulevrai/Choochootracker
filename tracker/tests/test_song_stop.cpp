#include "doctest.h"
#include "chipnomad_lib.h"
#include "soundbank.h"
#include "soundbank_runtime.h"
#include "pitch_table_utils.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <memory>
#include <vector>
TEST_CASE("Song stop preserves audio and declicks across engine families") {
 for(auto type:{InstrumentType::SoundFont,InstrumentType::OPLL,InstrumentType::DX7,InstrumentType::Monk,InstrumentType::Braids,InstrumentType::SegaPSG,InstrumentType::AY1}){
  auto s=std::unique_ptr<ChipNomadState,decltype(&chipnomadDestroy)>(chipnomadCreate(),chipnomadDestroy);
  CAPTURE(int(type));
  REQUIRE(projectLoad(&s->project,"packaging/common/projects/gm-midi-demo.cct")==0);
  auto&p=s->project;p.tracksCount=1;p.tickRate=50;p.linearPitch=1;calculateLinearPitchTable12TET(&p);
  for(auto&g:p.grooves)for(auto&v:g.speed)v=50;
  p.song[0][0]=0;p.chains[0].rows[0].phrase=0;p.chains[0].rows[0].transpose=0;phraseClear(&p.phrases[0]);
  for(int t=0;t<PROJECT_MAX_TRACKS;++t){p.trackReverbSend[t]=p.trackDelaySend[t]=0;}
  auto&i=p.instruments[0];getInstrumentFunctions(type).init(&i);
  soundbank::SampleCache cache;
  if(type==InstrumentType::SoundFont){auto b=soundbank::parseBank({"tests/fixtures/soundbanks/tone.sf2",""});for(auto&z:b.presets[0].zones){z.loop=soundbank::Loop::continuous;z.envelope.sustain=1;z.envelope.release=.1f;}i.chip.soundfont.prepared=soundbank::publish(cache.prepare(b,"0:0"));}
  if(auto*a=instrumentFMAmpSettings(&i)){a->enabled=1;a->sustain=255;}
  auto&r=p.phrases[0].rows[0];r.note=45;r.instrument=0;r.volume=PHRASE_VOLUME_MAX;
  chipnomadInitChips(s.get(),48000,nullptr);chipnomadReserveRenderBuffers(s.get(),512);
  chipnomadQueueProjectRefresh(s.get());chipnomadQueuePlaybackStartSong(s.get(),0,0,1);
  float out[1024]{};double peak=0;
  for(int b=0;b<7;++b){chipnomadRender(s.get(),out,512);for(float x:out)peak=std::max(peak,std::abs(double(x)));}
  REQUIRE(peak>0);
  chipnomadQueuePlaybackStop(s.get());
  const int n=chipnomadRender(s.get(),out,512);
  REQUIRE(n==400); // 256 frames to the tick boundary, then 144 at 48 kHz.
  for(int c=0;c<2;++c) {
    CHECK(out[256*2+c]==doctest::Approx(out[255*2+c]));
    for(int f=257;f<400;++f)
      CHECK(out[f*2+c]==doctest::Approx(out[255*2+c]*(399-f)/143.f));
  }
  CHECK(out[798]==0); CHECK(out[799]==0);
  for(int j=800;j<1024;++j) CHECK(out[j]==0);
  for(int b=0;b<4;++b) {
    std::fill(std::begin(out),std::end(out),99.f);
    CHECK(chipnomadRender(s.get(),out,512)==0);
    for(float x:out) CHECK(x==0);
  }
  chipnomadQueuePlaybackStartSong(s.get(),0,0,1);
  CHECK(chipnomadRender(s.get(),out,512)==512);
 }
}

TEST_CASE("Stop smoothing is independent of callback size and leaves normal audio unchanged") {
  for(int rate:{22050,44100,48000,96000}) {
    StopTransition whole{}, split{};
    float initial[]={.4f,-.2f};
    whole.process(initial,1);
    CHECK(initial[0]==.4f); CHECK(initial[1]==-.2f);
    split=whole;
    whole.begin(rate);split.begin(rate);
    const int count=rate*3/1000+32;
    std::vector<float> a(count*2,0),b(count*2,0);
    whole.process(a.data(),count);
    for(int f=0;f<count;++f)split.process(b.data()+f*2,1);
    CHECK(a==b);
    CHECK(a[0]==.4f);CHECK(a[1]==-.2f);
    CHECK(a[count*2-2]==0);CHECK(a[count*2-1]==0);
  }
}
