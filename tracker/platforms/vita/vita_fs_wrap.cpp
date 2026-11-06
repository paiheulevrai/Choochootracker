// PS Vita replacement for tmpfile(), linked in with `-Wl,--wrap=tmpfile`
// (see Makefile.vita; the Android build uses the same mechanism).
//
// newlib's tmpfile() creates its file in "/tmp", which does not exist on the
// Vita, so it always fails. The project writer ("Can't create temporary
// project") and the soundbank/preset ZIP reader both depend on it. Instead of
// touching the memory card, the temporary file is kept in memory behind a
// FILE* created with fopencookie(); it is freed by fclose().

#ifdef VITA_BUILD

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <cstdint>
#include <new>
#include <vector>

namespace {

struct MemoryFile {
  std::vector<uint8_t> data;
  size_t position = 0;
};

// The seek callback's offset type differs between libc flavours (off64_t,
// _off64_t); take it from the declaration instead of spelling it out.
template <typename F> struct SeekOffset;
template <typename O> struct SeekOffset<int (*)(void*, O*, int)> { using type = O; };
using Offset = SeekOffset<decltype(cookie_io_functions_t::seek)>::type;

ssize_t memoryRead(void* cookie, char* buffer, size_t size) {
  MemoryFile* file = static_cast<MemoryFile*>(cookie);
  if (file->position >= file->data.size()) return 0;
  const size_t count = std::min(size, file->data.size() - file->position);
  memcpy(buffer, file->data.data() + file->position, count);
  file->position += count;
  return (ssize_t)count;
}

ssize_t memoryWrite(void* cookie, const char* buffer, size_t size) {
  MemoryFile* file = static_cast<MemoryFile*>(cookie);
  try {
    if (file->position + size > file->data.size()) file->data.resize(file->position + size);
  } catch (const std::bad_alloc&) {
    errno = ENOSPC;
    return -1;
  }
  memcpy(file->data.data() + file->position, buffer, size);
  file->position += size;
  return (ssize_t)size;
}

int memorySeek(void* cookie, Offset* offset, int whence) {
  MemoryFile* file = static_cast<MemoryFile*>(cookie);
  long long base = 0;
  if (whence == SEEK_CUR) base = (long long)file->position;
  else if (whence == SEEK_END) base = (long long)file->data.size();
  else if (whence != SEEK_SET) { errno = EINVAL; return -1; }
  const long long target = base + (long long)*offset;
  if (target < 0) { errno = EINVAL; return -1; }
  file->position = (size_t)target;  // seeking past the end is allowed; a later write zero-fills
  *offset = (Offset)target;
  return 0;
}

int memoryClose(void* cookie) {
  delete static_cast<MemoryFile*>(cookie);
  return 0;
}

}  // namespace

extern "C" FILE* __wrap_tmpfile(void) {
  MemoryFile* file = new (std::nothrow) MemoryFile();
  if (!file) { errno = ENOMEM; return nullptr; }
  cookie_io_functions_t functions;
  functions.read = memoryRead;
  functions.write = memoryWrite;
  functions.seek = memorySeek;
  functions.close = memoryClose;
  FILE* stream = fopencookie(file, "w+", functions);
  if (!stream) delete file;
  return stream;
}

#endif  // VITA_BUILD
