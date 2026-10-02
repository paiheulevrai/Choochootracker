#include "export_path.h"
#include "common.h"
#include "corelib/corelib_file.h"
#include "project_instruments.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#endif

// Folder used for exports while the project has no name yet
static const char* kCurrentProjectFolder = "current-project";

// Per-project bounce counter (1-based); reset on project load / new project
static int bounceCounter = 0;

// Formats the current counter value without advancing it
static void formatBounceNumber(char* buffer, int bufferSize, int number) {
  if (number > 999) number = 999;
  snprintf(buffer, bufferSize, "%03d", number);
}

void exportGetBaseDir(char* buffer, int bufferSize) {
  if (appSettings.exportPath[0]) {
    snprintf(buffer, bufferSize, "%s", appSettings.exportPath);
    return;
  }
#ifdef WEB_BUILD
  snprintf(buffer, bufferSize, "/user/samples/Exports");
#else
  char defaultDir[PATH_LENGTH];
  // Non-AppImage desktop builds keep the historical cwd-relative location:
  // samplePath defaults to relative "samples" (launcher scripts cd into the
  // install dir), so exports live next to it and sample paths stored
  // cwd-relative by sampleStorePath() keep matching after folder renames.
  // AppImages anchor to the writable seeded workspace instead (matches
  // initDefaultAppSettings()'s AppImage handling).
  int resolved;
  if (fileIsRunningFromAppImage()) {
    resolved = fileGetDefaultDirectory(defaultDir, PATH_LENGTH) == 0;
  } else {
    resolved = getcwd(defaultDir, PATH_LENGTH) != NULL;
  }
  if (!resolved) {
    snprintf(buffer, bufferSize, "samples%sExports", PATH_SEPARATOR_STR);
    return;
  }
  snprintf(buffer, bufferSize, "%s%ssamples%sExports", defaultDir, PATH_SEPARATOR_STR, PATH_SEPARATOR_STR);
#endif
}

void exportGetProjectFolderName(char* buffer, int bufferSize) {
  snprintf(buffer, bufferSize, "%s",
    appSettings.projectFilename[0] ? appSettings.projectFilename : kCurrentProjectFolder);
}

void exportGetProjectDir(char* buffer, int bufferSize) {
  char baseDir[EXPORT_PATH_MAX];
  exportGetBaseDir(baseDir, sizeof(baseDir));

  // A user-chosen folder is the exact export destination; the project
  // subfolder only applies to the default location scheme
  if (appSettings.exportPath[0]) {
    snprintf(buffer, bufferSize, "%s", baseDir);
    return;
  }

  char folderName[FILENAME_LENGTH + 32];
  exportGetProjectFolderName(folderName, sizeof(folderName));
  snprintf(buffer, bufferSize, "%s%s%s", baseDir, PATH_SEPARATOR_STR, folderName);
}

// Ensures the export destination exists. Under the default scheme it also
// records the folder name so a later project save can rename it.
int exportEnsureProjectDir(void) {
  char projectDir[EXPORT_PATH_MAX];
  exportGetProjectDir(projectDir, sizeof(projectDir));
  if (fileCreateDirectoryRecursive(projectDir) != 0) return -1;

  if (!appSettings.exportPath[0]) {
    char folderName[FILENAME_LENGTH + 32];
    exportGetProjectFolderName(folderName, sizeof(folderName));
    strncpy(appSettings.exportLastFolder, folderName, FILENAME_LENGTH);
    appSettings.exportLastFolder[FILENAME_LENGTH] = 0;
  }
  return 0;
}

int exportBuildFilePath(char* outputPath, int maxLen, const char* name, const char* extension) {
  if (name == NULL || name[0] == 0) return -1;

  char projectDir[EXPORT_PATH_MAX];
  exportGetProjectDir(projectDir, sizeof(projectDir));

  if (extension && extension[0]) {
    snprintf(outputPath, maxLen, "%s%s%s.%s", projectDir, PATH_SEPARATOR_STR, name, extension);
  } else {
    snprintf(outputPath, maxLen, "%s%s%s", projectDir, PATH_SEPARATOR_STR, name);
  }

  // Check for collision with an existing file
  FILE* file = fopen(outputPath, "r");
  if (file == NULL) return 0;
  fclose(file);

  for (int i = 1; i <= 999; i++) {
    if (extension && extension[0]) {
      snprintf(outputPath, maxLen, "%s%s%s_%03d.%s", projectDir, PATH_SEPARATOR_STR, name, i, extension);
    } else {
      snprintf(outputPath, maxLen, "%s%s%s_%03d", projectDir, PATH_SEPARATOR_STR, name, i);
    }
    FILE* probe = fopen(outputPath, "r");
    if (probe == NULL) return 0;
    fclose(probe);
  }
  return -1;
}

int exportSyncFolderWithProjectName(void) {
  char folderName[FILENAME_LENGTH + 32];
  exportGetProjectFolderName(folderName, sizeof(folderName));

  // Rename-on-save only applies to the default location scheme
  if (appSettings.exportPath[0]) return 0;
  if (!appSettings.projectFilename[0]) return 0;
  if (strcmp(folderName, appSettings.exportLastFolder) == 0) return 0;

  char baseDir[EXPORT_PATH_MAX];
  exportGetBaseDir(baseDir, sizeof(baseDir));

  if (appSettings.exportLastFolder[0] == 0) {
    // Tracking was lost (settings reset, exports made before tracking
    // existed, or the app restarted). Recover by renaming the on-disk
    // "current-project" folder when it exists, so exports made while the
    // project was unnamed still follow the project name after a save.
    char currentPath[EXPORT_PATH_MAX + FILENAME_LENGTH + 32];
    char newPath[EXPORT_PATH_MAX + FILENAME_LENGTH + 32];
    snprintf(currentPath, sizeof(currentPath), "%s%s%s", baseDir, PATH_SEPARATOR_STR, kCurrentProjectFolder);
    snprintf(newPath, sizeof(newPath), "%s%s%s", baseDir, PATH_SEPARATOR_STR, folderName);

    if (strcmp(kCurrentProjectFolder, folderName) != 0 && fileDirectoryExists(currentPath) &&
        !fileDirectoryExists(newPath)) {
      if (fileRename(currentPath, newPath) == 0 && chipnomadState) {
        exportRefreshSamplePaths(&chipnomadState->project, baseDir, kCurrentProjectFolder, folderName);
      }
    }
    strncpy(appSettings.exportLastFolder, folderName, FILENAME_LENGTH);
    appSettings.exportLastFolder[FILENAME_LENGTH] = 0;
    return 0;
  }

  char oldPath[EXPORT_PATH_MAX + FILENAME_LENGTH + 32];
  char newPath[EXPORT_PATH_MAX + FILENAME_LENGTH + 32];
  snprintf(oldPath, sizeof(oldPath), "%s%s%s", baseDir, PATH_SEPARATOR_STR, appSettings.exportLastFolder);
  snprintf(newPath, sizeof(newPath), "%s%s%s", baseDir, PATH_SEPARATOR_STR, folderName);

  if (!fileDirectoryExists(oldPath)) {
    // Folder was never created (or already gone); just adopt the new name
    strncpy(appSettings.exportLastFolder, folderName, FILENAME_LENGTH);
    appSettings.exportLastFolder[FILENAME_LENGTH] = 0;
    return 0;
  }

  if (fileDirectoryExists(newPath)) {
    // Target already exists; keep both folders and adopt the new name going
    // forward rather than merging or overwriting user data
    strncpy(appSettings.exportLastFolder, folderName, FILENAME_LENGTH);
    appSettings.exportLastFolder[FILENAME_LENGTH] = 0;
    return 0;
  }

  int result = fileRename(oldPath, newPath);
  if (result == 0 && chipnomadState) {
    exportRefreshSamplePaths(&chipnomadState->project, baseDir, appSettings.exportLastFolder, folderName);
  }
  strncpy(appSettings.exportLastFolder, folderName, FILENAME_LENGTH);
  appSettings.exportLastFolder[FILENAME_LENGTH] = 0;
  return result;
}

void exportResetFolderTracking(void) {
  appSettings.exportLastFolder[0] = 0;
  bounceCounter = 0;
}

// Proposes the next bounce file name for the file name field: the next
// sequence number that is not already taken in the export folder (001.wav,
// 002.wav, ...). The proposal is a plain name, not a suffix - the user can
// edit it freely before starting the bounce.
void exportProposeBounceName(char* buffer, int bufferSize) {
  char projectDir[EXPORT_PATH_MAX];
  exportGetProjectDir(projectDir, sizeof(projectDir));

  // bounceCounter counts claimed bounces, so the next candidate is +1
  char candidate[8];
  int number = bounceCounter + 1;
  if (number > 999) number = 999;
  for (;;) {
    formatBounceNumber(candidate, sizeof(candidate), number);
    char path[EXPORT_PATH_MAX + 16];
    snprintf(path, sizeof(path), "%s%s%s.wav", projectDir, PATH_SEPARATOR_STR, candidate);
    FILE* probe = fopen(path, "r");
    if (probe == NULL) break; // Free
    fclose(probe);
    if (number >= 999) break; // All sequence numbers taken
    number++;
  }
  snprintf(buffer, bufferSize, "%s", candidate);
}

// Records a bounce name for the per-project sequence. A plain sequence
// number ("001") advances the counter past it; a custom name leaves the
// counter untouched. Collision handling (_001.._999 suffixes) is done by
// exportBuildFilePath when the file is written.
void exportClaimBounceName(const char* name) {
  char proposed[8];
  formatBounceNumber(proposed, sizeof(proposed), bounceCounter + 1);
  if (strcmp(name, proposed) == 0) {
    if (bounceCounter < 999) bounceCounter++;
  }
}

// Rewrites sample paths that live under <baseDir>/<oldFolder>/ so they point
// at <baseDir>/<newFolder>/ after the export folder was renamed. Sample
// paths are stored relative to the working directory (sampleStorePath strips
// the CWD prefix), so the base directory is compared both with and without
// its CWD prefix.
void exportRefreshSamplePaths(struct Project* project, const char* baseDir,
                              const char* oldFolder, const char* newFolder) {
  if (project == NULL || baseDir == NULL || oldFolder == NULL || newFolder == NULL) return;
  if (!oldFolder[0] || !newFolder[0]) return;
  if (strcmp(oldFolder, newFolder) == 0) return;

  // Build the two prefix variants to match: CWD-relative and absolute
  char relBase[EXPORT_PATH_MAX];
  char absBase[EXPORT_PATH_MAX];
  relBase[0] = 0;
  absBase[0] = 0;

  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd))) {
    size_t cwdLen = strlen(cwd);
    size_t baseLen = strlen(baseDir);
    if (baseLen > cwdLen && strncmp(baseDir, cwd, cwdLen) == 0 &&
        (baseDir[cwdLen] == '/' || baseDir[cwdLen] == '\\')) {
      // baseDir is under the CWD: also match the CWD-relative form
      snprintf(relBase, sizeof(relBase), "%s", baseDir + cwdLen + 1);
    }
  }
  if (baseDir[0] == '/' || baseDir[0] == '\\') {
    snprintf(absBase, sizeof(absBase), "%s", baseDir);
  }

  char oldRelPrefix[EXPORT_PATH_MAX + FILENAME_LENGTH + 32];
  char oldAbsPrefix[EXPORT_PATH_MAX + FILENAME_LENGTH + 32];
  const char* prefixes[2] = {NULL, NULL};
  int prefixCount = 0;
  if (relBase[0]) {
    snprintf(oldRelPrefix, sizeof(oldRelPrefix), "%s%s%s%s", relBase, PATH_SEPARATOR_STR, oldFolder, PATH_SEPARATOR_STR);
    prefixes[prefixCount++] = oldRelPrefix;
  }
  if (absBase[0]) {
    snprintf(oldAbsPrefix, sizeof(oldAbsPrefix), "%s%s%s%s", absBase, PATH_SEPARATOR_STR, oldFolder, PATH_SEPARATOR_STR);
    prefixes[prefixCount++] = oldAbsPrefix;
  }
  if (prefixCount == 0) return;

  for (int i = 0; i < PROJECT_MAX_INSTRUMENTS; ++i) {
    Instrument* instrument = &project->instruments[i];
    InstrumentSample* samples[2] = {NULL, NULL};
    int count = 0;
    if (instrument->type == InstrumentType::Sample) {
      samples[count++] = &instrument->chip.sample;
    } else if (instrument->type == InstrumentType::SCWF) {
      samples[count++] = &instrument->chip.scwf.oscillator[0];
      samples[count++] = &instrument->chip.scwf.oscillator[1];
    } else if (instrument->type == InstrumentType::BYOWTBL) {
      samples[count++] = &instrument->chip.byowtbl.oscillator[0];
      samples[count++] = &instrument->chip.byowtbl.oscillator[1];
    }
    for (int j = 0; j < count; ++j) {
      InstrumentSample* sample = samples[j];
      if (!sample->path[0]) continue;
      for (int k = 0; k < prefixCount; ++k) {
        size_t prefixLen = strlen(prefixes[k]);
        if (strncmp(sample->path, prefixes[k], prefixLen) != 0) continue;
        // Rebuild the path with the new folder segment
        char rebuilt[PROJECT_SAMPLE_PATH_LENGTH + 1];
        const char* base = (prefixes[k] == oldRelPrefix) ? relBase : absBase;
        int written = snprintf(rebuilt, sizeof(rebuilt), "%s%s%s%s%s", base, PATH_SEPARATOR_STR, newFolder,
                               PATH_SEPARATOR_STR, sample->path + prefixLen);
        if (written < 0 || written >= (int)sizeof(rebuilt)) break;
        strncpy(sample->path, rebuilt, PROJECT_SAMPLE_PATH_LENGTH);
        sample->path[PROJECT_SAMPLE_PATH_LENGTH] = 0;
        break;
      }
    }
  }
}
