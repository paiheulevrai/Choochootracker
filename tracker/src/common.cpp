#include "common.h"
#include "corelib/corelib_file.h"
#include "corelib/corelib_gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef WEB_BUILD
#include <emscripten.h>
#endif
#include <chipnomad_lib.h>

static char settingsPath[PATH_LENGTH + 32];
static char autosavePath[PATH_LENGTH + 32];
static char lineBuffer[1024];
static const int kAudioSampleRate = 48000;

static int defaultAudioBufferSize(void) {
#ifdef WEB_BUILD
  return 1024;
#elif defined(PORTMASTER_BUILD)
  return 512;
#elif defined(ANDROID_BUILD) || defined(DESKTOP_BUILD)
  return 256;
#else
  return 512;
#endif
}

static int normalizeAudioBufferSize(int size) {
  static const int sizes[] = {32, 64, 128, 256, 512, 1024};
  int closest = sizes[0];
  long long closestDistance = llabs((long long)closest - size);
  for (int candidate : sizes) {
    const long long distance = llabs((long long)candidate - size);
    if (distance < closestDistance) {
      closest = candidate;
      closestDistance = distance;
    }
  }
  return closest;
}

AppSettings appSettings;

void initDefaultAppSettings(void) {
  appSettings.screenWidth = 0; // 0 to auto-detect resolution
  appSettings.screenHeight = 0;
  appSettings.audioSampleRate = kAudioSampleRate;
  appSettings.audioBufferSize = defaultAudioBufferSize();
  appSettings.audioOutputDeviceName[0] = 0;
  appSettings.aySampleDithering = 1; // Default: ON
  appSettings.doubleTapFrames = 20;
  appSettings.keyRepeatDelay = 16;
#ifdef PORTMASTER_BUILD
  // One navigation step per 60 Hz UI frame feels natural on a physical D-pad.
  appSettings.keyRepeatSpeed = 1;
#else
  appSettings.keyRepeatSpeed = 2;
#endif
  appSettings.mixVolume = 1.0f;
  appSettings.quality = (int)ChipNomadQuality::medium;
  appSettings.braidsBits = 6;
  appSettings.braidsDrift = 0;
  appSettings.braidsSignature = 0;
  appSettings.braidsSignatureSeed = (uint32_t)time(NULL) ^
    (uint32_t)(uintptr_t)&appSettings;
  if (!appSettings.braidsSignatureSeed) appSettings.braidsSignatureSeed = 1;
  appSettings.pitchConflictWarning = 0;
  appSettings.quickHelpReleaseSeen = 0;
  appSettings.ayWavetableLfoView = 0;
#ifdef PORTMASTER_BUILD
  appSettings.waveformRefreshHz = 60;
#else
  appSettings.waveformRefreshHz = 30;
#endif
  appSettings.playbackCounterMode = 0;
  appSettings.playbackBarRows = 16;
  appSettings.playbackBarTicks = 6;
  appSettings.stickLiveMode = StickLiveMode::hold;
  for (auto& visual : appSettings.trackVisuals)
    visual.mode = TrackVisualMode::detailed;
  appSettings.midiInputDevice = -1;
  appSettings.midiOutputDevice = -1;
  appSettings.midiInputDeviceName[0] = '\0';
  appSettings.midiOutputDeviceName[0] = '\0';
  appSettings.clockSource = ClockSource::internal;
  appSettings.midiSendClock = 0;
  appSettings.midiSendTransport = 0;
  appSettings.linkTransport = 1; // Preserve transport following from older settings.
  appSettings.midiOutputDelayMs = 0;
  for (int i = 0; i < MIDI_CHANNEL_COUNT; i++) appSettings.midiChannelInstrument[i] = -1;
  appSettings.persistentWaveform = 0;
#ifdef ANDROID_BUILD
  appSettings.hideTouchPadWithGamepad = 0; // phones have no built-in pad: keep touch controls
  appSettings.touchHaptics = 1;
#else
  appSettings.hideTouchPadWithGamepad = 1; // handhelds: their built-in pad replaces it
  appSettings.touchHaptics = 0;
#endif
  appSettings.bootScreenNight = 0;

  // Audio input recording defaults: nothing enumerated, nothing armed,
  // monitoring off, no recording folder override.
  appSettings.audioInputDeviceCount = 0;
  memset(appSettings.audioInputDeviceNames, 0, sizeof(appSettings.audioInputDeviceNames));
  for (int i = 0; i < AUDIO_INPUT_MAX_ARMED; i++) appSettings.audioInputArmed[i] = -1;
  appSettings.audioInputMonitor = 0;
  appSettings.recordingPath[0] = '\0';

  // Zero out key mapping (platform-specific defaults applied later)
  memset(&appSettings.keyMapping, 0, sizeof(KeyMapping));

  // Color scheme defaults
  appSettings.colorScheme.background = 0x050c1f;
  appSettings.colorScheme.textEmpty = 0x10284a;
  appSettings.colorScheme.textInfo = 0x52799b;
  appSettings.colorScheme.textDefault = 0xc9bfa5;
  appSettings.colorScheme.textLabel = 0x7d8fa6;
  appSettings.colorScheme.textNames = 0x9fcf8f;
  appSettings.colorScheme.valueBar = 0x64d6ff;
  appSettings.colorScheme.textValue = 0xffd665;
  appSettings.colorScheme.textTitles = 0xe6b84f;
  appSettings.colorScheme.playMarkers = 0x64d6ff;
  appSettings.colorScheme.cursor = 0x65d8ff;
  appSettings.colorScheme.selection = 0x245072;
  appSettings.colorScheme.warning = 0xff6655;

  // String defaults
  strncpy(appSettings.themeName, "Choo", THEME_NAME_LENGTH);
  appSettings.themeName[THEME_NAME_LENGTH] = '\0';
  appSettings.projectFilename[0] = '\0';
#ifdef WEB_BUILD
  strncpy(appSettings.projectPath, "/user/projects", PATH_LENGTH);
  strncpy(appSettings.samplePath, "/user/samples", PATH_LENGTH);
  strncpy(appSettings.ayWavetablePath, "/AY_wavetables", PATH_LENGTH);
  strncpy(appSettings.scwfPath, "/waveforms", PATH_LENGTH);
  strncpy(appSettings.srWavetablePath, "/SR_wavetables", PATH_LENGTH);
#elif defined(ANDROID_BUILD)
  char workspace[PATH_LENGTH];
  if (fileGetDefaultDirectory(workspace, sizeof(workspace)) != 0) strncpy(workspace, ".", sizeof(workspace));
  snprintf(appSettings.projectPath, sizeof(appSettings.projectPath), "%s/projects", workspace);
  snprintf(appSettings.samplePath, sizeof(appSettings.samplePath), "%s/samples", workspace);
  snprintf(appSettings.ayWavetablePath, sizeof(appSettings.ayWavetablePath), "%s/AY_wavetables", workspace);
  snprintf(appSettings.scwfPath, sizeof(appSettings.scwfPath), "%s/waveforms", workspace);
  snprintf(appSettings.srWavetablePath, sizeof(appSettings.srWavetablePath), "%s/SR_wavetables", workspace);
#else
  // Normally these stay relative: the tar.gz/Windows releases' launcher
  // scripts cd into the install directory first, so "projects" etc.
  // resolve next to the bundled packaging/common/ content. An AppImage
  // mounts read-only, so anchor to the writable per-user directory instead
  // (see fileGetDefaultDirectory()'s own APPIMAGE handling) - AppRun seeds
  // that directory with the same bundled content once on first launch.
  if (fileIsRunningFromAppImage()) {
    char workspace[PATH_LENGTH];
    if (fileGetDefaultDirectory(workspace, sizeof(workspace)) != 0) strncpy(workspace, ".", sizeof(workspace));
    snprintf(appSettings.projectPath, sizeof(appSettings.projectPath), "%s/projects", workspace);
    snprintf(appSettings.samplePath, sizeof(appSettings.samplePath), "%s/samples", workspace);
    snprintf(appSettings.ayWavetablePath, sizeof(appSettings.ayWavetablePath), "%s/AY_wavetables", workspace);
    snprintf(appSettings.scwfPath, sizeof(appSettings.scwfPath), "%s/waveforms", workspace);
    snprintf(appSettings.srWavetablePath, sizeof(appSettings.srWavetablePath), "%s/SR_wavetables", workspace);
  } else {
    strncpy(appSettings.projectPath, "projects", PATH_LENGTH);
    strncpy(appSettings.samplePath, "samples", PATH_LENGTH);
    strncpy(appSettings.ayWavetablePath, "AY_wavetables", PATH_LENGTH);
    strncpy(appSettings.scwfPath, "waveforms", PATH_LENGTH);
    strncpy(appSettings.srWavetablePath, "SR_wavetables", PATH_LENGTH);
  }
#endif
  appSettings.projectPath[PATH_LENGTH] = '\0';
  appSettings.samplePath[PATH_LENGTH] = '\0';
  appSettings.exportPath[0] = '\0'; // Empty = use the default export location
  appSettings.exportLastFolder[0] = '\0';
  appSettings.ayWavetablePath[PATH_LENGTH] = '\0';
  appSettings.scwfPath[PATH_LENGTH] = '\0';
  appSettings.srWavetablePath[PATH_LENGTH] = '\0';
#ifdef ANDROID_BUILD
  snprintf(appSettings.pitchTablePath, sizeof(appSettings.pitchTablePath), "%s/pitch-tables", workspace);
  snprintf(appSettings.instrumentPath, sizeof(appSettings.instrumentPath), "%s/instruments", workspace);
  snprintf(appSettings.themePath, sizeof(appSettings.themePath), "%s/themes", workspace);
  snprintf(appSettings.fontFolderPath, sizeof(appSettings.fontFolderPath), "%s/fonts", workspace);
#else
  if (fileIsRunningFromAppImage()) {
    char workspace[PATH_LENGTH];
    if (fileGetDefaultDirectory(workspace, sizeof(workspace)) != 0) strncpy(workspace, ".", sizeof(workspace));
    snprintf(appSettings.pitchTablePath, sizeof(appSettings.pitchTablePath), "%s/pitch-tables", workspace);
    snprintf(appSettings.instrumentPath, sizeof(appSettings.instrumentPath), "%s/instruments", workspace);
    snprintf(appSettings.themePath, sizeof(appSettings.themePath), "%s/themes", workspace);
    snprintf(appSettings.fontFolderPath, sizeof(appSettings.fontFolderPath), "%s/fonts", workspace);
  } else {
    strncpy(appSettings.pitchTablePath, "pitch-tables", PATH_LENGTH);
    strncpy(appSettings.instrumentPath, "instruments", PATH_LENGTH);
    strncpy(appSettings.themePath, "themes", PATH_LENGTH);
    strncpy(appSettings.fontFolderPath, "fonts", PATH_LENGTH);
  }
#endif
  appSettings.pitchTablePath[PATH_LENGTH] = '\0';
  appSettings.instrumentPath[PATH_LENGTH] = '\0';
  appSettings.themePath[PATH_LENGTH] = '\0';
  appSettings.fontFolderPath[PATH_LENGTH] = '\0';
  appSettings.fontPath[0] = '\0';
}

int* pSongRow;
int* pSongTrack;
int* pChainRow;
ChipNomadState* chipnomadState;
int projectModified = 0;
int sampleLazyPlaybackActive = 0;
int autosaveLoadFailed = 0;
int autosaveKept = 0;

int settingsSave(void) {
#ifdef WEB_BUILD
  snprintf(settingsPath, sizeof(settingsPath), "/user/settings.txt");
#else
  char defaultDir[PATH_LENGTH];
  if (fileGetDefaultDirectory(defaultDir, PATH_LENGTH) != 0) return 1;
  snprintf(settingsPath, sizeof(settingsPath), "%s%ssettings.txt", defaultDir, PATH_SEPARATOR_STR);
#endif

  FILE* file = fopen(settingsPath, "w");
  if (file == NULL) return 1;

  fprintf(file, "screenWidth: %d\n", appSettings.screenWidth);
  fprintf(file, "screenHeight: %d\n", appSettings.screenHeight);
  fprintf(file, "audioSampleRate: %d\n", appSettings.audioSampleRate);
  fprintf(file, "audioBufferSize: %d\n", appSettings.audioBufferSize);
  fprintf(file, "audioOutputDeviceName: %s\n", appSettings.audioOutputDeviceName);
  fprintf(file, "aySampleDithering: %d\n", appSettings.aySampleDithering);
  fprintf(file, "doubleTapFrames: %d\n", appSettings.doubleTapFrames);
  fprintf(file, "keyRepeatDelay: %d\n", appSettings.keyRepeatDelay);
  fprintf(file, "keyRepeatSpeed: %d\n", appSettings.keyRepeatSpeed);
  fprintf(file, "mixVolume: %f\n", appSettings.mixVolume);
  fprintf(file, "quality: %d\n", appSettings.quality);
  fprintf(file, "braidsBits: %d\n", appSettings.braidsBits);
  fprintf(file, "braidsDrift: %d\n", appSettings.braidsDrift);
  fprintf(file, "braidsSignature: %d\n", appSettings.braidsSignature);
  fprintf(file, "braidsSignatureSeed: %u\n", appSettings.braidsSignatureSeed);
  fprintf(file, "pitchConflictWarning: %d\n", appSettings.pitchConflictWarning);
  fprintf(file, "quickHelpReleaseSeen: %d\n", appSettings.quickHelpReleaseSeen);
  fprintf(file, "ayWavetableLfoView: %d\n", appSettings.ayWavetableLfoView);
  fprintf(file, "waveformRefreshHz: %d\n", appSettings.waveformRefreshHz);
  fprintf(file, "playbackCounterMode: %d\n", appSettings.playbackCounterMode);
  fprintf(file, "playbackBarRows: %d\n", appSettings.playbackBarRows);
  fprintf(file, "playbackBarTicks: %d\n", appSettings.playbackBarTicks);

  const char* stickLiveMode = appSettings.stickLiveMode == StickLiveMode::free ? "FREE" :
    appSettings.stickLiveMode == StickLiveMode::toggle ? "TOGGLE" : "HOLD";
  fprintf(file, "stickLiveMode: %s\n", stickLiveMode);
  fprintf(file, "persistentWaveform: %d\n", appSettings.persistentWaveform);
  fprintf(file, "hideTouchPadWithGamepad: %d\n", appSettings.hideTouchPadWithGamepad);
  fprintf(file, "touchHaptics: %d\n", appSettings.touchHaptics);
  fprintf(file, "bootScreenNight: %d\n", appSettings.bootScreenNight);
  for (int track = 0; track < PROJECT_MAX_TRACKS; ++track)
    fprintf(file, "trackVisuals%d: %d\n", track + 1, (int)appSettings.trackVisuals[track].mode);

  fprintf(file, "midiChannelInstrument: ");
  for (int i = 0; i < MIDI_CHANNEL_COUNT; i++) {
    fprintf(file, "%d%s", appSettings.midiChannelInstrument[i], i < MIDI_CHANNEL_COUNT - 1 ? "," : "\n");
  }
  fprintf(file, "midiInputDeviceName: %s\n", appSettings.midiInputDeviceName);
  fprintf(file, "midiOutputDeviceName: %s\n", appSettings.midiOutputDeviceName);
  // Write the canonical `clockSource:` line for v3+; also write the
  // legacy `midiClockMode:` alias (0/1 only, never 2) so older binaries
  // that read the old field still load the Internal/Midi-Clock choice.
  fprintf(file, "midiSendClock: %d\n", appSettings.midiSendClock);
  fprintf(file, "midiSendTransport: %d\n", appSettings.midiSendTransport);
  fprintf(file, "linkTransport: %d\n", appSettings.linkTransport);
  fprintf(file, "midiOutputDelayMs: %d\n", appSettings.midiOutputDelayMs);
  fprintf(file, "clockSource: %d\n", (int)appSettings.clockSource);
  fprintf(file, "midiClockMode: %d\n",
    appSettings.clockSource == ClockSource::midiClock ? 1 : 0);

  // Audio input recording: persist the device-name list and the armed slots
  // BY NAME (indices into the live enumeration aren't stable across
  // reboots/replugging - same policy as MIDI devices).
  fprintf(file, "audioInputDeviceCount: %d\n", appSettings.audioInputDeviceCount);
  for (int i = 0; i < appSettings.audioInputDeviceCount && i < AUDIO_INPUT_MAX_DEVICES; i++)
    fprintf(file, "audioInputDeviceName.%d: %s\n", i, appSettings.audioInputDeviceNames[i]);
  for (int i = 0; i < AUDIO_INPUT_MAX_ARMED; i++) {
    if (appSettings.audioInputArmed[i] >= 0 && appSettings.audioInputArmed[i] < appSettings.audioInputDeviceCount)
      fprintf(file, "audioInputArmed.%d: %s\n", i, appSettings.audioInputDeviceNames[appSettings.audioInputArmed[i]]);
  }
  fprintf(file, "audioInputMonitor: %d\n", appSettings.audioInputMonitor);
  fprintf(file, "recordingPath: %s\n", appSettings.recordingPath);

  // Save key mapping codes
  fprintf(file, "keyUp: %d,%d,%d\n", appSettings.keyMapping.keyUp[0].code, appSettings.keyMapping.keyUp[1].code, appSettings.keyMapping.keyUp[2].code);
  fprintf(file, "keyDown: %d,%d,%d\n", appSettings.keyMapping.keyDown[0].code, appSettings.keyMapping.keyDown[1].code, appSettings.keyMapping.keyDown[2].code);
  fprintf(file, "keyLeft: %d,%d,%d\n", appSettings.keyMapping.keyLeft[0].code, appSettings.keyMapping.keyLeft[1].code, appSettings.keyMapping.keyLeft[2].code);
  fprintf(file, "keyRight: %d,%d,%d\n", appSettings.keyMapping.keyRight[0].code, appSettings.keyMapping.keyRight[1].code, appSettings.keyMapping.keyRight[2].code);
  fprintf(file, "keyEdit: %d,%d,%d\n", appSettings.keyMapping.keyEdit[0].code, appSettings.keyMapping.keyEdit[1].code, appSettings.keyMapping.keyEdit[2].code);
  fprintf(file, "keyOpt: %d,%d,%d\n", appSettings.keyMapping.keyOpt[0].code, appSettings.keyMapping.keyOpt[1].code, appSettings.keyMapping.keyOpt[2].code);
  fprintf(file, "keyPlay: %d,%d,%d\n", appSettings.keyMapping.keyPlay[0].code, appSettings.keyMapping.keyPlay[1].code, appSettings.keyMapping.keyPlay[2].code);
  fprintf(file, "keyShift: %d,%d,%d\n", appSettings.keyMapping.keyShift[0].code, appSettings.keyMapping.keyShift[1].code, appSettings.keyMapping.keyShift[2].code);
  fprintf(file, "keyMotionLive: %d,%d,%d\n", appSettings.keyMapping.keyMotionLive[0].code, appSettings.keyMapping.keyMotionLive[1].code, appSettings.keyMapping.keyMotionLive[2].code);
  fprintf(file, "keyMotionRecord: %d,%d,%d\n", appSettings.keyMapping.keyMotionRecord[0].code, appSettings.keyMapping.keyMotionRecord[1].code, appSettings.keyMapping.keyMotionRecord[2].code);
  fprintf(file, "keyMotionErase: %d,%d,%d\n", appSettings.keyMapping.keyMotionErase[0].code, appSettings.keyMapping.keyMotionErase[1].code, appSettings.keyMapping.keyMotionErase[2].code);

  // Save key mapping device types
  fprintf(file, "keyUpType: %d,%d,%d\n", appSettings.keyMapping.keyUp[0].deviceType, appSettings.keyMapping.keyUp[1].deviceType, appSettings.keyMapping.keyUp[2].deviceType);
  fprintf(file, "keyDownType: %d,%d,%d\n", appSettings.keyMapping.keyDown[0].deviceType, appSettings.keyMapping.keyDown[1].deviceType, appSettings.keyMapping.keyDown[2].deviceType);
  fprintf(file, "keyLeftType: %d,%d,%d\n", appSettings.keyMapping.keyLeft[0].deviceType, appSettings.keyMapping.keyLeft[1].deviceType, appSettings.keyMapping.keyLeft[2].deviceType);
  fprintf(file, "keyRightType: %d,%d,%d\n", appSettings.keyMapping.keyRight[0].deviceType, appSettings.keyMapping.keyRight[1].deviceType, appSettings.keyMapping.keyRight[2].deviceType);
  fprintf(file, "keyEditType: %d,%d,%d\n", appSettings.keyMapping.keyEdit[0].deviceType, appSettings.keyMapping.keyEdit[1].deviceType, appSettings.keyMapping.keyEdit[2].deviceType);
  fprintf(file, "keyOptType: %d,%d,%d\n", appSettings.keyMapping.keyOpt[0].deviceType, appSettings.keyMapping.keyOpt[1].deviceType, appSettings.keyMapping.keyOpt[2].deviceType);
  fprintf(file, "keyPlayType: %d,%d,%d\n", appSettings.keyMapping.keyPlay[0].deviceType, appSettings.keyMapping.keyPlay[1].deviceType, appSettings.keyMapping.keyPlay[2].deviceType);
  fprintf(file, "keyShiftType: %d,%d,%d\n", appSettings.keyMapping.keyShift[0].deviceType, appSettings.keyMapping.keyShift[1].deviceType, appSettings.keyMapping.keyShift[2].deviceType);
  fprintf(file, "keyMotionLiveType: %d,%d,%d\n", appSettings.keyMapping.keyMotionLive[0].deviceType, appSettings.keyMapping.keyMotionLive[1].deviceType, appSettings.keyMapping.keyMotionLive[2].deviceType);
  fprintf(file, "keyMotionRecordType: %d,%d,%d\n", appSettings.keyMapping.keyMotionRecord[0].deviceType, appSettings.keyMapping.keyMotionRecord[1].deviceType, appSettings.keyMapping.keyMotionRecord[2].deviceType);
  fprintf(file, "keyMotionEraseType: %d,%d,%d\n", appSettings.keyMapping.keyMotionErase[0].deviceType, appSettings.keyMapping.keyMotionErase[1].deviceType, appSettings.keyMapping.keyMotionErase[2].deviceType);

  fprintf(file, "colorBackground: 0x%06x\n", appSettings.colorScheme.background);
  fprintf(file, "colorTextEmpty: 0x%06x\n", appSettings.colorScheme.textEmpty);
  fprintf(file, "colorTextInfo: 0x%06x\n", appSettings.colorScheme.textInfo);
  fprintf(file, "colorTextDefault: 0x%06x\n", appSettings.colorScheme.textDefault);
  fprintf(file, "colorTextLabels: 0x%06x\n", appSettings.colorScheme.textLabel);
  fprintf(file, "colorTextNames: 0x%06x\n", appSettings.colorScheme.textNames);
  fprintf(file, "colorValueBars: 0x%06x\n", appSettings.colorScheme.valueBar);
  fprintf(file, "colorTextValue: 0x%06x\n", appSettings.colorScheme.textValue);
  fprintf(file, "colorTextTitles: 0x%06x\n", appSettings.colorScheme.textTitles);
  fprintf(file, "colorPlayMarkers: 0x%06x\n", appSettings.colorScheme.playMarkers);
  fprintf(file, "colorCursor: 0x%06x\n", appSettings.colorScheme.cursor);
  fprintf(file, "colorSelection: 0x%06x\n", appSettings.colorScheme.selection);
  fprintf(file, "colorWarning: 0x%06x\n", appSettings.colorScheme.warning);
  fprintf(file, "themeName: %s\n", appSettings.themeName);
  fprintf(file, "projectFilename: %s\n", appSettings.projectFilename);
  fprintf(file, "projectPath: %s\n", appSettings.projectPath);
  fprintf(file, "pitchTablePath: %s\n", appSettings.pitchTablePath);
  fprintf(file, "instrumentPath: %s\n", appSettings.instrumentPath);
  fprintf(file, "themePath: %s\n", appSettings.themePath);
  fprintf(file, "fontPath: %s\n", appSettings.fontPath);
  fprintf(file, "fontFolderPath: %s\n", appSettings.fontFolderPath);
  fprintf(file, "samplePath: %s\n", appSettings.samplePath);
  fprintf(file, "exportPath: %s\n", appSettings.exportPath);
  fprintf(file, "exportLastFolder: %s\n", appSettings.exportLastFolder);
  fprintf(file, "ayWavetablePath: %s\n", appSettings.ayWavetablePath);
  fprintf(file, "scwfPath: %s\n", appSettings.scwfPath);
  fprintf(file, "srWavetablePath: %s\n", appSettings.srWavetablePath);

  fclose(file);
#ifdef WEB_BUILD
  // Emscripten's in-memory settings file becomes durable only after IDBFS sync.
  EM_ASM({ if (window.choochooSettingsSaved) window.choochooSettingsSaved(); });
#endif
  return 0;
}

int settingsLoad(void) {
  // Always initialize defaults first
  initDefaultAppSettings();

#ifndef WEB_BUILD
  char defaultDir[PATH_LENGTH];
  if (fileGetDefaultDirectory(defaultDir, PATH_LENGTH) != 0) return 1;
  snprintf(settingsPath, sizeof(settingsPath), "%s%ssettings.txt", defaultDir, PATH_SEPARATOR_STR);
#else
  snprintf(settingsPath, sizeof(settingsPath), "/user/settings.txt");
#endif

  FILE* file = fopen(settingsPath, "r");
  if (file == NULL) return 1;

  // Colours the file doesn't have are filled in after reading it.
  appSettings.colorScheme.textLabel = appSettings.colorScheme.textNames = appSettings.colorScheme.valueBar = -1;
  bool hasClockSource = false;
  while (fgets(lineBuffer, sizeof(lineBuffer), file) != NULL) {
    char* line = lineBuffer;
    // Strip newline characters from the end of the line
    int len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
      line[len - 1] = '\0';
      len--;
    }

    if (strncmp(line, "bootScreenNight: ", 17) == 0) {
      int enabled;
      if (sscanf(line + 17, "%d", &enabled) == 1 && (enabled == 0 || enabled == 1))
        appSettings.bootScreenNight = (uint8_t)enabled;
    } else if (strncmp(line, "persistentWaveform: ", 20) == 0) {
      int enabled;
      if (sscanf(line + 20, "%d", &enabled) == 1 && (enabled == 0 || enabled == 1))
        appSettings.persistentWaveform = (uint8_t)enabled;
    } else if (strncmp(line, "hideTouchPadWithGamepad: ", 25) == 0) {
      int enabled;
      if (sscanf(line + 25, "%d", &enabled) == 1 && (enabled == 0 || enabled == 1))
        appSettings.hideTouchPadWithGamepad = (uint8_t)enabled;
    } else if (strncmp(line, "touchHaptics: ", 14) == 0) {
      int enabled;
      if (sscanf(line + 14, "%d", &enabled) == 1 && (enabled == 0 || enabled == 1))
        appSettings.touchHaptics = (uint8_t)enabled;
    } else if (strncmp(line, "trackVisuals", 11) == 0) {
      int track, mode;
      if (sscanf(line, "trackVisuals%d: %d", &track, &mode) == 2 &&
          track >= 1 && track <= PROJECT_MAX_TRACKS && mode >= 0 && mode <= 1)
        appSettings.trackVisuals[track - 1].mode = (TrackVisualMode)mode;
    } else if (strncmp(line, "screenWidth: ", 13) == 0) {
      sscanf(line + 13, "%d", &appSettings.screenWidth);
    } else if (strncmp(line, "screenHeight: ", 14) == 0) {
      sscanf(line + 14, "%d", &appSettings.screenHeight);
    } else if (strncmp(line, "audioSampleRate: ", 17) == 0) {
      sscanf(line + 17, "%d", &appSettings.audioSampleRate);
    } else if (strncmp(line, "audioBufferSize: ", 17) == 0) {
      sscanf(line + 17, "%d", &appSettings.audioBufferSize);
    } else if (strncmp(line, "audioOutputDeviceName: ", 23) == 0) {
      strncpy(appSettings.audioOutputDeviceName, line + 23, AUDIO_DEVICE_NAME_LENGTH);
      appSettings.audioOutputDeviceName[AUDIO_DEVICE_NAME_LENGTH] = 0;
    } else if (strncmp(line, "aySampleDithering: ", 19) == 0) {
      sscanf(line + 19, "%d", &appSettings.aySampleDithering);
    } else if (strncmp(line, "doubleTapFrames: ", 17) == 0) {
      sscanf(line + 17, "%d", &appSettings.doubleTapFrames);
    } else if (strncmp(line, "keyRepeatDelay: ", 16) == 0) {
      sscanf(line + 16, "%d", &appSettings.keyRepeatDelay);
    } else if (strncmp(line, "keyRepeatSpeed: ", 16) == 0) {
      sscanf(line + 16, "%d", &appSettings.keyRepeatSpeed);
    } else if (strncmp(line, "mixVolume: ", 11) == 0) {
      sscanf(line + 11, "%f", &appSettings.mixVolume);
    } else if (strncmp(line, "quality: ", 9) == 0) {
      sscanf(line + 9, "%d", &appSettings.quality);
    } else if (strncmp(line, "braidsBits: ", 12) == 0) {
      sscanf(line + 12, "%d", &appSettings.braidsBits);
    } else if (strncmp(line, "braidsDrift: ", 13) == 0) {
      sscanf(line + 13, "%d", &appSettings.braidsDrift);
    } else if (strncmp(line, "braidsSignature: ", 17) == 0) {
      sscanf(line + 17, "%d", &appSettings.braidsSignature);
    } else if (strncmp(line, "braidsSignatureSeed: ", 21) == 0) {
      sscanf(line + 21, "%u", &appSettings.braidsSignatureSeed);
    } else if (strncmp(line, "pitchConflictWarning: ", 22) == 0) {
      sscanf(line + 22, "%d", &appSettings.pitchConflictWarning);
    } else if (strncmp(line, "quickHelpReleaseSeen: ", 22) == 0) {
      sscanf(line + 22, "%d", &appSettings.quickHelpReleaseSeen);
    } else if (strncmp(line, "ayWavetableLfoView: ", 20) == 0) {
      sscanf(line + 20, "%d", &appSettings.ayWavetableLfoView);
    } else if (strncmp(line, "waveformRefreshHz: ", 19) == 0) {
      sscanf(line + 19, "%d", &appSettings.waveformRefreshHz);
    } else if (strncmp(line, "playbackCounterMode: ", 21) == 0) {
      int mode;
      if (sscanf(line + 21, "%d", &mode) == 1 && mode >= 0 && mode <= 1)
        appSettings.playbackCounterMode = (uint8_t)mode;
    } else if (strncmp(line, "playbackBarRows: ", 17) == 0) {
      int rows;
      if (sscanf(line + 17, "%d", &rows) == 1 && rows >= 1 && rows <= 16)
        appSettings.playbackBarRows = (uint8_t)rows;
    } else if (strncmp(line, "playbackBarTicks: ", 18) == 0) {
      int ticks;
      if (sscanf(line + 18, "%d", &ticks) == 1 && ticks >= 1 && ticks <= 254)
        appSettings.playbackBarTicks = (uint8_t)ticks;
    } else if (strncmp(line, "stickLiveMode: ", 15) == 0) {
      appSettings.stickLiveMode = strcmp(line + 15, "FREE") == 0 ? StickLiveMode::free :
        strcmp(line + 15, "TOGGLE") == 0 ? StickLiveMode::toggle : StickLiveMode::hold;
    } else if (strncmp(line, "midiChannelInstrument: ", 23) == 0) {
      char* token = strtok(line + 23, ",");
      int i = 0;
      while (token && i < MIDI_CHANNEL_COUNT) {
        appSettings.midiChannelInstrument[i] = (int8_t)atoi(token);
        token = strtok(NULL, ",");
        i++;
      }
    } else if (strncmp(line, "midiInputDeviceName: ", 21) == 0) {
      strncpy(appSettings.midiInputDeviceName, line + 21, MIDI_DEVICE_NAME_LENGTH);
      appSettings.midiInputDeviceName[MIDI_DEVICE_NAME_LENGTH] = 0;
    } else if (strncmp(line, "midiOutputDeviceName: ", 22) == 0) {
      strncpy(appSettings.midiOutputDeviceName, line + 22, MIDI_DEVICE_NAME_LENGTH);
      appSettings.midiOutputDeviceName[MIDI_DEVICE_NAME_LENGTH] = 0;
    } else if (strncmp(line, "linkTransport: ", 15) == 0) {
      int value = 0;
      if (sscanf(line + 15, "%d", &value) == 1 && (value == 0 || value == 1))
        appSettings.linkTransport = (uint8_t)value;
    } else if (strncmp(line, "midiSendClock: ", 15) == 0) {
      int value = 0;
      if (sscanf(line + 15, "%d", &value) == 1 && (value == 0 || value == 1))
        appSettings.midiSendClock = (uint8_t)value;
    } else if (strncmp(line, "midiSendTransport: ", 19) == 0) {
      int value = 0;
      if (sscanf(line + 19, "%d", &value) == 1 && (value == 0 || value == 1))
        appSettings.midiSendTransport = (uint8_t)value;
    } else if (strncmp(line, "midiOutputDelayMs: ", 19) == 0) {
      int value = 0;
      if (sscanf(line + 19, "%d", &value) == 1 && value >= 0 && value <= 200)
        appSettings.midiOutputDelayMs = (uint8_t)value;
    } else if (strncmp(line, "clockSource: ", 13) == 0) {
      // Canonical v3+ setting. Read before the legacy `midiClockMode:`
      // alias below; if both lines exist (new binaries write both), the
      // canonical one wins regardless of their order.
      int source = 0;
      if (sscanf(line + 13, "%d", &source) == 1 &&
          (source == 0 || source == 1 || source == 2)) {
        appSettings.clockSource = (ClockSource)source;
        hasClockSource = true;
      }
    } else if (strncmp(line, "midiClockMode: ", 15) == 0) {
      // Legacy alias from v2 settings.txt: 0 -> internal, 1 -> midiClock.
      // Only applied when the canonical `clockSource:` line was absent,
      // so a file written by a new binary keeps its Link selection.
      int mode = 0;
      if (!hasClockSource && sscanf(line + 15, "%d", &mode) == 1 && (mode == 0 || mode == 1))
        appSettings.clockSource = mode == 1 ? ClockSource::midiClock : ClockSource::internal;
    } else if (strncmp(line, "audioInputDeviceCount: ", 23) == 0) {
      int count;
      if (sscanf(line + 23, "%d", &count) == 1)
        appSettings.audioInputDeviceCount = count < 0 ? 0 :
          (count > AUDIO_INPUT_MAX_DEVICES ? AUDIO_INPUT_MAX_DEVICES : count);
    } else if (strncmp(line, "audioInputDeviceName.", 21) == 0) {
      int index;
      if (sscanf(line + 21, "%d:", &index) == 1 && index >= 0 && index < AUDIO_INPUT_MAX_DEVICES) {
        const char* name = strchr(line + 21, ':');
        if (name && name[1] == ' ') {
          name += 2;
          strncpy(appSettings.audioInputDeviceNames[index], name, AUDIO_DEVICE_NAME_LENGTH);
          appSettings.audioInputDeviceNames[index][AUDIO_DEVICE_NAME_LENGTH] = 0;
        }
      }
    } else if (strncmp(line, "audioInputArmed.", 16) == 0) {
      int slot;
      if (sscanf(line + 16, "%d:", &slot) == 1 && slot >= 0 && slot < AUDIO_INPUT_MAX_ARMED) {
        const char* name = strchr(line + 16, ':');
        if (name && name[1] == ' ') {
          name += 2;
          // Resolve the persisted name to an index in the saved device list;
          // append if it isn't there yet (e.g. list was truncated on save).
          int index = -1;
          for (int i = 0; i < appSettings.audioInputDeviceCount && i < AUDIO_INPUT_MAX_DEVICES; i++) {
            if (strcmp(appSettings.audioInputDeviceNames[i], name) == 0) { index = i; break; }
          }
          if (index < 0 && appSettings.audioInputDeviceCount < AUDIO_INPUT_MAX_DEVICES) {
            index = appSettings.audioInputDeviceCount++;
            strncpy(appSettings.audioInputDeviceNames[index], name, AUDIO_DEVICE_NAME_LENGTH);
            appSettings.audioInputDeviceNames[index][AUDIO_DEVICE_NAME_LENGTH] = 0;
          }
          appSettings.audioInputArmed[slot] = (int8_t)index;
        }
      }
    } else if (strncmp(line, "audioInputMonitor: ", 19) == 0) {
      int enabled;
      if (sscanf(line + 19, "%d", &enabled) == 1 && (enabled == 0 || enabled == 1))
        appSettings.audioInputMonitor = enabled;
    } else if (strncmp(line, "recordingPath: ", 15) == 0) {
      strncpy(appSettings.recordingPath, line + 15, PATH_LENGTH);
      appSettings.recordingPath[PATH_LENGTH] = 0;
    } else if (strncmp(line, "keyUp: ", 7) == 0) {
      sscanf(line + 7, "%d,%d,%d", &appSettings.keyMapping.keyUp[0].code, &appSettings.keyMapping.keyUp[1].code, &appSettings.keyMapping.keyUp[2].code);
    } else if (strncmp(line, "keyDown: ", 9) == 0) {
      sscanf(line + 9, "%d,%d,%d", &appSettings.keyMapping.keyDown[0].code, &appSettings.keyMapping.keyDown[1].code, &appSettings.keyMapping.keyDown[2].code);
    } else if (strncmp(line, "keyLeft: ", 9) == 0) {
      sscanf(line + 9, "%d,%d,%d", &appSettings.keyMapping.keyLeft[0].code, &appSettings.keyMapping.keyLeft[1].code, &appSettings.keyMapping.keyLeft[2].code);
    } else if (strncmp(line, "keyRight: ", 10) == 0) {
      sscanf(line + 10, "%d,%d,%d", &appSettings.keyMapping.keyRight[0].code, &appSettings.keyMapping.keyRight[1].code, &appSettings.keyMapping.keyRight[2].code);
    } else if (strncmp(line, "keyEdit: ", 9) == 0) {
      sscanf(line + 9, "%d,%d,%d", &appSettings.keyMapping.keyEdit[0].code, &appSettings.keyMapping.keyEdit[1].code, &appSettings.keyMapping.keyEdit[2].code);
    } else if (strncmp(line, "keyOpt: ", 8) == 0) {
      sscanf(line + 8, "%d,%d,%d", &appSettings.keyMapping.keyOpt[0].code, &appSettings.keyMapping.keyOpt[1].code, &appSettings.keyMapping.keyOpt[2].code);
    } else if (strncmp(line, "keyPlay: ", 9) == 0) {
      sscanf(line + 9, "%d,%d,%d", &appSettings.keyMapping.keyPlay[0].code, &appSettings.keyMapping.keyPlay[1].code, &appSettings.keyMapping.keyPlay[2].code);
    } else if (strncmp(line, "keyShift: ", 10) == 0) {
      sscanf(line + 10, "%d,%d,%d", &appSettings.keyMapping.keyShift[0].code, &appSettings.keyMapping.keyShift[1].code, &appSettings.keyMapping.keyShift[2].code);
    } else if (strncmp(line, "keyMotionLive: ", 15) == 0) {
      sscanf(line + 15, "%d,%d,%d", &appSettings.keyMapping.keyMotionLive[0].code, &appSettings.keyMapping.keyMotionLive[1].code, &appSettings.keyMapping.keyMotionLive[2].code);
    } else if (strncmp(line, "keyMotionRecord: ", 17) == 0) {
      sscanf(line + 17, "%d,%d,%d", &appSettings.keyMapping.keyMotionRecord[0].code, &appSettings.keyMapping.keyMotionRecord[1].code, &appSettings.keyMapping.keyMotionRecord[2].code);
    } else if (strncmp(line, "keyMotionErase: ", 16) == 0) {
      sscanf(line + 16, "%d,%d,%d", &appSettings.keyMapping.keyMotionErase[0].code, &appSettings.keyMapping.keyMotionErase[1].code, &appSettings.keyMapping.keyMotionErase[2].code);
    } else if (strncmp(line, "keyUpType: ", 11) == 0) {
      sscanf(line + 11, "%d,%d,%d", (int *)&appSettings.keyMapping.keyUp[0].deviceType, (int *)&appSettings.keyMapping.keyUp[1].deviceType, (int *)&appSettings.keyMapping.keyUp[2].deviceType);
    } else if (strncmp(line, "keyDownType: ", 13) == 0) {
      sscanf(line + 13, "%d,%d,%d", (int *)&appSettings.keyMapping.keyDown[0].deviceType, (int *)&appSettings.keyMapping.keyDown[1].deviceType, (int *)&appSettings.keyMapping.keyDown[2].deviceType);
    } else if (strncmp(line, "keyLeftType: ", 13) == 0) {
      sscanf(line + 13, "%d,%d,%d", (int *)&appSettings.keyMapping.keyLeft[0].deviceType, (int *)&appSettings.keyMapping.keyLeft[1].deviceType, (int *)&appSettings.keyMapping.keyLeft[2].deviceType);
    } else if (strncmp(line, "keyRightType: ", 14) == 0) {
      sscanf(line + 14, "%d,%d,%d", (int *)&appSettings.keyMapping.keyRight[0].deviceType, (int *)&appSettings.keyMapping.keyRight[1].deviceType, (int *)&appSettings.keyMapping.keyRight[2].deviceType);
    } else if (strncmp(line, "keyEditType: ", 13) == 0) {
      sscanf(line + 13, "%d,%d,%d", (int *)&appSettings.keyMapping.keyEdit[0].deviceType, (int *)&appSettings.keyMapping.keyEdit[1].deviceType, (int *)&appSettings.keyMapping.keyEdit[2].deviceType);
    } else if (strncmp(line, "keyOptType: ", 12) == 0) {
      sscanf(line + 12, "%d,%d,%d", (int *)&appSettings.keyMapping.keyOpt[0].deviceType, (int *)&appSettings.keyMapping.keyOpt[1].deviceType, (int *)&appSettings.keyMapping.keyOpt[2].deviceType);
    } else if (strncmp(line, "keyPlayType: ", 13) == 0) {
      sscanf(line + 13, "%d,%d,%d", (int *)&appSettings.keyMapping.keyPlay[0].deviceType, (int *)&appSettings.keyMapping.keyPlay[1].deviceType, (int *)&appSettings.keyMapping.keyPlay[2].deviceType);
    } else if (strncmp(line, "keyShiftType: ", 14) == 0) {
      sscanf(line + 14, "%d,%d,%d", (int *)&appSettings.keyMapping.keyShift[0].deviceType, (int *)&appSettings.keyMapping.keyShift[1].deviceType, (int *)&appSettings.keyMapping.keyShift[2].deviceType);
    } else if (strncmp(line, "keyMotionLiveType: ", 19) == 0) {
      sscanf(line + 19, "%d,%d,%d", (int *)&appSettings.keyMapping.keyMotionLive[0].deviceType, (int *)&appSettings.keyMapping.keyMotionLive[1].deviceType, (int *)&appSettings.keyMapping.keyMotionLive[2].deviceType);
    } else if (strncmp(line, "keyMotionRecordType: ", 21) == 0) {
      sscanf(line + 21, "%d,%d,%d", (int *)&appSettings.keyMapping.keyMotionRecord[0].deviceType, (int *)&appSettings.keyMapping.keyMotionRecord[1].deviceType, (int *)&appSettings.keyMapping.keyMotionRecord[2].deviceType);
    } else if (strncmp(line, "keyMotionEraseType: ", 20) == 0) {
      sscanf(line + 20, "%d,%d,%d", (int *)&appSettings.keyMapping.keyMotionErase[0].deviceType, (int *)&appSettings.keyMapping.keyMotionErase[1].deviceType, (int *)&appSettings.keyMapping.keyMotionErase[2].deviceType);
    } else if (strncmp(line, "colorBackground: ", 17) == 0) {
      sscanf(line + 17, "0x%x", &appSettings.colorScheme.background);
    } else if (strncmp(line, "colorTextEmpty: ", 16) == 0) {
      sscanf(line + 16, "0x%x", &appSettings.colorScheme.textEmpty);
    } else if (strncmp(line, "colorTextInfo: ", 15) == 0) {
      sscanf(line + 15, "0x%x", &appSettings.colorScheme.textInfo);
    } else if (strncmp(line, "colorTextDefault: ", 18) == 0) {
      sscanf(line + 18, "0x%x", &appSettings.colorScheme.textDefault);
    } else if (strncmp(line, "colorTextLabels: ", 17) == 0) {
      sscanf(line + 17, "0x%x", &appSettings.colorScheme.textLabel);
    } else if (strncmp(line, "colorTextNames: ", 16) == 0) {
      sscanf(line + 16, "0x%x", &appSettings.colorScheme.textNames);
    } else if (strncmp(line, "colorValueBars: ", 16) == 0) {
      sscanf(line + 16, "0x%x", &appSettings.colorScheme.valueBar);
    } else if (strncmp(line, "colorTextValue: ", 16) == 0) {
      sscanf(line + 16, "0x%x", &appSettings.colorScheme.textValue);
    } else if (strncmp(line, "colorTextTitles: ", 17) == 0) {
      sscanf(line + 17, "0x%x", &appSettings.colorScheme.textTitles);
    } else if (strncmp(line, "colorPlayMarkers: ", 18) == 0) {
      sscanf(line + 18, "0x%x", &appSettings.colorScheme.playMarkers);
    } else if (strncmp(line, "colorCursor: ", 13) == 0) {
      sscanf(line + 13, "0x%x", &appSettings.colorScheme.cursor);
    } else if (strncmp(line, "colorSelection: ", 16) == 0) {
      sscanf(line + 16, "0x%x", &appSettings.colorScheme.selection);
    } else if (strncmp(line, "colorWarning: ", 14) == 0) {
      sscanf(line + 14, "0x%x", &appSettings.colorScheme.warning);
    } else if (strncmp(line, "themeName: ", 11) == 0) {
      strncpy(appSettings.themeName, line + 11, 16);
      appSettings.themeName[16] = 0;
    } else if (strncmp(line, "projectFilename: ", 17) == 0) {
      strncpy(appSettings.projectFilename, line + 17, FILENAME_LENGTH);
      appSettings.projectFilename[FILENAME_LENGTH] = 0;
    } else if (strncmp(line, "projectPath: ", 13) == 0) {
      strncpy(appSettings.projectPath, line + 13, PATH_LENGTH);
      appSettings.projectPath[PATH_LENGTH] = 0;
    } else if (strncmp(line, "pitchTablePath: ", 16) == 0) {
      strncpy(appSettings.pitchTablePath, line + 16, PATH_LENGTH);
      appSettings.pitchTablePath[PATH_LENGTH] = 0;
    } else if (strncmp(line, "instrumentPath: ", 16) == 0) {
      strncpy(appSettings.instrumentPath, line + 16, PATH_LENGTH);
      appSettings.instrumentPath[PATH_LENGTH] = 0;
    } else if (strncmp(line, "themePath: ", 11) == 0) {
      strncpy(appSettings.themePath, line + 11, PATH_LENGTH);
      appSettings.themePath[PATH_LENGTH] = 0;
    } else if (strncmp(line, "fontPath: ", 10) == 0) {
      strncpy(appSettings.fontPath, line + 10, PATH_LENGTH);
      appSettings.fontPath[PATH_LENGTH] = 0;
    } else if (strncmp(line, "fontFolderPath: ", 16) == 0) {
      strncpy(appSettings.fontFolderPath, line + 16, PATH_LENGTH);
      appSettings.fontFolderPath[PATH_LENGTH] = 0;
    } else if (strncmp(line, "samplePath: ", 12) == 0) {
      strncpy(appSettings.samplePath, line + 12, PATH_LENGTH);
      appSettings.samplePath[PATH_LENGTH] = 0;
    } else if (strncmp(line, "exportPath: ", 12) == 0) {
      strncpy(appSettings.exportPath, line + 12, PATH_LENGTH);
      appSettings.exportPath[PATH_LENGTH] = 0;
    } else if (strncmp(line, "exportLastFolder: ", 18) == 0) {
      strncpy(appSettings.exportLastFolder, line + 18, FILENAME_LENGTH);
      appSettings.exportLastFolder[FILENAME_LENGTH] = 0;
    } else if (strncmp(line, "ayWavetablePath: ", 17) == 0) {
      strncpy(appSettings.ayWavetablePath, line + 17, PATH_LENGTH);
      appSettings.ayWavetablePath[PATH_LENGTH] = 0;
    } else if (strncmp(line, "scwfPath: ", 10) == 0) {
      strncpy(appSettings.scwfPath, line + 10, PATH_LENGTH);
      appSettings.scwfPath[PATH_LENGTH] = 0;
    } else if (strncmp(line, "srWavetablePath: ", 17) == 0) {
      strncpy(appSettings.srWavetablePath, line + 17, PATH_LENGTH);
      appSettings.srWavetablePath[PATH_LENGTH] = 0;
    } else if (strncmp(line, "wavetablePath: ", 15) == 0) {
      // Backwards compatibility with settings written before AY/SR separation.
      strncpy(appSettings.ayWavetablePath, line + 15, PATH_LENGTH);
      appSettings.ayWavetablePath[PATH_LENGTH] = 0;
    }
  }

  fclose(file);
  colorSchemeFillMissing(&appSettings.colorScheme);
  if (appSettings.audioSampleRate != 32000 && appSettings.audioSampleRate != 48000)
    appSettings.audioSampleRate = kAudioSampleRate;
  // 4,906 was the legacy platform default; other values are rounded to the
  // nearest choice exposed by the audio settings screen.
  appSettings.audioBufferSize = appSettings.audioBufferSize == 4906
    ? defaultAudioBufferSize() : normalizeAudioBufferSize(appSettings.audioBufferSize);
#ifdef PORTMASTER_BUILD
  if (appSettings.keyRepeatSpeed == 2) appSettings.keyRepeatSpeed = 1;
  if (appSettings.waveformRefreshHz == 30) appSettings.waveformRefreshHz = 60;
#endif
  if (appSettings.braidsBits < 0 || appSettings.braidsBits > 6) appSettings.braidsBits = 6;
  if (appSettings.braidsDrift < 0 || appSettings.braidsDrift > 4) appSettings.braidsDrift = 0;
  if (appSettings.braidsSignature < 0 || appSettings.braidsSignature > 4) appSettings.braidsSignature = 0;
  if (appSettings.waveformRefreshHz < 1 || appSettings.waveformRefreshHz > 60) appSettings.waveformRefreshHz = 30;
  for (int i = 0; i < MIDI_CHANNEL_COUNT; i++) {
    if (appSettings.midiChannelInstrument[i] < -1 || appSettings.midiChannelInstrument[i] >= PROJECT_MAX_INSTRUMENTS)
      appSettings.midiChannelInstrument[i] = -1;
  }
  if (appSettings.audioInputDeviceCount < 0) appSettings.audioInputDeviceCount = 0;
  if (appSettings.audioInputDeviceCount > AUDIO_INPUT_MAX_DEVICES) appSettings.audioInputDeviceCount = AUDIO_INPUT_MAX_DEVICES;
  for (int i = 0; i < AUDIO_INPUT_MAX_ARMED; i++) {
    if (appSettings.audioInputArmed[i] >= appSettings.audioInputDeviceCount) appSettings.audioInputArmed[i] = -1;
  }
  if (appSettings.audioInputMonitor != 0 && appSettings.audioInputMonitor != 1) appSettings.audioInputMonitor = 0;
  return 0;
}

void colorSchemeFillMissing(ColorScheme* scheme) {
  // The built-in Choo text colours, on any background, have their own
  // choices; other themes get labels as Text Default dimmed a quarter of the
  // way to the background, names as Text Default and bars in the cursor
  // colour.
  const bool choo = scheme->textDefault == 0xc9bfa5 && scheme->textValue == 0xffd665;
  if (scheme->textLabel < 0) {
    int color = 0;
    for (int shift = 0; shift <= 16; shift += 8) {
      const int text = (scheme->textDefault >> shift) & 0xff, ground = (scheme->background >> shift) & 0xff;
      color |= (ground + (text - ground) * 3 / 4) << shift;
    }
    scheme->textLabel = choo ? 0x7d8fa6 : color;
  }
  if (scheme->textNames < 0) scheme->textNames = choo ? 0x9fcf8f : scheme->textDefault;
  if (scheme->valueBar < 0) scheme->valueBar = choo ? 0x64d6ff : scheme->cursor;
}

void resetToDefaultColors(void) {
  appSettings.colorScheme.background = 0x000f1a;
  appSettings.colorScheme.textEmpty = 0x002638;
  appSettings.colorScheme.textInfo = 0x4878b0;
  appSettings.colorScheme.textDefault = 0xa0d0f0;
  appSettings.colorScheme.textLabel = appSettings.colorScheme.textNames = appSettings.colorScheme.valueBar = -1;
  appSettings.colorScheme.textValue = 0xe2ebf8;
  appSettings.colorScheme.textTitles = 0xbfdf50;
  appSettings.colorScheme.playMarkers = 0xefe000;
  appSettings.colorScheme.cursor = 0x7ddcff;
  appSettings.colorScheme.selection = 0x00d090;
  appSettings.colorScheme.warning = 0xff4040;
  colorSchemeFillMissing(&appSettings.colorScheme);
}

int saveTheme(const char* path) {
  FILE* file = fopen(path, "w");
  if (file == NULL) return 1;

  fprintf(file, "colorBackground: 0x%06x\n", appSettings.colorScheme.background);
  fprintf(file, "colorTextEmpty: 0x%06x\n", appSettings.colorScheme.textEmpty);
  fprintf(file, "colorTextInfo: 0x%06x\n", appSettings.colorScheme.textInfo);
  fprintf(file, "colorTextDefault: 0x%06x\n", appSettings.colorScheme.textDefault);
  fprintf(file, "colorTextLabels: 0x%06x\n", appSettings.colorScheme.textLabel);
  fprintf(file, "colorTextNames: 0x%06x\n", appSettings.colorScheme.textNames);
  fprintf(file, "colorValueBars: 0x%06x\n", appSettings.colorScheme.valueBar);
  fprintf(file, "colorTextValue: 0x%06x\n", appSettings.colorScheme.textValue);
  fprintf(file, "colorTextTitles: 0x%06x\n", appSettings.colorScheme.textTitles);
  fprintf(file, "colorPlayMarkers: 0x%06x\n", appSettings.colorScheme.playMarkers);
  fprintf(file, "colorCursor: 0x%06x\n", appSettings.colorScheme.cursor);
  fprintf(file, "colorSelection: 0x%06x\n", appSettings.colorScheme.selection);
  fprintf(file, "colorWarning: 0x%06x\n", appSettings.colorScheme.warning);

  fclose(file);
  return 0;
}

int loadTheme(const char* path) {
  FILE* file = fopen(path, "r");
  if (file == NULL) return 1;

  // First reset to defaults to ensure all colors have valid values
  resetToDefaultColors();

  // Colours the file doesn't have are filled in after reading it.
  appSettings.colorScheme.textLabel = appSettings.colorScheme.textNames = appSettings.colorScheme.valueBar = -1;
  while (fgets(lineBuffer, sizeof(lineBuffer), file) != NULL) {
    char* line = lineBuffer;
    if (strncmp(line, "colorBackground: ", 17) == 0) {
      sscanf(line + 17, "0x%x", &appSettings.colorScheme.background);
    } else if (strncmp(line, "colorTextEmpty: ", 16) == 0) {
      sscanf(line + 16, "0x%x", &appSettings.colorScheme.textEmpty);
    } else if (strncmp(line, "colorTextInfo: ", 15) == 0) {
      sscanf(line + 15, "0x%x", &appSettings.colorScheme.textInfo);
    } else if (strncmp(line, "colorTextDefault: ", 18) == 0) {
      sscanf(line + 18, "0x%x", &appSettings.colorScheme.textDefault);
    } else if (strncmp(line, "colorTextLabels: ", 17) == 0) {
      sscanf(line + 17, "0x%x", &appSettings.colorScheme.textLabel);
    } else if (strncmp(line, "colorTextNames: ", 16) == 0) {
      sscanf(line + 16, "0x%x", &appSettings.colorScheme.textNames);
    } else if (strncmp(line, "colorValueBars: ", 16) == 0) {
      sscanf(line + 16, "0x%x", &appSettings.colorScheme.valueBar);
    } else if (strncmp(line, "colorTextValue: ", 16) == 0) {
      sscanf(line + 16, "0x%x", &appSettings.colorScheme.textValue);
    } else if (strncmp(line, "colorTextTitles: ", 17) == 0) {
      sscanf(line + 17, "0x%x", &appSettings.colorScheme.textTitles);
    } else if (strncmp(line, "colorPlayMarkers: ", 18) == 0) {
      sscanf(line + 18, "0x%x", &appSettings.colorScheme.playMarkers);
    } else if (strncmp(line, "colorCursor: ", 13) == 0) {
      sscanf(line + 13, "0x%x", &appSettings.colorScheme.cursor);
    } else if (strncmp(line, "colorSelection: ", 16) == 0) {
      sscanf(line + 16, "0x%x", &appSettings.colorScheme.selection);
    } else if (strncmp(line, "colorWarning: ", 14) == 0) {
      sscanf(line + 14, "0x%x", &appSettings.colorScheme.warning);
    }
  }

  fclose(file);
  colorSchemeFillMissing(&appSettings.colorScheme);
  return 0;
}

const char* getAutosavePath(void) {
#ifdef WEB_BUILD
  return "/user/autosave.cct";
#else
  char defaultDir[PATH_LENGTH];
  if (fileGetDefaultDirectory(defaultDir, PATH_LENGTH) != 0) return AUTOSAVE_FILENAME;
  snprintf(autosavePath, sizeof(autosavePath), "%s%s%s", defaultDir, PATH_SEPARATOR_STR, AUTOSAVE_FILENAME);
  return autosavePath;
#endif
}

void extractFilenameWithoutExtension(const char* path, char* output, int maxLength) {
  // Extract filename from path
  const char* filename = strrchr(path, PATH_SEPARATOR);
  if (filename) {
    filename++; // Skip the separator
  } else {
    filename = path; // No path separator found
  }

  // Copy filename
  strncpy(output, filename, maxLength - 1);
  output[maxLength - 1] = 0;

  // Remove file extension
  char* dot = strrchr(output, '.');
  if (dot) {
    *dot = 0;
  }
}

void updatePathFromFile(char* destination, const char* path) {
  const char* separator = strrchr(path, PATH_SEPARATOR);
  if (!separator) return;
  size_t length = separator - path;
  if (length > PATH_LENGTH) length = PATH_LENGTH;
  memcpy(destination, path, length);
  destination[length] = 0;
}

void clearNotePreview(void) {
  // Clear the note preview area for all tracks (right side of screen)
  gfxClearRect(35, 3, 5, PROJECT_MAX_TRACKS);
}

void initDefaultKeyMapping(void) {
  // Platform-specific initialization is done in corelib_input
  // This function is kept for API compatibility
  memset(&appSettings.keyMapping, 0, sizeof(KeyMapping));
}

void resetKeyMappingToDefaults(void) {
  // Platform-specific reset is done in corelib_input
  // This function is kept for API compatibility
  memset(&appSettings.keyMapping, 0, sizeof(KeyMapping));
}
