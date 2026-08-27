#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "corelib/corelib_file.h"
#include "file_browser.h"
#include "synth/sample_voice.h"
#include "utils.h"
#include "audio_manager.h"
#include "waveform_display.h"

#include <string.h>

static int sampleButtonDown;
static constexpr int sourceValueX = 9;
static constexpr int sourceValueWidth = 7;
static constexpr int previewRow = 16, previewWidth = 32, previewHeight = 3;
static Bitmap* samplePreviewBitmap;

static void updateSamplePreview(const InstrumentSample* sample) {
  if (!samplePreviewBitmap) samplePreviewBitmap = gfxBitmapCreate(previewWidth, previewHeight);
  uint32_t start = sample->frameCount ? (uint64_t)sample->start * (sample->frameCount - 1) / 255 : 0;
  uint32_t end = sample->end == 255 ? sample->frameCount :
    (uint64_t)(sample->end + 1) * sample->frameCount / 256;
  if (start > end) { uint32_t swap = start; start = end; end = swap + 1; }
  renderPCM16Preview(samplePreviewBitmap, sample->data, start, end, sample->channels);
}

static void drawSamplePreview(void) {
  if (!samplePreviewBitmap) return;
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxDrawBitmap(samplePreviewBitmap, 0, previewRow);
}

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
  audioManager.pause();
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
  audioManager.resume();
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
  gfxPrint(0,7,"SOURCE");
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(0,8,"Pitch"); gfxPrint(0,9,"Start"); gfxPrint(0,10,"End"); gfxPrint(0,11,"Loop"); gfxPrint(0,12,"Speed");
  instrumentCommonDrawVoicePostStatic(1);
  InstrumentSample* sample = &chipnomadState->project.instruments[cInstrument].chip.sample;
  updateSamplePreview(sample);
  drawSamplePreview();
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  if (instrumentCommonDrawVoicePostCursor(col, row)) return;
  else if (row == 3) gfxCursor(sourceValueX,6,sourceValueWidth);
  else gfxCursor(col ? 26 : sourceValueX,row+4,col?8:sourceValueWidth);
}

static const char* sampleFilename(const char* path) {
  const char* separator = strrchr(path, PATH_SEPARATOR);
  return separator ? separator + 1 : path;
}

static const char* shortSampleFilename(const char* path, size_t maxLength) {
  const char* name = sampleFilename(path);
  size_t length = strlen(name);
  return length > maxLength ? name + length - maxLength : name;
}

static void drawField(int col, int row, CellState state) {
  if (row < 3) return instrumentCommonDrawField(col, row, state);
  InstrumentSample* sample = &chipnomadState->project.instruments[cInstrument].chip.sample;
  if (instrumentCommonDrawVoicePostField(col, row, state, sample)) return;
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  if (row == 3) gfxClearRect(sourceValueX,6,sourceValueWidth,1); else gfxClearRect(col?26:sourceValueX,row+4,col?8:sourceValueWidth,1);
  switch (row) {
    case 3: gfxPrint(sourceValueX,6,sample->path[0] ? shortSampleFilename(sample->path, 7) : "Load"); break;
    case 4: if(!col) gfxPrintf(sourceValueX,8,"%+d st",sample->pitch); break;
    case 5: if(!col) gfxPrint(sourceValueX,9,byteToHex(sample->start)); break;
    case 6: if(!col) gfxPrint(sourceValueX,10,byteToHex(sample->end)); break;
    case 7: if(!col) { static const char* m[]={"Off","Loop","Ping"}; gfxPrint(sourceValueX,11,m[sample->loopMode<=2?sample->loopMode:0]); } break;
    case 8: if(!col) gfxPrintf(sourceValueX,12,"%03u%%",sample->speedPercent); break;
  }
}

static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentSample* sample = &chipnomadState->project.instruments[cInstrument].chip.sample;
  if ((row == 9) || (col && row >= 4 && row <= 8)) {
    int handled = instrumentCommonOnEditVoicePost(col, row, action, sample);
    if (handled) projectModified = 1;
    return handled;
  }
  int handled = 0;
  switch (row) {
    case 3:
      return 0;
    case 4: handled=!col?editSigned8(action,&sample->pitch,12,-48,48):0; break;
    case 5: handled=!col?edit8noLast(action,&sample->start,16,0,255):0; break;
    case 6: handled=!col?edit8noLast(action,&sample->end,16,0,255):0; break;
    case 7: handled=!col?edit8noLast(action,&sample->loopMode,1,0,2):0; break;
    case 8: handled=!col?edit16withMinMax(action,&sample->speedPercent,25,0,500):0; break;
  }
  if (handled) projectModified = 1;
  if (handled && !col && (row == 5 || row == 6)) {
    updateSamplePreview(sample);
    screenFullRedraw(&screenInstrumentSample);
  }
  return handled;
}

static int loadAdjacentSample(int direction) {
  InstrumentSample* sample = &chipnomadState->project.instruments[cInstrument].chip.sample;
  char path[PROJECT_SAMPLE_PATH_LENGTH + 1];
  if (!fileBrowserGetAdjacentPath(sample->path, ".wav", direction, path, sizeof(path))) return 0;
  onSampleLoaded(path);
  return 1;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (screenInstrumentSample.cursorRow != 3) {
    sampleButtonDown = 0;
    return 0;
  }
  PopupEditInput input = popupEditInput(isKeyDown, keys, &sampleButtonDown);
  if (input == PopupEditInput::cycle && keys == (keyEdit | keyLeft)) {
    return loadAdjacentSample(-1);
  }
  if (input == PopupEditInput::cycle && keys == (keyEdit | keyRight)) {
    return loadAdjacentSample(1);
  }
  if (input == PopupEditInput::hold) return 1;
  if (input == PopupEditInput::open) {
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
