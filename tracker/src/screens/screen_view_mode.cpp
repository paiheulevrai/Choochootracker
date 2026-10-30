#include "common.h"
#include "screens.h"
#include "screen_project.h"
#include "project_utils.h"
#include "corelib_gfx.h"
#include "audio_manager.h"
#include "export_path.h"
#include <string.h>
#include <chrono>

// Mode chooser: shown after NEW (from the title screen or the project
// screen). The chosen view mode is stored per project and cannot be
// changed afterwards (conversion between modes is out of scope for now),
// so this screen is the only place a new project's mode is decided.
//
// Layout:
//   row 2: "NEW PROJECT" title
//   row 4: "> Classic    Song > Chain > Phrase"
//   row 5: "  Simplified Song > Adjustable length pattern"
//   row 8: helper text (marquee when it does not fit)
//   row 17: "EDIT: confirm   OPT: cancel"

#define VIEW_MODE_OPTION_COUNT (2)

static const AppScreen* returnScreen = NULL;
static int selected = 0;

static const char* modeNames[VIEW_MODE_OPTION_COUNT] = {
  "Classic",
  "Simplified",
};

static const char* modeDescriptions[VIEW_MODE_OPTION_COUNT] = {
  "Song > Chain > Phrase",
  "Song > Adjustable length pattern",
};

static const char* helperText =
  "View mode is stored in the project and cannot be changed later";

// Marquee timing (same feel as the fork's selection popup): hold 1s,
// scroll at 180ms per character, hold 1s at the end, then restart.
static std::chrono::steady_clock::time_point helperSince;
static size_t helperOffset;

static void resetMarquee(void) {
  helperSince = std::chrono::steady_clock::now();
  helperOffset = 0;
}

static void fullRedraw(void) {
  gfxSetBgColor(appSettings.colorScheme.background);
  gfxClear();

  const ColorScheme cs = appSettings.colorScheme;

  gfxSetFgColor(cs.textTitles);
  gfxPrint(0, 2, "NEW PROJECT");

  for (int i = 0; i < VIEW_MODE_OPTION_COUNT; i++) {
    gfxSetFgColor(i == selected ? cs.textValue : cs.textDefault);
    gfxPrintf(0, 4 + i, "%c %-11s", i == selected ? '>' : ' ', modeNames[i]);
    gfxSetFgColor(cs.textInfo);
    gfxPrint(14, 4 + i, modeDescriptions[i]);
  }

  // Helper text: scroll horizontally when it does not fit on one line.
  gfxSetFgColor(cs.textInfo);
  const size_t length = strlen(helperText);
  const size_t width = 40;
  size_t offset = 0;
  if (length > width) {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - helperSince).count();
    const auto travel = (long long)(length - width) * 180;
    const auto phase = elapsed % (1000 + travel + 1000);
    if (phase >= 1000) {
      long long pos = (phase - 1000) / 180 + 1;
      if (pos > (long long)(length - width)) pos = length - width;
      offset = (size_t)pos;
    }
  }
  if (offset != helperOffset) {
    helperOffset = offset;
    fullRedraw();
    return;
  }
  // gfxPrint wraps at the screen edge instead of clipping, so the visible
  // slice must be precision-limited to at most 40 characters.
  const size_t remaining = length - helperOffset;
  const size_t visible = remaining < width ? remaining : width;
  gfxPrintf(0, 8, "%.*s", (int)visible, helperText + helperOffset);

  gfxSetFgColor(cs.textInfo);
  gfxPrint(0, 17, "EDIT: confirm   OPT: cancel");
}

static void setup(int input) {
  (void)input;
  selected = 0;
  resetMarquee();
  fullRedraw();
}

static void draw(void) {
  // Advance the marquee: recompute the offset and redraw only when it
  // changed (fullRedraw re-enters here through the offset check).
  const size_t length = strlen(helperText);
  const size_t width = 40;
  if (length > width) {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - helperSince).count();
    const auto travel = (long long)(length - width) * 180;
    const auto phase = elapsed % (1000 + travel + 1000);
    size_t offset = 0;
    if (phase >= 1000) {
      long long pos = (phase - 1000) / 180 + 1;
      if (pos > (long long)(length - width)) pos = length - width;
      offset = (size_t)pos;
    }
    if (offset != helperOffset) fullRedraw();
  }
}

static void confirm(void) {
  Project* project = &chipnomadState->project;

  // Build the new project. projectInitAY always yields a classic project
  // (projectInit resets viewMode), so the chosen mode is applied after it,
  // followed by simplified seeding when needed.
  projectInitAY(project);
  project->viewMode = (selected == 1) ? ViewMode::simplified : ViewMode::classic;
  if (project->viewMode == ViewMode::simplified) {
    projectUtilsSeedSimplifiedPatterns(project);
  }

  // Same handover as the plain new-project flow: swap the audio engine to
  // the fresh project, reset export folder tracking, persist settings.
  audioManager.replaceProject(project);
  projectModified = 0;
  appSettings.projectFilename[0] = 0;
  exportResetFolderTracking();
  settingsSave();
  screensInitAll();
  screenSetup(&screenSong, 0);
}

static void cancel(void) {
  screenSetup(returnScreen ? returnScreen : &screenProject, 0);
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  (void)tapCount;
  if (!isKeyDown) return 1;

  if (keys == keyUp && selected > 0) {
    selected--;
    fullRedraw();
    return 1;
  } else if (keys == keyDown && selected < VIEW_MODE_OPTION_COUNT - 1) {
    selected++;
    fullRedraw();
    return 1;
  } else if (keys == keyEdit || keys == keyPlay) {
    confirm();
    return 1;
  } else if (keys == keyOpt) {
    cancel();
    return 1;
  }
  return 1; // Swallow everything else: no playback shortcuts on this screen
}

const AppScreen screenViewMode = {
  .init = NULL,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = NULL,
};

void viewModeChooserSetup(const AppScreen* fromScreen) {
  returnScreen = fromScreen ? fromScreen : &screenProject;
  screenSetup(&screenViewMode, 0);
}
