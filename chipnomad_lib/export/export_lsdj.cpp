#include "export_lsdj.h"
#include "../lsdj_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char projectExportLSDJError[48];

static int fail(const char* message) {
  snprintf(projectExportLSDJError, sizeof(projectExportLSDJError), "%s", message);
  return 1;
}

static void setBit(uint8_t* bits, int index) {
  bits[index / 8] |= (uint8_t)(1 << (index % 8));
}

// Inverse of the importer: record bytes are type, envelope (Game Boy NRx2),
// noise stability in byte 2, duty in the top bits of byte 7 and panning in
// its low bits. The rest keeps the default instrument values.
static void writeInstrument(uint8_t* rec, const Instrument* inst) {
  rec[0] = 0;
  memcpy(rec + 1, LSDJ_DEFAULT_INSTRUMENT, LSDJ_INSTRUMENT_SIZE - 1);
  uint8_t pan = inst->pan < 64 ? 2 : inst->pan > 192 ? 1 : 3;
  rec[7] = pan;
  if (inst->type == InstrumentType::GBPulse || inst->type == InstrumentType::GBNoise) {
    const InstrumentSimpleChip* s = &inst->chip.simpleChip;
    rec[1] = (uint8_t)((s->envelopeInitial << 4) | (s->envelopeIncrease << 3) | s->envelopePeriod);
    if (inst->type == InstrumentType::GBNoise) {
      rec[0] = LSDJ_NOISE;
      rec[2] = s->mode & 1;
    } else {
      rec[7] |= (uint8_t)((s->mode & 3) << 6);
    }
  }
}

static void writeName(uint8_t* dest, const char* name) {
  memset(dest, 0, LSDJ_INSTRUMENT_NAME_SIZE);
  int n = 0;
  for (const char* c = name; *c && n < LSDJ_INSTRUMENT_NAME_SIZE; c++) {
    char u = (*c >= 'a' && *c <= 'z') ? (char)(*c - 32) : *c;
    if ((u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || u == ' ') dest[n++] = (uint8_t)u;
  }
}

int projectExportLSDJ(Project* project, const char* templatePath, const char* outputPath) {
  if (!project || !templatePath || !outputPath) return fail("Invalid arguments");

  FILE* f = fopen(templatePath, "rb");
  if (!f) return fail("Cannot open LSDj template");
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (size < LSDJ_MIN_FILE_SIZE || size > LSDJ_MAX_FILE_SIZE) {
    fclose(f);
    return fail("Template is not a valid LSDj song");
  }
  uint8_t* file = (uint8_t*)malloc(size);
  size_t got = file ? fread(file, 1, size, f) : 0;
  fclose(f);
  if (!file || got != (size_t)size) {
    free(file);
    return fail("Cannot read LSDj template");
  }
  uint8_t* data = (uint8_t*)malloc(LSDJ_SONG_SIZE);
  bool ok = data && lsdjDecompress(file + LSDJ_HEADER_SIZE, size - LSDJ_HEADER_SIZE, data) &&
            data[LSDJ_MARKER1] == 'r' && data[LSDJ_MARKER1 + 1] == 'b' &&
            data[LSDJ_MARKER2] == 'r' && data[LSDJ_MARKER2 + 1] == 'b';
  if (!ok) {
    free(data);
    free(file);
    return fail("Template is not a valid LSDj song");
  }

  // Chains and phrases LSDj cannot hold: fail rather than silently drop notes.
  for (int row = 0; row < PROJECT_MAX_LENGTH; row++) {
    for (int t = 0; t < LSDJ_CHANNELS; t++) {
      uint16_t chain = project->song[row][t];
      if (chain == EMPTY_VALUE_16 || chain >= PROJECT_MAX_CHAINS) continue;
      if (chain >= LSDJ_CHAINS) {
        free(data);
        free(file);
        return fail("Too many chains for LSDj (max 128)");
      }
      for (int s = 0; s < 16; s++) {
        uint16_t phrase = project->chains[chain].rows[s].phrase;
        if (phrase != EMPTY_VALUE_16 && phrase >= LSDJ_PHRASES) {
          free(data);
          free(file);
          return fail("Too many phrases for LSDj (max 255)");
        }
      }
    }
  }

  // Tempo: the inverse of the importer (0.4 ticks per second per bpm)
  float bpm = project->tickRate / 0.4f;
  if (!(bpm == bpm)) bpm = 128.0f;
  if (bpm < 40.0f) bpm = 40.0f;
  if (bpm > 255.0f) bpm = 255.0f;
  data[LSDJ_TEMPO] = (uint8_t)(bpm + 0.5f);

  // Song rows
  uint8_t chainUsed[LSDJ_CHAINS] = {0};
  for (int row = 0; row < LSDJ_ROWS; row++) {
    for (int t = 0; t < LSDJ_CHANNELS; t++) {
      uint16_t chain = (row < PROJECT_MAX_LENGTH && t < PROJECT_MAX_TRACKS) ? project->song[row][t] : EMPTY_VALUE_16;
      uint8_t v = chain < LSDJ_CHAINS ? (uint8_t)chain : 0xFF;
      data[LSDJ_SONG_ROWS + row * LSDJ_CHANNELS + t] = v;
      if (v != 0xFF) chainUsed[v] = 1;
    }
  }

  // Chains
  uint8_t phraseUsed[LSDJ_PHRASES] = {0};
  memset(data + LSDJ_CHAIN_ALLOC, 0, 16);
  for (int c = 0; c < LSDJ_CHAINS; c++) {
    for (int s = 0; s < 16; s++) {
      uint16_t phrase = c < PROJECT_MAX_CHAINS ? project->chains[c].rows[s].phrase : EMPTY_VALUE_16;
      uint8_t* ph = data + LSDJ_CHAIN_PHRASES + c * 16 + s;
      uint8_t* tr = data + LSDJ_CHAIN_TRANSPOSE + c * 16 + s;
      if (phrase != EMPTY_VALUE_16 && phrase < LSDJ_PHRASES) {
        *ph = (uint8_t)phrase;
        *tr = project->chains[c].rows[s].transpose;
        if (chainUsed[c]) phraseUsed[phrase] = 1;
      } else {
        *ph = 0xFF;
        *tr = 0;
      }
    }
    if (chainUsed[c]) setBit(data + LSDJ_CHAIN_ALLOC, c);
  }

  // Phrases. The importer's inverse: LSDj note = pitch table index - 23.
  uint8_t instrumentUsed[LSDJ_INSTRUMENTS] = {0};
  memset(data + LSDJ_PHRASE_ALLOC, 0, 32);
  for (int ph = 0; ph < LSDJ_PHRASES; ph++) {
    for (int s = 0; s < 16; s++) {
      uint8_t* note = data + LSDJ_PHRASE_NOTES + ph * 16 + s;
      uint8_t* inst = data + LSDJ_PHRASE_INSTRUMENTS + ph * 16 + s;
      data[LSDJ_PHRASE_COMMANDS + ph * 16 + s] = 0;
      data[LSDJ_PHRASE_COMMAND_VALUES + ph * 16 + s] = 0;
      *note = 0;
      *inst = 0xFF;
      if (ph >= PROJECT_MAX_PHRASES) continue;
      const PhraseRow* row = &project->phrases[ph].rows[s];
      if (row->note >= PROJECT_MAX_PITCHES) continue;
      int n = row->note - LSDJ_NOTE_INDEX_OFFSET;
      *note = (uint8_t)(n < 1 ? 1 : n > 0x7F ? 0x7F : n);
      if (row->instrument != EMPTY_VALUE_8 && row->instrument < LSDJ_INSTRUMENTS) {
        *inst = row->instrument;
        if (phraseUsed[ph]) instrumentUsed[row->instrument] = 1;
      }
    }
    if (phraseUsed[ph]) setBit(data + LSDJ_PHRASE_ALLOC, ph);
  }

  // Instruments used by an exported phrase
  for (int i = 0; i < LSDJ_INSTRUMENTS; i++) {
    if (!instrumentUsed[i]) continue;
    const Instrument* inst = &project->instruments[i];
    writeInstrument(data + LSDJ_INSTRUMENT_PARAMS + i * LSDJ_INSTRUMENT_SIZE, inst);
    writeName(data + LSDJ_INSTRUMENT_NAMES + i * LSDJ_INSTRUMENT_NAME_SIZE, inst->name);
    data[LSDJ_INSTRUMENT_ALLOC + i] = 1;
  }

  uint8_t* packed = (uint8_t*)malloc(LSDJ_MAX_BLOCKS * LSDJ_BLOCK_SIZE);
  size_t packedSize = packed ? lsdjCompress(data, packed) : 0;
  if (!packedSize) {
    free(packed);
    free(data);
    free(file);
    return fail("Song too big for an .lsdsng");
  }

  // Header: the title (8 chars) and the template's format version
  uint8_t header[LSDJ_HEADER_SIZE];
  memset(header, 0, sizeof(header));
  for (int i = 0, n = 0; project->title[i] && n < LSDJ_NAME_SIZE; i++) {
    char c = project->title[i];
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) header[n++] = (uint8_t)c;
  }
  header[8] = file[8];

  FILE* out = fopen(outputPath, "wb");
  if (!out) {
    free(packed);
    free(data);
    free(file);
    return fail("Cannot write LSDj file");
  }
  size_t written = fwrite(header, 1, sizeof(header), out) + fwrite(packed, 1, packedSize, out);
  int closeResult = fclose(out);
  free(packed);
  free(data);
  free(file);
  if (written != sizeof(header) + packedSize || closeResult != 0) return fail("Cannot write LSDj file");
  return 0;
}
