#include "monk_patch.h"

const MonkControl monkControls[monkParameterCount] = {
  {"Vowel", fxMWV, 0, 255}, {"Voice", fxMIN, 0, 255},
  {"Breath", fxMAM, 0, 255}, {"Glide", fxMFL, 0, 255},
  {"Vib depth", fxMFB, 0, 255}, {"Vib rate", fxMSH, 0, 255},
  {"Attack", fxEAT, 0, 255}, {"Decay", fxEDC, 0, 255},
  {"Sustain", fxESU, 0, 255}, {"Release", fxERL, 0, 255},
  {"Voices", fxMUV, 1, 10}, {"Detune", fxMDT, 0, 255},
  {"Spread", fxMVS, 0, 255}, {"Delay mix", fxMDM, 0, 255},
  {"Delay rate", fxMDR, 0, 255}, {"Level", fxMLV, 0, 255},
};
uint8_t* monkParameter(InstrumentMonk* p, int parameter) {
  if (!p || parameter < 0 || parameter >= monkParameterCount) return nullptr;
  uint8_t* values[] = {&p->vowel, &p->character, &p->breath, &p->glide, &p->vibrato, &p->rate,
    &p->attack, &p->decay, &p->sustain, &p->release, &p->unison, &p->detune,
    &p->voiceSpread, &p->delayMix, &p->delayRate, &p->level};
  return values[parameter];
}
int monkValue(const InstrumentMonk& patch, int parameter) {
  // The lookup does not mutate the patch; keep the member mapping in one place.
  const auto* value = monkParameter(const_cast<InstrumentMonk*>(&patch), parameter);
  return value ? *value : 0;
}
