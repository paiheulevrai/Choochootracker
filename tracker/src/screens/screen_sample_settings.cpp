#include "screens.h"
#include "common.h"
#include "corelib_gfx.h"
#include "corelib/corelib_file.h"
#include "chipnomad_lib.h"
#include "screen_instrument.h"
#include "waveform_display.h"
#include "synth/sample_voice.h"
#include <string.h>

static constexpr int valueX = 9;
static constexpr int valueWidth = 7;
static constexpr int previewRow = 4;
static constexpr int previewWidth = 32;
static constexpr int previewHeight = 6;
static constexpr int fieldRow0 = 11;
static const char* sliceLabels[] = {"Off", "2", "4", "8", "16", "32"};
static const uint8_t sliceValues[] = {0, 2, 4, 8, 16, 32};
static constexpr int sliceCount = 6;

static Bitmap* samplePreviewBitmap;
static Bitmap* sampleSliceMarkerBitmap;
static Bitmap* sampleStartMarkerBitmap;
static Bitmap* sampleEndMarkerBitmap;

static int sliceToIndex(uint8_t slice) {
  for (int i = 1; i < sliceCount; ++i) {
    if (sliceValues[i] == slice) return i;
  }
  return 0;
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

static InstrumentSample* currentSample(void) {
  return &chipnomadState->project.instruments[cInstrument].chip.sample;
}

static Bitmap* ensurePreviewBitmap(Bitmap** bitmap) {
  if (*bitmap && ((*bitmap)->widthChars != previewWidth || (*bitmap)->heightChars != previewHeight)) {
    gfxBitmapFree(*bitmap);
    *bitmap = NULL;
  }
  if (!*bitmap) *bitmap = gfxBitmapCreate(previewWidth, previewHeight);
  return *bitmap;
}

static uint8_t sampleSliceDivisions(const InstrumentSample* sample) {
  return sampleNormalizeSlice(sample->slice);
}

static void updateSamplePreview(const InstrumentSample* sample) {
  Bitmap* waveform = ensurePreviewBitmap(&samplePreviewBitmap);
  Bitmap* markers = ensurePreviewBitmap(&sampleSliceMarkerBitmap);
  Bitmap* startMarker = ensurePreviewBitmap(&sampleStartMarkerBitmap);
  Bitmap* endMarker = ensurePreviewBitmap(&sampleEndMarkerBitmap);
  // Calculate actual start/end positions in frames
  uint32_t frameCount = sample->frameCount;
  uint32_t startFrame = frameCount ? (uint64_t)sample->start * (frameCount - 1) / 255 : 0;
  uint32_t endFrame = sample->end == 255 ? frameCount :
      (uint64_t)(sample->end + 1) * frameCount / 256;
  if (startFrame > endFrame) { uint32_t swap = startFrame; startFrame = endFrame; endFrame = swap + 1; }

  // Determine if we should use start/end or full range
  uint8_t slices = sampleSliceDivisions(sample);

  // Always show full waveform so we can grey out inactive regions
  // The start/end markers and grey areas will indicate the active loop region
  uint32_t displayStart = 0;
  uint32_t displayEnd = frameCount;

  // Render the waveform (always show full sample)
  renderPCM16Preview(waveform, sample->data, displayStart, displayEnd, sample->channels);

  // Create start marker bitmap (single vertical line)
  if (startMarker) {
    gfxBitmapClear(startMarker);
    int width = startMarker->widthPixels;
    int height = startMarker->heightPixels;
    int startX = startFrame >= frameCount ? width - 1 : (int)((uint64_t)startFrame * width / frameCount);
    if (startX >= 0 && startX < width) {
      for (int y = 0; y < height; y++) {
        startMarker->data[y * width + startX] = 255;
      }
    }
  }
  // Create end marker bitmap (single vertical line)
  if (endMarker) {
    gfxBitmapClear(endMarker);
    int width = endMarker->widthPixels;
    int height = endMarker->heightPixels;
    int endX = endFrame >= frameCount ? width - 1 : (int)((uint64_t)endFrame * width / frameCount);
    if (endX >= 0 && endX < width) {
      for (int y = 0; y < height; y++) {
        endMarker->data[y * width + endX] = 255;
      }
    }
  }
  // Adjust waveform brightness
  // - Active area (between start/end): waveform at 255 (light blue)
  // - Inactive area (before start, after end): ONLY waveform pixels greyed to 48, background stays 0
  if (waveform) {
    int width = waveform->widthPixels;
    int height = waveform->heightPixels;
    int startX = startFrame >= frameCount ? width - 1 : (int)((uint64_t)startFrame * width / frameCount);
    int endX = endFrame >= frameCount ? width - 1 : (int)((uint64_t)endFrame * width / frameCount);

    // Apply greying ONLY to waveform pixels (value 255) in inactive regions
    for (int x = 0; x < width; x++) {
      for (int y = 0; y < height; y++) {
        if ((x < startX || x >= endX) && waveform->data[y * width + x] == 255) {
          // Inactive waveform pixel: dark gray
          waveform->data[y * width + x] = 48;
        }
        // Active waveform pixels stay at 255 (light blue)
        // Background pixels (0) stay at 0 in both areas
      }
    }
  }
  // Create slice markers
  if (markers) gfxBitmapClear(markers);
  if (markers && slices && markers->widthPixels > 0) {
    int width = markers->widthPixels;
    int height = markers->heightPixels;

    // When in slice mode, slice markers divide the loop region (start to end)
    // This ensures slices follow the start/end markers
    uint32_t loopLength = endFrame > startFrame ? (endFrame - startFrame) : frameCount;
    if (loopLength == 0) loopLength = frameCount;

    // Calculate pixel positions for start and end of loop
    int startX = startFrame >= frameCount ? 0 : (int)((uint64_t)startFrame * width / frameCount);
    int endX = endFrame >= frameCount ? width - 1 : (int)((uint64_t)endFrame * width / frameCount);

    // Draw slice markers within the loop region
    for (int i = 1; i < slices; ++i) {
      // Position is relative to the loop region, then mapped to full width
      float positionInLoop = (float)i / slices;
      int x = startX + (int)(positionInLoop * (endX - startX));
      if (x < 0) x = 0;
      if (x >= width) x = width - 1;
      for (int y = 0; y < height; ++y) markers->data[y * width + x] = 255;
    }
  }
}

static void drawSamplePreview(void) {
  // Clear the waveform area and space for frame
  gfxClearRect(0, previewRow, previewWidth, previewHeight);
  // Draw the waveform with light blue color for active area
  if (samplePreviewBitmap) {
    gfxSetFgColor(0xADD8E6); // Light blue
    gfxDrawBitmap(samplePreviewBitmap, 0, previewRow);
  }
  // Draw start marker (yellow) - always shown
  if (sampleStartMarkerBitmap) {
    gfxSetFgColor(0xFFFF00); // Yellow
    gfxDrawBitmap(sampleStartMarkerBitmap, 0, previewRow);
  }
  // Draw end marker (orange) - always shown
  if (sampleEndMarkerBitmap) {
    gfxSetFgColor(0xFFA500); // Orange
    gfxDrawBitmap(sampleEndMarkerBitmap, 0, previewRow);
  }
  // Draw the slice markers on top
  if (sampleSliceMarkerBitmap) {
    gfxSetFgColor(appSettings.colorScheme.textDefault);
    gfxDrawBitmap(sampleSliceMarkerBitmap, 0, previewRow);
  }
}

static int settingsColumnCount(int row) {
  (void)row;
  return 1;
}

static void settingsDrawStatic(void) {
  const ColorScheme cs = appSettings.colorScheme;
  InstrumentSample* sample = currentSample();
  gfxSetFgColor(cs.textTitles);
  gfxPrint(0, 0, "SAMPLE SETTINGS");
  gfxSetFgColor(cs.textDefault);
  gfxPrint(0, 2, shortSampleFilename(sample->path, 32));
  updateSamplePreview(sample);
  drawSamplePreview();
  gfxSetFgColor(cs.textDefault);
  gfxPrint(0, fieldRow0, "Start");
  gfxPrint(0, fieldRow0 + 1, "End");
  gfxPrint(0, fieldRow0 + 2, "Slice");
}

static void settingsDrawCursor(int col, int row) {
  (void)col;
  gfxCursor(valueX, fieldRow0 + row, row == 2 ? 3 : valueWidth);
}

static void settingsDrawRowHeader(int row, CellState state) {
  (void)row;
  (void)state;
}

static void settingsDrawColHeader(int col, CellState state) {
  (void)col;
  (void)state;
}

static void settingsDrawField(int col, int row, CellState state) {
  (void)col;
  InstrumentSample* sample = currentSample();
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  gfxClearRect(valueX, fieldRow0 + row, valueWidth, 1);
  if (row == 0) gfxPrint(valueX, fieldRow0, byteToHex(sample->start));
  else if (row == 1) gfxPrint(valueX, fieldRow0 + 1, byteToHex(sample->end));
  else if (row == 2) {
    // Slice is inert while Stretch drives the duration: dim it.
    if (sample->stretchMode != 0) gfxSetFgColor(appSettings.colorScheme.textEmpty);
    gfxPrint(valueX, fieldRow0 + 2, sliceLabels[sliceToIndex(sample->slice)]);
  }
}

static int settingsOnEdit(int col, int row, CellEditAction action) {
  (void)col;
  InstrumentSample* sample = currentSample();
  int handled = 0;
  if (row == 0) handled = edit8noLast(action, &sample->start, 16, 0, 255);
  else if (row == 1) handled = edit8noLast(action, &sample->end, 16, 0, 255);
  else if (row == 2) {
    // Slice is inert while Stretch drives the duration.
    if (sample->stretchMode != 0) return 0;
    uint8_t index = (uint8_t)sliceToIndex(sample->slice);
    handled = edit8noLast(action, &index, 1, 0, sliceCount - 1);
    if (handled) {
      sample->slice = sliceValues[index];
      // Stretch and Slice are mutually exclusive: enabling one disables the other.
      if (sample->slice) sample->stretchMode = 0;
      projectModified = 1;
      updateSamplePreview(sample);
      drawSamplePreview();
    }
    return handled;
  }
  if (handled) {
    projectModified = 1;
    updateSamplePreview(sample);
    drawSamplePreview();
  }
  return handled;
}

// Slice is inert while Stretch drives the duration: skip it in navigation.
static int settingsIsCellValid(int col, int row) {
  (void)col;
  if (row == 2 && currentSample()->stretchMode != 0) return 0;
  return 1;
}

static ScreenData screenSampleSettingsData = {
  .rows = 3,
  .cursorRow = 0,
  .cursorCol = 0,
  .topRow = 0,
  .selectMode = -1,
  .selectStartRow = 0,
  .selectStartCol = 0,
  .selectAnchorRow = 0,
  .selectAnchorCol = 0,
  .playbackLevel = ScreenPlaybackLevel::none,
  .getColumnCount = settingsColumnCount,
  .drawStatic = settingsDrawStatic,
  .drawCursor = settingsDrawCursor,
  .drawSelection = NULL,
  .drawRowHeader = settingsDrawRowHeader,
  .drawColHeader = settingsDrawColHeader,
  .drawField = settingsDrawField,
  .onEdit = settingsOnEdit,
  .onInput = NULL,
  .onRawInput = NULL,
  .isCellValid = settingsIsCellValid,
  .getLoopRange = NULL,
};

static void setup(int input) {
  if (input != -1) cInstrument = input;
}

static void fullRedraw(void) {
  // The cursor persists across screens: if it is parked on Slice while Stretch
  // is active, move it up so it never rests on a disabled cell.
  if (screenSampleSettingsData.cursorRow == 2 && currentSample()->stretchMode != 0) {
    screenSampleSettingsData.cursorRow = 1;
  }
  screenFullRedraw(&screenSampleSettingsData);
}

static void draw(void) {
}

static int inputScreenNavigation(int keys) {
  if (keys == keyOpt || keys == (keyLeft | keyShift)) {
    screenSetup(&screenInstrument, cInstrument);
    return 1;
  }
  if (keys == (keyRight | keyShift)) {
    screenSetup(&screenTable, cInstrument);
    return 1;
  }
  if (keys == (keyDown | keyShift)) {
    screenSetup(&screenInstrumentPool, cInstrument);
    return 1;
  }
  if (keys == (keyUp | keyShift)) {
    screenSetup(&screenModulation, cInstrument);
    return 1;
  }
  return 0;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (inputScreenNavigation(keys)) return 1;
  return screenInput(&screenSampleSettingsData, isKeyDown, keys, tapCount);
}

static ScreenPlaybackLevel getPlaybackLevel(void) {
  return ScreenPlaybackLevel::phrase;
}

const AppScreen screenSampleSettings = {
  .init = NULL,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = getPlaybackLevel,
};
