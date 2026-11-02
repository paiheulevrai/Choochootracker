// Small file-system and WAV-writing helpers shared by the test files.
#pragma once

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace testhelp {

inline void removeFile(const char* path) { std::remove(path); }

inline bool fileExists(const char* path) {
  FILE* file = std::fopen(path, "rb");
  if (file) std::fclose(file);
  return file != nullptr;
}

inline bool dirExists(const char* path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

inline std::string readFile(const char* path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

#ifdef _WIN32
inline int makeDirectory(const char* path) { return _mkdir(path); }
inline int removeDirectory(const char* path) { return _rmdir(path); }
#else
inline int makeDirectory(const char* path) { return mkdir(path, 0755); }
inline int removeDirectory(const char* path) { return rmdir(path); }
#endif

inline void writeU16(FILE* file, uint16_t value) {
  std::fputc(value & 0xff, file);
  std::fputc(value >> 8, file);
}

inline void writeU32(FILE* file, uint32_t value) {
  writeU16(file, value & 0xffff);
  writeU16(file, value >> 16);
}

}  // namespace testhelp
