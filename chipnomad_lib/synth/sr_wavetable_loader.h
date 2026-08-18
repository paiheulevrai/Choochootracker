#ifndef CHOOCHOO_SR_WAVETABLE_LOADER_H
#define CHOOCHOO_SR_WAVETABLE_LOADER_H

#include <stddef.h>
#include <stdint.h>

struct InstrumentSample;

// Loads mono PCM WAV data and determines its concatenated Serum/WaveEdit layout.
// Serum's optional `clm ` chunk wins; WaveEdit's exact 64 x 256 layout and
// conventional 2048-sample Serum frames are recognized without metadata.
int srWavetableLoadWav(const char* path, InstrumentSample* table,
                     uint16_t* frameSize, uint16_t* frameCount,
                     char* error, size_t errorSize);

#endif
