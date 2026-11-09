// sscanf() with working "hh" (8-bit) conversions on libcs without C99 stdio
// formats.
//
// newlib as shipped in VitaSDK is built without _WANT_IO_C99_FORMATS: its
// scanf treats the first 'h' as "short" and fails the whole match on the second
// one, so every "%hhu" / "%hhX" in the project reader returned a match failure
// ("Invalid project settings"). printf degrades harmlessly to %hu, so only the
// reading side needs help.
//
// compatSscanf() rewrites each "hh" conversion to "h", scans it into a 16-bit
// temporary and stores the result into the real 8-bit target, only for the
// conversions that scanf actually assigned. Include this header LAST in a
// source file that parses with %hh; elsewhere nothing changes.

#pragma once

#if defined(VITA_BUILD)

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace scanf_compat {

constexpr int kMaxArgs = 24;  // the longest format in the project files has 14

inline int sscanfHH(const char* input, const char* format, ...) {
  char rewritten[512];
  void* args[kMaxArgs] = {};
  unsigned short temporary[kMaxArgs] = {};
  struct Slot { void* target; bool narrow; bool isSigned; bool counted; };
  Slot slots[kMaxArgs];
  int slotCount = 0;

  va_list list;
  va_start(list, format);
  size_t out = 0;
  bool overflow = false;
  for (const char* p = format; *p;) {
    if (out + 2 >= sizeof(rewritten)) { overflow = true; break; }
    if (*p != '%') { rewritten[out++] = *p++; continue; }
    rewritten[out++] = *p++;                                   // '%'
    if (*p == '%') { rewritten[out++] = *p++; continue; }
    bool suppress = false;
    if (*p == '*') { suppress = true; rewritten[out++] = *p++; }
    while (*p >= '0' && *p <= '9' && out + 2 < sizeof(rewritten)) rewritten[out++] = *p++;  // width
    bool narrow = false;
    if (p[0] == 'h' && p[1] == 'h') { narrow = true; p += 2; rewritten[out++] = 'h'; }
    else if (p[0] == 'h' || p[0] == 'l' || p[0] == 'L' || p[0] == 'j' || p[0] == 'z' || p[0] == 't') {
      rewritten[out++] = *p++;
      if (p[-1] == 'l' && *p == 'l') rewritten[out++] = *p++;
    }
    if (!*p) break;
    const char conversion = *p;
    rewritten[out++] = *p++;
    if (conversion == '[') {                                    // copy the scan set verbatim
      if (*p == '^') rewritten[out++] = *p++;
      if (*p == ']') rewritten[out++] = *p++;
      while (*p && *p != ']' && out + 2 < sizeof(rewritten)) rewritten[out++] = *p++;
      if (*p == ']') rewritten[out++] = *p++;
    }
    if (suppress) continue;                                     // consumes no argument
    if (slotCount >= kMaxArgs) { overflow = true; break; }
    void* target = va_arg(list, void*);
    const bool isSigned = conversion == 'd' || conversion == 'i';
    slots[slotCount] = {target, narrow, isSigned, conversion != 'n'};
    args[slotCount] = narrow ? static_cast<void*>(&temporary[slotCount]) : target;
    slotCount++;
  }
  va_end(list);
  rewritten[out] = 0;
  if (overflow) return EOF;

  const int assigned = sscanf(input, rewritten,
      args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7],
      args[8], args[9], args[10], args[11], args[12], args[13], args[14], args[15],
      args[16], args[17], args[18], args[19], args[20], args[21], args[22], args[23]);
  if (assigned <= 0) return assigned;

  int counted = 0;
  for (int i = 0; i < slotCount; i++) {
    if (slots[i].counted && counted++ >= assigned) break;
    if (!slots[i].narrow) continue;
    if (slots[i].isSigned) *static_cast<int8_t*>(slots[i].target) = (int8_t)(short)temporary[i];
    else *static_cast<uint8_t*>(slots[i].target) = (uint8_t)temporary[i];
  }
  return assigned;
}

}  // namespace scanf_compat

#undef sscanf
#define sscanf scanf_compat::sscanfHH

#endif  // VITA_BUILD
