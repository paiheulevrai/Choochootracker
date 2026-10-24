#include "corelib_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#endif

#ifndef ANDROID_BUILD
// Helper: Create directory recursively
static void createDirectoryRecursive(const char* path) {
  char tmp[4096];
  char* p = NULL;
  size_t len;

  snprintf(tmp, sizeof(tmp), "%s", path);
  len = strlen(tmp);
  if (tmp[len - 1] == '/') tmp[len - 1] = 0;

  for (p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;
      #ifdef _WIN32
      mkdir(tmp);
      #else
      mkdir(tmp, 0755);
      #endif
      *p = '/';
    }
  }
  #ifdef _WIN32
  mkdir(tmp);
  #else
  mkdir(tmp, 0755);
  #endif
}
#endif // !ANDROID_BUILD

// Windows and Linux desktop builds are meant to be portable (see
// docs/build-notes.md: a Windows release ships as the exe alongside its
// DLLs and assets, run from anywhere). Resolving settings/autosave/projects
// relative to getcwd() breaks that: it depends on the shell's current
// directory at launch time (e.g. being cd'd into a subfolder), not on where
// the app itself lives, so settings.txt can silently end up somewhere
// different from one run to the next. Resolve the running executable's own
// directory instead, so the app always finds "its" files regardless of the
// caller's cwd.
#if !defined(ANDROID_BUILD) && !defined(MACOS_BUILD)
static int fileGetExecutableDirectory(char* buffer, int bufferSize) {
#ifdef _WIN32
  char exePath[4096];
  DWORD len = GetModuleFileNameA(NULL, exePath, sizeof(exePath));
  if (len == 0 || len >= sizeof(exePath)) return -1;
  exePath[len] = '\0';
  char* lastSep = strrchr(exePath, '\\');
#else
  char exePath[4096];
  ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
  if (len <= 0) return -1;
  exePath[len] = '\0';
  char* lastSep = strrchr(exePath, '/');
#endif
  if (!lastSep) return -1;
  *lastSep = '\0';
  snprintf(buffer, bufferSize, "%s", exePath);
  return 0;
}
#endif

int fileIsRunningFromAppImage(void) {
#if defined(ANDROID_BUILD) || defined(MACOS_BUILD) || defined(_WIN32)
  return 0;
#else
  return getenv("APPIMAGE") != NULL;
#endif
}

int fileCreateDirectoryRecursive(const char* path) {
  if (path == NULL || path[0] == 0) return -1;
  if (fileDirectoryExists(path)) return 0;

  char tmp[4096];
  snprintf(tmp, sizeof(tmp), "%s", path);
  size_t len = strlen(tmp);
  if (len == 0) return -1;
#ifdef _WIN32
  // Callers commonly build paths with '/', while the Windows APIs and the
  // drive-prefix check below use '\\'. Normalize once so recursive creation
  // behaves consistently for both spellings.
  for (size_t i = 0; i < len; i++) {
    if (tmp[i] == '/') tmp[i] = '\\';
  }
#endif
  if (tmp[len - 1] == '/' || tmp[len - 1] == '\\') tmp[len - 1] = 0;
  if (tmp[0] == 0) return -1;

  // Create each path level in turn; existing levels report EEXIST and are fine
  char* firstSeparator = tmp + 1;
#ifdef _WIN32
  if (tmp[1] == ':' && (tmp[2] == '/' || tmp[2] == '\\')) firstSeparator = tmp + 3;
#endif
  for (char* p = firstSeparator; *p; p++) {
    if (*p == '/' || *p == '\\') {
      *p = 0;
      if (!fileDirectoryExists(tmp)) {
        if (fileCreateDirectory(tmp) != 0) return -1;
      }
      *p = PATH_SEPARATOR;
    }
  }
  if (!fileDirectoryExists(tmp)) {
    if (fileCreateDirectory(tmp) != 0) return -1;
  }
  return 0;
}

int fileRename(const char* oldPath, const char* newPath) {
  if (oldPath == NULL || newPath == NULL || oldPath[0] == 0 || newPath[0] == 0) return -1;
#ifdef WEB_BUILD
  // The virtual FS has no rename; callers must treat failure as non-fatal
  (void)oldPath;
  (void)newPath;
  return -1;
#elif defined(_WIN32)
  return MoveFileA(oldPath, newPath) ? 0 : -1;
#else
  return rename(oldPath, newPath) == 0 ? 0 : -1;
#endif
}

int fileGetDefaultDirectory(char* buffer, int bufferSize) {
#ifdef ANDROID_BUILD
  extern int androidGetWorkspacePath(char*, int);
  return androidGetWorkspacePath(buffer, bufferSize);
#elif defined(MACOS_BUILD)
  const char* home = getenv("HOME");
  if (home) {
    snprintf(buffer, bufferSize, "%s/Library/Application Support/ChipNomad", home);
    createDirectoryRecursive(buffer);
  } else {
    snprintf(buffer, bufferSize, ".");
  }
  return 0;
#else
  // AppImages run from a read-only squashfs mount, so resolving next to the
  // executable (below) would put settings.txt/autosave.cct somewhere that
  // can't be written to. Fall back to a normal writable per-user directory
  // instead.
  if (fileIsRunningFromAppImage()) {
    const char* dataHome = getenv("XDG_DATA_HOME");
    if (dataHome && dataHome[0] != '\0') {
      snprintf(buffer, bufferSize, "%s/ChooChooTracker", dataHome);
      createDirectoryRecursive(buffer);
      return 0;
    }
    const char* home = getenv("HOME");
    if (home) {
      snprintf(buffer, bufferSize, "%s/.local/share/ChooChooTracker", home);
      createDirectoryRecursive(buffer);
      return 0;
    }
  }
  // Fall back to cwd only if the executable's own path can't be resolved
  // (e.g. an unsupported OS/filesystem) - better than failing outright.
  if (fileGetExecutableDirectory(buffer, bufferSize) == 0) return 0;
  return getcwd(buffer, bufferSize) ? 0 : -1;
#endif
}

void fileImportDocument(const char* mimeType, const char* relativeDirectory) {
#ifdef ANDROID_BUILD
  extern void androidOpenDocument(const char*, const char*);
  androidOpenDocument(mimeType, relativeDirectory);
#else
  (void)mimeType;
  (void)relativeDirectory;
#endif
}

void fileExportDocument(const char* path, const char* mimeType) {
#ifdef ANDROID_BUILD
  extern void androidSaveDocument(const char*, const char*);
  androidSaveDocument(path, mimeType);
#else
  (void)path;
  (void)mimeType;
#endif
}

void filePickWorkingFolder(void) {
#ifdef ANDROID_BUILD
  extern void androidWorkingFolderCommand(const char*);
  androidWorkingFolderCommand("pickWorkingFolder");
#endif
}

void fileClearWorkingFolder(void) {
#ifdef ANDROID_BUILD
  extern void androidWorkingFolderCommand(const char*);
  androidWorkingFolderCommand("clearWorkingFolder");
#endif
}

int fileGetWorkingFolderStatus(char* buffer, int bufferSize) {
#ifdef ANDROID_BUILD
  extern int androidGetWorkingFolderStatus(char*, int);
  return androidGetWorkingFolderStatus(buffer, bufferSize);
#else
  snprintf(buffer, bufferSize, "Unavailable");
  return 0;
#endif
}

int fileDirectoryExists(const char* path) {
#ifdef _WIN32
  if (!path || !path[0]) return 0;
  DWORD attributes = GetFileAttributesA(path);
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
#else
  struct stat statBuf;
  return (stat(path, &statBuf) == 0 && S_ISDIR(statBuf.st_mode)) ? 1 : 0;
#endif
}

int fileCreateDirectory(const char* path) {
  #ifdef _WIN32
  return mkdir(path) == 0 ? 0 : -1;
  #else
  return mkdir(path, 0755) == 0 ? 0 : -1;
  #endif
}

int fileDelete(const char* path) {
  return remove(path) == 0 ? 0 : -1;
}

FileEntry* fileListDirectory(const char* path, const char* extension, int* entryCount) {
  DIR* dir = opendir(path);
  if (!dir) {
    // Unreadable folder (e.g. /storage/emulated above the user's storage on
    // Android): still offer "..", otherwise the browser dead-ends there.
    if (strcmp(path, "/") != 0) {
      FileEntry* parentOnly = (FileEntry*)malloc(sizeof(FileEntry));
      if (parentOnly) {
        strcpy(parentOnly[0].name, "..");
        parentOnly[0].isDirectory = 1;
        *entryCount = 1;
        return parentOnly;
      }
    }
    *entryCount = 0;
    return NULL;
  }

  struct dirent* entry;
  int capacity = 100;
  int count = 0;
  FileEntry* entries = (FileEntry*)malloc(capacity * sizeof(FileEntry));

  if (!entries) {
    closedir(dir);
    *entryCount = 0;
    return NULL;
  }

  // Check first entry - if it's neither "." nor "..", insert ".." (unless at root)
  entry = readdir(dir);
  if (entry && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && strcmp(path, "/") != 0) {
    strcpy(entries[0].name, "..");
    entries[0].isDirectory = 1;
    count = 1;
  }

  // Process entries using do-while to handle first entry
  if (entry) {
    do {
      // Skip hidden files and current directory
      if (entry->d_name[0] == '.') {
        // Allow ".." for parent directory
        if (strcmp(entry->d_name, "..") != 0) continue;
      }

      char fullPath[2048];
      snprintf(fullPath, sizeof(fullPath), "%s%s%s", path, PATH_SEPARATOR_STR, entry->d_name);

      struct stat statBuf;
      if (stat(fullPath, &statBuf) != 0) continue;

      int isDir = S_ISDIR(statBuf.st_mode);

      if (!isDir && extension && extension[0] != '\0') {
        char* dot = strrchr(entry->d_name, '.');
        if (!dot) continue;

        int match = 0;
        const char* pos = extension;
        while (pos) {
          if (strcasecmp(pos, dot) == 0 ||
          (strncasecmp(pos, dot, strlen(dot)) == 0 && pos[strlen(dot)] == ',')) {
            match = 1;
            break;
          }
          pos = strchr(pos, ',');
          if (pos) pos++;
        }
        if (!match) continue;
      }

      // Resize array if needed
      if (count >= capacity) {
        capacity *= 2;
        FileEntry* newEntries = (FileEntry*)realloc(entries, capacity * sizeof(FileEntry));
        if (!newEntries) {
          free(entries);
          closedir(dir);
          *entryCount = 0;
          return NULL;
        }
        entries = newEntries;
      }

      strncpy(entries[count].name, entry->d_name, 255);
      entries[count].name[255] = 0;
      entries[count].isDirectory = isDir;
      count++;
    } while ((entry = readdir(dir)));
  }

  closedir(dir);

  // Empty folders (and some FUSE-backed ones) list nothing at all, not even
  // ".." - make sure there is always a way back up.
  if (strcmp(path, "/") != 0) {
    int hasParent = 0;
    for (int i = 0; i < count; i++) {
      if (strcmp(entries[i].name, "..") == 0) { hasParent = 1; break; }
    }
    if (!hasParent) {
      FileEntry* grown = (FileEntry*)realloc(entries, (count + 1) * sizeof(FileEntry));
      if (grown) {
        entries = grown;
        memmove(entries + 1, entries, count * sizeof(FileEntry));
        strcpy(entries[0].name, "..");
        entries[0].isDirectory = 1;
        count++;
      }
    }
  }
#ifdef ANDROID_BUILD
  // /storage/emulated is not listable by apps: no way back from there, so
  // user storage is the top of the tree (as is the picked-folder workspace).
  if (strcmp(path, "/storage/emulated/0") == 0 || strcmp(path, "/ChooChooTracker") == 0) {
    int kept = 0;
    for (int i = 0; i < count; i++) {
      if (strcmp(entries[i].name, "..") != 0) entries[kept++] = entries[i];
    }
    count = kept;
  }
#endif
  *entryCount = count;
  return entries;
}
