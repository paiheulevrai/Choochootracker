#include <cmath>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <functional>

#include "chipnomad_lib.h"
#include "doctest.h"
#include "project_utils.h"
#include "pitch_table_utils.h"
#include "screen_instrument.h"
#include "soundbank.h"
#include "soundbank_runtime.h"
#include "synth/soundbank_voice.h"
#include "mocks/app_ui_mock.h"
#include "corelib_gfx.h"

TEST_CASE("SoundFont project and instrument files retain identities without runtime handles") {
  auto p = std::make_unique<Project>(), q = std::make_unique<Project>();
  projectInitAY(p.get());
  projectInit(q.get());
  fillFXNames();
  auto& i = p->instruments[0];
  getInstrumentFunctions(InstrumentType::SoundFont).init(&i);
  strcpy(i.chip.soundfont.bank, "Korean_Gugak/Wind/Flute.exs");
  strcpy(i.chip.soundfont.preset, "exs");
  i.chip.soundfont.prepared = 12345;
  const SoundbankControls controls{-125, 2400, 55, 20, 150, 30, 250, 65, 900, 1};
  i.chip.soundfont.controls = controls;
  auto path =
      (std::filesystem::temp_directory_path() / "choochoo-soundbank-roundtrip.cct").string();
  REQUIRE(projectSave(p.get(), path.c_str()) == 0);
  REQUIRE(projectLoad(q.get(), path.c_str()) == 0);
  CHECK(q->instruments[0].type == InstrumentType::SoundFont);
  CHECK(std::string(q->instruments[0].chip.soundfont.bank) == i.chip.soundfont.bank);
  CHECK(std::string(q->instruments[0].chip.soundfont.preset) == "exs");
  CHECK(q->instruments[0].chip.soundfont.prepared == 0);
  CHECK(memcmp(&q->instruments[0].chip.soundfont.controls, &controls, sizeof(controls)) == 0);
  REQUIRE(instrumentSave(p.get(), path.c_str(), 0) == 0);
  REQUIRE(instrumentLoad(q.get(), path.c_str(), 3) == 0);
  CHECK(std::string(q->instruments[3].chip.soundfont.bank) == i.chip.soundfont.bank);
  CHECK(q->instruments[3].chip.soundfont.prepared == 0);
  CHECK(memcmp(&q->instruments[3].chip.soundfont.controls, &controls, sizeof(controls)) == 0);
  CHECK(int(InstrumentType::SoundFont) == 28);
  std::ifstream saved(path);
  std::string text((std::istreambuf_iterator<char>(saved)), {});
  saved.close();
  auto legacy = text;
  auto start = legacy.find("- Sound controls: ");
  REQUIRE(start != std::string::npos);
  legacy.erase(start, legacy.find('\n', start) - start + 1);
  { std::ofstream rewritten(path); rewritten << legacy; }
  REQUIRE(instrumentLoad(q.get(), path.c_str(), 4) == 0);
  auto defaults = soundbankDefaultControls();
  CHECK(memcmp(&q->instruments[4].chip.soundfont.controls, &defaults, sizeof(defaults)) == 0);
  auto invalid = text;
  start = invalid.find("- Sound controls: ");
  invalid.replace(start, invalid.find('\n', start) - start, "- Sound controls: 0 99999 -1 -1 -1 -1 -1 -1 -1");
  { std::ofstream rewritten(path); rewritten << invalid; }
  CHECK(instrumentLoad(q.get(), path.c_str(), 4) != 0);
  auto typeLine = text.find("- Type: 28");
  REQUIRE(typeLine != std::string::npos);
  text.replace(typeLine, 10, "- Type: 255");
  { std::ofstream rewritten(path); rewritten << text; }
  CHECK(instrumentLoad(q.get(), path.c_str(), 4) != 0);
  std::filesystem::remove(path);
  projectFree(p.get());
  projectFree(q.get());
}
TEST_CASE("SoundFont tracker preview renders then stops and pins queued assets") {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(chipnomadCreate(),
                                                                            chipnomadDestroy);
  projectInitAY(&state->project);
  auto& i = state->project.instruments[0];
  getInstrumentFunctions(InstrumentType::SoundFont).init(&i);
  soundbank::SampleCache cache;
  auto b = soundbank::parseBank({"tests/fixtures/soundbanks/tone.sf2", ""});
  auto p = cache.prepare(b, "0:0");
  auto h = soundbank::publish(p);
  i.chip.soundfont.prepared = h;
  chipnomadInitChips(state.get(), 48000, nullptr);
  chipnomadReserveRenderBuffers(state.get(), 480);
  REQUIRE(chipnomadQueueProjectRefresh(state.get()));
  chipnomadCollectSoundbanks(state.get());
  CHECK(soundbank::prepared(h) != nullptr);
  REQUIRE(chipnomadQueuePlaybackPreviewNote(state.get(), 0, 48, 0));
  float out[960];
  double energy = 0;
  for (int n = 0; n < 10; ++n) {
    REQUIRE(chipnomadRender(state.get(), out, 480));
    for (float x : out) {
      REQUIRE(std::isfinite(x));
      energy += x * x;
    }
  }
  CHECK(energy > 0.01);
  CHECK(state->soundbankVoices[0][0]->active());
  i.chip.soundfont.controls.cutoffHz = 100;
  REQUIRE(chipnomadQueueProjectRefresh(state.get()));
  REQUIRE(chipnomadQueuePlaybackPreviewNote(state.get(), 0, 48, 0));
  double filteredEnergy = 0;
  for (int n = 0; n < 10; ++n) {
    REQUIRE(chipnomadRender(state.get(), out, 480));
    for (float x : out) { REQUIRE(std::isfinite(x)); filteredEnergy += x*x; }
  }
  CHECK(filteredEnergy < energy * .5);

  REQUIRE(chipnomadQueuePlaybackStopPreview(state.get(), 0));
  for (int n = 0; n < 20; ++n) chipnomadRender(state.get(), out, 480);
  CHECK_FALSE(state->soundbankVoices[0][0]->active());
  state.reset();
  soundbank::collect([](uint32_t) { return false; });
}
TEST_CASE("SoundFont uses the compact Instrument page and sample category") {
  CHECK(getInstrumentDefinition(InstrumentType::SoundFont)->category == InstrumentCategory::sample);
  CHECK(screenInstrumentSoundFont.rows == 5);
  CHECK(screenInstrumentSoundFont.getColumnCount(3) == 1);
  CHECK(screenInstrumentSoundFont.getColumnCount(4) == 2);
}

TEST_CASE("SoundFont editor restores Bank defaults without changing the preset identity") {
  auto* savedState = chipnomadState;
  auto* savedScreen = currentScreen;
  const int savedInstrument = cInstrument, savedModified = projectModified;
  chipnomadState = chipnomadCreate(); cInstrument = 0;
  auto& i = chipnomadState->project.instruments[0];
  getInstrumentFunctions(InstrumentType::SoundFont).init(&i);
  strcpy(i.chip.soundfont.bank, "tone.sf2"); strcpy(i.chip.soundfont.preset, "0:0");
  i.chip.soundfont.prepared = 123;
  screenInstrumentSoundFont.onEdit(1,4,CellEditAction::tap);
  CHECK(currentScreen == &screenSoundFontEdit);
  screenSoundFontEdit.fullRedraw(); auto* table = mockScreenData;
  REQUIRE(table != nullptr);
  table->onEdit(0,1,CellEditAction::increase);
  CHECK(i.chip.soundfont.controls.cutoffHz == 20);
  table->onEdit(0,1,CellEditAction::decrease);
  CHECK(i.chip.soundfont.controls.cutoffHz == -1);
  table->onEdit(0,0,CellEditAction::decreaseBig);
  CHECK(i.chip.soundfont.controls.tuneCents == -100);
  table->onEdit(0,0,CellEditAction::clear);
  CHECK(i.chip.soundfont.controls.tuneCents == 0);
  i.chip.soundfont.controls.attackMs = 1800;
  table->onEdit(0,4,CellEditAction::clear);
  CHECK(i.chip.soundfont.controls.attackMs == -1);
  CHECK(std::string(i.chip.soundfont.bank) == "tone.sf2");
  CHECK(i.chip.soundfont.prepared == 123);
  chipnomadDestroy(chipnomadState);chipnomadState=savedState;currentScreen=savedScreen;
  cInstrument=savedInstrument;projectModified=savedModified;
}

TEST_CASE("SoundFont overrides shape audio without mutating bank zones or retargeting keys") {
  using namespace soundbank;
  auto bank = parseBank({"tests/fixtures/soundbanks/tone.sf2", ""});
  auto& zone = bank.presets[0].zones[0];
  zone.loop = Loop::continuous; zone.loKey = zone.hiKey = 60;
  zone.envelope = {}; zone.envelope.sustain=1;zone.envelope.release=.01;
  zone.cutoff=20000;zone.resonance=0;
  SampleCache cache;auto preset=cache.prepare(bank,"0:0");auto handle=publish(preset);
  SoundbankVoice voice;voice.init(48000);
  auto render = [&](const SoundbankControls& controls) {
    // Compare independent settings from silence. KILL now preserves a short
    // outgoing de-click tail, which belongs in the transition tests instead.
    voice.init(48000);voice.configure(handle,6000,1,127,20000,controls);voice.noteOn();
    std::vector<float> out(9600);voice.render(out.data(),out.size()/2);
    for (float sample:out) REQUIRE(std::isfinite(sample));
    return out;
  };
  auto defaults=soundbankDefaultControls();auto original=render(defaults);
  auto changed=defaults;changed.cutoffHz=100;
  auto filtered=render(changed);
  auto energy=[](const auto& out) { double e=0;for(float x:out)e+=x*x;return e; };
  CHECK(energy(filtered)<energy(original)*.5);
  changed=defaults;changed.tuneCents=1200;auto tuned=render(changed);
  CHECK(energy(tuned)>0);CHECK(tuned!=original); // Key 60 still selects the same zone.
  changed=defaults;changed.delayMs=200;CHECK(energy(render(changed))==0);
  changed=defaults;changed.attackMs=1000;changed.releaseMs=200;
  auto slow=render(changed);CHECK(energy(slow)<energy(original)*.1);
  CHECK(voice.envelopeLevel()==doctest::Approx(.1).epsilon(.01));
  // The explicit release lasts 200 ms after note-off.
  voice.configure(handle,6000,1,127,20000,changed);
  voice.noteOff();std::vector<float> tail(4800);voice.render(tail.data(),2400);
  CHECK(voice.active());voice.render(tail.data(),2400);voice.render(tail.data(),2400);
  voice.render(tail.data(),2400);voice.render(tail.data(),1);CHECK_FALSE(voice.active());
  CHECK(preset->zones[0].zone.envelope.attack==0);
  CHECK(preset->zones[0].zone.cutoff==20000);
  changed=defaults;changed.cutoffHz=20;changed.resonanceTenths=400;
  render(changed); // Extreme filter settings must remain finite.
  changed=defaults;changed.attackMs=0;changed.decayMs=0;changed.holdMs=0;changed.sustainPercent=0;
  CHECK(energy(render(changed))==0);
  CHECK(render(defaults)==original);
  voice.kill();collect([](uint32_t){return false;});
}

#include "soundbank_browser.h"
#include "soundbank_fx.h"
#include "playback_internal.h"

TEST_CASE("SoundFont collections flatten folders and cycle in visible order across boundaries") {
  using namespace soundbank;
  std::vector<BankEntry> banks(5);
  banks[0].id="Gugak/Wind/Flute.exs"; banks[0].format="exs"; banks[0].presets={{"exs","Flute"}};
  banks[1].id="Gugak/Wind/Samples/note.wav"; banks[1].format="wav"; banks[1].presets={{"wav","note"}};
  banks[2].id="Fluid/Piano.zip::Banks/Piano.sf2";banks[2].format="sf2";banks[2].presets={{"0:0","Piano"},{"0:1","Bright"}};
  banks[3].id="broken.sf2";banks[3].format="sf2";banks[3].error="Invalid bank";
  banks[4].id="Solo.wav";banks[4].format="wav";banks[4].presets={{"wav","Solo"}};
  auto groups=collections(banks);REQUIRE(groups.size()==3);
  CHECK(groups[0].name=="Fluid");REQUIRE(groups[0].presets.size()==2);
  CHECK(groups[0].presets[0].label=="Bright");CHECK(groups[0].presets[1].label=="Piano");
  CHECK(groups[1].name=="Gugak");REQUIRE(groups[1].presets.size()==1);
  CHECK(groups[1].presets[0].label=="Flute");
  CHECK(groups[2].name=="Loose samples");
  CHECK(cyclePreset(banks,banks[2].id,"0:1",1)==std::make_pair(2,0));
  CHECK(cyclePreset(banks,banks[2].id,"0:0",1)==std::make_pair(0,0));
  CHECK(cyclePreset(banks,"Gugak/Wind/Flute.exs","exs",1)==std::make_pair(4,0));
  CHECK(cyclePreset(banks,"Solo.wav","wav",1)==std::make_pair(2,1));
  CHECK(cyclePreset(banks,banks[2].id,"0:1",-1)==std::make_pair(4,0));
  CHECK(cyclePreset(banks,banks[0].id,"exs",-1)==std::make_pair(2,0));
  CHECK(cyclePreset(banks,"missing","",1)==std::make_pair(2,1));
  CHECK(cyclePreset({},"","",1)==std::make_pair(-1,-1));
}

TEST_CASE("SoundFont eight contextual effects preserve IDs default values and track isolation") {
  auto p=std::make_unique<Project>();projectInitAY(p.get());fillFXNames();
  auto& i=p->instruments[0];getInstrumentFunctions(InstrumentType::SoundFont).init(&i);
  auto before=i;
  const auto* d=getInstrumentDefinition(i.type);REQUIRE(d->fxCount==8);
  auto playback=std::make_unique<PlaybackState>();playback->p=p.get();
  playback->tracks[0].note.instrument=0;playback->tracks[1].note.instrument=1;
  CHECK(fxTPN==243);CHECK(fxFPT==fxTPN+1);CHECK(fxTotalCount<FX_ID_EMPTY);
  for(int n=0;n<8;++n) {
    int fx=d->fxList[n].fx;
    NativeFXInfo info{};REQUIRE(instrumentNativeFXInfo(&i,fx,&info));
    CHECK(instrumentFXAvailable(i.type,fx));CHECK_FALSE(instrumentFXAvailable(InstrumentType::SID,fx));
    CHECK(info.maximum==(fx==fxFPT ? 48 : 255));
    CHECK(std::string(fxNames[fx].name)==d->fxList[n].name);
    FXCell cell{FXId(fx),127};initFX(playback.get(),0,&cell,nullptr,0);initFX(playback.get(),1,&cell,nullptr,0);
    CHECK(playback->tracks[0].note.fx[fx].isOn);CHECK_FALSE(playback->tracks[1].note.fx[fx].isOn);
    p->phrases[0].rows[n].fx[0]=cell;
  }
  CHECK(memcmp(&before,&i,sizeof(i))==0);
  auto path=(std::filesystem::temp_directory_path()/"choochoo-soundfont-fx.cct").string();
  REQUIRE(projectSave(p.get(),path.c_str())==0);
  auto q=std::make_unique<Project>();projectInit(q.get());REQUIRE(projectLoad(q.get(),path.c_str())==0);
  for(int n=0;n<8;++n) {
    CHECK(q->phrases[0].rows[n].fx[0].command==d->fxList[n].fx);
    CHECK(q->phrases[0].rows[n].fx[0].value==127);
  }
  std::filesystem::remove(path);projectFree(q.get());projectFree(p.get());
  CHECK(soundbankFXCutoff(0)==20);CHECK(soundbankFXCutoff(254)==20000);
  CHECK(soundbankFXTime(0)==0);CHECK(soundbankFXTime(254)==20000);
  for(int v=1;v<=254;++v) {
    CHECK(soundbankFXCutoff(v)>=soundbankFXCutoff(v-1));
    CHECK(soundbankFXTime(v)>=soundbankFXTime(v-1));
  }
  PlaybackFXState fx[fxTotalCount]{};auto base=soundbankDefaultControls();base.cutoffHz=3210;base.attackMs=250;
  for(int id=fxFCF;id<=fxFRL;++id){fx[id].isOn=1;fx[id].fxValue=255;}
  auto restored=soundbankFXControls(base,fx);CHECK(memcmp(&base,&restored,sizeof(base))==0);
}

static std::vector<float> renderSoundFontFX(int command,int value,bool useFX) {
  auto state=std::unique_ptr<ChipNomadState,decltype(&chipnomadDestroy)>(chipnomadCreate(),chipnomadDestroy);
  auto& p=state->project;REQUIRE(projectLoad(&p,"packaging/common/projects/gm-midi-demo.cct")==0);
  p.tracksCount=1;p.tickRate=50;p.linearPitch=1;calculateLinearPitchTable12TET(&p);
  for(auto& g:p.grooves)for(auto& speed:g.speed)speed=5;
  p.song[0][0]=0;p.chains[0].rows[0].phrase=0;p.chains[0].rows[0].transpose=0;
  phraseClear(&p.phrases[0]);auto& i=p.instruments[0];getInstrumentFunctions(InstrumentType::SoundFont).init(&i);
  soundbank::SampleCache cache;auto bank=soundbank::parseBank({"tests/fixtures/soundbanks/tone.sf2",""});
  auto& zone=bank.presets[0].zones[0];zone.loop=soundbank::Loop::continuous;
  zone.envelope.sustain=.2;zone.envelope.attack=.01;zone.envelope.decay=.1;zone.envelope.release=.1;
  i.chip.soundfont.prepared=soundbank::publish(cache.prepare(bank,"0:0"));
  if(command==fxFRS) i.chip.soundfont.controls.cutoffHz=1000;
  auto& row=p.phrases[0].rows[0];row.note=48;row.instrument=0;row.volume=PHRASE_VOLUME_MAX;
  p.phrases[0].rows[1].note=NOTE_OFF;
  if(useFX) row.fx[0]={FXId(command),uint8_t(value)};
  else {
    switch(command) {
      case fxFPT:i.chip.soundfont.controls.tuneCents=(value-24)*100;break;
      case fxFVL:i.volume=value;break;
      case fxFPN:i.pan=value;break;
      case fxFCF:i.chip.soundfont.controls.cutoffHz=soundbankFXCutoff(value);break;
      case fxFRS:i.chip.soundfont.controls.resonanceTenths=(value*400+127)/254;break;
      case fxFAT:i.chip.soundfont.controls.attackMs=soundbankFXTime(value);break;
      case fxFDC:i.chip.soundfont.controls.decayMs=soundbankFXTime(value);break;
      case fxFRL:i.chip.soundfont.controls.releaseMs=soundbankFXTime(value);break;
      case fxFHD:i.chip.soundfont.controls.holdMs=soundbankFXTime(value);break;
      case fxFSU:i.chip.soundfont.controls.sustainPercent=(value*100+127)/254;break;
    }
  }
  auto saved=std::make_unique<Project>(p);
  chipnomadInitChips(state.get(),48000,nullptr);chipnomadReserveRenderBuffers(state.get(),480);
  REQUIRE(chipnomadQueueProjectRefresh(state.get()));REQUIRE(chipnomadQueuePlaybackStartSong(state.get(),0,0,1));
  std::vector<float> audio(28800);
  for(int b=0;b<30;++b)REQUIRE(chipnomadRender(state.get(),audio.data()+b*960,480)==480);
  CHECK(std::memcmp(saved.get(),&p,sizeof(Project))==0);
  for(float sample:audio)REQUIRE(std::isfinite(sample));
  return audio;
}
TEST_CASE("SoundFont current and legacy Lane FX reach playback and match instrument edits") {
  for(int fx=fxFPT;fx<=fxFSU;++fx) {
    CAPTURE(fx);
    int value=fx==fxFPT ? 36 : fx==fxFCF ? 80 : fx==fxFHD ? 4 : 32;
    auto direct=renderSoundFontFX(fx,value,false), lane=renderSoundFontFX(fx,value,true);
    CHECK(direct==lane);
    double energy=0;for(float v:lane)energy+=v*v;CHECK(energy>0);
  }
  auto muted=renderSoundFontFX(fxFVL,0,true);
  double energy=0;for(float v:muted)energy+=v*v;CHECK(energy==0);
  soundbank::collect([](uint32_t){return false;});
}

TEST_CASE("SoundFont tracker drains removed chord slots and KILL tails") {
  auto state = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>(
      chipnomadCreate(), chipnomadDestroy);
  auto& p = state->project;
  REQUIRE(projectLoad(&p, "packaging/common/projects/gm-midi-demo.cct") == 0);
  p.tracksCount = 1; p.tickRate = 50; p.linearPitch = 1;
  calculateLinearPitchTable12TET(&p);
  for (auto& groove : p.grooves) for (auto& speed : groove.speed) speed = 5;
  p.song[0][0] = 0; p.chains[0].rows[0].phrase = 0; p.chains[0].rows[0].transpose = 0;
  phraseClear(&p.phrases[0]);
  auto& instrument = p.instruments[0];
  getInstrumentFunctions(InstrumentType::SoundFont).init(&instrument);
  soundbank::SampleCache cache;
  auto bank = soundbank::parseBank({"tests/fixtures/soundbanks/tone.wav", ""});
  instrument.chip.soundfont.prepared = soundbank::publish(cache.prepare(bank, "sample"));
  auto& first = p.phrases[0].rows[0];
  first.note = 48; first.instrument = 0; first.volume = PHRASE_VOLUME_MAX;
  first.fx[0] = {fxCRD, 0};
  auto& second = p.phrases[0].rows[1];
  second.note = 36; second.instrument = 0; second.volume = PHRASE_VOLUME_MAX;
  p.phrases[0].rows[2].fx[0] = {fxKIL, 0};
  chipnomadInitChips(state.get(), 48000, nullptr);
  chipnomadReserveRenderBuffers(state.get(), 480);
  REQUIRE(chipnomadQueueProjectRefresh(state.get()));
  REQUIRE(chipnomadQueuePlaybackStartSong(state.get(), 0, 0, 1));
  float audio[960];
  for (int block = 0; block < 5; ++block) REQUIRE(chipnomadRender(state.get(), audio, 480) == 480);
  CHECK(state->soundbankVoices[0][1]->active());
  CHECK(state->soundbankVoices[0][2]->active());
  for (int block = 0; block < 10; ++block) REQUIRE(chipnomadRender(state.get(), audio, 480) == 480);
  CHECK(state->soundbankVoices[0][0]->active());
  CHECK_FALSE(state->soundbankVoices[0][1]->active());
  CHECK_FALSE(state->soundbankVoices[0][2]->active());
  for (int block = 0; block < 10; ++block) REQUIRE(chipnomadRender(state.get(), audio, 480) == 480);
  CHECK_FALSE(state->soundbankVoices[0][0]->active());
  state.reset();
  soundbank::collect([](uint32_t) { return false; });
}

TEST_CASE("SoundFont bend resolution is per instrument and legacy files keep stock scale") {
  auto p=std::make_unique<Project>();projectInitAY(p.get());initFXHandlers();
  auto& i=p->instruments[0];getInstrumentFunctions(InstrumentType::SoundFont).init(&i);
  CHECK(i.chip.soundfont.controls.bendStepCents==1);
  auto playback=std::make_unique<PlaybackState>();playback->p=p.get();
  auto& t=playback->tracks[0];t.note.instrument=0;t.grooveIdx=t.grooveRow=0;
  p->grooves[0].speed[0]=4;p->linearPitch=1;
  auto bend=[&](int value){t.note.fx[fxPBN]={};FXCell f{fxPBN,uint8_t(value)};initFX(playback.get(),0,&f,nullptr,-1);for(int k=0;k<4;++k){t.note.fineOffset=0;handleFX(playback.get(),0,0);}return t.note.fineOffset;};
  CHECK(bend(1)==1);CHECK(bend(255)==-1);
  i.chip.soundfont.controls.bendStepCents=5;CHECK(bend(1)==5);
  i.chip.soundfont.controls.bendStepCents=0;CHECK(bend(1)==25);
  p->linearPitch=0;CHECK(bend(1)==1);
  p->linearPitch=1;getInstrumentFunctions(InstrumentType::AY1).init(&p->instruments[1]);t.note.instrument=1;CHECK(bend(1)==25);
  auto path=(std::filesystem::temp_directory_path()/"soundfont-legacy-bend.cni").string();
  REQUIRE(instrumentSave(p.get(),path.c_str(),0)==0);
  std::ifstream in(path);std::string text((std::istreambuf_iterator<char>(in)),{});in.close();
  auto pos=text.find("- Sound bend step: ");REQUIRE(pos!=std::string::npos);text.erase(pos,text.find('\n',pos)-pos+1);
  {std::ofstream out(path);out<<text;}
  REQUIRE(instrumentLoad(p.get(),path.c_str(),2)==0);CHECK(p->instruments[2].chip.soundfont.controls.bendStepCents==0);
  std::filesystem::remove(path);projectFree(p.get());
}

TEST_CASE("SoundFont live decay sustain and release edits reach an existing voice") {
  using namespace soundbank;
  auto b=parseBank({"tests/fixtures/soundbanks/tone.sf2",""});auto& z=b.presets[0].zones[0];
  z.loop=Loop::continuous;z.envelope={0,0,2,.2f,2,0};
  SampleCache cache;auto h=publish(cache.prepare(b,"0:0"));SoundbankVoice voice;voice.init(48000);
  auto controls=soundbankDefaultControls();voice.configure(h,6000,1,127,20000,controls);voice.noteOn();
  float audio[960];for(int n=0;n<10;++n)voice.render(audio,480);
  CHECK(voice.envelopeLevel()>.9f);
  controls.decayMs=10;controls.sustainPercent=25;
  voice.configure(h,6000,1,127,20000,controls);for(int n=0;n<3;++n)voice.render(audio,480);
  CHECK(voice.envelopeLevel()==doctest::Approx(.25));
  controls.releaseMs=10;voice.configure(h,6000,1,127,20000,controls);voice.noteOff();
  for(int n=0;n<3;++n)voice.render(audio,480);
  CHECK_FALSE(voice.active());collect([](uint32_t){return false;});
}

TEST_CASE("SoundFont folder display removes archive hashes without changing identifiers") {
  CHECK(soundbank::browserLabel("stargate-sample-pack-dbfd6ec52d4ed53b60bdbea5fc6adf295127c027.zip")=="stargate-sample-pack");
  CHECK(soundbank::browserLabel("Korean_Gugak")=="Korean Gugak");
  CHECK(soundbank::browserLabel("Flute.exs")=="Flute");
  CHECK(soundbank::browserLabel("219157__jagadamba__bongo01.WAV")=="bongo01");
  CHECK(soundbank::browserLabel("Chord_Eb_Natural_Minor_77BPM.wav")=="Chord Eb Natural Minor 77BPM");
  CHECK(soundbank::browserLabel("v1.3.1")=="v1.3.1");
  CHECK_FALSE(instrumentFXAvailable(InstrumentType::SoundFont,fxFVL));
  CHECK_FALSE(instrumentFXAvailable(InstrumentType::SoundFont,fxFPN));
  fillFXNames();CHECK(std::string(fxNames[fxFVL].name)=="FVL");CHECK(std::string(fxNames[fxFPN].name)=="FPN");
}

#include "soundbank_library.h"
#include <chrono>
#include <thread>
TEST_CASE("SoundFont preset completion stays local to its instrument page") {
  auto* saved=chipnomadState;std::string savedMessage=screenGetActiveMessage();chipnomadState=chipnomadCreate();
  soundbankShutdown();auto& i=chipnomadState->project.instruments[0];
  getInstrumentFunctions(InstrumentType::SoundFont).init(&i);
  // Use the real library's working root so this also runs on Windows.
  const auto root=std::filesystem::path(soundbankRoot());std::filesystem::create_directories(root);
  const auto file=root/"__test_local_status.sf2";
  REQUIRE_FALSE(std::filesystem::exists(file));
  std::filesystem::copy_file("tests/fixtures/soundbanks/tone.sf2",file);
  screenMessage(1,"Existing unrelated message");
  soundbankLoad(0,file.filename().string(),"0:0",true);
  auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
  while(soundbankBusy() && std::chrono::steady_clock::now()<until){soundbankPoll();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  REQUIRE_FALSE(soundbankBusy());CHECK(i.chip.soundfont.prepared!=0);
  CHECK(soundbankInstrumentStatus(0)=="Preset ready");CHECK(soundbankInstrumentWarning(0).empty());
  CHECK(std::string(screenGetActiveMessage())=="Existing unrelated message");
  soundbankShutdown();chipnomadDestroy(chipnomadState);chipnomadState=saved;std::filesystem::remove(file);screenMessage(1,"%s",savedMessage.c_str());
}

TEST_CASE("SoundFont failed selections stay silent and preserve cycling and error layout") {
  struct Fixture {
    ChipNomadState* saved = chipnomadState;
    int savedInstrument = cInstrument, modified = projectModified;
    std::filesystem::path base, folder;
    bool hadRoot = std::getenv("CHOOCHOO_SOUNDBANKS") != nullptr;
    std::string previousRoot = hadRoot ? std::getenv("CHOOCHOO_SOUNDBANKS") : "";
    static void root(const char* value) {
#ifdef _WIN32
      _putenv_s("CHOOCHOO_SOUNDBANKS",value ? value : "");
#else
      if(value)setenv("CHOOCHOO_SOUNDBANKS",value,1);else unsetenv("CHOOCHOO_SOUNDBANKS");
#endif
    }
    Fixture() {
      soundbankShutdown();chipnomadState=chipnomadCreate();projectInitAY(&chipnomadState->project);
      cInstrument=0;getInstrumentFunctions(InstrumentType::SoundFont).init(&chipnomadState->project.instruments[0]);
      base=std::filesystem::temp_directory_path()/"choochoo-failed-selection-library";
      REQUIRE_FALSE(std::filesystem::exists(base));folder=base/"__test_failed_selection";
      std::filesystem::create_directories(folder);root(base.string().c_str());
    }
    ~Fixture() {
      soundbankShutdown();chipnomadDestroy(chipnomadState);chipnomadState=saved;
      cInstrument=savedInstrument;projectModified=modified;std::filesystem::remove_all(base);
      root(hadRoot ? previousRoot.c_str() : nullptr);
    }
  } fixture;
  for (const char* name : {"01 Good.wav","02 Failed.wav","03 Good.wav"})
    std::filesystem::copy_file("tests/fixtures/soundbanks/tone.wav",fixture.folder/name);
  auto source=soundbank::openFile((fixture.folder/"02 Failed.wav").string());
  auto bytes=source->bytes(0,source->size());source.reset();
  for(size_t pos=12;pos+8<=bytes.size();) {
    if(!memcmp(bytes.data()+pos,"smpl",4)){bytes[pos+8+12]=128;break;}
    auto len=soundbank::read32(bytes.data()+pos+4);pos+=8+len+(len&1);
  }
  {std::ofstream out(fixture.folder/"02 Failed.wav",std::ios::binary);out.write((const char*)bytes.data(),bytes.size());}
  auto settle=[] {
    const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    do {soundbankPoll();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    while(soundbankBusy() && std::chrono::steady_clock::now()<until);
    REQUIRE_FALSE(soundbankBusy());
  };
  soundbankRescan(true);settle();
  auto& i=chipnomadState->project.instruments[0];
  auto id=[](const char* name){return std::string("__test_failed_selection/")+name;};
  soundbankLoad(0,id("01 Good.wav"),"sample");settle();REQUIRE(i.chip.soundfont.prepared!=0);
  chipnomadInitChips(chipnomadState,48000,nullptr);chipnomadReserveRenderBuffers(chipnomadState,480);
  REQUIRE(chipnomadQueuePlaybackPreviewNote(chipnomadState,0,48,0));
  float audio[960];REQUIRE(chipnomadRender(chipnomadState,audio,480));
  REQUIRE(chipnomadState->soundbankVoices[0][0]->active());
  projectModified=0;
  soundbankCycle(0,1);settle();
  CHECK(std::string(i.chip.soundfont.bank)==id("02 Failed.wav"));
  CHECK(std::string(i.chip.soundfont.preset)=="sample");CHECK(std::string(i.name)=="02 Failed");
  CHECK(i.chip.soundfont.prepared==0);CHECK(projectModified==1);
  CHECK(soundbankInstrumentError(0).find("Invalid audio root note: ")==0);
  gfxClear();screenInstrumentSoundFont.drawField(0,3,CellState::normal);
  screenInstrumentSoundFont.drawField(0,4,CellState::normal);
  CHECK(std::string(mockGfxCells[7],17)=="Preset: 02 Failed");
  CHECK(std::string(mockGfxCells[8],32)==std::string(32,' '));
  CHECK(std::string(mockGfxCells[16],6)=="Error:");
  CHECK(std::string(mockGfxCells[17],23)=="Invalid audio root note");
  REQUIRE(chipnomadQueuePlaybackPreviewNote(chipnomadState,0,48,0));
  for(int n=0;n<10;++n)REQUIRE(chipnomadRender(chipnomadState,audio,480));
  CHECK_FALSE(chipnomadState->soundbankVoices[0][0]->active());
  for(float x:audio)CHECK(std::abs(x)<.00001f);
  // No automatic retry loop, and both directions move from the failed item.
  for(int n=0;n<3;++n){soundbankPoll();CHECK_FALSE(soundbankBusy());}
  soundbankCycle(0,1);settle();CHECK(std::string(i.chip.soundfont.bank)==id("03 Good.wav"));
  CHECK(i.chip.soundfont.prepared!=0);CHECK(soundbankInstrumentError(0).empty());
  screenInstrumentSoundFont.drawField(0,3,CellState::normal);
  CHECK(std::string(mockGfxCells[16],32)==std::string(32,' '));
  mockGfxCells[16][31]='X'; // The healthy instrument's live waveform owns the footer.
  screenInstrumentSoundFont.drawField(0,3,CellState::normal);
  CHECK(mockGfxCells[16][31]=='X');
  soundbankCycle(0,-1);settle();CHECK(std::string(i.chip.soundfont.bank)==id("02 Failed.wav"));
  soundbankCycle(0,-1);settle();CHECK(std::string(i.chip.soundfont.bank)==id("01 Good.wav"));
  // A later request supersedes a failure even when decoding finishes between taps.
  soundbankCycle(0,1);soundbankCycle(0,1);settle();
  CHECK(std::string(i.chip.soundfont.bank)==id("03 Good.wav"));CHECK(soundbankInstrumentError(0).empty());
  // A stale worker must not overwrite an instrument that was replaced meanwhile.
  soundbankLoad(0,id("02 Failed.wav"),"sample");
  strcpy(i.chip.soundfont.preset,"replacement");settle();
  CHECK(std::string(i.chip.soundfont.preset)=="replacement");CHECK(soundbankInstrumentError(0).empty());
  // Parse / missing-file failures follow the same selection and silence path.
  soundbankLoad(0,id("Missing.wav"),"sample");settle();
  CHECK(std::string(i.chip.soundfont.bank)==id("Missing.wav"));CHECK(i.chip.soundfont.prepared==0);
  CHECK_FALSE(soundbankInstrumentError(0).empty());
}

TEST_CASE("SoundFont live envelope edits preserve exclusive group choke fades") {
  using namespace soundbank;
  auto b=parseBank({"tests/fixtures/soundbanks/tone.sf2",""});auto& z=b.presets[0].zones[0];
  z.loop=Loop::continuous;z.offBy=1;z.offTime=.005f;
  SampleCache cache;auto h=publish(cache.prepare(b,"0:0"));SoundbankVoice voice;voice.init(48000);
  auto controls=soundbankDefaultControls();controls.releaseMs=10000;
  voice.configure(h,6000,1,127,20000,controls);voice.noteOn();float audio[960];voice.render(audio,480);
  voice.choke(1,h);voice.configure(h,6000,1,127,20000,controls);
  voice.render(audio,480);CHECK_FALSE(voice.active());collect([](uint32_t){return false;});
}

TEST_CASE("SoundFont native range uses note-on mappings and keeps generic modulation in diagnostics") {
  using namespace soundbank;
  auto* saved=chipnomadState;chipnomadState=chipnomadCreate();
  auto& i=chipnomadState->project.instruments[0];getInstrumentFunctions(InstrumentType::SoundFont).init(&i);
  CHECK(soundbankInstrumentRange(0).empty());CHECK(soundbankInstrumentRange(-1).empty());
  auto bank=parseBank({"tests/fixtures/soundbanks/tone.sf2",""});
  auto& zones=bank.presets[0].zones;zones[0].loKey=60;zones[0].hiKey=62;
  auto high=zones[0];high.loKey=65;high.hiKey=67;zones.push_back(high);
  auto release=zones[0];release.loKey=0;release.hiKey=127;release.releaseTrigger=true;zones.push_back(release);
  bank.warnings={"SoundFont custom modulators unsupported: pmod"};
  SampleCache cache;auto preparedPreset=cache.prepare(bank,"0:0");i.chip.soundfont.prepared=publish(preparedPreset);
  CHECK(soundbankInstrumentRange(0)=="Native keys: C4 to G4 (gaps)");
  CHECK(soundbankInstrumentWarning(0).empty());CHECK_FALSE(preparedPreset->warnings.empty());
  i.chip.soundfont.controls.tuneCents=1200;
  CHECK(soundbankInstrumentRange(0)=="Native keys: C4 to G4 (gaps)");
  zones[0].hiKey=67;bank.warnings.push_back("Other compatibility warning");
  i.chip.soundfont.prepared=publish(cache.prepare(bank,"0:0"));
  CHECK(soundbankInstrumentRange(0)=="Native keys: C4 to G4");
  CHECK(soundbankInstrumentWarning(0)=="Other compatibility warning");
  for(auto& z:zones)z.releaseTrigger=true;
  i.chip.soundfont.prepared=publish(cache.prepare(bank,"0:0"));
  CHECK(soundbankInstrumentRange(0)=="Native keys: none");
  chipnomadDestroy(chipnomadState);chipnomadState=saved;collect([](uint32_t){return false;});
}

#include "selection_popup.h"
TEST_CASE("SoundFont collection popup opens on the active preset without an extra folder step") {
  auto* prior=chipnomadState;chipnomadState=chipnomadCreate();
  static int selected;selected=-1;
  SelectionItem children[]={{"First",10,nullptr,0,nullptr},{"Active",11,nullptr,0,nullptr}};
  SelectionItem roots[]={{"< Instrument",-2,nullptr,0,nullptr},{"Collection",-100,children,2,nullptr}};
  selectionPopupSetup("Presets",roots,2,11,[](int value){selected=value;},nullptr,false,[](int,bool){},true,true);
  screenSelectionPopup.onInput(1,keyEdit,0);CHECK(selected==-1);
  screenSelectionPopup.onInput(0,0,0);CHECK(selected==11);
  screenSelectionPopup.onInput(1,keyDown,0);
  screenSelectionPopup.onInput(1,keyEdit,0);screenSelectionPopup.onInput(0,0,0);CHECK(selected==10);
  chipnomadDestroy(chipnomadState);chipnomadState=prior;
}

TEST_CASE("SoundFont cycling follows nested browser folders instead of interleaving their names") {
  using namespace soundbank;
  std::vector<BankEntry> banks(4);
  banks[0].id="open-sharable/Korean/Flutes.sf2";banks[0].format="sf2";banks[0].presets={{"0:0","A flute"},{"0:1","Z flute"}};
  banks[1].id="open-sharable/Akai/Bass.exs";banks[1].format="exs";banks[1].presets={{"exs","Middle bass"}};
  banks[2].id="open-sharable/Korean/Strings.sf2";banks[2].format="sf2";banks[2].presets={{"0:0","B strings"}};
  banks[3].id="reddit/Drums.sf2";banks[3].format="sf2";banks[3].presets={{"0:0","A drum"}};
  auto order=presetSequence(banks);
  const std::vector<std::pair<int,int>> expected={{1,0},{0,0},{2,0},{0,1},{3,0}};
  CHECK(order==expected);
  CHECK(cyclePreset(banks,banks[0].id,"0:0",1)==std::make_pair(2,0));
  CHECK(cyclePreset(banks,banks[0].id,"0:1",-1)==std::make_pair(2,0));
  // Compare the flat sequence with an actual recursive browser walk.
  std::vector<std::pair<int,int>> visible;
  std::function<void(std::string)> visit=[&](std::string path){
    for(const auto& c:browseCollections(banks,path)) {
      if(c.hasFolders)visit(c.path);
      else for(const auto& p:c.presets)visible.emplace_back(p.bank,p.preset);
    }
  };
  visit("");CHECK(order==visible);
}
