#ifndef CHIPNOMAD_PLAYBACK_BLEND_H
#define CHIPNOMAD_PLAYBACK_BLEND_H

extern "C++" {
#include "project.h"
#include <vector>

struct BlendSegment {
  uint32_t begin;
  uint32_t end;
  uint16_t rows;
  uint8_t target;
};

struct PreparedBlends {
  std::vector<BlendSegment> segments;
  void prepare(const Project& project);
  const BlendSegment* find(uint32_t begin) const;
};

struct PlaybackBlendState {
  uint32_t begin;
  uint32_t end;
  uint32_t rows;
  uint32_t elapsed;
  int32_t start;
  int32_t current;
  uint32_t rebase;
  uint8_t curve;
  uint8_t target;
  uint8_t active;
};

enum class BlendError : uint8_t { none, destination, endpoint };

uint32_t blendPhraseKey(int track, int column, int songRow, int chainRow, int row);
uint32_t blendTableKey(int table, int column, int row);
int blendCurve(int progress, uint8_t curve);
bool blendDestination(FXId command);
int blendValue(FXId command, uint8_t value);

}
#endif
