#include "doctest.h"
#include "project.h"
#include "opll_presets.h"
#include "synth/opll_voice.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

TEST_CASE("OPLL types preserve existing IDs and do not grow instrument snapshots") {
  CHECK(int(InstrumentType::Midi) == 16);
  CHECK(int(InstrumentType::Sintered) == 13);
  CHECK(getInstrumentDefinition((InstrumentType)14)->category == InstrumentCategory::none);
  CHECK(getInstrumentDefinition(InstrumentType::totalCount)->category == InstrumentCategory::none);
  CHECK(sizeof(InstrumentOPLL) <= sizeof(InstrumentSample));
  for (auto type : {InstrumentType::OPLL, InstrumentType::VRC7}) {
    Instrument instrument{}; getInstrumentFunctions(type).init(&instrument);
    CHECK(instrument.type == type); CHECK(instrument.chip.opll.schema == 1);
    CHECK(instrument.chip.opll.program == 3);
    CHECK(getInstrumentDefinition(type)->screen == InstrumentScreenKind::opl);
  }
}
TEST_CASE("OPLL programs render independently across arbitrary blocks and sample rates") {
  for (auto type : {InstrumentType::OPLL, InstrumentType::VRC7}) for (int rate : {44100,48000,96000}) {
    for (int program = 1; program <= 15; ++program) {
      CAPTURE(int(type)); CAPTURE(rate); CAPTURE(program);
      Instrument instrument{}; getInstrumentFunctions(type).init(&instrument); REQUIRE(opllApplyPreset(&instrument, program));
      OPLLVoice a,b; a.init(rate); b.init(rate);
      a.configure(&instrument.chip.opll, 6000, 1); b.configure(&instrument.chip.opll, 6000, 1);
      a.noteOn(); b.noteOn();
      std::vector<float> x(rate / 4), y(x.size()); a.render(x.data(), x.size());
      for (size_t i = 0; i < y.size();) { size_t n = std::min(size_t(137), y.size()-i); b.render(y.data()+i,n); i+=n; }
      double energy = 0; float peak = 0;
      for (size_t i = 0; i < x.size(); ++i) { REQUIRE(std::isfinite(x[i])); CHECK(x[i] == y[i]); energy += x[i]*x[i]; peak = std::max(peak,std::abs(x[i])); }
      CHECK(energy > .001); CHECK(peak < 1);
      a.noteOff(); a.render(x.data(),x.size());
      a.kill(); a.render(x.data(),x.size()); for (float v : x) REQUIRE(v == 0);
    }
  }
}
TEST_CASE("OPLL and VRC7 carry different complete preset tones") {
  Instrument a{},b{}; getInstrumentFunctions(InstrumentType::OPLL).init(&a); getInstrumentFunctions(InstrumentType::VRC7).init(&b);
  CHECK(std::memcmp(a.chip.opll.patch,b.chip.opll.patch,8) != 0);
  CHECK_FALSE(opllApplyPreset(&a,0)); CHECK_FALSE(opllApplyPreset(&a,16));
}
TEST_CASE("OPLL CNI and CCT round trips preserve native patch without installed banks") {
  auto a=std::make_unique<Project>(), b=std::make_unique<Project>(); fillFXNames(); projectInit(a.get());projectInit(b.get());
  REQUIRE(projectLoad(a.get(), "packaging/common/projects/gm-midi-demo.cct") == 0);
  for (auto type : {InstrumentType::OPLL,InstrumentType::VRC7}) {
    auto* instrument=&a->instruments[7]; getInstrumentFunctions(type).init(instrument); opllApplyPreset(instrument,14);instrument->chip.opll.fineTune=-27;
    instrument->chip.opll.program=0;instrument->chip.opll.bankId=500;
    strcpy(instrument->chip.opll.presetName,"Custom portable tone");
    REQUIRE(instrumentSave(a.get(),"test_opll.cni",7) == 0);
    REQUIRE(instrumentLoad(b.get(),"test_opll.cni",12) == 0);
    CHECK(b->instruments[12].type == type);
    CHECK(std::memcmp(&b->instruments[12].chip.opll,&instrument->chip.opll,sizeof(InstrumentOPLL)) == 0);
    REQUIRE(projectSave(a.get(),"test_opll.cct") == 0);
    REQUIRE(projectLoad(b.get(),"test_opll.cct") == 0);
    CHECK(b->instruments[7].type == type);
    CHECK(std::memcmp(&b->instruments[7].chip.opll,&instrument->chip.opll,sizeof(InstrumentOPLL)) == 0);
  }
  std::remove("test_opll.cni");std::remove("test_opll.cct");projectFree(a.get());projectFree(b.get());
}
TEST_CASE("OPLL malformed new CNI fails transactionally") {
  auto p=std::make_unique<Project>();projectInit(p.get());getInstrumentFunctions(InstrumentType::MME).init(&p->instruments[3]);
  Instrument before=p->instruments[3]; Table table=p->tables[3];
  FILE* file=fopen("test_opll_bad.cni","wb");REQUIRE(file);
  fputs("# ChipNomad Instrument 6.0\n\n### Instrument 0\n\n- Name: Bad\n- Type: 17\n- Table speed: 1\n- Volume: 255\n- Transpose: 1\n- Chip data:\n- OPLL schema: 999\n",file);fclose(file);
  CHECK(instrumentLoad(p.get(),"test_opll_bad.cni",3) != 0);
  CHECK(std::memcmp(&before,&p->instruments[3],sizeof(before)) == 0); CHECK(std::memcmp(&table,&p->tables[3],sizeof(table)) == 0);
  std::remove("test_opll_bad.cni");projectFree(p.get());
}

#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
TEST_CASE("OPLL preset audition uses owned audio commands without changing song data") {
  ChipNomadState* state=chipnomadCreate(); REQUIRE(state);
  state->project.chipsCount=1;state->project.tracksCount=3;state->project.tickRate=50;state->project.linearPitch=1;
  calculateLinearPitchTable12TET(&state->project);
  chipnomadInitChips(state,48000,nullptr);
  auto before=std::make_unique<Project>(state->project);
  Instrument candidate{};getInstrumentFunctions(InstrumentType::VRC7).init(&candidate);
  REQUIRE(chipnomadQueueOPLLPreview(state,0,&candidate.chip.opll));
  candidate.chip.opll.patch[5]=0; // Queue owns its copy.
  std::vector<float> audio(2048);double energy=0;
  for(int block=0;block<20;++block) { chipnomadRender(state,audio.data(),1024);for(float x:audio)energy+=x*x; }
  CHECK(energy>.001);CHECK(std::memcmp(before.get(),&state->project,sizeof(Project))==0);
  REQUIRE(chipnomadQueueOPLLPreview(state,0,nullptr));
  chipnomadRender(state,audio.data(),1024);chipnomadRender(state,audio.data(),1024);
  for(float x:audio)CHECK(x==0);
  CHECK(std::memcmp(before.get(),&state->project,sizeof(Project))==0);
  chipnomadDestroy(state);
}
TEST_CASE("OPLL frequency follows cents at every supported host rate") {
  InstrumentOPLL sine{};sine.schema=1;sine.program=1;
  const uint8_t tone[8]={0x01,0x21,0x3f,0,0xf0,0xf0,0,0x07};memcpy(sine.patch,tone,8);
  for(int rate:{22050,32000,44100,48000,96000})for(int note:{4500,5700,6900,8100}) {
    CAPTURE(rate);CAPTURE(note);
    OPLLVoice voice;voice.init(rate);voice.configure(&sine,note,1);voice.noteOn();
    std::vector<float> buffer(rate);voice.render(buffer.data(),buffer.size());
    int crossings=0;for(int i=rate/4+1;i<rate;++i)if(buffer[i-1]<=0 && buffer[i]>0)++crossings;
    double hz=crossings/.75,expected=440*std::exp2((note-6900)/1200.0);
    CHECK(std::abs(hz-expected)<expected*.012+1.5);
  }
}
