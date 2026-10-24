#ifndef __CHIPNOMAD_LIB__LSDJ_FORMAT_H__
#define __CHIPNOMAD_LIB__LSDJ_FORMAT_H__

// Little Sound Dj song format (.lsdsng), shared by the importer and the
// exporter. Layout of the decompressed 32 KB song as documented by liblsdj
// (MIT, https://github.com/stijnfrishert/liblsdj).

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define LSDJ_SONG_SIZE 0x8000
#define LSDJ_BLOCK_SIZE 0x200
#define LSDJ_MAX_BLOCKS 192
#define LSDJ_HEADER_SIZE 9 // 8 bytes of name, 1 byte of format version
#define LSDJ_NAME_SIZE 8
#define LSDJ_MIN_FILE_SIZE (LSDJ_HEADER_SIZE + LSDJ_BLOCK_SIZE)
#define LSDJ_MAX_FILE_SIZE (LSDJ_HEADER_SIZE + LSDJ_MAX_BLOCKS * LSDJ_BLOCK_SIZE)

#define LSDJ_PHRASE_NOTES 0x0000       // 255 phrases x 16 bytes, 0 = empty
#define LSDJ_INSTRUMENT_NAMES 0x1E7A   // 64 x 5 bytes
#define LSDJ_GROOVES 0x1090            // 32 grooves x 16 bytes of ticks, 0 ends
#define LSDJ_SONG_ROWS 0x1290          // 256 rows x 4 channels, 0xFF = empty
#define LSDJ_INSTRUMENT_ALLOC 0x2040   // 64 flags
#define LSDJ_CHAIN_PHRASES 0x2080      // 128 chains x 16 bytes, 0xFF = empty
#define LSDJ_CHAIN_TRANSPOSE 0x2880    // 128 chains x 16 bytes, signed
#define LSDJ_INSTRUMENT_PARAMS 0x3080  // 64 x 16 bytes
#define LSDJ_PHRASE_ALLOC 0x3E82       // 255 bits
#define LSDJ_CHAIN_ALLOC 0x3EA2        // 128 bits
#define LSDJ_TEMPO 0x3FB4
#define LSDJ_PHRASE_COMMANDS 0x4000    // 255 phrases x 16 bytes
#define LSDJ_PHRASE_COMMAND_VALUES 0x4FF0
#define LSDJ_PHRASE_INSTRUMENTS 0x7000 // 255 phrases x 16 bytes, 0xFF = none
#define LSDJ_MARKER1 0x1E78            // "rb"
#define LSDJ_MARKER2 0x3E80
#define LSDJ_MARKER3 0x7FF0

#define LSDJ_PHRASES 255
#define LSDJ_CHAINS 128
#define LSDJ_INSTRUMENTS 64
#define LSDJ_CHANNELS 4
#define LSDJ_ROWS 256
#define LSDJ_INSTRUMENT_SIZE 16
#define LSDJ_INSTRUMENT_NAME_SIZE 5

enum { LSDJ_PULSE = 0, LSDJ_WAVE = 1, LSDJ_KIT = 2, LSDJ_NOISE = 3 };

// LSDj note 1 is its lowest note, C-3, about 65.4 Hz (MIDI 36). Pitch table
// index N is MIDI note 12+N, as in import_midi.cpp.
#define LSDJ_NOTE_INDEX_OFFSET 23

static const uint8_t LSDJ_DEFAULT_WAVE[16] = {0x8E, 0xCD, 0xCC, 0xBB, 0xAA, 0xA9, 0x99, 0x88, 0x87, 0x76, 0x66, 0x55, 0x54, 0x43, 0x32, 0x31};
// Stamped by the compressor at any offset, so it is not aligned on a record:
// a pulse record is the type byte 0 followed by the first 15 bytes.
static const uint8_t LSDJ_DEFAULT_INSTRUMENT[16] = {0xA8, 0, 0, 0xFF, 0, 0, 3, 0, 0, 0xD0, 0, 0, 0, 0xF3, 0, 0};

// Decompression. 0xC0 is a run (C0 value count) or a literal C0 (C0 C0);
// 0xE0 is a special code: E0 E0 literal E0, E0 FF end of data, E0 F0 n and
// E0 F1 n repeat the default wave or instrument (16 bytes) n times, any other
// E0 n jumps to the start of the next 512 byte block. Returns true when
// exactly LSDJ_SONG_SIZE bytes came out.
static inline bool lsdjDecompress(const uint8_t* in, size_t size, uint8_t* out) {
  size_t i = 0, n = 0;
  while (i < size) {
    uint8_t b = in[i++];
    if (b == 0xC0) {
      if (i >= size) return false;
      uint8_t v = in[i++];
      if (v == 0xC0) {
        if (n >= LSDJ_SONG_SIZE) return false;
        out[n++] = 0xC0;
      } else {
        if (i >= size) return false;
        size_t count = in[i++];
        if (n + count > LSDJ_SONG_SIZE) return false;
        memset(out + n, v, count);
        n += count;
      }
    } else if (b == 0xE0) {
      if (i >= size) return false;
      uint8_t v = in[i++];
      if (v == 0xE0) {
        if (n >= LSDJ_SONG_SIZE) return false;
        out[n++] = 0xE0;
      } else if (v == 0xFF) {
        return n == LSDJ_SONG_SIZE;
      } else if (v == 0xF0 || v == 0xF1) {
        if (i >= size) return false;
        size_t count = in[i++];
        if (n + count * 16 > LSDJ_SONG_SIZE) return false;
        for (size_t k = 0; k < count; k++) memcpy(out + n + k * 16, v == 0xF0 ? LSDJ_DEFAULT_WAVE : LSDJ_DEFAULT_INSTRUMENT, 16);
        n += count * 16;
      } else {
        i = (i + LSDJ_BLOCK_SIZE - 1) / LSDJ_BLOCK_SIZE * LSDJ_BLOCK_SIZE;
      }
    } else {
      if (n >= LSDJ_SONG_SIZE) return false;
      out[n++] = b;
    }
  }
  return false;
}

// Compression into 512 byte blocks, each ending with a jump to the next block
// (E0 id, ids being 1-based block numbers, so the first jump is E0 02) and the last one with E0 FF. out must hold
// LSDJ_MAX_BLOCKS * LSDJ_BLOCK_SIZE bytes. Returns the compressed size in
// bytes (a multiple of the block size), or 0 when the song does not fit.
static inline size_t lsdjCompress(const uint8_t* song, uint8_t* out) {
  size_t o = 0, blockStart = 0, i = 0;
  unsigned block = 1; // ids are 1-based block numbers
  const size_t limit = LSDJ_MAX_BLOCKS * LSDJ_BLOCK_SIZE;
  while (i < LSDJ_SONG_SIZE) {
    // Room for the longest code (3 bytes) plus the 2 byte jump or end code
    if (o - blockStart > LSDJ_BLOCK_SIZE - 5) {
      if (o + LSDJ_BLOCK_SIZE > limit) return 0;
      out[o++] = 0xE0;
      out[o++] = (uint8_t)(++block);
      while (o % LSDJ_BLOCK_SIZE) out[o++] = 0xFF;
      blockStart = o;
    }
    size_t run = 1;
    while (i + run < LSDJ_SONG_SIZE && song[i + run] == song[i] && run < 255) run++;
    if (run >= 4) {
      out[o++] = 0xC0;
      out[o++] = song[i];
      out[o++] = (uint8_t)run;
      i += run;
    } else {
      if (song[i] == 0xC0) out[o++] = 0xC0;
      if (song[i] == 0xE0) out[o++] = 0xE0;
      out[o++] = song[i];
      i++;
    }
  }
  out[o++] = 0xE0;
  out[o++] = 0xFF;
  while (o % LSDJ_BLOCK_SIZE) out[o++] = 0;
  return o;
}

// .sav file: 128 KB. The working song comes first (0x0000, uncompressed), then
// the header at 0x8000 (32 names of 8 bytes, 32 versions, "jk" marker, active
// project, then one owner byte per block, 0xFF = free), then 191 blocks of 512
// bytes from 0x8200. A project is its blocks chained by the jump ids.
#define LSDJ_SAV_SIZE 0x20000
#define LSDJ_SAV_PROJECTS 32
#define LSDJ_SAV_NAMES 0x8000
#define LSDJ_SAV_VERSIONS 0x8100
#define LSDJ_SAV_MARKER 0x813E
#define LSDJ_SAV_BLOCK_TABLE 0x8141
#define LSDJ_SAV_BLOCKS 0x8200
#define LSDJ_SAV_BLOCK_COUNT 191

// Id (1-based block number) of the block that follows this compressed block, or -1 at the end of the
// song (or when the block is malformed).
static inline int lsdjBlockNext(const uint8_t* block) {
  size_t i = 0;
  while (i < LSDJ_BLOCK_SIZE) {
    uint8_t b = block[i++];
    if (b == 0xC0) {
      if (i >= LSDJ_BLOCK_SIZE) return -1;
      if (block[i++] != 0xC0) i++;
    } else if (b == 0xE0) {
      if (i >= LSDJ_BLOCK_SIZE) return -1;
      uint8_t v = block[i++];
      if (v == 0xFF) return -1;
      if (v == 0xF0 || v == 0xF1) i++;
      else if (v != 0xE0) return v;
    }
  }
  return -1;
}

// Gathers the blocks of one project of a .sav into an .lsdsng stream, in
// order. out must hold LSDJ_MAX_BLOCKS * LSDJ_BLOCK_SIZE bytes. Returns the
// size in bytes, or 0 when the slot is empty or its chain is broken.
static inline size_t lsdjSavProject(const uint8_t* sav, int project, uint8_t* out) {
  const uint8_t* table = sav + LSDJ_SAV_BLOCK_TABLE;
  int block = -1;
  for (int i = 0; i < LSDJ_SAV_BLOCK_COUNT; i++) {
    if (table[i] == project) { block = i; break; }
  }
  if (block < 0) return 0;
  size_t n = 0;
  for (int count = 0; count < LSDJ_MAX_BLOCKS; count++) {
    const uint8_t* data = sav + LSDJ_SAV_BLOCKS + (size_t)block * LSDJ_BLOCK_SIZE;
    memcpy(out + n, data, LSDJ_BLOCK_SIZE);
    n += LSDJ_BLOCK_SIZE;
    int id = lsdjBlockNext(data);
    if (id < 0) return n;
    int next = id - 1; // ids are 1-based block numbers
    if (next < 0 || next >= LSDJ_SAV_BLOCK_COUNT || table[next] != project) return 0;
    block = next;
  }
  return 0;
}

#endif // __CHIPNOMAD_LIB__LSDJ_FORMAT_H__
