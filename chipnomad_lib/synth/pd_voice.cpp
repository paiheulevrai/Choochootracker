#include "pd_voice.h"
#include <stdio.h>
#include <string.h>

#ifdef PD_ENABLED
extern "C" {
#include "z_libpd.h"
}
#endif

#ifdef PD_ENABLED
static PDVoice* pdMetadataTarget = NULL;
#endif

PDVoice::PDVoice() : sampleRate_(48000), gain_(1), loaded_(false), active_(false), vco_(false), stereo_(false), instance_(NULL), patch_(NULL), binding_(NULL), blockPosition_(64) {
  path_[0] = 0;
  error_[0] = 0;
  for (int i = 0; i < 8; ++i) snprintf(macroNames_[i], sizeof(macroNames_[i]), "P%d", i + 1);
  memset(block_, 0, sizeof(block_));
}
PDVoice::~PDVoice() { close(); }

void PDVoice::init(float sampleRate) { sampleRate_ = sampleRate > 0 ? sampleRate : 48000; post_.init(sampleRate_); }

void PDVoice::close() {
#ifdef PD_ENABLED
  if (instance_) {
    libpd_set_instance((t_pdinstance*)instance_);
    if (binding_) libpd_unbind(binding_);
    if (patch_) libpd_closefile(patch_);
    libpd_free_instance((t_pdinstance*)instance_);
  }
#endif
  instance_ = patch_ = binding_ = NULL; loaded_ = active_ = false; path_[0] = 0;
}

bool PDVoice::load(const char* path) {
  if (!path || !path[0]) { close(); return false; }
  if (loaded_ && strcmp(path_, path) == 0) return true;
  close(); error_[0] = 0;
#ifdef PD_ENABLED
  static bool initialized = false;
  if (!initialized) { libpd_init(); initialized = true; }
  instance_ = libpd_new_instance();
  if (!instance_) { strcpy(error_, "Cannot create libpd instance"); return false; }
  libpd_set_instance((t_pdinstance*)instance_);
  libpd_set_messagehook((t_libpd_messagehook)messageHook);
  libpd_set_printhook((t_libpd_printhook)printHook);
  if (libpd_init_audio(0, 2, (int)sampleRate_) != 0) { strcpy(error_, "Cannot initialize libpd audio"); close(); return false; }
  libpd_add_to_search_path("pd");
  libpd_add_to_search_path("packaging/common/pd");
  char directory[256] = ".", filename[256];
  strncpy(filename, path, sizeof(filename) - 1); filename[255] = 0;
  char* slash = strrchr(filename, '/'); char* backslash = strrchr(filename, '\\');
  if (!slash || (backslash && backslash > slash)) slash = backslash;
  if (slash) { *slash = 0; strncpy(directory, filename, sizeof(directory) - 1); directory[255] = 0; memmove(filename, slash + 1, strlen(slash + 1) + 1); }
  libpd_add_to_search_path(directory);
  pdMetadataTarget = this;
  binding_ = libpd_bind("cct-meta");
  patch_ = libpd_openfile(filename, directory);
  if (!patch_) { strncpy(error_, "Patch file not found", sizeof(error_) - 1); pdMetadataTarget = NULL; close(); return false; }
  libpd_start_message(1); libpd_add_float(1); libpd_finish_message("pd", "dsp");
  libpd_bang("cct-meta"); pdMetadataTarget = NULL;
  if (error_[0]) { close(); return false; }
  strncpy(path_, path, sizeof(path_) - 1); path_[255] = 0; loaded_ = true; blockPosition_ = 64;
  return true;
#else
  (void)path; return false;
#endif
}

void PDVoice::sendFloat(const char* receiver, float value) {
#ifdef PD_ENABLED
  if (loaded_) { libpd_set_instance((t_pdinstance*)instance_); libpd_float(receiver, value); }
#else
  (void)receiver; (void)value;
#endif
}
void PDVoice::configure(const uint8_t macros[8], bool vco, bool stereo, float gain) {
  vco_ = vco; stereo_ = stereo; gain_ = gain; post_.setGain(gain);
  char receiver[] = "cct-m1";
  for (int i = 0; i < 8; ++i) { receiver[5] = (char)('1' + i); sendFloat(receiver, macros[i] / 255.0f); }
}
void PDVoice::setPost(bool enabled, uint8_t character, uint8_t mode, bool slope24, float cutoff, float resonance, float attack, float decay, float sustain, float release, uint8_t shape) {
  post_.setFilter(enabled, character, mode, slope24, cutoff, resonance);
  post_.setEnvelope(true, attack, decay, sustain, release, shape);
}
void PDVoice::sendNote() { sendFloat("cct-note", note_); }
void PDVoice::noteOn() { active_ = true; sendNote(); sendFloat("cct-gate", 1); if (vco_) post_.noteOn(); }
void PDVoice::noteOff() { sendFloat("cct-gate", 0); if (vco_) post_.noteOff(); }
void PDVoice::kill() { sendFloat("cct-gate", 0); active_ = false; post_.kill(); }

void PDVoice::renderBlock() {
  memset(block_, 0, sizeof(block_));
#ifdef PD_ENABLED
  if (loaded_) { libpd_set_instance((t_pdinstance*)instance_); libpd_process_float(1, NULL, block_); }
#endif
  blockPosition_ = 0;
}
void PDVoice::render(float* output, size_t frames) {
  if (!output) return;
  for (size_t i = 0; i < frames; ++i) {
    if (blockPosition_ >= 64) renderBlock();
    float left = block_[blockPosition_ * 2], right = stereo_ ? block_[blockPosition_ * 2 + 1] : left;
    ++blockPosition_;
    if (vco_) { left = post_.process(left); right = left; if (!post_.envelopeActive()) active_ = false; }
    else { left *= gain_; right *= gain_; }
    output[i * 2] = active_ ? left : 0; output[i * 2 + 1] = active_ ? right : 0;
  }
}
const char* PDVoice::macroName(int index) const { return index >= 0 && index < 8 ? macroNames_[index] : ""; }

void PDVoice::printHook(const char* message) {
#ifdef PD_ENABLED
  if (!pdMetadataTarget || !message) return;
  if (strstr(message, "couldn't create") || strstr(message, "can't load") || strstr(message, "no such object")) {
    strncpy(pdMetadataTarget->error_, message, sizeof(pdMetadataTarget->error_) - 1);
    pdMetadataTarget->error_[sizeof(pdMetadataTarget->error_) - 1] = 0;
  }
#else
  (void)message;
#endif
}

void PDVoice::messageHook(const char*, const char* message, int argc, void* raw) {
#ifdef PD_ENABLED
  PDVoice* self = pdMetadataTarget; t_atom* argv = (t_atom*)raw;
  if (!self || !message) return;
  if (strcmp(message, "param") == 0 && argc >= 2 && libpd_is_float(argv) && libpd_is_symbol(argv + 1)) {
    int index = (int)libpd_get_float(argv) - 1;
    if (index >= 0 && index < 8) {
      strncpy(self->macroNames_[index], libpd_get_symbol(argv + 1), 15);
      self->macroNames_[index][15] = 0;
      for (char* c = self->macroNames_[index]; *c; ++c) if (*c == '_') *c = ' ';
    }
  } else if (strcmp(message, "audio") == 0 && argc >= 1 && libpd_is_symbol(argv) && strcmp(libpd_get_symbol(argv), "stereo") == 0) self->stereo_ = true;
#else
  (void)message; (void)argc; (void)raw;
#endif
}
