#include "screens.h"
#include "audio_manager.h"
#include "common.h"
#include "corelib_gfx.h"
#include <stdio.h>
#include <string.h>

static void fullRedraw(void);
static int displayedCpuLoad = -1;
static int mixerPage = 0; // 0 mixer, 1 reverb, 2 delay
static uint8_t autoMixPreview[PROJECT_MAX_TRACKS];
static uint8_t autoMixOriginal[PROJECT_MAX_TRACKS];

static void applyAutoMix(void) {
  projectModified = 1;
  screenSetup(&screenMixer, 0);
}
static void cancelAutoMix(void) {
  for (int i = 0; i < chipnomadState->project.tracksCount; ++i)
    chipnomadState->project.trackVolume[i] = autoMixOriginal[i];
  screenSetup(&screenMixer, 0);
}

int screenMixerGetPage(void) { return mixerPage; }

static int columnCount(int row) { return mixerPage == 0 ? (row == PROJECT_MAX_TRACKS ? 1 : 5) : 1; }
static void drawRowHeader(int row, CellState state) {}
static void drawColHeader(int col, CellState state) {}

static void drawStatic(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  if (mixerPage == 0) {
    gfxPrint(0, 0, "MIXER");
    gfxPrint(3, 2, "LVL"); gfxPrint(8, 2, "REV"); gfxPrint(13, 2, "DLY");
    gfxPrint(18, 2, "MUTE"); gfxPrint(25, 2, "SOLO");
    gfxPrint(32, 2, "CLIP");
    gfxPrint(0, 12, "AUTO MIX");
  } else if (mixerPage == 1) {
    gfxPrint(0, 0, "CLOUDS REVERB");
    gfxPrint(0, 3, "Return"); gfxPrint(0, 4, "Time");
    gfxPrint(0, 5, "Damping"); gfxPrint(0, 6, "Filter");
  } else {
    gfxPrint(0, 0, "PING-PONG DELAY");
    gfxPrint(0, 3, "Return"); gfxPrint(0, 4, "To Reverb");
    gfxPrint(0, 5, "Ticks"); gfxPrint(0, 6, "Feedback"); gfxPrint(0, 7, "Filter");
  }
}

static void drawCursor(int col, int row) {
  if (mixerPage == 0) {
    static const int x[] = {3, 8, 13, 18, 25};
    static const int width[] = {3, 3, 3, 3, 3};
    if (row == PROJECT_MAX_TRACKS) { gfxCursor(10, 12, 16); return; }
    if (col < 0 || col >= 5 || row < 0 || row >= PROJECT_MAX_TRACKS) return;
    gfxCursor(x[col], 3 + row, width[col]);
  } else if (row >= 0 && row < (mixerPage == 2 ? 5 : 4)) {
    int width = row == (mixerPage == 2 ? 4 : 3) ? 8 : 4;
    if (mixerPage == 2 && row == 2) width = 2;
    gfxCursor(12, 3 + row, width);
  }
}

static void drawField(int col, int row, CellState state) {
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);
  if (mixerPage == 0) {
    if (row == PROJECT_MAX_TRACKS) {
      gfxClearRect(10, 12, 16, 1);
      gfxPrint(10, 12, state == CellState::focus ? "> RUN AUTO MIX <" : "[ RUN AUTO MIX ]");
      return;
    }
    if (col < 0 || col >= 5 || row < 0 || row >= PROJECT_MAX_TRACKS) return;
    gfxPrintf(0, 3 + row, "T%d", row + 1);
    if (col == 0) gfxPrintf(3, 3 + row, "%03d", chipnomadState->project.trackVolume[row]);
    else if (col == 1) gfxPrintf(8, 3 + row, "%03d", chipnomadState->project.trackReverbSend[row]);
    else if (col == 2) gfxPrintf(13, 3 + row, "%03d", chipnomadState->project.trackDelaySend[row]);
    else if (col == 3) gfxPrint(18, 3 + row, audioManager.trackStates[row] == TRACK_MUTED ? "ON " : "OFF");
    else gfxPrint(25, 3 + row, audioManager.trackStates[row] == TRACK_SOLO ? "ON " : "OFF");
    gfxSetFgColor(chipnomadState->trackClipping[row] ? appSettings.colorScheme.warning : appSettings.colorScheme.textDefault);
    gfxPrint(32, 3 + row, chipnomadState->trackClipping[row] ? "!" : " ");
    return;
  }
  if (row < 0 || row >= (mixerPage == 2 ? 5 : 4)) return;
  Project* p = &chipnomadState->project;
  if (mixerPage == 1) {
    if (row == 0) gfxPrintf(12, 3, "%03d%%", p->reverbReturn);
    else if (row == 1) gfxPrintf(12, 4, "%03d%%", p->reverbTime * 100 / 255);
    else if (row == 2) gfxPrintf(12, 5, "%03d%%", p->reverbDamping * 100 / 255);
    else gfxPrintf(12, 6, "%u Hz", p->reverbFilterCutoffHz);
  } else {
    if (row == 0) gfxPrintf(12, 3, "%03d%%", p->delayReturn);
    else if (row == 1) gfxPrintf(12, 4, "%03d%%", p->delayReverbSend);
    else if (row == 2) gfxPrintf(12, 5, "%02X", p->delayTicks);
    else if (row == 3) gfxPrintf(12, 6, "%03d%%", p->delayFeedback);
    else gfxPrintf(12, 7, "%u Hz", p->delayFilterCutoffHz);
  }
}

static int onEdit(int col, int row, CellEditAction action) {
  Project* p = &chipnomadState->project;
  int handled = 0;
  if (mixerPage == 0) {
    if (row == PROJECT_MAX_TRACKS) {
      if (action != CellEditAction::tap) return 0;
      if (chipnomadAutoMix(chipnomadState, 6, autoMixPreview)) screenMessage(MESSAGE_TIME, "Auto mix: no audio");
      else {
        char summary[128] = "Apply? ";
        for (int i = 0; i < chipnomadState->project.tracksCount; ++i) {
          autoMixOriginal[i] = chipnomadState->project.trackVolume[i];
          char change[16];
          snprintf(change, sizeof(change), "T%d %d>%d ", i + 1,
            autoMixOriginal[i], autoMixPreview[i]);
          strncat(summary, change, sizeof(summary) - strlen(summary) - 1);
          chipnomadState->project.trackVolume[i] = autoMixPreview[i];
        }
        chipnomadQueuePlaybackStartSong(chipnomadState, 0, 0, 1);
        confirmSetup(summary, applyAutoMix, cancelAutoMix);
        screenSetup(&screenConfirm, 0);
      }
      return 1;
    }
    if (col < 0 || col >= 5 || row < 0 || row >= PROJECT_MAX_TRACKS) return 0;
    if (col == 0) handled = edit8noLast(action, &p->trackVolume[row], 10, 0, 100);
    else if (col == 1) handled = edit8noLast(action, &p->trackReverbSend[row], 10, 0, 100);
    else if (col == 2) handled = edit8noLast(action, &p->trackDelaySend[row], 10, 0, 100);
    else if (action == CellEditAction::tap) {
      if (col == 3) audioManager.toggleTrackMute(row); else audioManager.toggleTrackSolo(row);
      handled = 1;
    }
  } else if (row >= 0 && row < (mixerPage == 2 ? 5 : 4)) {
    if (mixerPage == 1) {
      if (row == 0) handled = edit8noLast(action, &p->reverbReturn, 10, 0, 100);
      else if (row == 1) handled = edit8noLast(action, &p->reverbTime, 16, 0, 255);
      else if (row == 2) handled = edit8noLast(action, &p->reverbDamping, 16, 0, 255);
      else handled = editFilterCutoff(action, &p->reverbFilterCutoffHz);
    } else {
      if (row == 0) handled = edit8noLast(action, &p->delayReturn, 10, 0, 100);
      else if (row == 1) handled = edit8noLast(action, &p->delayReverbSend, 10, 0, 100);
      else if (row == 2) handled = edit8noLast(action, &p->delayTicks, 4, 1, 255);
      else if (row == 3) handled = edit8noLast(action, &p->delayFeedback, 10, 0, 95);
      else handled = editFilterCutoff(action, &p->delayFilterCutoffHz);
    }
  }
  if (handled) {
    projectModified = 1;
    if (mixerPage == 0 && col >= 3) fullRedraw();
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
static void draw(void) {
  int cpuLoad = audioManager.getCpuLoadPercent();
  if (cpuLoad == displayedCpuLoad) return;
  displayedCpuLoad = cpuLoad;
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxClearRect(31, 1, 9, 1);
  gfxPrintf(31, 1, "CPU %03d%%", cpuLoad);
}

static void setPage(int page) {
  mixerPage = page;
  screen.rows = page == 0 ? PROJECT_MAX_TRACKS + 1 : (page == 2 ? 5 : 4);
  screen.cursorRow = 0;
  screen.cursorCol = 0;
  fullRedraw();
}

static int onInput(int isKeyDown, int keys, int tapCount) {
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
