#include <cmath>
#include <cstdio>
#include <cstring>
#include "synth/pd_voice.h"

extern "C" {
#include "z_libpd.h"
}

static int parameters;
static int stereo;

static void messageHook(const char*, const char* message, int argc, t_atom* argv) {
  if (!std::strcmp(message, "param") && argc >= 2 && libpd_is_float(argv) && libpd_is_symbol(argv + 1))
    ++parameters;
  if (!std::strcmp(message, "audio") && argc && libpd_is_symbol(argv) && !std::strcmp(libpd_get_symbol(argv), "stereo"))
    stereo = 1;
}

static int exercise(const char* name, int expectedParameters) {
  t_pdinstance* instance = libpd_new_instance();
  if (!instance) return 1;
  libpd_set_instance(instance);
  libpd_set_messagehook(messageHook);
  if (libpd_init_audio(0, 2, 48000)) return 2;
  libpd_add_to_search_path("packaging/common/pd");
  void* binding = libpd_bind("cct-meta");
  void* patch = libpd_openfile(name, "packaging/common/pd");
  if (!patch) return 3;
  libpd_start_message(1); libpd_add_float(1); libpd_finish_message("pd", "dsp");
  parameters = stereo = 0;
  libpd_bang("cct-meta");
  if (parameters < expectedParameters || stereo) return 4;
  libpd_float("cct-note", 69);
  libpd_float("cct-m1", 0.6f);
  libpd_float("cct-gate", 1);
  float output[128] = {};
  double energy = 0;
  for (int block = 0; block < 8; ++block) {
    libpd_process_float(1, NULL, output);
    for (float sample : output) energy += std::fabs(sample);
  }
  libpd_float("cct-gate", 0);
  libpd_closefile(patch);
  libpd_unbind(binding);
  libpd_free_instance(instance);
  return energy > 0.001 ? 0 : 5;
}

int main() {
  if (libpd_init()) return 10;
  if (exercise("Warp Wobble.pd", 3)) return 11;
  if (exercise("Pigeon Laser.pd", 2)) return 12;

  t_pdinstance* missingInstance = libpd_new_instance();
  libpd_set_instance(missingInstance);
  if (libpd_openfile("missing.pd", "packaging/common/pd")) return 13;
  libpd_free_instance(missingInstance);

  t_pdinstance* tracks[2] = {libpd_new_instance(), libpd_new_instance()};
  void* patches[2] = {};
  const char* names[2] = {"Warp Wobble.pd", "Pigeon Laser.pd"};
  for (int i = 0; i < 2; ++i) {
    if (!tracks[i]) return 14;
    libpd_set_instance(tracks[i]);
    if (libpd_init_audio(0, 2, 48000)) return 15;
    libpd_add_to_search_path("packaging/common/pd");
    patches[i] = libpd_openfile(names[i], "packaging/common/pd");
    if (!patches[i]) return 16;
    libpd_start_message(1); libpd_add_float(1); libpd_finish_message("pd", "dsp");
    libpd_float("cct-note", 60 + i * 7);
    libpd_float("cct-m1", 0.5f);
    libpd_float("cct-gate", 1);
  }
  float simultaneousEnergy[2] = {};
  for (int block = 0; block < 8; ++block) for (int i = 0; i < 2; ++i) {
    float output[128] = {};
    libpd_set_instance(tracks[i]);
    libpd_process_float(1, NULL, output);
    for (float sample : output) simultaneousEnergy[i] += std::fabs(sample);
  }
  for (int i = 0; i < 2; ++i) {
    libpd_set_instance(tracks[i]);
    libpd_closefile(patches[i]);
    libpd_free_instance(tracks[i]);
    if (simultaneousEnergy[i] <= 0.001f) return 17;
  }

  uint8_t macros[8] = {};
  float output[1024] = {};
  PDVoice vco;
  vco.init(48000);
  if (!vco.load("packaging/common/pd/Warp Wobble.pd")) {
    std::printf("PDVoice load error:");
    for (const unsigned char* c = (const unsigned char*)vco.lastError(); *c; ++c) std::printf(" %02x", *c);
    std::puts(""); return 18;
  }
  if (std::strcmp(vco.macroName(0), "Warp")) return 19;
  vco.configure(macros, true, false, 1);
  vco.setPost(false, 0, 0, false, 20000, 0, 0, 0, 1, 0.002f, 128);
  vco.noteOn(69);
  vco.render(output, 512);
  vco.noteOff();
  for (int i = 0; i < 20 && vco.active(); ++i) vco.render(output, 512);
  if (vco.active()) return 20;

  PDVoice mono;
  mono.init(48000);
  if (!mono.load("packaging/common/pd/Pigeon Laser.pd")) return 21;
  mono.configure(macros, false, mono.stereo(), 1);
  mono.noteOn(69);
  mono.render(output, 512);
  for (int i = 0; i < 512; ++i) if (output[i * 2] != output[i * 2 + 1]) return 22;

  PDVoice stereoVoice;
  stereoVoice.init(48000);
  if (!stereoVoice.load("tests/pd_stereo_test.pd") || !stereoVoice.stereo()) return 23;
  stereoVoice.configure(macros, false, true, 1);
  stereoVoice.noteOn(60);
  stereoVoice.render(output, 512);
  bool channelsDiffer = false;
  for (int i = 0; i < 512; ++i) if (output[i * 2] != output[i * 2 + 1]) channelsDiffer = true;
  if (!channelsDiffer) return 24;

  PDVoice badExternal;
  badExternal.init(48000);
  if (badExternal.load("tests/pd_bad_external_test.pd") || !badExternal.lastError()[0]) return 25;
  std::puts("libpd smoke test: OK");
  return 0;
}
