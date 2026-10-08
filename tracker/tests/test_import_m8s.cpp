#include "doctest.h"

#include <import/import_m8s.h>
#include <export/export_m8s.h>
#include <chipnomad_lib.h>
#include <four_op_patch.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

const size_t M8S_SIZE = 0x13A3E + 128 * 215 + 4096;

std::vector<uint8_t> makeM8S(uint8_t major = 2) {
  std::vector<uint8_t> d(M8S_SIZE, 0);
  memcpy(d.data(), "M8VERSION", 9);
  d[10] = 0x51;
  d[11] = major;
  memcpy(&d[14], "/Songs/", 7);
  float bpm = 120.0f;
  memcpy(&d[14 + 128 + 1], &bpm, sizeof(bpm));
  memcpy(&d[14 + 128 + 6], "TESTSONG", 8);
  memset(&d[0x2EE], 0xFF, 256 * 8);
  for (int p = 0; p < 255; p++)
    for (int s = 0; s < 16; s++) {
      uint8_t* step = &d[0xAEE + (p * 16 + s) * 9];
      memset(step, 0xFF, 9);
    }
  memset(&d[0x9A5E], 0xFF, 255 * 32);
  return d;
}

bool writeFile(const char* path, const std::vector<uint8_t>& d) {
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  fwrite(d.data(), 1, d.size(), f);
  fclose(f);
  return true;
}

}

TEST_SUITE("import_m8s") {

TEST_CASE("imports song, chain, transpose and phrase notes with the same indices") {
  auto d = makeM8S();
  d[0x2EE + 2 * 8 + 1] = 3;                 // song row 2, track 1 -> chain 3
  d[0x9A5E + 3 * 32 + 0] = 7;               // chain 3 step 0 -> phrase 7
  d[0x9A5E + 3 * 32 + 1] = 0xFE;            // transpose -2
  uint8_t* s0 = &d[0xAEE + (7 * 16 + 0) * 9];
  s0[0] = 0x3C; s0[1] = 0x64; s0[2] = 0x05; // note 60, vel 100, instrument 5
  uint8_t* s1 = &d[0xAEE + (7 * 16 + 4) * 9];
  s1[0] = 0x80;                             // note off
  memcpy(&d[0x13A3E + 5 * 215 + 1], "PULSEBASS", 9);
  d[0x13A3E + 5 * 215 + 10] = 0xFF;

  const char* path = "test_import_m8s_basic.m8s";
  REQUIRE(writeFile(path, d));

  Project p;
  projectInit(&p);
  REQUIRE(projectLoadM8S(&p, path) == 0);
  remove(path);

  CHECK(strcmp(p.title, "TESTSONG") == 0);
  CHECK(p.song[2][1] == 3);
  CHECK(p.song[0][0] == EMPTY_VALUE_16);
  CHECK(p.chains[3].rows[0].phrase == 7);
  CHECK((int8_t)p.chains[3].rows[0].transpose == -2);
  CHECK(p.chains[3].rows[1].phrase == EMPTY_VALUE_16);
  CHECK(p.phrases[7].rows[0].note == 48); // M8 60 = MIDI 60 = pitch index 48
  CHECK(p.phrases[7].rows[0].volume == 0x64);
  CHECK(p.phrases[7].rows[0].instrument == 5);
  CHECK(p.phrases[7].rows[1].note == EMPTY_VALUE_8);
  CHECK(p.phrases[7].rows[4].note == NOTE_OFF);
  CHECK(strcmp(p.instruments[5].name, "PULSEBASS") == 0);
  CHECK(p.instruments[5].type == InstrumentType::AChChid); // type 0 is a WavSynth
  // 120 BPM, 4 steps per beat, 6 ticks per step -> 48 ticks/s
  CHECK(p.tickRate == doctest::Approx(48.0f));
  projectFree(&p);
}

TEST_CASE("MacroSynth instruments become Braids") {
  auto d = makeM8S();
  uint8_t* s0 = &d[0xAEE];
  s0[0] = 0x3C; s0[2] = 0x00;
  uint8_t* s1 = &d[0xAEE + 9];
  s1[0] = 0x3C; s1[2] = 0x01;
  uint8_t* in = &d[0x13A3E];
  in[0] = 1;                                // MacroSynth
  in[18] = 0x0E; in[19] = 0xBA; in[20] = 0x6B;
  in[23] = 1; in[24] = 0x9F; in[25] = 0x6B; in[28] = 0xB1;
  d[0x13A3E + 215] = 0xFF;                  // empty slot stays AY

  const char* path = "test_import_m8s_macro.m8s";
  REQUIRE(writeFile(path, d));
  Project p;
  projectInit(&p);
  REQUIRE(projectLoadM8S(&p, path) == 0);
  remove(path);

  const Instrument& b = p.instruments[0];
  CHECK(b.type == InstrumentType::Braids);
  CHECK(b.chip.braids.model == 14);
  CHECK(b.chip.braids.timbre == 0xBA * 129);
  CHECK(b.chip.braids.color == 0x6B * 129);
  CHECK(b.chip.braids.filterEnabled == 1);
  CHECK(b.chip.braids.filterMode == 0);
  CHECK(b.chip.braids.filterResonance == 0x6B);
  CHECK(b.pan == 0xB1);
  CHECK(p.instruments[1].type == InstrumentType::AY1);
  projectFree(&p);
}

TEST_CASE("sampler, FM, wavsynth and hypersynth instruments are converted") {
  auto d = makeM8S();
  for (int i = 0; i < 5; i++) {
    uint8_t* s = &d[0xAEE + i * 9];
    s[0] = 0x3C; s[2] = (uint8_t)i;
  }
  auto inst = [&](int i) { return &d[0x13A3E + i * 215]; };
  uint8_t* wav = inst(0);                   // WavSynth, saw
  wav[0] = 0; wav[18] = 4;
  uint8_t* smp = inst(1);                   // Sampler, forward loop, start 0x10, length 0x40
  smp[0] = 2; smp[18] = 2; smp[20] = 0x10; smp[22] = 0x40; smp[24] = 2; smp[25] = 0xFF; smp[26] = 0x20; smp[29] = 0x90;
  strcpy((char*)smp + 0x57, "/Samples/none/missing.wav");
  uint8_t* fm = inst(2);                    // FMSynth
  fm[0] = 4; fm[18] = 3;
  fm[23] = 2; fm[31] = 0xFF; fm[32] = 0xFF;  // op1 ratio 2, level full, feedback full
  fm[25] = 20; fm[33] = 0;                   // op2 ratio 20 (clamped), level 0
  uint8_t* hy = inst(3);                    // HyperSynth
  hy[0] = 5; hy[27] = 0x80; hy[28] = 0x40;
  uint8_t* mi = inst(4);                    // MIDIOut
  mi[0] = 3; mi[16] = 9; mi[17] = 0xFF; mi[18] = 5; mi[22] = 74; mi[24] = 0xFF; mi[26] = 1;

  const char* path = "test_import_m8s_others.m8s";
  REQUIRE(writeFile(path, d));
  Project p;
  projectInit(&p);
  REQUIRE(projectLoadM8S(&p, path) == 0);
  remove(path);

  CHECK(p.instruments[0].type == InstrumentType::AChChid);
  CHECK(p.instruments[0].chip.achchid.wave == AChChidWave::saw);

  const Instrument& s = p.instruments[1];
  CHECK(s.type == InstrumentType::Sample);
  CHECK(strcmp(s.chip.sample.path, "/Samples/none/missing.wav") == 0);
  CHECK(s.chip.sample.loopMode == 1);
  CHECK(s.chip.sample.pitch == 24);
  CHECK(s.chip.sample.start == 0x10);
  CHECK(s.chip.sample.end == 0x50);
  CHECK(s.chip.sample.filterEnabled == 1);
  CHECK(s.chip.sample.filterMode == 2);
  CHECK(s.chip.sample.filterCutoffHz == 20000);
  CHECK(s.chip.sample.filterResonance == 0x20);
  CHECK(s.pan == 0x90);

  const Instrument& f = p.instruments[2];
  CHECK(f.type == InstrumentType::GenesisFM);
  CHECK(f.chip.fourOp.algorithm == 3);
  CHECK(f.chip.fourOp.operators[0].multiplier == 2);
  CHECK(f.chip.fourOp.operators[0].level == 0);
  CHECK(f.chip.fourOp.operators[1].multiplier == 15);
  CHECK(f.chip.fourOp.operators[1].level == 127);
  CHECK(f.chip.fourOp.feedback == 7);
  CHECK(validFourOp(f.type, f.chip.fourOp));

  const Instrument& h = p.instruments[3];
  CHECK(h.type == InstrumentType::AChChid);
  CHECK(h.chip.achchid.wave == AChChidWave::braids);
  CHECK(h.chip.achchid.model == 14);
  CHECK(h.chip.achchid.timbre == 0x80 * 129);
  CHECK(h.chip.achchid.color == 0x40 * 129);

  const Instrument& m = p.instruments[4];
  CHECK(m.type == InstrumentType::Midi);
  CHECK(m.chip.midi.channel == 9);
  CHECK(m.chip.midi.bankHigh == EMPTY_VALUE_8);
  CHECK(m.chip.midi.program == 5);
  CHECK(m.chip.midi.ccNumber[0] == 74);
  CHECK(m.chip.midi.ccNumber[1] == EMPTY_VALUE_8);
  CHECK(m.chip.midi.ccNumber[2] == 1);
  projectFree(&p);
}

TEST_CASE("sampler finds its WAV near the song or in the sample folder") {
  auto d = makeM8S();
  d[0xAEE] = 0x3C; d[0xAEE + 2] = 0;
  uint8_t* smp = &d[0x13A3E];
  smp[0] = 2; smp[22] = 0xFF;
  strcpy((char*)smp + 0x57, "/Samples/Pack/Kick_4.wav");
  REQUIRE(system("rm -rf m8s_t && mkdir -p m8s_t/song m8s_t/lib/samples/Pack") == 0);
  const char* path = "m8s_t/song/a.m8s";
  REQUIRE(writeFile(path, d));

  Project p;
  projectInit(&p);
  REQUIRE(projectLoadM8SWithSamples(&p, path, "m8s_t/lib/samples") == 0);
  CHECK(strcmp(p.instruments[0].chip.sample.path, "/Samples/Pack/Kick_4.wav") == 0); // not there yet
  projectFree(&p);

  // a real (tiny) WAV in a lower case samples folder, found through the sample dir
  const uint8_t wav[] = {'R','I','F','F',40,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,0x44,0xAC,0,0,0x88,0x58,1,0,2,0,16,0,
                         'd','a','t','a',4,0,0,0,0,0,0x10,0};
  FILE* f = fopen("m8s_t/lib/samples/Pack/Kick_4.wav", "wb");
  REQUIRE(f);
  fwrite(wav, 1, sizeof(wav), f);
  fclose(f);
  projectInit(&p);
  REQUIRE(projectLoadM8SWithSamples(&p, path, "m8s_t/lib") == 0);
  CHECK((strstr(p.instruments[0].chip.sample.path, "samples/Pack/Kick_4.wav") != nullptr) == true);
  CHECK(p.instruments[0].chip.sample.frameCount == 2);
  projectFree(&p);
  system("rm -rf m8s_t");
}

TEST_CASE("rejects files that are not M8 songs") {
  auto d = makeM8S();
  memcpy(d.data(), "NOTM8SONG", 9);
  const char* path = "test_import_m8s_bad.m8s";
  REQUIRE(writeFile(path, d));
  Project p;
  projectInit(&p);
  CHECK(projectLoadM8S(&p, path) != 0);

  auto v = makeM8S(9);
  REQUIRE(writeFile(path, v));
  CHECK(projectLoadM8S(&p, path) != 0);

  std::vector<uint8_t> tiny(100, 0);
  REQUIRE(writeFile(path, tiny));
  CHECK(projectLoadM8S(&p, path) != 0);
  remove(path);
  projectFree(&p);
}


TEST_CASE("export converts Braids and Sample instruments using template records") {
  auto tmpl = makeM8S();
  uint8_t* mac = &tmpl[0x13A3E + 20 * 215];
  mac[0] = 1; mac[100] = 0x77;              // a MacroSynth to copy from
  uint8_t* smp = &tmpl[0x13A3E + 21 * 215];
  smp[0] = 2; smp[60] = 0x66;              // a Sampler to copy from
  const char* tmplPath = "test_export_m8s_inst_template.m8s";
  const char* outPath = "test_export_m8s_inst_out.m8s";
  REQUIRE(writeFile(tmplPath, tmpl));

  Project src;
  projectInit(&src);
  Instrument* b = &src.instruments[7];
  getInstrumentFunctions(InstrumentType::Braids).init(b);
  strcpy(b->name, "SWARM");
  b->chip.braids.model = 14;
  b->chip.braids.timbre = 0xBA * 129;
  b->chip.braids.color = 0x6B * 129;
  b->chip.braids.filterMode = 2;
  b->chip.braids.filterCutoffHz = 20000;
  b->chip.braids.filterResonance = 0x40;
  b->pan = 0xB1;
  Instrument* s = &src.instruments[8];
  getInstrumentFunctions(InstrumentType::Sample).init(s);
  strcpy(s->name, "KICK");
  strcpy(s->chip.sample.path, "/home/x/samples/Pack/Kick_4.wav");
  s->chip.sample.loopMode = 1;
  s->chip.sample.start = 0x10;
  s->chip.sample.end = 0x50;
  s->chip.sample.filterEnabled = 0;
  Instrument* unused = &src.instruments[9];
  getInstrumentFunctions(InstrumentType::Braids).init(unused);
  src.phrases[0].rows[0].note = 36; src.phrases[0].rows[0].instrument = 7;
  src.phrases[0].rows[1].note = 36; src.phrases[0].rows[1].instrument = 8;
  REQUIRE(projectExportM8S(&src, tmplPath, outPath) == 0);

  FILE* f = fopen(outPath, "rb");
  REQUIRE(f);
  std::vector<uint8_t> out(M8S_SIZE);
  REQUIRE(fread(out.data(), 1, out.size(), f) == out.size());
  fclose(f);
  remove(tmplPath);

  const uint8_t* m = &out[0x13A3E + 7 * 215];
  CHECK(m[0] == 1);
  CHECK(m[100] == 0x77);                    // the rest of the template record is kept
  CHECK(strcmp((const char*)m + 1, "SWARM") == 0);
  CHECK(m[18] == 14);
  CHECK(m[19] == 0xBA);
  CHECK(m[20] == 0x6B);
  CHECK(m[23] == 2);                        // high pass
  CHECK(m[24] == 255);                      // 20 kHz
  CHECK(m[25] == 0x40);
  CHECK(m[28] == 0xB1);
  const uint8_t* r = &out[0x13A3E + 8 * 215];
  CHECK(r[0] == 2);
  CHECK(r[60] == 0x66);
  CHECK(strcmp((const char*)r + 0x57, "/Samples/Kick_4.wav") == 0);
  CHECK(r[18] == 2);                        // forward loop
  CHECK(r[20] == 0x10);
  CHECK(r[22] == 0x40);
  CHECK(r[24] == 0);                        // no filter
  CHECK(out[0x13A3E + 9 * 215] == 0);       // unused instrument: template slot untouched

  Project back;
  projectInit(&back);
  REQUIRE(projectLoadM8S(&back, outPath) == 0);
  remove(outPath);
  CHECK(back.instruments[7].type == InstrumentType::Braids);
  CHECK(back.instruments[7].chip.braids.model == 14);
  CHECK(back.instruments[7].chip.braids.timbre == 0xBA * 129);
  CHECK(back.instruments[8].type == InstrumentType::Sample);
  CHECK(back.instruments[8].chip.sample.loopMode == 1);
  CHECK(back.instruments[8].chip.sample.end == 0x50);
  projectFree(&back);
  projectFree(&src);
}

TEST_CASE("export writes structure and notes into the template and round-trips") {
  auto tmpl = makeM8S();
  tmpl[0x13A3E + 5 * 215 + 50] = 0x5A;  // instrument data must survive untouched
  tmpl[0x1A600] = 0xA5;                 // so must anything past the instruments
  tmpl[0xAEE] = 0x30;                   // template phrase content is replaced
  const char* tmplPath = "test_export_m8s_template.m8s";
  const char* outPath = "test_export_m8s_out.m8s";
  REQUIRE(writeFile(tmplPath, tmpl));

  Project src;
  projectInit(&src);
  strcpy(src.title, "ROUNDTRIP");
  src.tickRate = 60.0f; // 150 BPM
  src.song[1][2] = 4;
  src.chains[4].rows[0].phrase = 9;
  src.chains[4].rows[0].transpose = 0xFD; // -3
  src.phrases[9].rows[0].note = 36;       // MIDI 48
  src.phrases[9].rows[0].volume = 100;
  src.phrases[9].rows[0].instrument = 3;
  src.phrases[9].rows[5].note = NOTE_OFF;
  REQUIRE(projectExportM8S(&src, tmplPath, outPath) == 0);

  FILE* f = fopen(outPath, "rb");
  REQUIRE(f);
  std::vector<uint8_t> out(M8S_SIZE);
  REQUIRE(fread(out.data(), 1, out.size(), f) == out.size());
  fclose(f);
  CHECK(out.size() == tmpl.size());
  CHECK(out[0x13A3E + 5 * 215 + 50] == 0x5A);
  CHECK(out[0x1A600] == 0xA5);
  CHECK(out[0xAEE] == 0xFF);
  CHECK(out[0x2EE + 1 * 8 + 2] == 4);
  CHECK(out[0x2EE] == 0xFF);
  const uint8_t* s0 = &out[0xAEE + (9 * 16) * 9];
  CHECK(s0[0] == 48);
  CHECK(s0[1] == 100);
  CHECK(s0[2] == 3);
  CHECK(s0[3] == 0xFF);
  CHECK(out[0xAEE + (9 * 16 + 5) * 9] == 0x80);

  Project back;
  projectInit(&back);
  REQUIRE(projectLoadM8S(&back, outPath) == 0);
  CHECK(strcmp(back.title, "ROUNDTRIP") == 0);
  CHECK(back.tickRate == doctest::Approx(60.0f));
  CHECK(back.song[1][2] == 4);
  CHECK(back.chains[4].rows[0].phrase == 9);
  CHECK(back.chains[4].rows[0].transpose == 0xFD);
  CHECK(back.phrases[9].rows[0].note == 36);
  CHECK(back.phrases[9].rows[0].volume == 100);
  CHECK(back.phrases[9].rows[0].instrument == 3);
  CHECK(back.phrases[9].rows[5].note == NOTE_OFF);

  remove(tmplPath);
  remove(outPath);
  projectFree(&src);
  projectFree(&back);
}

TEST_CASE("export refuses phrases the M8 cannot hold and bad templates") {
  auto tmpl = makeM8S();
  const char* tmplPath = "test_export_m8s_template2.m8s";
  const char* outPath = "test_export_m8s_out2.m8s";
  REQUIRE(writeFile(tmplPath, tmpl));

  Project p;
  projectInit(&p);
  p.song[0][0] = 0;
  p.chains[0].rows[0].phrase = 300;
  CHECK(projectExportM8S(&p, tmplPath, outPath) != 0);

  p.chains[0].rows[0].phrase = 1;
  memcpy(tmpl.data(), "NOTM8SONG", 9);
  REQUIRE(writeFile(tmplPath, tmpl));
  CHECK(projectExportM8S(&p, tmplPath, outPath) != 0);
  CHECK(projectExportM8S(&p, "does-not-exist.m8s", outPath) != 0);

  remove(tmplPath);
  remove(outPath);
  projectFree(&p);
}

}
