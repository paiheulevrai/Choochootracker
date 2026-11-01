#include "common.h"
#include "screens.h"
#include "screen_project.h"
#include "project_utils.h"
#include "corelib_gfx.h"
#include "audio_manager.h"
#include "export_path.h"
#include <string.h>

// Mode chooser: shown after NEW (from the title screen or the project
// screen). The chosen view mode is stored per project and cannot be
// changed afterwards (conversion between modes is out of scope for now),
// so this screen is the only place a new project's mode is decided.
//
// Layout:
//   row 2:  "NEW PROJECT" title
//   row 4:  "> Classic"      (selected: value color, else default)
//   row 5:  "  Simplified"
//   row 7+: description of the SELECTED mode, word-wrapped to fit
//   below:  always the fixed notice, word-wrapped
//
// Both texts are word-wrapped (no marquee — wrapped, always-readable
// text per spec). EDIT confirms, OPT cancels (no footer row).

#define VIEW_MODE_OPTION_COUNT (2)

static const AppScreen* returnScreen = NULL;
static int selected = 0;

static const char* modeNames[VIEW_MODE_OPTION_COUNT] = {
  "Classic",
  "Simplified",
};

static const char* modeDescriptions[VIEW_MODE_OPTION_COUNT] = {
  "Follows LSDJ style Song -> Chain -> Phrase navigation.",
  "Song has no chains, but adjustable length patterns.",
};

static const char* fixedNotice =
  "View is fixed to the project and it cannot be changed in the future (for now).";

#define CHOOSER_TEXT_WIDTH (40)

// Word-wrap `text` into lines of at most `width` chars; breaks at spaces,
// never mid-word. Returns the line count; lines[] hold up to 6 entries.
static int wrapText(const char* text, int width, char lines[][CHOOSER_TEXT_WIDTH + 1]) {
  int lineCount = 0;
  const char* p = text;
  while (*p && lineCount < 6) {
    while (*p == ' ') p++;
    if (!*p) break;
    int take = 0;
    int lastSpace = -1;
    while (p[take] && take < width) {
      if (p[take] == ' ') lastSpace = take;
      take++;
    }
    if (p[take] && lastSpace > 0) take = lastSpace;  // break at the space
    if (take == 0) take = 1;                        // defensive
    memcpy(lines[lineCount], p, take);
    lines[lineCount][take] = 0;
    lineCount++;
    p += take;
  }
  return lineCount;
}

static void drawDescription(int y, const char* text, int color) {
  char lines[6][CHOOSER_TEXT_WIDTH + 1];
  int count = wrapText(text, CHOOSER_TEXT_WIDTH, lines);
  gfxSetFgColor(color);
  for (int i = 0; i < count; i++) {
    gfxPrint(0, y + i, lines[i]);
  }
}

static void fullRedraw(void) {
  gfxSetBgColor(appSettings.colorScheme.background);
  gfxClear();

  const ColorScheme cs = appSettings.colorScheme;

  gfxSetFgColor(cs.textTitles);
  gfxPrint(0, 2, "NEW PROJECT");

  for (int i = 0; i < VIEW_MODE_OPTION_COUNT; i++) {
    gfxSetFgColor(i == selected ? cs.textValue : cs.textDefault);
    gfxPrintf(0, 4 + i, "%c %s", i == selected ? '>' : ' ', modeNames[i]);
  }

  // Description of the selected mode (wrapped), then the fixed notice.
  drawDescription(7, modeDescriptions[selected], cs.textInfo);
  drawDescription(10, fixedNotice, cs.textInfo);
}

static void setup(int input) {
  (void)input;
  selected = 0;
  fullRedraw();
}

static void draw(void) {
  // Static screen: nothing animates (descriptions are wrapped, not
  // scrolled). No per-frame work.
}

static void confirm(void) {
  // Build the new project in a LOCAL struct — replaceProject() frees the
  // current project and then zeroes its argument, so passing
  // &chipnomadState->project here would zero the live project (the bug that
  // wiped navigation: tracksCount 0, ARP in every FX lane). This mirrors the
  // original doNewProject flow.
  Project replacement;
  projectInitAY(&replacement);

  // projectInitAY always yields a classic project (projectInit resets
  // viewMode), so the chosen mode is applied after it, followed by simplified
  // seeding when needed.
  replacement.viewMode = (selected == 1) ? ViewMode::simplified : ViewMode::classic;
  if (replacement.viewMode == ViewMode::simplified) {
    projectUtilsSeedSimplifiedPatterns(&replacement);
  }

  // Same handover as the plain new-project flow: swap the audio engine to
  // the fresh project, reset export folder tracking, persist settings.
  audioManager.replaceProject(&replacement);
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
