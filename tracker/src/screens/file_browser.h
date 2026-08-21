#ifndef __FILE_BROWSER_H__
#define __FILE_BROWSER_H__

#ifdef __cplusplus
extern "C" {
#endif

// Setup file browser with extension filter and callbacks
void fileBrowserSetup(const char* title, const char* extension, const char* startPath, void (*fileCallback)(const char*), void (*cancelCallback)(void));
void fileBrowserSetupWithPreview(const char* title, const char* extension,
  const char* startPath, void (*fileCallback)(const char*),
  void (*cancelCallback)(void), void (*previewCallback)(const char*));
void fileBrowserSetupFolderMode(const char* title, const char* startPath, const char* filename, const char* extension, void (*folderCallback)(const char*), void (*cancelCallback)(void));

void fileBrowserSetPath(const char* path);
int fileBrowserGetAdjacentPath(const char* currentPath, const char* extension,
  int direction, char* outputPath, int outputPathSize);


#ifdef __cplusplus
}
#endif

#endif
