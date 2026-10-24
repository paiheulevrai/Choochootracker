#include "fm_amp.h"
#include <cstring>

int loadFMAmpSetting(const char* line, InstrumentFMAmp& amp, bool& seen) {
  if (strncmp(line, "- FM amp: ", 10)) return 0;
  int v[6]; char tail;
  if (seen || sscanf(line + 10, "%d,%d,%d,%d,%d,%d %c",
      &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &tail) != 6) return -1;
  for (int i = 0; i < 6; ++i) if (v[i] < 0 || v[i] > (i ? 255 : 1)) return -1;
  amp = {};
  amp.enabled = v[0]; amp.attack = v[1]; amp.decay = v[2];
  amp.sustain = v[3]; amp.release = v[4]; amp.envelopeShape = v[5];
  seen = true;
  return 1;
}

void saveFMAmpSetting(FILE* file, const InstrumentFMAmp& amp) {
  if (!(amp.enabled || amp.attack || amp.decay || amp.sustain || amp.release || amp.envelopeShape)) return;
  fprintf(file, "- FM amp: %u,%u,%u,%u,%u,%u\n", amp.enabled, amp.attack,
          amp.decay, amp.sustain, amp.release, amp.envelopeShape);
}

int loadFMToneSetting(const char* line, InstrumentFMTone& tone, bool& seen) {
  if (strncmp(line, "- FM tone: ", 11)) return 0;
  int brightness, feedback; char tail;
  if (seen || sscanf(line + 11, "%d,%d %c", &brightness, &feedback, &tail) != 2 ||
      brightness < -63 || brightness > 63 || feedback < 0 || feedback > 8) return -1;
  tone.brightness = brightness; tone.feedback = feedback; seen = true;
  return 1;
}
void saveFMToneSetting(FILE* file, const InstrumentFMTone& tone) {
  if (tone.brightness || tone.feedback)
    fprintf(file, "- FM tone: %d,%u\n", tone.brightness, tone.feedback);
}
