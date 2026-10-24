#pragma once
#include "project.h"
#include <algorithm>

inline int nativeFMValue(const InstrumentFMTone& tone,int op,int fx,int base) {
  int v=fx>=fxLFR?tone.direct.global[fx-fxLFR]:tone.direct.operators[op][fx-fxOAR];
  return v?v-1:base;
}
inline void nativeFMDX7(const InstrumentFMTone& t,uint8_t* voice) {
  for(int op=0;op<6;++op) {
    auto* p=voice+(5-op)*21;
    const int fields[]={0,1,2,3,6,20,18,19,17,4,5,7};
    for(int n=0;n<12;++n)p[fields[n]]=nativeFMValue(t,op,fxOAR+n,p[fields[n]]);
  }
  voice[137]=nativeFMValue(t,0,fxLFR,voice[137]);
  voice[140]=nativeFMValue(t,0,fxLAD,voice[140]);
  voice[139]=nativeFMValue(t,0,fxLPD,voice[139]);
  voice[143]=nativeFMValue(t,0,fxLPS,voice[143]);
}
