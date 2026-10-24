#include "chip_program.h"

#include <cstring>

namespace {
int number(const uint8_t* p) {
  return int32_t(uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
                 (uint32_t(p[3]) << 24));
}
int hex(char c) {
  return c >= '0' && c <= '9'   ? c - '0'
         : c >= 'a' && c <= 'f' ? c - 'a' + 10
         : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                : -1;
}
}  // namespace
bool validChipProgram(const ChipProgram& p) {
  if (!p.format) return !p.size;
  if (p.size > sizeof(p.data) || !p.size || p.rate < 1 || p.rate > 200) return false;
  if (p.format == 2) {
    if (p.size < 33 || memcmp(p.data, "GTI5", 4)) return false;
    size_t pos = 29;
    unsigned lengths[4]{};
    for (int t = 0; t < 4; ++t) {
      if (pos >= p.size) return false;
      lengths[t] = p.data[pos++];
      if (pos + 2 * lengths[t] > p.size) return false;
      pos += 2 * lengths[t];
    }
    if (pos != p.size) return false;
    for (int t = 0; t < 4; ++t)
      if (p.data[6 + t] > lengths[t]) return false;
    return true;
  }
  if (p.format != 1) return false;
  unsigned seen = 0;
  size_t pos = 0;
  while (pos < p.size) {
    if (pos + 8 > p.size) return false;
    const auto* m = p.data + pos;
    unsigned id = m[0], len = m[1];
    if (id >= 16 || !len || (seen & (1u << id)) || m[6] == 0 || m[7] || (m[4] & ~2)) return false;
    if ((m[2] != 255 && m[2] >= len) || (m[3] != 255 && m[3] >= len)) return false;
    seen |= 1u << id;
    pos += 8 + 4 * len;
    if (pos > p.size) return false;
    if (id != 0 && id != 1 && id != 2 && id != 3 && id != 5 && id != 15) return false;
    for (unsigned n = 0; n < len; ++n) {
      int v = number(m + 8 + 4 * n);
      if (id == 1) {
        if ((uint32_t(v) & 0xc0000000) == 0x40000000) v ^= 0x40000000;
        if (v < -120 || v > 180) return false;
      } else if (v < 0 || v > (id == 0 ? 15 : id == 2 ? 4095 : id == 3 ? 8 : id == 5 ? 2047 : 1))
        return false;
    }
  }
  return pos == p.size;
}
int loadChipProgramLine(const char* line, ChipProgram& p, bool& seen) {
  if (!strncmp(line, "- Source program: ", 18)) {
    int format, rate;
    char tail;
    if (seen || sscanf(line + 18, "%d,%d %c", &format, &rate, &tail) != 2 || format < 1 ||
        format > 2 || rate < 1 || rate > 200)
      return -1;
    p = {};
    p.format = format;
    p.rate = rate;
    seen = true;
    return 1;
  }
  if (strncmp(line, "- Source bytes: ", 16)) return 0;
  if (!seen) return -1;
  size_t len = strlen(line + 16);
  if (!len || len % 2 || len > 128 || p.size + len / 2 > sizeof(p.data)) return -1;
  for (size_t n = 0; n < len; n += 2) {
    int a = hex(line[16 + n]), b = hex(line[17 + n]);
    if (a < 0 || b < 0) return -1;
    p.data[p.size++] = (a << 4) | b;
  }
  return 1;
}
void saveChipProgram(FILE* f, const ChipProgram& p) {
  if (!p.format) return;
  fprintf(f, "- Source program: %u,%u\n", p.format, p.rate);
  for (unsigned i = 0; i < p.size; i += 64) {
    fputs("- Source bytes: ", f);
    for (unsigned j = i; j < p.size && j < i + 64; ++j) fprintf(f, "%02x", p.data[j]);
    fputc('\n', f);
  }
}
void ChipMacroPlayer::reset(const ChipProgram& p) {
  *this = {};
  if (p.format != 1) return;
  for (unsigned pos = 0; pos < p.size;) {
    const auto* m = p.data + pos;
    lanes_[m[0]] = {uint16_t(pos), 0, m[5], false};
    present_[m[0]] = true;
    pos += 8 + 4 * m[1];
  }
  tick(p);
}
void ChipMacroPlayer::release(const ChipProgram& p) {
  released_ = true;
  for (unsigned id = 0; id < 16; ++id)
    if (present_[id]) {
      auto& l = lanes_[id];
      const auto* m = p.data + l.offset;
      if (m[3] != 255 && (l.pos <= m[3] || (m[4] & 2))) {
        l.pos = m[3] + 1;
        l.wait = 0;
        l.done = l.pos >= m[1];
      }
    }
}
void ChipMacroPlayer::tick(const ChipProgram& p) {
  if (p.format != 1) return;
  for (unsigned id = 0; id < 16; ++id)
    if (present_[id]) {
      auto& l = lanes_[id];
      const auto* m = p.data + l.offset;
      if (l.done) continue;
      if (l.wait) {
        --l.wait;
        continue;
      }
      values_[id] = number(m + 8 + 4 * l.pos);
      ready_[id] = true;
      l.wait = m[6] - 1;
      unsigned next = unsigned(l.pos) + 1;
      if (!released_ && m[3] != 255 && next > m[3])
        next = (m[2] != 255 && m[2] <= m[3]) ? m[2] : m[3];
      else if (next >= m[1]) {
        if (m[2] != 255 && (m[3] == 255 || m[2] > m[3]))
          next = m[2];
        else {
          l.done = true;
          continue;
        }
      }
      l.pos = next;
    }
}

void GoatProgramPlayer::reset(const ChipProgram& p) {
  *this = {};
  if (p.format != 2) return;
  ad = p.data[4];
  sr = p.data[5];
  control = p.data[12];
  pulse = 2048;
  unsigned pos = 29;
  for (int n = 0; n < 4; ++n) {
    lengths_[n] = p.data[pos++];
    offsets_[n] = pos;
    positions_[n] = p.data[6 + n];
    pos += 2 * lengths_[n];
  }
}
void GoatProgramPlayer::tick(const ChipProgram& p) {
  if (p.format != 2) return;
  auto row = [&](int t, int& a, int& b) {
    // Bounded jump traversal also handles malicious cycles in a saved CNI.
    for (int budget = 0; budget < 256; ++budget) {
      unsigned pos = positions_[t];
      if (!pos || pos > lengths_[t]) return false;
      a = p.data[offsets_[t] + pos - 1];
      b = p.data[offsets_[t] + lengths_[t] + pos - 1];
      if (a != 255) return true;
      positions_[t] = b;
    }
    positions_[t] = 0;
    return false;
  };
  int a, b;
  if (row(0, a, b)) {
    bool advance = true;
    if (a >= 1 && a <= 15) {
      if (waits_[0] < a) {
        ++waits_[0];
        advance = false;
      } else
        waits_[0] = 0;
    }
    if (advance) {
      if (a == 0xf5)
        ad = b;
      else if (a == 0xf6)
        sr = b;
      else if (a < 0xf0) {
        if (a >= 0x10) control = a >= 0xe0 ? a & 15 : a;
        if (b != 0x80) {
          absolute = b >= 0x81;
          note = absolute ? b - 128 + 12 : b >= 0x60 ? b - 128 : b;
        }
      }
      ++positions_[0];
    }
  }
  if (row(1, a, b)) {
    if (a >= 0x80) {
      pulse = ((a & 15) << 8) | b;
      ++positions_[1];
      waits_[1] = 0;
    } else if (a) {
      pulse = (pulse + int8_t(b)) & 4095;
      if (++waits_[1] >= a) {
        ++positions_[1];
        waits_[1] = 0;
      }
    } else
      ++positions_[1];
  }
  if (row(2, a, b)) {
    if (a >= 0x80) {
      filter = (b & 7) ? (a >> 4) & 7 : 0;
      resonance = b >> 4;
      ++positions_[2];
      waits_[2] = 0;
      if (row(2, a, b) && a == 0) {
        cutoff = b;
        ++positions_[2];
      }
    } else if (a == 0) {
      cutoff = b;
      ++positions_[2];
      waits_[2] = 0;
    } else {
      cutoff = (cutoff + int8_t(b)) & 255;
      if (++waits_[2] >= a) {
        ++positions_[2];
        waits_[2] = 0;
      }
    }
  }
}
