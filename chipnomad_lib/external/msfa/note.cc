/*
 * Copyright 2016-2025 Pascal Gauthier.
 * Copyright 2012 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// Modified for ChooChooTracker: namespace isolation and standalone scalar adapter.
// See PATCHES.md for the complete changes and dependency boundary.


// ChooChooTracker adaptation: isolated note state, no app dependencies.
#include <cmath>
#include "note.h"
#include "freqlut.h"
namespace choochoo_msfa {
const int32_t coarsemul[] = {
    -16777216, 0, 16777216, 26591258, 33554432, 38955489, 43368474, 47099600,
    50331648, 53182516, 55732705, 58039632, 60145690, 62083076, 63876816,
    65546747, 67108864, 68576247, 69959732, 71268397, 72509921, 73690858,
    74816848, 75892776, 76922906, 77910978, 78860292, 79773775, 80654032,
    81503396, 82323963, 83117622
};

int32_t midinote_to_logfreq(int midinote) {
  const int base = 50857777;  // (1 << 24) * (log(440) / log(2) - 69/12)
  const int step = (1 << 24) / 12;
  return base + step * midinote;
}

int32_t osc_freq(int midinote, int mode, int coarse, int fine, int detune) {
  // TODO: pitch randomization
  int32_t logfreq;
  if (mode == 0) {
    logfreq = midinote_to_logfreq(midinote);
    double detuneRatio = 0.0209 * exp(-0.396 * (double(logfreq)/(1<<24))) / 7;
    logfreq += detuneRatio * logfreq * (detune - 7);
    logfreq += coarsemul[coarse & 31];
    if (fine) {
      // (1 << 24) / log(2)
      logfreq += (int32_t)floor(24204406.323123 * log(1 + 0.01 * fine) + 0.5);
    }
    // This was measured at 7.213Hz per count at 9600Hz, but the exact
    // value is somewhat dependent on midinote. Close enough for now.

  } else {
    // ((1 << 24) * log(10) / log(2) * .01) << 3
    logfreq = (4458616 * ((coarse & 3) * 100 + fine)) >> 3;
    logfreq += detune > 7 ? 13457 * (detune - 7) : 0;
  }
  return logfreq;
}

const uint8_t velocity_data[64] = {
    0, 70, 86, 97, 106, 114, 121, 126, 132, 138, 142, 148, 152, 156, 160, 163,
    166, 170, 173, 174, 178, 181, 184, 186, 189, 190, 194, 196, 198, 200, 202,
    205, 206, 209, 211, 214, 216, 218, 220, 222, 224, 225, 227, 229, 230, 232,
    233, 235, 237, 238, 240, 241, 242, 243, 244, 246, 246, 248, 249, 250, 251,
    252, 253, 254
};

// See "velocity" section of notes. Returns velocity delta in microsteps.
int ScaleVelocity(int velocity, int sensitivity) {
    int clamped_vel = max(0, min(127, velocity));
    int vel_value = velocity_data[clamped_vel >> 1] - 239;
    int scaled_vel = ((sensitivity * vel_value + 7) >> 3) * 16;
    return scaled_vel;
}

int ScaleRate(int midinote, int sensitivity) {
    int x = min(31, max(0, midinote / 3 - 7));
    int qratedelta = (sensitivity * x) >> 3;
#ifdef SUPER_PRECISE
    int rem = x & 7;
    if (sensitivity == 3 && rem == 3) {
        qratedelta -= 1;
    } else if (sensitivity == 7 && rem > 0 && rem < 4) {
        qratedelta += 1;
    }
#endif
    return qratedelta;
}

const uint8_t exp_scale_data[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 14, 16, 19, 23, 27, 33, 39, 47, 56, 66,
    80, 94, 110, 126, 142, 158, 174, 190, 206, 222, 238, 250
};

int ScaleCurve(int group, int depth, int curve) {
    int scale;
    if (curve == 0 || curve == 3) {
        // linear
        scale = (group * depth * 329) >> 12;
    } else {
        // exponential
        int n_scale_data = sizeof(exp_scale_data);
        int raw_exp = exp_scale_data[min(group, n_scale_data - 1)];
        scale = (raw_exp * depth * 329) >> 15;
    }
    if (curve < 2) {
        scale = -scale;
    }
    return scale;
}

int ScaleLevel(int midinote, int break_pt, int left_depth, int right_depth,
               int left_curve, int right_curve) {
    int offset = midinote - break_pt - 17;
    if (offset >= 0) {
        return ScaleCurve((offset+1) / 3, right_depth, right_curve);
    } else {
        return ScaleCurve(-(offset-1) / 3, left_depth, left_curve);
    }
}

static const uint8_t pitchmodsenstab[] = {
    0, 10, 20, 33, 55, 92, 153, 255
};

// 0, 66, 109, 255
static const uint32_t ampmodsenstab[] = {
    0, 4342338, 7171437, 16777216
};


void Note::start(const uint8_t* patch,int midi,int velocity) {
  velocity_=velocity;
  for(int op=0;op<6;++op) {
    const uint8_t* p=patch+op*21; int rates[4],levels[4];
    for(int j=0;j<4;++j){rates[j]=p[j];levels[j]=p[j+4];}
    int level=Env::scaleoutlevel(p[16])+ScaleLevel(midi,p[8],p[9],p[10],p[11],p[12]);
    level=max(0,min(127,level))*32+ScaleVelocity(velocity,p[15]);
    env_[op].init(rates,levels,max(0,level),ScaleRate(midi,p[13]));
    modes_[op]=p[17];basepitch_[op]=osc_freq(midi,p[17],p[18],p[19],p[20]);
    ampSensitivity_[op]=ampmodsenstab[p[14]];
    if(patch[136])params_[op].phase=0;
    params_[op].gain_out=0;
  }
  int rates[4],levels[4];for(int j=0;j<4;++j){rates[j]=patch[126+j];levels[j]=patch[130+j];}
  pitchenv_.set(rates,levels);algorithm_=patch[134];
  feedbackShift_=patch[135]?8-patch[135]:16;feedback_[0]=feedback_[1]=0;
  pitchDepth_=(patch[139]*165)>>6;pitchSensitivity_=pitchmodsenstab[patch[143]];
  ampDepth_=(patch[140]*165)>>6;
}
void Note::compute(int32_t* buffer,int32_t lfo,int32_t delay,int32_t pitchOffset,int brightness,int feedback,const int8_t* operatorOffset) {
  uint32_t depth=pitchDepth_*uint32_t(delay);
  int32_t sensitivity=pitchSensitivity_*(lfo-(1<<23));
  int32_t pitch=pitchenv_.getsample()+int32_t((int64_t(depth)*sensitivity)>>39);
  uint32_t amplitude=uint32_t((int64_t(ampDepth_)*delay)>>8);
  amplitude=uint32_t((int64_t(amplitude)*((1<<24)-lfo))>>24);
  for(int op=0;op<6;++op) {
    params_[op].freq=Freqlut::lookup(basepitch_[op]+pitchOffset+(modes_[op]?0:pitch));
    int32_t level=env_[op].getsample();
    if(ampSensitivity_[op]) {
      uint32_t sensitivity=uint32_t((uint64_t(amplitude)*ampSensitivity_[op])>>24);
      // MSFA's DX7 amplitude-modulation response; see upstream dx7note.cc.
      uint32_t response=std::exp(float(sensitivity)/262144*.07+12.2);
      level-=int32_t((uint64_t(level)*(uint64_t(response)<<4))>>28);
    }
    if(brightness && level>0 && !FmCore::isCarrier(algorithm_,op))
      level=max(0,min(17*(1<<24),level+max(-63,min(63,brightness))*(1<<21)));
    // User-facing OP1..OP6 reverses canonical Yamaha OP6..OP1 order.
    if(operatorOffset && operatorOffset[5-op] && level>0) {
      int offset=operatorOffset[5-op];
      level=int32_t(max(int64_t(0),min(int64_t(17)*(1<<24),int64_t(level)+int64_t(offset)*(1<<21))));
    }
    params_[op].level_in=level;
  }
  int shift=feedback<0?feedbackShift_:feedback?8-min(7,feedback):16;
  core_.render(buffer,params_,algorithm_,feedback_,shift);
}
void Note::updateTimbre(const uint8_t* patch,int midi) {
  for(int op=0;op<6;++op) {
    const uint8_t* p=patch+op*21;
    int rates[4];for(int j=0;j<4;++j)rates[j]=p[j];
    env_[op].setRates(rates);
    int level=Env::scaleoutlevel(p[16])+ScaleLevel(midi,p[8],p[9],p[10],p[11],p[12]);
    level=max(0,min(127,level))*32+ScaleVelocity(velocity_,p[15]);
    env_[op].setOutputLevel(max(0,level));
    basepitch_[op]=osc_freq(midi,p[17],p[18],p[19],p[20]);
  }
  pitchDepth_=(patch[139]*165)>>6;pitchSensitivity_=pitchmodsenstab[patch[143]];
  ampDepth_=(patch[140]*165)>>6;
}
void Note::keyup(){for(auto& e:env_)e.keydown(false);pitchenv_.keydown(false);}
bool Note::playing(){for(int i=0;i<6;++i)if(FmCore::isCarrier(algorithm_,i)&&env_[i].isActive())return true;return false;}

}
