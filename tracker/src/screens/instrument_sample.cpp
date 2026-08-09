#include "screen_instrument.h"
#include "corelib_gfx.h"
#include "corelib/corelib_file.h"
#include "file_browser.h"
#include "synth/sample_voice.h"
#include "utils.h"

#include <string.h>

static void onSampleCancelled(void) {
  screenSetup(&screenInstrument, cInstrument);
}

static void onSampleLoaded(const char* path) {
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
  return row == 13 ? 4 : 1;
}

static void drawStatic(void) {
  instrumentCommonDrawStatic();
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  static const char* labels[] = {
    "Sample", "Pitch", "Start", "End", "Volume", "Filter",
    "Mode", "Slope", "Cutoff", "Reso"
  };
  for (int i = 0; i < 10; i++) gfxPrint(0, 6 + i, labels[i]);
  gfxPrint(0, 16, "ADSR");
  gfxPrint(6, 16, "A"); gfxPrint(11, 16, "D");
  gfxPrint(16, 16, "S"); gfxPrint(21, 16, "R");
}

static void drawCursor(int col, int row) {
  if (row < 3) return instrumentCommonDrawCursor(col, row);
  if (row == 13) gfxCursor(7 + col * 5, 16, 2);
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
  if (row == 13) {
    uint8_t values[] = {sample->attack, sample->decay, sample->sustain, sample->release};
    gfxPrint(7 + col * 5, 16, byteToHex(values[col]));
    return;
  }
  gfxClearRect(12, row + 3, 27, 1);
  switch (row) {
    case 3: gfxPrintf(12, 6, "Load %s", sampleFilename(sample->path)); break;
    case 4: gfxPrintf(12, 7, "%+d st", sample->pitch); break;
    case 5: gfxPrint(12, 8, byteToHex(sample->start)); break;
    case 6: gfxPrint(12, 9, byteToHex(sample->end)); break;
    case 7: gfxPrintf(12, 10, "%03d", sample->volume); break;
    case 8: gfxPrint(12, 11, sample->filterEnabled ? "On" : "Off"); break;
    case 9: { static const char* modes[] = {"LP", "BP", "HP"}; gfxPrint(12, 12, modes[sample->filterMode <= 2 ? sample->filterMode : 0]); break; }
    case 10: gfxPrint(12, 13, sample->filterSlope24dB ? "24 dB" : "12 dB"); break;
    case 11: gfxPrintf(12, 14, "%u Hz", sample->filterCutoffHz); break;
    case 12: gfxPrint(12, 15, byteToHex(sample->filterResonance)); break;
  }
}

static int onEdit(int col, int row, CellEditAction action) {
  if (row < 3) return instrumentCommonOnEdit(col, row, action);
  InstrumentSample* sample = &chipnomadState->project.instruments[cInstrument].chip.sample;
  int handled = 0;
  switch (row) {
    case 3:
      if (action == CellEditAction::tap) {
        fileBrowserSetup("LOAD PCM16 SAMPLE", ".wav", appSettings.samplePath, onSampleLoaded, onSampleCancelled);
        screenSetup(&screenFileBrowser, 0);
      }
      return 0;
    case 4: handled = editSigned8(action, &sample->pitch, 12, -48, 48); break;
    case 5: handled = edit8noLast(action, &sample->start, 16, 0, sample->end); break;
    case 6: handled = edit8noLast(action, &sample->end, 16, sample->start, 255); break;
    case 7: handled = edit8noLast(action, &sample->volume, 16, 0, 255); break;
    case 8: handled = edit8noLast(action, &sample->filterEnabled, 1, 0, 1); break;
    case 9: handled = edit8noLast(action, &sample->filterMode, 1, 0, 2); break;
    case 10: handled = edit8noLast(action, &sample->filterSlope24dB, 1, 0, 1); break;
    case 11: handled = edit16withMinMax(action, &sample->filterCutoffHz, 1000, 20, 43200); break;
    case 12: handled = edit8noLast(action, &sample->filterResonance, 16, 0, 255); break;
    case 13: {
      uint8_t* values[] = {&sample->attack, &sample->decay, &sample->sustain, &sample->release};
      handled = edit8noLast(action, values[col], 16, 0, 255);
      break;
    }
  }
  if (handled) projectModified = 1;
  return handled;
}

ScreenData screenInstrumentSample = {
  .rows = 14, .cursorRow = 0, .cursorCol = 0, .topRow = 0,
  .selectMode = -1, .selectStartRow = 0, .selectStartCol = 0,
  .selectAnchorRow = 0, .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = getColumnCount, .drawStatic = drawStatic,
  .drawCursor = drawCursor, .drawSelection = NULL,
  .drawRowHeader = NULL, .drawColHeader = NULL, .drawField = drawField,
  .onEdit = onEdit, .onInput = NULL, .onRawInput = NULL,
  .isCellValid = NULL, .getLoopRange = NULL,
};
