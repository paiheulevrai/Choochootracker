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

}  // namespace

int assetsInit(void) {
  char workspace[PATH_LENGTH_VITA];
  if (fileGetDefaultDirectory(workspace, sizeof(workspace)) != 0) {
    fprintf(stderr, "Cannot create the Vita data folder\n");
    return 1;
  }

  if (isDirectory(kBundledAssets)) {
    const int failures = copyTreeIfMissing(kBundledAssets, workspace);
    if (failures)
      fprintf(stderr, "Vita: %d bundled asset(s) could not be copied to %s\n", failures, workspace);
  }

  // Relative paths in settings.txt (themes, fonts, ...) resolve from here.
  // A failure is not fatal: the absolute workspace path is used elsewhere.
  if (chdir(workspace) != 0)
    fprintf(stderr, "Vita: cannot enter %s: %s\n", workspace, strerror(errno));
  return 0;
}

#endif  // VITA_BUILD
