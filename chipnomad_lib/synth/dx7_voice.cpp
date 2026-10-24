#include "native_chip_gain.h"
#include "dx7_voice.h"
#include "../dx7_patch.h"
#include "../external/msfa/sin.h"
#include "../external/msfa/exp2.h"
#include "../external/msfa/freqlut.h"
#include <mutex>
#include <cstring>
#include "../native_fm_values.h"

namespace {
void macroPatch(const InstrumentDX7& saved,uint8_t* out) {
  memcpy(out,saved.voice,155);
  const auto& t=saved.tone;
  for(int op=0;op<6;++op) {
    auto* p=out+op*21;
    if(t.operatorLevel[5-op])p[16]=t.operatorLevel[5-op]-1;
  }
  nativeFMDX7(t,out);
}
}

void DX7Part::init(float rate) {
  // Shared tables remain immutable at44100 after initialization off callback.
  // This supports independent offline/live renderers at different output rates.
  static std::once_flag tables;
  std::call_once(tables,[]{using namespace choochoo_msfa;Sin::init();Exp2::init();Freqlut::init(44100);Env::init_sr(44100);PitchEnv::init(44100);Lfo::init(44100);});
  for(auto& v:voices)v.amp_.init(44100);
  resampler_.init(44100,rate);configured_=false;lfo_={};kill();
}
void DX7Voice::configure(const InstrumentDX7* patch,float cents,float gain) {
  if(!patch||!validDX7(*patch))return;
  bool changed=!configured_||memcmp(patch_.voice,patch->voice,sizeof(patch_.voice));
  patch_=*patch;configured_=true;cents_=std::isfinite(cents)?std::clamp(cents+patch_.fineTune,0.f,14000.f):6000;
  gain_=std::isfinite(gain)?std::clamp(gain,0.f,1.f):0;
  amp_.configure(patch_.amp,gain_);
  if(changed&&gated_)pendingOn_=true;
}
void DX7Voice::noteOn(){if(configured_){pendingOn_=true;pendingOff_=false;active_=true;}}
void DX7Voice::noteOff(){if(active_)pendingOff_=true;}
void DX7Voice::kill(){amp_.kill();active_=gated_=pendingOn_=pendingOff_=false;level_=0;std::memset(block_,0,sizeof(block_));}
bool DX7Voice::applyEvents() {
  bool triggered=pendingOn_;
  if(pendingOn_) {
    baseNote_=std::clamp(int(std::lround(cents_/100)),0,127);
    amp_.noteOn();
    uint8_t effective[155];macroPatch(patch_,effective);
    note_.start(effective,baseNote_+int(patch_.voice[144])-24,patch_.velocity);
    directCache_=patch_.tone.direct;
    memcpy(levelCache_,patch_.tone.operatorLevel,6);
    pendingOn_=false;active_=gated_=true;
  }
  if(pendingOff_){amp_.noteOff();note_.keyup();gated_=false;pendingOff_=false;}
  return triggered;
}
void DX7Voice::compute(int32_t lfo,int32_t delay) {
  std::memset(block_,0,sizeof(block_));
  if(!active_)return;
  if(!gated_&&!note_.playing()){active_=false;return;}
  if(memcmp(&directCache_,&patch_.tone.direct,sizeof(directCache_))||memcmp(levelCache_,patch_.tone.operatorLevel,6)) {
    uint8_t effective[155];macroPatch(patch_,effective);
    note_.updateTimbre(effective,baseNote_+int(patch_.voice[144])-24);
    directCache_=patch_.tone.direct;
    memcpy(levelCache_,patch_.tone.operatorLevel,6);
  }
  int32_t pitch=int32_t(std::lround((cents_-baseNote_*100.f)*16777216.0/1200));
  note_.compute(block_,lfo,delay,pitch,patch_.tone.brightness,patch_.tone.feedback?patch_.tone.feedback-1:-1);
}
void DX7Part::kill(){for(auto& v:voices)v.kill();cursor_=64;resampler_.reset();}
bool DX7Part::active()const{for(const auto& v:voices)if(v.active())return true;return false;}
float DX7Part::envelopeLevel()const{float sum=0;for(const auto& v:voices)sum+=v.envelopeLevel();return sum;}
void DX7Part::native(float& left,float& right) {
  if(cursor_==64) {
    bool trigger=false;
    for(auto& v:voices) {
      if(v.active_) {
        uint8_t parameters[6];memcpy(parameters,v.patch_.voice+137,6);
        parameters[0]=nativeFMValue(v.patch_.tone,0,fxLFR,parameters[0]);
        if(!configured_||memcmp(lfoParameters_,parameters,6)) {
          memcpy(lfoParameters_,parameters,6);lfo_.reset(lfoParameters_);configured_=true;
        }
      }
      trigger|=v.applyEvents();
    }
    if(trigger)lfo_.keydown(); // Once per part/quantum, never once per operator.
    int32_t value=lfo_.getsample(),delay=lfo_.getdelay();
    for(auto& v:voices)v.compute(value,delay);
    cursor_=0;
  }
  left=0;
  for(auto& v:voices) {
    float sample=v.active_?v.amp_.process(v.block_[cursor_]/16777216.f*.18f):0;
    left+=sample;v.level_=std::max(std::abs(sample),v.level_*.999f);
  }
  right=left;++cursor_;
}
void DX7Part::render(float* output,size_t frames) {
  // Advance the shared clock even through empty slots; arbitrary caller blocks
  // cannot reset LFO phase or cause extra envelope steps.
  for(size_t i=0;i<frames;++i){float l,r;resampler_.next([&](float& a,float& b){native(a,b);},l,r);output[i]=l*nativeChipGain(InstrumentType::DX7);}
}

unsigned limitDX7Voices(DX7Part* const* parts, size_t count, unsigned limit) {
  unsigned active = 0, stolen = 0;
  for (size_t track = 0; track < count; ++track)
    if (parts[track]) for (const auto& voice : parts[track]->voices)
      active += voice.active();
  while (active > limit) {
    DX7Voice* victim = nullptr;
    int priority = 3, selectedSlot = -1;
    size_t selectedTrack = 0;
    float level = 0;
    for (size_t track = 0; track < count; ++track) {
      if (!parts[track]) continue;
      for (int slot = 0; slot < CHORD_MAX_VOICES; ++slot) {
        auto& voice = parts[track]->voices[slot];
        if (!voice.active()) continue;
        const int candidate = voice.releasing() ? 0 : voice.newlyTriggered() ? 2 : 1;
        const float amplitude = voice.envelopeLevel();
        // Release tails first, then quiet held notes, then fresh attacks.
        // Equal attacks retain roots across tracks before chord extensions.
        if (!victim || candidate < priority ||
            (candidate == priority && (amplitude < level ||
             (amplitude == level && (slot > selectedSlot ||
              (slot == selectedSlot && track > selectedTrack)))))) {
          victim = &voice;
          priority = candidate;
          level = amplitude;
          selectedSlot = slot;
          selectedTrack = track;
        }
      }
    }
    if (!victim) break;
    victim->kill();
    --active;
    ++stolen;
  }
  return stolen;
}
