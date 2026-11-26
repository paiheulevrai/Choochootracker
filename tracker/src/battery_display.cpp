#include "battery_display.h"
#include "app.h"
#include "corelib_gfx.h"
#include "corelib_power.h"
#include <stdio.h>
#include <string.h>

struct BatterySample { uint64_t time; int percent; };
static BatterySample batterySamples[11];
static int batterySampleCount;
static int batteryEstimatedSeconds;
static uint64_t batteryLastPoll;
static bool batteryHasPoll;
static PowerInfo batteryInfo;
static const int batteryFallbackSeconds = 6 * 60 * 60;

static bool batteryHasDischargeInfo(const PowerInfo& info) {
  if (info.state == PowerState::onBattery) return true;
  return (info.state == PowerState::unknown || info.state == PowerState::noBattery) &&
    (info.percent >= 0 || info.seconds >= 0);
}

void appResetBatteryInfo(void) {
  batterySampleCount = 0;
  batteryEstimatedSeconds = -1;
  batteryLastPoll = 0;
  batteryHasPoll = false;
  batteryInfo = {PowerState::unknown, -1, -1};
}

static void batteryAddSample(uint64_t now, int percent) {
  const int previousPercent = batterySampleCount ? batterySamples[batterySampleCount - 1].percent : percent;
  if (batterySampleCount == 11) {
    memmove(batterySamples, batterySamples + 1, 10 * sizeof(batterySamples[0]));
    batterySampleCount = 10;
  }
  batterySamples[batterySampleCount++] = {now, percent};
  if (batterySampleCount < 2 || percent >= previousPercent) return;
  const int drop = batterySamples[0].percent - percent;
  const uint64_t elapsed = now - batterySamples[0].time;
  if (drop < 1 || elapsed < 60) return;
  const uint64_t raw = (uint64_t)percent * elapsed / (uint64_t)drop;
  const int bounded = raw > 359999 ? 359999 : (int)raw;
  batteryEstimatedSeconds = batteryEstimatedSeconds < 0 ? bounded :
    (batteryEstimatedSeconds * 3 + bounded) / 4;
}

static void batteryPoll(void) {
  const uint64_t now = powerNowSeconds();
  if (batteryHasPoll && now >= batteryLastPoll && now - batteryLastPoll < 60) return;
  const PowerInfo next = powerGetInfo();
  const bool interrupted = batteryHasPoll && (now < batteryLastPoll || now - batteryLastPoll > 150);
  const bool stateChanged = batteryHasPoll && next.state != batteryInfo.state;
  const bool percentRose = batterySampleCount && next.percent > batterySamples[batterySampleCount - 1].percent;
  if (interrupted || stateChanged || percentRose || !batteryHasDischargeInfo(next) || next.percent < 0) {
    batterySampleCount = 0;
    batteryEstimatedSeconds = -1;
  }
  batteryInfo = next;
  batteryLastPoll = now;
  batteryHasPoll = true;
  if (batteryHasDischargeInfo(next) && next.percent >= 0)
    batteryAddSample(now, next.percent);
}

void batteryDisplayDraw(void) {
  gfxSetFgColor(appSettings.colorScheme.textTitles);
  gfxClearRect(34, 1, 6, 1);
  if (!appSettings.batteryInfoMode) return;
  batteryPoll();
  char label[7] = "";
  if (batteryInfo.state == PowerState::charging) snprintf(label, sizeof(label), "CHARGE");
  else if (batteryInfo.state == PowerState::charged) snprintf(label, sizeof(label), "FULL");
  else if (!batteryHasDischargeInfo(batteryInfo)) return;
  else if (appSettings.batteryInfoMode == 2) {
    if (batteryInfo.percent < 0) return;
    snprintf(label, sizeof(label), "%d%%", batteryInfo.percent);
  } else {
    int seconds = batteryInfo.seconds >= 0 ? batteryInfo.seconds : batteryEstimatedSeconds;
    if (seconds < 0 && batteryInfo.percent >= 0)
      seconds = batteryInfo.percent * batteryFallbackSeconds / 100;
    if (seconds < 0) return;
    const int minutes = (seconds + 59) / 60;
    if (minutes < 60) snprintf(label, sizeof(label), "%dmin", minutes);
    else if (minutes < 6000) snprintf(label, sizeof(label), "%dh%02d", minutes / 60, minutes % 60);
    else snprintf(label, sizeof(label), "99h+");
  }
  char field[7] = "      ";
  const size_t length = strlen(label);
  memcpy(field + 6 - length, label, length);
  gfxPrint(34, 1, field);
}
