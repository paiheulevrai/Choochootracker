#ifndef __COMMON_H__
#define __COMMON_H__

#include "chipnomad_lib.h"
#include "corelib/corelib_input.h"
#include "corelib_mainloop.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define AUTOSAVE_FILENAME "autosave.cct"
#define FILENAME_LENGTH (24)
#define PATH_LENGTH (4096)
#define THEME_NAME_LENGTH (16)
#define MIDI_CHANNEL_COUNT (16)
#define MIDI_DEVICE_NAME_LENGTH (127)

struct ColorScheme {
  int background;
  int textEmpty;
  int textInfo;
  int textDefault;
  int textValue;
  int textTitles;
  int playMarkers;
  int cursor;
  int selection;
  int warning;
};

// Key mapping: 10 buttons × 3 keys each
struct KeyMapping {
  InputCode keyUp[3];
  InputCode keyDown[3];
  InputCode keyLeft[3];
  InputCode keyRight[3];
  InputCode keyEdit[3];
  InputCode keyOpt[3];
  InputCode keyPlay[3];
  InputCode keyShift[3];
  InputCode keyMotionLive[3];
  InputCode keyMotionRecord[3];
  InputCode keyMotionErase[3];
};

enum class StickLiveMode { hold, toggle, free };

struct AppSettings {
  int screenWidth;
  int screenHeight;
  int audioSampleRate;
  int audioBufferSize;
  int aySampleDithering;
  int doubleTapFrames;
  int keyRepeatDelay;
  int keyRepeatSpeed;
  float mixVolume;
  int quality;
  int braidsBits;
  int braidsDrift;
  int braidsSignature;
  uint32_t braidsSignatureSeed;
  int pitchConflictWarning;
  int quickHelpReleaseSeen;
  int ayWavetableLfoView;
  int waveformRefreshHz;
  // Port indices are runtime-only (not saved): enumeration order isn't
  // stable across reboots/replugging. -1 = off. What IS saved is each
  // device's name (below); appSetup() resolves it back to a live index on
  // launch, or leaves the device off (with the name kept) if not found -
  // see midiDeviceLabel() in screen_midi.cpp for the "not found" status
  // that produces instead of silently picking a different port.
  int midiInputDevice;
  int midiOutputDevice;
  char midiInputDeviceName[MIDI_DEVICE_NAME_LENGTH + 1];
  char midiOutputDeviceName[MIDI_DEVICE_NAME_LENGTH + 1];
  // Saved to settings.txt: which instrument a MIDI-in note on a given
  // channel (0-15) plays during preview, e.g. channel 0 -> instrument 5.
  // -1 = channel not assigned (falls back to the currently selected
  // instrument, the pre-existing behavior).
  int8_t midiChannelInstrument[MIDI_CHANNEL_COUNT];
  StickLiveMode stickLiveMode;
  KeyMapping keyMapping;
  ColorScheme colorScheme;
  char themeName[THEME_NAME_LENGTH + 1];
  char projectFilename[FILENAME_LENGTH + 1];
  char projectPath[PATH_LENGTH + 1];
  char pitchTablePath[PATH_LENGTH + 1];
  char instrumentPath[PATH_LENGTH + 1];
  char themePath[PATH_LENGTH + 1];
  char fontPath[PATH_LENGTH + 1];
  char fontFolderPath[PATH_LENGTH + 1];
  char samplePath[PATH_LENGTH + 1];
  char exportPath[PATH_LENGTH + 1]; // Custom export folder; empty = default
  char exportLastFolder[FILENAME_LENGTH + 1]; // Last default-scheme export folder (for rename on save)
  char ayWavetablePath[PATH_LENGTH + 1];
  char scwfPath[PATH_LENGTH + 1];
  char srWavetablePath[PATH_LENGTH + 1];
};

extern AppSettings appSettings;
extern int* pSongRow;
extern int* pSongTrack;
extern int* pChainRow;

extern ChipNomadState* chipnomadState;

extern int projectModified; // Flag to track if the project has unsaved changes

// Set at startup when an autosave file exists but failed to load, so the
// title screen can warn instead of silently offering to "continue" into the
// demo project it fell back to.
extern int autosaveLoadFailed;

// Settings functions
void initDefaultAppSettings(void);
int settingsSave(void);
int settingsLoad(void);
int saveTheme(const char* path);
int loadTheme(const char* path);
void resetToDefaultColors(void);
void initDefaultKeyMapping(void);
void resetKeyMappingToDefaults(void);

// Utility functions
void extractFilenameWithoutExtension(const char* path, char* output, int maxLength);
void updatePathFromFile(char* destination, const char* path);
const char* getAutosavePath(void);
void clearNotePreview(void);

#ifdef __cplusplus
}
#endif

#endif
