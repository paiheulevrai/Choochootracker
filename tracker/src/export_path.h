#ifndef __EXPORT_PATH_H__
#define __EXPORT_PATH_H__

#ifdef __cplusplus
extern "C" {
#endif

// Export location scheme:
//   <app folder>/samples/Exports/<project name or "current-project">/<file>
// The app folder is the platform default directory (the build folder on
// desktop, /user on web, the app workspace on Android). The samples folder
// is the app's preexisting samples directory. A custom folder can be set in
// the export screen; it replaces the default root entirely.

// Maximum length of a generated export path
#define EXPORT_PATH_MAX (1024)

// Root folder for exports: the custom path when set, otherwise
// <default directory>/samples/Exports
void exportGetBaseDir(char* buffer, int bufferSize);

// Project export folder: base dir + "/" + project name (or "current-project"
// when the project has no name yet)
void exportGetProjectDir(char* buffer, int bufferSize);

// Name of the folder used for the current project ("current-project" when
// unnamed)
void exportGetProjectFolderName(char* buffer, int bufferSize);

// Ensure the project export folder exists (created recursively)
// Returns 0 on success, -1 on failure
int exportEnsureProjectDir(void);

// Build a collision-free file path inside the project export folder:
// <projectDir>/<name>.<extension>, adding _001.._999 suffixes on collision.
// Returns 0 on success, -1 if the name is empty or all suffixes are taken.
int exportBuildFilePath(char* outputPath, int maxLen, const char* name, const char* extension);

// After a project is saved under a new name, rename the previously used
// export folder to match. Only renames when the old folder exists and the
// name actually changed. Returns 0 when the folder now matches the project
// name (or nothing needed renaming), -1 when the rename failed.
int exportSyncFolderWithProjectName(void);

// Forget the tracked previous folder name (used when a project is loaded or
// a new project is created)
void exportResetFolderTracking(void);

// Proposes the next bounce file name for the bounce screen's file name
// field: the next sequence number ("001", "002", ...) whose .wav file does
// not exist yet in the export folder. The proposal is a plain name the user
// can edit freely - it is not a mandatory suffix.
void exportProposeBounceName(char* buffer, int bufferSize);

// Records a bounce name when a bounce starts. A plain sequence number
// ("001") advances the per-project counter; a custom name leaves it
// untouched. File collisions are resolved with _001.._999 suffixes when the
// file is written (exportBuildFilePath).
void exportClaimBounceName(const char* name);

// Rewrite instrument sample paths after the export folder was renamed, so
// loaded samples keep pointing at the moved files. Matches paths under
// <baseDir>/<oldFolder>/ and replaces the folder segment with newFolder.
void exportRefreshSamplePaths(struct Project* project, const char* baseDir,
                              const char* oldFolder, const char* newFolder);

#ifdef __cplusplus
}
#endif

#endif // __EXPORT_PATH_H__
