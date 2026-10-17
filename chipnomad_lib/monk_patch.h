#pragma once
#include "project.h"

enum MonkBank : uint8_t { monkBankUser = 0, monkBankFactory = 1, monkBankAll = 2 };

enum MonkParameter {
  monkVowel, monkVoice, monkBreath, monkGlide, monkVibrato, monkVibRate,
  monkAttack, monkDecay, monkSustain, monkRelease,
  monkUnison, monkDetune, monkSpread, monkDelayMix, monkDelayRate, monkLevel,
  monkParameterCount
};
struct MonkControl { const char* name; FXId fx; int minimum, maximum; };
extern const MonkControl monkControls[monkParameterCount];
uint8_t* monkParameter(InstrumentMonk* patch, int parameter);
int monkValue(const InstrumentMonk& patch, int parameter);
