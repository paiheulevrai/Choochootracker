#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "corelib/corelib_file.h"
#include "file_browser.h"
#include "synth/sample_voice.h"
#include "utils.h"
#include "audio_manager.h"

#include <string.h>

static int sampleButtonDown;

static void onSampleCancelled(void) {
  audioManager.stopSamplePreview();
  screenSetup(&screenInstrument, cInstrument);
}

static void onSamplePreview(const char* path) {
  if (audioManager.previewSample(path)) screenMessage(MESSAGE_TIME, "Cannot preview WAV");
}

static void onSampleLoaded(const char* path) {
  audioManager.stopSamplePreview();
  Instrument* instrument = &chipnomadState->project.instruments[cInstrument];
  char error[64];
  if (sampleLoadWav16(path, &instrument->chip.sample, error, sizeof(error))) {
    screenMessage(MESSAGE_TIME * 3, "%s", error);
  } else {
    const char* separator = strrchr(path, PATH_SEPARATOR);
    const char* filename = separator ? separator + 1 : path;
    if (!instrument->name[0]) {
      strncpy(instrument->name, filename, PROJECT_INSTRUMENT_NAME_LENGTH);
      instrument->name[PROJECT_INSTRUMENT_NAME_LENGTH] = 0;
      char* extension = strrchr(instrument->name, '.');
      if (extension) *extension = 0;
    }
    if (separator) {
      size_t length = separator - path;
      if (length < PATH_LENGTH) {
        memcpy(appSettings.samplePath, path, length);
        appSettings.samplePath[length] = 0;
      }
    }
    projectModified = 1;
    screenMessage(MESSAGE_TIME, "PCM16 %lu frames %lu Hz",
      (unsigned long)instrument->chip.sample.frameCount,
      (unsigned long)instrument->chip.sample.sampleRate);
  }
  screenSetup(&screenInstrument, cInstrument);
}

static int getColumnCount(int row) {
  if (row < 3) return instrumentCommonColumnCount(row);
  return row == 12 ? 4 : 1;
}

static void drawStatic(void) {
  instrumentCommonDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  static const char* labels[] = {
    "Sample", "Pitch", "Start", "End", "Filter",
    "Mode", "Slope", "Cutoff", "Reso"
  };
  for (int i = 0; i < 9; i++) gfxPrint(0, 6 + i, labels[i]);
  gfxPrint(0, 15, "ADSR");
  gfxPrint(6, 15, "A"); gfxPrint(11, 15, "D");
  gfxPrint(16, 15, "S"); gfxPrint(21, 15, "R");
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  if (row == 12) gfxCursor(7 + col * 5, 15, 2);
  else gfxCursor(12, row + 3, row == 3 ? 4 : 7);
}

static const char* sampleFilename(const char* path) {
  const char* separator = strrchr(path, PATH_SEPARATOR);
  return separator ? separator + 1 : path;
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) return instrumentCommonDrawField(col, row, state);
  InstrumentSample* sample = &chipnomadState->project.instruments[cInstrument].chip.sample;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  if (row == 12) {
    uint8_t values[] = {sample->attack, sample->decay, sample->sustain, sample->release};
    gfxPrint(7 + col * 5, 15, byteToHex(values[col]));
    return;
  }
  gfxClearRect(12, row + 3, 27, 1);
  switch (row) {
    case 3: gfxPrintf(12, 6, "Load %s", sampleFilename(sample->path)); break;
    case 4: gfxPrintf(12, 7, "%+d st", sample->pitch); break;
    case 5: gfxPrint(12, 8, byteToHex(sample->start)); break;
    case 6: gfxPrint(12, 9, byteToHex(sample->end)); break;
    case 7: gfxPrint(12, 10, sample->filterEnabled ? "On" : "Off"); break;
    case 8: { static const char* modes[] = {"LP", "BP", "HP"}; gfxPrint(12, 11, modes[sample->filterMode <= 2 ? sample->filterMode : 0]); break; }
    case 9: gfxPrint(12, 12, sample->filterSlope24dB ? "24 dB" : "12 dB"); break;
    case 10: gfxPrintf(12, 13, "%u Hz", sample->filterCutoffHz); break;
    case 11: gfxPrint(12, 14, byteToHex(sample->filterResonance)); break;
  }
}

static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentSample* sample = &chipnomadState->project.instruments[cInstrument].chip.sample;
  int handled = 0;
  switch (row) {
    case 3:
      return 0;
    case 4: handled = editSigned8(action, &sample->pitch, 12, -48, 48); break;
    case 5: handled = edit8noLast(action, &sample->start, 16, 0, sample->end); break;
    case 6: handled = edit8noLast(action, &sample->end, 16, sample->start, 255); break;
    case 7: handled = edit8noLast(action, &sample->filterEnabled, 1, 0, 1); break;
    case 8: handled = edit8noLast(action, &sample->filterMode, 1, 0, 2); break;
    case 9: handled = edit8noLast(action, &sample->filterSlope24dB, 1, 0, 1); break;
    case 10: handled = editFilterCutoff(action, &sample->filterCutoffHz); break;
    case 11: handled = edit8noLast(action, &sample->filterResonance, 16, 0, 255); break;
    case 12: {
      uint8_t* values[] = {&sample->attack, &sample->decay, &sample->sustain, &sample->release};
      handled = edit8noLast(action, values[col], 16, 0, 255);
      break;
    }
  }
  if (handled) projectModified = 1;
  return handled;
}

static int loadAdjacentSample(int direction) {
  InstrumentSample* sample = &chipnomadState->project.instruments[cInstrument].chip.sample;
  const char* separator = strrchr(sample->path, PATH_SEPARATOR);
  if (!separator) return 0;
  char directory[PROJECT_SAMPLE_PATH_LENGTH + 1];
  size_t directoryLength = separator - sample->path;
  if (directoryLength >= sizeof(directory)) return 0;
  memcpy(directory, sample->path, directoryLength);
  directory[directoryLength] = 0;
  int count = 0;
  FileEntry* entries = fileListDirectory(directory, ".wav", &count);
  if (!entries) return 0;
  const char* current = separator + 1;
  const char* selected = NULL;
  for (int i = 0; i < count; ++i) {
    if (entries[i].isDirectory) continue;
    int comparison = strcmp(entries[i].name, current);
    if (direction > 0 && comparison > 0 &&
        (!selected || strcmp(entries[i].name, selected) < 0)) selected = entries[i].name;
    if (direction < 0 && comparison < 0 &&
        (!selected || strcmp(entries[i].name, selected) > 0)) selected = entries[i].name;
  }
  if (!selected) {
    for (int i = 0; i < count; ++i) {
      if (entries[i].isDirectory) continue;
      if (!selected || (direction > 0 ? strcmp(entries[i].name, selected) < 0
                                      : strcmp(entries[i].name, selected) > 0)) {
        selected = entries[i].name;
      }
    }
  }
  char path[PROJECT_SAMPLE_PATH_LENGTH + 1];
  if (selected) snprintf(path, sizeof(path), "%s%s%s", directory, PATH_SEPARATOR_STR, selected);
  free(entries);
  if (!selected) return 0;
  onSampleLoaded(path);
  return 1;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (screenInstrumentSample.cursorRow != 3) {
    sampleButtonDown = 0;
    return 0;
  }
  if (isKeyDown && keys == keyEdit) {
    sampleButtonDown = 1;
    return 1;
  }
  if (isKeyDown && keys == (keyEdit | keyLeft)) {
    sampleButtonDown = 0;
    return loadAdjacentSample(-1);
  }
  if (isKeyDown && keys == (keyEdit | keyRight)) {
    sampleButtonDown = 0;
    return loadAdjacentSample(1);
  }
  if (!isKeyDown && keys == 0 && sampleButtonDown) {
    sampleButtonDown = 0;
    fileBrowserSetupWithPreview("LOAD PCM SAMPLE", ".wav", appSettings.samplePath,
      onSampleLoaded, onSampleCancelled, onSamplePreview);
    screenSetup(&screenFileBrowser, 0);
    return 1;
  }
  return 0;
}

ScreenData screenInstrumentSample = {
  .rows = 13, .cursorRow = 0, .cursorCol = 0, .topRow = 0,
  .selectMode = -1, .selectStartRow = 0, .selectStartCol = 0,
  .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = getColumnCount, .drawStatic = drawStatic,
  .drawCursor = drawCursor, .drawSelection = NULL,
  .drawRowHeader = NULL, .drawColHeader = NULL, .drawField = drawField,
  .onEdit = onEdit, .onInput = onInput, .onRawInput = NULL,
  .isCellValid = NULL, .getLoopRange = NULL,
};
