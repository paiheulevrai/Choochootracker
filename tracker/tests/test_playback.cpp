#include "doctest.h"
#include "chipnomad_lib.h"
#include "playback_internal.h"
#include "pitch_table_utils.h"
#include "midi/midi_router.h"

#include <cstring>
#include <cmath>
#include <vector>

namespace {
// Minimal fake MidiBackend for the two MIDI Out integration tests below:
// captures every scheduled event so the tests can assert on what actually
// got sent, without reaching into the router's now-encapsulated internal
// state (see test_midi_router.cpp for router-level unit tests through this
// same fake-backend mechanism).
struct SentMidiEvent { uint8_t type, channel, data1, data2; };
std::vector<SentMidiEvent> g_sentMidiEvents;

void fakeScheduleOutput(void*, const MidiEvent* event, uint64_t) {
  g_sentMidiEvents.push_back({event->type, event->channel, event->data1, event->data2});
}
void fakeFlushOutputQueue(void*) {}
uint64_t fakeNowMicros(void*) { return 0; }

const MidiBackend kFakeMidiBackend = {
  nullptr, // userdata
  nullptr, nullptr, nullptr, nullptr, // port enumeration - unused here
  nullptr, nullptr, nullptr, nullptr, // open/close - unused here
  nullptr, // pollInput - unused here (Out-path only)
  fakeScheduleOutput,
  fakeFlushOutputQueue,
  nullptr, // droppedCount - unused here
  fakeNowMicros,
  nullptr, nullptr,
  1, 1, 0, 0,
};
}

TEST_SUITE("playback") {

// Mock AY chip — only stores register writes

class MockSoundChip : public SoundChipAY {
  public:
    MockSoundChip(int sampleRate, ChipSetup setup) : SoundChipAY(sampleRate, setup) {}
};

static SoundChip* mockChipFactory(int chipIndex, int sampleRate, ChipSetup setup) {
  return new SoundChipAY(sampleRate, setup);
}

// Test fixture
struct PlaybackFixture {
  ChipNomadState* state;

  PlaybackFixture() {
    state = chipnomadCreate();

    Project* p = &state->project;
    p->tickRate = 50;
    p->chipType = ChipType::AY;
    p->chipsCount = 1;
    p->chipSetup.ay = (ChipSetupAY){ .clock = 1773400, .isYM = 0, .stereoMode = StereoModeAY::ABC, .stereoSeparation = 50, .pwmFullRange = 0 };
    p->tracksCount = projectGetTotalTracks(p);
    p->linearPitch = 0;
    calculatePitchTableAY(p);

    chipnomadInitChips(state, 44100, mockChipFactory);
    playbackInit(&state->playbackState, p);
  }

  ~PlaybackFixture() {
    chipnomadDestroy(state);
  }

  // Helper: set up a simple instrument
  void setInstrument(int idx, uint8_t veA, uint8_t veD, uint8_t veS, uint8_t veR) {
    state->project.instruments[idx].type = InstrumentType::AY1;
    state->project.instruments[idx].tableSpeed = 1;
    state->project.instruments[idx].transposeEnabled = 1;
    state->project.instruments[idx].chip.ay.volumeEnvelope.type = ModulationType::ADSR;
    state->project.instruments[idx].chip.ay.volumeEnvelope.amount = 127;  // Full amount
    state->project.instruments[idx].chip.ay.volumeEnvelope.p1 = veA;  // Attack
    state->project.instruments[idx].chip.ay.volumeEnvelope.p2 = veD;  // Decay
    state->project.instruments[idx].chip.ay.volumeEnvelope.p3 = veS;  // Sustain
    state->project.instruments[idx].chip.ay.volumeEnvelope.p4 = veR;  // Release
    state->project.instruments[idx].chip.ay.defaultMixer = 0x01; // Tone only
  }

  // Helper: advance playback by N frames
  void advanceFrames(int n) {
    for (int i = 0; i < n; i++) {
      playbackNextFrame(state);
    }
  }
};

TEST_CASE_FIXTURE(PlaybackFixture, "slowLFO preserves free and structural phase across notes") {
  setInstrument(0, 0, 0, 15, 0);
  Modulation& mod = state->project.instruments[0].modulation[2];
  mod.type = ModulationType::SlowLFO;
  mod.p3 = SLOW_LFO_MAX_STEP;
  PhraseRow row;
  memset(&row, EMPTY_VALUE_8, sizeof(row));
  row.note = 48;
  row.instrument = 0;
  for (auto trigger : {LFOTrigger::free, LFOTrigger::phrase, LFOTrigger::chain, LFOTrigger::retrig}) {
    mod.p2 = static_cast<uint8_t>(trigger);
    readPhraseRowDirect(&state->playbackState, 0, &row, 0);
    PlaybackModState& playing = state->playbackState.tracks[0].note.modulation[2];
    playing.counter = 100000;
    readPhraseRowDirect(&state->playbackState, 0, &row, 0);
    CHECK(playing.counter == (trigger == LFOTrigger::retrig ? 0 : 100000));
  }
}

TEST_CASE("track enable commands apply on the audio tick") {
  PlaybackFixture fixture;
  uint8_t enabled[PROJECT_MAX_TRACKS];
  memset(enabled, 1, sizeof(enabled));
  enabled[0] = 0;
  REQUIRE(chipnomadQueueTrackEnabled(fixture.state, enabled));

  float buffer[2] = {};
  chipnomadRender(fixture.state, buffer, 1);

  CHECK(fixture.state->playbackState.trackEnabled[0] == 0);
}

TEST_CASE("project refresh applies on the audio tick") {
  PlaybackFixture fixture;
  fixture.state->project.trackVolume[0] = 23;
  REQUIRE(chipnomadQueueProjectRefresh(fixture.state));

  float buffer[2] = {};
  chipnomadRender(fixture.state, buffer, 1);

  CHECK(fixture.state->audioProject.trackVolume[0] == 23);
}

TEST_CASE("latest project snapshot wins before the audio tick") {
  PlaybackFixture fixture;
  fixture.state->project.trackVolume[0] = 23;
  REQUIRE(chipnomadQueueProjectRefresh(fixture.state));
  fixture.state->project.trackVolume[0] = 71;
  REQUIRE(chipnomadQueueProjectRefresh(fixture.state));

  float buffer[2] = {};
  chipnomadRender(fixture.state, buffer, 1);

  CHECK(fixture.state->audioProject.trackVolume[0] == 71);
}

TEST_CASE("transport commands apply after the project snapshot") {
  PlaybackFixture fixture;
  fixture.state->project.song[0][0] = EMPTY_VALUE_16;
  REQUIRE(chipnomadQueueProjectRefresh(fixture.state));
  REQUIRE(chipnomadQueuePlaybackStartSong(fixture.state, 0, 0, 1));

  float buffer[2] = {};
  chipnomadRender(fixture.state, buffer, 1);

  CHECK(fixture.state->playbackState.p == &fixture.state->audioProject);
}

TEST_CASE("transport queue reports overflow without touching playback") {
  PlaybackFixture fixture;
  for (int i = 0; i < 63; ++i)
    REQUIRE(chipnomadQueuePlaybackPreviewNote(fixture.state, 0, 48, 0));
  CHECK_FALSE(chipnomadQueuePlaybackPreviewNote(fixture.state, 0, 48, 0));
  CHECK(chipnomadGetCommandOverflow(fixture.state));
}

TEST_CASE("oversize render is rejected without growing buffers") {
  PlaybackFixture fixture;
  float buffer[2] = {};
  fixture.state->frameSampleCounter = (float)(fixture.state->mixBufferSize / 2 + 1);
  CHECK(chipnomadRender(fixture.state, buffer, fixture.state->mixBufferSize / 2 + 1) == 0);
  CHECK(chipnomadGetRenderBufferOverflow(fixture.state));
}

TEST_CASE("playback stop command applies on the audio tick") {
  PlaybackFixture fixture;
  playbackStartSong(&fixture.state->playbackState, 0, 0, 1);
  chipnomadQueuePlaybackStop(fixture.state);

  float buffer[2] = {};
  chipnomadRender(fixture.state, buffer, 1);

  CHECK_FALSE(playbackIsPlaying(&fixture.state->playbackState));
}

TEST_CASE("playback stop sends MIDI Note Off instead of leaving the note stuck") {
  midiRouterSetBackend(&kFakeMidiBackend);
  g_sentMidiEvents.clear();

  PlaybackFixture fixture;
  ChipNomadState* state = fixture.state;
  getInstrumentFunctions(InstrumentType::Midi).init(&state->project.instruments[0]);
  REQUIRE(chipnomadQueueProjectRefresh(state));
  playbackPreviewNote(&state->playbackState, 0, 60, 0);

  // Render a full tick's worth of samples (not just 1) so frameSampleCounter
  // actually cycles back to 0 within each call and advancePlaybackFrame -
  // where the preview command, then later the queued stop, are consumed -
  // runs both times, rather than only on the very first sample ever rendered.
  float buffer[2048] = {};
  chipnomadRender(state, buffer, 1024);
  REQUIRE(g_sentMidiEvents.size() >= 1);
  CHECK(g_sentMidiEvents.back().type == 0x90); // Note On

  // resetTrack() (called from playbackStop) clears noteTriggered/noteReleased
  // directly, bypassing the normal path applyVoiceEvents uses to notice a
  // release and send Note Off - the stop command must still flush it via
  // chipnomadMidiPanic, or the note stays stuck on the external device.
  g_sentMidiEvents.clear();
  chipnomadQueuePlaybackStop(state);
  chipnomadRender(state, buffer, 1024);

  int sawNoteOff = 0;
  for (const SentMidiEvent& e : g_sentMidiEvents) if (e.type == 0x80) sawNoteOff = 1;
  CHECK(sawNoteOff);
}

TEST_CASE_FIXTURE(PlaybackFixture, "playback init all tracks stopped") {
  CHECK_FALSE(playbackIsPlaying(&state->playbackState));
}

TEST_CASE_FIXTURE(PlaybackFixture, "Live chains loop and switch at the requested boundary") {
  Project* p = &state->project;
  p->tracksCount = 2;
  for (int i = 0; i < 16; ++i) p->grooves[0].speed[i] = 1;
  p->chains[0].rows[0].phrase = 0;
  p->chains[0].rows[1].phrase = 0;
  p->chains[1].rows[0].phrase = 0;
  p->chains[2].rows[0].phrase = 0;
  p->song[0][0] = 0;
  p->song[1][0] = 1;
  p->song[2][0] = 2;

  playbackStartLiveChain(&state->playbackState, 0, 0);
  playbackNextFrame(state);
  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::live);

  state->playbackState.tracks[0].phraseRow = 15;
  playbackNextFrame(state);
  CHECK(state->playbackState.tracks[0].chainRow == 1);

  playbackQueueLiveChain(&state->playbackState, 0, 1, 0);
  state->playbackState.tracks[0].phraseRow = 15;
  playbackNextFrame(state);
  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::live);
  CHECK(state->playbackState.tracks[0].songRow == 1);
  CHECK(state->playbackState.tracks[0].chainRow == 0);
}

TEST_CASE_FIXTURE(PlaybackFixture, "table volume and VOL apply to the shared voice gain") {
  PlaybackTrackState* track = &state->playbackState.tracks[0];
  track->note.volume = PHRASE_VOLUME_MAX;
  track->note.volumeOffset = -1;
  tableInit(&state->playbackState, 0, &track->note.instrumentTable, 0, 0, 1);
  state->project.tables[0].rows[0].volume = 15;
  CHECK(playbackVolumeGain(&state->playbackState, track) == doctest::Approx(126.0f / PHRASE_VOLUME_MAX));

  state->project.tables[0].rows[0].volume = 0;
  CHECK(playbackVolumeGain(&state->playbackState, track) == 0.0f);
}

TEST_CASE_FIXTURE(PlaybackFixture, "Live urgent switches at a phrase boundary and stop is quantized") {
  Project* p = &state->project;
  for (int i = 0; i < 16; ++i) p->grooves[0].speed[i] = 1;
  p->chains[0].rows[0].phrase = 0;
  p->chains[0].rows[1].phrase = 0;
  p->chains[1].rows[0].phrase = 0;
  p->chains[2].rows[0].phrase = 0;
  p->song[0][0] = 0;
  p->song[1][0] = 1;
  p->song[2][0] = 2;
  playbackStartLiveChain(&state->playbackState, 0, 0);
  playbackNextFrame(state);

  playbackQueueLiveChain(&state->playbackState, 0, 1, 0);
  playbackQueueLiveChain(&state->playbackState, 0, 2, 1);
  CHECK(state->playbackState.tracks[0].queue.liveAction == LiveQueueAction::urgent);
  CHECK(state->playbackState.tracks[0].queue.songRow == 2);
  LoopRange range = {1};
  playbackSetLoopRange(&state->playbackState, range);
  CHECK(state->playbackState.tracks[0].queue.liveAction == LiveQueueAction::none);
  playbackQueueLiveChain(&state->playbackState, 0, 1, 0);
  CHECK(state->playbackState.tracks[0].queue.liveAction == LiveQueueAction::none);
  playbackClearLoopRange(&state->playbackState);

  playbackQueueLiveChain(&state->playbackState, 0, 1, 1);
  state->playbackState.tracks[0].phraseRow = 15;
  playbackNextFrame(state);
  CHECK(state->playbackState.tracks[0].songRow == 1);
  CHECK(state->playbackState.tracks[0].chainRow == 0);

  playbackQueueLiveChain(&state->playbackState, 0, -1, 0);
  state->playbackState.tracks[0].phraseRow = 15;
  playbackNextFrame(state);
  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::stopped);
}

TEST_CASE_FIXTURE(PlaybackFixture, "instrument FX holds until the next note trigger") {
  state->project.instruments[0].type = InstrumentType::Braids;
  state->project.instruments[0].tableSpeed = 1;
  state->playbackState.tracks[0].mode = PlaybackMode::phraseRow;

  PhraseRow lockedRow;
  memset(&lockedRow, EMPTY_VALUE_8, sizeof(lockedRow));
  lockedRow.note = 48;
  lockedRow.instrument = 0;
  lockedRow.fx[0][0] = fxBTM;
  lockedRow.fx[0][1] = 42;
  readPhraseRowDirect(&state->playbackState, 0, &lockedRow, 0);
  CHECK(state->playbackState.tracks[0].note.fx[fxBTM].isOn == 1);
  CHECK(state->playbackState.tracks[0].note.fx[fxBTM].fxValue == 42);

  PhraseRow nextTrig;
  memset(&nextTrig, EMPTY_VALUE_8, sizeof(nextTrig));
  nextTrig.note = 49;
  readPhraseRowDirect(&state->playbackState, 0, &nextTrig, 0);
  CHECK(state->playbackState.tracks[0].note.fx[fxBTM].isOn == 0);
}

static void setupAChChidGatePattern(PlaybackFixture& fixture) {
  Project* p = &fixture.state->project;
  getInstrumentFunctions(InstrumentType::AChChid).init(&p->instruments[0]);
  p->song[0][0] = 0;
  p->chains[0].rows[0].phrase = 0;
  for (int row = 0; row < 16; ++row)
    memset(&p->phrases[0].rows[row], EMPTY_VALUE_8, sizeof(PhraseRow));
}

TEST_CASE_FIXTURE(PlaybackFixture, "aChChid normal notes release after half a six-tick step") {
  setupAChChidGatePattern(*this);
  PhraseRow* row = &state->project.phrases[0].rows[0];
  row->note = 48;
  row->instrument = 0;
  playbackStartSong(&state->playbackState, 0, 0, 0);

  playbackNextFrame(state);
  advanceFrames(2);
  CHECK_FALSE(state->playbackState.tracks[0].note.noteReleased);
  playbackNextFrame(state);
  CHECK(state->playbackState.tracks[0].note.noteReleased);
}

TEST_CASE_FIXTURE(PlaybackFixture, "aChChid ATY keeps a note open for its full step") {
  setupAChChidGatePattern(*this);
  PhraseRow* row = &state->project.phrases[0].rows[0];
  row->note = 48;
  row->instrument = 0;
  row->fx[0][0] = fxATY;
  row->fx[0][1] = 0;
  playbackStartSong(&state->playbackState, 0, 0, 0);

  playbackNextFrame(state);
  advanceFrames(5);
  CHECK_FALSE(state->playbackState.tracks[0].note.noteReleased);
  playbackNextFrame(state);
  CHECK(state->playbackState.tracks[0].note.noteReleased);
}

TEST_CASE_FIXTURE(PlaybackFixture, "aChChid holds the preceding note for ASL across a chain boundary") {
  setupAChChidGatePattern(*this);
  Project* p = &state->project;
  p->chains[0].rows[1].phrase = 1;
  for (int row = 0; row < 16; ++row)
    memset(&p->phrases[1].rows[row], EMPTY_VALUE_8, sizeof(PhraseRow));
  PhraseRow* source = &p->phrases[0].rows[15];
  source->note = 48;
  source->instrument = 0;
  PhraseRow* target = &p->phrases[1].rows[0];
  target->note = 50;
  target->fx[0][0] = fxASL;
  target->fx[0][1] = 6;

  PlaybackTrackState* track = &state->playbackState.tracks[0];
  track->mode = PlaybackMode::song;
  track->songRow = 0;
  track->chainRow = 0;
  track->phraseRow = 15;
  readPhraseRowDirect(&state->playbackState, 0, source, 0);
  advanceFrames(4);
  CHECK_FALSE(track->note.noteReleased);
  CHECK(track->achchidGateTicks == 0);
}

TEST_CASE_FIXTURE(PlaybackFixture, "instrument table FX applies on the trigger row") {
  state->project.instruments[0].type = InstrumentType::Braids;
  state->project.instruments[0].tableSpeed = 1;
  state->project.tables[0].rows[0].fx[0][0] = fxBCF;
  state->project.tables[0].rows[0].fx[0][1] = 90;
  state->playbackState.tracks[0].mode = PlaybackMode::phraseRow;

  PhraseRow row;
  memset(&row, EMPTY_VALUE_8, sizeof(row));
  row.note = 48;
  row.instrument = 0;
  readPhraseRowDirect(&state->playbackState, 0, &row, 0);

  CHECK(state->playbackState.tracks[0].note.fx[fxBCF].isOn == 1);
  CHECK(state->playbackState.tracks[0].note.fx[fxBCF].fxValue == 90);
}

TEST_CASE_FIXTURE(PlaybackFixture, "table retrigger modes only replace an active Inst table") {
  PlaybackState* playback = &state->playbackState;
  PlaybackTrackState* track = &playback->tracks[0];
  state->project.instruments[0].tableSpeed = 3;
  state->project.instruments[1].tableSpeed = 5;
  track->mode = PlaybackMode::phraseRow;

  PhraseRow row;
  memset(&row, EMPTY_VALUE_8, sizeof(row));
  row.instrument = 0;
  readPhraseRowDirect(playback, 0, &row, 0);
  CHECK(track->note.instrumentTable.tableIdx == 0);
  CHECK(track->note.instrumentTable.baseSpeed == 3);

  state->project.tables[0].retriggerMode = TableRetriggerMode::phrase;
  row.instrument = 1;
  readPhraseRowDirect(playback, 0, &row, 0);
  CHECK(track->note.instrumentTable.tableIdx == 0);

  state->project.tables[0].retriggerMode = TableRetriggerMode::instrument;
  readPhraseRowDirect(playback, 0, &row, 0);
  CHECK(track->note.instrumentTable.tableIdx == 1);
  CHECK(track->note.instrumentTable.baseSpeed == 5);

  // TBL remains explicit, including FF which stops the active table.
  row.instrument = EMPTY_VALUE_8;
  row.fx[0][0] = fxTBL;
  row.fx[0][1] = 2;
  readPhraseRowDirect(playback, 0, &row, 0);
  CHECK(track->note.instrumentTable.tableIdx == 2);
  row.fx[0][1] = EMPTY_VALUE_8;
  readPhraseRowDirect(playback, 0, &row, 0);
  CHECK(track->note.instrumentTable.tableIdx == EMPTY_VALUE_8);
}

TEST_CASE_FIXTURE(PlaybackFixture, "single note outputs to registers") {
  setInstrument(0, 15, 0, 15, 0);

  // Put a note in phrase 0, row 0
  state->project.phrases[0].rows[0].note = 48; // C-4
  state->project.phrases[0].rows[0].instrument = 0;
  state->project.phrases[0].rows[0].volume = PHRASE_VOLUME_MAX;

  // Put phrase 0 in chain 0
  state->project.chains[0].rows[0].phrase = 0;

  // Put chain 0 in song row 0, track 0
  state->project.song[0][0] = 0;

  // Start playback and advance a few frames to let attack ramp up
  playbackStartSong(&state->playbackState, 0, 0, 0);
  advanceFrames(5);

  // Channel 0 tone period should be set (regs 0,1)
  SoundChipAY* ayChip = static_cast<SoundChipAY*>(state->chips[0]);
  uint16_t period = ayChip->getRegister(0) | (ayChip->getRegister(1) << 8);
  CHECK(period == state->project.pitchTable.values[48]);

  // Channel 0 volume should be non-zero (attack phase ramping up)
  CHECK((ayChip->getRegister(8) & 0x0f) != 0);
}

TEST_CASE_FIXTURE(PlaybackFixture, "each tracker track owns an AY") {
  state->project.chipsCount = 2;
  state->project.tracksCount = 2;
  chipnomadInitChips(state, 44100, mockChipFactory);
  playbackInit(&state->playbackState, &state->project);
  setInstrument(0, 0, 0, 15, 0);

  playbackPreviewNote(&state->playbackState, 0, 48, 0);
  playbackPreviewNote(&state->playbackState, 1, 60, 0);
  playbackNextFrame(state);

  SoundChipAY* first = static_cast<SoundChipAY*>(state->chips[0]);
  SoundChipAY* second = static_cast<SoundChipAY*>(state->chips[1]);
  CHECK((first->getRegister(0) | (first->getRegister(1) << 8)) == state->project.pitchTable.values[48]);
  CHECK((second->getRegister(0) | (second->getRegister(1) << 8)) == state->project.pitchTable.values[60]);
  CHECK(first->getRegister(9) == 0);
  CHECK(second->getRegister(9) == 0);
  CHECK((first->getRegister(7) & 0x36) == 0x36);
  CHECK((second->getRegister(7) & 0x36) == 0x36);
}

TEST_CASE_FIXTURE(PlaybackFixture, "ADSR volume envelope ranges") {
  // Test with A=15, D=16, S=1, R=10
  // This should produce low initial volume, decay to sustain level 1, then release
  setInstrument(0, 15, 16, 1, 10);

  // Put a note in phrase 0
  state->project.phrases[0].rows[0].note = 48;
  state->project.phrases[0].rows[0].instrument = 0;
  state->project.phrases[0].rows[0].volume = PHRASE_VOLUME_MAX;

  // Put phrase in chain and song
  state->project.chains[0].rows[0].phrase = 0;
  state->project.song[0][0] = 0;

  // Start playback
  playbackStartSong(&state->playbackState, 0, 0, 0);

  SoundChipAY* ayChip = static_cast<SoundChipAY*>(state->chips[0]);

  // First frame: should start attack phase with low volume (0-2)
  advanceFrames(1);
  uint8_t vol1 = ayChip->getRegister(8) & 0x0f;
  CHECK(vol1 <= 2);

  // After attack (15 frames): should be at max volume (15)
  advanceFrames(14);
  uint8_t volMax = ayChip->getRegister(8) & 0x0f;
  CHECK(volMax == 15);

  // After decay (16 more frames): should be at sustain level (1)
  advanceFrames(16);
  uint8_t volSustain = ayChip->getRegister(8) & 0x0f;
  CHECK(volSustain == doctest::Approx(1).epsilon(1));

  // Trigger note off
  handleNoteOff(&state->playbackState, 0);

  // After release starts: volume should decrease or stay same
  advanceFrames(1);
  uint8_t volRelease1 = ayChip->getRegister(8) & 0x0f;
  CHECK(volRelease1 <= volSustain); // Should be decreasing or same

  // After full release (10 frames): should be at 0
  advanceFrames(10);
  uint8_t volEnd = ayChip->getRegister(8) & 0x0f;
  CHECK(volEnd == 0);
}

TEST_CASE_FIXTURE(PlaybackFixture, "Braids instrument renders through a tracker track") {
  getInstrumentFunctions(InstrumentType::Braids).init(&state->project.instruments[0]);
  REQUIRE(chipnomadQueueProjectRefresh(state));
  playbackPreviewNote(&state->playbackState, 0, 60, 0);

  float buffer[2048];
  int rendered = chipnomadRender(state, buffer, 1024);
  REQUIRE(rendered == 1024);

  double energy = 0.0;
  for (int i = 0; i < rendered; i++) {
    CHECK(buffer[i * 2] == doctest::Approx(buffer[i * 2 + 1]));
    CHECK(std::isfinite(buffer[i * 2]));
    energy += std::fabs(buffer[i * 2]);
  }
  CHECK(energy > 0.0);

  SoundChipAY* ayChip = static_cast<SoundChipAY*>(state->chips[0]);
  CHECK(ayChip->getRegister(8) == 0);
}

TEST_CASE_FIXTURE(PlaybackFixture, "Plaits instrument renders and receives note off") {
  getInstrumentFunctions(InstrumentType::Plaits).init(&state->project.instruments[0]);
  REQUIRE(chipnomadQueueProjectRefresh(state));
  playbackPreviewNote(&state->playbackState, 0, 60, 0);

  float buffer[2048];
  int rendered = chipnomadRender(state, buffer, 1024);
  REQUIRE(rendered == 1024);

  double energy = 0.0;
  for (int i = 0; i < rendered; ++i) {
    CHECK(std::isfinite(buffer[i * 2]));
    energy += std::fabs(buffer[i * 2]);
  }
  CHECK(energy > 0.0);

  handleNoteOff(&state->playbackState, 0);
  CHECK(state->playbackState.tracks[0].note.noteReleased == 1);
  CHECK(state->playbackState.tracks[0].note.pitchBase == EMPTY_VALUE_8);
}

TEST_CASE_FIXTURE(PlaybackFixture, "MIDI Out instrument tracks its active note and panic sends Note Off") {
  midiRouterSetBackend(&kFakeMidiBackend);
  g_sentMidiEvents.clear();

  getInstrumentFunctions(InstrumentType::Midi).init(&state->project.instruments[0]);
  state->project.instruments[0].chip.midi.channel = 3;
  REQUIRE(chipnomadQueueProjectRefresh(state));
  playbackPreviewNote(&state->playbackState, 0, 60, 0);

  float buffer[2] = {};
  chipnomadRender(state, buffer, 1);

  REQUIRE(g_sentMidiEvents.size() >= 1);
  CHECK(g_sentMidiEvents.back().type == 0x90); // Note On
  CHECK(g_sentMidiEvents.back().channel == 3);

  g_sentMidiEvents.clear();
  chipnomadMidiPanic(state);
  int sawNoteOff = 0;
  for (const SentMidiEvent& e : g_sentMidiEvents) if (e.type == 0x80 && e.channel == 3) sawNoteOff = 1;
  CHECK(sawNoteOff);
  // Router-level cache invalidation (Program/Bank resend after panic) is
  // covered directly in test_midi_router.cpp.
}

TEST_CASE_FIXTURE(PlaybackFixture, "SLE reaches every BYOWTBL engine FX destination") {
  getInstrumentFunctions(InstrumentType::BYOWTBL).init(&state->project.instruments[0]);
  REQUIRE(chipnomadQueueProjectRefresh(state));

  float buffer[2] = {};
  chipnomadRender(state, buffer, 1);

  PlaybackTrackState* track = &state->playbackState.tracks[0];
  track->note.instrument = 0;
  track->note.pitchFinal = 60;
  track->note.volume = PHRASE_VOLUME_MAX;
  track->slewTicks = 8;
  const FX destinations[] = {fxSDT, fxSMX, fxBIA, fxBIB, fxSCF2, fxSRS2};
  for (FX fx : destinations) {
    track->note.fx[fx].isOn = 1;
    track->note.fx[fx].fxValue = 16;
  }
  track->mode = PlaybackMode::phraseRow;
  track->songRow = 0;
  state->frameSampleCounter = 0;
  chipnomadRender(state, buffer, 1);

  for (FX fx : destinations) track->note.fx[fx].fxValue = 240;
  state->frameSampleCounter = 0;
  chipnomadRender(state, buffer, 1);

  for (FX fx : destinations) {
    CHECK(track->slewTarget[fx] == 240);
    CHECK(track->slewCurrent[fx] > 16);
    CHECK(track->slewCurrent[fx] < 240);
    CHECK(track->slewRemaining[fx] == 7);
  }
}

TEST_CASE_FIXTURE(PlaybackFixture, "PRO 00 suppresses a row and PRO 64 always triggers") {
  setInstrument(0, 0, 0, 15, 0);
  PhraseRow* row = &state->project.phrases[0].rows[0];
  row->note = 48;
  row->instrument = 0;
  row->fx[0][0] = fxPRO;
  row->fx[0][1] = 0x00;
  state->project.chains[0].rows[0].phrase = 0;
  state->project.song[0][0] = 0;
  playbackStartSong(&state->playbackState, 0, 0, 0);
  advanceFrames(1);
  CHECK(state->playbackState.tracks[0].note.pitchBase == EMPTY_VALUE_8);

  playbackStop(&state->playbackState);
  row->fx[0][1] = 0x64;
  playbackStartSong(&state->playbackState, 0, 0, 0);
  advanceFrames(1);
  CHECK(state->playbackState.tracks[0].note.pitchBase == 48);
}

TEST_CASE_FIXTURE(PlaybackFixture, "MOD 22 triggers on the second visit") {
  setInstrument(0, 0, 0, 15, 0);
  PhraseRow* row = &state->project.phrases[0].rows[0];
  row->note = 48;
  row->instrument = 0;
  row->fx[0][0] = fxMOD;
  row->fx[0][1] = 0x22;
  state->project.chains[0].rows[0].phrase = 0;
  state->project.chains[0].rows[1].phrase = 0;
  state->project.song[0][0] = 0;
  playbackStartSong(&state->playbackState, 0, 0, 0);
  advanceFrames(1);
  CHECK(state->playbackState.tracks[0].note.pitchBase == EMPTY_VALUE_8);
  advanceFrames(96);
  CHECK(state->playbackState.tracks[0].note.pitchBase == 48);
}

TEST_CASE_FIXTURE(PlaybackFixture, "SPD 01 doubles one track clock and persists") {
  state->project.phrases[0].rows[0].fx[0][0] = fxSPD;
  state->project.phrases[0].rows[0].fx[0][1] = 0x01;
  state->project.chains[0].rows[0].phrase = 0;
  state->project.song[0][0] = 0;
  playbackStartSong(&state->playbackState, 0, 0, 0);
  advanceFrames(1);
  CHECK(state->playbackState.tracks[0].speedRatio == 0x01);
  advanceFrames(3);
  CHECK(state->playbackState.tracks[0].phraseRow == 1);
  advanceFrames(3);
  CHECK(state->playbackState.tracks[0].phraseRow == 2);
}

} // TEST_SUITE("playback")
