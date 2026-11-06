// PS Vita (VitaSDK) platform glue: process-wide limits, CPU clocks and the
// first-run copy of the bundled assets into the writable data folder.
//
// Everything else (video, audio, controller) goes through the shared SDL2
// platform layer in platforms/sdl2, built against the VitaSDK SDL2 port.

#ifdef VITA_BUILD

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <psp2/power.h>

#include "../../src/corelib/corelib_assets.h"
#include "../../src/corelib/corelib_file.h"

// Vita paths are short ("ux0:data/ChooChooTracker/..."); 1 KB per buffer keeps
// the recursive asset copy cheap on the stack.
#define PATH_LENGTH_VITA 1024

// newlib's default heap is far too small for the DSP engines, sample banks
// and project data. 192 MB leaves room for the GPU/system memory pools SDL
// allocates outside the libc heap.
extern "C" {
int _newlib_heap_size_user = 192 * 1024 * 1024;
}

void vitaPlatformInit(void) {
  // Homebrew starts at 333 MHz. The synth engines are CPU bound, so run the
  // CPU at its maximum; the bus/GPU clocks follow the usual homebrew values.
  scePowerSetArmClockFrequency(444);
  scePowerSetBusClockFrequency(222);
  scePowerSetGpuClockFrequency(222);
  scePowerSetGpuXbarClockFrequency(166);
}

namespace {

const char* const kBundledAssets = "app0:common";

bool isDirectory(const char* path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool fileExists(const char* path) {
  struct stat st;
  return stat(path, &st) == 0;
}

void makeDirectory(const char* path) {
  if (mkdir(path, 0777) != 0 && errno != EEXIST)
    fprintf(stderr, "Cannot create '%s': %s\n", path, strerror(errno));
}

// Copy one file unless the destination already exists, so the user's edits
// and saved projects survive a VPK update.
bool copyFileIfMissing(const char* source, const char* target) {
  if (fileExists(target)) return true;
  FILE* in = fopen(source, "rb");
  if (!in) return false;
  FILE* out = fopen(target, "wb");
  if (!out) {
    fclose(in);
    return false;
  }
  static char buffer[64 * 1024];
  bool ok = true;
  size_t count;
  while ((count = fread(buffer, 1, sizeof(buffer), in)) > 0) {
    if (fwrite(buffer, 1, count, out) != count) {
      ok = false;
      break;
    }
  }
  if (ferror(in)) ok = false;
  fclose(in);
  if (fclose(out) != 0) ok = false;
  if (!ok) remove(target);  // never leave a truncated asset behind
  return ok;
}

// Returns the number of entries that could not be copied.
int copyTreeIfMissing(const char* source, const char* target) {
  DIR* dir = opendir(source);
  if (!dir) return 1;
  makeDirectory(target);
  int failures = 0;
  while (struct dirent* entry = readdir(dir)) {
    if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
    char from[PATH_LENGTH_VITA], to[PATH_LENGTH_VITA];
    if (snprintf(from, sizeof(from), "%s/%s", source, entry->d_name) >= (int)sizeof(from) ||
        snprintf(to, sizeof(to), "%s/%s", target, entry->d_name) >= (int)sizeof(to)) {
      failures++;
      continue;
    }
    if (isDirectory(from)) failures += copyTreeIfMissing(from, to);
    else if (!copyFileIfMissing(from, to)) failures++;
  }
  closedir(dir);
  return failures;
}

// Number of entries in a directory, or -1 if it cannot be opened.
int countEntries(const char* path) {
  DIR* dir = opendir(path);
  if (!dir) return -1;
  int count = 0;
  while (struct dirent* entry = readdir(dir))
    if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) count++;
  closedir(dir);
  return count;
}

// Remove leftovers of temporary files a crashed session could not delete.
void clearDirectory(const char* path) {
  DIR* dir = opendir(path);
  if (!dir) return;
  while (struct dirent* entry = readdir(dir)) {
    if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
    char file[PATH_LENGTH_VITA];
    if (snprintf(file, sizeof(file), "%s/%s", path, entry->d_name) < (int)sizeof(file)) remove(file);
  }
  closedir(dir);
}

// Writes <data folder>/vita_log.txt: a few facts about the file system that
// explain "files not found" problems without a debugger.
void writeDiagnostics(const char* workspace, int copyFailures, int chdirResult, int chdirErrno) {
  char path[PATH_LENGTH_VITA];
  snprintf(path, sizeof(path), "%s/vita_log.txt", workspace);
  FILE* log = fopen(path, "w");
  if (!log) return;
  char cwd[PATH_LENGTH_VITA] = "?";
  if (!getcwd(cwd, sizeof(cwd))) snprintf(cwd, sizeof(cwd), "(getcwd failed: %s)", strerror(errno));
  fprintf(log, "workspace: %s\n", workspace);
  fprintf(log, "bundled assets present: %d, copy failures: %d\n", isDirectory(kBundledAssets) ? 1 : 0, copyFailures);
  fprintf(log, "chdir(workspace): %d (errno %d)\n", chdirResult, chdirErrno);
  fprintf(log, "cwd: %s\n", cwd);
  const char* const folders[] = {"projects", "instruments", "instruments/dx7", "themes", "fonts", "samples",
                                 "pitch-tables", "title", "AY_wavetables", "waveforms", "SR_wavetables"};
  for (const char* folder : folders) {
    char full[PATH_LENGTH_VITA];
    snprintf(full, sizeof(full), "%s/%s", workspace, folder);
    struct stat st;
    const int statResult = stat(full, &st);
    fprintf(log, "%-18s stat=%d mode=0x%x isdir=%d entries=%d relative_entries=%d\n", folder, statResult,
            statResult == 0 ? (unsigned)st.st_mode : 0u, statResult == 0 && S_ISDIR(st.st_mode) ? 1 : 0,
            countEntries(full), countEntries(folder));
  }
  fclose(log);
}

}  // namespace

int assetsInit(void) {
  char workspace[PATH_LENGTH_VITA];
  if (fileGetDefaultDirectory(workspace, sizeof(workspace)) != 0) {
    fprintf(stderr, "Cannot create the Vita data folder\n");
    return 1;
  }

  int failures = 0;
  if (isDirectory(kBundledAssets)) {
    failures = copyTreeIfMissing(kBundledAssets, workspace);
    if (failures)
      fprintf(stderr, "Vita: %d bundled asset(s) could not be copied to %s\n", failures, workspace);
  }

  // Temporary files (tmpfile() is redirected here, see vita_fs_wrap.cpp).
  char tempFolder[PATH_LENGTH_VITA];
  snprintf(tempFolder, sizeof(tempFolder), "%s/tmp", workspace);
  makeDirectory(tempFolder);
  clearDirectory(tempFolder);
  setenv("TMPDIR", tempFolder, 1);

  // Relative paths are also resolved by the wrappers in vita_fs_wrap.cpp, so a
  // failing chdir is not fatal.
  const int chdirResult = chdir(workspace);
  const int chdirErrno = chdirResult != 0 ? errno : 0;
  if (chdirResult != 0)
    fprintf(stderr, "Vita: cannot enter %s: %s\n", workspace, strerror(chdirErrno));

  writeDiagnostics(workspace, failures, chdirResult, chdirErrno);
  return 0;
}

#endif  // VITA_BUILD
