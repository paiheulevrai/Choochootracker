#pragma once
// Host-only shim for audio_diagnostics_check.cpp.
#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_ERROR 6
inline int __android_log_print(int, const char*, const char*, ...) { return 0; }
