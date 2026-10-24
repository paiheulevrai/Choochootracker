#include "support_report.h"

#include "audio_manager.h"
#include "common.h"
#include "corelib/corelib_file.h"
#include "corelib/corelib_mainloop.h"
#include "monitor_display.h"
#include "project_utils.h"
#include "version.h"

#include <stdio.h>
#include <string.h>

#ifdef ANDROID_BUILD
#include "../platforms/android/audio_diagnostics.h"
#endif

#ifdef WEB_BUILD
#include <emscripten/emscripten.h>
EM_JS(void, webDownloadSupportReport, (const char* path), {
  if (window.choochooDownloadFile) {
    window.choochooDownloadFile(UTF8ToString(path));
  }
});
#endif

static const char* supportPlatformName(void) {
#ifdef ANDROID_BUILD
  return "Android";
#elif defined(WEB_BUILD)
  return "Web";
#elif defined(PORTMASTER_BUILD)
  return "PortMaster";
#elif defined(MIYOOPORTS_BUILD)
  return "MiyooPorts";
#elif defined(MACOS_BUILD)
  return "macOS";
#elif defined(_WIN32)
  return "Windows";
#elif defined(__linux__)
  return "Linux";
#elif defined(DESKTOP_BUILD)
  return "Desktop";
#else
  return "Unknown";
#endif
}

static void writeSanitized(FILE* file, const char* text) {
  if (!text) return;
  for (const unsigned char* p = (const unsigned char*)text; *p; ++p) {
    if (*p == '\n' || *p == '\r' || *p == '\t') fputc(' ', file);
    else if (*p >= 32 && *p != 127) fputc(*p, file);
  }
}

static void writeProjectSummary(FILE* file) {
  if (!chipnomadState) {
    fprintf(file, "project.loaded=0\n");
    return;
  }

  Project* project = &chipnomadState->project;
  int instrumentCount = 0;
  for (int i = 0; i < PROJECT_MAX_INSTRUMENTS; ++i)
    if (project->instruments[i].type != InstrumentType::none) ++instrumentCount;

  fprintf(file, "project.loaded=1\n");
  fprintf(file, "project.modified=%d\n", projectModified ? 1 : 0);
  fprintf(file, "project.tracks=%d\n", project->tracksCount);
  fprintf(file, "project.chips=%d\n", project->chipsCount);
  fprintf(file, "project.chip_type=%d\n", (int)project->chipType);
  fprintf(file, "project.tick_rate=%.3f\n", project->tickRate);
  fprintf(file, "project.instrument_count=%d\n", instrumentCount);

  fprintf(file, "project.instrument_types=");
  int listed = 0;
  for (int i = 0; i < PROJECT_MAX_INSTRUMENTS && listed < 16; ++i) {
    InstrumentType type = project->instruments[i].type;
    if (type == InstrumentType::none) continue;
    if (listed) fputc(',', file);
    fprintf(file, "%02X:", i);
    writeSanitized(file, instrumentTypeName(type));
    ++listed;
  }
  if (instrumentCount > listed) fprintf(file, ",...");
  fputc('\n', file);
}

int supportReportWrite(const char* path, const char* screenName) {
  if (!path || !path[0]) return 1;

  FILE* file = fopen(path, "w");
  if (!file) return 1;

  fprintf(file, "ChooChooTracker support report\n");
  fprintf(file, "version=");
  writeSanitized(file, appVersion);
  fputc('\n', file);
  fprintf(file, "build=");
  writeSanitized(file, appBuild);
  fputc('\n', file);
  fprintf(file, "platform=%s\n", supportPlatformName());
  fprintf(file, "screen=");
  writeSanitized(file, screenName ? screenName : "Unknown");
  fputc('\n', file);

  fprintf(file, "audio.sample_rate=%d\n", appSettings.audioSampleRate);
  fprintf(file, "audio.buffer_size=%d\n", appSettings.audioBufferSize);
  fprintf(file, "audio.cpu_load_percent=%d\n",
          audioManager.getCpuLoadPercent ? audioManager.getCpuLoadPercent() : -1);
  fprintf(file, "ui.target_fps=%d\n", mainLoopGetTargetFps());
  fprintf(file, "ui.frame_time_us=%d\n", mainLoopGetFrameTimeUs());
  fprintf(file, "ui.audio_monitor_active=%d\n", monitorDisplayActive() ? 1 : 0);

  fprintf(file, "runtime.command_overflow=%d\n", chipnomadGetCommandOverflow(chipnomadState));
  fprintf(file, "runtime.render_buffer_overflow=%d\n", chipnomadGetRenderBufferOverflow(chipnomadState));
  fprintf(file, "runtime.motion_record_overflow=%d\n", chipnomadGetMotionRecordOverflow());
  fprintf(file, "runtime.motion_record_dropped=%u\n", chipnomadGetMotionRecordDroppedCount());

#ifdef ANDROID_BUILD
  fprintf(file, "android.audio_diagnostics_enabled=%d\n", audioDiagnostics::enabled ? 1 : 0);
  fprintf(file, "android.invalid_buffers=%u\n",
          audioDiagnostics::invalidBuffers.load(std::memory_order_relaxed));
  fprintf(file, "android.render_failures=%u\n",
          audioDiagnostics::renderFailures.load(std::memory_order_relaxed));
#endif

  int isPlaying = 0;
  if (chipnomadState && chipnomadState->audioCommands) {
    const PlaybackStatus* status = chipnomadGetPlaybackStatus(chipnomadState);
    if (status) isPlaying = status->isPlaying ? 1 : 0;
  }
  fprintf(file, "playback.is_playing=%d\n", isPlaying);

  writeProjectSummary(file);

  fprintf(file, "last_project_file_error=");
  writeSanitized(file, projectFileError);
  fputc('\n', file);

  int result = ferror(file) ? 1 : 0;
  if (fclose(file) != 0) result = 1;
  return result;
}

int supportReportSaveDefault(const char* screenName, char* outputPath, int outputPathSize) {
  if (!outputPath || outputPathSize <= 0) return 1;

#ifdef WEB_BUILD
  int written = snprintf(outputPath, outputPathSize, "/user/support-report.txt");
#else
  char defaultDir[PATH_LENGTH];
  if (fileGetDefaultDirectory(defaultDir, sizeof(defaultDir)) != 0) return 1;
  int written = snprintf(outputPath, outputPathSize, "%s%ssupport-report.txt",
                         defaultDir, PATH_SEPARATOR_STR);
#endif
  if (written < 0 || written >= outputPathSize) return 1;
  if (supportReportWrite(outputPath, screenName) != 0) return 1;

#ifdef ANDROID_BUILD
  fileExportDocument(outputPath, "text/plain");
#elif defined(WEB_BUILD)
  webDownloadSupportReport(outputPath);
#endif
  return 0;
}
