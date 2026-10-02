#include "doctest.h"
#include "common.h"
#include "corelib/corelib_file.h"
#include "export_path.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

TEST_SUITE("export_path") {

// Isolated settings for each test; restores the previous state afterwards
struct ExportPathFixture {
  AppSettings saved;
  char workDir[512];
  char savedCwd[512];

  ExportPathFixture() {
    saved = appSettings;
    appSettings.projectFilename[0] = 0;
    appSettings.exportPath[0] = 0;
    appSettings.exportLastFolder[0] = 0;
    snprintf(workDir, sizeof(workDir), "/tmp/choochoo_export_test_%d", (int)getpid());
    // Start from a clean slate
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", workDir);
    system(cmd);
    // The default export base dir is derived from the process working
    // directory, so run the test from inside the work dir
    char* savedCwdResult = getcwd(savedCwd, sizeof(savedCwd));
    REQUIRE(savedCwdResult != NULL);
    REQUIRE(mkdir(workDir, 0755) == 0);
    REQUIRE(chdir(workDir) == 0);
  }

  ~ExportPathFixture() {
    chdir(savedCwd);
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", workDir);
    system(cmd);
    appSettings = saved;
  }

  void setCustomPath(const char* path) {
    snprintf(appSettings.exportPath, PATH_LENGTH, "%s", path);
  }
};

static bool dirExists(const char* path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool fileExists(const char* path) {
  FILE* f = fopen(path, "r");
  if (!f) return false;
  fclose(f);
  return true;
}

TEST_CASE_FIXTURE(ExportPathFixture, "Default base dir is <cwd>/samples/Exports") {
  char base[EXPORT_PATH_MAX];
  exportGetBaseDir(base, sizeof(base));
  char expected[EXPORT_PATH_MAX];
  snprintf(expected, sizeof(expected), "%s/samples/Exports", workDir);
  CHECK(strcmp(base, expected) == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "Custom export path overrides the base dir") {
  setCustomPath("/tmp/my_custom_exports");
  char base[EXPORT_PATH_MAX];
  exportGetBaseDir(base, sizeof(base));
  CHECK(strcmp(base, "/tmp/my_custom_exports") == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "Unnamed project uses current-project folder") {
  appSettings.projectFilename[0] = 0;
  char dir[EXPORT_PATH_MAX];
  exportGetProjectDir(dir, sizeof(dir));
  char expected[EXPORT_PATH_MAX];
    snprintf(expected, sizeof(expected), "%s/samples/Exports/current-project", workDir);
  CHECK(strcmp(dir, expected) == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "Named project uses the project name as folder") {
  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "mysong");
  char dir[EXPORT_PATH_MAX];
  exportGetProjectDir(dir, sizeof(dir));
  char expected[EXPORT_PATH_MAX];
    snprintf(expected, sizeof(expected), "%s/samples/Exports/mysong", workDir);
  CHECK(strcmp(dir, expected) == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "Custom path is used directly without project subfolder") {
  setCustomPath(workDir);
  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "mysong");
  char dir[EXPORT_PATH_MAX];
  exportGetProjectDir(dir, sizeof(dir));
  CHECK(strcmp(dir, workDir) == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportEnsureProjectDir creates nested folders") {
  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "deep");
  char expected[600];
  snprintf(expected, sizeof(expected), "%s/samples/Exports/deep", workDir);
  CHECK(dirExists(expected) == false);
  CHECK(exportEnsureProjectDir() == 0);
  CHECK(dirExists(expected));
  // Second call is a no-op success
  CHECK(exportEnsureProjectDir() == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportEnsureProjectDir tracks the folder name") {
  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "tracked");
  CHECK(exportEnsureProjectDir() == 0);
  CHECK(strcmp(appSettings.exportLastFolder, "tracked") == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportEnsureProjectDir does not track with custom path") {
  setCustomPath(workDir);
  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "tracked");
  CHECK(exportEnsureProjectDir() == 0);
  CHECK(appSettings.exportLastFolder[0] == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportBuildFilePath builds name.extension in project dir") {
  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "song");
  char path[EXPORT_PATH_MAX];
  CHECK(exportBuildFilePath(path, sizeof(path), "bounce", "wav") == 0);
  char expected[EXPORT_PATH_MAX];
  snprintf(expected, sizeof(expected), "%s/samples/Exports/song/bounce.wav", workDir);
  CHECK(strcmp(path, expected) == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportBuildFilePath rejects empty names") {
  char path[EXPORT_PATH_MAX];
  CHECK(exportBuildFilePath(path, sizeof(path), "", "wav") == -1);
  CHECK(exportBuildFilePath(path, sizeof(path), NULL, "wav") == -1);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportBuildFilePath adds _001 suffix on collision") {
  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "coll");
  CHECK(exportEnsureProjectDir() == 0);
  char path[EXPORT_PATH_MAX];
  CHECK(exportBuildFilePath(path, sizeof(path), "take", "wav") == 0);
  FILE* f = fopen(path, "w");
  REQUIRE(f != NULL);
  fputs("x", f);
  fclose(f);

  CHECK(exportBuildFilePath(path, sizeof(path), "take", "wav") == 0);
  const char* suffix1 = "take_001.wav";
  CHECK(strcmp(path + strlen(path) - strlen(suffix1), suffix1) == 0);

  // Create the _001 file too; next free suffix is _002
  f = fopen(path, "w");
  REQUIRE(f != NULL);
  fputs("x", f);
  fclose(f);
  CHECK(exportBuildFilePath(path, sizeof(path), "take", "wav") == 0);
  const char* suffix2 = "take_002.wav";
  CHECK(strcmp(path + strlen(path) - strlen(suffix2), suffix2) == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportSyncFolderWithProjectName renames current-project") {
  // Bounce while unnamed: creates samples/Exports/current-project
  CHECK(exportEnsureProjectDir() == 0);
  char oldDir[600];
  snprintf(oldDir, sizeof(oldDir), "%s/samples/Exports/current-project", workDir);
  CHECK(dirExists(oldDir));

  // Save the project under a name: folder must be renamed
  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "renamed");
  CHECK(exportSyncFolderWithProjectName() == 0);
  CHECK(dirExists(oldDir) == false);
  char newDir[600];
  snprintf(newDir, sizeof(newDir), "%s/samples/Exports/renamed", workDir);
  CHECK(dirExists(newDir));
  CHECK(strcmp(appSettings.exportLastFolder, "renamed") == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportSyncFolderWithProjectName recovers lost tracking") {
  // Exports were made while the project was unnamed, but the tracked name
  // was lost (settings reset / app restart): the on-disk current-project
  // folder must still be renamed on save
  CHECK(exportEnsureProjectDir() == 0);
  char oldDir[600];
  snprintf(oldDir, sizeof(oldDir), "%s/samples/Exports/current-project", workDir);
  CHECK(dirExists(oldDir));

  // Simulate lost tracking
  appSettings.exportLastFolder[0] = 0;

  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "recovered");
  CHECK(exportSyncFolderWithProjectName() == 0);
  CHECK(dirExists(oldDir) == false);
  char newDir[600];
  snprintf(newDir, sizeof(newDir), "%s/samples/Exports/recovered", workDir);
  CHECK(dirExists(newDir));
  CHECK(strcmp(appSettings.exportLastFolder, "recovered") == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportSyncFolderWithProjectName lost tracking keeps existing target") {
  CHECK(exportEnsureProjectDir() == 0);
  char oldDir[600];
  snprintf(oldDir, sizeof(oldDir), "%s/samples/Exports/current-project", workDir);
  CHECK(dirExists(oldDir));

  // The target folder already exists: keep both, adopt the name
  char target[600];
  snprintf(target, sizeof(target), "%s/samples/Exports/exists", workDir);
  REQUIRE(fileCreateDirectoryRecursive(target) == 0);

  appSettings.exportLastFolder[0] = 0;
  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "exists");
  CHECK(exportSyncFolderWithProjectName() == 0);
  CHECK(dirExists(oldDir)); // Not merged or deleted
  CHECK(dirExists(target));
  CHECK(strcmp(appSettings.exportLastFolder, "exists") == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportRefreshSamplePaths rewrites matching paths") {
  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "song");
  char base[EXPORT_PATH_MAX];
  exportGetBaseDir(base, sizeof(base));

  Project project;
  projectInit(&project);

  // A sample under the old folder (CWD-relative, as sampleStorePath stores it)
  char relPath[256];
  snprintf(relPath, sizeof(relPath), "samples/Exports/old/take.wav");
  project.instruments[0].type = InstrumentType::Sample;
  snprintf(project.instruments[0].chip.sample.path, PROJECT_SAMPLE_PATH_LENGTH + 1, "%s", relPath);

  // An SCWF oscillator under the old folder
  project.instruments[1].type = InstrumentType::SCWF;
  snprintf(project.instruments[1].chip.scwf.oscillator[0].path, PROJECT_SAMPLE_PATH_LENGTH + 1, "%s", relPath);
  snprintf(project.instruments[1].chip.scwf.oscillator[1].path, PROJECT_SAMPLE_PATH_LENGTH + 1, "%s", relPath);

  // A path outside the folder: must stay untouched
  project.instruments[2].type = InstrumentType::Sample;
  snprintf(project.instruments[2].chip.sample.path, PROJECT_SAMPLE_PATH_LENGTH + 1, "samples/other/keep.wav");

  exportRefreshSamplePaths(&project, base, "old", "new");

  char expected[256];
  snprintf(expected, sizeof(expected), "samples/Exports/new/take.wav");
  CHECK(strcmp(project.instruments[0].chip.sample.path, expected) == 0);
  CHECK(strcmp(project.instruments[1].chip.scwf.oscillator[0].path, expected) == 0);
  CHECK(strcmp(project.instruments[1].chip.scwf.oscillator[1].path, expected) == 0);
  CHECK(strcmp(project.instruments[2].chip.sample.path, "samples/other/keep.wav") == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportRefreshSamplePaths handles absolute paths") {
  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "song");
  char base[EXPORT_PATH_MAX];
  exportGetBaseDir(base, sizeof(base));

  Project project;
  projectInit(&project);

  char absPath[512];
  snprintf(absPath, sizeof(absPath), "%s/old/take.wav", base);
  project.instruments[0].type = InstrumentType::Sample;
  snprintf(project.instruments[0].chip.sample.path, PROJECT_SAMPLE_PATH_LENGTH + 1, "%s", absPath);

  exportRefreshSamplePaths(&project, base, "old", "new");

  char expected[512];
  snprintf(expected, sizeof(expected), "%s/new/take.wav", base);
  CHECK(strcmp(project.instruments[0].chip.sample.path, expected) == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportProposeBounceName skips taken numbers") {
  CHECK(exportEnsureProjectDir() == 0);
  char name[32];
  exportProposeBounceName(name, sizeof(name));
  CHECK(strcmp(name, "001") == 0);

  // 001.wav exists on disk: the proposal advances to the next free number
  char filePath[700];
  snprintf(filePath, sizeof(filePath), "%s/samples/Exports/current-project/001.wav", workDir);
  FILE* f = fopen(filePath, "w");
  REQUIRE(f != NULL);
  fputs("data", f);
  fclose(f);

  exportProposeBounceName(name, sizeof(name));
  CHECK(strcmp(name, "002") == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportClaimBounceName advances only for sequence names") {
  char name[32];
  exportProposeBounceName(name, sizeof(name));
  CHECK(strcmp(name, "001") == 0);

  // Claiming the proposed sequence number advances the counter
  exportClaimBounceName("001");
  exportProposeBounceName(name, sizeof(name));
  CHECK(strcmp(name, "002") == 0);

  // A custom name leaves the counter untouched
  exportClaimBounceName("my-take");
  exportProposeBounceName(name, sizeof(name));
  CHECK(strcmp(name, "002") == 0);

  // Loading a project / creating a new one resets the counter
  exportResetFolderTracking();
  exportProposeBounceName(name, sizeof(name));
  CHECK(strcmp(name, "001") == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportSyncFolderWithProjectName keeps files across rename") {
  CHECK(exportEnsureProjectDir() == 0);
 char filePath[700];
  snprintf(filePath, sizeof(filePath), "%s/samples/Exports/current-project/keep.wav", workDir);
  FILE* f = fopen(filePath, "w");
  REQUIRE(f != NULL);
  fputs("data", f);
  fclose(f);

  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "final");
  CHECK(exportSyncFolderWithProjectName() == 0);
  // The file must survive the rename under the new folder name
  char movedPath[700];
  snprintf(movedPath, sizeof(movedPath), "%s/samples/Exports/final/keep.wav", workDir);
  CHECK(fileExists(movedPath));
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportSyncFolderWithProjectName no-ops without prior folder") {
  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "fresh");
  CHECK(exportSyncFolderWithProjectName() == 0);
  CHECK(strcmp(appSettings.exportLastFolder, "fresh") == 0);
  char dir[600];
  snprintf(dir, sizeof(dir), "%s/samples/Exports/fresh", workDir);
  CHECK(dirExists(dir) == false); // Nothing was created
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportSyncFolderWithProjectName no-ops when names match") {
  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "same");
  strncpy(appSettings.exportLastFolder, "same", FILENAME_LENGTH);
  CHECK(exportSyncFolderWithProjectName() == 0);
  char dir[600];
  snprintf(dir, sizeof(dir), "%s/samples/Exports/same", workDir);
  CHECK(dirExists(dir) == false);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportSyncFolderWithProjectName no-ops with custom path") {
  setCustomPath(workDir);
  strncpy(appSettings.exportLastFolder, "old", FILENAME_LENGTH);
  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "newname");
  CHECK(exportSyncFolderWithProjectName() == 0);
  // Tracking is untouched: custom path never renames
  CHECK(strcmp(appSettings.exportLastFolder, "old") == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportSyncFolderWithProjectName keeps both when target exists") {
  CHECK(exportEnsureProjectDir() == 0); // creates current-project, tracks it
  // Pre-create the target folder with different content
  char target[600];
  snprintf(target, sizeof(target), "%s/samples/Exports/other", workDir);
  REQUIRE(fileCreateDirectoryRecursive(target) == 0);
  char marker[700];
  snprintf(marker, sizeof(marker), "%s/marker.txt", target);
  FILE* f = fopen(marker, "w");
  REQUIRE(f != NULL);
  fputs("keep", f);
  fclose(f);

  snprintf(appSettings.projectFilename, FILENAME_LENGTH + 1, "other");
  CHECK(exportSyncFolderWithProjectName() == 0);
  // Old folder still exists (not merged, not deleted)
  char oldDir[600];
  snprintf(oldDir, sizeof(oldDir), "%s/samples/Exports/current-project", workDir);
  CHECK(dirExists(oldDir));
  CHECK(fileExists(marker));
  CHECK(strcmp(appSettings.exportLastFolder, "other") == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "exportResetFolderTracking clears the tracked name") {
  strncpy(appSettings.exportLastFolder, "stale", FILENAME_LENGTH);
  exportResetFolderTracking();
  CHECK(appSettings.exportLastFolder[0] == 0);
}

TEST_CASE_FIXTURE(ExportPathFixture, "fileRename moves a directory") {
  char src[600], dst[600];
  snprintf(src, sizeof(src), "%s/rename_src", workDir);
  snprintf(dst, sizeof(dst), "%s/rename_dst", workDir);
  REQUIRE(fileCreateDirectoryRecursive(src) == 0);
  char inner[700];
  snprintf(inner, sizeof(inner), "%s/file.txt", src);
  FILE* f = fopen(inner, "w");
  REQUIRE(f != NULL);
  fputs("x", f);
  fclose(f);

  CHECK(fileRename(src, dst) == 0);
  CHECK(dirExists(src) == false);
  CHECK(dirExists(dst));
  char movedFile[700];
  snprintf(movedFile, sizeof(movedFile), "%s/file.txt", dst);
  CHECK(fileExists(movedFile));
}

TEST_CASE_FIXTURE(ExportPathFixture, "fileRename fails on missing source") {
  char dst[600];
  snprintf(dst, sizeof(dst), "%s/nothing_dst", workDir);
  CHECK(fileRename("/tmp/choochoo_export_test_missing_dir_xyz", dst) == -1);
}

TEST_CASE_FIXTURE(ExportPathFixture, "fileCreateDirectoryRecursive handles existing and deep paths") {
  char deep[700];
  snprintf(deep, sizeof(deep), "%s/a/b/c/d", workDir);
  CHECK(fileCreateDirectoryRecursive(deep) == 0);
  CHECK(dirExists(deep));
  // Already exists: still success
  CHECK(fileCreateDirectoryRecursive(deep) == 0);
  // Empty path: failure
  CHECK(fileCreateDirectoryRecursive("") == -1);
  CHECK(fileCreateDirectoryRecursive(NULL) == -1);
}

}
