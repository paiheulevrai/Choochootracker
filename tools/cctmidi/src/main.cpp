#include <chipnomad_lib.h>
#include <export/export_midi.h>
#include <import/import_midi.h>
#include <midi/smf_file.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hasExtension(const char* path, const char* ext) {
  size_t pathLen = strlen(path);
  size_t extLen = strlen(ext);
  if (pathLen < extLen) return 0;
  for (size_t i = 0; i < extLen; i++) {
    if (tolower((unsigned char)path[pathLen - extLen + i]) != tolower((unsigned char)ext[i])) return 0;
  }
  return 1;
}

static void printUsage(const char* argv0) {
  fprintf(stderr, "Usage: %s <input> <output>\n", argv0);
  fprintf(stderr, "  <input>.cct -> <output>.mid/.midi   export a project to a Standard MIDI File\n");
  fprintf(stderr, "  <input>.mid/.midi -> <output>.cct   import a MIDI file as a new project\n");
}

int main(int argc, char** argv) {
  if (argc != 3) {
    printUsage(argv[0]);
    return 1;
  }

  const char* input = argv[1];
  const char* output = argv[2];

  fillFXNames();

  if (hasExtension(input, ".cct") && (hasExtension(output, ".mid") || hasExtension(output, ".midi"))) {
    Project* project = (Project*)malloc(sizeof(Project));
    if (!project) return 1;
    projectInit(project);
    if (projectLoad(project, input) != 0) {
      fprintf(stderr, "Error loading '%s': %s\n", input, projectFileError);
      free(project);
      return 1;
    }
    int result = projectExportMidi(project, output);
    if (result != 0) {
      fprintf(stderr, "Error exporting to '%s': %s\n", output, smfFileError);
      projectFree(project);
      free(project);
      return 1;
    }
    projectFree(project);
    free(project);
    printf("Exported '%s' -> '%s'\n", input, output);
    return 0;
  }

  if ((hasExtension(input, ".mid") || hasExtension(input, ".midi")) && hasExtension(output, ".cct")) {
    Project* project = (Project*)malloc(sizeof(Project));
    if (!project) return 1;
    projectInit(project);
    if (projectLoadMidi(project, input) != 0) {
      fprintf(stderr, "Error loading '%s': %s\n", input, projectFileError);
      free(project);
      return 1;
    }
    int result = projectSave(project, output);
    if (result != 0) {
      fprintf(stderr, "Error saving '%s': %s\n", output, projectFileError);
      projectFree(project);
      free(project);
      return 1;
    }
    projectFree(project);
    free(project);
    printf("Imported '%s' -> '%s'\n", input, output);
    return 0;
  }

  fprintf(stderr, "Could not determine a conversion direction from the file extensions given.\n");
  printUsage(argv[0]);
  return 1;
}
