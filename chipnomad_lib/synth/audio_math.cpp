#include "audio_math.h"

#include <math.h>

float audioTanhTable[1025];

namespace {
struct AudioTanhTableInitializer {
  AudioTanhTableInitializer() {
    for (int i = 0; i <= 1024; ++i)
      audioTanhTable[i] = tanhf(i / 64.0f - 8.0f);
  }
} audioTanhTableInitializer;
}  // namespace
