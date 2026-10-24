#pragma once
#include "../project_instruments.h"
#include "../external/emu76489/emu76489.h"
#include "../external/gb_apu/gb_apu.h"
#include "native_resampler.h"
#include "voice_post_processor.h"
#include "../chip_program.h"
class SimpleChipVoice {
 public:
  SimpleChipVoice()=default;
  ~SimpleChipVoice(){if(apu_)apu_quit(apu_);}
  SimpleChipVoice(const SimpleChipVoice&)=delete;SimpleChipVoice& operator=(const SimpleChipVoice&)=delete;
  void init(float rate);void configure(InstrumentType type,const InstrumentSimpleChip* patch,float cents,float gain);
  void noteOn();void noteOff();void kill();void render(float* mono,size_t frames);
  bool active()const{return active_;}float envelopeLevel()const{return active_?post_.envelopeLevel():0;}
 private:
  void registers(bool trigger);void gbWrite(unsigned reg,unsigned value){apu_write_io(apu_,reg,value,0);}
  float gbSample();float segaSample();
  SNG sega_{};GbApu* apu_=nullptr;NativeResampler resampler_;VoicePostProcessor<> post_;
  InstrumentType type_=InstrumentType::none;InstrumentSimpleChip patch_{};
  float rate_=48000,cents_=6000,dc_=0,previous_=0,dcPole_=.997f;
  unsigned sequencerRemaining_=8192;
  int lastFrequency_=-1,lastMode_=-1,lastNoise_=-1,lastEnvelope_=-1;
  bool active_=false;
  ChipMacroPlayer macros_;
  double macroPhase_=0;
  bool macroRelease_=false;
  float macroGain_=1;
};
