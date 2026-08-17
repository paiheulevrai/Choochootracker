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
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0,6,"Sample");
  gfxPrint(0,7,"SOURCE"); gfxPrint(18,7,"FILTER");
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(0,8,"Pitch"); gfxPrint(18,8,"Status"); gfxPrint(0,9,"Start"); gfxPrint(18,9,"Mode"); gfxPrint(0,10,"End"); gfxPrint(18,10,"Slope"); gfxPrint(0,11,"Loop"); gfxPrint(18,11,"Cutoff"); gfxPrint(0,12,"Speed"); gfxPrint(18,12,"Reso");
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxPrint(0,14,"ADSR");
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(6,14,"A"); gfxPrint(11,14,"D"); gfxPrint(16,14,"S"); gfxPrint(21,14,"R"); gfxPrint(27,14,"Shape");
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  if (row == 9) gfxCursor(col == 4 ? 35 : 7 + col * 5, 14, 2);
  else if (row == 3) gfxCursor(11,6,28);
  else gfxCursor(col ? 26 : 11,row+4,col?8:7);
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
    gfxPrint(col == 4 ? 35 : 7 + col * 5, 14, byteToHex(values[col]));
    instrumentCommonDrawEnvelopePreview(sample->attack, sample->decay, sample->sustain, sample->release, sample->envelopeShape);
    return;
  }
  if (row == 3) gfxClearRect(11,6,29,1); else gfxClearRect(col?26:11,row+4,col?8:7,1);
  switch (row) {
    case 3: gfxPrintf(11,6,"Load %s",sampleFilename(sample->path)); break;
    case 4: if(!col) gfxPrintf(11,8,"%+d st",sample->pitch); else gfxPrint(26,8,sample->filterEnabled?"On":"Off"); break;
    case 5: if(!col) gfxPrint(11,9,byteToHex(sample->start)); else { static const char* m[]={"LP","BP","HP"}; gfxPrint(26,9,m[sample->filterMode<=2?sample->filterMode:0]); } break;
    case 6: if(!col) gfxPrint(11,10,byteToHex(sample->end)); else gfxPrint(26,10,sample->filterSlope24dB?"24 dB":"12 dB"); break;
    case 7: if(!col) { static const char* m[]={"Off","Loop","Ping"}; gfxPrint(11,11,m[sample->loopMode<=2?sample->loopMode:0]); } else gfxPrintf(26,11,"%u Hz",sample->filterCutoffHz); break;
    case 8: if(!col) gfxPrintf(11,12,"%03u%%",sample->speedPercent); else gfxPrint(26,12,byteToHex(sample->filterResonance)); break;
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
    case 5: handled=!col?edit8noLast(action,&sample->start,16,0,255):edit8noLast(action,&sample->filterMode,1,0,2); break;
    case 6: handled=!col?edit8noLast(action,&sample->end,16,0,255):edit8noLast(action,&sample->filterSlope24dB,1,0,1); break;
    case 7: handled=!col?edit8noLast(action,&sample->loopMode,1,0,2):editFilterCutoff(action,&sample->filterCutoffHz); break;
    case 8: handled=!col?edit16withMinMax(action,&sample->speedPercent,25,0,500):edit8noLast(action,&sample->filterResonance,16,0,255); break;
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
