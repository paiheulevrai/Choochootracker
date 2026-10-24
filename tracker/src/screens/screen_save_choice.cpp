#include "screens.h"
#include "corelib_gfx.h"
#include <string.h>

// Save-destination dialog (Phase 4): SAVE TO SAMPLE writes the slice points
// as WAV cue chunks (portable, visible in DAWs), SAVE TO PROJECT keeps them
// in the .cct only. A "don't ask again" checkbox stores the choice in the
// project (Project::sampleSaveChoice) so the dialog is skipped from then on.

static char sampleName[40];
static int selectedOption = 0; // 0 = sample, 1 = project, 2 = cancel
static int dontAskAgain = 0;
static void (*onSample)(void);
static void (*onProject)(void);
static void (*onCancel)(void);

void saveChoiceSetup(const char* name, void (*sampleCallback)(void),
                     void (*projectCallback)(void), void (*cancelCallback)(void)) {
  strncpy(sampleName, name ? name : "", sizeof(sampleName) - 1);
  sampleName[sizeof(sampleName) - 1] = 0;
  selectedOption = 0;
  dontAskAgain = 0;
  onSample = sampleCallback;
  onProject = projectCallback;
  onCancel = cancelCallback;
}

static void setup(int input) {
}

static void fullRedraw(void) {
  gfxSetBgColor(appSettings.colorScheme.background);
  gfxClear();

  gfxSetFgColor(appSettings.colorScheme.selection);
  gfxPrint(0, 8, "SAVE SLICES");
  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(0, 9, sampleName);

  if (selectedOption == 0) {
    gfxSetFgColor(appSettings.colorScheme.textValue);
    gfxPrint(0, 11, "> SAVE TO SAMPLE");
  } else {
    gfxSetFgColor(appSettings.colorScheme.textDefault);
    gfxPrint(0, 11, "  SAVE TO SAMPLE");
  }
  if (selectedOption == 1) {
    gfxSetFgColor(appSettings.colorScheme.textValue);
    gfxPrint(0, 12, "> SAVE TO PROJECT");
  } else {
    gfxSetFgColor(appSettings.colorScheme.textDefault);
    gfxPrint(0, 12, "  SAVE TO PROJECT");
  }
  if (selectedOption == 2) {
    gfxSetFgColor(appSettings.colorScheme.textValue);
    gfxPrint(0, 13, "> CANCEL");
  } else {
    gfxSetFgColor(appSettings.colorScheme.textDefault);
    gfxPrint(0, 13, "  CANCEL");
  }

  gfxSetFgColor(appSettings.colorScheme.textDefault);
  gfxPrint(0, 15, dontAskAgain ? "[*] Don't ask again in this project"
                               : "[ ] Don't ask again in this project");
}

static void draw(void) {
}

static void confirmSelection(void) {
  if (selectedOption == 0) {
    if (dontAskAgain) {
      chipnomadState->project.sampleSaveChoice = 1;
      projectModified = 1;
    }
    if (onSample) onSample();
  } else if (selectedOption == 1) {
    if (dontAskAgain) {
      chipnomadState->project.sampleSaveChoice = 2;
      projectModified = 1;
    }
    if (onProject) onProject();
  } else if (onCancel) {
    onCancel();
  }
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  (void)isKeyDown;
  (void)tapCount;
  if (keys == keyUp && selectedOption > 0) {
    selectedOption--;
    fullRedraw();
    return 1;
  } else if (keys == keyDown && selectedOption < 2) {
    selectedOption++;
    fullRedraw();
    return 1;
  } else if (keys == keyEdit) {
    confirmSelection();
    return 1;
  } else if (keys == keyOpt && onCancel) {
    onCancel();
    return 1;
  }
  return 0;
}

const AppScreen screenSaveChoice = {
  .init = NULL,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = NULL,
};
