#include "chipnomad_lib.h"
#include "chipnomad_lib_live_stick.h"

#include <atomic>
#include <math.h>

static std::atomic<int32_t> liveStickAxes[4];
static std::atomic<int> liveStickEnabled;
static std::atomic<int> motionRecordMode;
static std::atomic<int> motionRecordRateReset;
static std::atomic<int> motionRecordDirty;
static std::atomic<int> motionRecordOverflow;

static float deadzoneStickAxis(float value) {
  const float deadzone = 0.05f;
  if (value > 1.0f) value = 1.0f;
  if (value < -1.0f) value = -1.0f;
  float magnitude = fabsf(value);
  if (magnitude <= deadzone) return 0.0f;
  float normalized = (magnitude - deadzone) / (1.0f - deadzone);
  return value < 0.0f ? -normalized : normalized;
}

void chipnomadSetLiveStickAxes(float leftVertical, float leftHorizontal,
                               float rightVertical, float rightHorizontal) {
  const float axes[4] = {leftVertical, leftHorizontal, rightVertical, rightHorizontal};
  for (int i = 0; i < 4; ++i)
    liveStickAxes[i].store((int32_t)(deadzoneStickAxis(axes[i]) * 1000000.0f), std::memory_order_relaxed);
}

void chipnomadSetLiveStickEnabled(int enabled) { liveStickEnabled.store(enabled ? 1 : 0, std::memory_order_relaxed); }

void chipnomadSetMotionRecordMode(int record, int erase) {
  int mode = erase ? 2 : record ? 1 : 0;
  int previousMode = motionRecordMode.exchange(mode, std::memory_order_relaxed);
  if (previousMode == 1 && mode != 1) motionRecordRateReset.store(1, std::memory_order_relaxed);
  if (!record && !erase) motionRecordOverflow.store(0, std::memory_order_relaxed);
}

int chipnomadConsumeMotionRecordDirty(void) { return motionRecordDirty.exchange(0, std::memory_order_relaxed); }
int chipnomadGetMotionRecordOverflow(void) { return motionRecordOverflow.load(std::memory_order_relaxed); }
float chipnomadLiveStickAxis(int axis) { return liveStickAxes[axis].load(std::memory_order_relaxed) / 1000000.0f; }
int chipnomadLiveStickIsEnabled(void) { return liveStickEnabled.load(std::memory_order_relaxed); }
int chipnomadMotionTakeRateReset(void) { return motionRecordRateReset.exchange(0, std::memory_order_relaxed); }
int chipnomadMotionRateResetPending(void) { return motionRecordRateReset.load(std::memory_order_relaxed); }
int chipnomadMotionMode(void) { return motionRecordMode.load(std::memory_order_relaxed); }
void chipnomadMotionClearOverflow(void) { motionRecordOverflow.store(0, std::memory_order_relaxed); }
void chipnomadMotionSetDirty(void) { motionRecordDirty.store(1, std::memory_order_relaxed); }
void chipnomadMotionSetOverflow(void) { motionRecordOverflow.store(1, std::memory_order_relaxed); }
