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
#pragma once
#include "synth.h"
#include "env.h"
#include "pitchenv.h"
#include "fm_core.h"
namespace choochoo_msfa {
// Tracker adaptation of MSFA note assembly. All mutable state is voice owned.
class Note {
 public:
  void start(const uint8_t* patch,int midi,int velocity);
  void compute(int32_t* buffer,int32_t lfo,int32_t delay,int32_t pitchOffset,int brightness=0,int feedback=-1,const int8_t* operatorOffset=nullptr);
  // Update operator rates/frequencies and native LFO depths without retrigger.
  void updateTimbre(const uint8_t* patch,int midi);
  void keyup();
  bool playing();
 private:
  FmCore core_;
  Env env_[6]; PitchEnv pitchenv_{};
  FmOpParams params_[6]{};
  int32_t basepitch_[6]{},feedback_[2]{},ampSensitivity_[6]{};
  uint8_t modes_[6]{};
  int velocity_=0;
  int algorithm_=0,feedbackShift_=16,pitchDepth_=0,pitchSensitivity_=0,ampDepth_=0;
};
}
