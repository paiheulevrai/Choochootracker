#include "doctest.h"
#include "chipnomad_lib.h"
#include "pitch_table_utils.h"
#include "project.h"
#include "utils.h"

#include <set>

namespace {

// Plays a Braids preview note and collects what track 0 reports for Timbre.
std::set<int> reportedTimbre(bool lfo, uint8_t* reportedInstrument) {
  auto* s = chipnomadCreate();
  auto& p = s->project;
  p.chipsCount = 1; p.tracksCount = 1; p.tickRate = 50; p.linearPitch = 1;
  calculateLinearPitchTable12TET(&p);
  Instrument& braids = p.instruments[2];
  getInstrumentFunctions(InstrumentType::Braids).init(&braids);
  if (lfo) {
    // Destination 3 is Timbre; an LFO that stays within range.
    braids.modulation[2] = {};
    braids.modulation[2].type = ModulationType::LFO;
    braids.modulation[2].destination = 3;
    braids.modulation[2].amount = 40;
    braids.modulation[2].p3 = 37;  // ticks per cycle, so each tick lands somewhere new
  }
  chipnomadInitChips(s, 48000, nullptr);
  REQUIRE(chipnomadQueueProjectRefresh(s));
  chipnomadQueuePlaybackPreviewNote(s, 0, 48, 2);
  std::set<int> values;
  float audio[1920];
  for (int block = 0; block < 40; ++block) {
    chipnomadRender(s, audio, 960);
    const PlaybackStatus* status = chipnomadGetPlaybackStatus(s);
    *reportedInstrument = status->tracks[0].liveInstrument;
    values.insert(status->tracks[0].liveControls[liveSource1]);
  }
  chipnomadDestroy(s);
  return values;
}

}  // namespace

TEST_CASE("Engines report what a track plays for the instrument page's bars") {
  uint8_t instrument = 0;
  // Without modulation, Timbre is reported at its set value.
  const std::set<int> steady = reportedTimbre(false, &instrument);
  CHECK(instrument == 2);
  Instrument fresh;
  getInstrumentFunctions(InstrumentType::Braids).init(&fresh);
  const int set = controlFromRange(fresh.chip.braids.timbre, 32767);
  CHECK(steady.size() == 1);
  CHECK(abs(*steady.begin() - set) <= 1);
  // With an LFO on Timbre, the reported value moves.
  const std::set<int> moving = reportedTimbre(true, &instrument);
  CHECK(moving.size() > 5);
}

TEST_CASE("Live bars clear when a playing instrument changes to an unreported engine") {
  auto* s = chipnomadCreate();
  auto& p = s->project;
  p.chipsCount = 1; p.tracksCount = 1; p.tickRate = 50; p.linearPitch = 1;
  calculateLinearPitchTable12TET(&p);
  Instrument& instrument = p.instruments[2];
  getInstrumentFunctions(InstrumentType::Braids).init(&instrument);
  chipnomadInitChips(s, 48000, nullptr);
  REQUIRE(chipnomadQueueProjectRefresh(s));
  chipnomadQueuePlaybackPreviewNote(s, 0, 48, 2);
  float audio[1920];
  chipnomadRender(s, audio, 960);
  CHECK(chipnomadGetPlaybackStatus(s)->tracks[0].liveInstrument == 2);

  getInstrumentFunctions(InstrumentType::Braids).free(&instrument);
  getInstrumentFunctions(InstrumentType::OPLL).init(&instrument);
  REQUIRE(chipnomadQueueProjectRefresh(s));
  chipnomadRender(s, audio, 960);
  CHECK(chipnomadGetPlaybackStatus(s)->tracks[0].liveInstrument == EMPTY_VALUE_8);
  chipnomadDestroy(s);
}
