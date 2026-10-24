// Storage Access Framework (SAF) workspace for Android.
//
// The engine uses plain stdio/POSIX calls. A folder picked with
// ACTION_OPEN_DOCUMENT_TREE has no usable filesystem path, so Makefile.android
// links libchipnomad.so with -Wl,--wrap for the functions below. Paths under
// SAF_ROOT are served from the picked folder through ChooChooTrackerActivity
// (saf* methods); every other path goes to the real libc function.
//
// The bundled content (banks, presets, demo projects) stays in the app's
// private workspace and shows through as a read-only overlay: a path missing
// from the picked folder falls back to the private copy, and the picked folder
// wins when both exist. Writes always go to the picked folder.

#include <SDL2/SDL.h>
#include <jni.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#define SAF_ROOT "/ChooChooTracker"

extern "C" {
FILE* __real_fopen(const char*, const char*);
int __real_open(const char*, int, ...);
int __real_access(const char*, int);
int __real_chdir(const char*);
char* __real_getcwd(char*, size_t);
DIR* __real_opendir(const char*);
struct dirent* __real_readdir(DIR*);
int __real_closedir(DIR*);
int __real_stat(const char*, struct stat*);
int __real_lstat(const char*, struct stat*);
int __real_mkdir(const char*, mode_t);
char* __real_realpath(const char*, char*);
int __real_remove(const char*);
int __real_rename(const char*, const char*);
int __real_unlinkat(int, const char*, int);
}

extern int androidGetPrivateWorkspacePath(char* buffer, int bufferSize);

namespace {

std::mutex stateMutex;
std::string virtualCwd;             // set by chdir(SAF_ROOT...), empty otherwise
std::string privateRootCache;

const std::string& privateRoot() {
  std::lock_guard<std::mutex> lock(stateMutex);
  if (privateRootCache.empty()) {
    char buffer[1024];
    if (androidGetPrivateWorkspacePath(buffer, sizeof(buffer)) == 0) privateRootCache = buffer;
  }
  return privateRootCache;
}

// Resolve ".", ".." and duplicate slashes. Relative paths use the virtual
// working directory when one is set, otherwise they are left alone.
bool normalize(const char* path, std::string& out) {
  if (!path || !path[0]) return false;
  std::string full;
  if (path[0] == '/') {
    full = path;
  } else {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (virtualCwd.empty()) return false;
    full = virtualCwd + "/" + path;
  }
  std::vector<std::string> parts;
  size_t i = 0;
  while (i <= full.size()) {
    size_t j = full.find('/', i);
    if (j == std::string::npos) j = full.size();
    std::string part = full.substr(i, j - i);
    if (part == "..") { if (!parts.empty()) parts.pop_back(); }
    else if (!part.empty() && part != ".") parts.push_back(part);
    i = j + 1;
  }
  out.clear();
  for (const std::string& part : parts) out += "/" + part;
  if (out.empty()) out = "/";
  return true;
}

// True when `path` is inside the SAF workspace; `rel` is the part after the root.
bool toVirtual(const char* path, std::string& rel) {
  std::string full;
  if (!normalize(path, full)) return false;
  const size_t rootLength = strlen(SAF_ROOT);
  if (full.compare(0, rootLength, SAF_ROOT) != 0) return false;
  if (full.size() == rootLength) { rel.clear(); return true; }
  if (full[rootLength] != '/') return false;
  rel = full.substr(rootLength + 1);
  return true;
}

std::string privatePath(const std::string& rel) {
  const std::string& root = privateRoot();
  return rel.empty() ? root : root + "/" + rel;
}

struct Jni {
  JNIEnv* env;
  jobject activity;
  Jni() : env((JNIEnv*)SDL_AndroidGetJNIEnv()), activity((jobject)SDL_AndroidGetActivity()) {}
  ~Jni() { if (env && activity) env->DeleteLocalRef(activity); }
  bool ok() const { return env && activity; }
};

jmethodID method(Jni& jni, const char* name, const char* signature) {
  jclass cls = jni.env->GetObjectClass(jni.activity);
  jmethodID id = jni.env->GetMethodID(cls, name, signature);
  jni.env->DeleteLocalRef(cls);
  if (jni.env->ExceptionCheck()) jni.env->ExceptionClear();
  return id;
}

int callIntWithPath(const char* name, const std::string& rel, const char* extra = NULL) {
  Jni jni;
  if (!jni.ok()) return -1;
  jmethodID id = method(jni, name, extra ? "(Ljava/lang/String;Ljava/lang/String;)I" : "(Ljava/lang/String;)I");
  if (!id) return -1;
  jstring path = jni.env->NewStringUTF(rel.c_str());
  int result;
  if (extra) {
    jstring second = jni.env->NewStringUTF(extra);
    result = jni.env->CallIntMethod(jni.activity, id, path, second);
    jni.env->DeleteLocalRef(second);
  } else {
    result = jni.env->CallIntMethod(jni.activity, id, path);
  }
  jni.env->DeleteLocalRef(path);
  if (jni.env->ExceptionCheck()) { jni.env->ExceptionClear(); return -1; }
  return result;
}

struct SafStat { bool exists = false; bool isDir = false; long long size = 0; long long mtime = 0; };

SafStat safStat(const std::string& rel) {
  SafStat result;
  Jni jni;
  if (!jni.ok()) return result;
  jmethodID id = method(jni, "safStat", "(Ljava/lang/String;)[J");
  if (!id) return result;
  jstring path = jni.env->NewStringUTF(rel.c_str());
  jlongArray array = (jlongArray)jni.env->CallObjectMethod(jni.activity, id, path);
  jni.env->DeleteLocalRef(path);
  if (jni.env->ExceptionCheck()) { jni.env->ExceptionClear(); return result; }
  if (!array) return result;
  jlong values[3] = {0, 0, 0};
  jni.env->GetLongArrayRegion(array, 0, 3, values);
  jni.env->DeleteLocalRef(array);
  result.exists = true;
  result.isDir = values[0] != 0;
  result.size = values[1];
  result.mtime = values[2];
  return result;
}

struct SafEntry { std::string name; bool isDir; };

// Returns false when `rel` is not a directory in the picked folder.
bool safList(const std::string& rel, std::vector<SafEntry>& entries) {
  Jni jni;
  if (!jni.ok()) return false;
  jmethodID id = method(jni, "safList", "(Ljava/lang/String;)[Ljava/lang/String;");
  if (!id) return false;
  jstring path = jni.env->NewStringUTF(rel.c_str());
  jobjectArray array = (jobjectArray)jni.env->CallObjectMethod(jni.activity, id, path);
  jni.env->DeleteLocalRef(path);
  if (jni.env->ExceptionCheck()) { jni.env->ExceptionClear(); return false; }
  if (!array) return false;
  jsize count = jni.env->GetArrayLength(array);
  for (jsize i = 0; i < count; ++i) {
    jstring item = (jstring)jni.env->GetObjectArrayElement(array, i);
    const char* chars = item ? jni.env->GetStringUTFChars(item, NULL) : NULL;
    if (chars && chars[0]) entries.push_back({chars + 1, chars[0] == 'd'});  // "d<name>" or "f<name>"
    if (chars) jni.env->ReleaseStringUTFChars(item, chars);
    if (item) jni.env->DeleteLocalRef(item);
  }
  jni.env->DeleteLocalRef(array);
  return true;
}

void fillStat(struct stat* st, const SafStat& info) {
  memset(st, 0, sizeof(*st));
  st->st_mode = info.isDir ? (S_IFDIR | 0775) : (S_IFREG | 0664);
  st->st_size = info.isDir ? 4096 : (off_t)info.size;
  st->st_nlink = 1;
  st->st_mtime = (time_t)(info.mtime / 1000);
  st->st_atime = st->st_mtime;
  st->st_ctime = st->st_mtime;
  st->st_blksize = 4096;
}

// Virtual directory stream. Its address is registered so readdir/closedir can
// tell it apart from a libc DIR*.
struct VirtualDir {
  std::vector<SafEntry> entries;
  size_t index = 0;
  struct dirent current;
};

std::mutex dirMutex;
std::set<void*> virtualDirs;

bool isVirtualDir(DIR* dir) {
  std::lock_guard<std::mutex> lock(dirMutex);
  return virtualDirs.count((void*)dir) != 0;
}

// Modes that need the picked folder rather than the private overlay.
bool isWriteMode(const char* mode) {
  return mode && (strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+'));
}

const char* safMode(const char* mode) {
  bool plus = strchr(mode, '+') != NULL;
  if (strchr(mode, 'a')) return plus ? "rwa" : "wa";
  if (strchr(mode, 'w')) return plus ? "rwt" : "wt";
  return plus ? "rw" : "r";
}

FILE* openVirtual(const std::string& rel, const char* mode) {
  bool write = isWriteMode(mode);
  if (!write) {
    SafStat info = safStat(rel);
    if (!info.exists) {
      std::string fallback = privatePath(rel);
      return __real_fopen(fallback.c_str(), mode);
    }
    if (info.isDir) { errno = EISDIR; return NULL; }
  }
  int fd = callIntWithPath("safOpen", rel, safMode(mode));
  if (fd < 0) { errno = write ? EACCES : ENOENT; return NULL; }
  FILE* file = fdopen(fd, mode);
  if (!file) close(fd);
  return file;
}

}  // namespace

extern "C" {

FILE* __wrap_fopen(const char* path, const char* mode) {
  std::string rel;
  if (path && mode && toVirtual(path, rel)) return openVirtual(rel, mode);
  return __real_fopen(path, mode);
}

int __wrap_open(const char* path, int flags, ...) {
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list args;
    va_start(args, flags);
    mode = (mode_t)va_arg(args, int);
    va_end(args);
  }
  std::string rel;
  if (path && toVirtual(path, rel)) {
    int access = flags & O_ACCMODE;
    bool write = access != O_RDONLY || (flags & (O_CREAT | O_TRUNC | O_APPEND));
    if (!write) {
      SafStat info = safStat(rel);
      if (!info.exists) {
        std::string fallback = privatePath(rel);
        return __real_open(fallback.c_str(), flags, mode);
      }
    }
    const char* safFlags = !write ? "r" : (flags & O_APPEND) ? (access == O_RDWR ? "rwa" : "wa")
                           : (flags & O_TRUNC) ? (access == O_RDWR ? "rwt" : "wt") : "rw";
    int fd = callIntWithPath("safOpen", rel, safFlags);
    if (fd < 0) errno = write ? EACCES : ENOENT;
    return fd;
  }
  return __real_open(path, flags, mode);
}

int __wrap_stat(const char* path, struct stat* st) {
  std::string rel;
  if (path && st && toVirtual(path, rel)) {
    SafStat info = safStat(rel);
    if (info.exists) { fillStat(st, info); return 0; }
    std::string fallback = privatePath(rel);
    return __real_stat(fallback.c_str(), st);
  }
  return __real_stat(path, st);
}

int __wrap_lstat(const char* path, struct stat* st) {
  std::string rel;
  if (path && st && toVirtual(path, rel)) return __wrap_stat(path, st);
  return __real_lstat(path, st);
}

int __wrap_access(const char* path, int mode) {
  std::string rel;
  if (path && toVirtual(path, rel)) {
    struct stat st;
    if (__wrap_stat(path, &st) != 0) return -1;
    if ((mode & W_OK) && !safStat(rel).exists && !rel.empty()) { errno = EACCES; return -1; }
    return 0;
  }
  return __real_access(path, mode);
}

int __wrap_mkdir(const char* path, mode_t mode) {
  std::string rel;
  if (path && toVirtual(path, rel)) {
    if (rel.empty()) { errno = EEXIST; return -1; }
    struct stat st;
    if (__wrap_stat(path, &st) == 0) { errno = EEXIST; return -1; }
    int result = callIntWithPath("safMkdir", rel);
    if (result < 0) { errno = EACCES; return -1; }
    return 0;
  }
  return __real_mkdir(path, mode);
}

int __wrap_remove(const char* path) {
  std::string rel;
  if (path && toVirtual(path, rel)) {
    if (!safStat(rel).exists) {
      struct stat st;
      errno = __wrap_stat(path, &st) == 0 ? EACCES : ENOENT;  // bundled content is read-only
      return -1;
    }
    if (callIntWithPath("safDelete", rel) != 0) { errno = EACCES; return -1; }
    return 0;
  }
  return __real_remove(path);
}

int __wrap_unlinkat(int dirfd, const char* path, int flags) {
  if (dirfd == AT_FDCWD && path) {
    std::string rel;
    if (toVirtual(path, rel)) return __wrap_remove(path);
  }
  return __real_unlinkat(dirfd, path, flags);
}

int __wrap_rename(const char* from, const char* to) {
  std::string relFrom, relTo;
  bool virtualFrom = from && toVirtual(from, relFrom);
  bool virtualTo = to && toVirtual(to, relTo);
  if (virtualFrom && virtualTo) {
    if (!safStat(relFrom).exists) { errno = ENOENT; return -1; }
    if (callIntWithPath("safRename", relFrom, relTo.c_str()) != 0) { errno = EACCES; return -1; }
    return 0;
  }
  if (virtualFrom || virtualTo) { errno = EXDEV; return -1; }
  return __real_rename(from, to);
}

int __wrap_chdir(const char* path) {
  std::string rel;
  if (path && toVirtual(path, rel)) {
    struct stat st;
    if (__wrap_stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) { errno = ENOENT; return -1; }
    std::string full;
    normalize(path, full);
    std::lock_guard<std::mutex> lock(stateMutex);
    virtualCwd = full;
    return 0;
  }
  int result = __real_chdir(path);
  if (result == 0) {
    std::lock_guard<std::mutex> lock(stateMutex);
    virtualCwd.clear();
  }
  return result;
}

char* __wrap_getcwd(char* buffer, size_t size) {
  std::string cwd;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    cwd = virtualCwd;
  }
  if (cwd.empty()) return __real_getcwd(buffer, size);
  if (!buffer) buffer = (char*)malloc(cwd.size() + 1), size = cwd.size() + 1;
  if (!buffer || cwd.size() + 1 > size) { errno = ERANGE; return NULL; }
  memcpy(buffer, cwd.c_str(), cwd.size() + 1);
  return buffer;
}

char* __wrap_realpath(const char* path, char* resolved) {
  std::string rel;
  if (path && toVirtual(path, rel)) {
    struct stat st;
    if (__wrap_stat(path, &st) != 0) { errno = ENOENT; return NULL; }
    std::string full;
    normalize(path, full);
    if (!resolved) return strdup(full.c_str());
    strncpy(resolved, full.c_str(), PATH_MAX - 1);
    resolved[PATH_MAX - 1] = 0;
    return resolved;
  }
  return __real_realpath(path, resolved);
}

DIR* __wrap_opendir(const char* path) {
  std::string rel;
  if (!path || !toVirtual(path, rel)) return __real_opendir(path);

  VirtualDir* dir = new VirtualDir();
  std::set<std::string> seen;
  bool found = false;

  std::vector<SafEntry> picked;
  if (safList(rel, picked)) {
    found = true;
    for (const SafEntry& entry : picked) {
      if (seen.insert(entry.name).second) dir->entries.push_back(entry);
    }
  }
  std::string fallback = privatePath(rel);
  if (DIR* real = __real_opendir(fallback.c_str())) {
    found = true;
    while (struct dirent* item = __real_readdir(real)) {
      if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..")) continue;
      if (seen.insert(item->d_name).second) dir->entries.push_back({item->d_name, item->d_type == DT_DIR});
    }
    __real_closedir(real);
  }
  if (!found) {
    delete dir;
    errno = ENOENT;
    return NULL;
  }
  dir->entries.insert(dir->entries.begin(), {{".", true}, {"..", true}});
  std::lock_guard<std::mutex> lock(dirMutex);
  virtualDirs.insert(dir);
  return (DIR*)dir;
}

struct dirent* __wrap_readdir(DIR* handle) {
  if (!handle || !isVirtualDir(handle)) return __real_readdir(handle);
  VirtualDir* dir = (VirtualDir*)handle;
  if (dir->index >= dir->entries.size()) return NULL;
  const SafEntry& entry = dir->entries[dir->index++];
  memset(&dir->current, 0, sizeof(dir->current));
  dir->current.d_type = entry.isDir ? DT_DIR : DT_REG;
  strncpy(dir->current.d_name, entry.name.c_str(), sizeof(dir->current.d_name) - 1);
  return &dir->current;
}

int __wrap_closedir(DIR* handle) {
  if (!handle) return __real_closedir(handle);
  {
    std::lock_guard<std::mutex> lock(dirMutex);
    if (virtualDirs.erase((void*)handle)) {
      delete (VirtualDir*)handle;
      return 0;
    }
  }
  return __real_closedir(handle);
}

}  // extern "C"
