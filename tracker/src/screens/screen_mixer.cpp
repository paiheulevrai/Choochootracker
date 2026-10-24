#include "screens.h"
#include "audio_manager.h"
#include "audio_monitor.h"
#include "common.h"
#include "corelib_gfx.h"
#include "meter_display.h"
#include "monitor_display.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdio.h>
#include <string.h>

static void fullRedraw(void);
static void refreshEffectRows(void);
static int displayedCpuLoad = -1;
static int mixerPage = 0; // 0 mixer, 1 reverb, 2 delay
static Bitmap* stereoImage;
static Bitmap* spectrum;
static std::chrono::steady_clock::time_point spectrumRefresh;

static Bitmap* analyzerBitmap(Bitmap*& bitmap, int columns, int rows, bool clear = true) {
  if (bitmap && (bitmap->widthPixels != columns * gfxGetCharWidth() || bitmap->heightPixels != rows * gfxGetCharHeight())) {
    gfxBitmapFree(bitmap);
    bitmap = NULL;
  }
  if (!bitmap) bitmap = gfxBitmapCreate(columns, rows);
  if (bitmap && clear) gfxBitmapClear(bitmap);
  return bitmap;
}

static void analyzerPixel(Bitmap* bitmap, int x, int y, uint8_t value) {
  if (bitmap && x >= 0 && x < bitmap->widthPixels && y >= 0 && y < bitmap->heightPixels)
    bitmap->data[y * bitmap->widthPixels + x] = value;
}

static void drawAnalyzers(void) {
  constexpr int imageColumns = 17, spectrumColumns = 17, rows = 5;
  Bitmap* image = analyzerBitmap(stereoImage, imageColumns, rows);
  Bitmap* fft = analyzerBitmap(spectrum, spectrumColumns, rows, false);
  const float (*samples)[2] = monitorDisplayMixStereoSamples();
  if (image) {
    const int w = image->widthPixels, h = image->heightPixels;
    for (int x = 0; x < w; ++x) analyzerPixel(image, x, h / 2, 48);
    for (int y = 0; y < h; ++y) analyzerPixel(image, w / 2, y, 48);
    if (samples) for (int i = 0; i < AUDIO_MONITOR_SAMPLES; ++i) {
      float left = std::isfinite(samples[i][0]) ? samples[i][0] : 0.0f;
      float right = std::isfinite(samples[i][1]) ? samples[i][1] : 0.0f;
      int x = w / 2 + (int)((left - right) * (w / 2 - 2) * 0.8f);
      int y = h / 2 - (int)((left + right) * (h / 2 - 2) * 0.5f);
      analyzerPixel(image, x, y, 255);
    }
  }
  static float sine[128], cosine[128], window[128];
  static bool spectrumLutReady = false;
  if (!spectrumLutReady) {
    constexpr float pi = 3.14159265358979323846f;
    for (int i = 0; i < 128; ++i) {
      cosine[i] = cosf(2.0f * pi * i / 128.0f);
      sine[i] = sinf(2.0f * pi * i / 128.0f);
      window[i] = 0.5f - 0.5f * cosf(2.0f * pi * i / 127.0f);
    }
    spectrumLutReady = true;
  }
  // A DFT still does thousands of arithmetic operations per draw even though
  // its trigonometric terms come from LUTs. Keep this noncritical diagnostic
  // display below the PortMaster audio deadline.
  const auto now = std::chrono::steady_clock::now();
  if (fft && samples &&
      (spectrumRefresh == std::chrono::steady_clock::time_point() ||
       std::chrono::duration<float>(now - spectrumRefresh).count() >= 1.0f / 20.0f)) {
    gfxBitmapClear(fft);
    const int w = fft->widthPixels, h = fft->heightPixels, bands = 17, bandWidth = std::max(1, w / bands);
    for (int band = 0; band < bands; ++band) {
      const int bin = 1 + band * 3;
      float real = 0, imaginary = 0;
      for (int i = 0; i < 128; ++i) {
        float sample = (samples[i][0] + samples[i][1]) * 0.5f;
        const int phase = (bin * i) & 127;
        real += sample * window[i] * cosine[phase];
        imaginary -= sample * window[i] * sine[phase];
      }
      float amplitude = sqrtf(real * real + imaginary * imaginary) * 4.0f / 128.0f;
      float db = 20.0f * log10f(std::max(amplitude, 0.001f));
      int level = std::clamp((int)lroundf((db + 60.0f) * h / 60.0f), 0, h);
      for (int x = band * bandWidth; x < std::min(w, (band + 1) * bandWidth - 1); ++x)
        for (int y = 0; y < level; ++y) analyzerPixel(fft, x, h - 1 - y, 255);
    }
    spectrumRefresh = now;
  }
  gfxSetFgColor(appSettings.colorScheme.textInfo);
  gfxClearRect(0, 14, 35, 6);
  gfxPrint(0, 14, "STEREO IMAGE");
  gfxPrint(18, 14, "FFT SPECTRUM");
  if (image) gfxDrawBitmap(image, 0, 15);
  if (fft) gfxDrawBitmap(fft, 18, 15);
}

int screenMixerGetPage(void) { return mixerPage; }

static bool beadsReverb() { return chipnomadState->project.reverbAlgorithm == 1; }
static bool tapeEcho() { return chipnomadState->project.delayAlgorithm == 1; }
static int effectRows() { return mixerPage == 0 ? 0 : mixerPage == 1 ? (beadsReverb() ? 5 : 6) : (tapeEcho() ? 9 : 6); }
static int columnCount(int row) { return mixerPage == 0 ? (row == PROJECT_MAX_TRACKS ? 1 : 7) : 1; }
static void drawRowHeader(int row, CellState state) {}
static void drawColHeader(int col, CellState state) {}

static const char* tapeDivisionName(uint8_t division) {
  static const char* names[] = {"1/16", "1/8T", "1/8", "1/8.", "1/4", "1/4.", "1/2"};
  return names[division <= 6 ? division : 2];
}

static const char* tapeHeadsName(uint8_t heads) {
  static const char* names[] = {"H1", "H2", "H3", "H1+H2", "H1+H3", "H2+H3", "H1+H2+H3"};
  return names[heads <= 6 ? heads : 6];
}

static void drawStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  if (mixerPage == 0) {
    gfxPrint(0, 0, "MIXER");
    gfxPrint(0, 2, "TRK"); gfxPrint(4, 2, "LVL"); gfxPrint(9, 2, "REV"); gfxPrint(14, 2, "DLY");
    gfxPrint(19, 2, "TLT"); gfxPrint(24, 2, "PAN"); gfxPrint(29, 2, "M"); gfxPrint(32, 2, "S");
    gfxPrint(0, 12, "MASTER FX");
  } else if (mixerPage == 1) {
    gfxPrint(0, 0, beadsReverb() ? "BEADS REVERB" : "CLOUDS REVERB");
    gfxPrint(0, 3, "Algorithm"); gfxPrint(0, 4, "Return"); gfxPrint(0, 5, "Time");
    if (beadsReverb()) { gfxPrint(0, 6, "Tone"); gfxPrint(0, 7, "HP filter"); }
    else { gfxPrint(0, 6, "Damping"); gfxPrint(0, 7, "LP filter"); gfxPrint(0, 8, "HP filter"); }
  } else {
    gfxPrint(0, 0, tapeEcho() ? "TAPE ECHO" : "PING-PONG DELAY");
    gfxPrint(0, 3, "Algorithm"); gfxPrint(0, 4, "Return"); gfxPrint(0, 5, "To Reverb");
    if (tapeEcho()) {
      gfxPrint(0, 6, "Time"); gfxPrint(0, 7, "Heads"); gfxPrint(0, 8, "Feedback");
      gfxPrint(0, 9, "Tone"); gfxPrint(0, 10, "Flutter"); gfxPrint(0, 11, "Spring");
    } else {
      gfxPrint(0, 6, "Ticks"); gfxPrint(0, 7, "Feedback"); gfxPrint(0, 8, "Filter");
    }
  }
}

static void drawCursor(int col, int row) {
  if (mixerPage == 0) {
    static const int x[] = {4, 9, 14, 19, 24, 29, 32};
    static const int width[] = {2, 2, 2, 2, 2, 1, 1};
    if (row == PROJECT_MAX_TRACKS) { gfxCursor(10, 12, 16); return; }
    if (col < 0 || col >= 7 || row < 0 || row >= PROJECT_MAX_TRACKS) return;
    gfxCursor(x[col], 3 + row, width[col]);
  } else if (row >= 0 && row < effectRows()) {
    int filterRow = mixerPage == 1 ? 4 : -1;
    int width = mixerPage == 1 && row == 0 ? (beadsReverb() ? 5 : 6) : 2;
    if (mixerPage == 2) {
      if (row == 0) width = tapeEcho() ? 4 : 8;
      else if (tapeEcho() && row == 3) width = (int)strlen(tapeDivisionName(chipnomadState->project.tapeDivision));
      else if (tapeEcho() && row == 4) width = (int)strlen(tapeHeadsName(chipnomadState->project.tapeHeads));
    }
    if (mixerPage == 1 && row >= filterRow) {
      char value[12];
      unsigned int cutoff = mixerPage == 2 ? chipnomadState->project.delayFilterCutoffHz
        : beadsReverb() ? chipnomadState->project.beadsReverbHighpassCutoffHz
        : row == 5 ? chipnomadState->project.reverbHighpassCutoffHz : chipnomadState->project.reverbFilterCutoffHz;
      snprintf(value, sizeof(value), "%u Hz", cutoff);
      width = (int)strlen(value);
    }
    gfxCursor(12, 3 + row, width);
  }
}

static void drawField(int col, int row, CellState state) {
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  if (mixerPage == 0) {
    if (row == PROJECT_MAX_TRACKS) {
      gfxClearRect(10, 12, 16, 1);
      gfxPrint(10, 12, state == CellState::focus ? "> EDIT MASTER FX <" : "[ EDIT MASTER FX ]");
      return;
    }
    if (col < 0 || col >= 7 || row < 0 || row >= PROJECT_MAX_TRACKS) return;
    gfxPrintf(0, 3 + row, "%2d", row + 1);
    if (col == 0) gfxPrint(4, 3 + row, byteToHex(controlFromRange(chipnomadState->project.trackVolume[row], 100)));
    else if (col == 1) gfxPrint(9, 3 + row, byteToHex(controlFromRange(chipnomadState->project.trackReverbSend[row], 100)));
    else if (col == 2) gfxPrint(14, 3 + row, byteToHex(controlFromRange(chipnomadState->project.trackDelaySend[row], 100)));
    else if (col == 3) gfxPrint(19, 3 + row, byteToHex(chipnomadState->project.trackTilt[row]));
    else if (col == 4) gfxPrint(24, 3 + row, byteToHex(chipnomadState->project.trackPan[row]));
    else if (col == 5) gfxPrint(29, 3 + row, audioManager.trackStates[row] == TRACK_MUTED ? "*" : "-");
    else gfxPrint(32, 3 + row, audioManager.trackStates[row] == TRACK_SOLO ? "*" : "-");
    gfxSetFgColor(chipnomadState->trackClipping[row] ? appSettings.colorScheme.warning : appSettings.colorScheme.textDefault);
    gfxPrint(38, 3 + row, chipnomadState->trackClipping[row] ? "!" : " ");
    return;
  }
  if (row < 0 || row >= effectRows()) return;
  Project* p = &chipnomadState->project;
  if (mixerPage == 1) {
    if (row == 0) gfxPrint(12, 3, beadsReverb() ? "BEADS" : "CLOUDS");
    else if (row == 1) gfxPrint(12, 4, byteToHex(controlFromRange(p->reverbReturn, 100)));
    else if (row == 2) gfxPrint(12, 5, byteToHex(beadsReverb() ? p->beadsReverbTime : p->reverbTime));
    else if (row == 3) gfxPrint(12, 6, byteToHex(beadsReverb() ? p->beadsReverbTone : p->reverbDamping));
    else {
      unsigned int cutoff = beadsReverb() ? p->beadsReverbHighpassCutoffHz : row == 4 ? p->reverbFilterCutoffHz : p->reverbHighpassCutoffHz;
      gfxClearRect(12, 3 + row, 8, 1);
      gfxPrintf(12, 3 + row, "%u Hz", cutoff);
    }
  } else {
    if (row == 0) gfxPrint(12, 3, tapeEcho() ? "TAPE" : "PINGPONG");
    else if (row == 1) gfxPrint(12, 4, byteToHex(controlFromRange(p->delayReturn, 100)));
    else if (row == 2) gfxPrint(12, 5, byteToHex(controlFromRange(p->delayReverbSend, 100)));
    else if (!tapeEcho() && row == 3) gfxPrintf(12, 6, "%02X", p->delayTicks);
    else if (!tapeEcho() && row == 4) gfxPrint(12, 7, byteToHex(controlFromRange(p->delayFeedback, 95)));
    else if (!tapeEcho()) {
      gfxClearRect(12, 8, 8, 1);
      gfxPrintf(12, 8, "%u Hz", p->delayFilterCutoffHz);
    } else if (row == 3) gfxPrint(12, 6, tapeDivisionName(p->tapeDivision));
    else if (row == 4) gfxPrint(12, 7, tapeHeadsName(p->tapeHeads));
    else if (row == 5) gfxPrint(12, 8, byteToHex(controlFromRange(p->delayFeedback, 95)));
    else if (row == 6) gfxPrint(12, 9, byteToHex(p->tapeTone));
    else if (row == 7) gfxPrint(12, 10, byteToHex(controlFromRange(p->tapeFlutter, 100)));
    else gfxPrint(12, 11, byteToHex(controlFromRange(p->tapeSpring, 100)));
  }
}

static int onEdit(int col, int row, CellEditAction action) {
  Project* p = &chipnomadState->project;
  int handled = 0;
  if (mixerPage == 0) {
    if (row == PROJECT_MAX_TRACKS) {
      if (action != CellEditAction::tap) return 0;
      screenInsertFXOpenMaster();
      return 1;
    }
    if (col < 0 || col >= 7 || row < 0 || row >= PROJECT_MAX_TRACKS) return 0;
    // These values are stored as 0-100 percentages.  Do not round-trip them
    // through the 0-255 display scale: a one-step left/right edit can round
    // back to the same percentage and appear broken.
    if (col == 0) handled = edit8noLast(action, &p->trackVolume[row], 10, 0, 100);
    else if (col == 1) handled = edit8noLast(action, &p->trackReverbSend[row], 10, 0, 100);
    else if (col == 2) handled = edit8noLast(action, &p->trackDelaySend[row], 10, 0, 100);
    else if (col == 3) handled = edit8noLast(action, &p->trackTilt[row], 16, 0, 255);
    else if (col == 4 && action == CellEditAction::clear) { p->trackPan[row] = 0x80; handled = 1; }
    else if (col == 4) handled = edit8noLast(action, &p->trackPan[row], 16, 0, 255);
    else if (action == CellEditAction::tap) {
      if (col == 5) audioManager.toggleTrackMute(row); else audioManager.toggleTrackSolo(row);
      handled = 1;
    }
  } else if (row >= 0 && row < effectRows()) {
    if (mixerPage == 1) {
      if (row == 0) handled = edit8noLast(action, &p->reverbAlgorithm, 1, 0, 1);
      else if (row == 1) handled = edit8noLast(action, &p->reverbReturn, 10, 0, 100);
      else if (row == 2) handled = edit8noLast(action, beadsReverb() ? &p->beadsReverbTime : &p->reverbTime, 16, 0, 255);
      else if (row == 3) handled = edit8noLast(action, beadsReverb() ? &p->beadsReverbTone : &p->reverbDamping, 16, 0, 255);
      else if (beadsReverb()) handled = editFilterCutoff(action, &p->beadsReverbHighpassCutoffHz);
      else handled = editFilterCutoff(action, row == 4 ? &p->reverbFilterCutoffHz : &p->reverbHighpassCutoffHz);
    } else {
      if (row == 0) handled = edit8noLast(action, &p->delayAlgorithm, 1, 0, 1);
      else if (row == 1) handled = edit8noLast(action, &p->delayReturn, 10, 0, 100);
      else if (row == 2) handled = edit8noLast(action, &p->delayReverbSend, 10, 0, 100);
      else if (!tapeEcho() && row == 3) handled = edit8noLast(action, &p->delayTicks, 4, 1, 255);
      else if (!tapeEcho() && row == 4) handled = edit8noLast(action, &p->delayFeedback, 10, 0, 95);
      else if (!tapeEcho()) handled = editFilterCutoff(action, &p->delayFilterCutoffHz);
      else if (row == 3) handled = edit8noLast(action, &p->tapeDivision, 1, 0, 6);
      else if (row == 4) handled = edit8noLast(action, &p->tapeHeads, 1, 0, 6);
      else if (row == 5) handled = edit8noLast(action, &p->delayFeedback, 10, 0, 95);
      else if (row == 6) handled = edit8noLast(action, &p->tapeTone, 16, 0, 255);
      else if (row == 7) handled = edit8noLast(action, &p->tapeFlutter, 10, 0, 100);
      else handled = edit8noLast(action, &p->tapeSpring, 10, 0, 100);
    }
  }
  if (handled) {
    projectModified = 1;
    if ((mixerPage == 0 && col >= 5) || (mixerPage == 1 && row == 0) || (mixerPage == 2 && row == 0)) {
      if (mixerPage == 2 && row == 0) refreshEffectRows();
      fullRedraw();
    }
  }
  return handled;
}

static ScreenData screen = {
  .rows = PROJECT_MAX_TRACKS, .cursorRow = 0, .cursorCol = 0, .topRow = 0,
  .selectMode = -1, .selectStartRow = 0, .selectStartCol = 0,
  .selectAnchorRow = 0, .selectAnchorCol = 0, .playbackLevel = ScreenPlaybackLevel::song,
  .getColumnCount = columnCount, .drawStatic = drawStatic, .drawCursor = drawCursor,
  .drawSelection = NULL, .drawRowHeader = drawRowHeader, .drawColHeader = drawColHeader,
  .drawField = drawField, .onEdit = onEdit, .onInput = NULL, .onRawInput = NULL,
  .isCellValid = NULL, .getLoopRange = NULL,
};

static void setup(int input) { displayedCpuLoad = -1; mixerPage = 0; screen.rows = PROJECT_MAX_TRACKS + 1; }
static void fullRedraw(void) { screenFullRedraw(&screen); }
static void refreshEffectRows(void) { screen.rows = effectRows(); }
static void draw(void) {
  if (mixerPage == 0) {
    for (int track = 0; track < chipnomadState->project.tracksCount; ++track)
      monitorDisplayDrawMeter(track, 2, 3 + track);
    drawAnalyzers();
    gfxClearRect(34, 12, 5, 1);
    if (chipnomadState->audioOverload) {
      gfxSetFgColor(appSettings.colorScheme.warning);
      gfxPrint(34, 12, "CLIP!");
    }
  }
  int cpuLoad = audioManager.getCpuLoadPercent();
  if (cpuLoad == displayedCpuLoad) return;
  displayedCpuLoad = cpuLoad;
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxClearRect(31, 0, 9, 1);
  gfxPrintf(31, 0, "CPU %03d%%", cpuLoad);
}

static void setPage(int page) {
  mixerPage = page;
  screen.rows = page == 0 ? PROJECT_MAX_TRACKS + 1 : effectRows();
  screen.cursorRow = 0;
  screen.cursorCol = 0;
  fullRedraw();
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (isKeyDown && keys == (keyLeft | keyShift) && mixerPage != 0) {
    setPage(0);
    return 1;
  }
  if (isKeyDown && keys == (keyUp | keyShift)) {
    setPage(mixerPage == 2 ? 0 : 1);
    return 1;
  }
  if (isKeyDown && keys == (keyDown | keyShift)) {
    setPage(mixerPage == 1 ? 0 : 2);
    return 1;
  }
  if (isKeyDown && keys == (keyRight | keyShift)) {
    screenSetup(&screenSong, 0);
    return 1;
  }
  return screenInput(&screen, isKeyDown, keys, tapCount);
}

static ScreenPlaybackLevel getPlaybackLevel(void) { return ScreenPlaybackLevel::song; }

const AppScreen screenMixer = {
  .init = NULL, .setup = setup, .fullRedraw = fullRedraw, .draw = draw,
  .onInput = onInput, .getPlaybackLevel = getPlaybackLevel,
};
