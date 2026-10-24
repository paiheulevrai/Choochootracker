#pragma once
#include "../sid_patch.h"
#include "../external/chips/m6581.h"
#include "native_fm_amp.h"
#include "../chip_program.h"
#include <cstddef>
// One independent note/filter context. Quiet/released notes are retired first
// by the same bounded policy as other CPU-heavy native instruments.
class SIDVoice {
 public:
  void init(float rate);void configure(const InstrumentSID* patch,float cents,float gain);
  void noteOn();void noteOff();void kill();void render(float* mono,size_t frames);
  bool active()const{return active_;}bool releasing()const{return !gated_;}
  unsigned cost()const{return patch_.value[sidRing]||patch_.value[sidSync]?2:1;}
  bool fresh()const{return fresh_;}
  float envelopeLevel()const{return active_?level_:0;}
 private:
  void write(int reg,int value);void program();float clock();
  m6581_t chip_{};InstrumentSID patch_{};NativeFMAmp amp_;
  float rate_=48000,cents_=6000,level_=0,previous_=0,dc_=0,dcPole_=0;
  double cyclePhase_=0,macroPhase_=0;
  unsigned macroFrame_=0,silent_=0;
  int registers_[25]{};
  bool active_=false,gated_=false,configured_=false,fresh_=false;
  ChipMacroPlayer macros_;
  GoatProgramPlayer goat_;
};
unsigned limitSIDVoices(SIDVoice* const* voices,size_t count,unsigned limit);
