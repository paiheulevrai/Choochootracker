#include "doctest.h"
#include "chipnomad_lib.h"
#include "playback_internal.h"
#include "pitch_table_utils.h"
#include "export/export.h"

#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>

TEST_SUITE("bounce") {

static SoundChip* mockChipFactory(int chipIndex, int sampleRate, ChipSetup setup) {
  return new SoundChipAY(sampleRate, setup);
}

// Test fixture: same shape as PlaybackFixture in test_playback.cpp
struct BounceFixture {
  ChipNomadState* state;

  BounceFixture() {
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

  ~BounceFixture() {
    chipnomadDestroy(state);
  }

  void setInstrument(int idx, uint8_t veA, uint8_t veD, uint8_t veS, uint8_t veR) {
    state->project.instruments[idx].type = InstrumentType::AY1;
    state->project.instruments[idx].tableSpeed = 1;
    state->project.instruments[idx].transposeEnabled = 1;
    state->project.instruments[idx].chip.ay.volumeEnvelope.type = ModulationType::ADSR;
    state->project.instruments[idx].chip.ay.volumeEnvelope.amount = 127;
    state->project.instruments[idx].chip.ay.volumeEnvelope.p1 = veA;
    state->project.instruments[idx].chip.ay.volumeEnvelope.p2 = veD;
    state->project.instruments[idx].chip.ay.volumeEnvelope.p3 = veS;
    state->project.instruments[idx].chip.ay.volumeEnvelope.p4 = veR;
    state->project.instruments[idx].chip.ay.defaultMixer = 0x01; // Tone only
  }

  void advanceFrames(int n) {
    for (int i = 0; i < n; i++) {
      playbackNextFrame(state);
    }
  }
};

// Build a project with `rows` note rows in phrase 0 (chain 0, song row 0),
// default groove 6 ticks/row.
static void buildNoteRows(BounceFixture& f, int rows) {
  f.setInstrument(0, 15, 0, 15, 0);
  for (int r = 0; r < rows; r++) {
    f.state->project.phrases[0].rows[r].note = 48; // C-4
    f.state->project.phrases[0].rows[r].instrument = 0;
    f.state->project.phrases[0].rows[r].volume = 15;
  }
  f.state->project.chains[0].rows[0].phrase = 0;
  f.state->project.song[0][0] = 0;
}

TEST_CASE_FIXTURE(BounceFixture, "stopRange phrase level stops the track at the boundary") {
  buildNoteRows(*this, 16);

  StopRange range = {};
  range.enabled = 1;
  range.level = 2;
  range.endSongRow = 0;
  range.endChainRow = 0;
  range.endPhraseRow = 3; // Play rows 0..3, then stop
  playbackSetStopRange(&state->playbackState, range);

  playbackStartPhrase(&state->playbackState, 0, 0, 0, 0);
  advanceFrames(6 * 4 + 10); // 4 rows at 6 ticks/row + margin

  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::stopped);
  CHECK(state->playbackState.tracks[0].songRow == EMPTY_VALUE_16);
}

TEST_CASE_FIXTURE(BounceFixture, "stopRange phrase level lets earlier rows play") {
  buildNoteRows(*this, 16);

  StopRange range = {};
  range.enabled = 1;
  range.level = 2;
  range.endSongRow = 0;
  range.endChainRow = 0;
  range.endPhraseRow = 3;
  playbackSetStopRange(&state->playbackState, range);

  playbackStartPhrase(&state->playbackState, 0, 0, 0, 0);
  // Frame 1 consumes the queue (row 0); each row then lasts 6 frames
  advanceFrames(6 * 2 + 1); // 2 rows in, still inside the range

  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::phrase);
  CHECK(state->playbackState.tracks[0].phraseRow == 2);
}

TEST_CASE_FIXTURE(BounceFixture, "stopRange chain level stops at the chain boundary") {
  buildNoteRows(*this, 16);
  // Chain 0 rows 0 and 1 both use phrase 0
  state->project.chains[0].rows[1].phrase = 0;

  StopRange range = {};
  range.enabled = 1;
  range.level = 1;
  range.endSongRow = 0;
  range.endChainRow = 1; // Play chain rows 0..1, then stop
  range.endPhraseRow = 15;
  playbackSetStopRange(&state->playbackState, range);

  playbackStartChain(&state->playbackState, 0, 0, 0, 0);
  advanceFrames(6 * 16 * 2 + 10); // 2 full phrases + margin

  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::stopped);
}

TEST_CASE_FIXTURE(BounceFixture, "stopRange song level stops at the song boundary") {
  buildNoteRows(*this, 16);
  // Song rows 0 and 1 both have chain 0
  state->project.song[1][0] = 0;

  StopRange range = {};
  range.enabled = 1;
  range.level = 0;
  range.endSongRow = 1; // Play song rows 0..1, then stop
  range.endChainRow = 15;
  range.endPhraseRow = 15;
  playbackSetStopRange(&state->playbackState, range);

  playbackStartSong(&state->playbackState, 0, 0, 0);
  advanceFrames(6 * 16 * 2 + 10); // 2 full phrases + margin

  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::stopped);
}

TEST_CASE_FIXTURE(BounceFixture, "stopRange song level stops when next song row is empty") {
  buildNoteRows(*this, 16);
  // Song row 1 is empty (default) - without stopRange the track would stop
  // anyway, but the boundary check must fire first and behave identically

  StopRange range = {};
  range.enabled = 1;
  range.level = 0;
  range.endSongRow = 0;
  range.endChainRow = 15;
  range.endPhraseRow = 15;
  playbackSetStopRange(&state->playbackState, range);

  playbackStartSong(&state->playbackState, 0, 0, 0);
  advanceFrames(6 * 16 + 10);

  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::stopped);
}

TEST_CASE_FIXTURE(BounceFixture, "stopRange disabled does not stop playback") {
  buildNoteRows(*this, 16);
  state->project.chains[0].rows[1].phrase = 0;
  state->project.chains[0].rows[2].phrase = 0;

  StopRange range = {};
  range.enabled = 0;
  range.level = 2;
  range.endPhraseRow = 3;
  playbackSetStopRange(&state->playbackState, range);

  playbackStartPhrase(&state->playbackState, 0, 0, 0, 0);
  advanceFrames(6 * 10 + 1);

  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::phrase);
  CHECK(state->playbackState.tracks[0].phraseRow == 10);
}

TEST_CASE_FIXTURE(BounceFixture, "stopRange kills the sounding note at the boundary") {
  buildNoteRows(*this, 16);
  // Sustaining instrument: instant attack, full sustain
  setInstrument(0, 0, 0, 15, 0);

  StopRange range = {};
  range.enabled = 1;
  range.level = 2;
  range.endSongRow = 0;
  range.endChainRow = 0;
  range.endPhraseRow = 3;
  playbackSetStopRange(&state->playbackState, range);

  playbackStartPhrase(&state->playbackState, 0, 0, 0, 0);
  // Render through the boundary: 4 rows * 6 ticks * (44100/50) samples/tick
  int samplesPerTick = 44100 / 50;
  int totalSamples = 4 * 6 * samplesPerTick;
  float buffer[44100];
  int rendered = 0;
  while (rendered < totalSamples + samplesPerTick * 6) {
    int n = chipnomadRender(state, buffer, 256);
    if (n == 0) break;
    rendered += n;
  }

  // After the boundary the track is stopped: further render calls produce
  // silence (zero-filled remainder)
  float tail[256];
  int tailRendered = chipnomadRender(state, tail, 256);
  CHECK(tailRendered == 256);
  for (int i = 0; i < 256 * 2; i++) {
    CHECK(tail[i] == 0.0f);
  }
}

TEST_CASE_FIXTURE(BounceFixture, "playbackStartPhrase starts at the requested phrase row") {
  buildNoteRows(*this, 16);

  playbackStartPhrase(&state->playbackState, 0, 0, 0, 0, 5);
  advanceFrames(1);

  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::phrase);
  CHECK(state->playbackState.tracks[0].phraseRow == 5);
}

TEST_CASE_FIXTURE(BounceFixture, "SNG jump past the stop boundary stops the track") {
  buildNoteRows(*this, 16);
  // Row 0 has an SNG +2 command: jumps from song row 0 to song row 2
  state->project.phrases[0].rows[0].fx[0][0] = fxSNG;
  state->project.phrases[0].rows[0].fx[0][1] = 2;
  state->project.song[2][0] = 0; // Valid target at song row 2

  StopRange range = {};
  range.enabled = 1;
  range.level = 0;
  range.endSongRow = 1; // Selection covers song rows 0..1
  range.endChainRow = 15;
  range.endPhraseRow = 15;
  playbackSetStopRange(&state->playbackState, range);

  playbackStartSong(&state->playbackState, 0, 0, 0);
  advanceFrames(6 + 10); // First row + margin

  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::stopped);
}

TEST_CASE_FIXTURE(BounceFixture, "SNG jump inside the stop boundary is allowed") {
  buildNoteRows(*this, 16);
  // Row 0 has an SNG +1 command: jumps from song row 0 to song row 1
  state->project.phrases[0].rows[0].fx[0][0] = fxSNG;
  state->project.phrases[0].rows[0].fx[0][1] = 1;
  state->project.song[1][0] = 0; // Valid target at song row 1

  StopRange range = {};
  range.enabled = 1;
  range.level = 0;
  range.endSongRow = 1;
  range.endChainRow = 15;
  range.endPhraseRow = 15;
  playbackSetStopRange(&state->playbackState, range);

  playbackStartSong(&state->playbackState, 0, 0, 0);
  advanceFrames(6 + 5);

  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::song);
  CHECK(state->playbackState.tracks[0].songRow == 1);
}

TEST_CASE_FIXTURE(BounceFixture, "HOP target outside the phrase selection stops the track") {
  buildNoteRows(*this, 16);
  // Row 3 has a conditional HOP: loop count 1, target row 5 - outside 0..3
  state->project.phrases[0].rows[3].fx[0][0] = fxHOP;
  state->project.phrases[0].rows[3].fx[0][1] = 0x15;

  StopRange range = {};
  range.enabled = 1;
  range.level = 2;
  range.endSongRow = 0;
  range.endChainRow = 0;
  range.startPhraseRow = 0;
  range.endPhraseRow = 3;
  playbackSetStopRange(&state->playbackState, range);

  playbackStartPhrase(&state->playbackState, 0, 0, 0, 0);
  advanceFrames(6 * 4 + 10);

  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::stopped);
}

TEST_CASE_FIXTURE(BounceFixture, "HOP target inside the phrase selection is allowed") {
  buildNoteRows(*this, 16);
  // Row 3 has a conditional HOP: loop count 1, target row 2 - inside 0..3
  state->project.phrases[0].rows[3].fx[0][0] = fxHOP;
  state->project.phrases[0].rows[3].fx[0][1] = 0x12;

  StopRange range = {};
  range.enabled = 1;
  range.level = 2;
  range.endSongRow = 0;
  range.endChainRow = 0;
  range.startPhraseRow = 0;
  range.endPhraseRow = 3;
  playbackSetStopRange(&state->playbackState, range);

  playbackStartPhrase(&state->playbackState, 0, 0, 0, 0);
  // Frame 19: row 3 is read and the conditional HOP jumps back to row 2
  advanceFrames(19);

  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::phrase);
  CHECK(state->playbackState.tracks[0].phraseRow == 2);
}

TEST_CASE_FIXTURE(BounceFixture, "ExporterSelectionWAV renders phrase selection length") {
  buildNoteRows(*this, 16);

  ExportSelection selection = {};
  selection.level = 2;
  selection.startSongRow = 0;
  selection.endSongRow = 0;
  selection.startChainRow = 0;
  selection.endChainRow = 0;
  selection.startPhraseRow = 0;
  selection.endPhraseRow = 3; // 4 rows
  selection.trackMask = 1;

  ExporterSelectionWAV exporter("/tmp/test_bounce_phrase.wav", &state->project, selection, 44100, 16, 1.0f);

  // 4 rows * 6 ticks/row * (44100/50) samples/tick = 21204 samples
  int expectedSamples = 4 * 6 * (44100 / 50);
  while (true) {
    int seconds = exporter.next();
    if (seconds == -1) break;
    if (seconds > 100) break; // Safety guard
  }
  exporter.finish();

  FILE* file = fopen("/tmp/test_bounce_phrase.wav", "rb");
  REQUIRE(file != NULL);
  uint8_t header[44];
  REQUIRE(fread(header, 1, 44, file) == 44);
  uint32_t dataSize = header[40] | (header[41] << 8) | (header[42] << 16) | ((uint32_t)header[43] << 24);
  CHECK(dataSize == (uint32_t)(expectedSamples * 2 * 2)); // stereo, 16-bit
  fseek(file, 0, SEEK_END);
  long fileSize = ftell(file);
  CHECK(fileSize == 44 + (long)dataSize);
  fclose(file);
  remove("/tmp/test_bounce_phrase.wav");
}

TEST_CASE_FIXTURE(BounceFixture, "ExporterSelectionWAV mutes unselected tracks") {
  // Track 0 -> chain 0 -> phrase 0 (empty, silent)
  // Track 1 -> chain 1 -> phrase 1 (notes, audible only if muting fails)
  // The exporter renders in its own private chipnomadState, so muting is
  // verified behaviorally through the WAV output.
  setInstrument(0, 15, 0, 15, 0);
  // 2 chips = 2 tracks (1 track per chip); the exporter copies the project
  // and initializes its own chips from chipsCount
  state->project.chipsCount = 2;
  state->project.tracksCount = 2;
  for (int r = 0; r < 16; r++) {
    state->project.phrases[1].rows[r].note = 48;
    state->project.phrases[1].rows[r].instrument = 0;
    state->project.phrases[1].rows[r].volume = 15;
  }
  state->project.chains[0].rows[0].phrase = 0;
  state->project.chains[1].rows[0].phrase = 1;
  state->project.song[0][0] = 0;
  state->project.song[0][1] = 1;

  ExportSelection selection = {};
  selection.level = 0;
  selection.startSongRow = 0;
  selection.endSongRow = 0;
  selection.startChainRow = 0;
  selection.endChainRow = 15;
  selection.startPhraseRow = 0;
  selection.endPhraseRow = 15;
  selection.trackMask = 0x01; // Only track 0 (silent phrase)

  {
    ExporterSelectionWAV exporter("/tmp/test_bounce_mute.wav", &state->project, selection, 44100, 16, 1.0f);
    while (exporter.next() != -1) {}
    exporter.finish();
  }

  // The whole mix must be silent: track 1 (the only one with notes) is muted
  float maxAbs = 0.0f;
  FILE* file = fopen("/tmp/test_bounce_mute.wav", "rb");
  REQUIRE(file != NULL);
  fseek(file, 0, SEEK_END);
  long fileSize = ftell(file);
  fseek(file, 44, SEEK_SET);
  long dataBytes = fileSize - 44;
  int16_t* samples = (int16_t*)malloc(dataBytes);
  REQUIRE(fread(samples, 1, dataBytes, file) == (size_t)dataBytes);
  fclose(file);
  for (long i = 0; i < dataBytes / 2; i++) {
    float v = fabsf(samples[i] / 32768.0f);
    if (v > maxAbs) maxAbs = v;
  }
  free(samples);
  CHECK(maxAbs < 1e-5f);
  remove("/tmp/test_bounce_mute.wav");

  // Positive control: selecting track 1 instead produces sound
  selection.trackMask = 0x02;
  {
    ExporterSelectionWAV exporter("/tmp/test_bounce_unmuted.wav", &state->project, selection, 44100, 16, 1.0f);
    while (exporter.next() != -1) {}
    exporter.finish();
  }
  file = fopen("/tmp/test_bounce_unmuted.wav", "rb");
  REQUIRE(file != NULL);
  fseek(file, 0, SEEK_END);
  fileSize = ftell(file);
  fseek(file, 44, SEEK_SET);
  dataBytes = fileSize - 44;
  samples = (int16_t*)malloc(dataBytes);
  REQUIRE(fread(samples, 1, dataBytes, file) == (size_t)dataBytes);
  fclose(file);
  maxAbs = 0.0f;
  for (long i = 0; i < dataBytes / 2; i++) {
    float v = fabsf(samples[i] / 32768.0f);
    if (v > maxAbs) maxAbs = v;
  }
  free(samples);
  CHECK(maxAbs > 0.01f);
  remove("/tmp/test_bounce_unmuted.wav");
}

TEST_CASE_FIXTURE(BounceFixture, "ExporterSelectionWAV empty selection produces a valid empty WAV") {
  ExportSelection selection = {};
  selection.level = 2;
  selection.trackMask = 1;
  // No track started: song row points at an empty song cell
  state->project.song[0][0] = EMPTY_VALUE_16;

  ExporterSelectionWAV exporter("/tmp/test_bounce_empty.wav", &state->project, selection, 44100, 16, 1.0f);

  int seconds = exporter.next();
  CHECK(seconds == -1);
  exporter.finish();

  FILE* file = fopen("/tmp/test_bounce_empty.wav", "rb");
  REQUIRE(file != NULL);
  uint8_t header[44];
  REQUIRE(fread(header, 1, 44, file) == 44);
  uint32_t dataSize = header[40] | (header[41] << 8) | (header[42] << 16) | ((uint32_t)header[43] << 24);
  CHECK(dataSize == 0);
  fclose(file);
  remove("/tmp/test_bounce_empty.wav");
}

// Reads the peak absolute sample value from a 16-bit stereo WAV file
static float readWavMaxAbs(const char* path) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) return -1.0f;
  fseek(file, 0, SEEK_END);
  long fileSize = ftell(file);
  fseek(file, 44, SEEK_SET);
  long dataBytes = fileSize - 44;
  int16_t* samples = (int16_t*)malloc(dataBytes);
  if (fread(samples, 1, dataBytes, file) != (size_t)dataBytes) {
    free(samples);
    fclose(file);
    return -1.0f;
  }
  fclose(file);
  float maxAbs = 0.0f;
  for (long i = 0; i < dataBytes / 2; i++) {
    float v = fabsf(samples[i] / 32768.0f);
    if (v > maxAbs) maxAbs = v;
  }
  free(samples);
  return maxAbs;
}

TEST_CASE_FIXTURE(BounceFixture, "ExporterSelectionWAV starts tracks that enter mid-selection") {
  // Track 0 has a chain at song row 0; track 1's first chain is at song row 2.
  // A song-level bounce of rows 0..3 must include track 1's chain: the track
  // waits silently through the empty rows and joins at song row 2.
  setInstrument(0, 15, 0, 15, 0);
  state->project.chipsCount = 2;
  state->project.tracksCount = 2;
  // Phrase 1 (played by track 1) has notes; phrase 0 (track 0) is empty
  for (int r = 0; r < 16; r++) {
    state->project.phrases[1].rows[r].note = 48;
    state->project.phrases[1].rows[r].instrument = 0;
    state->project.phrases[1].rows[r].volume = 15;
  }
  state->project.chains[0].rows[0].phrase = 0;
  state->project.chains[1].rows[0].phrase = 1;
  state->project.song[0][0] = 0;
  state->project.song[2][1] = 1;

  ExportSelection selection = {};
  selection.level = 0;
  selection.startSongRow = 0;
  selection.endSongRow = 3;
  selection.startChainRow = 0;
  selection.endChainRow = 15;
  selection.startPhraseRow = 0;
  selection.endPhraseRow = 15;
  selection.trackMask = 0x03; // Both tracks

  {
    ExporterSelectionWAV exporter("/tmp/test_bounce_late.wav", &state->project, selection, 44100, 16, 1.0f);
    while (exporter.next() != -1) {}
    exporter.finish();
  }

  // Track 1's notes must be present in the mix
  float maxAbs = readWavMaxAbs("/tmp/test_bounce_late.wav");
  REQUIRE(maxAbs >= 0.0f);
  CHECK(maxAbs > 0.01f);
  remove("/tmp/test_bounce_late.wav");

  // Control: selecting only track 0 (empty phrase) produces silence, proving
  // the sound above comes from track 1's late-entering chain
  selection.trackMask = 0x01;
  {
    ExporterSelectionWAV exporter("/tmp/test_bounce_late_ctrl.wav", &state->project, selection, 44100, 16, 1.0f);
    while (exporter.next() != -1) {}
    exporter.finish();
  }
  maxAbs = readWavMaxAbs("/tmp/test_bounce_late_ctrl.wav");
  REQUIRE(maxAbs >= 0.0f);
  CHECK(maxAbs < 1e-5f);
  remove("/tmp/test_bounce_late_ctrl.wav");
}

TEST_CASE_FIXTURE(BounceFixture, "exportSelectionLengthRows phrase level") {
  ExportSelection selection = {};
  selection.level = 2;
  selection.trackMask = 1;
  selection.startSongRow = 0;
  selection.endSongRow = 0;
  selection.startChainRow = 0;
  selection.endChainRow = 0;

  // Two full phrases: rows 0..31 spans two phrases
  selection.startPhraseRow = 0;
  selection.endPhraseRow = 15;
  CHECK(exportSelectionLengthRows(&state->project, selection) == 16);

  // 3/16 of a phrase: rows 0..2
  selection.startPhraseRow = 0;
  selection.endPhraseRow = 2;
  CHECK(exportSelectionLengthRows(&state->project, selection) == 3);

  // 7/16 of a phrase: rows 4..10
  selection.startPhraseRow = 4;
  selection.endPhraseRow = 10;
  CHECK(exportSelectionLengthRows(&state->project, selection) == 7);

  // Two full beats: rows 0..7
  selection.startPhraseRow = 0;
  selection.endPhraseRow = 7;
  CHECK(exportSelectionLengthRows(&state->project, selection) == 8);
}

TEST_CASE_FIXTURE(BounceFixture, "exportSelectionLengthRows chain level counts consecutive rows") {
  // Chain 0: rows 0-1 filled, row 2 empty, row 3 filled
  state->project.chains[0].rows[0].phrase = 0;
  state->project.chains[0].rows[1].phrase = 0;
  state->project.chains[0].rows[3].phrase = 0;
  state->project.song[0][0] = 0;

  ExportSelection selection = {};
  selection.level = 1;
  selection.trackMask = 1;
  selection.startSongRow = 0;
  selection.endSongRow = 0;
  selection.startChainRow = 0;
  selection.endChainRow = 15;
  selection.startPhraseRow = 0;
  selection.endPhraseRow = 15;

  // The engine stops at the first empty chain row: 2 phrases = 32 rows
  CHECK(exportSelectionLengthRows(&state->project, selection) == 32);

  // Starting mid-chain: rows 3..15 -> 1 phrase = 16 rows
  selection.startChainRow = 3;
  CHECK(exportSelectionLengthRows(&state->project, selection) == 16);

  // Starting on an empty chain row: nothing plays
  selection.startChainRow = 2;
  CHECK(exportSelectionLengthRows(&state->project, selection) == 0);
}

TEST_CASE_FIXTURE(BounceFixture, "exportSelectionLengthRows song level takes the longest track") {
  // Both tracks play on the same song rows with different chain lengths:
  // chain 0 has 2 consecutive phrases, chain 1 has 1
  state->project.chipsCount = 2;
  state->project.tracksCount = 2;
  state->project.chains[0].rows[0].phrase = 0;
  state->project.chains[0].rows[1].phrase = 0;
  state->project.chains[1].rows[0].phrase = 1;
  state->project.song[0][0] = 0;
  state->project.song[1][0] = 0;
  state->project.song[0][1] = 1;
  state->project.song[1][1] = 1;

  ExportSelection selection = {};
  selection.level = 0;
  selection.trackMask = 0x03;
  selection.startSongRow = 0;
  selection.endSongRow = 1;
  selection.startChainRow = 0;
  selection.endChainRow = 15;
  selection.startPhraseRow = 0;
  selection.endPhraseRow = 15;

  // Track 0: 2 song rows x 2 phrases = 64; track 1: 2 song rows x 1 phrase = 32
  CHECK(exportSelectionLengthRows(&state->project, selection) == 64);

  // Only track 0: 64 rows
  selection.trackMask = 0x01;
  CHECK(exportSelectionLengthRows(&state->project, selection) == 64);

  // Only track 1: 32 rows
  selection.trackMask = 0x02;
  CHECK(exportSelectionLengthRows(&state->project, selection) == 32);
}

TEST_CASE_FIXTURE(BounceFixture, "exportSelectionLengthRows song level stops at empty chain row 0") {
  // Chain 1 has an empty row 0: a track entering it dies immediately
  state->project.chains[0].rows[0].phrase = 0;
  state->project.chains[1].rows[1].phrase = 0; // Row 0 empty
  state->project.song[0][0] = 0;
  state->project.song[1][0] = 1;

  ExportSelection selection = {};
  selection.level = 0;
  selection.trackMask = 1;
  selection.startSongRow = 0;
  selection.endSongRow = 3;
  selection.startChainRow = 0;
  selection.endChainRow = 15;
  selection.startPhraseRow = 0;
  selection.endPhraseRow = 15;

  // Track plays song row 0 (16 rows), then dies on chain 1's empty row 0
  CHECK(exportSelectionLengthRows(&state->project, selection) == 16);
}

TEST_CASE_FIXTURE(BounceFixture, "exportSelectionLengthRows song level empty rows wait") {
  // All song rows empty in the selection: the track waits through each row
  // except the last one - sitting on an empty cell at endSongRow stops the
  // track immediately (no trailing silence past the selection end)
  ExportSelection selection = {};
  selection.level = 0;
  selection.trackMask = 1;
  selection.startSongRow = 0;
  selection.endSongRow = 2;
  selection.startChainRow = 0;
  selection.endChainRow = 15;
  selection.startPhraseRow = 0;
  selection.endPhraseRow = 15;

  // Waits through rows 0 and 1 (16 each), dies on row 2 == endSongRow
  CHECK(exportSelectionLengthRows(&state->project, selection) == 32);

  // Single empty row at the boundary: nothing is rendered
  selection.endSongRow = 0;
  CHECK(exportSelectionLengthRows(&state->project, selection) == 0);

  // Two empty rows: only the first one is waited through
  selection.endSongRow = 1;
  CHECK(exportSelectionLengthRows(&state->project, selection) == 16);
}

}
