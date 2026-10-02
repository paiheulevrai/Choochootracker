#include "screen_export.h"
#include "common.h"
#include "corelib_gfx.h"
#include "corelib/corelib_file.h"
#include "chipnomad_lib.h"
#include "screens.h"
#include "file_browser.h"
#include "export_path.h"
#include "export/export.h"
#include "export/export_midi.h"
#include "midi/smf_file.h"
#include <string.h>

#ifdef WEB_BUILD
#include <emscripten/emscripten.h>
EM_JS(void, webDownloadExportFile, (const char* path), {
  if (window.choochooDownloadFile) window.choochooDownloadFile(UTF8ToString(path));
});
#endif

// Export state
Exporter* currentExporter = NULL;
static char currentExportPath[1024];
static int currentExportIsStems = 0;
static int currentExportTrackCount = 0;
static int currentExportIsBounce = 0;
static ExportSelection pendingBounceSelection;
static const AppScreen* bounceReturnScreen = NULL;
static void bounceReturnToOrigin(void);

static void drawRowHeader(int row, CellState state) {}
static void drawColHeader(int col, CellState state) {}
static void drawSelection(int col1, int row1, int col2, int row2) {}

static int sampleRates[] = {44100, 48000, 88200, 96000};
static int bitDepths[] = {16, 24, 32};
static int currentSampleRateIndex = 0;
static int currentBitDepthIndex = 0;
int startRow = 0;

static ScreenData screenExportCommon = {
  .rows = SCR_EXPORT_ROWS,
  .cursorRow = 0,
  .cursorCol = 0,
  .topRow = 0,
  .selectMode = -1,
  .selectStartRow = 0,
  .selectStartCol = 0,
  .selectAnchorRow = 0,
  .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = exportCommonColumnCount,
  .drawStatic = exportCommonDrawStatic,
  .drawCursor = exportCommonDrawCursor,
  .drawSelection = drawSelection,
  .drawRowHeader = drawRowHeader,
  .drawColHeader = drawColHeader,
  .drawField = exportCommonDrawField,
  .onEdit = exportCommonOnEdit,
  .onInput = NULL,
  .onRawInput = NULL,
  .isCellValid = NULL,
  .getLoopRange = NULL,
};

static ScreenData* exportScreen(void) {
  return &screenExportCommon;
}

static void setup(int input) {
  currentSampleRateIndex = 0;
  currentBitDepthIndex = 0;
  startRow = 0;
}

static void fullRedraw(void) {
  ScreenData* screen = exportScreen();
  screenFullRedraw(screen);
}

static void exportPump(void) {
  if (currentExporter) {
    int seconds = currentExporter->next();
    if (seconds == -1) {
      if (currentExporter->finish() == 0) {
        screenMessage(MESSAGE_TIME, currentExportIsBounce ? "Bounce completed" : "Export completed");
#ifdef WEB_BUILD
        if (currentExportIsStems) {
          for (int i = 0; i < currentExportTrackCount; i++) {
            char path[1100];
            snprintf(path, sizeof(path), "%s-%02d.wav", currentExportPath, i + 1);
            webDownloadExportFile(path);
          }
        } else {
          webDownloadExportFile(currentExportPath);
        }
#endif
#ifdef ANDROID_BUILD
        if (!currentExportIsStems) fileExportDocument(currentExportPath, "audio/wav");
#endif
      } else {
        screenMessage(MESSAGE_TIME, currentExportIsBounce ? "Bounce failed" : "Export failed");
      }
      delete currentExporter;
      currentExporter = NULL;
      // A finished bounce returns to the screen it was triggered from
      if (currentExportIsBounce) bounceReturnToOrigin();
    } else {
      screenMessage(MESSAGE_TIME, currentExportIsBounce ? "Bouncing... %ds. OPT to cancel" : "Exporting... %ds. OPT to cancel", seconds);
    }
  }
}

static void draw(void) {
  exportPump();
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (currentExporter) {
    if (keys == keyOpt) {
      currentExporter->cancel();
      delete currentExporter;
      currentExporter = NULL;
      screenMessage(MESSAGE_TIME, currentExportIsBounce ? "Bounce cancelled" : "Export cancelled");
      // A cancelled bounce returns to the screen it was triggered from
      if (currentExportIsBounce) bounceReturnToOrigin();
    }
    return 1; // Block all other input during export
  }

  if (keys == keyOpt) {
    screenSetup(&screenProject, 0);
    return 1;
  }

  ScreenData* screen = exportScreen();
  return screenInput(screen, isKeyDown, keys, tapCount);
}

const AppScreen screenExport = {
  .init = NULL,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = NULL,
};

///////////////////////////////////////////////////////////////////////////////
//
// Common part of the export screen
//

int exportCommonColumnCount(int row) {
  if (row == 0) {
    return 1;
  } else if (row == 1) {
    return 2;
  } else if (row == 2) {
    return 1;
  } else if (row == 3) {
    return 1;
  } else if (row == 4) {
    return 1;
  } else if (row == 5) {
    return 1;
  }
  return 0;
}

void exportCommonDrawStatic(void) {
  const ColorScheme cs = appSettings.colorScheme;

  gfxSetFgColor(cs.textTitles);
  gfxPrint(0, 0, "EXPORT");

  gfxSetFgColor(cs.textDefault);
  gfxPrint(0, 2, "Start row");

  gfxSetFgColor(cs.textValue);
  gfxPrint(0, 4, "WAV");
  gfxSetFgColor(cs.textDefault);
  gfxPrint(0, 5, "Sample rate");
  gfxPrint(0, 6, "Bit depth");
  gfxPrint(0, 7, "Folder");

  gfxSetFgColor(cs.textValue);
  gfxPrint(0, 8, "MIDI");
}

void exportCommonDrawCursor(int col, int row) {
  if (row == 0) {
    gfxCursor(13, 2, 2);
  } else if (row == 1) {
    if (col == 0) {
      gfxCursor(13, 4, 6);
    } else {
      gfxCursor(20, 4, 5);
    }
  } else if (row == 2) {
    gfxCursor(13, 5, 5);
  } else if (row == 3) {
    gfxCursor(13, 6, 2);
  } else if (row == 4) {
    gfxCursor(13, 7, 26);
  } else if (row == 5) {
    gfxCursor(13, 8, 6);
  }
}

void exportCommonDrawField(int col, int row, CellState state) {
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);

  if (row == 0) {
    gfxClearRect(13, 2, 2, 1);
    gfxPrintf(13, 2, "%02X", startRow);
  } else if (row == 1) {
    if (col == 0) {
      gfxPrint(13, 4, "Export");
    } else {
      gfxPrint(20, 4, "Stems");
    }
  } else if (row == 2) {
    gfxClearRect(13, 5, 6, 1);
    gfxPrintf(13, 5, "%d", sampleRates[currentSampleRateIndex]);
  } else if (row == 3) {
    gfxClearRect(13, 6, 2, 1);
    gfxPrintf(13, 6, "%d", bitDepths[currentBitDepthIndex]);
  } else if (row == 4) {
    gfxClearRect(13, 7, 26, 1);
    if (appSettings.exportPath[0]) {
      // Show the tail of a long custom path
      const char* path = appSettings.exportPath;
      int len = strlen(path);
      if (len > 26) {
        gfxPrintf(13, 7, "...%.23s", path + len - 23);
      } else {
        gfxPrintf(13, 7, "%s", path);
      }
    } else {
      gfxPrint(13, 7, "Default");
    }
  } else if (row == 5) {
    gfxPrint(13, 8, "Export");
  }
}

static int fileExists(const char* path) {
  FILE* file = fopen(path, "r");
  if (file != NULL) {
    fclose(file);
    return 1;
  }
  return 0;
}

typedef int (*FileExistsCheckFunc)(const char* basePath, int count);

static int multiFileExists(const char* basePath, int count, const char* format) {
  for (int i = 0; i < count; i++) {
    char filename[1024];
    snprintf(filename, sizeof(filename), format, basePath, i + 1);
    if (fileExists(filename)) return 1;
  }
  return 0;
}

static int stemsFilesExist(const char* basePath, int trackCount) {
  return multiFileExists(basePath, trackCount, "%s-%02d.wav");
}

static void generateMultiFileExportPath(char* outputPath, int maxLen, FileExistsCheckFunc checkFunc, int count) {
  char basePath[EXPORT_PATH_MAX];
  exportBuildFilePath(basePath, sizeof(basePath), appSettings.projectFilename[0] ? appSettings.projectFilename : "choochootracker", NULL);

  if (!checkFunc(basePath, count)) {
    strncpy(outputPath, basePath, maxLen - 1);
    outputPath[maxLen - 1] = 0;
    return;
  }

  for (int i = 1; i <= 999; i++) {
    snprintf(outputPath, maxLen, "%s_%03d", basePath, i);
    if (!checkFunc(outputPath, count)) {
      return;
    }
  }

  strncpy(outputPath, basePath, maxLen - 1);
  outputPath[maxLen - 1] = 0;
}

void generateStemsExportPath(char* outputPath, int maxLen) {
  int trackCount = chipnomadState->project.tracksCount;
  generateMultiFileExportPath(outputPath, maxLen, stemsFilesExist, trackCount);
}

void generateExportPath(char* outputPath, int maxLen, const char* extension) {
  char basePath[EXPORT_PATH_MAX];
  exportBuildFilePath(basePath, sizeof(basePath), appSettings.projectFilename[0] ? appSettings.projectFilename : "choochootracker", extension);

  if (!fileExists(basePath)) {
    strncpy(outputPath, basePath, maxLen - 1);
    outputPath[maxLen - 1] = 0;
    return;
  }

  for (int i = 1; i <= 999; i++) {
    snprintf(outputPath, maxLen, "%s_%03d.%s", basePath, i, extension);
    if (!fileExists(outputPath)) {
      return;
    }
  }

  strncpy(outputPath, basePath, maxLen - 1);
  outputPath[maxLen - 1] = 0;
}

int exportCommonOnEdit(int col, int row, CellEditAction action) {
  int handled = 0;

  if (row == 0) {
    handled = edit8noLast(action, (uint8_t*)&startRow, 16, 0, PROJECT_MAX_LENGTH - 1);
  } else if (row == 1 && col == 0) {
    if (currentExporter) return 1;

    // Make sure the export destination exists before writing files
    if (exportEnsureProjectDir() != 0) {
      screenMessage(MESSAGE_TIME_ERROR, "Cannot create export folder");
      return 1;
    }

    char exportPath[1024];
    generateExportPath(exportPath, sizeof(exportPath), "wav");

    currentExporter = new ExporterWAV(exportPath, &chipnomadState->project, startRow, sampleRates[currentSampleRateIndex], bitDepths[currentBitDepthIndex], appSettings.mixVolume);
    if (currentExporter) {
      strncpy(currentExportPath, exportPath, sizeof(currentExportPath) - 1);
      currentExportPath[sizeof(currentExportPath) - 1] = 0;
      currentExportIsStems = 0;
      currentExportTrackCount = 0;
      currentExportIsBounce = 0;
      screenMessage(MESSAGE_TIME, "Starting export...");
    } else {
      screenMessage(MESSAGE_TIME, "Export failed to start");
    }
    handled = 1;
  } else if (row == 1 && col == 1) {
    if (currentExporter) return 1;

    // Make sure the export destination exists before writing files
    if (exportEnsureProjectDir() != 0) {
      screenMessage(MESSAGE_TIME_ERROR, "Cannot create export folder");
      return 1;
    }

    char basePath[512];
    generateStemsExportPath(basePath, sizeof(basePath));

    currentExporter = new ExporterWAV(basePath, &chipnomadState->project, startRow, sampleRates[currentSampleRateIndex], bitDepths[currentBitDepthIndex], appSettings.mixVolume, true);
    if (currentExporter) {
      int trackCount = chipnomadState->project.tracksCount;
      strncpy(currentExportPath, basePath, sizeof(currentExportPath) - 1);
      currentExportPath[sizeof(currentExportPath) - 1] = 0;
      currentExportIsStems = 1;
      currentExportTrackCount = trackCount;
      currentExportIsBounce = 0;
      screenMessage(MESSAGE_TIME, "Starting stems export (%d files)...", trackCount);
    } else {
      screenMessage(MESSAGE_TIME, "Export failed to start");
    }
    handled = 1;
  } else if (row == 2) {
    if (action == CellEditAction::increase) {
      currentSampleRateIndex = (currentSampleRateIndex + 1) % 4;
      handled = 1;
    } else if (action == CellEditAction::decrease) {
      currentSampleRateIndex = (currentSampleRateIndex + 3) % 4;
      handled = 1;
    }
  } else if (row == 3) {
    if (action == CellEditAction::increase) {
      currentBitDepthIndex = (currentBitDepthIndex + 1) % 3;
      handled = 1;
    } else if (action == CellEditAction::decrease) {
      currentBitDepthIndex = (currentBitDepthIndex + 2) % 3;
      handled = 1;
    }
  } else if (row == 4) {
    // Export folder: tap opens the folder picker, clear resets to default
    if (action == CellEditAction::tap || action == CellEditAction::doubleTap) {
      exportFolderPickerBegin();
      handled = 1;
    } else if (action == CellEditAction::clear) {
      if (appSettings.exportPath[0]) {
        appSettings.exportPath[0] = 0;
        settingsSave();
        screenMessage(MESSAGE_TIME, "Export folder: default");
      }
      handled = 1;
    }
  } else if (row == 5) {
    if (currentExporter) return 1;
    char exportPath[1024];
    generateExportPath(exportPath, sizeof(exportPath), "mid");
    if (projectExportMidi(&chipnomadState->project, exportPath) == 0) {
      screenMessage(MESSAGE_TIME, "Exported %s", exportPath);
#ifdef WEB_BUILD
      webDownloadExportFile(exportPath);
#endif
#ifdef ANDROID_BUILD
      fileExportDocument(exportPath, "audio/midi");
#endif
    } else {
      screenMessage(MESSAGE_TIME_ERROR, "%s", smfFileError);
    }
    handled = 1;
  }

  return handled;
}

///////////////////////////////////////////////////////////////////////////////
//
// Bounce selection to audio
//

// Trims spaces and validates the user-entered bounce file name
static int sanitizeBounceName(const char* input, char* output, int maxLen) {
  int start = 0;
  int end = strlen(input);
  while (start < end && input[start] == ' ') start++;
  while (end > start && input[end - 1] == ' ') end--;
  int len = end - start;
  if (len == 0) return 0;
  if (len > maxLen - 1) len = maxLen - 1;
  memcpy(output, input + start, len);
  output[len] = 0;
  return 1;
}

// Builds <export folder>/<name>.wav, adding a _NNN suffix on collision
static void generateBouncePath(char* outputPath, int maxLen, const char* name) {
  exportBuildFilePath(outputPath, maxLen, name, "wav");
}

static int exportStartBounceNamed(const ExportSelection& selection, const char* name, int sampleRate, int bitDepth) {
  if (currentExporter) return 0;

  char cleanName[128];
  if (!sanitizeBounceName(name, cleanName, sizeof(cleanName))) return 0;

  // Ensure the export destination exists before writing files
  if (exportEnsureProjectDir() != 0) return 0;

  char bouncePath[1024];
  generateBouncePath(bouncePath, sizeof(bouncePath), cleanName);

  ExporterSelectionWAV* exporter = new ExporterSelectionWAV(bouncePath, &chipnomadState->project, selection,
                                                            sampleRate, bitDepth,
                                                            appSettings.mixVolume);
  if (!exporter) return 0;

  currentExporter = exporter;
  strncpy(currentExportPath, bouncePath, sizeof(currentExportPath) - 1);
  currentExportPath[sizeof(currentExportPath) - 1] = 0;
  currentExportIsStems = 0;
  currentExportTrackCount = 0;
  currentExportIsBounce = 1;
  screenMessage(MESSAGE_TIME, "Starting bounce...");
  return 1;
}

// Returns from the bounce screen to the screen the bounce was triggered from
static void bounceReturnToOrigin(void) {
  const AppScreen* returnScreen = bounceReturnScreen ? bounceReturnScreen : &screenProject;
  bounceReturnScreen = NULL;
  currentExportIsBounce = 0;
  screenSetup(returnScreen, 0);
}

///////////////////////////////////////////////////////////////////////////////
//
// BOUNCE TO SAMPLE screen
//
// Shown after double-tap EDIT on a selection. Offers the file name and the
// same sample quality options as the export screen, then renders the bounce
// in place and returns to the screen the bounce was triggered from.
///////////////////////////////////////////////////////////////////////////////

static char bounceName[FILENAME_LENGTH + 1];
static int bounceSampleRateIndex = 0;
static int bounceBitDepthIndex = 0;
static int bounceIsCharEdit = 0;
// "Include in a sample name" prefix checkboxes (all off by default)
static int bouncePrefixBpm = 0;
static int bouncePrefixKey = 0;
static int bouncePrefixLength = 0;

// Layout: file name field sits on the same row as its label
#define BOUNCE_NAME_LABEL_X (0)
#define BOUNCE_NAME_FIELD_X (13)
#define BOUNCE_NAME_FIELD_W (24)
#define BOUNCE_PREFIX_ROW (3)
#define BOUNCE_START_ROW (6)
// Screen row of Start/Cancel: one blank row below the checkboxes
#define BOUNCE_START_SCREEN_Y (11)

static int bounceColumnCount(int row) {
  if (row == 0) return BOUNCE_NAME_FIELD_W; // File name field
  if (row == BOUNCE_START_ROW) return 2;    // Start, Cancel
  return 1;
}

static void bounceDrawStatic(void) {
  const ColorScheme cs = appSettings.colorScheme;

  gfxSetFgColor(cs.textTitles);
  gfxPrint(0, 0, "BOUNCE TO SAMPLE");

  gfxSetFgColor(cs.textDefault);
  gfxPrint(BOUNCE_NAME_LABEL_X, 2, "File name");
  gfxPrint(0, 3, "Sample rate");
  gfxPrint(0, 4, "Bit depth");
  gfxPrint(0, 6, "Include in a sample name:");
}

static void bounceDrawCursor(int col, int row) {
  if (row == 0) {
    gfxCursor(BOUNCE_NAME_FIELD_X + col, 2, 1);
  } else if (row == 1) {
    gfxCursor(13, 3, 5);
  } else if (row == 2) {
    gfxCursor(13, 4, 2);
  } else if (row >= BOUNCE_PREFIX_ROW && row < BOUNCE_START_ROW) {
    gfxCursor(0, row + 4, 1); // On the checkbox bracket
  } else if (row == BOUNCE_START_ROW) {
    if (col == 0) {
      gfxCursor(0, BOUNCE_START_SCREEN_Y, 5);
    } else {
      gfxCursor(9, BOUNCE_START_SCREEN_Y, 6);
    }
  }
}

static void bounceDrawField(int col, int row, CellState state) {
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);

  if (row == 0) {
    gfxClearRect(BOUNCE_NAME_FIELD_X, 2, BOUNCE_NAME_FIELD_W, 1);
    gfxPrint(BOUNCE_NAME_FIELD_X, 2, bounceName);
  } else if (row == 1) {
    gfxClearRect(13, 3, 6, 1);
    gfxPrintf(13, 3, "%d", sampleRates[bounceSampleRateIndex]);
  } else if (row == 2) {
    gfxClearRect(13, 4, 2, 1);
    gfxPrintf(13, 4, "%d", bitDepths[bounceBitDepthIndex]);
  } else if (row >= BOUNCE_PREFIX_ROW && row < BOUNCE_START_ROW) {
    static const char* const prefixLabels[] = {"[BPM]", "[Key]", "[Bars:Beats:16ths]"};
    int idx = row - BOUNCE_PREFIX_ROW;
    int enabled = idx == 0 ? bouncePrefixBpm : (idx == 1 ? bouncePrefixKey : bouncePrefixLength);
    gfxPrintf(0, row + 4, "[%c] %s", enabled ? 'x' : ' ', prefixLabels[idx]);
  } else if (row == BOUNCE_START_ROW) {
    if (col == 0) {
      gfxPrint(0, BOUNCE_START_SCREEN_Y, "Start");
    } else {
      gfxPrint(9, BOUNCE_START_SCREEN_Y, "Cancel");
    }
  }
}

// Builds the final file name on Start: optional [BPM][Key][length] prefixes
// followed by the sanitized user name. The sequence number is not appended
// here: it is proposed in the file name field when the bounce screen opens
// (exportProposeBounceName) and used as the name itself.
static void buildBounceFileName(char* output, int maxLen, const char* cleanName) {
  Project* p = &chipnomadState->project;

  char prefix[64];
  prefix[0] = 0;

  if (bouncePrefixBpm) {
    float bpm = p->tickRate * 60.0f / 24.0f;
    char bpmText[16];
    snprintf(bpmText, sizeof(bpmText), "%d", (int)(bpm + 0.5f));
    strncat(prefix, "[", sizeof(prefix) - strlen(prefix) - 1);
    strncat(prefix, bpmText, sizeof(prefix) - strlen(prefix) - 1);
    strncat(prefix, "]", sizeof(prefix) - strlen(prefix) - 1);
  }

  if (bouncePrefixKey) {
    static const char* const noteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const char* scaleName = scalePresetName(p->scalePreset);
    // Abbreviate major/minor variants: "Major" -> "maj", "Minor" -> "min"
    char scaleText[32];
    if (strncmp(scaleName, "Major", 5) == 0) {
      snprintf(scaleText, sizeof(scaleText), "maj%s", scaleName + 5);
    } else if (strncmp(scaleName, "Minor", 5) == 0) {
      snprintf(scaleText, sizeof(scaleText), "min%s", scaleName + 5);
    } else {
      snprintf(scaleText, sizeof(scaleText), "%s", scaleName);
    }
    strncat(prefix, "[", sizeof(prefix) - strlen(prefix) - 1);
    strncat(prefix, noteNames[p->scaleRoot & 0x0F], sizeof(prefix) - strlen(prefix) - 1);
    strncat(prefix, scaleText, sizeof(prefix) - strlen(prefix) - 1);
    strncat(prefix, "]", sizeof(prefix) - strlen(prefix) - 1);
  }

  if (bouncePrefixLength) {
    int rows = exportSelectionLengthRows(p, pendingBounceSelection);
    // A selection ending mid-beat reads one sixteenth shorter: the last
    // selected row acts as the cut point. Selections ending on a whole beat
    // or bar keep the full count.
    if (rows % 4 != 0) rows--;
    char lengthText[32];
    if (rows % 16 == 0) {
      // Whole phrases: just the bar count
      snprintf(lengthText, sizeof(lengthText), "%d", rows / 16);
    } else if (rows % 4 == 0) {
      snprintf(lengthText, sizeof(lengthText), "%d:%d", rows / 16, (rows % 16) / 4);
    } else {
      snprintf(lengthText, sizeof(lengthText), "%d:%d:%d", rows / 16, (rows % 16) / 4, rows % 4);
    }
    strncat(prefix, "[", sizeof(prefix) - strlen(prefix) - 1);
    strncat(prefix, lengthText, sizeof(prefix) - strlen(prefix) - 1);
    strncat(prefix, "]", sizeof(prefix) - strlen(prefix) - 1);
  }

  snprintf(output, maxLen, "%s%s", prefix, cleanName);
}

static void startBounce(void) {
  char cleanName[64];
  // An empty name falls back to the proposed sequence number
  if (!sanitizeBounceName(bounceName, cleanName, sizeof(cleanName))) {
    exportProposeBounceName(cleanName, sizeof(cleanName));
  }

  // A plain sequence-number name advances the per-project counter; custom
  // names leave it untouched. Claimed only when the bounce actually starts,
  // so a failed start does not skip a number.

  char finalName[192];
  buildBounceFileName(finalName, sizeof(finalName), cleanName);

  if (!exportStartBounceNamed(pendingBounceSelection, finalName,
                              sampleRates[bounceSampleRateIndex],
                              bitDepths[bounceBitDepthIndex])) {
    screenMessage(MESSAGE_TIME_ERROR, "Bounce failed to start");
    bounceReturnToOrigin();
    return;
  }

  exportClaimBounceName(cleanName);
}

static int bounceOnEdit(int col, int row, CellEditAction action) {
  int handled = 0;

  if (row == 0) {
    int res = editCharacter(action, bounceName, col, FILENAME_LENGTH);
    if (res == 1) {
      bounceIsCharEdit = 1;
    } else if (res > 1) {
      handled = 1;
    }
  } else if (row == 1) {
    if (action == CellEditAction::increase) {
      bounceSampleRateIndex = (bounceSampleRateIndex + 1) % 4;
      handled = 1;
    } else if (action == CellEditAction::decrease) {
      bounceSampleRateIndex = (bounceSampleRateIndex + 3) % 4;
      handled = 1;
    }
  } else if (row == 2) {
    if (action == CellEditAction::increase) {
      bounceBitDepthIndex = (bounceBitDepthIndex + 1) % 3;
      handled = 1;
    } else if (action == CellEditAction::decrease) {
      bounceBitDepthIndex = (bounceBitDepthIndex + 2) % 3;
      handled = 1;
    }
  } else if (row >= BOUNCE_PREFIX_ROW && row < BOUNCE_START_ROW) {
    // Checkbox: tap toggles, up/down edit flips too
    if (action == CellEditAction::tap || action == CellEditAction::doubleTap ||
        action == CellEditAction::increase || action == CellEditAction::decrease) {
      int idx = row - BOUNCE_PREFIX_ROW;
      if (idx == 0) bouncePrefixBpm ^= 1;
      else if (idx == 1) bouncePrefixKey ^= 1;
      else bouncePrefixLength ^= 1;
      handled = 1;
    }
  } else if (row == BOUNCE_START_ROW) {
    if (col == 0) {
      startBounce();
    } else {
      bounceReturnToOrigin();
    }
  }

  return handled;
}

static ScreenData bounceScreenData = {
  .rows = SCR_BOUNCE_ROWS,
  .cursorRow = 0,
  .cursorCol = 0,
  .topRow = 0,
  .selectMode = -1,
  .selectStartRow = 0,
  .selectStartCol = 0,
  .selectAnchorRow = 0,
  .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = bounceColumnCount,
  .drawStatic = bounceDrawStatic,
  .drawCursor = bounceDrawCursor,
  .drawSelection = drawSelection,
  .drawRowHeader = drawRowHeader,
  .drawColHeader = drawColHeader,
  .drawField = bounceDrawField,
  .onEdit = bounceOnEdit,
  .onInput = NULL,
  .onRawInput = NULL,
  .isCellValid = NULL,
  .getLoopRange = NULL,
};

static void bounceSetup(int input) {
  bounceIsCharEdit = 0;
  bouncePrefixBpm = 0;
  bouncePrefixKey = 0;
  bouncePrefixLength = 0;
  bounceScreenData.cursorRow = 0;
  bounceScreenData.cursorCol = 0;
}

static void bounceFullRedraw(void) {
  screenFullRedraw(&bounceScreenData);
}

static void bounceDraw(void) {
  exportPump();
}

static int bounceOnInput(int isKeyDown, int keys, int tapCount) {
  if (currentExporter) {
    if (keys == keyOpt) {
      currentExporter->cancel();
      delete currentExporter;
      currentExporter = NULL;
      screenMessage(MESSAGE_TIME, "Bounce cancelled");
      bounceReturnToOrigin();
    }
    return 1; // Block all other input during bounce
  }

  if (bounceIsCharEdit) {
    char result = charEditInput(keys, tapCount, bounceName, bounceScreenData.cursorCol, FILENAME_LENGTH);
    if (result) {
      bounceIsCharEdit = 0;
      if (bounceScreenData.cursorCol < FILENAME_LENGTH - 1) bounceScreenData.cursorCol++;
      bounceFullRedraw();
    }
    return 1;
  }

  if (keys == keyOpt) {
    bounceReturnToOrigin();
    return 1;
  }

  return screenInput(&bounceScreenData, isKeyDown, keys, tapCount);
}

const AppScreen screenBounce = {
  .init = NULL,
  .setup = bounceSetup,
  .fullRedraw = bounceFullRedraw,
  .draw = bounceDraw,
  .onInput = bounceOnInput,
  .getPlaybackLevel = NULL,
};

// Called from selection mode (double-tap A): opens the BOUNCE TO SAMPLE screen
void exportBounceBegin(const ExportSelection& selection) {
  if (currentExporter) {
    screenMessage(MESSAGE_TIME_ERROR, "Bounce failed to start");
    return;
  }
  pendingBounceSelection = selection;
  bounceReturnScreen = currentScreen;
  // The file name field proposes the next free sequence number (001, 002,
  // ...); the user keeps it or types a custom name. A custom name is written
  // as-is when it does not collide with an existing file, and gets a
  // _001.._999 suffix on collision - the sequence number is never appended
  // to it.
  exportProposeBounceName(bounceName, sizeof(bounceName));
  bounceSampleRateIndex = 0;
  bounceBitDepthIndex = 0;
  screenSetup(&screenBounce, 0);
}

///////////////////////////////////////////////////////////////////////////////
//
// Export folder picker
//

static void onExportFolderSelected(const char* folderPath) {
  strncpy(appSettings.exportPath, folderPath, PATH_LENGTH);
  appSettings.exportPath[PATH_LENGTH] = 0;
  settingsSave();
  screenSetup(&screenExport, 0);
  screenMessage(MESSAGE_TIME, "Export folder set");
}

static void onExportFolderCancelled(void) {
  screenSetup(&screenExport, 0);
}

// Opens the folder picker for choosing a custom export folder
void exportFolderPickerBegin(void) {
  // Start browsing from the current custom folder, or the default base dir
  char startPath[EXPORT_PATH_MAX];
  if (appSettings.exportPath[0]) {
    snprintf(startPath, sizeof(startPath), "%s", appSettings.exportPath);
  } else {
    exportGetBaseDir(startPath, sizeof(startPath));
    // Create the default location so the browser opens there
    fileCreateDirectoryRecursive(startPath);
  }
  // The dummy filename keeps the browser's overwrite check away from the
  // directory itself; only the chosen folder path reaches the callback
  fileBrowserSetupFolderMode("EXPORT FOLDER", startPath, "export", "", onExportFolderSelected, onExportFolderCancelled);
  screenSetup(&screenFileBrowser, 0);
}
