#ifndef __CHIPNOMAD_LIB_LIVE_STICK_H__
#define __CHIPNOMAD_LIB_LIVE_STICK_H__

float chipnomadLiveStickAxis(int axis);
int chipnomadLiveStickIsEnabled(void);
int chipnomadMotionTakeRateReset(void);
int chipnomadMotionRateResetPending(void);
int chipnomadMotionMode(void);
void chipnomadMotionClearOverflow(void);
void chipnomadMotionSetDirty(void);
void chipnomadMotionSetOverflow(void);

#endif
