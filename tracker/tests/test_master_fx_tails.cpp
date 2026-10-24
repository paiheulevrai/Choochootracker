#include "doctest.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "synth/master_effects.h"
#include <cmath>
#include <memory>
#include <vector>

// Renders one AY track: a note on row 0 (optionally with a per-note send FX)
// and a second note on row 2 without it. The mixer sends stay at zero, so the
// only send into the effect is the per-note FX on the first note.
using State = std::unique_ptr<ChipNomadState, decltype(&chipnomadDestroy)>;

static State startTwoNotes(FX sendFX, bool withSend) {
  State s(chipnomadCreate(), chipnomadDestroy);
  REQUIRE(projectLoad(&s->project, "packaging/common/projects/gm-midi-demo.cct") == 0);
  Project& p = s->project;
  p.tracksCount = 1;
  p.tickRate = 50;
  p.linearPitch = 1;
  calculateLinearPitchTable12TET(&p);
  for (auto& g : p.grooves) for (auto& v : g.speed) v = 6;
  for (int t = 0; t < PROJECT_MAX_TRACKS; ++t) p.trackReverbSend[t] = p.trackDelaySend[t] = 0;
  p.delayReverbSend = 0;
  p.reverbReturn = 100;
  p.reverbTime = 220;
  p.delayReturn = 100;
  p.delayFeedback = 70;
  p.delayTicks = 3;
  p.song[0][0] = 0;
  p.chains[0].rows[0].phrase = 0;
  p.chains[0].rows[0].transpose = 0;
  phraseClear(&p.phrases[0]);
  getInstrumentFunctions(InstrumentType::AY1).init(&p.instruments[0]);
  PhraseRow& first = p.phrases[0].rows[0];
  first.note = 57; first.instrument = 0; first.volume = PHRASE_VOLUME_MAX;
  if (withSend) { first.fx[0].command = sendFX; first.fx[0].value = 0xFF; }
  PhraseRow& second = p.phrases[0].rows[2];
  second.note = 60; second.instrument = 0; second.volume = PHRASE_VOLUME_MAX;

  chipnomadInitChips(s.get(), 48000, nullptr);
  chipnomadReserveRenderBuffers(s.get(), 512);
  chipnomadQueueProjectRefresh(s.get());
  chipnomadQueuePlaybackStartSong(s.get(), 0, 0, 1);
  return s;
}

static std::vector<float> renderTwoNotes(FX sendFX, bool withSend) {
  State s = startTwoNotes(sendFX, withSend);
  std::vector<float> audio;
  float block[1024];
  while (audio.size() < 48000 * 2) {  // one second
    int frames = chipnomadRender(s.get(), block, 512);
    audio.insert(audio.end(), block, block + frames * 2);
  }
  return audio;
}

// Average level of (with - without) between two times: the effect return only.
static double effectLevel(const std::vector<float>& with, const std::vector<float>& without,
                          double fromSeconds, double toSeconds) {
  double sum = 0;
  size_t from = (size_t)(fromSeconds * 48000) * 2, to = (size_t)(toSeconds * 48000) * 2;
  for (size_t i = from; i < to; ++i) sum += std::fabs((double)with[i] - without[i]);
  return sum / (double)(to - from);
}

TEST_CASE("Reverb tail from a per-note send rings out after the next note") {
  auto with = renderTwoNotes(fxRSN, true), without = renderTwoNotes(fxRSN, false);
  // Rows last 6 ticks at 50 Hz: the second note triggers at 0.24 s.
  double beforeSecondNote = effectLevel(with, without, 0.14, 0.23);
  double justAfter = effectLevel(with, without, 0.26, 0.36);
  double later = effectLevel(with, without, 0.60, 0.70);
  CHECK(beforeSecondNote > 1e-4);
  CHECK(justAfter > beforeSecondNote * 0.2);  // no sudden cut at the next note
  CHECK(later > 1e-4);                        // still ringing
}

TEST_CASE("Delay echoes from a per-note send continue after the next note") {
  auto with = renderTwoNotes(fxDSN, true), without = renderTwoNotes(fxDSN, false);
  CHECK(effectLevel(with, without, 0.14, 0.23) > 1e-4);
  CHECK(effectLevel(with, without, 0.26, 0.50) > 1e-4);  // echoes keep coming
}

TEST_CASE("Effect tails ring out after stop, then the effects switch off") {
  for (FX sendFX : {fxRSN, fxDSN}) {
    CAPTURE(sendFX == fxRSN ? "reverb" : "delay");
    State s = startTwoNotes(sendFX, true);
    float block[1024];
    int rendered = 0;
    while (rendered < 48000 * 3 / 10) rendered += chipnomadRender(s.get(), block, 512);  // 0.3 s
    chipnomadQueuePlaybackStop(s.get());
    // After the stop, the engine keeps rendering only while an effect tail
    // is still audible, then returns 0 frames.
    int tailFrames = 0, frames = 0;
    do {
      frames = chipnomadRender(s.get(), block, 512);
      tailFrames += frames;
    } while (frames > 0 && tailFrames < 48000 * 60);
    CHECK(tailFrames > 48000 / 5);       // the tail rang on after the stop
    CHECK(tailFrames < 48000 * 60);      // and then the effects switched off
    CHECK_FALSE(s->masterEffects->tailActive());
  }
}
