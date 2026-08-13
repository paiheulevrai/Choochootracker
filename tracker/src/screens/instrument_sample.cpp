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
  if (row == 3) return 1;
  return row == 9 ? 5 : 2;
}

static void drawStatic(void) {
  instrumentCommonDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(0,6,"Sample"); gfxPrint(0,7,"Pitch"); gfxPrint(21,7,"Filter"); gfxPrint(0,8,"Start"); gfxPrint(21,8,"Mode"); gfxPrint(0,9,"End"); gfxPrint(21,9,"Slope"); gfxPrint(21,10,"Cutoff"); gfxPrint(21,11,"Reso");
  gfxPrint(0,13,"ADSR"); gfxPrint(6,13,"A"); gfxPrint(11,13,"D"); gfxPrint(16,13,"S"); gfxPrint(21,13,"R"); gfxPrint(27,13,"Shape");
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  if (row == 9) gfxCursor(col == 4 ? 35 : 7 + col * 5, 13, 2);
  else if (row == 3) gfxCursor(11,6,28);
  else gfxCursor(col ? 31 : 11,row+3,col?8:9);
}

static const char* sampleFilename(const char* path) {
  const char* separator = strrchr(path, PATH_SEPARATOR);
  return separator ? separator + 1 : path;
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) return instrumentCommonDrawField(col, row, state);
  InstrumentSample* sample = &chipnomadState->project.instruments[cInstrument].chip.sample;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  if (row == 9) {
    uint8_t values[] = {sample->attack, sample->decay, sample->sustain, sample->release, sample->envelopeShape};
    gfxPrint(col == 4 ? 35 : 7 + col * 5, 13, byteToHex(values[col]));
    return;
  }
  if (row == 3) gfxClearRect(11,6,29,1); else gfxClearRect(col?31:11,row+3,col?9:10,1);
  switch (row) {
    case 3: gfxPrintf(11,6,"Load %s",sampleFilename(sample->path)); break;
    case 4: if(!col) gfxPrintf(11,7,"%+d st",sample->pitch); else gfxPrint(31,7,sample->filterEnabled?"On":"Off"); break;
    case 5: if(!col) gfxPrint(11,8,byteToHex(sample->start)); else { static const char* m[]={"LP","BP","HP"}; gfxPrint(31,8,m[sample->filterMode<=2?sample->filterMode:0]); } break;
    case 6: if(!col) gfxPrint(11,9,byteToHex(sample->end)); else gfxPrint(31,9,sample->filterSlope24dB?"24 dB":"12 dB"); break;
    case 7: if(col) gfxPrintf(31,10,"%u Hz",sample->filterCutoffHz); break;
    case 8: if(col) gfxPrint(31,11,byteToHex(sample->filterResonance)); break;
  }
}

static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentSample* sample = &chipnomadState->project.instruments[cInstrument].chip.sample;
  int handled = 0;
  switch (row) {
    case 3:
      return 0;
    case 4: handled=!col?editSigned8(action,&sample->pitch,12,-48,48):edit8noLast(action,&sample->filterEnabled,1,0,1); break;
    case 5: handled=!col?edit8noLast(action,&sample->start,16,0,sample->end):edit8noLast(action,&sample->filterMode,1,0,2); break;
    case 6: handled=!col?edit8noLast(action,&sample->end,16,sample->start,255):edit8noLast(action,&sample->filterSlope24dB,1,0,1); break;
    case 7: handled=col?editFilterCutoff(action,&sample->filterCutoffHz):0; break;
    case 8: handled=col?edit8noLast(action,&sample->filterResonance,16,0,255):0; break;
    case 9: {
      uint8_t* values[] = {&sample->attack, &sample->decay, &sample->sustain, &sample->release, &sample->envelopeShape};
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
  .rows = 10, .cursorRow = 0, .cursorCol = 0, .topRow = 0,
  .selectMode = -1, .selectStartRow = 0, .selectStartCol = 0,
  .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = getColumnCount, .drawStatic = drawStatic,
  .drawCursor = drawCursor, .drawSelection = NULL,
  .drawRowHeader = NULL, .drawColHeader = NULL, .drawField = drawField,
  .onEdit = onEdit, .onInput = onInput, .onRawInput = NULL,
  .isCellValid = NULL, .getLoopRange = NULL,
};
