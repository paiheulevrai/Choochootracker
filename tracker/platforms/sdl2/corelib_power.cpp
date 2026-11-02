#include "corelib_power.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#if defined(__linux__) && !defined(ANDROID_BUILD)
#include <dirent.h>
#endif

static int batteryPlusPercent(void) {
#if defined(__linux__) && !defined(ANDROID_BUILD)
  FILE* file = fopen("/tmp/battery.percent", "r");
  if (!file) return -1;
  int percent = -1;
  const int read = fscanf(file, "%d", &percent);
  fclose(file);
  if (read == 1 && percent >= 0 && percent <= 100) return percent;
#endif
  return -1;
}

static bool systemBatteryInfo(PowerInfo* info) {
#if defined(__linux__) && !defined(ANDROID_BUILD)
  DIR* supplies = opendir("/sys/class/power_supply");
  if (!supplies) return false;
  struct dirent* entry;
  while ((entry = readdir(supplies)) != NULL) {
    if (entry->d_name[0] == '.') continue;
    char path[256];
    if (snprintf(path, sizeof(path), "/sys/class/power_supply/%s/type", entry->d_name) >= (int)sizeof(path))
      continue;
    FILE* file = fopen(path, "r");
    if (!file) continue;
    char type[16];
    const int typeRead = fscanf(file, "%15s", type);
    fclose(file);
    if (typeRead != 1 || strcmp(type, "Battery") != 0) continue;

    int percent = -1;
    if (snprintf(path, sizeof(path), "/sys/class/power_supply/%s/capacity", entry->d_name) < (int)sizeof(path)) {
      file = fopen(path, "r");
      if (file) {
        const int percentRead = fscanf(file, "%d", &percent);
        fclose(file);
        if (percentRead != 1) percent = -1;
      }
    }
    if (percent < 0 || percent > 100) percent = -1;

    PowerState state = PowerState::unknown;
    if (snprintf(path, sizeof(path), "/sys/class/power_supply/%s/status", entry->d_name) < (int)sizeof(path)) {
      file = fopen(path, "r");
      if (file) {
        char status[32];
        if (fscanf(file, "%31s", status) == 1) {
          if (strcmp(status, "Discharging") == 0) state = PowerState::onBattery;
          else if (strcmp(status, "Charging") == 0) state = PowerState::charging;
          else if (strcmp(status, "Full") == 0) state = PowerState::charged;
        }
        fclose(file);
      }
    }
    closedir(supplies);
    *info = {state, percent, -1};
    return true;
  }
  closedir(supplies);
#else
  (void)info;
#endif
  return false;
}

PowerInfo powerGetInfo(void) {
  int seconds = -1, percent = -1;
  const SDL_PowerState state = SDL_GetPowerInfo(&seconds, &percent);
  PowerInfo result = {state == SDL_POWERSTATE_NO_BATTERY ? PowerState::noBattery :
    state == SDL_POWERSTATE_ON_BATTERY ? PowerState::onBattery :
    state == SDL_POWERSTATE_CHARGING ? PowerState::charging :
    state == SDL_POWERSTATE_CHARGED ? PowerState::charged : PowerState::unknown, percent, seconds};
  const int calibratedPercent = batteryPlusPercent();
  if (calibratedPercent >= 0) {
    result.percent = calibratedPercent;
    result.seconds = -1;
  }
  PowerInfo systemInfo;
  if (systemBatteryInfo(&systemInfo)) {
    if (systemInfo.state != PowerState::unknown) result.state = systemInfo.state;
    if (calibratedPercent < 0 && systemInfo.percent >= 0) result.percent = systemInfo.percent;
  }
  return result;
}

uint64_t powerNowSeconds(void) { return SDL_GetTicks() / 1000u; }
