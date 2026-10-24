#include <chipnomad_lib.h>
#include <project_utils.h>
#include "import_lsdj.h"
#include "import_common.h"
#include <lsdj_format.h>

#include "corelib/corelib_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory>

static void copyName(char* dest, size_t destSize, const uint8_t* src, size_t srcSize) {
  size_t len = 0;
  while (len < srcSize && src[len] >= 0x20 && src[len] < 0x7F && len < destSize - 1) {
    dest[len] = (char)src[len];
    len++;
  }
  while (len > 0 && dest[len - 1] == ' ') len--;
  dest[len] = 0;
}

// The envelope byte is the Game Boy NRx2 register: initial volume in the high
// nibble, direction in bit 3 and period in the low 3 bits.
static void convertEnvelope(InstrumentSimpleChip* s, uint8_t env) {
  s->envelopeInitial = env >> 4;
  s->envelopeIncrease = (env >> 3) & 1;
  s->envelopePeriod = env & 7;
  // Only the hardware envelope shapes the sound, like on the Game Boy.
  s->attack = 0;
  s->decay = 0;
  s->sustain = 255;
  s->release = 20;
}

// LSDj panning bits: 0 mute, 1 right, 2 left, 3 both.
static uint8_t convertPan(uint8_t bits) {
  bits &= 3;
  return bits == 1 ? 255 : bits == 2 ? 0 : 128;
}

// LSDj command numbers, as stored in the phrase command column.
enum { CMD_NONE, CMD_A, CMD_C, CMD_D, CMD_E, CMD_F, CMD_G, CMD_H, CMD_K, CMD_L, CMD_M, CMD_O, CMD_P, CMD_R, CMD_S, CMD_T, CMD_V, CMD_W, CMD_Z };

static void setFx(PhraseRow* row, int slot, enum FX fx, uint8_t value) {
  row->fx[slot][0] = (uint8_t)fx;
  row->fx[slot][1] = value;
}

// Commands with a clear equivalent are converted; A, F, H, L, M, O, P, R, S,
// T, W and Z (tables, hop, slide, bend, retrigger, sweep, tempo, wave
// settings...) are dropped. LSDj has one command per row, so up to three
// FX slots are free: E needs all of them.
static void convertCommand(PhraseRow* row, uint8_t cmd, uint8_t value, int channel) {
  switch (cmd) {
    case CMD_C: setFx(row, 0, fxARP, value); break;           // chord
    case CMD_D: setFx(row, 0, fxDEL, value); break;           // delay in ticks
    case CMD_G: setFx(row, 0, fxGGR, value & 0x1F); break;    // groove
    case CMD_K: setFx(row, 0, fxKIL, value); break;           // kill after n ticks
    case CMD_V: setFx(row, 0, fxPVB, value); break;           // vibrato, speed and depth
    case CMD_E:
      // Envelope of pulse and noise (NRx2 layout). On the wave channel the
      // same command sets its volume, which has no equivalent here.
      if (channel == 2) break;
      setFx(row, 0, fxCEI, value >> 4);
      setFx(row, 1, fxCEP, value & 7);
      setFx(row, 2, fxCED, (value >> 3) & 1);
      break;
  }
}

static void convertInstrument(Instrument* inst, const uint8_t* src) {
  switch (src[0]) {
    case LSDJ_PULSE: {
      getInstrumentFunctions(InstrumentType::GBPulse).init(inst);
      InstrumentSimpleChip* s = &inst->chip.simpleChip;
      s->mode = src[7] >> 6; // 12.5%, 25%, 50%, 75%
      convertEnvelope(s, src[1]);
      // The pulse sweep byte is not imported: read as a Game Boy NR10 it
      // detunes the notes.
      s->sweepPeriod = s->sweepShift = s->sweepNegate = 0;
      inst->pan = convertPan(src[7]);
      break;
    }
    case LSDJ_NOISE: {
      getInstrumentFunctions(InstrumentType::GBNoise).init(inst);
      InstrumentSimpleChip* s = &inst->chip.simpleChip;
      s->mode = src[2] & 1; // stable = 7 bit
      convertEnvelope(s, src[1]);
      inst->pan = convertPan(src[7]);
      break;
    }
    case LSDJ_WAVE: { // the wave frames are not converted: Hollow Lead stands in
      getInstrumentFunctions(InstrumentType::GBPulse).init(inst);
      inst->pan = convertPan(src[7]);
      break;
    }
    default: { // kit: samples are not converted
      getInstrumentFunctions(InstrumentType::GBNoise).init(inst);
      inst->pan = convertPan(src[7]);
      break;
    }
  }
}

// file: an .lsdsng stream (name, version, compressed blocks). Not freed here.
static int loadLsdjBuffer(Project* project, const uint8_t* file, size_t size) {
  if (!project || !file) return 1;
  if (size < LSDJ_MIN_FILE_SIZE || size > LSDJ_MAX_FILE_SIZE) {
    snprintf(projectFileError, 40, "Not a valid LSDj song");
    return 1;
  }

  uint8_t* data = (uint8_t*)malloc(LSDJ_SONG_SIZE);
  bool ok = data && lsdjDecompress(file + LSDJ_HEADER_SIZE, size - LSDJ_HEADER_SIZE, data);
  // Both memory markers "rb" must be where LSDj puts them.
  ok = ok && data[LSDJ_MARKER1] == 'r' && data[LSDJ_MARKER1 + 1] == 'b' && data[LSDJ_MARKER2] == 'r' && data[LSDJ_MARKER2 + 1] == 'b';
  if (!ok) {
    free(data);
    snprintf(projectFileError, 40, "Not a valid LSDj song");
    return 1;
  }

  auto owned = std::make_unique<Project>();
  Project& p = *owned;
  projectInitAY(&p);

  // One LSDj row is a 16th note: 6 ticks at T bpm is T / 60 * 4 * 6 ticks per second.
  int bpm = data[LSDJ_TEMPO];
  if (bpm < 40) bpm = 128;
  p.tickRate = (float)(bpm * 0.4);

  copyName(p.title, sizeof(p.title), file, LSDJ_NAME_SIZE);

  for (int row = 0; row < LSDJ_ROWS && row < PROJECT_MAX_LENGTH; row++) {
    for (int t = 0; t < LSDJ_CHANNELS && t < PROJECT_MAX_TRACKS; t++) {
      uint8_t chain = data[LSDJ_SONG_ROWS + row * LSDJ_CHANNELS + t];
      p.song[row][t] = chain < LSDJ_CHAINS ? chain : EMPTY_VALUE_16;
    }
  }

  for (int c = 0; c < LSDJ_CHAINS && c < PROJECT_MAX_CHAINS; c++) {
    chainClear(&p.chains[c]);
    for (int s = 0; s < 16; s++) {
      uint8_t phrase = data[LSDJ_CHAIN_PHRASES + c * 16 + s];
      if (phrase < LSDJ_PHRASES) {
        p.chains[c].rows[s].phrase = phrase;
        p.chains[c].rows[s].transpose = data[LSDJ_CHAIN_TRANSPOSE + c * 16 + s];
      }
    }
  }

  // The first channel a phrase is played on (the wave channel reads E differently)
  int phraseChannel[LSDJ_PHRASES];
  for (int i = 0; i < LSDJ_PHRASES; i++) phraseChannel[i] = -1;
  for (int row = 0; row < LSDJ_ROWS; row++) {
    for (int t = 0; t < LSDJ_CHANNELS; t++) {
      uint8_t chain = data[LSDJ_SONG_ROWS + row * LSDJ_CHANNELS + t];
      if (chain >= LSDJ_CHAINS) continue;
      for (int st = 0; st < 16; st++) {
        uint8_t phrase = data[LSDJ_CHAIN_PHRASES + chain * 16 + st];
        if (phrase < LSDJ_PHRASES && phraseChannel[phrase] < 0) phraseChannel[phrase] = t;
      }
    }
  }

  // Grooves: ticks per step, 0 ends the groove
  for (int g = 0; g < 32 && g < PROJECT_MAX_GROOVES; g++) {
    for (int i = 0; i < 16; i++) {
      uint8_t ticks = data[LSDJ_GROOVES + g * 16 + i];
      p.grooves[g].speed[i] = ticks ? ticks : EMPTY_VALUE_8;
    }
  }

  uint8_t instrumentUsed[LSDJ_INSTRUMENTS] = {0};
  for (int ph = 0; ph < LSDJ_PHRASES && ph < PROJECT_MAX_PHRASES; ph++) {
    phraseClear(&p.phrases[ph]);
    for (int s = 0; s < 16; s++) {
      PhraseRow* dest = &p.phrases[ph].rows[s];
      initEmptyPhraseRow(dest);
      uint8_t command = data[LSDJ_PHRASE_COMMANDS + ph * 16 + s];
      if (command) convertCommand(dest, command, data[LSDJ_PHRASE_COMMAND_VALUES + ph * 16 + s], phraseChannel[ph]);
      uint8_t note = data[LSDJ_PHRASE_NOTES + ph * 16 + s];
      if (note == 0) continue;
      int index = LSDJ_NOTE_INDEX_OFFSET + note;
      if (p.pitchTable.length > 0 && index >= p.pitchTable.length) index = p.pitchTable.length - 1;
      dest->note = (uint8_t)index;
      dest->volume = PHRASE_VOLUME_MAX; // LSDj has no note volume: full level
      uint8_t inst = data[LSDJ_PHRASE_INSTRUMENTS + ph * 16 + s];
      if (inst < LSDJ_INSTRUMENTS && inst < PROJECT_MAX_INSTRUMENTS) {
        dest->instrument = inst;
        instrumentUsed[inst] = 1;
      }
    }
  }

  for (int i = 0; i < LSDJ_INSTRUMENTS && i < PROJECT_MAX_INSTRUMENTS; i++) {
    if (!instrumentUsed[i] && !data[LSDJ_INSTRUMENT_ALLOC + i]) continue;
    Instrument* inst = &p.instruments[i];
    convertInstrument(inst, data + LSDJ_INSTRUMENT_PARAMS + i * LSDJ_INSTRUMENT_SIZE);
    char name[LSDJ_INSTRUMENT_NAME_SIZE + 1];
    copyName(name, sizeof(name), data + LSDJ_INSTRUMENT_NAMES + i * LSDJ_INSTRUMENT_NAME_SIZE, LSDJ_INSTRUMENT_NAME_SIZE);
    if (name[0]) snprintf(inst->name, sizeof(inst->name), "%s", name);
    else snprintf(inst->name, sizeof(inst->name), "LSDJ %02X", i);
  }

  free(data);
  projectFree(project);
  *project = p;
  return 0;
}

int projectLoadLSDJ(Project* project, const char* path) {
  if (!project || !path) return 1;

  FILE* f = fopen(path, "rb");
  if (!f) {
    snprintf(projectFileError, 40, "Cannot open LSDj file");
    return 1;
  }
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (size < LSDJ_MIN_FILE_SIZE || size > LSDJ_MAX_FILE_SIZE) {
    fclose(f);
    snprintf(projectFileError, 40, "Not a valid LSDj song");
    return 1;
  }
  uint8_t* file = (uint8_t*)malloc(size);
  size_t got = file ? fread(file, 1, size, f) : 0;
  fclose(f);
  if (!file || got != (size_t)size) {
    free(file);
    snprintf(projectFileError, 40, "Cannot read LSDj file");
    return 1;
  }
  int result = loadLsdjBuffer(project, file, size);
  free(file);
  return result;
}

// Reads a whole .sav; the caller frees the result.
static uint8_t* readSav(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) {
    snprintf(projectFileError, 40, "Cannot open LSDj save");
    return NULL;
  }
  uint8_t* sav = (uint8_t*)malloc(LSDJ_SAV_SIZE);
  size_t got = sav ? fread(sav, 1, LSDJ_SAV_SIZE, f) : 0;
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  if (!sav || got != LSDJ_SAV_SIZE || size != LSDJ_SAV_SIZE || sav[LSDJ_SAV_MARKER] != 'j' || sav[LSDJ_SAV_MARKER + 1] != 'k') {
    free(sav);
    snprintf(projectFileError, 40, "Not a valid LSDj save");
    return NULL;
  }
  return sav;
}

// The song of a .sav slot as an .lsdsng stream: header, then its blocks.
// Returns the size, or 0 when the slot is empty.
static size_t savSong(const uint8_t* sav, int slot, uint8_t* out) {
  size_t n = lsdjSavProject(sav, slot, out + LSDJ_HEADER_SIZE);
  if (!n) return 0;
  memcpy(out, sav + LSDJ_SAV_NAMES + slot * LSDJ_NAME_SIZE, LSDJ_NAME_SIZE);
  out[LSDJ_NAME_SIZE] = sav[LSDJ_SAV_VERSIONS + slot];
  return LSDJ_HEADER_SIZE + n;
}

int lsdjListSav(const char* savPath, LsdjSavSong* songs, int maxSongs, int* count) {
  if (count) *count = 0;
  if (!savPath || !songs) return 1;
  uint8_t* sav = readSav(savPath);
  if (!sav) return 1;
  uint8_t* buffer = (uint8_t*)malloc(LSDJ_HEADER_SIZE + LSDJ_MAX_BLOCKS * LSDJ_BLOCK_SIZE);
  int n = 0;
  for (int i = 0; buffer && i < LSDJ_SAV_PROJECTS && n < maxSongs; i++) {
    size_t size = savSong(sav, i, buffer);
    if (!size) continue;
    LsdjSavSong* s = &songs[n++];
    s->slot = i;
    s->version = buffer[LSDJ_NAME_SIZE];
    s->blocks = (int)((size - LSDJ_HEADER_SIZE) / LSDJ_BLOCK_SIZE);
    copyName(s->name, sizeof(s->name), buffer, LSDJ_NAME_SIZE);
  }
  free(buffer);
  free(sav);
  if (count) *count = n;
  if (!n) {
    snprintf(projectFileError, 40, "No song found in the save");
    return 1;
  }
  return 0;
}

int projectLoadLSDJSav(Project* project, const char* savPath, int slot) {
  if (!project || !savPath || slot < 0 || slot >= LSDJ_SAV_PROJECTS) return 1;
  uint8_t* sav = readSav(savPath);
  if (!sav) return 1;
  uint8_t* buffer = (uint8_t*)malloc(LSDJ_HEADER_SIZE + LSDJ_MAX_BLOCKS * LSDJ_BLOCK_SIZE);
  size_t size = buffer ? savSong(sav, slot, buffer) : 0;
  int result = 1;
  if (size) result = loadLsdjBuffer(project, buffer, size);
  else snprintf(projectFileError, 40, "No song in this slot");
  free(buffer);
  free(sav);
  return result;
}

// A file name made of the song name and version, as LSDj tools do
static void savFileName(char* dest, size_t size, const uint8_t* name, uint8_t version) {
  char clean[LSDJ_NAME_SIZE + 1];
  size_t n = 0;
  for (int i = 0; i < LSDJ_NAME_SIZE && name[i]; i++) {
    char c = (char)name[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_') clean[n++] = c;
  }
  clean[n] = 0;
  snprintf(dest, size, "%s_v%02X.lsdsng", n ? clean : "SONG", version);
}

int lsdjExtractSav(const char* savPath, const char* outDir, int* count) {
  if (count) *count = 0;
  if (!savPath || !outDir) return 1;
  FILE* f = fopen(savPath, "rb");
  if (!f) {
    snprintf(projectFileError, 40, "Cannot open LSDj save");
    return 1;
  }
  uint8_t* sav = (uint8_t*)malloc(LSDJ_SAV_SIZE);
  size_t got = sav ? fread(sav, 1, LSDJ_SAV_SIZE, f) : 0;
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  if (!sav || got != LSDJ_SAV_SIZE || size != LSDJ_SAV_SIZE || sav[LSDJ_SAV_MARKER] != 'j' || sav[LSDJ_SAV_MARKER + 1] != 'k') {
    free(sav);
    snprintf(projectFileError, 40, "Not a valid LSDj save");
    return 1;
  }
  if (!fileDirectoryExists(outDir) && fileCreateDirectory(outDir) != 0) {
    free(sav);
    snprintf(projectFileError, 40, "Cannot create folder");
    return 1;
  }

  uint8_t* packed = (uint8_t*)malloc(LSDJ_MAX_BLOCKS * LSDJ_BLOCK_SIZE);
  int written = 0;
  for (int i = 0; packed && i < LSDJ_SAV_PROJECTS; i++) {
    size_t n = lsdjSavProject(sav, i, packed);
    if (!n) continue;
    uint8_t header[LSDJ_HEADER_SIZE];
    memcpy(header, sav + LSDJ_SAV_NAMES + i * LSDJ_NAME_SIZE, LSDJ_NAME_SIZE);
    header[LSDJ_NAME_SIZE] = sav[LSDJ_SAV_VERSIONS + i];
    char fileName[64], path[2048];
    savFileName(fileName, sizeof(fileName), header, header[LSDJ_NAME_SIZE]);
    snprintf(path, sizeof(path), "%s%s%s", outDir, PATH_SEPARATOR_STR, fileName);
    FILE* out = fopen(path, "wb");
    if (!out) continue;
    size_t w = fwrite(header, 1, sizeof(header), out) + fwrite(packed, 1, n, out);
    if (fclose(out) == 0 && w == sizeof(header) + n) written++;
  }
  free(packed);
  free(sav);
  if (count) *count = written;
  if (!written) {
    snprintf(projectFileError, 40, "No song found in the save");
    return 1;
  }
  return 0;
}
