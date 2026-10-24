#include "screen_project.h"
#include "common.h"
#include "corelib/corelib_file.h"
#include "corelib_gfx.h"
#include "utils.h"
#include "chipnomad_lib.h"
#include "project_utils.h"
#include "pitch_table_utils.h"
#include "version.h"
#include "audio_manager.h"
#include "file_browser.h"
#include "export_path.h"
#include "import/import_vt2.h"
#include "import/import_midi.h"
#include "import/import_m8s.h"
#include "string_utils.h"
#include "midi/midi_router.h"
#include "link/link_client.h"
#include <string.h>
#include <strings.h>

#include <chrono>

#ifdef WEB_BUILD
#include <emscripten/emscripten.h>
#endif

static int isCharEdit = 0;
static const AppScreen* projectReturnScreen = &screenProject;
static char* editingString = NULL;
static int editingStringLength = 0;
// Tempo is edited/displayed in whole BPM. The project's canonical tickRate
// stays in Hz (see project_io.cpp's "- Frame rate" format): BPM =
// tickRate * 60/24 = tickRate * 2.5, so tickRate = BPM * 0.4. Old projects
// with a fractional tempo (e.g. 59 Hz = 147.5 BPM) display rounded and snap
// to a whole BPM on the first tempo edit; untouched projects save unchanged.
static uint16_t bpmValue = 125;
// Mirror of appSettings.clockSource for the Clock source row.
// 0 = Internal, 1 = Midi Clock, 2 = Ableton Link (future).
static uint8_t clockSourceMirror = 0;

// Whole-BPM range: tickRate 1-200 Hz maps to 2.5-500 BPM, so whole numbers
// start at 3.
#define BPM_MIN (3)
#define BPM_MAX (500)
#define BPM_DEFAULT (125) // the old 50 Hz default

int projectLoadFromPath(const char* path) {
  if (!path) {
    return 1;
  }

  // Store the directory path from the selected file
  const char* lastSeparator = strrchr(path, PATH_SEPARATOR);
  if (lastSeparator) {
    int pathLen = lastSeparator - path;
    if (pathLen > 0 && pathLen < PATH_LENGTH) {
      strncpy(appSettings.projectPath, path, pathLen);
      appSettings.projectPath[pathLen] = '\0';
    }
  } else {
    appSettings.projectPath[0] = '\0';
  }

  // Check file extension to determine loader
  const char* ext = strrchr(path, '.');
  int loadResult = -1;
  Project replacement;
  projectInitAY(&replacement);

  if (ext != NULL) {
    if (strcasecmp(ext, ".vt2") == 0) {
      // Load VT2 file
      loadResult = projectLoadVT2(&replacement, path);
    } else if (strcasecmp(ext, ".mid") == 0 || strcasecmp(ext, ".midi") == 0) {
      // Import a Standard MIDI File as a new project
      loadResult = projectLoadMidi(&replacement, path);
    } else if (strcasecmp(ext, ".m8s") == 0) {
      // Import a Dirtywave M8 song (structure and notes only)
      loadResult = projectLoadM8S(&replacement, path);
    } else if (strcasecmp(ext, ".cct") == 0) {
      // Load ChooChooTracker native format
      loadResult = projectLoad(&replacement, path);
    } else {
      // Try native format by default
      loadResult = projectLoad(&replacement, path);
    }
  } else {
    // No extension, try native format
    loadResult = projectLoad(&replacement, path);
  }

  if (loadResult == 0) {
    audioManager.replaceProject(&replacement);
    autosaveLoadFailed = 0;
    projectModified = 0; // Clear modified flag after loading

    // Store filename without extension
    extractFilenameWithoutExtension(path, appSettings.projectFilename, FILENAME_LENGTH + 1);
    // The loaded project's export folder (if any) is adopted on next export
    exportResetFolderTracking();
    settingsSave();

    // Reset all screen states (including song position)
    screensInitAll();
    // Clear FX states for all tracks
    // TODO: Make it a cleaner solution and have it somewhere in chipnomad_lib
    for (int i = 0; i < PROJECT_MAX_TRACKS; i++) {
      chipnomadQueuePlaybackClearTrackFX(chipnomadState, i);
    }
  } else {
    projectFree(&replacement);
    screenMessage(MESSAGE_TIME_ERROR, "%s", projectFileError);
  }

  return loadResult;
}

static void onProjectLoaded(const char* path) {
  if (projectLoadFromPath(path) == 0 && projectReturnScreen == &screenTitle)
    screenSetup(&screenSong, 0);
  else
    screenSetup(projectReturnScreen, 0);
}

#ifdef WEB_BUILD
extern "C" EMSCRIPTEN_KEEPALIVE int webLoadProject(const char* path) {
  return projectLoadFromPath(path);
}
#endif

static void onProjectSaved(const char* folderPath) {
  char fullPath[2048];
  snprintf(fullPath, sizeof(fullPath), "%s%s%s.cct", folderPath, PATH_SEPARATOR_STR, appSettings.projectFilename);

  if (projectSave(&chipnomadState->project, fullPath) == 0) {
    // Save the directory path
    projectModified = 0; // Clear modified flag after saving
    strncpy(appSettings.projectPath, folderPath, PATH_LENGTH);
    appSettings.projectPath[PATH_LENGTH] = 0;
    // If exports were made under a previous project name, move that folder
    // to match the name the project was just saved with
    exportSyncFolderWithProjectName();
    settingsSave();
  }
  screenSetup(&screenProject, 0);
}

static void onProjectCancelled(void) {
  screenSetup(projectReturnScreen, 0);
}

static void doLoadProject(void) {
  fileBrowserSetup("LOAD PROJECT", ".cct,.vt2,.mid,.midi,.m8s", appSettings.projectPath,
    onProjectLoaded, onProjectCancelled);
  screenSetup(&screenFileBrowser, 0);
}

static void doNewProject(void) {
  Project replacement;
  projectInitAY(&replacement);
  audioManager.replaceProject(&replacement);
  projectModified = 0;
  appSettings.projectFilename[0] = 0;
  exportResetFolderTracking();
  settingsSave();
  screensInitAll();
  screenSetup(projectReturnScreen, 0);
}

void projectOpenFromScreen(const AppScreen* returnScreen) {
  projectReturnScreen = returnScreen ? returnScreen : &screenProject;
  doLoadProject();
}

void projectOpenFromScreenAtPath(const AppScreen* returnScreen, const char* path) {
  projectReturnScreen = returnScreen ? returnScreen : &screenProject;
  fileBrowserSetup("LOAD PROJECT", ".cct,.vt2,.mid,.midi,.m8s", path,
    onProjectLoaded, onProjectCancelled);
  screenSetup(&screenFileBrowser, 0);
}

void projectCreateNewFromScreen(const AppScreen* returnScreen) {
  projectReturnScreen = returnScreen ? returnScreen : &screenProject;
  doNewProject();
}

static void cancelConfirm(void) {
  screenSetup(&screenProject, 0);
}

static void drawRowHeader(int row, CellState state);
static void drawColHeader(int col, CellState state);

static ScreenData* projectScreen(void) {
  ScreenData* data = &screenProjectExtra;
  data->drawRowHeader = drawRowHeader;
  data->drawColHeader = drawColHeader;

  return data;
}

static void setup(int input) {
  isCharEdit = 0;
  editingString = NULL;
  editingStringLength = 0;
  // Round to whole BPM: 50 Hz -> 125, 59 Hz -> 148 (147.5 rounded).
  int bpm = (int)(chipnomadState->project.tickRate * 2.5f + 0.5f);
  if (bpm < BPM_MIN) bpm = BPM_MIN;
  if (bpm > BPM_MAX) bpm = BPM_MAX;
  bpmValue = (uint16_t)bpm;
  clockSourceMirror = (uint8_t)appSettings.clockSource;
}

static void fullRedraw(void) {
  ScreenData* screen = projectScreen();
  screenFullRedraw(screen);
}

// Live followed tempo while slaved to MIDI clock, rounded to whole BPM
// (app.cpp keeps project.tickRate updated from the router's measurement).
static int liveClockBpm(void) {
  return (int)(chipnomadState->project.tickRate * 2.5f + 0.5f);
}

// Scratch serial for status queries that don't need change tracking.
static uint32_t statusSerialScratch = 0;

// Current Link status, cached per draw pass (queried once per row render).
static LinkStatus currentLinkStatus(void) {
  return linkClientGetStatus(chipnomadState->linkClient, &statusSerialScratch);
}

// Alternating "connected" / "Peers: N" status on the Link row: the phase
// flips roughly once a second while connected, and a status transition
// resets it so a fresh connection always shows "connected" first.
static LinkStatus linkStatusPhaseStatus = LinkStatus::disabled;
static std::chrono::steady_clock::time_point linkStatusPhaseFlip = {};
static int linkStatusShowPeers = 0;

static void updateLinkStatusPhase(void) {
  const LinkStatus status = currentLinkStatus();
  const auto now = std::chrono::steady_clock::now();
  if (status != linkStatusPhaseStatus) {
    linkStatusPhaseStatus = status;
    linkStatusPhaseFlip = now;
    linkStatusShowPeers = 0;
    return;
  }
  if (status == LinkStatus::connected &&
      (linkStatusPhaseFlip == std::chrono::steady_clock::time_point() ||
        std::chrono::duration<float>(now - linkStatusPhaseFlip).count() >= 1.0f)) {
    linkStatusPhaseFlip = now;
    linkStatusShowPeers = !linkStatusShowPeers;
  }
}

static void draw(void) {
  // Row 6 is asynchronous in both clock-slave modes: MIDI clock shows the
  // live measured tempo, Link shows the connect toggle and session status
  // (discovery threshold, peer joins/leaves, the 1 s alternation).
  // fullRedraw only runs on navigation/edit, so refresh the field here
  // every frame. gfxClearRect erases the cursor underline together with
  // the field and the framework does not redraw the cursor after draw(),
  // so restore it whenever it rests on this row (same pattern as the
  // song screen's live refresh).
  if (!clockSourceMirror) return;
  if (clockSourceMirror == 2) updateLinkStatusPhase();

  ScreenData* screen = projectScreen();
  const int focused = screen->cursorRow == 6;
  if (clockSourceMirror == 1) {
    // MIDI clock: live followed tempo in the info color, as before.
    const ColorScheme cs = appSettings.colorScheme;
    gfxSetFgColor(cs.textInfo);
    gfxClearRect(13, 11, 27, 1);
    gfxPrintf(13, 11, "%u BPM", liveClockBpm());
  } else {
    projectCommonDrawField(0, 6, focused ? CellState::focus : CellState::normal);
  }
  if (focused) screen->drawCursor(screen->cursorCol, 6);
}

///////////////////////////////////////////////////////////////////////////////
//
// Common part of the form
//

static void drawRowHeader(int row, CellState state) {}
static void drawColHeader(int col, CellState state) {}

int projectCommonColumnCount(int row) {
  if (row == 0) {
    return 5; // Load, save, new, export, manage
  } else if (row >= 1 && row <= 3) {
    return 24; // File, title, author
  } else if (row == 4) {
    return 1; // Linear pitch
  } else if (row == 5) {
    return 1; // Clock source
  } else if (row == 6) {
    return 1; // Tempo (BPM), or the Link connect toggle in Link mode
  }
  return 1; // Default value
}

void projectCommonDrawStatic(void) {
  const ColorScheme cs = appSettings.colorScheme;

  gfxSetFgColor(cs.textTitles);
  gfxPrint(0, 0, "PROJECT");

  gfxSetFgColor(cs.textDefault);
  gfxPrint(8, 0, appTitle);
  gfxPrintf(0, 1, "v%s (%s)", appVersion, appBuild);

  gfxPrint(0, 5, "File");
  gfxPrint(0, 6, "Title");
  gfxPrint(0, 7, "Author");

  gfxPrint(0, 9, "Linear pitch");
  gfxPrint(0, 10, "Clock source");
  // Row 6 doubles as the Link connect row; the label follows the mode
  // (a full redraw runs whenever the clock source changes).
  gfxPrint(0, 11, clockSourceMirror == 2 ? "Link" : "Tempo");
}

void projectCommonDrawCursor(int col, int row) {
  if (row == 0) {
    if (col == 0) {
      gfxCursor(7, 3, 4); // Load
    } else if (col == 1) {
      gfxCursor(12, 3, 4); // Save
    } else if (col == 2) {
      gfxCursor(17, 3, 3); // New
    } else if (col == 3) {
      gfxCursor(21, 3, 6); // Export
    } else if (col == 4) {
      gfxCursor(28, 3, 6); // Manage
    }
  } else if (row >= 1 && row <= 3) {
    // Text fields: file name, title, author
    gfxCursor(7 + col, 4 + row, 1);
  } else if (row == 4) {
    // Linear pitch
    gfxCursor(13, 9, 3);
  } else if (row == 5) {
    // Clock source ("Internal" / "Midi Clock" / "Ableton Link")
    gfxCursor(13, 10, 12);
  } else if (row == 6) {
    // Tempo ("500.0 BPM"), or in Link mode the connect toggle whose
    // underline follows the label width.
    if (clockSourceMirror == 2) {
      const LinkStatus status = currentLinkStatus();
      gfxCursor(13, 11, status == LinkStatus::connected || status == LinkStatus::connecting ? 12 : 9);
    } else {
      gfxCursor(13, 11, 8);
    }
  }
}

void projectCommonDrawField(int col, int row, CellState state) {
  gfxSetFgColor(state == CellState::focus ? appSettings.colorScheme.textValue : appSettings.colorScheme.textDefault);

  if (row == 0) {
    if (col == 0) {
      gfxPrint(7, 3, "Load");
    } else if (col == 1) {
      gfxPrint(12, 3, "Save");
    } else if (col == 2) {
      gfxPrint(17, 3, "New");
    } else if (col == 3) {
      gfxPrint(21, 3, "Export");
    } else if (col == 4) {
      gfxPrint(28, 3, "Manage");
    }
  } else if (row == 1) {
    // File name
    gfxClearRect(7, 5, FILENAME_LENGTH, 1);
    gfxPrintf(7, 5, "%s", appSettings.projectFilename);
  } else if (row == 2) {
    // Title
    gfxClearRect(7, 6, PROJECT_TITLE_LENGTH, 1);
    gfxPrintf(7, 6, "%s", chipnomadState->project.title);
  } else if (row == 3) {
    // Author
    gfxClearRect(7, 7, PROJECT_TITLE_LENGTH, 1);
    gfxPrintf(7, 7, "%s", chipnomadState->project.author);
  } else if (row == 4) {
    // Linear pitch
    gfxPrint(13, 9, chipnomadState->project.linearPitch ? "ON " : "OFF");
  } else if (row == 5) {
    // Clock source: Internal / Midi Clock / Ableton Link
    gfxClearRect(13, 10, 27, 1);
    gfxPrint(13, 10,
      clockSourceMirror == 2 ? "Ableton Link" :
      clockSourceMirror == 1 ? "Midi Clock"  :
      "Internal");
  } else if (row == 6) {
    gfxClearRect(13, 11, 27, 1);
    if (clockSourceMirror == 2) {
      // Link mode: the row is the session connect/disconnect toggle. The
      // session owns the tempo (app.cpp applies it to the project), so no
      // BPM is shown or edited here.
      const LinkStatus status = currentLinkStatus();
      const char* label = (status == LinkStatus::connected) ? "[Disconnect]" :
                          (status == LinkStatus::connecting) ? "[Connecting]" :
                          "[Connect]";
      gfxPrint(13, 11, label);
      // Status message on the right, in the info color. While connected,
      // alternate between "connected" and the peer count roughly once a
      // second (phase kept by draw()) so both the link state and the
      // session size stay visible without widening the row.
      gfxSetFgColor(appSettings.colorScheme.textInfo);
      if (status == LinkStatus::connected) {
        if (linkStatusShowPeers) {
          gfxPrintf(26, 11, "Peers: %d", linkClientGetPeerCount(chipnomadState->linkClient));
        } else {
          gfxPrint(26, 11, "connected");
        }
      } else {
        const char* statusStr =
          status == LinkStatus::noNetwork  ? "No net"  :
          status == LinkStatus::connecting ? "Wait..." :
          status == LinkStatus::noPeers    ? "No sess" :
          status == LinkStatus::error      ? "Err"     : "";
        if (statusStr[0]) gfxPrint(26, 11, statusStr);
      }
    } else if (clockSourceMirror == 1) {
      // Slaved to MIDI clock: read-only live tempo measured from the
      // incoming clock, shown in the info color (draw() refreshes it
      // every frame with the same color).
      gfxSetFgColor(appSettings.colorScheme.textInfo);
      gfxPrintf(13, 11, "%u BPM", liveClockBpm());
    } else {
      gfxPrintf(13, 11, "%u BPM", bpmValue);
    }
  }
}

int projectCommonOnEdit(int col, int row, enum CellEditAction action) {
  int handled = 0;

  if (row == 0) {
    // Load/Save/New/Export
    if (col == 0) {
      // Load project
      projectReturnScreen = &screenProject;
      if (projectModified) {
        confirmSetup("Lose unsaved changes?", doLoadProject, cancelConfirm);
        screenSetup(&screenConfirm, 0);
      } else {
        doLoadProject();
      }
    } else if (col == 1) {
      // Save project - check filename first
      if (strlen(appSettings.projectFilename) == 0) {
        screenMessage(MESSAGE_TIME, "Enter filename");
        handled = 1;
      } else {
        fileBrowserSetupFolderMode("SAVE PROJECT", appSettings.projectPath, appSettings.projectFilename, ".cct", onProjectSaved, onProjectCancelled);
        screenSetup(&screenFileBrowser, 0);
      }
    } else if (col == 2) {
      // New project
      projectReturnScreen = &screenProject;
      if (projectModified) {
        confirmSetup("Lose unsaved changes?", doNewProject, cancelConfirm);
        screenSetup(&screenConfirm, 0);
      } else {
        doNewProject();
      }
      handled = 1;
    } else if (col == 3) {
      // Export - go to export screen
      screenSetup(&screenExport, 0);
      handled = 0;
    } else if (col == 4) {
      // Manage - go to manage screen
      screenSetup(&screenManage, 0);
      handled = 0;
    }
  } else if (row == 1) {
    // File name
    int res = editCharacter(action, appSettings.projectFilename, col, FILENAME_LENGTH);
    if (res == 1) {
      isCharEdit = 1;
      editingString = appSettings.projectFilename;
      editingStringLength = FILENAME_LENGTH;
    } else if (res > 1) {
      handled = 1;
    }
  } else if (row == 2) {
    // Title
    int res = editCharacter(action, chipnomadState->project.title, col, PROJECT_TITLE_LENGTH);
    if (res == 1) {
      isCharEdit = 1;
      editingString = chipnomadState->project.title;
      editingStringLength = PROJECT_TITLE_LENGTH;
    } else if (res > 1) {
      handled = 1;
    }
  } else if (row == 3) {
    // Author
    int res = editCharacter(action, chipnomadState->project.author, col, PROJECT_TITLE_LENGTH);
    if (res == 1) {
      isCharEdit = 1;
      editingString = chipnomadState->project.author;
      editingStringLength = PROJECT_TITLE_LENGTH;
    } else if (res > 1) {
      handled = 1;
    }
  } else if (row == 4) {
    // Linear pitch (ON/OFF)
    handled = edit8noLast(action, &chipnomadState->project.linearPitch, 1, 0, 1);
    if (handled) {
      projectModified = 1;
      chipnomadQueuePlaybackStop(chipnomadState);
      reinitializePitchTable(&chipnomadState->project);
    }
  } else if (row == 5) {
    // Clock source: Internal / Midi Clock / Ableton Link. Persisted in
    // settings.txt as `clockSource: 0|1|2` (the `midiClockMode:` line is
    // also written as a backwards-compat alias for older binaries).
    // Applied to the router live; the audio thread picks it up through
    // the regular project refresh. The connect/disconnect toggle lives
    // on row 6 in Link mode.
    action = convertMultiAction(action);
    switch (action) {
      case CellEditAction::tap:
      case CellEditAction::doubleTap:
        clockSourceMirror = (clockSourceMirror + 1) % 3;
        handled = 1;
        break;
      case CellEditAction::clear:
        clockSourceMirror = 0;
        handled = 1;
        break;
      case CellEditAction::increase:
      case CellEditAction::increaseBig:
        clockSourceMirror = (clockSourceMirror + 1) % 3;
        handled = 1;
        break;
      case CellEditAction::decrease:
      case CellEditAction::decreaseBig:
        clockSourceMirror = (clockSourceMirror + 2) % 3;
        handled = 1;
        break;
      default:
        break;
    }
    if (handled) {
      appSettings.clockSource = (ClockSource)clockSourceMirror;
      midiRouterSetClockMode(chipnomadState->midiRouter,
        appSettings.clockSource == ClockSource::midiClock ? MidiClockMode::slave : MidiClockMode::off);
      // Link mode: selecting arms the client (status -> selected, no
      // network I/O); leaving Link mode disconnects. Actual Connect is
      // the toggle on row 6.
      if (appSettings.clockSource == ClockSource::link) {
        linkClientSelect(chipnomadState->linkClient);
      } else {
        linkClientDeselect(chipnomadState->linkClient);
      }
      // Row 6 switches between the tempo field and the Link toggle (and
      // the row label changes with it), so redraw the whole form.
      // Re-sync the BPM mirror from the project's tick rate first: while
      // slaved (MIDI clock or Link) app.cpp drives tickRate, and leaving
      // a slaved mode should display the tempo actually in effect.
      int bpm = (int)(chipnomadState->project.tickRate * 2.5f + 0.5f);
      if (bpm < BPM_MIN) bpm = BPM_MIN;
      if (bpm > BPM_MAX) bpm = BPM_MAX;
      bpmValue = (uint16_t)bpm;
      fullRedraw();
    }
  } else if (row == 6) {
    if (clockSourceMirror == 2) {
      // Link mode: the row is the connect/disconnect toggle (the session
      // owns the tempo, so there is nothing to edit here). Tap connects
      // (from selected/noNetwork/noPeers/error) or disconnects (from
      // connected); taps during connecting are swallowed so the pending
      // discovery isn't torn down mid-flight.
      action = convertMultiAction(action);
      if (action == CellEditAction::tap || action == CellEditAction::doubleTap) {
        const LinkStatus status = currentLinkStatus();
        if (status == LinkStatus::connected) {
          linkClientDisconnect(chipnomadState->linkClient);
          handled = 1;
        } else if (status == LinkStatus::selected || status == LinkStatus::noNetwork ||
                   status == LinkStatus::noPeers || status == LinkStatus::error) {
          linkClientConnect(chipnomadState->linkClient);
          handled = 1;
        } else if (status == LinkStatus::connecting) {
          handled = 1; // swallow: connection attempt already in flight
        }
      } else if (action == CellEditAction::clear) {
        // Clear = disconnect if connected, otherwise no-op.
        if (currentLinkStatus() == LinkStatus::connected) {
          linkClientDisconnect(chipnomadState->linkClient);
        }
        handled = 1;
      }
    } else if (clockSourceMirror == 1) {
      // Slaved to MIDI clock: the tempo is driven by the incoming clock,
      // so edits are swallowed (the field shows the live followed tempo).
      handled = 1;
    } else {
      // Tempo in whole BPM. Fine steps are 1 BPM, coarse 5 BPM; clear
      // restores the old default 50 Hz = 125 BPM.
      action = convertMultiAction(action);
      switch (action) {
        case CellEditAction::tap:
        case CellEditAction::doubleTap:
          bpmValue += 1;
          if (bpmValue > BPM_MAX) bpmValue = BPM_MIN;
          handled = 1;
          break;
        case CellEditAction::clear:
          bpmValue = BPM_DEFAULT;
          handled = 1;
          break;
        case CellEditAction::increase:
          if (bpmValue < BPM_MAX) bpmValue += 1;
          handled = 1;
          break;
        case CellEditAction::decrease:
          if (bpmValue > BPM_MIN) bpmValue -= 1;
          handled = 1;
          break;
        case CellEditAction::increaseBig:
          bpmValue = bpmValue > BPM_MAX - 5 ? BPM_MAX : bpmValue + 5;
          handled = 1;
          break;
        case CellEditAction::decreaseBig:
          bpmValue = bpmValue < BPM_MIN + 5 ? BPM_MIN : bpmValue - 5;
          handled = 1;
          break;
        default:
          break;
      }
      if (handled) {
        // Backwards compatible writeback: the project file keeps storing
        // Hz ("- Frame rate"), BPM * 0.4 in Hz.
        chipnomadState->project.tickRate = (float)bpmValue * 0.4f;
        projectModified = 1;
      }
    }
  }

  return handled;
}


///////////////////////////////////////////////////////////////////////////////
//
// Input handling
//

static int inputScreenNavigation(int keys, int tapCount) {
  if (keys == keyOpt || keys == (keyDown | keyShift)) {
    screenSetup(&screenSong, 0);
    return 1;
  }
  return 0;
}

static int onInput(int isKeyDown, int keys, int tapCount) {
  if (isCharEdit) {
    ScreenData* screen = projectScreen();
    char result = charEditInput(keys, tapCount, editingString, screen->cursorCol, editingStringLength);

    if (result) {
      projectModified = 1;
      isCharEdit = 0;
      if (screen->cursorCol < editingStringLength - 1) screen->cursorCol++;
      editingString = NULL;
      editingStringLength = 0;
      fullRedraw();
    }
  } else {
    if (inputScreenNavigation(keys, tapCount)) return 1;

    ScreenData* screen = projectScreen();
    if (screenInput(screen, isKeyDown, keys, tapCount)) return 1;
  }
  return 0;
}

#ifdef DESKTOP_BUILD

// Key jazz text entry (see keyJazzTextHandleRawKey): filename, title, author.
int projectKeyJazzTextField(KeyJazzTextField* field) {
  ScreenData* screen = projectScreen();
  field->screen = screen;
  field->row = screen->cursorRow;
  field->popupOpen = isCharEdit;
  field->marksProjectModified = 1;
  field->str = NULL;
  if (screen->cursorRow == 1) { field->str = appSettings.projectFilename; field->maxLen = FILENAME_LENGTH; }
  else if (screen->cursorRow == 2) { field->str = chipnomadState->project.title; field->maxLen = PROJECT_TITLE_LENGTH; }
  else if (screen->cursorRow == 3) { field->str = chipnomadState->project.author; field->maxLen = PROJECT_TITLE_LENGTH; }
  return 1;
}

#endif // DESKTOP_BUILD

static ScreenPlaybackLevel getPlaybackLevel(void) {
  return ScreenPlaybackLevel::song;
}

const AppScreen screenProject = {
  .init = NULL,
  .setup = setup,
  .fullRedraw = fullRedraw,
  .draw = draw,
  .onInput = onInput,
  .getPlaybackLevel = getPlaybackLevel,
};
