#include "screens.h"
#include "common.h"
#include "corelib_gfx.h"
#include "corelib/corelib_file.h"
#include "chipnomad_lib.h"
#include "screen_instrument.h"
#include "waveform_display.h"
#include "file_browser.h"
#include "screen_enter_name.h"
#include "synth/sample_voice.h"
#include "synth/sample_ops.h"
#include "synth/sample_transient.h"
#include "audio_manager.h"
#include <stdio.h>
#include <string.h>

// Field geometry (32 columns; the shared instrument panel starts at x=34).
// Row labels sit at x=0; values start at x=9.
static constexpr int valueX = 9;
// Process op and File action share the widest field ("Normalize" = 9 chars)
static constexpr int opWidth = 9;
static constexpr int sliceWidth = 6;
static constexpr int goX = 19;
static constexpr int goWidth = 3;
static constexpr int undoX = 24;
static constexpr int undoWidth = 6;
static constexpr int fileGoX = 19;
static constexpr int fileGoWidth = 3;
// Region/Select rows carry two values: "START [] END []"
static constexpr int markerLabelX = 9;   // "START"
static constexpr int selValX = 15;
static constexpr int selValWidth = 6;
static constexpr int endLabelX = 22;     // "END"
static constexpr int selEndValX = 26;
// A blank row separates the preview from the field block; the File row
// stays above the message line (screen row 19).
static constexpr int previewRow = 3;
static constexpr int previewWidth = 32;
static constexpr int previewHeight = 8;
static constexpr int fieldRow0 = 12;
static const char* speedAlgorithmLabels[] = {"Dirty", "Clean"};

// Slice row: one row hosting four value cells - Mode / Count / Slice /
// Frame. The label sits at x=0; values start at x=6. No per-cell labels,
// per the "only value boxes" rule.
static constexpr int sliceModeX = 6;
static constexpr int sliceModeW = 5;   // OFF / EQUAL / AUTO / LAZY
static constexpr int sliceNumX = 12;
static constexpr int sliceNumW = 2;    // total slice count
static constexpr int sliceBrowseX = 15;
static constexpr int sliceBrowseW = 2; // current slice index, 1-indexed
static constexpr int sliceFrameX = 18;
static constexpr int sliceFrameW = 6;  // start frame of the current slice
static const char* sliceModeLabels[] = {"OFF", "EQUAL", "AUTO", "LAZY"};
// Session-only "current slice" being edited (0-based index into
// sliceBounds). Never saved with the project.
static int currentSlice;

// Process toolbox: one selected operation plus GO/UNDO buttons. The op is
// cycled with Edit+Left/Right; Edit+Opt clears it to "none".
static const char* processOpLabels[] = {"Crop", "Normalize", "Delete", "Silence", "Fade In", "Fade Out", "Reverse"};
static const char* processOpDoneMessages[] = {"Cropped", "Normalized", "Deleted", "Silenced", "Faded in", "Faded out", "Reversed"};
static constexpr int processOpCount = 7;
static int processOp; // index into processOpLabels, -1 = none

// File row action: 0 = Save (overwrite), 1 = Save As (new file). GO runs it.
static int fileAction;

// One-level undo slot for the process tools. Screen module state: never
// saved with the project, dropped when the screen is (re)entered.
static SampleUndo editorUndo;

// Set when the sample data in RAM differs from the file on disk. Cleared by
// the Save flows; shown as a '*' before the filename.
static int sampleDirtyToDisk;

// Dialog round trips re-enter the screen, which resets the session state.
// The dirty flag travels through this slot so a cancelled save or a rename
// cannot make unsaved changes look saved.
static int pendingDirtyRestore;

// File name (without extension) captured between the Save As name entry and
// the folder browser.
static char saveAsName[64];

// Stashed slice setting captured when leaving EQUAL/AUTO for LAZY: the
// sentinel (mode + count) and the bounds array. Switching back restores
// it verbatim - no detection re-run, no confirmation dialog. Keyed by
// instrument index so one sample's stash never leaks into another.
static int sliceStashInstrument = -1;
static uint8_t sliceStashSlice;
static uint32_t sliceStashBounds[PROJECT_SAMPLE_MAX_SLICES];

static Bitmap* samplePreviewBitmap;
static Bitmap* sampleSliceMarkerBitmap;
static Bitmap* sampleStartMarkerBitmap;
static Bitmap* sampleEndMarkerBitmap;
static Bitmap* sampleSelectionBitmap;
static Bitmap* sampleSliceBandBitmap;
static Bitmap* samplePlaybackMarkerBitmap;

// Total slice count for the current sentinel (0 when off). Legacy samples
// (sentinel set, bounds empty) still report the sentinel count - the
// preview falls back to the even division for them.
static uint8_t sampleSliceDivisions(const InstrumentSample* sample) {
  return sampleDecodeSliceCount(sample->slice);
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

// Basename without extension: "dir/loop.wav" -> "loop". A name with no dot
// (or a leading dot) is returned unchanged.
static void sampleBasenameSansExt(const char* path, char* out, size_t outSize) {
  const char* name = sampleFilename(path);
  const char* dot = strrchr(name, '.');
  size_t length = dot && dot != name ? (size_t)(dot - name) : strlen(name);
  if (length >= outSize) length = outSize - 1;
  memcpy(out, name, length);
  out[length] = 0;
}

static InstrumentSample* currentSample(void) {
  return &chipnomadState->project.instruments[cInstrument].chip.sample;
}

// Contextual combo hints (session-only): while the cursor rests on a Slice
// row cell, draw() shows a hint for that cell in the message bar. Every
// Slice cell has one persistent hint, re-issued every frame: the mode
// cell's hint is bound to the active slice mode (switches instantly when
// the mode changes, disappears when the mode turns OFF), the other cells
// show their fixed combo description. All hints pause while any other
// message is active.
static int hintCursorRow = -1;
static int hintCursorCol = -1;
// Text the hint system last put in the message bar ("" = none). The bar is
// shared with action feedback, so draw() compares the active message
// against this to tell our hint apart from a foreign message.
static char hintOwnedText[48];

// Advance the hint for the given cell. Called from draw() when the message
// bar is empty or holds the hint we set. ownsBar tells whether the bar
// currently shows our hint (a persistent hint re-arms every frame; a
// foreign message pauses everything until it expires).
static void settingsUpdateHint(int row, int col, int ownsBar) {
  const int changed = row != hintCursorRow || col != hintCursorCol;
  if (changed) {
    hintCursorRow = row;
    hintCursorCol = col;
  }
  const char* issued = NULL;
  if (row == 2 && col == 0) {
    // Slice mode cell: describe what the active mode does. The mode name
    // is already in the cell, so the hint carries only the description;
    // it is bound to the mode (re-issued every frame so a mode switch
    // swaps the text immediately), and OFF clears the bar.
    const SliceMode mode = sampleDecodeSliceMode(currentSample()->slice);
    static const char* modeHints[] = {
      "Divides sample in equal parts",
      "Divides sample based on transients",
      "Press Play and add slices with EDIT",
    };
    if (mode >= sliceModeEqual && mode <= sliceModeLazy) {
      issued = modeHints[mode - 1];
    }
  } else if (row == 2 && col == 1) {
    // Slice count box: single static hint.
    issued = "Adjust the number of slices";
  } else if (row == 2 && col == 2) {
    // Slice browser box: single static hint.
    issued = "Browse slices";
  } else if (row == 2 && col == 3) {
    // Slice frame cell: single static hint.
    issued = "Adjust slice start";
  }
  if (issued) {
    if (!ownsBar || changed) {
      screenMessage(2, "%s", issued);
      // Copy the bar text (post-truncation) so the ownership comparison
      // next frame matches exactly what is displayed.
      snprintf(hintOwnedText, sizeof(hintOwnedText), "%s",
               screenGetActiveMessage());
    }
  } else if (hintOwnedText[0]) {
    // No hint for this cell (e.g. the mode turned OFF): drop our hint.
    screenClearMessage();
    hintOwnedText[0] = '\0';
  }
}

static Bitmap* ensurePreviewBitmap(Bitmap** bitmap) {
  if (*bitmap && ((*bitmap)->widthChars != previewWidth || (*bitmap)->heightChars != previewHeight)) {
    gfxBitmapFree(*bitmap);
    *bitmap = NULL;
  }
  if (!*bitmap) *bitmap = gfxBitmapCreate(previewWidth, previewHeight);
  return *bitmap;
}

// Which marker the view window follows: 0 none, 1 Start, 2 End,
// 3/4 the selection handles.
enum {
  kViewAnchorNone = 0,
  kViewAnchorStart = 1,
  kViewAnchorEnd = 2,
  kViewAnchorSelStart = 3,
  kViewAnchorSelEnd = 4,
};

// View window into the sample: viewStart is the first visible frame,
// viewEnd one past the last visible frame (<= frameCount).
struct SampleEditorView {
  uint32_t viewStart;
  uint32_t viewEnd;
  int anchor;
};

static SampleEditorView editorView;

// Processing selection: the region the process tools (editor phase 2) act
// on. Independent from the playback Start/End markers and stored in frames,
// not 0-255 normalized values. Session-only editor state: never saved with
// the project; entering the screen seeds it with the playback Region span.
struct SampleEditorSelection {
  uint32_t start;  // inclusive frame
  uint32_t end;    // exclusive frame; start == end => empty
  uint8_t active;  // 0 = empty/none
};

static SampleEditorSelection editorSelection;

// Set while a fine adjustment (EDIT+LEFT/RIGHT) holds the zoomed view in;
// releasing EDIT drops the view back to the full sample. Session-only.
static int zoomHoldActive;

// Set after a LAZY playback-drop consumed an EDIT press; re-armed on the
// EDIT release. Key repeat re-fires onInput with isKeyDown=1, so without
// this guard a held EDIT would machine-gun slices at the repeat rate.
static int lazyEditArmed;

// Latch for the LAZY playback flag: set once the draw loop has OBSERVED the
// preview actually running (phrase row + active voice). Until then the
// auto-clear in draw() is suppressed - the queued start command takes a
// frame or two to reach the audio thread, and clearing the flag on that
// first frame broke the marker and the app-level key-up guard.
static int lazyPlaybackSeenActive;

// Set while an A-tap slice preview (EDIT tap on the Number or Frame cell)
// is sounding. Hold-preview semantics come from app.cpp's auto-stop on
// keys==0 release; this flag only guards re-entry (a second tap while the
// preview still sounds restarts it) and the navigation stop.
static int slicePreviewActive;

// Smallest zoomed window in frames
static constexpr uint32_t kMinViewSpan = 8;

// Fixed zoom span for fine adjustments: one second of audio, clamped to
// the sample length. Samples that fit inside the window keep the full 1:1
// view; longer ones always show the same time span, so the window stays
// readable on short one-shots and long recordings alike.
static uint32_t zoomSpan(const InstrumentSample* sample) {
  const uint32_t frameCount = sample->frameCount;
  uint32_t rate = sample->sampleRate;
  if (rate == 0) rate = 44100; // fresh samples: assume the default rate
  const uint64_t window = (uint64_t)rate;
  if (frameCount <= window) return frameCount; // whole sample fits: 1:1
  uint32_t span = (uint32_t)window;
  if (span < kMinViewSpan) span = kMinViewSpan;
  return span;
}

// Frame position of the view anchor: a playback marker (1 = Start, 2 = End,
// before the reverse-order swap - the zoom anchor follows the marker itself,
// not the active region) or a selection handle (3/4).
static uint32_t anchorFrame(const InstrumentSample* sample,
                            const SampleEditorSelection* selection, int anchor) {
  switch (anchor) {
    case kViewAnchorStart: return sampleMarkerToStartFrame(sample->frameCount, sample->start);
    case kViewAnchorEnd: return sampleMarkerToEndFrame(sample->frameCount, sample->end);
    case kViewAnchorSelStart: return selection->start;
    case kViewAnchorSelEnd: return selection->end;
    default: return 0;
  }
}

static void zoomOutFull(const InstrumentSample* sample, SampleEditorView* view) {
  view->viewStart = 0;
  view->viewEnd = sample->frameCount;
  view->anchor = kViewAnchorNone;
}

// Maps an absolute frame to a pixel column inside the view window. Returns
// -1 when the frame is outside the window. A frame exactly on viewEnd pins
// to the last column only when pinToEnd is set (the end marker is an
// exclusive bound drawn at its limit).
static int frameToPixel(uint32_t frame, const SampleEditorView* view, int width, int pinToEnd) {
  if (width <= 0 || view->viewEnd <= view->viewStart) return -1;
  if (frame < view->viewStart || frame > view->viewEnd) return -1;
  if (frame == view->viewEnd) return pinToEnd ? width - 1 : -1;
  int x = (int)((uint64_t)(frame - view->viewStart) * width / (view->viewEnd - view->viewStart));
  return x < width ? x : width - 1;
}

// Zooms to the fixed span (one second of audio) around markerFrame, keeping
// the marker at its relative position inside the window (the first zoom from
// the full view centers it). When the marker would sit at a window edge it
// pans just enough to keep a small margin, so repeated fine steps follow
// the marker without drift. The span never shrinks: every fine step shows
// the same window size.
static void zoomToMarker(const InstrumentSample* sample, SampleEditorView* view,
                         uint32_t markerFrame) {
  const uint32_t frameCount = sample->frameCount;
  if (frameCount == 0 || sample->data == NULL) {
    zoomOutFull(sample, view);
    return;
  }

  const uint32_t newSpan = zoomSpan(sample);
  if (newSpan >= frameCount) {
    zoomOutFull(sample, view);
    return;
  }

  const uint32_t span = view->viewEnd - view->viewStart;
  uint32_t viewStart;
  const int fullView = span == 0 || (view->viewStart == 0 && view->viewEnd == frameCount);
  if (fullView || markerFrame < view->viewStart || markerFrame > view->viewEnd) {
    // First zoom-in (or the marker left the window): center on the marker
    viewStart = markerFrame >= newSpan / 2 ? markerFrame - newSpan / 2 : 0;
  } else {
    // Keep the marker at its relative position inside the window
    const uint64_t relative = (uint64_t)(markerFrame - view->viewStart) * newSpan / span;
    viewStart = markerFrame >= relative ? markerFrame - (uint32_t)relative : 0;
  }

  // Pan so the marker stays visible with a margin at the hit edge
  const uint32_t margin = newSpan / 8 > 0 ? newSpan / 8 : 1;
  if (markerFrame < viewStart + margin) {
    viewStart = markerFrame >= margin ? markerFrame - margin : 0;
  } else if ((uint64_t)markerFrame + margin > (uint64_t)viewStart + newSpan) {
    viewStart = (uint64_t)markerFrame + margin > newSpan
      ? (uint32_t)((uint64_t)markerFrame + margin - newSpan) : 0;
  }

  if (viewStart + newSpan > frameCount) viewStart = frameCount - newSpan;
  view->viewStart = viewStart;
  view->viewEnd = viewStart + newSpan;
}

static void updateSamplePreview(const InstrumentSample* sample, const SampleEditorView* view,
                                const SampleEditorSelection* selection) {
  Bitmap* waveform = ensurePreviewBitmap(&samplePreviewBitmap);
  Bitmap* markers = ensurePreviewBitmap(&sampleSliceMarkerBitmap);
  Bitmap* startMarker = ensurePreviewBitmap(&sampleStartMarkerBitmap);
  Bitmap* endMarker = ensurePreviewBitmap(&sampleEndMarkerBitmap);
  Bitmap* selectionBand = ensurePreviewBitmap(&sampleSelectionBitmap);
  Bitmap* sliceBand = ensurePreviewBitmap(&sampleSliceBandBitmap);
  if (!waveform || !markers || !startMarker || !endMarker || !selectionBand || !sliceBand) return;

  // Calculate actual start/end positions in frames
  uint32_t frameCount = sample->frameCount;
  uint32_t startFrame = sampleMarkerToStartFrame(frameCount, sample->start);
  uint32_t endFrame = sampleMarkerToEndFrame(frameCount, sample->end);
  if (startFrame > endFrame) { uint32_t swap = startFrame; startFrame = endFrame; endFrame = swap + 1; }

  // Determine if we should use start/end or full range
  uint8_t slices = sampleSliceDivisions(sample);

  // Render only the view window; markers and greying map through it
  renderPCM16Preview(waveform, sample->data, view->viewStart, view->viewEnd, sample->channels);

  int width = waveform->widthPixels;
  int height = waveform->heightPixels;
  uint32_t viewSpan = view->viewEnd - view->viewStart;

  // Create start marker bitmap (single vertical line); skipped when the
  // marker sits outside the view window
  gfxBitmapClear(startMarker);
  {
    int startX = frameToPixel(startFrame, view, width, 0);
    if (startX >= 0) {
      for (int y = 0; y < height; y++) {
        startMarker->data[y * width + startX] = 255;
      }
    }
  }
  // Create end marker bitmap (single vertical line). It is an exclusive
  // bound, so a marker on the window's right edge pins to the last column.
  gfxBitmapClear(endMarker);
  {
    int endX = frameToPixel(endFrame, view, width, 1);
    if (endX >= 0) {
      for (int y = 0; y < height; y++) {
        endMarker->data[y * width + endX] = 255;
      }
    }
  }
  // Adjust waveform brightness
  // - Active area (between start/end): waveform at 255 (light blue)
  // - Inactive area (before start, after end): ONLY waveform pixels greyed to 48, background stays 0
  // A column is active when its frame range intersects [startFrame, endFrame);
  // an empty region (Start == End) leaves every column inactive.
  {
    for (int x = 0; x < width; x++) {
      uint32_t columnStart = view->viewStart + (uint64_t)x * viewSpan / width;
      uint32_t columnEnd = view->viewStart + (uint64_t)(x + 1) * viewSpan / width;
      if (columnEnd > view->viewEnd) columnEnd = view->viewEnd;
      if (columnEnd > startFrame && columnStart < endFrame && endFrame > startFrame) continue;
      for (int y = 0; y < height; y++) {
        if (waveform->data[y * width + x] == 255) {
          // Inactive waveform pixel: dark gray
          waveform->data[y * width + x] = 48;
        }
        // Active waveform pixels stay at 255 (light blue)
        // Background pixels (0) stay at 0 in both areas
      }
    }
  }
  // Create slice markers (2px wide, dark orange): markers come from
  // sliceBounds when populated (manual edits, AUTO detection); legacy
  // samples with empty bounds keep the even division of the loop region.
  // The active slice gets a black background band over its frame range for
  // visual separation (color coding TBD); its start marker is drawn
  // brighter (255) than the others (160).
  gfxBitmapClear(markers);
  const int boundsPopulated = sample->sliceBounds[0] != 0 ||
    (slices > 1 && sample->sliceBounds[1] != 0);
  if (slices) {
    for (int i = 0; i < slices; ++i) {
      uint32_t position;
      if (boundsPopulated) {
        position = sample->sliceBounds[i];
      } else {
        // Even division of the loop region (start to end markers)
        uint32_t loopLength = endFrame > startFrame ? (endFrame - startFrame) : frameCount;
        if (loopLength == 0) loopLength = frameCount;
        position = startFrame + (uint32_t)((uint64_t)loopLength * i / slices);
      }
      int x = frameToPixel(position, view, width, 0);
      if (x < 0) continue;
      const uint8_t brightness = i == currentSlice ? 255 : 160;
      for (int y = 0; y < height; ++y) {
        markers->data[y * width + x] = brightness;
        if (x + 1 < width) markers->data[y * width + x + 1] = brightness;
      }
    }
  }
  // Active-slice band: opaque background over the columns covered by the
  // current slice's frame range. Drawn in black UNDER the waveform: the
  // waveform's opaque pixels cover the band, its transparent background
  // lets the band show through. The frame range is kept for the selection
  // fill below, which skips these columns so the band stays pure black
  // (the tint would otherwise wash it out to near-background).
  gfxBitmapClear(sliceBand);
  uint32_t sliceStart = 0;
  uint32_t sliceEnd = 0;
  int sliceHasRange = 0;
  if (currentSlice >= 0 && currentSlice < slices) {
    sliceStart = boundsPopulated
      ? sample->sliceBounds[currentSlice]
      : startFrame + (uint32_t)((uint64_t)(endFrame > startFrame ? (endFrame - startFrame) : frameCount) * currentSlice / slices);
    if (boundsPopulated) {
      sliceEnd = currentSlice + 1 < slices ? sample->sliceBounds[currentSlice + 1] : endFrame;
    } else {
      uint32_t loopLength = endFrame > startFrame ? (endFrame - startFrame) : frameCount;
      if (loopLength == 0) loopLength = frameCount;
      sliceEnd = startFrame + (uint32_t)((uint64_t)loopLength * (currentSlice + 1) / slices);
    }
    if (sliceEnd > frameCount) sliceEnd = frameCount;
    if (sliceStart < sliceEnd) {
      sliceHasRange = 1;
      for (int x = 0; x < width; x++) {
        uint32_t columnStart = view->viewStart + (uint64_t)x * viewSpan / width;
        uint32_t columnEnd = view->viewStart + (uint64_t)(x + 1) * viewSpan / width;
        if (columnEnd > view->viewEnd) columnEnd = view->viewEnd;
        if (columnEnd <= sliceStart || columnStart >= sliceEnd) continue;
        for (int y = 0; y < height; y++) {
          sliceBand->data[y * width + x] = 255;
        }
      }
    }
  }
  // Selection band + handles: dim fill over columns overlapping
  // [selection->start, selection->end), bright lines at the handles.
  // Positions map through the view window; handles outside are skipped.
  gfxBitmapClear(selectionBand);
  if (selection->active) {
    for (int x = 0; x < width; x++) {
      uint32_t columnStart = view->viewStart + (uint64_t)x * viewSpan / width;
      uint32_t columnEnd = view->viewStart + (uint64_t)(x + 1) * viewSpan / width;
      if (columnEnd > view->viewEnd) columnEnd = view->viewEnd;
      if (columnEnd <= selection->start || columnStart >= selection->end) continue;
      // Skip the active slice's columns: its black band must not be
      // washed out by the tint (this is what keeps it maximally dark
      // and distinct from the tinted rest of the preview).
      if (sliceHasRange && columnEnd > sliceStart && columnStart < sliceEnd) continue;
      for (int y = 0; y < height; y++) selectionBand->data[y * width + x] = 96;
    }
    int selStartX = frameToPixel(selection->start, view, width, 0);
    if (selStartX >= 0) {
      for (int y = 0; y < height; y++) selectionBand->data[y * width + selStartX] = 255;
    }
    int selEndX = frameToPixel(selection->end, view, width, 1);
    if (selEndX >= 0) {
      for (int y = 0; y < height; y++) selectionBand->data[y * width + selEndX] = 255;
    }
  }
}

static void drawSamplePreview(void) {
  // Clear the waveform area and space for frame
  gfxClearRect(0, previewRow, previewWidth, previewHeight);
  // Draw the slice band in black UNDER the waveform: the waveform's opaque
  // pixels cover the band, its transparent background lets the band show
  // through.
  if (sampleSliceBandBitmap) {
    gfxSetFgColor(0x000000); // Black
    gfxDrawBitmap(sampleSliceBandBitmap, 0, previewRow);
  }
  // Draw the waveform with light blue color for active area
  if (samplePreviewBitmap) {
    gfxSetFgColor(0xADD8E6); // Light blue
    gfxDrawBitmap(samplePreviewBitmap, 0, previewRow);
  }
  // Draw the selection band + handles (scheme info color, distinct from the
  // yellow/orange playback markers)
  if (sampleSelectionBitmap) {
    gfxSetFgColor(appSettings.colorScheme.textInfo);
    gfxDrawBitmap(sampleSelectionBitmap, 0, previewRow);
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
  // Draw the slice markers on top (dark orange, 2px wide)
  if (sampleSliceMarkerBitmap) {
    gfxSetFgColor(0xFF8C00); // Dark orange
    gfxDrawBitmap(sampleSliceMarkerBitmap, 0, previewRow);
  }
}

static int settingsColumnCount(int row) {
  // Region/Select rows: START + END; Slice row: Mode + Count + Slice +
  // Frame; Process row: op + GO + UNDO; File row: action + GO
  if (row == 0 || row == 1) return 2;
  if (row == 2) return 4;
  if (row == 4) return 3;
  if (row == 5) return 2;
  return 1;
}

// Filename row with the '*' dirty marker. Called from the static draw and
// after process ops so the marker appears without a full redraw.
static void drawFilenameRow(void) {
  InstrumentSample* sample = currentSample();
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxClearRect(0, 1, 32, 1);
  int nameX = 0;
  if (sampleDirtyToDisk) {
    gfxPrint(0, 1, "*");
    nameX = 1;
  }
  gfxPrint(nameX, 1, shortSampleFilename(sample->path, 32 - nameX));
}

static void settingsDrawStatic(void) {
  const ColorScheme cs = appSettings.colorScheme;
  InstrumentSample* sample = currentSample();
  gfxSetFgColor(cs.textTitles);
  gfxPrint(0, 0, "SAMPLE EDIT");
  drawFilenameRow();
  gfxSetFgColor(cs.textInfo);
  if (sample->data && sample->frameCount) {
    char formatText[24];
    snprintf(formatText, sizeof(formatText), "%u Hz %s", (unsigned)sample->sampleRate,
             sample->channels >= 2 ? "STEREO" : "MONO");
    gfxPrint(0, 2, formatText);
  }
  updateSamplePreview(sample, &editorView, &editorSelection);
  drawSamplePreview();
  gfxSetFgColor(cs.textDefault);
  gfxPrint(0, fieldRow0, "Region");
  gfxPrint(markerLabelX, fieldRow0, "START");
  gfxPrint(endLabelX, fieldRow0, "END");
  gfxPrint(0, fieldRow0 + 1, "Select");
  gfxPrint(markerLabelX, fieldRow0 + 1, "START");
  gfxPrint(endLabelX, fieldRow0 + 1, "END");
  gfxPrint(0, fieldRow0 + 2, "Slice");
  gfxPrint(0, fieldRow0 + 3, "Spd algo");
  gfxPrint(0, fieldRow0 + 4, "Process");
  gfxPrint(0, fieldRow0 + 5, "File");
}

static void settingsDrawCursor(int col, int row) {
  if (row == 0 || row == 1) {
    gfxCursor(col == 0 ? selValX : selEndValX, fieldRow0 + row, selValWidth);
  } else if (row == 2) {
    // Slice row: Mode / Count / Slice / Frame cells
    const int x = col == 0 ? sliceModeX : col == 1 ? sliceNumX :
                  col == 2 ? sliceBrowseX : sliceFrameX;
    const int w = col == 0 ? sliceModeW : col == 1 ? sliceNumW :
                  col == 2 ? sliceBrowseW : sliceFrameW;
    gfxCursor(x, fieldRow0 + row, w);
  } else if (row == 4) {
    gfxCursor(col == 0 ? valueX : col == 1 ? goX : undoX, fieldRow0 + row,
              col == 0 ? opWidth : col == 1 ? goWidth : undoWidth);
  } else if (row == 5) {
    gfxCursor(col == 0 ? valueX : fileGoX, fieldRow0 + row, col == 0 ? opWidth : fileGoWidth);
  } else {
    gfxCursor(valueX, fieldRow0 + row, sliceWidth);
  }
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
  InstrumentSample* sample = currentSample();
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  if (row == 4) {
    if (col == 0) {
      // Process op selector
      gfxClearRect(valueX, fieldRow0 + row, opWidth, 1);
      if (processOp < 0) gfxSetFgColor(appSettings.colorScheme.textEmpty);
      gfxPrint(valueX, fieldRow0 + row, processOp < 0 ? "-" : processOpLabels[processOp]);
    } else if (col == 1) {
      gfxClearRect(goX, fieldRow0 + row, goWidth, 1);
      gfxPrint(goX, fieldRow0 + row, "GO");
    } else {
      gfxClearRect(undoX, fieldRow0 + row, undoWidth, 1);
      // UNDO is inert without a prepared slot: dim it
      if (!editorUndo.active) gfxSetFgColor(appSettings.colorScheme.textEmpty);
      gfxPrint(undoX, fieldRow0 + row, "UNDO");
    }
    return;
  }
  if (row == 5) {
    // File action + GO. Save needs a file path, Save As needs sample data
    // (it can assign a path to a fresh sample).
    if (col == 0) {
      gfxClearRect(valueX, fieldRow0 + row, opWidth, 1);
      const char* label = fileAction == 0 ? "Save" : "Save As";
      if (fileAction == 0 && !sample->path[0]) gfxSetFgColor(appSettings.colorScheme.textEmpty);
      if (fileAction == 1 && (!sample->data || !sample->frameCount)) gfxSetFgColor(appSettings.colorScheme.textEmpty);
      gfxPrint(valueX, fieldRow0 + row, label);
    } else {
      gfxClearRect(fileGoX, fieldRow0 + row, fileGoWidth, 1);
      gfxPrint(fileGoX, fieldRow0 + row, "GO");
    }
    return;
  }
  if (row == 3) {
    gfxClearRect(valueX, fieldRow0 + row, sliceWidth, 1);
    gfxPrint(valueX, fieldRow0 + row,
             speedAlgorithmLabels[sample->speedAlgorithm <= 1 ? sample->speedAlgorithm : 0]);
    return;
  }
  if (row == 0 || row == 1) {
    // Region (hex markers) and Select (frame handles): two values per row
    const int x = col == 0 ? selValX : selEndValX;
    gfxClearRect(x, fieldRow0 + row, selValWidth, 1);
    if (row == 0) {
      gfxPrint(x, fieldRow0 + row, byteToHex(col == 0 ? sample->start : sample->end));
    } else {
      if (!editorSelection.active) {
        gfxPrint(x, fieldRow0 + row, "-");
      } else {
        uint32_t frame = col == 0 ? editorSelection.start : editorSelection.end;
        char text[16];
        snprintf(text, sizeof(text), "%06u", (unsigned)frame);
        gfxPrint(x, fieldRow0 + row, text);
      }
    }
    return;
  }
  // Row 2: Slice - Mode / Count / Slice / Frame cells. All four are inert
  // while Stretch drives the duration: dim them. While the LAZY
  // playback-drop is armed, the Frame cell dims too - slices are being
  // placed by the playback marker, not edited by hand.
  {
    const SliceMode mode = sampleDecodeSliceMode(sample->slice);
    const uint8_t count = sampleDecodeSliceCount(sample->slice);
    const int dimmed = sample->stretchMode != 0 ||
      (sampleLazyPlaybackActive && mode == sliceModeLazy && col == 3);
    if (col == 0) {
      gfxClearRect(sliceModeX, fieldRow0 + row, sliceModeW, 1);
      if (dimmed) gfxSetFgColor(appSettings.colorScheme.textEmpty);
      gfxPrint(sliceModeX, fieldRow0 + row, sliceModeLabels[mode <= sliceModeLazy ? mode : 0]);
    } else if (col == 1) {
      // Count box: the total number of slices, always live. Shows "-"
      // in OFF. In LAZY it stays bright (it counts the hand-placed
      // slices) but is display-only: the cursor skips the cell.
      gfxClearRect(sliceNumX, fieldRow0 + row, sliceNumW, 1);
      if (dimmed || mode == sliceModeOff) {
        gfxSetFgColor(appSettings.colorScheme.textEmpty);
      }
      char text[8];
      if (mode == sliceModeOff) {
        gfxPrint(sliceNumX, fieldRow0 + row, "-");
      } else {
        snprintf(text, sizeof(text), "%02d", count);
        gfxPrint(sliceNumX, fieldRow0 + row, text);
      }
    } else if (col == 2) {
      // Slice browser box: the 1-indexed current slice. The browsed slice
      // is also indicated on the waveform by its brighter marker and the
      // black band.
      gfxClearRect(sliceBrowseX, fieldRow0 + row, sliceBrowseW, 1);
      if (dimmed || mode == sliceModeOff) gfxSetFgColor(appSettings.colorScheme.textEmpty);
      char text[8];
      if (mode == sliceModeOff) {
        gfxPrint(sliceBrowseX, fieldRow0 + row, "-");
      } else {
        snprintf(text, sizeof(text), "%02d", currentSlice + 1);
        gfxPrint(sliceBrowseX, fieldRow0 + row, text);
      }
    } else {
      gfxClearRect(sliceFrameX, fieldRow0 + row, sliceFrameW, 1);
      if (dimmed || mode == sliceModeOff) gfxSetFgColor(appSettings.colorScheme.textEmpty);
      char text[16];
      const int32_t frame = sampleSliceStartFrame(sample, (uint8_t)currentSlice,
                                                  sample->start, sample->end);
      if (frame < 0) {
        gfxPrint(sliceFrameX, fieldRow0 + row, "------");
      } else {
        snprintf(text, sizeof(text), "%06X", (unsigned)frame);
        gfxPrint(sliceFrameX, fieldRow0 + row, text);
      }
    }
    return;
  }
}

// Clamp the selection to the sample, swap inverted handles and update the
// active flag; clamp the view window and re-anchor it if its anchor marker
// vanished. Repaints the preview. Shared with the process tools (phase 2).
static void sampleEditorNormalizeState(const InstrumentSample* sample,
                                       SampleEditorSelection* selection,
                                       SampleEditorView* view) {
  const uint32_t frameCount = sample->frameCount;
  if (selection->start > frameCount) selection->start = frameCount;
  if (selection->end > frameCount) selection->end = frameCount;
  if (selection->start > selection->end) {
    uint32_t swap = selection->start;
    selection->start = selection->end;
    selection->end = swap;
  }
  selection->active = selection->start < selection->end;

  if (frameCount == 0) {
    zoomOutFull(sample, view);
  } else {
    if (view->viewEnd > frameCount) view->viewEnd = frameCount;
    if (view->viewStart >= view->viewEnd) zoomOutFull(sample, view);
    if (view->anchor != kViewAnchorNone) {
      // Drop the anchor when its marker no longer exists (empty selection,
      // or a playback marker that fell outside the sample)
      uint32_t frame = anchorFrame(sample, selection, view->anchor);
      if (view->anchor >= kViewAnchorSelStart && !selection->active) {
        view->anchor = kViewAnchorNone;
      } else if (frame < view->viewStart || frame > view->viewEnd) {
        view->anchor = kViewAnchorNone;
      }
    }
  }
  updateSamplePreview(sample, view, selection);
}

// Repaints everything a process op can invalidate: the preview, the
// marker/selection fields and the Process row.
static void settingsRepaintAfterOp(void) {
  drawFilenameRow();
  drawSamplePreview();
  for (int row = 0; row < 3; ++row) {
    const int cols = row == 2 ? 3 : 2;
    for (int col = 0; col < cols; ++col) {
      settingsDrawField(col, row, CellState::normal);
    }
  }
  settingsDrawField(0, 3, CellState::normal);
  settingsDrawField(0, 4, CellState::normal);
  settingsDrawField(1, 4, CellState::normal);
  settingsDrawField(2, 4, CellState::normal);
}

// Runs the selected process op on the current selection (or the whole
// sample when no selection is active). Pauses audio while the buffer is
// swapped or edited, prepares the one-level undo slot first.
static void settingsRunProcessOp(void) {
  InstrumentSample* sample = currentSample();
  if (processOp < 0) {
    screenMessage(MESSAGE_TIME, "Select operation");
    return;
  }
  // Crop and Delete reshape the sample: they need an explicit region.
  if ((processOp == 0 || processOp == 2) && !editorSelection.active) {
    screenMessage(MESSAGE_TIME, "Select region first");
    return;
  }

  uint32_t selStart = editorSelection.active ? editorSelection.start : 0;
  uint32_t selEnd = editorSelection.active ? editorSelection.end : 0;

  audioManager.pause();
  int result = sampleOpPrepareUndo(sample, &editorUndo);
  if (result == sampleOpOk) {
    switch (processOp) {
      case 0: result = sampleOpCrop(sample, selStart, selEnd); break;
      case 1: result = sampleOpNormalize(sample, selStart, selEnd); break;
      case 2: result = sampleOpDelete(sample, selStart, selEnd); break;
      case 3: result = sampleOpSilence(sample, selStart, selEnd); break;
      case 4: result = sampleOpFade(sample, selStart, selEnd, 1); break;
      case 5: result = sampleOpFade(sample, selStart, selEnd, 0); break;
      case 6: result = sampleOpReverse(sample, selStart, selEnd); break;
    }
  }
  audioManager.resume();

  if (result != sampleOpOk) {
    sampleOpFreeUndo(&editorUndo);
    const char* error = "Operation failed";
    switch (result) {
      case sampleOpErrorNoSample: error = "No sample loaded"; break;
      case sampleOpErrorRange: error = "Empty range"; break;
      case sampleOpErrorWholeSample: error = "Delete whole sample not allowed"; break;
      case sampleOpErrorMemory: error = "Out of memory"; break;
    }
    screenMessage(MESSAGE_TIME_ERROR, "%s", error);
    return;
  }

  projectModified = 1;
  sampleDirtyToDisk = 1;
  // Crop/Delete reshape the sample: drop the selection and zoom back out
  if (processOp == 0 || processOp == 2) {
    editorSelection.start = 0;
    editorSelection.end = 0;
    editorSelection.active = 0;
    zoomOutFull(sample, &editorView);
  }
  sampleEditorNormalizeState(sample, &editorSelection, &editorView);
  settingsRepaintAfterOp();
  screenMessage(MESSAGE_TIME, "%s", processOpDoneMessages[processOp]);
}

static void settingsRunUndo(void) {
  if (!editorUndo.active) return;
  InstrumentSample* sample = currentSample();
  audioManager.pause();
  int result = sampleOpApplyUndo(sample, &editorUndo);
  audioManager.resume();
  if (result != sampleOpOk) {
    screenMessage(MESSAGE_TIME_ERROR, "Undo failed");
    return;
  }
  projectModified = 1;
  zoomOutFull(sample, &editorView);
  sampleEditorNormalizeState(sample, &editorSelection, &editorView);
  settingsRepaintAfterOp();
  screenMessage(MESSAGE_TIME, "Undone");
}

// Return to the sample editor from a dialog. Re-entering the screen resets
// the session state, so the dirty flag travels through pendingDirtyRestore:
// keep it whenever the file on disk may still differ from the sample in RAM.
static void settingsReturnFromDialog(int keepDirty) {
  pendingDirtyRestore = keepDirty ? sampleDirtyToDisk : 0;
  screenSetup(&screenSampleSettings, cInstrument);
}

static void settingsCancelDialog(void) {
  settingsReturnFromDialog(1);
}

// Save: overwrite the WAV the sample was loaded from, after confirmation.
// SAVE TO PROJECT: plain WAV write, slice data stays in the .cct (the
// existing behavior).
static void settingsDoSaveToProject(void) {
  InstrumentSample* sample = currentSample();
  char error[128];
  audioManager.pause();
  int result = sampleSaveWav16(sample, sample->path, error, sizeof(error));
  audioManager.resume();
  if (result != 0) {
    screenMessage(MESSAGE_TIME_ERROR, "%s", error);
    settingsReturnFromDialog(1);
    return;
  }
  screenMessage(MESSAGE_TIME, "Saved %s", shortSampleFilename(sample->path, 24));
  settingsReturnFromDialog(0);
}

// SAVE TO SAMPLE: same WAV write, plus one cue point per slice start so
// the chops travel inside the file (visible as markers in DAWs). The cue
// frames come from sliceBounds when populated; legacy samples with an
// empty bounds array fall back to the even division of the loop region.
static void settingsDoSaveToSample(void) {
  InstrumentSample* sample = currentSample();
  uint32_t cueFrames[PROJECT_SAMPLE_MAX_SLICES];
  uint8_t cueCount = 0;
  const uint8_t count = sampleDecodeSliceCount(sample->slice);
  if (count > 0) {
    const uint32_t startFrame = sampleMarkerToStartFrame(sample->frameCount, sample->start);
    const uint32_t endFrame = sampleMarkerToEndFrame(sample->frameCount, sample->end);
    const int boundsPopulated = sample->sliceBounds[0] != 0 ||
      (count > 1 && sample->sliceBounds[1] != 0);
    for (uint8_t i = 0; i < count; ++i) {
      if (boundsPopulated) {
        cueFrames[cueCount++] = sample->sliceBounds[i];
      } else {
        uint32_t loopLength = endFrame > startFrame ? (endFrame - startFrame) : sample->frameCount;
        if (loopLength == 0) loopLength = sample->frameCount;
        cueFrames[cueCount++] = startFrame + (uint32_t)((uint64_t)loopLength * i / count);
      }
    }
  }
  char error[128];
  audioManager.pause();
  int result = sampleSaveWav16WithCues(sample, sample->path, cueFrames, cueCount,
                                       error, sizeof(error));
  audioManager.resume();
  if (result != 0) {
    screenMessage(MESSAGE_TIME_ERROR, "%s", error);
    settingsReturnFromDialog(1);
    return;
  }
  screenMessage(MESSAGE_TIME, "Saved %s", shortSampleFilename(sample->path, 24));
  settingsReturnFromDialog(0);
}

static void settingsRunSave(void) {
  InstrumentSample* sample = currentSample();
  if (!sample->path[0]) return;
  // Sliced samples ask where the chops should live (Phase 4): inside the
  // WAV as cue chunks, or in the project only. The per-project preference
  // ("don't ask again") skips the dialog; samples without slices keep the
  // plain overwrite confirmation.
  const uint8_t choice = chipnomadState->project.sampleSaveChoice;
  if (sampleDecodeSliceMode(sample->slice) != sliceModeOff) {
    if (choice == 0) {
      saveChoiceSetup(sample->path, settingsDoSaveToSample, settingsDoSaveToProject,
                      settingsCancelDialog);
      screenSetup(&screenSaveChoice, 0);
    } else if (choice == 1) {
      settingsDoSaveToSample();
    } else {
      settingsDoSaveToProject();
    }
    return;
  }
  char message[128];
  snprintf(message, sizeof(message), "Overwrite %s?", shortSampleFilename(sample->path, 48));
  confirmSetup(message, settingsDoSaveToProject, settingsCancelDialog);
  screenSetup(&screenConfirm, 0);
}

// Save As: pick a name, then a folder; the sample is written as
// <folder>/<name>.wav and the instrument points at the new file.
static void settingsSaveAsFolderSelected(const char* folderPath) {
  InstrumentSample* sample = currentSample();
  // folder + separator + name + ".wav" must fit the stored path length
  if (strlen(folderPath) + strlen(saveAsName) + 5 > PROJECT_SAMPLE_PATH_LENGTH) {
    screenMessage(MESSAGE_TIME_ERROR, "Path too long");
    settingsReturnFromDialog(1);
    return;
  }
  char newPath[PROJECT_SAMPLE_PATH_LENGTH + 2];
  snprintf(newPath, sizeof(newPath), "%s%s%s.wav", folderPath, PATH_SEPARATOR_STR, saveAsName);
  char error[128];
  audioManager.pause();
  int result = sampleSaveWav16(sample, newPath, error, sizeof(error));
  audioManager.resume();
  if (result != 0) {
    screenMessage(MESSAGE_TIME_ERROR, "%s", error);
    settingsReturnFromDialog(1);
    return;
  }
  strncpy(sample->path, newPath, PROJECT_SAMPLE_PATH_LENGTH);
  sample->path[PROJECT_SAMPLE_PATH_LENGTH] = 0;
  strncpy(appSettings.samplePath, folderPath, PATH_LENGTH);
  appSettings.samplePath[PATH_LENGTH] = 0;
  projectModified = 1;
  screenMessage(MESSAGE_TIME, "Saved %s", saveAsName);
  settingsReturnFromDialog(0);
}

static void settingsSaveAsNameEntered(const char* name) {
  strncpy(saveAsName, name, sizeof(saveAsName) - 1);
  saveAsName[sizeof(saveAsName) - 1] = 0;
  fileBrowserSetupFolderMode("SAVE SAMPLE", appSettings.samplePath, saveAsName, ".wav",
                             settingsSaveAsFolderSelected, settingsCancelDialog);
  screenSetup(&screenFileBrowser, 0);
}

static void settingsRunSaveAs(void) {
  InstrumentSample* sample = currentSample();
  if (!sample->data || !sample->frameCount) return;
  char initialName[25]; // the name entry field holds 24 characters
  sampleBasenameSansExt(sample->path, initialName, sizeof(initialName));
  enterNameSetup("SAVE SAMPLE", "File name:", initialName, settingsSaveAsNameEntered, settingsCancelDialog);
  screenSetup(&screenEnterName, 0);
}

// --- Slice row (Phase 1): Mode / Number / Frame --------------------------

// Repaints everything a slice edit can invalidate: the four Slice cells,
// the preview and (after mode switches) the Stretch row on the instrument
// screen is handled there - here only the editor's own fields matter.
static void settingsRepaintSlice(InstrumentSample* sample, int focusCol) {
  updateSamplePreview(sample, &editorView, &editorSelection);
  drawSamplePreview();
  for (int col = 0; col < 4; ++col) {
    settingsDrawField(col, 2, col == focusCol ? CellState::focus : CellState::normal);
  }
}

// Clamp the session-only current slice into the live count.
static void settingsClampCurrentSlice(const InstrumentSample* sample) {
  const uint8_t count = sampleDecodeSliceCount(sample->slice);
  if (count == 0) {
    currentSlice = 0;
  } else if (currentSlice >= count) {
    currentSlice = count - 1;
  }
}

// A-tap slice preview (EDIT tap on the Number or Frame cell): plays the
// currently selected slice as a one-shot phrase row, in every slice mode.
// The row's note IS the slice index (0-based) - sliced samples map notes
// chromatically to slices, so C-0 plays slice 0, C#0 slice 1, and so on.
// fxSLP=0 forces one-shot playback; the preview stops when the key is
// released (app.cpp auto-stop) or another preview replaces it.
static void settingsSlicePreviewCurrent(InstrumentSample* sample) {
  if (sample->stretchMode != 0) return;
  if (!sample->data || sample->frameCount == 0) return;
  if (sampleDecodeSliceMode(sample->slice) == sliceModeOff) return;
  if (sampleLazyPlaybackActive) return; // LAZY playback-drop owns EDIT
  PhraseRow row;
  memset(&row, 0, sizeof(row));
  row.note = (uint8_t)currentSlice;
  row.instrument = cInstrument;
  row.volume = PHRASE_VOLUME_MAX;
  row.fx[0][0] = fxSLP;
  row.fx[0][1] = 0; // one-shot
  row.fx[1][0] = EMPTY_VALUE_8;
  row.fx[1][1] = EMPTY_VALUE_8;
  row.fx[2][0] = EMPTY_VALUE_8;
  row.fx[2][1] = EMPTY_VALUE_8;
  chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
  chipnomadQueuePlaybackStartPhraseRow(chipnomadState, *pSongTrack, &row);
  slicePreviewActive = 1;
}

// LAZY playback-drop (Phase 3): while the full sample is playing back
// (tap PLAY toggles it), every EDIT click drops a slice at the playback
// marker's position. The marker is read straight from the track's sample
// voice - the same tolerated cross-thread pattern as the voice monitors.
static void settingsDropSliceAtPlayback(InstrumentSample* sample) {
  const PlaybackStatus* playback = chipnomadGetPlaybackStatus(chipnomadState);
  SampleVoice* voice = chipnomadState->sampleVoices[*pSongTrack][0];
  if (playback->tracks[*pSongTrack].mode != PlaybackMode::phraseRow || !voice || !voice->active() ||
      playback->tracks[*pSongTrack].note.instrument != cInstrument) {
    screenMessage(MESSAGE_TIME, "Not playing");
    return;
  }
  if (!sample->data || sample->frameCount == 0) return;
  int frame = (int)voice->playbackFrame();
  if (frame < 0) frame = 0;
  if (frame >= (int)sample->frameCount) frame = (int)sample->frameCount - 1;
  // Keep slices at least 50 ms apart so rapid EDIT taps don't pile up
  // micro-slices on top of each other.
  uint32_t minGap = sample->sampleRate / 20;
  if (minGap == 0) minGap = 1;
  const int index = sampleSliceInsertAtFrameGapped(sample, (uint32_t)frame, minGap);
  if (index < 0) {
    screenMessage(MESSAGE_TIME, "Too close to slice");
    return;
  }
  currentSlice = index;
  projectModified = 1;
  // The repaint covers all four Slice cells: the Count box shows the
  // incremented count immediately and the browser follows the dropped
  // slice.
  settingsRepaintSlice(sample, -1);
}

// EDIT+OPT (CellEditAction::clear) deletes the current slice - universal
// across the Slice browser and Frame cells. The first slice cannot be
// deleted (there is no previous slice to join into); deleting the last
// remaining slice turns the mode off.
static int settingsSliceDeleteCurrent(InstrumentSample* sample) {
  const uint8_t count = sampleDecodeSliceCount(sample->slice);
  if (count == 0) return 0;
  if (currentSlice == 0) {
    screenMessage(MESSAGE_TIME, "Cannot delete first slice");
    return 0;
  }
  const int next = sampleSliceDelete(sample, (uint8_t)currentSlice);
  if (next < 0) {
    // Deleting the last slice turned the mode off (or nothing changed).
    if (sampleDecodeSliceMode(sample->slice) != sliceModeOff) return 0;
    currentSlice = 0;
  } else {
    currentSlice = next;
  }
  settingsClampCurrentSlice(sample);
  projectModified = 1;
  settingsRepaintSlice(sample, -1);
  return 1;
}

// AUTO initializer (Phase 2): runs the spectral-flux detection and stores
// the onsets as bounds. Sensitivity is derived from the requested slice
// count - more slices need a more sensitive threshold to find that many
// onsets (1..99, 50 at the default count of 4). Detection is a destructive
// op (it overwrites sliceBounds), so it goes through the process-op undo
// pattern: pause audio, snapshot, detect, resume. The "DETECTING..." message
// is flushed to the screen before the synchronous pass so the user sees
// feedback during longer analyses.
static void settingsRunAutoDetect(InstrumentSample* sample, uint8_t count) {
  int sensitivity = 50 + (count - 4) * 4;
  if (sensitivity < 1) sensitivity = 1;
  if (sensitivity > 99) sensitivity = 99;
  sample->autoSensitivity = (uint8_t)sensitivity;
  screenMessage(MESSAGE_TIME, "DETECTING...");
  gfxUpdateScreen();
  audioManager.pause();
  const int error = sampleOpPrepareUndo(sample, &editorUndo);
  if (error == 0) {
    sampleSliceInitAuto(sample, count);
  }
  audioManager.resume();
  if (error != 0) {
    sampleOpFreeUndo(&editorUndo);
    screenMessage(MESSAGE_TIME_ERROR, "Detection failed");
    return;
  }
  projectModified = 1;
  sampleDirtyToDisk = 1;
  settingsClampCurrentSlice(sample);
  settingsRepaintSlice(sample, -1);
  screenMessage(MESSAGE_TIME, "Detected %d slices", sampleDecodeSliceCount(sample->slice));
}

// Mode cell (col 0): cycle Off / EQUAL / AUTO / LAZY. Switching to a mode
// initializes the bounds (EQUAL: even division; AUTO: spectral-flux
// detection; LAZY: single whole-loop slice). Leaving EQUAL/AUTO for LAZY
// stashes the chops; leaving LAZY to EQUAL/AUTO restores the stash
// verbatim, so no confirmation dialog is needed anywhere.
static int settingsSliceModeEdit(InstrumentSample* sample, CellEditAction action) {
  SliceMode mode = sampleDecodeSliceMode(sample->slice);
  uint8_t nextMode = (uint8_t)mode;
  if (action == CellEditAction::clear) {
    if (mode == sliceModeOff) return 0;
    nextMode = sliceModeOff;
  } else if (action == CellEditAction::increase || action == CellEditAction::increaseBig ||
             action == CellEditAction::tap || action == CellEditAction::doubleTap) {
    nextMode = mode == sliceModeLazy ? sliceModeOff : (SliceMode)(mode + 1);
  } else if (action == CellEditAction::decrease || action == CellEditAction::decreaseBig) {
    nextMode = mode == sliceModeOff ? sliceModeLazy : (SliceMode)(mode - 1);
  } else {
    return 0;
  }
  if (nextMode == (uint8_t)mode) return 0;

  if (nextMode == sliceModeLazy) {
    // Leaving EQUAL/AUTO for LAZY: stash the chops so switching back
    // restores them verbatim - the setting is never lost, so no
    // confirmation is needed.
    if (mode == sliceModeEqual || mode == sliceModeAuto) {
      sliceStashInstrument = cInstrument;
      sliceStashSlice = sample->slice;
      memcpy(sliceStashBounds, sample->sliceBounds, sizeof(sliceStashBounds));
    }
    sampleSliceInitLazy(sample);
  } else if (mode == sliceModeLazy && sliceStashInstrument == cInstrument &&
             (nextMode == sliceModeEqual || nextMode == sliceModeAuto)) {
    // Back from LAZY: restore the stashed setting instead of
    // re-initializing (no detection re-run, no even re-division). The
    // stashed mode wins over the cycled-to one - this is the "move back".
    sample->slice = sliceStashSlice;
    memcpy(sample->sliceBounds, sliceStashBounds, sizeof(sample->sliceBounds));
    sliceStashInstrument = -1;
  } else if (nextMode == sliceModeOff) {
    // Off keeps the bounds in memory so toggling back restores them.
    sample->slice = 0;
  } else if (nextMode == sliceModeAuto) {
    // AUTO: detect transients with the current sensitivity. The count
    // defaults to the previous count or 4.
    uint8_t count = sampleDecodeSliceCount(sample->slice);
    if (count == 0) count = 4;
    settingsRunAutoDetect(sample, count);
    // settingsRunAutoDetect already repainted; skip the shared repaint.
    if (nextMode != sliceModeOff) sample->stretchMode = 0;
    return 1;
  } else {
    // EQUAL: even division. The count defaults to the previous count or 4.
    uint8_t count = sampleDecodeSliceCount(sample->slice);
    if (count == 0) count = 4;
    sampleSliceInitEven(sample, (SliceMode)nextMode, count);
  }
  // Stretch and Slice are mutually exclusive: enabling one disables the other.
  if (nextMode != sliceModeOff) sample->stretchMode = 0;
  settingsClampCurrentSlice(sample);
  projectModified = 1;
  settingsRepaintSlice(sample, 0);
  return 1;
}

// Count box (col 1): EDIT+Left/Right steps the slice count by one
// (clamped 1..64), EDIT+Up/Down cycles through the power-of-two counts
// 2,4,8,16,32,64 with wrap-around. Every change recalculates the
// division: EQUAL re-divides the Start/End window evenly, AUTO re-runs
// detection with the new count (sensitivity derived from it). In LAZY the
// cell is display-only (the cursor skips it); the handler stays inert in
// LAZY and OFF as a fallback.
static int settingsSliceCountEdit(InstrumentSample* sample, CellEditAction action) {
  const SliceMode mode = sampleDecodeSliceMode(sample->slice);
  if (mode == sliceModeOff || mode == sliceModeLazy) return 0;
  if (action == CellEditAction::increase || action == CellEditAction::decrease ||
      action == CellEditAction::increaseBig || action == CellEditAction::decreaseBig) {
    const uint8_t count = sampleDecodeSliceCount(sample->slice);
    static const uint8_t kSliceSteps[] = {2, 4, 8, 16, 32, 64};
    int next = -1;
    if (action == CellEditAction::increase) {
      next = count < PROJECT_SAMPLE_MAX_SLICES ? count + 1 : PROJECT_SAMPLE_MAX_SLICES;
    } else if (action == CellEditAction::decrease) {
      next = count > 1 ? count - 1 : 1;
    } else if (action == CellEditAction::increaseBig) {
      // Cycle up through 2,4,8,16,32,64 with wrap-around; count 0/1
      // enters at 2.
      next = 2;
      for (size_t i = 0; i < sizeof(kSliceSteps) / sizeof(kSliceSteps[0]); ++i) {
        if ((int)kSliceSteps[i] > count) {
          next = kSliceSteps[i];
          break;
        }
      }
    } else {
      // Cycle down through 64,32,16,8,4,2 with wrap-around; count 0/1
      // wraps to 64.
      next = 64;
      for (int i = (int)(sizeof(kSliceSteps) / sizeof(kSliceSteps[0])) - 1; i >= 0; --i) {
        if ((int)kSliceSteps[i] < count) {
          next = kSliceSteps[i];
          break;
        }
      }
    }
    if (next == count) return 0;
    if (mode == sliceModeAuto) {
      settingsRunAutoDetect(sample, (uint8_t)next);
    } else {
      sampleSliceInitEven(sample, mode, (uint8_t)next);
      settingsClampCurrentSlice(sample);
      projectModified = 1;
      settingsRepaintSlice(sample, 1);
    }
    return 1;
  }
  return 0;
}

// Slice browser box (col 2): EDIT+Left/Right navigates the current slice
// by one, EDIT+Up/Down jumps by four (clamped, no wrap) - the row's
// small-step/big-step convention. The view recenters on the browsed
// slice's start when zoomed.
static int settingsSliceBrowseEdit(InstrumentSample* sample, CellEditAction action) {
  const SliceMode mode = sampleDecodeSliceMode(sample->slice);
  const uint8_t count = sampleDecodeSliceCount(sample->slice);
  if (mode == sliceModeOff) return 0;
  if (action == CellEditAction::increase || action == CellEditAction::decrease ||
      action == CellEditAction::increaseBig || action == CellEditAction::decreaseBig) {
    if (count == 0) return 0;
    const int up = action == CellEditAction::increase || action == CellEditAction::increaseBig;
    const int step = action == CellEditAction::increaseBig || action == CellEditAction::decreaseBig ? 4 : 1;
    int next = currentSlice + (up ? step : -step);
    if (next < 0) next = 0;
    if (next >= count) next = count - 1;
    if (next == currentSlice) return 0;
    currentSlice = next;
    // Recenter the zoomed view on the new slice's start (the full view
    // shows every marker anyway).
    if (editorView.viewEnd - editorView.viewStart < sample->frameCount) {
      const int32_t frame = sampleSliceStartFrame(sample, (uint8_t)currentSlice,
                                                  sample->start, sample->end);
      if (frame >= 0) zoomToMarker(sample, &editorView, (uint32_t)frame);
    }
    settingsRepaintSlice(sample, 2);
    return 1;
  }
  return 0;
}

// Frame cell (col 2): EDIT+Left/Right nudges the current slice's start by
// +-10 frames (zoomed fine steps of +-1 while EDIT is held); EDIT+Up/Down
// moves by +-100.
static int settingsSliceFrameEdit(InstrumentSample* sample, CellEditAction action) {
  const SliceMode mode = sampleDecodeSliceMode(sample->slice);
  if (mode == sliceModeOff) return 0;
  if (action == CellEditAction::increase || action == CellEditAction::decrease ||
      action == CellEditAction::increaseBig || action == CellEditAction::decreaseBig) {
    const int fine = action == CellEditAction::increase || action == CellEditAction::decrease;
    const int up = action == CellEditAction::increase || action == CellEditAction::increaseBig;
    const int32_t delta = (up ? 1 : -1) * (fine ? 10 : 100);
    const int32_t frame = sampleSliceNudge(sample, (uint8_t)currentSlice, delta);
    if (frame < 0) return 0;
    // Fine steps zoom onto the nudged marker so the waveform shows exactly
    // what is being adjusted; coarse steps return to the full view (the
    // zoom holds while EDIT stays down, like the Region/Select rows).
    if (fine) {
      editorView.anchor = kViewAnchorNone;
      zoomToMarker(sample, &editorView, (uint32_t)frame);
      zoomHoldActive = 1;
    } else {
      zoomOutFull(sample, &editorView);
      zoomHoldActive = 0;
    }
    projectModified = 1;
    settingsRepaintSlice(sample, 3);
    return 1;
  }
  return 0;
}

// Dispatch for the four Slice row cells. EDIT+OPT (clear) deletes the
// current slice from the Slice browser and Frame cells (the first slice
// cannot be deleted - there is no previous slice to join into). EDIT
// tap/double-tap on those cells previews the current slice (works in
// every slice mode).
static int settingsOnEditSlice(int col, CellEditAction action, InstrumentSample* sample) {
  // Slice is inert while Stretch drives the duration.
  if (sample->stretchMode != 0) return 0;
  if (action == CellEditAction::clear && col >= 2) {
    // EDIT+OPT deletes the current slice from the browser or Frame cell
    return settingsSliceDeleteCurrent(sample);
  }
  if ((action == CellEditAction::tap || action == CellEditAction::doubleTap) && col >= 2) {
    settingsSlicePreviewCurrent(sample);
    settingsDrawField(2, 2, CellState::focus);
    return 1;
  }
  if (col == 0) return settingsSliceModeEdit(sample, action);
  if (col == 1) return settingsSliceCountEdit(sample, action);
  if (col == 2) return settingsSliceBrowseEdit(sample, action);
  return settingsSliceFrameEdit(sample, action);
}

static int settingsOnEdit(int col, int row, CellEditAction action) {
  InstrumentSample* sample = currentSample();
  int handled = 0;
  int marker = 0; // 1 = Start, 2 = End
  if (row == 3) {
    if (col) return 0;
    uint8_t algorithm = sample->speedAlgorithm <= 1 ? sample->speedAlgorithm : 0;
    handled = edit8noLast(action, &algorithm, 1, 0, 1);
    if (handled) {
      sample->speedAlgorithm = algorithm;
      projectModified = 1;
      settingsDrawField(0, 3, CellState::focus);
    }
    return handled;
  }
  if (row == 4) {
    if (col == 0) {
      // Cycle the operation; Edit+Opt clears it to none
      if (action == CellEditAction::clear) {
        if (processOp < 0) return 0;
        processOp = -1;
      } else if (action == CellEditAction::increase || action == CellEditAction::increaseBig) {
        processOp = processOp < 0 ? 0 : (processOp + 1) % processOpCount;
      } else if (action == CellEditAction::decrease || action == CellEditAction::decreaseBig) {
        processOp = processOp < 0 ? processOpCount - 1 : (processOp + processOpCount - 1) % processOpCount;
      } else if (action == CellEditAction::tap || action == CellEditAction::doubleTap) {
        processOp = processOp < 0 ? 0 : (processOp + 1) % processOpCount;
      } else {
        return 0;
      }
      settingsDrawField(0, 4, CellState::focus);
      return 1;
    }
    if (action != CellEditAction::tap && action != CellEditAction::doubleTap) return 0;
    if (col == 1) settingsRunProcessOp();
    else settingsRunUndo();
    return 1;
  }
  if (row == 5) {
    if (col == 0) {
      // Cycle the action (any edit key toggles); Edit+Opt resets it to Save
      if (action == CellEditAction::clear) {
        if (fileAction == 0) return 0;
        fileAction = 0;
      } else if (action == CellEditAction::tap || action == CellEditAction::doubleTap ||
                 action == CellEditAction::increase || action == CellEditAction::decrease ||
                 action == CellEditAction::increaseBig || action == CellEditAction::decreaseBig) {
        fileAction = fileAction == 0 ? 1 : 0;
      } else {
        return 0;
      }
      settingsDrawField(0, 5, CellState::focus);
      return 1;
    }
    if (action != CellEditAction::tap && action != CellEditAction::doubleTap) return 0;
    if (fileAction == 0) settingsRunSave();
    else settingsRunSaveAs();
    return 1;
  }
  if (row == 0) {
    // Region row: the playback Start/End markers, stored on the sample as
    // normalised 00-FF values. Fine steps move one unit, coarse steps 16;
    // edit8noLast already steps by one, so only the coarse clamping is
    // spelled out here (same shape as the Select row below).
    action = convertMultiAction(action);
    uint8_t* value = col == 0 ? &sample->start : &sample->end;
    if (action == CellEditAction::tap) {
      handled = 1;
    } else if (action == CellEditAction::clear) {
      if (*value != 0) handled = 1;
      *value = 0;
    } else if (action == CellEditAction::increase || action == CellEditAction::decrease ||
               action == CellEditAction::increaseBig || action == CellEditAction::decreaseBig) {
      const int fine = action == CellEditAction::increase || action == CellEditAction::decrease;
      const int up = action == CellEditAction::increase || action == CellEditAction::increaseBig;
      const uint8_t step = fine ? 1 : 16;
      uint8_t next;
      if (up) next = *value > 255 - step ? 255 : (uint8_t)(*value + step);
      else next = *value < step ? 0 : (uint8_t)(*value - step);
      if (next != *value) {
        *value = next;
        handled = 1;
      }
    }
    marker = col == 0 ? kViewAnchorStart : kViewAnchorEnd;
  } else if (row == 1) {
    // Select row: processing-selection handles. Fine steps move twenty
    // frames and zoom onto the handle; coarse steps jump frameCount/64 (min 16)
    // and return to the full-sample view. Tap copies the matching Region
    // marker position; clear empties the whole selection. Start/End are
    // untouched.
    const uint32_t frameCount = sample->frameCount;
    if (frameCount == 0) return 0;
    uint32_t* handle = col == 0 ? &editorSelection.start : &editorSelection.end;
    if (action == CellEditAction::tap) {
      uint32_t markerPos = col == 0 ? sampleMarkerToStartFrame(frameCount, sample->start)
                                    : sampleMarkerToEndFrame(frameCount, sample->end);
      if (editorSelection.active && *handle == markerPos) return 0;
      *handle = markerPos;
      handled = 1;
    } else if (action == CellEditAction::clear) {
      if (!editorSelection.active && editorSelection.start == 0 && editorSelection.end == 0) return 0;
      editorSelection.start = 0;
      editorSelection.end = 0;
      handled = 1;
    } else {
      // Fine steps move twenty frames; coarse steps jump frameCount/64 (min 16)
      uint32_t step = 20;
      if (action == CellEditAction::increaseBig || action == CellEditAction::decreaseBig) {
        step = frameCount / 64;
        if (step < 16) step = 16;
      }
      if (action == CellEditAction::increase || action == CellEditAction::increaseBig) {
        uint64_t next = (uint64_t)*handle + step;
        if (next > frameCount) next = frameCount;
        if (next == *handle) return 0;
        *handle = (uint32_t)next;
      } else if (action == CellEditAction::decrease || action == CellEditAction::decreaseBig) {
        uint32_t next = *handle > step ? *handle - step : 0;
        if (next == *handle) return 0;
        *handle = next;
      } else {
        return 0;
      }
      handled = 1;
    }
    if (handled) {
      if (action == CellEditAction::increase || action == CellEditAction::decrease) {
        editorView.anchor = col == 0 ? kViewAnchorSelStart : kViewAnchorSelEnd;
        zoomToMarker(sample, &editorView, *handle);
        zoomHoldActive = 1;
      } else if (action == CellEditAction::increaseBig || action == CellEditAction::decreaseBig ||
                 action == CellEditAction::clear) {
        zoomOutFull(sample, &editorView);
        zoomHoldActive = 0;
      }
      sampleEditorNormalizeState(sample, &editorSelection, &editorView);
      drawSamplePreview();
      // Repaint both handle readouts: normalization may have swapped them
      settingsDrawField(0, 1, CellState::focus);
      settingsDrawField(1, 1, CellState::focus);
    }
    return handled;
  } else if (row == 2) {
    return settingsOnEditSlice(col, action, sample);
  }
  if (handled) {
    projectModified = 1;
    // Fine steps zoom onto the edited marker so the waveform shows exactly
    // what is being adjusted; coarse steps (and clear) return to the
    // full-sample view. The zoom holds while EDIT stays down
    // (zoomHoldActive); EDIT release drops back to the full view.
    if (action == CellEditAction::increase || action == CellEditAction::decrease) {
      editorView.anchor = marker;
      zoomToMarker(sample, &editorView, anchorFrame(sample, &editorSelection, marker));
      zoomHoldActive = 1;
    } else if (action == CellEditAction::increaseBig || action == CellEditAction::decreaseBig ||
               action == CellEditAction::clear) {
      zoomOutFull(sample, &editorView);
      zoomHoldActive = 0;
    }
    updateSamplePreview(sample, &editorView, &editorSelection);
    drawSamplePreview();
  }
  return handled;
}

// Slice is inert while Stretch drives the duration: skip all four of its
// cells in navigation. In LAZY the Count box is display-only: touch taps
// on it are rejected (keyboard navigation skips the cell before the
// framework sees it). Save needs a file path, Save As needs sample data.
static int settingsIsCellValid(int col, int row) {
  InstrumentSample* sample = currentSample();
  if (row == 2 && sample->stretchMode != 0) return 0;
  if (row == 2 && col == 1 &&
      sampleDecodeSliceMode(sample->slice) == sliceModeLazy) return 0;
  if (row == 5 && col == 0) {
    return fileAction == 0 ? sample->path[0] != 0 : (sample->data != NULL && sample->frameCount > 0);
  }
  return 1;
}

static ScreenData screenSampleSettingsData = {
  .rows = 6,
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
  // Entering the screen always starts at the full-sample view. The
  // selection is seeded with the playback Region span - the whole sample
  // with the default markers - so the process tools act on the region out
  // of the box. Both are session-only editor state, re-derived on every
  // entry; the undo slot is dropped too - undo never survives leaving the
  // screen, and neither does the LAZY stash. The dirty flag is restored
  // from pendingDirtyRestore when a dialog round trip re-enters.
  InstrumentSample* sample = currentSample();
  sampleOpFreeUndo(&editorUndo);
  sliceStashInstrument = -1;
  zoomOutFull(sample, &editorView);
  editorSelection.start = sampleMarkerToStartFrame(sample->frameCount, sample->start);
  editorSelection.end = sampleMarkerToEndFrame(sample->frameCount, sample->end);
  editorSelection.active = 1;
  sampleEditorNormalizeState(sample, &editorSelection, &editorView);
  zoomHoldActive = 0;
  // LAZY playback never survives leaving the screen (or a dialog round
  // trip): the preview is stopped on the way out and the flag resets here.
  if (sampleLazyPlaybackActive) {
    chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
    sampleLazyPlaybackActive = 0;
  }
  lazyEditArmed = 1;
  slicePreviewActive = 0;
  // Hint state never survives leaving the screen either.
  hintCursorRow = -1;
  hintCursorCol = -1;
  hintOwnedText[0] = '\0';
  sampleDirtyToDisk = pendingDirtyRestore;
  pendingDirtyRestore = 0;
  // Legacy AUTO sentinel with empty bounds (old project saved before
  // bounds existed): populate them here on the UI thread, never on the
  // audio thread (detection allocates). The sentinel count is the target.
  if (sampleDecodeSliceMode(sample->slice) == sliceModeAuto &&
      sample->sliceBounds[0] == 0 &&
      !(sampleDecodeSliceCount(sample->slice) > 1 && sample->sliceBounds[1] != 0)) {
    settingsRunAutoDetect(sample, sampleDecodeSliceCount(sample->slice));
  }
}

static void fullRedraw(void) {
  // The cursor persists across screens: if it is parked on Slice while Stretch
  // is active, move it up so it never rests on a disabled cell.
  if (screenSampleSettingsData.cursorRow == 2 && currentSample()->stretchMode != 0) {
    screenSampleSettingsData.cursorRow = 1;
  }
  // Same for the File row: the previous instrument may have left the cursor
  // on an action the current sample cannot use.
  if (screenSampleSettingsData.cursorRow == 4 &&
      !settingsIsCellValid(screenSampleSettingsData.cursorCol, 4)) {
    screenSampleSettingsData.cursorRow = 1;
  }
  // Clamp the Slice row cursor into the 4 cells (older sessions may have
  // parked it beyond).
  if (screenSampleSettingsData.cursorRow == 2 &&
      screenSampleSettingsData.cursorCol > 3) {
    screenSampleSettingsData.cursorCol = 3;
  }
  // In LAZY the Count box is display-only: a session parked on it lands
  // on the Slice browser instead.
  if (screenSampleSettingsData.cursorRow == 2 &&
      screenSampleSettingsData.cursorCol == 1 &&
      sampleDecodeSliceMode(currentSample()->slice) == sliceModeLazy) {
    screenSampleSettingsData.cursorCol = 2;
  }
  settingsClampCurrentSlice(currentSample());
  screenFullRedraw(&screenSampleSettingsData);
}

static void draw(void) {
  InstrumentSample* sample = currentSample();
  const int wasActive = sampleLazyPlaybackActive;
  const PlaybackStatus* playback = chipnomadGetPlaybackStatus(chipnomadState);
  SampleVoice* voice = chipnomadState->sampleVoices[*pSongTrack][0];
  // Slice row combo hints: the bar is ours when it is empty or shows the
  // hint we set; a foreign message (action feedback, error) pauses the
  // hints until it expires.
  const char* activeMessage = screenGetActiveMessage();
  if (activeMessage[0] == '\0') {
    settingsUpdateHint(screenSampleSettingsData.cursorRow,
                       screenSampleSettingsData.cursorCol, 0);
  } else if (strcmp(activeMessage, hintOwnedText) == 0) {
    settingsUpdateHint(screenSampleSettingsData.cursorRow,
                       screenSampleSettingsData.cursorCol, 1);
  } else {
    hintOwnedText[0] = '\0';
  }
  if (sampleLazyPlaybackActive) {
    // The preview can end on its own (one-shot sample finished, or the
    // track was stopped from elsewhere): drop the flag when the phrase row
    // is gone or the voice went silent. The flag is only trusted after the
    // playback has been OBSERVED active at least once - right after the
    // PLAY press the audio thread has not processed the queued command yet,
    // so the status still shows no phrase row and the voice is inactive.
    // Clearing on that first frame killed the marker and disarmed the
    // app-level key-up guard (which then stopped the preview on PLAY
    // release - the "playback only while PLAY is held" bug).
    if (lazyPlaybackSeenActive) {
      if (playback->tracks[*pSongTrack].mode != PlaybackMode::phraseRow || !voice || !voice->active()) {
        sampleLazyPlaybackActive = 0;
      }
    } else if (playback->tracks[*pSongTrack].mode == PlaybackMode::phraseRow &&
               voice && voice->active()) {
      lazyPlaybackSeenActive = 1;
    }
  } else {
    lazyPlaybackSeenActive = 0;
  }
  if (sampleLazyPlaybackActive) {
    // Cheap per-frame repaint: the waveform/slice bitmaps are cached, so
    // only the marker column is drawn on top of the cached preview.
    drawSamplePreview();
    Bitmap* marker = ensurePreviewBitmap(&samplePlaybackMarkerBitmap);
    if (marker) {
      gfxBitmapClear(marker);
      double pos = voice ? voice->playbackFrame() : -1.0;
      if (pos < 0) pos = 0;
      if (pos >= (double)sample->frameCount) pos = (double)sample->frameCount - 1;
      const int x = frameToPixel((uint32_t)pos, &editorView, marker->widthPixels, 0);
      if (x >= 0) {
        for (int y = 0; y < marker->heightPixels; ++y) {
          marker->data[y * marker->widthPixels + x] = 255;
          if (x + 1 < marker->widthPixels) marker->data[y * marker->widthPixels + x + 1] = 255;
        }
        gfxSetFgColor(0x00FF00); // Green
        gfxDrawBitmap(marker, 0, previewRow);
      }
    }
  }
  if (wasActive != sampleLazyPlaybackActive) {
    // The Frame cell dims while the playback-drop is armed; repaint the
    // Slice row so the dim appears/disappears (drawField only fires on
    // edits and full redraws otherwise).
    for (int col = 0; col < 4; ++col) {
      const int focused = screenSampleSettingsData.cursorRow == 2 &&
        screenSampleSettingsData.cursorCol == col;
      settingsDrawField(col, 2, focused ? CellState::focus : CellState::normal);
    }
  }
}

static int inputScreenNavigation(int isKeyDown, int keys) {
  // Leaving the screen stops the LAZY preview and clears the flag: the
  // next screen's setup() never touches it, so without this the app-level
  // auto-stop guard would stay armed forever (and the preview would keep
  // sounding over the new screen). Key-down only: key-up events carry the
  // still-held buttons, so releasing a direction while B is held must not
  // fire the navigation.
  if (isKeyDown && sampleLazyPlaybackActive &&
      (keys == keyOpt || keys == (keyLeft | keyShift) || keys == (keyRight | keyShift) ||
       keys == (keyDown | keyShift) || keys == (keyUp | keyShift))) {
    chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
    sampleLazyPlaybackActive = 0;
  }
  // Leaving the screen also stops a running A-tap slice preview.
  if (isKeyDown && slicePreviewActive &&
      (keys == keyOpt || keys == (keyLeft | keyShift) || keys == (keyRight | keyShift) ||
       keys == (keyDown | keyShift) || keys == (keyUp | keyShift))) {
    chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
    slicePreviewActive = 0;
  }
  if (isKeyDown && (keys == keyOpt || keys == (keyLeft | keyShift))) {
    screenSetup(&screenInstrument, cInstrument);
    return 1;
  }
  if (isKeyDown && keys == (keyRight | keyShift)) {
    screenSetup(&screenTable, cInstrument);
    return 1;
  }
  if (isKeyDown && keys == (keyDown | keyShift)) {
    screenSetup(&screenInstrumentPool, cInstrument);
    return 1;
  }
  if (isKeyDown && keys == (keyUp | keyShift)) {
    screenSetup(&screenModulation, cInstrument);
    return 1;
  }
  return 0;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  // EDIT release ends a zoom hold: fine adjustments zoom in while EDIT is
  // down, and letting go of it returns to the full-sample view. Key-up
  // events carry the still-held buttons, so EDIT counts as released only
  // when its bit is gone (releasing another key while EDIT is held keeps
  // the zoom).
  if (zoomHoldActive && !isKeyDown && !(keys & keyEdit)) {
    zoomHoldActive = 0;
    InstrumentSample* sample = currentSample();
    if (editorView.viewStart != 0 || editorView.viewEnd != sample->frameCount) {
      zoomOutFull(sample, &editorView);
      updateSamplePreview(sample, &editorView, &editorSelection);
      drawSamplePreview();
    }
  }
  // Re-arm the LAZY playback-drop on the EDIT release (see lazyEditArmed).
  if (!isKeyDown && !(keys & keyEdit)) lazyEditArmed = 1;
  // The A-tap slice preview ends with the key: app.cpp stops the phrase row
  // on the keys==0 release (hold-preview semantics); the flag follows.
  if (!isKeyDown && keys == 0) slicePreviewActive = 0;
  if (inputScreenNavigation(isKeyDown, keys)) return 1;
  // LAZY playback (Phase 3): tap PLAY toggles a full-sample playback that
  // keeps running after the key is released, and while it runs every EDIT
  // click drops a slice at the playback marker. SHIFT+PLAY is left alone:
  // it falls through to the app-level handler and starts phrase playback
  // like on every other screen. Both intercepts only apply when the sample
  // is in LAZY mode; everything else falls through to the normal screen
  // input.
  InstrumentSample* sample = currentSample();
  if (sampleDecodeSliceMode(sample->slice) == sliceModeLazy && sample->data && sample->frameCount > 0) {
    if (keys == keyPlay && isKeyDown) {
      if (sampleLazyPlaybackActive) {
        // Second PLAY press stops the one-shot playback.
        chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
        sampleLazyPlaybackActive = 0;
      } else if (!chipnomadGetPlaybackStatus(chipnomadState)->isPlaying) {
        // One-shot full-sample playback at the sample's original pitch: a
        // custom phrase row with fxSLP=0 forces loopMode 0 (one-shot) on
        // the sample voice regardless of the sample's own loop setting, so
        // the playback runs once from the start and stops by itself. The
        // Full command sets the track's sliceBypass flag so the voice
        // ignores slice mapping (LAZY slices map chromatically now) and
        // the row's note pitch; the root note keeps the row valid on its
        // own (the first sequencer note used to leak in here and stretch
        // the preview to match it).
        PhraseRow row;
        memset(&row, 0, sizeof(row));
        int rootNote = chipnomadState->project.pitchTable.octaveSize * 4;
        if (rootNote >= chipnomadState->project.pitchTable.length) rootNote = 0;
        row.note = (uint8_t)rootNote;
        row.instrument = cInstrument;
        row.volume = PHRASE_VOLUME_MAX;
        row.fx[0][0] = fxSLP;
        row.fx[0][1] = 0; // one-shot
        row.fx[1][0] = EMPTY_VALUE_8;
        row.fx[1][1] = EMPTY_VALUE_8;
        row.fx[2][0] = EMPTY_VALUE_8;
        row.fx[2][1] = EMPTY_VALUE_8;
        chipnomadQueuePlaybackStopPreview(chipnomadState, *pSongTrack);
        chipnomadQueuePlaybackStartPhraseRowFull(chipnomadState, *pSongTrack, &row);
        sampleLazyPlaybackActive = 1;
      }
      // While the song is playing, PLAY falls through and stops it.
      else return 0;
      return 1;
    }
    if (sampleLazyPlaybackActive && isKeyDown && keys == keyEdit && lazyEditArmed &&
        (tapCount == 1 || tapCount == 2)) {
      settingsDropSliceAtPlayback(sample);
      lazyEditArmed = 0;
      return 1;
    }
  }
  // LAZY: the Count box is display-only (hand-placed slices have no
  // division to recalculate), so the cursor skips it: horizontal moves
  // step over col 1 and a vertical entry from the Select row lands on
  // the Slice browser. The intercept runs before the framework
  // navigation - its dead-cell recovery moves up a row instead of aside -
  // and repaints the affected cells itself (row/col headers are no-ops).
  if (isKeyDown && sampleDecodeSliceMode(sample->slice) == sliceModeLazy) {
    const int fromRow = screenSampleSettingsData.cursorRow;
    const int fromCol = screenSampleSettingsData.cursorCol;
    int toRow = fromRow;
    int toCol = -1;
    if (fromRow == 2 && fromCol == 0 && keys == keyRight) {
      toCol = 2;
    } else if (fromRow == 2 && fromCol == 2 && keys == keyLeft) {
      toCol = 0;
    } else if (fromRow == 1 && fromCol == 1 && keys == keyDown) {
      toRow = 2;
      toCol = 2;
    }
    if (toCol >= 0) {
      screenSampleSettingsData.cursorRow = toRow;
      screenSampleSettingsData.cursorCol = toCol;
      settingsDrawField(fromCol, fromRow, CellState::normal);
      settingsDrawField(toCol, toRow, CellState::focus);
      settingsDrawCursor(toCol, toRow);
      return 1;
    }
  }
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
