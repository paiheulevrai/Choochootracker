#include "doctest.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "synth/monk_voice.h"
#include "monk_patch.h"
#include <algorithm>
#include <initializer_list>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#include <string>

static Instrument monkInstrument() {
  Instrument i{}; getInstrumentFunctions(InstrumentType::Monk).init(&i); return i;
}
static double energy(const std::vector<float>& audio) {
  double result=0;
  for (float sample : audio) { REQUIRE(std::isfinite(sample)); result += sample*sample; }
  return result;
}

TEST_CASE("MonkSynth renders all vowel landmarks at supported playback rates") {
  auto voice = std::make_unique<MonkSynthVoice>();
  auto i = monkInstrument();
  for (int rate : {22050, 44100, 48000, 96000}) for (int vowel : {0,64,128,191,255}) {
    CAPTURE(rate); CAPTURE(vowel);
    voice->init(rate); i.chip.monk.vowel=vowel;
    voice->configure(&i.chip.monk,6000,1); voice->noteOn();
    std::vector<float> audio(rate/5); voice->renderMono(audio.data(),audio.size());
    CHECK(energy(audio) > .001);
    CHECK(*std::max_element(audio.begin(),audio.end()) < 4);
    voice->noteOff(); audio.resize(rate);
    voice->renderMono(audio.data(),audio.size());
    CHECK_FALSE(voice->active());
    CHECK(std::all_of(audio.end()-128,audio.end(),[](float x){return x==0;}));
  }
}

TEST_CASE("MonkSynth tone and motion controls alter the native sound") {
  for (int parameter=0;parameter<monkParameterCount;++parameter) {
    CAPTURE(parameter);
    auto a=std::make_unique<MonkSynthVoice>(), b=std::make_unique<MonkSynthVoice>();
    auto low=monkInstrument();low.chip.monk.unison=3;
    low.chip.monk.delayMix=128;low.chip.monk.decay=4;low.chip.monk.sustain=100;
    auto high=low;
    *monkParameter(&low.chip.monk,parameter)=monkControls[parameter].minimum;
    *monkParameter(&high.chip.monk,parameter)=monkControls[parameter].maximum;
    a->init(48000); b->init(48000);
    a->configure(&low.chip.monk,4800,1); b->configure(&high.chip.monk,4800,1);
    a->noteOn();b->noteOn();
    std::vector<float> x(12000),y(x.size());a->renderMono(x.data(),x.size());b->renderMono(y.data(),y.size());
    a->configure(&low.chip.monk,6000,1); b->configure(&high.chip.monk,6000,1);
    if(parameter==monkRelease){a->noteOff();b->noteOff();}
    a->renderMono(x.data(),x.size());b->renderMono(y.data(),y.size());
    CHECK(energy(y) > .001);
    double difference=0;for(size_t n=0;n<x.size();++n)difference+=std::abs(x[n]-y[n]);
    CHECK(difference>.01);
  }
}

TEST_CASE("MonkSynth unchanged tick updates do not restart native glide") {
  auto a=std::make_unique<MonkSynthVoice>(), b=std::make_unique<MonkSynthVoice>();
  auto i=monkInstrument();i.chip.monk.glide=180;
  std::vector<float> warm(4800),x(48000),y(x.size());
  for(auto* voice : {a.get(),b.get()}) {
    voice->init(48000);voice->configure(&i.chip.monk,4800,1);voice->noteOn();voice->renderMono(warm.data(),warm.size());
    voice->configure(&i.chip.monk,6000,1);
  }
  a->renderMono(x.data(),x.size());
  for(size_t n=0;n<y.size();n+=480){b->configure(&i.chip.monk,6000,1);b->renderMono(y.data()+n,480);}
  CHECK(x==y);
  b->kill();b->renderMono(y.data(),y.size());CHECK(energy(y)==0);
  b->noteOn();b->renderMono(y.data(),y.size());CHECK(energy(y)>.001);
  b->init(48000);b->renderMono(y.data(),y.size());CHECK(energy(y)==0);
  b->init(44100);b->renderMono(y.data(),y.size());CHECK(energy(y)==0);
}

TEST_CASE("MonkSynth prepared voices render independently like fresh voices") {
  auto source=std::make_unique<MonkSynthVoice>();
  auto copy=std::make_unique<MonkSynthVoice>();
  auto i=monkInstrument();i.chip.monk.unison=4;
  source->init(48000);copy->init(48000,source.get());
  for(auto* voice:{source.get(),copy.get()}) { voice->configure(&i.chip.monk,6000,1);voice->noteOn(); }
  std::vector<float> a(4800),b(a.size());
  source->renderMono(a.data(),a.size());copy->renderMono(b.data(),b.size());
  CHECK(a==b);
  copy->init(48000,source.get());copy->renderMono(b.data(),b.size());
  CHECK(energy(b)==0);
  source->renderMono(a.data(),a.size());
  CHECK(energy(a)>.001);
}

TEST_CASE("MonkSynth prepares engines only for projects with Monk instruments") {
  auto s=std::unique_ptr<ChipNomadState,decltype(&chipnomadDestroy)>(chipnomadCreate(),chipnomadDestroy);
  REQUIRE(s);
  chipnomadInitChips(s.get(),48000,nullptr);
  CHECK_FALSE(s->monkPreview->initialized());
  for(auto& track:s->monkVoices) for(auto* voice:track) CHECK(voice==nullptr);
  s->project.instruments[0]=monkInstrument();
  chipnomadInitChips(s.get(),48000,nullptr);
  CHECK(s->monkPreview->initialized());
  for(auto& track:s->monkVoices) for(auto* voice:track) CHECK(voice->initialized());
  s->project.instruments[0].type=InstrumentType::none;
  chipnomadInitChips(s.get(),48000,nullptr);
  CHECK_FALSE(s->monkPreview->initialized());
  for(auto& track:s->monkVoices) for(auto* voice:track) CHECK(voice==nullptr);
}

TEST_CASE("MonkSynth instrument and song save every control and preserve old IDs") {
  CHECK(int(InstrumentType::SoundFont)==28);CHECK(int(InstrumentType::Monk)==29);
  CHECK(fxFSU==253);CHECK(fxMUV==254);CHECK(fxMLV==259); // New commands may append; saved IDs cannot move.
  fillFXNames();
  auto p=std::make_unique<Project>(),q=std::make_unique<Project>();projectInit(p.get());projectInit(q.get());
  REQUIRE(projectLoad(p.get(),"packaging/common/projects/gm-midi-demo.cct")==0);
  p->instruments[0]=monkInstrument();auto& m=p->instruments[0].chip.monk;
  std::strcpy(p->instruments[0].name,"Monk vowel");
  m.vowel=23;m.character=211;m.breath=99;m.glide=17;m.vibrato=166;m.rate=252;
  m.attack=31;m.decay=49;m.sustain=190;m.release=77;
  m.unison=7;m.detune=111;m.voiceSpread=222;m.delayMix=44;m.delayRate=157;m.level=199;m.factory=0;
  p->instruments[0].modulation[0].destination=3;
  p->phrases[0].rows[0].fx[0][0]=fxMWV;p->phrases[0].rows[0].fx[0][1]=255;
  for(int n=0;n<monkParameterCount;++n) {
    p->phrases[0].rows[n].fx[0]={monkControls[n].fx,uint8_t(n+1)};
    p->tables[0].rows[n].fx[0]={monkControls[n].fx,uint8_t(n+1)};
  }
  REQUIRE(instrumentSave(p.get(),"test_monk.cni",0)==0);
  REQUIRE(instrumentLoad(q.get(),"test_monk.cni",0)==0);std::remove("test_monk.cni");
  CHECK(q->instruments[0].type==InstrumentType::Monk);
  CHECK(std::memcmp(&m,&q->instruments[0].chip.monk,sizeof(m))==0);
  for(int n=0;n<monkParameterCount;++n)CHECK(q->tables[0].rows[n].fx[0].command==monkControls[n].fx);
  REQUIRE(projectSave(p.get(),"test_monk.cct")==0);
  REQUIRE(projectLoad(q.get(),"test_monk.cct")==0);std::remove("test_monk.cct");
  CHECK(std::memcmp(&m,&q->instruments[0].chip.monk,sizeof(m))==0);
  CHECK(q->phrases[0].rows[0].fx[0][0]==fxMWV);
  for(int n=0;n<monkParameterCount;++n) {
    CHECK(q->phrases[0].rows[n].fx[0].command==monkControls[n].fx);
    CHECK(q->phrases[0].rows[n].fx[0].value==n+1);
  }
  projectFree(p.get());projectFree(q.get());
}

TEST_CASE("MonkSynth effect catalog starts with vowel and voice with correct preset values") {
  auto i=monkInstrument();const auto* d=getInstrumentDefinition(i.type);
  REQUIRE(d->fxCount==16);CHECK(d->fxList[0].fx==fxMWV);CHECK(d->fxList[1].fx==fxMIN);
  for(int n=0;n<monkParameterCount;++n) {
    int matches=0; for(int k=0;k<d->fxCount;++k) matches += d->fxList[k].fx==monkControls[n].fx;
    CHECK(matches==1);
    NativeFXInfo info{};REQUIRE(instrumentNativeFXInfo(&i,monkControls[n].fx,&info));
    CHECK(info.maximum==monkControls[n].maximum);CHECK(info.minimum==monkControls[n].minimum);CHECK(info.label!=nullptr);
    CHECK(instrumentFXAvailableForInstrument(&i,monkControls[n].fx));
  }
  CHECK_FALSE(instrumentFXAvailableForInstrument(&i,fxMCF));
  CHECK_FALSE(instrumentFXAvailableForInstrument(&i,fxMRS));
  CHECK(instrumentVoicePostSettings(&i)==nullptr);
  CHECK_FALSE(instrumentFXAvailable(i.type,fxMMD));
  CHECK(instrumentSetCCDestination(&i,3,127));CHECK(i.chip.monk.vowel==255);
  CHECK(instrumentSetCCDestination(&i,4,0));CHECK(i.chip.monk.character==0);
}

static std::vector<float> renderMonkTracker(bool fx, bool chord, int parameter=monkVowel) {
  auto s=std::unique_ptr<ChipNomadState,decltype(&chipnomadDestroy)>(chipnomadCreate(),chipnomadDestroy);
  auto& p=s->project;
  REQUIRE(projectLoad(&p,"packaging/common/projects/gm-midi-demo.cct")==0);
  p.tracksCount=1;p.tickRate=50;p.linearPitch=1;calculateLinearPitchTable12TET(&p);
  for(auto& g:p.grooves)for(auto& speed:g.speed)speed=50;
  p.song[0][0]=0;p.chains[0].rows[0].phrase=0;p.chains[0].rows[0].transpose=0;
  phraseClear(&p.phrases[0]);p.instruments[0]=monkInstrument();
  auto& row=p.phrases[0].rows[0];row.note=45;row.instrument=0;row.volume=PHRASE_VOLUME_MAX;
  int value=parameter==monkUnison ? 3 : 177;
  if(fx){row.fx[0][0]=monkControls[parameter].fx;row.fx[0][1]=value;}
  else *monkParameter(&p.instruments[0].chip.monk,parameter)=value;
  if(chord){row.fx[1][0]=fxCRD;row.fx[1][1]=1;}
  auto saved=std::make_unique<Project>(p);
  chipnomadInitChips(s.get(),48000,nullptr);chipnomadReserveRenderBuffers(s.get(),480);
  REQUIRE(chipnomadQueueProjectRefresh(s.get()));REQUIRE(chipnomadQueuePlaybackStartSong(s.get(),0,0,1));
  std::vector<float> audio(9600);
  for(int b=0;b<10;++b)REQUIRE(chipnomadRender(s.get(),audio.data()+b*960,480)==480);
  CHECK(std::memcmp(saved.get(),&p,sizeof(Project))==0);
  CHECK(s->monkVoices[0][0]->active());
  if(chord)CHECK(s->playbackState.tracks[0].chordVoiceCount>1);
  chipnomadQueuePlaybackStop(s.get());
  std::vector<float> stop(960);chipnomadRender(s.get(),stop.data(),480);
  CHECK_FALSE(s->monkVoices[0][0]->active());
  return audio;
}
TEST_CASE("MonkSynth tracker FX matches saved vowel and chords render through the mixer") {
  auto saved=renderMonkTracker(false,false), fx=renderMonkTracker(true,false);
  CHECK(energy(saved)>.001);CHECK(saved==fx);
  auto chord=renderMonkTracker(false,true);CHECK(energy(chord)>.001);CHECK(chord!=saved);
}

TEST_CASE("MonkSynth factory presets load all native controls") {
  auto p=std::make_unique<Project>();projectInit(p.get());
  const char* names[]={"Dorje","Jamyang","Ngawang","Rabten","Tinley"};
  const int voices[]={0,191,64,128,255}, glides[]={102,128,204,128,255}, mixes[]={77,0,153,204,230};
  for(int n=0;n<5;++n) {
    std::string path=std::string("packaging/common/instruments/FACTORY/monk/")+names[n]+".cni";
    REQUIRE(instrumentLoad(p.get(),path.c_str(),0)==0);
    const auto& m=p->instruments[0].chip.monk;
    CHECK(std::string(p->instruments[0].name)==names[n]);CHECK(m.character==voices[n]);
    CHECK(m.glide==glides[n]);CHECK(m.delayMix==mixes[n]);CHECK(m.unison==1);
    CHECK(m.vowel==128);CHECK(m.breath==128);CHECK(m.level==255);CHECK(m.factory==1);
  }
  projectFree(p.get());
}

TEST_CASE("MonkSynth full unison and stereo delay remain finite and stop cleanly") {
  auto i=monkInstrument(); auto& m=i.chip.monk;
  m.detune=180;m.voiceSpread=200;m.delayMix=180;m.delayRate=100;m.release=4;
  auto v=std::make_unique<MonkSynthVoice>();v->init(48000);
  std::vector<float> audio(96000);
  for(int count:{1,3,10}) {
    m.unison=count;v->configure(&m,6000,0.7f);v->noteOn();v->render(audio.data(),audio.size()/2);
    CHECK(energy(audio)>.001);
    double stereo=0;for(size_t n=0;n<audio.size();n+=2)stereo+=std::abs(audio[n]-audio[n+1]);
    CHECK(stereo>.01);
    v->noteOff();
    for(int sec=0;sec<20 && v->active();++sec)v->render(audio.data(),audio.size()/2);
    CHECK_FALSE(v->active());
    v->kill();v->render(audio.data(),audio.size()/2);CHECK(energy(audio)==0);
  }
}

TEST_CASE("MonkSynth preset preview does not modify the song") {
  auto s=std::unique_ptr<ChipNomadState,decltype(&chipnomadDestroy)>(chipnomadCreate(),chipnomadDestroy);
  REQUIRE(projectLoad(&s->project,"packaging/common/projects/gm-midi-demo.cct")==0);
  auto i=monkInstrument();i.chip.monk.unison=3;s->project.instruments[0]=i;
  auto saved=std::make_unique<Project>(s->project);
  chipnomadInitChips(s.get(),48000,nullptr);chipnomadReserveRenderBuffers(s.get(),480);
  REQUIRE(chipnomadQueueProjectRefresh(s.get()));
  REQUIRE(chipnomadQueueMonkPreview(s.get(),0,&i.chip.monk));std::vector<float> audio(960);
  double total=0;for(int n=0;n<20;++n){chipnomadRender(s.get(),audio.data(),480);total+=energy(audio);}
  CHECK(total>.001);CHECK(std::memcmp(saved.get(),&s->project,sizeof(Project))==0);
  REQUIRE(chipnomadQueueMonkPreview(s.get(),0,nullptr));chipnomadRender(s.get(),audio.data(),480);
  CHECK_FALSE(s->monkPreview->active());
}

TEST_CASE("MonkSynth all sixteen Lane FX controls match saved parameters") {
  for(int parameter=0;parameter<monkParameterCount;++parameter) {
    CAPTURE(parameter);
    CHECK(renderMonkTracker(false,false,parameter)==renderMonkTracker(true,false,parameter));
  }
}
