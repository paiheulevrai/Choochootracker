#include "doctest.h"

#include <import/import_lsdj.h>
#include <export/export_lsdj.h>
#include <lsdj_format.h>
#include <chipnomad_lib.h>
#include <import/import_common.h>

#include <corelib/corelib_file.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

const size_t SONG_SIZE = 0x8000;

std::vector<uint8_t> makeSong() {
  std::vector<uint8_t> s(SONG_SIZE, 0);
  s[0x1E78] = 'r'; s[0x1E79] = 'b';
  s[0x3E80] = 'r'; s[0x3E81] = 'b';
  s[0x7FF0] = 'r'; s[0x7FF1] = 'b';
  memset(&s[0x1290], 0xFF, 256 * 4);
  memset(&s[0x2080], 0xFF, 128 * 16);
  memset(&s[0x7000], 0xFF, 255 * 16);
  s[0x3FB4] = 150; // tempo
  return s;
}

bool writeLsdsng(const char* path, const std::vector<uint8_t>& song, const char* name) {
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  uint8_t header[9] = {0};
  memcpy(header, name, strlen(name));
  header[8] = 18;
  fwrite(header, 1, 9, f);
  std::vector<uint8_t> c(LSDJ_MAX_BLOCKS * LSDJ_BLOCK_SIZE);
  size_t n = lsdjCompress(song.data(), c.data());
  fwrite(c.data(), 1, n, f);
  fclose(f);
  return true;
}

}

TEST_SUITE("import_lsdj") {

TEST_CASE("imports song, chain, transpose, phrase notes and instruments") {
  auto s = makeSong();
  s[0x1290 + 2 * 4 + 1] = 3;       // song row 2, PU2 -> chain 3
  s[0x2080 + 3 * 16] = 7;          // chain 3 step 0 -> phrase 7
  s[0x2880 + 3 * 16] = 0xFE;       // transpose -2
  s[7 * 16 + 0] = 1;               // lowest note, C-3 = MIDI 36
  s[7 * 16 + 4] = 0x1D;
  s[0x7000 + 7 * 16 + 0] = 2;      // instrument 2
  s[0x7000 + 7 * 16 + 4] = 2;
  s[0x2040 + 2] = 1;               // instrument 2 allocated, pulse
  memcpy(&s[0x1E7A + 2 * 5], "LEAD", 4);
  uint8_t* in = &s[0x3080 + 2 * 16];
  in[0] = 0;                       // pulse
  in[1] = 0xA5;                    // volume A, decreasing, period 5
  in[4] = 0xDE;                    // sweep byte, ignored
  in[7] = 0x83;                    // 50% duty, left and right
  s[0x2040 + 4] = 1;               // instrument 4 allocated, noise, stable
  in = &s[0x3080 + 4 * 16];
  in[0] = 3; in[1] = 0xF3; in[2] = 1; in[7] = 0x02;

  const char* path = "test_import_lsdj_basic.lsdsng";
  REQUIRE(writeLsdsng(path, s, "SONGNAME"));
  Project p;
  projectInit(&p);
  REQUIRE(projectLoadLSDJ(&p, path) == 0);
  remove(path);

  CHECK(strcmp(p.title, "SONGNAME") == 0);
  CHECK(p.song[2][1] == 3);
  CHECK(p.song[0][0] == EMPTY_VALUE_16);
  CHECK(p.chains[3].rows[0].phrase == 7);
  CHECK((int8_t)p.chains[3].rows[0].transpose == -2);
  CHECK(p.chains[3].rows[1].phrase == EMPTY_VALUE_16);
  CHECK(p.phrases[7].rows[0].note == 24); // MIDI 36 = pitch index 24
  CHECK(p.phrases[7].rows[4].note == 24 + 0x1D - 1);
  CHECK(p.phrases[7].rows[0].instrument == 2);
  CHECK(p.phrases[7].rows[1].note == EMPTY_VALUE_8);
  CHECK(p.tickRate == doctest::Approx(60.0f)); // 150 bpm * 0.4

  const Instrument& lead = p.instruments[2];
  CHECK(lead.type == InstrumentType::GBPulse);
  CHECK(strcmp(lead.name, "LEAD") == 0);
  CHECK(lead.chip.simpleChip.mode == 2);
  CHECK(lead.chip.simpleChip.envelopeInitial == 10);
  CHECK(lead.chip.simpleChip.envelopeIncrease == 0);
  CHECK(lead.chip.simpleChip.envelopePeriod == 5);
  CHECK(lead.chip.simpleChip.sweepPeriod == 0);
  CHECK(p.phrases[7].rows[0].volume == PHRASE_VOLUME_MAX);
  CHECK(lead.pan == 128);

  const Instrument& noise = p.instruments[4];
  CHECK(noise.type == InstrumentType::GBNoise);
  CHECK(noise.chip.simpleChip.mode == 1);
  CHECK(noise.chip.simpleChip.envelopeInitial == 15);
  CHECK(noise.pan == 0);
  CHECK(strcmp(noise.name, "LSDJ 04") == 0);
  projectFree(&p);
}

TEST_CASE("imports commands and grooves") {
  auto s = makeSong();
  s[0x1290 + 1] = 3;               // row 0, PU2 -> chain 3 -> phrase 7
  s[0x2080 + 3 * 16] = 7;
  s[0x1290 + 2] = 4;               // row 0, WAV -> chain 4 -> phrase 8
  s[0x2080 + 4 * 16] = 8;
  s[0x4000 + 7 * 16 + 0] = 4;      // E 5A
  s[0x4FF0 + 7 * 16 + 0] = 0x5A;
  s[0x4000 + 7 * 16 + 1] = 8;      // K 03
  s[0x4FF0 + 7 * 16 + 1] = 3;
  s[0x4000 + 7 * 16 + 2] = 6;      // G 02
  s[0x4FF0 + 7 * 16 + 2] = 2;
  s[0x4000 + 7 * 16 + 3] = 2;      // C 37
  s[0x4FF0 + 7 * 16 + 3] = 0x37;
  s[0x4000 + 7 * 16 + 4] = 3;      // D 02
  s[0x4FF0 + 7 * 16 + 4] = 2;
  s[0x4000 + 7 * 16 + 5] = 16;     // V 44
  s[0x4FF0 + 7 * 16 + 5] = 0x44;
  s[0x4000 + 7 * 16 + 6] = 14;     // S: dropped
  s[0x4FF0 + 7 * 16 + 6] = 0x12;
  s[0x4000 + 8 * 16 + 0] = 4;      // E on the wave channel: dropped
  s[0x4FF0 + 8 * 16 + 0] = 0x20;
  uint8_t groove[16] = {9, 3, 9, 3};
  memcpy(&s[0x1090 + 2 * 16], groove, 16);

  const char* path = "test_import_lsdj_fx.lsdsng";
  REQUIRE(writeLsdsng(path, s, "FX"));
  Project p;
  projectInit(&p);
  REQUIRE(projectLoadLSDJ(&p, path) == 0);
  remove(path);

  const PhraseRow* r = p.phrases[7].rows;
  CHECK(r[0].fx[0][0] == fxCEI); CHECK(r[0].fx[0][1] == 5);
  CHECK(r[0].fx[1][0] == fxCEP); CHECK(r[0].fx[1][1] == 2);
  CHECK(r[0].fx[2][0] == fxCED); CHECK(r[0].fx[2][1] == 1);
  CHECK(r[1].fx[0][0] == fxKIL); CHECK(r[1].fx[0][1] == 3);
  CHECK(r[2].fx[0][0] == fxGGR); CHECK(r[2].fx[0][1] == 2);
  CHECK(r[3].fx[0][0] == fxARP); CHECK(r[3].fx[0][1] == 0x37);
  CHECK(r[4].fx[0][0] == fxDEL); CHECK(r[4].fx[0][1] == 2);
  CHECK(r[5].fx[0][0] == fxPVB); CHECK(r[5].fx[0][1] == 0x44);
  CHECK(r[6].fx[0].command == FX_ID_EMPTY);
  CHECK(p.phrases[8].rows[0].fx[0].command == FX_ID_EMPTY);
  CHECK(p.grooves[2].speed[0] == 9);
  CHECK(p.grooves[2].speed[1] == 3);
  CHECK(p.grooves[2].speed[4] == EMPTY_VALUE_8);
  projectFree(&p);
}

TEST_CASE("rejects files that are not LSDj songs") {
  const char* path = "test_import_lsdj_bad.lsdsng";
  FILE* f = fopen(path, "wb");
  REQUIRE(f);
  uint8_t junk[64] = {1, 2, 3};
  fwrite(junk, 1, sizeof(junk), f);
  fclose(f);
  Project p;
  projectInit(&p);
  CHECK(projectLoadLSDJ(&p, path) != 0);
  remove(path);
  CHECK(projectLoadLSDJ(&p, "does_not_exist.lsdsng") != 0);
  projectFree(&p);
}


TEST_CASE("compression round trip, including escaped bytes and runs") {
  std::vector<uint8_t> song(SONG_SIZE);
  for (size_t i = 0; i < song.size(); i++) song[i] = (uint8_t)((i * 7919) >> 3);
  song[100] = 0xC0; song[101] = 0xE0; song[102] = 0xC0; song[103] = 0xC0;
  memset(&song[5000], 0xAB, 700);
  std::vector<uint8_t> packed(LSDJ_MAX_BLOCKS * LSDJ_BLOCK_SIZE);
  size_t n = lsdjCompress(song.data(), packed.data());
  REQUIRE(n > 0);
  CHECK(n % LSDJ_BLOCK_SIZE == 0);
  std::vector<uint8_t> back(SONG_SIZE);
  REQUIRE(lsdjDecompress(packed.data(), n, back.data()));
  CHECK(back == song);
}

TEST_CASE("export into a template, then import again") {
  auto s = makeSong();
  memcpy(&s[0x6000], "WAVEDATA", 8); // template content outside the song stays
  const char* tpl = "test_export_lsdj_template.lsdsng";
  const char* out = "test_export_lsdj_out.lsdsng";
  REQUIRE(writeLsdsng(tpl, s, "TEMPLATE"));

  Project p;
  projectInit(&p);
  strcpy(p.title, "My song");
  p.tickRate = 60.0f;
  p.song[0][0] = 5;
  p.song[1][3] = 6;
  p.song[2][4] = 7; // fifth track: dropped
  chainClear(&p.chains[5]);
  p.chains[5].rows[0].phrase = 9;
  p.chains[5].rows[0].transpose = 0xFE;
  phraseClear(&p.phrases[9]);
  for (int r = 0; r < 16; r++) initEmptyPhraseRow(&p.phrases[9].rows[r]);
  p.phrases[9].rows[0].note = 30;   // index 30 = LSDj note 7
  p.phrases[9].rows[0].instrument = 3;
  p.phrases[9].rows[2].note = 0;    // below the LSDj range: clamped to 1
  p.phrases[9].rows[2].instrument = 3;
  getInstrumentFunctions(InstrumentType::GBPulse).init(&p.instruments[3]);
  p.instruments[3].chip.simpleChip.mode = 3;
  p.instruments[3].chip.simpleChip.envelopeInitial = 12;
  p.instruments[3].chip.simpleChip.envelopeIncrease = 1;
  p.instruments[3].chip.simpleChip.envelopePeriod = 2;
  snprintf(p.instruments[3].name, sizeof(p.instruments[3].name), "Lead 1");

  REQUIRE(projectExportLSDJ(&p, tpl, out) == 0);

  Project q;
  projectInit(&q);
  REQUIRE(projectLoadLSDJ(&q, out) == 0);
  CHECK(strcmp(q.title, "MYSONG") == 0);
  CHECK(q.tickRate == doctest::Approx(60.0f));
  CHECK(q.song[0][0] == 5);
  CHECK(q.song[1][3] == 6);
  CHECK(q.song[2][4] == EMPTY_VALUE_16);
  CHECK(q.chains[5].rows[0].phrase == 9);
  CHECK((int8_t)q.chains[5].rows[0].transpose == -2);
  CHECK(q.phrases[9].rows[0].note == 30);
  CHECK(q.phrases[9].rows[0].instrument == 3);
  CHECK(q.phrases[9].rows[1].note == EMPTY_VALUE_8);
  CHECK(q.phrases[9].rows[2].note == 24);
  CHECK(q.instruments[3].type == InstrumentType::GBPulse);
  CHECK(strcmp(q.instruments[3].name, "LEAD") == 0);
  CHECK(q.instruments[3].chip.simpleChip.mode == 3);
  CHECK(q.instruments[3].chip.simpleChip.envelopeInitial == 12);
  CHECK(q.instruments[3].chip.simpleChip.envelopeIncrease == 1);
  CHECK(q.instruments[3].chip.simpleChip.envelopePeriod == 2);

  // A chain above LSDj's 128 makes the export fail
  p.song[3][0] = 130;
  CHECK(projectExportLSDJ(&p, tpl, out) != 0);
  CHECK(projectExportLSDJ(&p, "does_not_exist.lsdsng", out) != 0);

  remove(tpl);
  remove(out);
  projectFree(&p);
  projectFree(&q);
}


// Position of the jump id that ends a compressed block, or -1
int jumpPosition(const uint8_t* block) {
  size_t i = 0;
  while (i < LSDJ_BLOCK_SIZE) {
    uint8_t b = block[i++];
    if (b == 0xC0) { if (block[i++] != 0xC0) i++; }
    else if (b == 0xE0) {
      uint8_t v = block[i++];
      if (v == 0xFF) return -1;
      if (v == 0xF0 || v == 0xF1) i++;
      else if (v != 0xE0) return (int)i - 1;
    }
  }
  return -1;
}

// Copies a compressed song into the sav at the given block indices
void placeSong(std::vector<uint8_t>& sav, int project, const char* name, uint8_t version,
               const std::vector<uint8_t>& song, const std::vector<int>& blocks) {
  std::vector<uint8_t> packed(LSDJ_MAX_BLOCKS * LSDJ_BLOCK_SIZE);
  size_t n = lsdjCompress(song.data(), packed.data());
  REQUIRE(n / LSDJ_BLOCK_SIZE == blocks.size());
  memcpy(&sav[LSDJ_SAV_NAMES + project * 8], name, strlen(name));
  sav[LSDJ_SAV_VERSIONS + project] = version;
  for (size_t b = 0; b < blocks.size(); b++) {
    uint8_t* block = &packed[b * LSDJ_BLOCK_SIZE];
    int pos = jumpPosition(block);
    if (pos >= 0) block[pos] = (uint8_t)(blocks[b + 1] + 1);
    memcpy(&sav[LSDJ_SAV_BLOCKS + blocks[b] * LSDJ_BLOCK_SIZE], block, LSDJ_BLOCK_SIZE);
    sav[LSDJ_SAV_BLOCK_TABLE + blocks[b]] = (uint8_t)project;
  }
}

TEST_CASE("extracts the songs of a .sav, with blocks out of order") {
  std::vector<uint8_t> sav(LSDJ_SAV_SIZE, 0);
  memset(&sav[LSDJ_SAV_BLOCK_TABLE], 0xFF, LSDJ_SAV_BLOCK_COUNT);
  sav[LSDJ_SAV_MARKER] = 'j'; sav[LSDJ_SAV_MARKER + 1] = 'k';

  auto a = makeSong();
  a[0x1290 + 1] = 3; a[0x2080 + 3 * 16] = 7; a[7 * 16] = 1; a[0x7000 + 7 * 16] = 0;
  auto b = makeSong();
  b[0x1290 + 1] = 4; b[0x2080 + 4 * 16] = 8; b[8 * 16] = 5;
  std::vector<uint8_t> packed(LSDJ_MAX_BLOCKS * LSDJ_BLOCK_SIZE);
  size_t blocksA = lsdjCompress(a.data(), packed.data()) / LSDJ_BLOCK_SIZE;
  size_t blocksB = lsdjCompress(b.data(), packed.data()) / LSDJ_BLOCK_SIZE;
  REQUIRE(blocksA >= 1); REQUIRE(blocksB >= 1);
  // Song A takes the even blocks 20, 22, ...; song B the odd ones starting at 1
  std::vector<int> ia, ib;
  for (size_t i = 0; i < blocksA; i++) ia.push_back(20 + 2 * (int)i);
  for (size_t i = 0; i < blocksB; i++) ib.push_back(1 + 2 * (int)i);
  placeSong(sav, 0, "ALPHA", 0x0B, a, ia);
  placeSong(sav, 5, "BETA", 0x02, b, ib);

  const char* savPath = "test_extract_lsdj.sav";
  const char* dir = "test_extract_lsdj";
  FILE* f = fopen(savPath, "wb");
  REQUIRE(f);
  fwrite(sav.data(), 1, sav.size(), f);
  fclose(f);

  int count = 0;
  LsdjSavSong songs[32];
  REQUIRE(lsdjListSav(savPath, songs, 32, &count) == 0);
  REQUIRE(count == 2);
  CHECK(songs[0].slot == 0);
  CHECK(strcmp(songs[0].name, "ALPHA") == 0);
  CHECK(songs[0].version == 0x0B);
  CHECK(songs[0].blocks == (int)blocksA);
  CHECK(songs[1].slot == 5);
  CHECK(strcmp(songs[1].name, "BETA") == 0);
  {
    Project direct;
    projectInit(&direct);
    CHECK(projectLoadLSDJSav(&direct, savPath, 3) != 0); // empty slot
    REQUIRE(projectLoadLSDJSav(&direct, savPath, 5) == 0);
    CHECK(strcmp(direct.title, "BETA") == 0);
    CHECK(direct.phrases[8].rows[0].note == 28);
    projectFree(&direct);
  }

  REQUIRE(lsdjExtractSav(savPath, dir, &count) == 0);
  CHECK(count == 2);

  Project p;
  projectInit(&p);
  REQUIRE(projectLoadLSDJ(&p, "test_extract_lsdj/ALPHA_v0B.lsdsng") == 0);
  CHECK(strcmp(p.title, "ALPHA") == 0);
  CHECK(p.song[0][1] == 3);
  CHECK(p.phrases[7].rows[0].note == 24);
  REQUIRE(projectLoadLSDJ(&p, "test_extract_lsdj/BETA_v02.lsdsng") == 0);
  CHECK(strcmp(p.title, "BETA") == 0);
  CHECK(p.song[0][1] == 4);
  CHECK(p.phrases[8].rows[0].note == 28);
  projectFree(&p);

  remove("test_extract_lsdj/ALPHA_v0B.lsdsng");
  remove("test_extract_lsdj/BETA_v02.lsdsng");
  remove(dir);
  CHECK(lsdjExtractSav("test_import_lsdj_none.sav", dir, &count) != 0);
  f = fopen(savPath, "wb");
  fwrite("junk", 1, 4, f);
  fclose(f);
  CHECK(lsdjExtractSav(savPath, dir, &count) != 0);
  remove(savPath);
}

}
