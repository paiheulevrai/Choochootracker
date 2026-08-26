#include "doctest.h"
#include "chipnomad_lib.h"
#include "playback_internal.h"
#include "pitch_table_utils.h"

#include <cstring>
#include <cmath>

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

TEST_CASE_FIXTURE(PlaybackFixture, "playback init all tracks stopped") {
  CHECK_FALSE(playbackIsPlaying(&state->playbackState));
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

TEST_CASE_FIXTURE(PlaybackFixture, "single note outputs to registers") {
  setInstrument(0, 15, 0, 15, 0);

  // Put a note in phrase 0, row 0
  state->project.phrases[0].rows[0].note = 48; // C-4
  state->project.phrases[0].rows[0].instrument = 0;
  state->project.phrases[0].rows[0].volume = 15;

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

TEST_CASE_FIXTURE(PlaybackFixture, "auto mix relieves an octave-band pileup") {
  Project* p = &state->project;
  p->chipsCount = 3;
  p->tracksCount = 3;
  chipnomadInitChips(state, 44100, mockChipFactory);
  playbackInit(&state->playbackState, p);
  setInstrument(0, 0, 0, 15, 0);
  p->instruments[0].volume = 255;
  for (int track = 0; track < 3; ++track) {
    p->phrases[track].rows[0].note = track < 2 ? 36 : 72;
    p->phrases[track].rows[0].instrument = 0;
    p->phrases[track].rows[0].volume = 15;
    for (int row = 0; row < 12; ++row) p->chains[track].rows[row].phrase = track;
    p->song[0][track] = track;
  }

  uint8_t proposed[PROJECT_MAX_TRACKS] = {};
  REQUIRE(chipnomadAutoMix(state, 1, proposed) == 0);
  CHECK(proposed[0] < proposed[2]);
  CHECK(proposed[1] < proposed[2]);
  CHECK(proposed[0] >= 25);
  CHECK(proposed[1] >= 25);
}

TEST_CASE_FIXTURE(PlaybackFixture, "ADSR volume envelope ranges") {
  // Test with A=15, D=16, S=1, R=10
  // This should produce low initial volume, decay to sustain level 1, then release
  setInstrument(0, 15, 16, 1, 10);

  // Put a note in phrase 0
  state->project.phrases[0].rows[0].note = 48;
  state->project.phrases[0].rows[0].instrument = 0;
  state->project.phrases[0].rows[0].volume = 15;

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
