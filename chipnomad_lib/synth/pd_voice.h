#ifndef CHOOCHOO_PD_VOICE_H
#define CHOOCHOO_PD_VOICE_H

#include <stddef.h>
#include <stdint.h>
#include "voice_post_processor.h"

class PDVoice {
 public:
  PDVoice();
  ~PDVoice();
  void init(float sampleRate);
  bool load(const char* path);
  void configure(const uint8_t macros[8], bool vco, bool stereo, float gain);
  void setPost(bool enabled, uint8_t character, uint8_t mode, bool slope24,
               float cutoff, float resonance, float attack, float decay,
               float sustain, float release, uint8_t shape);
  void setNote(float midiNote) { note_ = midiNote; if (active_) sendNote(); }
  void noteOn();
  void noteOff();
  void kill();
  void render(float* output, size_t frames);
  bool active() const { return active_; }
  bool loaded() const { return loaded_; }
  float envelopeLevel() const { return vco_ ? post_.envelopeLevel() : (active_ ? 1.0f : 0.0f); }
  const char* macroName(int index) const;
  const char* lastError() const { return error_; }
  bool stereo() const { return stereo_; }

 private:
  void close();
  void sendNote();
  void sendFloat(const char* receiver, float value);
  void renderBlock();
  static void messageHook(const char* receiver, const char* message, int argc, void* argv);
  static void printHook(const char* message);
  float sampleRate_, gain_, note_ = 60;
  bool loaded_, active_, vco_, stereo_;
  void* instance_;
  void* patch_;
  void* binding_;
  float block_[128];
  size_t blockPosition_;
  char path_[256];
  char error_[128];
  char macroNames_[8][16];
  VoicePostProcessor<> post_;
};

#endif
