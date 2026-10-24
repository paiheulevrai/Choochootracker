#pragma once
#include "native_fm_amp.h"
#include "../project_instruments.h"
#include "../chord.h"
#include "../external/msfa/note.h"
#include "../external/msfa/lfo.h"
#include "native_resampler.h"
class DX7Part;
class DX7Voice {
 public:
  void configure(const InstrumentDX7* patch,float cents,float gain);
  void noteOn(); void noteOff(); void kill();
  bool active() const {return active_;}
  float envelopeLevel() const {return active_?level_:0;}
  bool releasing() const {return active_ && (pendingOff_ || (!gated_ && !pendingOn_));}
  bool newlyTriggered() const {return pendingOn_;}
 private:
  friend class DX7Part;
  bool applyEvents();
  void compute(int32_t lfo,int32_t delay);
  choochoo_msfa::Note note_;
  NativeFMAmp amp_;
  InstrumentDX7 patch_{};
  NativeFMValues directCache_{};
  uint8_t levelCache_[6]{};
  int32_t block_[64]{};
  int baseNote_=60;
  float cents_=6000,gain_=1,level_=0;
  bool configured_=false,active_=false,gated_=false,pendingOn_=false,pendingOff_=false;
};
// One patch/track part owns one LFO and four independently gated chord voices.
// All slots share a quantum clock. Event changes apply at the next 64-frame
// native boundary without discarding buffered samples (maximum 1.45 ms).
class DX7Part {
 public:
  void init(float rate);
  void render(float* mono,size_t frames);
  void kill();
  bool active() const;
  float envelopeLevel() const;
  DX7Voice voices[CHORD_MAX_VOICES];
 private:
  void native(float& l,float& r);
  choochoo_msfa::Lfo lfo_{};
  NativeResampler resampler_;
  uint8_t lfoParameters_[6]{};
  bool configured_=false;
  int cursor_=64;
};

// Measured on R36H: leave headroom for the track effects, UI and other engines.
// The same policy on every platform keeps project playback predictable.
constexpr unsigned DX7_MAX_ACTIVE_VOICES = 16;
unsigned limitDX7Voices(DX7Part* const* parts, size_t count,
                       unsigned limit = DX7_MAX_ACTIVE_VOICES);
