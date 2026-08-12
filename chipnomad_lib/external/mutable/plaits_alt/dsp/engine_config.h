// Generated from lylepmills/eurorack catalog; Plaits-Alt only.
#ifndef PLAITS_ALT_GUARD_PLAITS_ALT_DSP_ENGINE_CONFIG_H_
#define PLAITS_ALT_GUARD_PLAITS_ALT_DSP_ENGINE_CONFIG_H_

#include "plaits_alt/dsp/engine2/glisson_engine.h"
#include "plaits_alt/dsp/engine2/pulsar_engine.h"
#include "plaits_alt/dsp/engine2/gendy_engine.h"
#include "plaits_alt/dsp/engine2/scanned_engine.h"
#include "plaits_alt/dsp/engine2/loopback_engine.h"
#include "plaits_alt/dsp/engine2/phase_weave_engine.h"
#include "plaits_alt/dsp/engine2/sideband_engine.h"
#include "plaits_alt/dsp/engine2/undertow_engine.h"
#include "plaits_alt/dsp/engine2/attractor_engine.h"
#include "plaits_alt/dsp/engine2/lockstep_engine.h"
#include "plaits_alt/dsp/engine2/reed_pipe_engine.h"
#include "plaits_alt/dsp/engine2/brass_engine.h"
#include "plaits_alt/dsp/engine2/shakers_engine.h"
#include "plaits_alt/dsp/engine2/clap_engine.h"
#include "plaits_alt/dsp/engine2/freshets_formant_engine.h"
#include "plaits_alt/dsp/engine2/diatonic_chord_engine.h"
#include "plaits_alt/dsp/engine2/scale_stack_engine.h"
#include "plaits_alt/dsp/engine2/wavetable_chord_engine.h"
#include "plaits_alt/dsp/engine2/wavetable_scale_stack_engine.h"
#include "plaits_alt/dsp/engine2/helix_engine.h"
#include "plaits_alt/dsp/engine2/bytebeat_engine.h"
#include "plaits_alt/dsp/engine2/rulefield_engine.h"
#include "plaits_alt/dsp/engine2/spectral_spiral_engine.h"
#include "plaits_alt/dsp/engine2/phase_flock_engine.h"

#define PLAITS_HAS_SPEECH_ENGINE 0
#define PLAITS_HAS_LPC_WORDS_ENGINE 0
#define PLAITS_HAS_CHIPTUNE_ENGINE 0
#define PLAITS_HAS_USER_DATA_BANK 0
#define PLAITS_HAS_USER_DATA_BANK_OVERRIDE 0

#define PLAITS_ENGINE_MEMBERS \
  GlissonEngine glisson_engine_; \
  PulsarEngine pulsar_engine_; \
  GendyEngine gendy_engine_; \
  ScannedEngine scanned_engine_; \
  LoopbackEngine loopback_engine_; \
  PhaseWeaveEngine phase_weave_engine_; \
  SidebandEngine sideband_engine_; \
  UndertowEngine undertow_engine_; \
  AttractorEngine attractor_engine_; \
  LockstepEngine lockstep_engine_; \
  ReedPipeEngine reed_pipe_engine_; \
  BrassEngine brass_engine_; \
  ShakersEngine shakers_engine_; \
  ClapEngine clap_engine_; \
  FreshetsFormantEngine freshets_formant_engine_; \
  DiatonicChordEngine diatonic_chord_engine_; \
  ScaleStackEngine scale_stack_engine_; \
  WavetableChordEngine wavetable_chord_engine_; \
  WavetableScaleStackEngine wavetable_scale_stack_engine_; \
  HelixEngine helix_engine_; \
  BytebeatEngine bytebeat_engine_; \
  RulefieldEngine rulefield_engine_; \
  SpectralSpiralEngine spectral_spiral_engine_; \
  PhaseFlockEngine phase_flock_engine_;

#define PLAITS_REGISTER_ENGINES(registry) do { \
  (registry).RegisterInstance(&glisson_engine_, false, 0.9f, 0.9f); \
  (registry).RegisterInstance(&pulsar_engine_, false, 0.9f, 0.9f); \
  (registry).RegisterInstance(&gendy_engine_, false, 0.8f, 0.8f); \
  (registry).RegisterInstance(&scanned_engine_, false, -1.0f, -1.0f); \
  (registry).RegisterInstance(&loopback_engine_, false, 0.8f, 0.8f); \
  (registry).RegisterInstance(&phase_weave_engine_, false, 0.7f, 0.7f); \
  (registry).RegisterInstance(&sideband_engine_, false, 0.7f, 0.7f); \
  (registry).RegisterInstance(&undertow_engine_, false, 0.7f, 0.7f); \
  (registry).RegisterInstance(&attractor_engine_, false, 0.8f, 0.8f); \
  (registry).RegisterInstance(&lockstep_engine_, false, 0.8f, 0.8f); \
  (registry).RegisterInstance(&reed_pipe_engine_, false, 0.8f, 0.8f); \
  (registry).RegisterInstance(&brass_engine_, false, -1.0f, 0.8f); \
  (registry).RegisterInstance(&shakers_engine_, true, 0.8f, 0.8f); \
  (registry).RegisterInstance(&clap_engine_, true, 0.8f, 0.8f); \
  (registry).RegisterInstance(&freshets_formant_engine_, false, 0.9f, 0.55f); \
  (registry).RegisterInstance(&diatonic_chord_engine_, false, -0.65f, -0.65f); \
  (registry).RegisterInstance(&scale_stack_engine_, false, -0.6f, -0.6f); \
  (registry).RegisterInstance(&wavetable_chord_engine_, false, -0.65f, -0.65f); \
  (registry).RegisterInstance(&wavetable_scale_stack_engine_, false, -0.6f, -0.6f); \
  (registry).RegisterInstance(&helix_engine_, false, 0.8f, 0.8f); \
  (registry).RegisterInstance(&bytebeat_engine_, false, -0.9f, -0.9f); \
  (registry).RegisterInstance(&rulefield_engine_, false, 0.7f, 0.7f); \
  (registry).RegisterInstance(&spectral_spiral_engine_, false, 0.7f, 0.7f); \
  (registry).RegisterInstance(&phase_flock_engine_, false, 0.7f, 0.7f); \
} while (0)

#endif  // PLAITS_ALT_DSP_ENGINE_CONFIG_H_

