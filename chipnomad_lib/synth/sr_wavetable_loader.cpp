#include "sr_wavetable_loader.h"
#include "sample_voice.h"
#include "../project_instruments.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t serumFrameSize(const char* path) {
  FILE* file = fopen(path, "rb");
  if (!file) return 0;
  char id[4];
  uint32_t size;
  uint16_t result = 0;
  fseek(file, 12, SEEK_SET); // RIFF header and WAVE form type.
  while (fread(id, 1, 4, file) == 4 && fread(&size, 4, 1, file) == 1) {
    if (!memcmp(id, "clm ", 4)) {
      char text[32] = {};
      size_t count = size < sizeof(text) - 1 ? size : sizeof(text) - 1;
      fread(text, 1, count, file);
      for (char* p = text; *p; ++p) if (isdigit((unsigned char)*p)) {
        result = (uint16_t)strtoul(p, NULL, 10);
        break;
      }
      break;
    }
    fseek(file, size + (size & 1), SEEK_CUR);
  }
  fclose(file);
  return result;
}

int srWavetableLoadWav(const char* path, InstrumentSample* table,
                     uint16_t* frameSize, uint16_t* frameCount,
                     char* error, size_t errorSize) {
  if (sampleLoadWav16(path, table, error, errorSize)) return 1;
  if (table->channels != 1) { snprintf(error, errorSize, "Wavetable needs mono WAV"); return 1; }
  uint16_t size = serumFrameSize(path);
  if (!size && table->frameCount == 64 * 256) size = 256;
  if (!size && table->frameCount % 2048 == 0) size = 2048;
  if (!size || table->frameCount % size != 0) {
    snprintf(error, errorSize, "Unknown wavetable frame size");
    return 1;
  }
  uint32_t count = table->frameCount / size;
  if (count == 0 || count > 256) { snprintf(error, errorSize, "Wavetable needs 1-256 frames"); return 1; }
  *frameSize = size;
  *frameCount = (uint16_t)count;
  return 0;
}
