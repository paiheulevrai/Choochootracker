#ifndef SNDSTATION_VOLUME_H
#define SNDSTATION_VOLUME_H

void sndstationVolumeStart(void);
void sndstationVolumePoll(void);
bool sndstationVolumeHandlesButtons(void);
bool sndstationVolumeExitRequested(void);
void sndstationVolumeStop(void);

inline long sndstationVolumeStep(long current, long minimum, long ceiling, int direction) {
  if (current > ceiling) current = ceiling;
  if (current < minimum) current = minimum;
  if (direction > 0) return current > ceiling - 150 ? ceiling : current + 150;
  return current < minimum + 150 ? minimum : current - 150;
}

#endif
