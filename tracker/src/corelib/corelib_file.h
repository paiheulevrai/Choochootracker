#ifndef CORELIB_FILE_H
#define CORELIB_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

// Platform-specific file and directory utilities
// For general file I/O, use standard stdio.h functions

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_STR "\\"
#else
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_STR "/"
#endif

// Directory entry structure
struct FileEntry {
  char name[256];
  int isDirectory;
};

// Get platform-specific default directory for ChipNomad files
// Returns 0 on success, -1 on failure
int fileGetDefaultDirectory(char* buffer, int bufferSize);

// True when running from a mounted AppImage (checks the APPIMAGE env var
// the AppImage runtime sets). Desktop-only; always false elsewhere.
// fileGetDefaultDirectory() and common.cpp's default projectPath/samplePath/
// etc. both use this to avoid pointing into the AppImage's read-only mount.
int fileIsRunningFromAppImage(void);

// Opens Android's system document picker and copies the selected file into
// the private workspace. Other targets intentionally do nothing.
void fileImportDocument(const char* mimeType, const char* relativeDirectory);
void fileExportDocument(const char* path, const char* mimeType);

// Android only: opens the folder picker to choose (or change) the workspace
// folder, offering to copy the current files over. Other targets do nothing.
void filePickWorkingFolder(void);
// Android only: back to the app's private storage. Other targets do nothing.
void fileClearWorkingFolder(void);
// Fills buffer with a short status of the workspace folder. Returns 0 on
// success; "Unavailable" where the feature does not exist.
int fileGetWorkingFolderStatus(char* buffer, int bufferSize);

// Check if a directory exists
// Returns 1 if exists, 0 if not
int fileDirectoryExists(const char* path);

// Create a directory
// Returns 0 on success, -1 on failure
int fileCreateDirectory(const char* path);

// Create a directory tree, including all missing parent levels
// Returns 0 on success (or if the directory already exists), -1 on failure
int fileCreateDirectoryRecursive(const char* path);

// Rename (move) a file or directory
// Returns 0 on success, -1 on failure
int fileRename(const char* oldPath, const char* newPath);

// Delete a file
// Returns 0 on success, -1 on failure
int fileDelete(const char* path);

// List directory contents with optional extension filter
// Returns array of FileEntry (caller must free), or NULL on error
// entryCount is set to number of entries
FileEntry* fileListDirectory(const char* path, const char* extension, int* entryCount);

#ifdef __cplusplus
}
#endif

#endif // CORELIB_FILE_H
