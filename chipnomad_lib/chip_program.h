#pragma once
#include <cstdio>

#include "project_instruments.h"

bool validChipProgram(const ChipProgram& p);
// 0 = unrelated; 1 = consumed; -1 = invalid. Rows must arrive in order.
int loadChipProgramLine(const char* line, ChipProgram& p, bool& seen);
void saveChipProgram(FILE* file, const ChipProgram& p);

// Allocation-free sequence cursor, advanced at the source tick rate. The owned
// patch stays in the voice; offsets never reference a file or browser buffer.
class ChipMacroPlayer {
 public:
  void reset(const ChipProgram& p);
  void release(const ChipProgram& p);
  void tick(const ChipProgram& p);
  bool has(unsigned id) const { return id < 16 && ready_[id]; }
  int value(unsigned id, int fallback = 0) const { return has(id) ? values_[id] : fallback; }
  bool releaseTail(const ChipProgram& p, unsigned id) const {
    return id < 16 && present_[id] && p.data[lanes_[id].offset + 3] != 255;
  }
  bool done(unsigned id) const { return id >= 16 || !present_[id] || lanes_[id].done; }

 private:
  struct Lane {
    uint16_t offset;
    uint8_t pos, wait;
    bool done;
  };
  Lane lanes_[16]{};
  int values_[16]{};
  bool present_[16]{}, ready_[16]{}, released_ = false;
};

class GoatProgramPlayer {
 public:
  void reset(const ChipProgram& p);
  void tick(const ChipProgram& p);
  int control = 0x21, ad = 0, sr = 0, pulse = 2048, cutoff = 0, filter = 0, resonance = 0;
  int note = 0;
  bool absolute = false;

 private:
  uint16_t offsets_[4]{};
  uint8_t lengths_[4]{}, positions_[4]{}, waits_[3]{};
};
