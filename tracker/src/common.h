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
#define AUDIO_DEVICE_NAME_LENGTH (127)
#define AUDIO_INPUT_MAX_DEVICES (16)
#define AUDIO_INPUT_MAX_ARMED (8)

struct ColorScheme {
  int background;
  int textEmpty;
  int textInfo;
  int textDefault;
  int textLabel;    // names of settings, a little dimmer than their values
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
enum class TrackVisualMode : uint8_t { detailed, audio };
struct TrackVisualSettings { TrackVisualMode mode = TrackVisualMode::detailed; };

// Clock source for engine tempo. Generalized from the old 0/1
// midiClockMode int to an enum: internal (project.tickRate),
// midiClock (follow incoming MIDI Clock 0xF8 + transport), or link
// (Ableton Link session). On file load, an old `midiClockMode:`
// line in settings.txt is read as a backwards-compat alias: 0 ->
// internal, 1 -> midiClock. New settings write `clockSource: 0|1|2`.
enum class ClockSource : int {
  internal = 0,
  midiClock = 1,
  link = 2,
};

// Audio input recording source. -1 = none; 0..7 = bounce the captured
// signal into that track's chain input; 100..115 = external MIDI instrument
// (index = value - 100), mirroring the MIDI channel-instrument convention.
enum class AudioInputSource : int {
  none = -1,
  track0 = 0,
  track1,
  track2,
  track3,
  track4,
  track5,
  track6,
  track7,
  externalBase = 100
};

inline bool audioInputIsTrackSource(int source) {
  return source >= 0 && source < PROJECT_MAX_TRACKS;
}
inline bool audioInputIsExternalSource(int source) {
  return source >= (int)AudioInputSource::externalBase &&
         source < (int)AudioInputSource::externalBase + MIDI_CHANNEL_COUNT;
}
inline int audioInputExternalIndex(int source) {
  return source - (int)AudioInputSource::externalBase;
}

struct AppSettings {
  int screenWidth;
  int screenHeight;
  int audioSampleRate;
  int audioBufferSize;
  char audioOutputDeviceName[AUDIO_DEVICE_NAME_LENGTH + 1];
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
  uint8_t playbackCounterMode; // 0 = bars, 1 = elapsed time
  uint8_t playbackBarRows;
  uint8_t playbackBarTicks;
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
  // Saved to settings.txt as `clockSource: 0|1|2` (internal / midiClock
  // / link). Applied to the router in appSetup() and changed live from
  // the Project screen. Old `midiClockMode:` lines in settings.txt are
  // read as a backwards-compat alias (0 -> internal, 1 -> midiClock).
  ClockSource clockSource;
  uint8_t midiSendClock;
  uint8_t midiSendTransport;
  uint8_t linkTransport;
  uint8_t midiOutputDelayMs;
  // Saved to settings.txt: which instrument a MIDI-in note on a given
  // channel (0-15) plays during preview, e.g. channel 0 -> instrument 5.
  // -1 = channel not assigned (falls back to the currently selected
  // instrument, the pre-existing behavior).
  int8_t midiChannelInstrument[MIDI_CHANNEL_COUNT];
  StickLiveMode stickLiveMode;
  TrackVisualSettings trackVisuals[PROJECT_MAX_TRACKS];
  uint8_t persistentWaveform;
  uint8_t hideTouchPadWithGamepad; // touch builds: hide the virtual pad while a gamepad is connected
  uint8_t touchHaptics; // Android touch controls: request the system haptic feedback
  uint8_t bootScreenNight; // 0 = original art; 1 = mellow night title
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
  // Audio input recording (see docs/audio-input-recording-implementation-plan.md).
  // Device names are persisted; indices are runtime-only (enumeration order
  // isn't stable across reboots), resolved at launch like MIDI devices.
  char audioInputDeviceNames[AUDIO_INPUT_MAX_DEVICES][AUDIO_DEVICE_NAME_LENGTH + 1];
  int audioInputDeviceCount;
  // Armed capture devices as indices into audioInputDeviceNames, -1 = empty.
  int8_t audioInputArmed[AUDIO_INPUT_MAX_ARMED];
  int audioInputMonitor; // 1 = route capture to output at -6 dB
  char recordingPath[PATH_LENGTH + 1]; // Last recording output folder
};

extern AppSettings appSettings;
extern int* pSongRow;
extern int* pSongTrack;
extern int* pChainRow;

extern ChipNomadState* chipnomadState;

extern int projectModified; // Flag to track if the project has unsaved changes

// Set while the sample settings screen is playing a full sample back in
// LAZY mode (tap PLAY toggles it). Guards the app-level auto-stop so the
// preview survives the PLAY key release, and arms EDIT to drop slices at
// the playback marker.
extern int sampleLazyPlaybackActive;

// Set at startup when an autosave file exists but failed to load, so the
// title screen can warn instead of silently offering to "continue" into the
// demo project it fell back to.
extern int autosaveLoadFailed;
// After a failed autosave load: 1 = the unreadable file was kept beside the
// autosave, -1 = it could not be moved, so autosaving is paused this session.
extern int autosaveKept;

// Settings functions
void initDefaultAppSettings(void);
int settingsSave(void);
int settingsLoad(void);
int saveTheme(const char* path);
int loadTheme(const char* path);
void resetToDefaultColors(void);
// Text Default dimmed towards the background: the label colour for themes
// that don't set one.
int defaultLabelColor(const ColorScheme& scheme);
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
