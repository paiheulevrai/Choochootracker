#include "synth/native_chip_gain.h"
#include "packaged_presets.h"
#include "doctest.h"
#include "project.h"
#include "dx7_patch.h"
#include "synth/dx7_voice.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include <vector>
#include <memory>
#include <cstring>
#include <cmath>
#include <cstdio>

TEST_CASE("DX7 bounded voices preserve roots and prefer released or quiet notes") {
  auto storage = std::make_unique<DX7Part[]>(8);
  DX7Part* parts[8];
  InstrumentDX7 patch{};
  initDX7Patch(&patch);
  for (int t = 0; t < 8; ++t) {
    parts[t] = &storage[t];
    parts[t]->init(48000);
    for (auto& voice : parts[t]->voices) {
      voice.configure(&patch, 6000, 1);
      voice.noteOn();
    }
  }
  SUBCASE("simultaneous full chords keep roots across all eight parts") {
    CHECK(limitDX7Voices(parts, 8) == 16);
    for (const auto* part : parts)
      for (int slot = 0; slot < 4; ++slot)
        CHECK(part->voices[slot].active() == (slot < 2));
    CHECK(limitDX7Voices(parts, 8) == 0);
  }
  SUBCASE("release tails count against the budget and are taken first") {
    std::vector<float> audio(512);
    for (int t = 0; t < 2; ++t) parts[t]->render(audio.data(), audio.size());
    parts[0]->voices[0].noteOff();
    parts[1]->voices[1].noteOff();
    CHECK(limitDX7Voices(parts, 2, 6) == 2);
    for (int t = 0; t < 2; ++t)
      for (int slot = 0; slot < 4; ++slot)
        CHECK(parts[t]->voices[slot].active() == (slot != t));
  }
  SUBCASE("a quiet held note is preferred over a fresh attack") {
    parts[0]->voices[0].configure(&patch, 6000, 0);
    std::vector<float> audio(512);
    for (int t = 0; t < 2; ++t) parts[t]->render(audio.data(), audio.size());
    auto& fresh = parts[1]->voices[3];
    fresh.kill();
    fresh.configure(&patch, 6000, 0);
    fresh.noteOn();
    CHECK(limitDX7Voices(parts, 2, 7) == 1);
    CHECK_FALSE(parts[0]->voices[0].active());
    CHECK(fresh.active());
    CHECK(parts[1]->voices[0].active());
  }
}
namespace {
std::vector<uint8_t> single(const InstrumentDX7& p) {
  std::vector<uint8_t> m={0xf0,0x43,0,0,1,27};m.insert(m.end(),p.voice,p.voice+155);
  unsigned sum=0;for(auto b:p.voice)sum+=b;m.push_back((-sum)&127);m.push_back(0xf7);return m;
}
std::vector<uint8_t> packed(const InstrumentDX7& p) {
  std::vector<uint8_t> b(128);
  for(int op=0;op<6;++op){const auto* v=p.voice+21*op;auto* d=b.data()+17*op;
    memcpy(d,v,11);d[11]=v[11]|v[12]<<2;d[12]=v[13]|v[20]<<3;d[13]=v[14]|v[15]<<2;d[14]=v[16];d[15]=v[17]|v[18]<<1;d[16]=v[19];}
  memcpy(b.data()+102,p.voice+126,9);b[111]=p.voice[135]|p.voice[136]<<3;memcpy(b.data()+112,p.voice+137,4);b[116]=p.voice[141]|p.voice[142]<<1|p.voice[143]<<4;memcpy(b.data()+117,p.voice+144,11);return b;
}
std::vector<uint8_t> bank(const InstrumentDX7& p) {
  std::vector<uint8_t> m={0xf0,0x43,15,9,32,0};auto b=packed(p);
  for(int i=0;i<32;++i)m.insert(m.end(),b.begin(),b.end());unsigned sum=0;for(size_t i=6;i<m.size();++i)sum+=m[i];m.push_back((-sum)&127);m.push_back(0xf7);return m;
}
double energy(const std::vector<float>& v){double e=0;for(float x:v){REQUIRE(std::isfinite(x));e+=x*x;}return e;}
}
TEST_CASE("DX7 strict single and bank SysEx agree including operator order") {
  InstrumentDX7 p{};initDX7Patch(&p);for(int op=0;op<6;++op)p.voice[op*21+19]=op*17;
  auto a=single(p),b=bank(p);std::vector<InstrumentDX7> result;std::string error;
  REQUIRE(importDX7SysEx(a.data(),a.size(),result,error));REQUIRE(result.size()==1);CHECK(memcmp(p.voice,result[0].voice,155)==0);
  REQUIRE(importDX7SysEx(b.data(),b.size(),result,error));REQUIRE(result.size()==32);for(auto& v:result)CHECK(memcmp(p.voice,v.voice,155)==0);
  a.insert(a.end(),b.begin(),b.end());REQUIRE(importDX7SysEx(a.data(),a.size(),result,error));CHECK(result.size()==33);
  // Zero rates/levels and maximum legal fixed-mode/scaling values survive.
  memset(p.voice,0,145);p.voice[20]=14;p.voice[17]=1;p.voice[18]=31;p.voice[19]=99;p.voice[144]=48;
  a=single(p);REQUIRE(importDX7SysEx(a.data(),a.size(),result,error));CHECK(memcmp(p.voice,result[0].voice,155)==0);
}
TEST_CASE("DX7 invalid SysEx leaves previously imported data intact") {
  InstrumentDX7 p{};initDX7Patch(&p);auto good=single(p);std::vector<InstrumentDX7> out={p};std::string error;
  for(size_t n=0;n<good.size();++n){CHECK_FALSE(importDX7SysEx(good.data(),n,out,error));REQUIRE(out.size()==1);CHECK(memcmp(&out[0],&p,sizeof(p))==0);}
  for(auto change:std::vector<std::pair<int,int>>{{0,0},{1,0x44},{2,16},{3,1},{4,0},{6,128},{161,0},{162,0}}){auto m=good;m[change.first]=change.second;CHECK_FALSE(importDX7SysEx(m.data(),m.size(),out,error));}
  p.voice[134]=32;auto bad=single(p);CHECK_FALSE(importDX7SysEx(bad.data(),bad.size(),out,error));
  auto huge=std::vector<uint8_t>(1024*1024+1);CHECK_FALSE(importDX7SysEx(huge.data(),huge.size(),out,error));
  auto m=good;m.insert(m.end(),bad.begin(),bad.end());CHECK_FALSE(importDX7SysEx(m.data(),m.size(),out,error));CHECK(out.size()==1);
}
TEST_CASE("DX7 all algorithms render with feedback and arbitrary blocks") {
  InstrumentDX7 patch{};initDX7Patch(&patch);
  for(int rate:{22050,44100,48000,96000})for(int algorithm=0;algorithm<32;++algorithm) {
    CAPTURE(rate);CAPTURE(algorithm);patch.voice[134]=algorithm;patch.voice[135]=algorithm%2?7:0;
    DX7Part a,b;a.init(rate);b.init(rate);a.voices[0].configure(&patch,6000,1);b.voices[0].configure(&patch,6000,1);a.voices[0].noteOn();b.voices[0].noteOn();
    std::vector<float> x(rate/5),y(x.size());a.render(x.data(),x.size());
    for(size_t i=0;i<y.size();){size_t n=std::min(size_t((i%137)+1),y.size()-i);b.render(y.data()+i,n);i+=n;}
    CHECK(energy(x)>.00001);for(size_t i=0;i<x.size();++i){REQUIRE(x[i]==y[i]);REQUIRE(std::abs(x[i])<2);}
    a.kill();a.render(x.data(),x.size());CHECK(energy(x)==0);
  }
}
TEST_CASE("DX7 awkward events keep block history and independent parts") {
  InstrumentDX7 p{};initDX7Patch(&p);p.voice[139]=70;p.voice[140]=60;p.voice[143]=7;p.voice[141]=0;
  DX7Part a,b,unrelated;a.init(48000);b.init(48000);unrelated.init(96000);
  for(auto* part:{&a,&b})for(int i=0;i<4;++i){part->voices[i].configure(&p,6000+i*300,.25f);part->voices[i].noteOn();}
  std::vector<float> x(701),y(701),scratch(200);
  for(int event=0;event<8;++event) {
    a.render(x.data(),x.size());for(int i=0;i<701;++i)b.render(y.data()+i,1);CHECK(x==y);
    unrelated.voices[0].configure(&p,3200+event*100,1);unrelated.voices[0].noteOn();unrelated.render(scratch.data(),scratch.size());
    if(event==2){a.voices[1].noteOff();b.voices[1].noteOff();}
    if(event==3){a.voices[2].configure(&p,7850,.25f);b.voices[2].configure(&p,7850,.25f);}
    if(event==4){a.voices[3].noteOn();b.voices[3].noteOn();}
    if(event==5){a.voices[0].kill();b.voices[0].kill();}
  }
  CHECK(energy(x)>0);for(auto& v:a.voices)v.noteOff();
  for(int i=0;i<2000&&a.active();++i)a.render(x.data(),x.size());CHECK_FALSE(a.active());
}
TEST_CASE("DX7 native files own full patch and wrapper without bank") {
  auto a=std::make_unique<Project>(),b=std::make_unique<Project>();fillFXNames();projectInit(a.get());projectInit(b.get());
  REQUIRE(projectLoad(a.get(),"packaging/common/projects/gm-midi-demo.cct")==0);
  getInstrumentFunctions(InstrumentType::DX7).init(&a->instruments[7]);auto& p=a->instruments[7].chip.dx7;p.fineTune=-37;p.velocity=73;p.bankId=900;p.sourceProgram=31;p.voice[20]=12;
  REQUIRE(instrumentSave(a.get(),"test_dx7.cni",7)==0);REQUIRE(instrumentLoad(b.get(),"test_dx7.cni",9)==0);CHECK(memcmp(&p,&b->instruments[9].chip.dx7,sizeof(p))==0);
  std::remove("test_dx7.cni");REQUIRE(projectSave(a.get(),"test_dx7.cct")==0);REQUIRE(projectLoad(b.get(),"test_dx7.cct")==0);CHECK(memcmp(&p,&b->instruments[7].chip.dx7,sizeof(p))==0);
  DX7Part x,y;x.init(48000);y.init(48000);x.voices[0].configure(&p,6423,1);y.voices[0].configure(&b->instruments[7].chip.dx7,6423,1);x.voices[0].noteOn();y.voices[0].noteOn();std::vector<float> v(4096),w(4096);x.render(v.data(),v.size());y.render(w.data(),w.size());CHECK(v==w);
  CHECK(sizeof(InstrumentDX7)<=512); /* Six-operator live values; overall Instrument size stays unchanged. */CHECK(sizeof(Instrument)==1208) /* merged: our InstrumentSample (autoSensitivity + 64 sliceBounds) grows the union; includes upstream sample stretch/speed fields */;CHECK(int(InstrumentType::DX7)==24);
  std::remove("test_dx7.cct");projectFree(a.get());projectFree(b.get());
}
TEST_CASE("DX7 audition owns its patch and never mutates project") {
  auto* state=chipnomadCreate();REQUIRE(state);state->project.chipsCount=1;state->project.tracksCount=3;state->project.tickRate=50;state->project.linearPitch=1;calculateLinearPitchTable12TET(&state->project);chipnomadInitChips(state,48000,nullptr);
  auto before=std::make_unique<Project>(state->project);InstrumentDX7 p{};initDX7Patch(&p);REQUIRE(chipnomadQueueDX7Preview(state,0,&p));memset(&p,0,sizeof(p));
  std::vector<float> audio(2048);double e=0;for(int i=0;i<20;++i){chipnomadRender(state,audio.data(),1024);e+=energy(audio);}CHECK(e>.001);CHECK(memcmp(before.get(),&state->project,sizeof(Project))==0);
  REQUIRE(chipnomadQueueDX7Preview(state,0,nullptr));chipnomadRender(state,audio.data(),1024);chipnomadRender(state,audio.data(),1024);CHECK(energy(audio)==0);chipnomadDestroy(state);
}

#include <filesystem>
TEST_CASE("DX7 all bundled patches are finite audible and portable") {
  auto p=std::make_unique<Project>();projectInit(p.get());fillFXNames();int count=0;
  for(const auto& file:packagedPresets()) {
    if(file.path.find("dx7-")!=0)continue;
    CAPTURE(file.path);REQUIRE((loadFMPreset("packaging/common/instruments/FACTORY",file,p.get(),0)?0:1)==0);
    REQUIRE(p->instruments[0].type==InstrumentType::DX7);REQUIRE(validDX7(p->instruments[0].chip.dx7));
    DX7Part part;part.init(48000);part.voices[0].configure(&p->instruments[0].chip.dx7,6000,1);part.voices[0].noteOn();std::vector<float> audio(48000);part.render(audio.data(),audio.size());double e=energy(audio);
    for(float x:audio)REQUIRE(std::abs(x)<2);CHECK(e>1e-6);++count;
  }
  CHECK(count==67);projectFree(p.get());
}
TEST_CASE("DX7 ratio fixed mode velocity and reference quantum semantics") {
  InstrumentDX7 p{};initDX7Patch(&p);p.voice[134]=31;p.voice[135]=0;
  for(int op=0;op<6;++op){auto* v=p.voice+op*21;v[16]=op==5?99:0;v[18]=1;v[19]=0;v[20]=7;v[15]=0;v[10]=0;v[4]=v[5]=v[6]=99;v[0]=99;}
  for(int rate:{22050,32000,44100,48000,96000})for(bool fixed:{false,true}) {
    CAPTURE(rate);CAPTURE(fixed);auto* carrier=p.voice+105;carrier[17]=fixed;carrier[18]=fixed?2:1;carrier[19]=fixed?64:0;
    DX7Part part;part.init(rate);part.voices[0].configure(&p,6900,1);part.voices[0].noteOn();std::vector<float> audio(rate);part.render(audio.data(),audio.size());
    int crossings=0;for(int i=rate/4+1;i<rate;++i)if(audio[i-1]<=0&&audio[i]>0)++crossings;
    double expected=fixed?std::pow(10,2.64):440;CHECK(std::abs(crossings/.75-expected)<2);
  }
  p.voice[122]=0;p.voice[123]=1;p.voice[124]=0;p.voice[120]=7;
  DX7Part low,high;low.init(44100);high.init(44100);p.velocity=30;low.voices[0].configure(&p,6000,1);p.velocity=120;high.voices[0].configure(&p,6000,1);low.voices[0].noteOn();high.voices[0].noteOn();std::vector<float>a(4410),b(4410);low.render(a.data(),a.size());high.render(b.data(),b.size());CHECK(energy(b)>energy(a)*2);
  // Independent reference assembly: pinned Note + part LFO + the same FIR.
  DX7Part adapter;adapter.init(48000);adapter.voices[0].configure(&p,6000,1);adapter.voices[0].noteOn();
  choochoo_msfa::Note reference;reference.start(p.voice,60,p.velocity);choochoo_msfa::Lfo lfo{};lfo.reset(p.voice+137);lfo.keydown();NativeResampler fir;fir.init(44100,48000);int32_t block[64]{};int cursor=64;
  a.resize(4800);b.resize(4800);adapter.render(a.data(),a.size());
  for(float& sample:b){float r;fir.next([&](float& x,float& y){if(cursor==64){memset(block,0,sizeof(block));reference.compute(block,lfo.getsample(),lfo.getdelay(),0);cursor=0;}x=y=block[cursor++]/16777216.f*.18f;},sample,r);sample*=nativeChipGain(InstrumentType::DX7);}
  // The native reference excludes the new 3 ms wrapper transition; after
  // its FIR history clears, the pinned core must still match sample-for-sample.
  for(size_t i=192;i<a.size();++i)CHECK(a[i]==b[i]);
}

TEST_CASE("DX7 sequencer enforces the measured budget without changing chord data") {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(
      chipnomadCreate(), chipnomadDestroy);
  REQUIRE(state);
  REQUIRE(projectLoad(&state->project, "packaging/common/projects/gm-midi-demo.cct") == 0);
  state->project.tracksCount = 8;
  for (int t = 0; t < 8; ++t) {
    getInstrumentFunctions(InstrumentType::DX7).init(&state->project.instruments[t]);
    state->project.song[0][t] = t;
    state->project.chains[t].rows[0].phrase = t;
    state->project.chains[t].rows[0].transpose = 0;
    phraseClear(&state->project.phrases[t]);
    auto& row = state->project.phrases[t].rows[0];
    row.note = 36 + t;
    row.instrument = t;
    row.volume = PHRASE_VOLUME_MAX;
    row.fx[0][0] = fxCRD;
    row.fx[0][1] = 7;
  }
  auto original = std::make_unique<Project>(state->project);
  chipnomadInitChips(state.get(), 48000, nullptr);
  chipnomadReserveRenderBuffers(state.get(), 512);
  REQUIRE(chipnomadQueueProjectRefresh(state.get()));
  REQUIRE(chipnomadQueuePlaybackStartSong(state.get(), 0, 0, 1));
  float audio[1024]{};
  REQUIRE(chipnomadRender(state.get(), audio, 512));
  unsigned active = 0;
  for (int t = 0; t < 8; ++t) {
    CHECK(state->playbackState.tracks[t].chordVoiceCount == 4);
    for (auto* voice : state->dx7Voices[t]) active += voice->active();
    CHECK(state->dx7Voices[t][0]->active());
  }
  CHECK(active == DX7_MAX_ACTIVE_VOICES);
  CHECK(memcmp(original.get(), &state->project, sizeof(Project)) == 0);
}
