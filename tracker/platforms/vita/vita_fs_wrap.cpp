// PS Vita file-system shims, linked in with `-Wl,--wrap=<name>` (see Makefile.vita).
//
// The Vita has no "current directory" the way desktop systems do: a path needs
// a device prefix ("ux0:", "app0:", ...). ChooChooTracker uses relative paths
// ("projects", "instruments/...", settings.txt entries) and newlib's tmpfile()
// wants "/tmp", which does not exist. Every file call that takes a path is
// therefore routed through here:
//   * relative paths are resolved against the app's data folder, and
//   * tmpfile() creates a real file in <data folder>/tmp that is deleted again
//     on fclose().
// The wrappers apply to every object in the link, including libstdc++
// (std::filesystem, fstreams) and SDL, so the whole program behaves the same.

#ifdef VITA_BUILD

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <map>
#include <mutex>
#include <string>

#define VITA_DATA_DIR "ux0:data/ChooChooTracker"

extern "C" {
// Weak so that a symbol missing from this newlib build degrades to ENOSYS
// instead of breaking the link.
FILE* __real_fopen(const char*, const char*) __attribute__((weak));
int __real_fclose(FILE*) __attribute__((weak));
int __real_open(const char*, int, ...) __attribute__((weak));
DIR* __real_opendir(const char*) __attribute__((weak));
int __real_stat(const char*, struct stat*) __attribute__((weak));
int __real_lstat(const char*, struct stat*) __attribute__((weak));
int __real_mkdir(const char*, mode_t) __attribute__((weak));
int __real_rename(const char*, const char*) __attribute__((weak));
int __real_remove(const char*) __attribute__((weak));
int __real_unlink(const char*) __attribute__((weak));
int __real_rmdir(const char*) __attribute__((weak));
int __real_access(const char*, int) __attribute__((weak));
}

namespace {

constexpr size_t kPathMax = 1024;

// A path is already usable when it starts with a device ("ux0:", "app0:") or
// with '/'. Everything else is relative to the data folder.
bool needsResolve(const char* path) {
  if (!path || !*path || *path == '/') return false;
  for (const char* c = path; *c && *c != '/'; ++c)
    if (*c == ':') return false;
  return true;
}

// Returns `path` itself, or `buffer` holding the resolved copy.
const char* resolve(const char* path, char* buffer) {
  if (!needsResolve(path)) return path;
  const int n = snprintf(buffer, kPathMax, VITA_DATA_DIR "/%s", path);
  if (n < 0 || n >= (int)kPathMax) return path;
  return buffer;
}

std::mutex g_tempMutex;
std::map<FILE*, std::string> g_tempFiles;
unsigned g_tempCounter = 0;

}  // namespace

extern "C" {

FILE* __wrap_fopen(const char* path, const char* mode) {
  if (!__real_fopen) { errno = ENOSYS; return nullptr; }
  char buffer[kPathMax];
  return __real_fopen(resolve(path, buffer), mode);
}

int __wrap_open(const char* path, int flags, ...) {
  if (!__real_open) { errno = ENOSYS; return -1; }
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list args;
    va_start(args, flags);
    mode = (mode_t)va_arg(args, int);
    va_end(args);
  }
  char buffer[kPathMax];
  return __real_open(resolve(path, buffer), flags, mode);
}

DIR* __wrap_opendir(const char* path) {
  if (!__real_opendir) { errno = ENOSYS; return nullptr; }
  char buffer[kPathMax];
  return __real_opendir(resolve(path, buffer));
}

int __wrap_stat(const char* path, struct stat* st) {
  if (!__real_stat) { errno = ENOSYS; return -1; }
  char buffer[kPathMax];
  return __real_stat(resolve(path, buffer), st);
}

int __wrap_lstat(const char* path, struct stat* st) {
  char buffer[kPathMax];
  const char* resolved = resolve(path, buffer);
  if (__real_lstat) return __real_lstat(resolved, st);
  if (__real_stat) return __real_stat(resolved, st);  // no symlinks on the Vita
  errno = ENOSYS;
  return -1;
}

int __wrap_mkdir(const char* path, mode_t mode) {
  if (!__real_mkdir) { errno = ENOSYS; return -1; }
  char buffer[kPathMax];
  return __real_mkdir(resolve(path, buffer), mode);
}

int __wrap_rename(const char* from, const char* to) {
  if (!__real_rename) { errno = ENOSYS; return -1; }
  char a[kPathMax], b[kPathMax];
  return __real_rename(resolve(from, a), resolve(to, b));
}

int __wrap_remove(const char* path) {
  if (!__real_remove) { errno = ENOSYS; return -1; }
  char buffer[kPathMax];
  return __real_remove(resolve(path, buffer));
}

int __wrap_unlink(const char* path) {
  if (!__real_unlink) { errno = ENOSYS; return -1; }
  char buffer[kPathMax];
  return __real_unlink(resolve(path, buffer));
}

int __wrap_rmdir(const char* path) {
  if (!__real_rmdir) { errno = ENOSYS; return -1; }
  char buffer[kPathMax];
  return __real_rmdir(resolve(path, buffer));
}

int __wrap_access(const char* path, int mode) {
  char buffer[kPathMax];
  const char* resolved = resolve(path, buffer);
  if (__real_access) return __real_access(resolved, mode);
  // No access(): fall back to "does it exist".
  struct stat st;
  return __wrap_stat(resolved, &st);
}

// newlib's tmpfile() looks in "/tmp", which does not exist on the Vita.
FILE* __wrap_tmpfile(void) {
  if (!__real_fopen) { errno = ENOSYS; return nullptr; }
  char path[kPathMax];
  unsigned id;
  {
    std::lock_guard<std::mutex> lock(g_tempMutex);
    id = ++g_tempCounter;
  }
  snprintf(path, sizeof(path), VITA_DATA_DIR "/tmp/t%u.tmp", id);
  if (__real_mkdir) __real_mkdir(VITA_DATA_DIR "/tmp", 0777);  // EEXIST is fine
  FILE* file = __real_fopen(path, "w+b");
  if (!file) return nullptr;
  std::lock_guard<std::mutex> lock(g_tempMutex);
  g_tempFiles[file] = path;
  return file;
}

// Temporary files are removed when closed, like a real tmpfile().
int __wrap_fclose(FILE* file) {
  if (!__real_fclose) { errno = ENOSYS; return EOF; }
  std::string temporaryPath;
  {
    std::lock_guard<std::mutex> lock(g_tempMutex);
    auto it = g_tempFiles.find(file);
    if (it != g_tempFiles.end()) {
      temporaryPath = it->second;
      g_tempFiles.erase(it);
    }
  }
  const int result = __real_fclose(file);
  if (!temporaryPath.empty() && __real_remove) __real_remove(temporaryPath.c_str());
  return result;
}

}  // extern "C"

#endif  // VITA_BUILD
