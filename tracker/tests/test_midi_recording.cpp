#include "doctest.h"
#include "chipnomad_lib.h"
#include "midi/midi_recording.h"
#include "midi/midi_router.h"
#include "pitch_table_utils.h"
#include "project_utils.h"
#include "synth/braids_voice.h"
#include "synth/mme_voice.h"
#include <cstring>
#include <vector>
#include <chrono>
#include <filesystem>

namespace {
struct RecordingFixture {
  ChipNomadState* state = chipnomadCreate();
  MidiRecording recording;
  RecordingFixture() {
    auto& p = state->project;
    p.tracksCount = 1;
    p.song[0][0] = 0;
    p.chains[0].rows[0].phrase = 0;
    p.chains[0].rows[1].phrase = 1;
    playbackInit(&state->playbackState, &p);
    auto& track = state->playbackState.tracks[0];
    track.mode = PlaybackMode::song;
    track.songRow = track.chainRow = track.phraseRow = 0;
  }
  ~RecordingFixture() { chipnomadDestroy(state); }
  void tick(uint64_t timestamp, int row, int offset, int chainRow = 0) {
    auto& track = state->playbackState.tracks[0];
    track.phraseRow = row;
    track.frameCounter = offset;
    track.chainRow = chainRow;
    recording.observe(state->playbackState, timestamp);
  }
};

struct AudioFixture {
  ChipNomadState* state = chipnomadCreate();
  MidiBackend backend{};
  uint64_t now = 1000000;
  float audio[2000]{};
  AudioFixture() {
    backend.userdata = this;
    backend.nowMicros = [](void* data) { return static_cast<AudioFixture*>(data)->now; };
    backend.scheduleOutput = [](void*, const MidiEvent*, uint64_t) {};
    midiRouterSetBackend(&backend);
    auto& p = state->project;
    p.tickRate = 48;
    p.chipType = ChipType::AY;
    p.chipsCount = 1;
    p.tracksCount = projectGetTotalTracks(&p);
    p.chipSetup.ay = {1773400, 0, StereoModeAY::ABC, 50, 0};
    calculatePitchTableAY(&p);
    p.instruments[0].type = InstrumentType::AY1;
    p.instruments[1].type = InstrumentType::AY1;
    p.song[0][0] = 0;
    p.chains[0].rows[0].phrase = 0;
    for (int i = 0; i < 16; ++i) {
      p.phrases[0].rows[i].note = 40 + i;
      p.phrases[0].rows[i].instrument = 0;
    }
    chipnomadInitChips(state, 48000, nullptr);
    playbackInit(&state->playbackState, &state->audioProject);
  }
  ~AudioFixture() {
    chipnomadDestroy(state);
    midiRouterSetBackend(nullptr);
  }
  void render() {
    chipnomadRender(state, audio, 1000);
    now += 20833;
  }
};
}

TEST_SUITE("MIDI recording") {
TEST_CASE("MIDI browser engines prepare one track without shrinking normal voice pools") {
  auto* state = chipnomadCreate();
  projectInitAY(&state->project);
  state->project.tracksCount = state->project.chipsCount = 1;
  for (auto type : {InstrumentType::Braids, InstrumentType::OPLL, InstrumentType::DX7}) {
    getInstrumentFunctions(state->project.instruments[0].type).free(&state->project.instruments[0]);
    getInstrumentFunctions(type).init(&state->project.instruments[0]);
    for (int tracks : {1, PROJECT_MAX_TRACKS}) {
      if (tracks == 1) chipnomadInitChips(state, 48000, nullptr, 1);
      else chipnomadInitChips(state, 48000, nullptr);
      for (int track = 0; track < PROJECT_MAX_TRACKS; ++track) {
        for (int slot = 0; slot < CHORD_MAX_VOICES; ++slot) {
          const bool allocated = type == InstrumentType::Braids ? state->braidsVoices[track][slot] != nullptr :
            type == InstrumentType::OPLL ? state->opllVoices[track][slot] != nullptr :
            state->dx7Voices[track][slot] != nullptr;
          CHECK(allocated == (track < tracks));
        }
      }
    }
  }
  chipnomadDestroy(state);
}

TEST_CASE_FIXTURE(RecordingFixture, "Quantized recording uses the timestamp's row, not the current playhead") {
  tick(100000, 0, 0);
  tick(120000, 0, 1);
  tick(140000, 1, 0);
  recording.record(0, 48, 3, 92, 131000, MidiRecordMode::quantized);
  MidiRecordEvent event{};
  REQUIRE(recording.consume(&event));
  CHECK(event.phrase == 0);
  CHECK(event.row == 0);
  CHECK(event.delay == 0);
  CHECK(event.note == 48);
  CHECK(event.instrument == 3);
  CHECK(event.velocity == 92);
  recording.record(0, 52, 3, 92, 139999, MidiRecordMode::quantized);
  REQUIRE(recording.consume(&event));
  CHECK(event.row == 0);
  CHECK(event.delay == 0);
  recording.record(0, 55, 3, 92, 140000, MidiRecordMode::quantized);
  REQUIRE(recording.consume(&event));
  CHECK(event.row == 1);
  CHECK(event.delay == 0);
}

TEST_CASE_FIXTURE(RecordingFixture, "Free recording floors to the previous tick, never the next phrase") {
  tick(100000, 15, 4);
  tick(120000, 15, 5);
  tick(140000, 0, 0, 1);
  MidiRecordEvent event{};
  recording.record(0, 48, 0, 100, 129999, MidiRecordMode::free);
  REQUIRE(recording.consume(&event));
  CHECK(event.phrase == 0);
  CHECK(event.row == 15);
  CHECK(event.delay == 5);
  recording.record(0, 52, 0, 100, 130000, MidiRecordMode::free);
  REQUIRE(recording.consume(&event));
  CHECK(event.phrase == 0);
  CHECK(event.row == 15);
  CHECK(event.delay == 5);
  recording.record(0, 55, 0, 100, 139999, MidiRecordMode::free);
  REQUIRE(recording.consume(&event));
  CHECK(event.phrase == 0);
  CHECK(event.row == 15);
  CHECK(event.delay == 5);
  recording.record(0, 57, 0, 100, 140000, MidiRecordMode::free);
  REQUIRE(recording.consume(&event));
  CHECK(event.phrase == 1);
  CHECK(event.row == 0);
  CHECK(event.delay == 0);
}

TEST_CASE_FIXTURE(RecordingFixture, "Free recording uses actual ticks under groove and speed changes") {
  tick(100000, 2, 0);
  tick(117000, 2, 1);
  tick(128000, 2, 2);
  tick(139000, 3, 0);
  recording.record(0, 50, 1, 110, 130000, MidiRecordMode::free);
  MidiRecordEvent event{};
  REQUIRE(recording.consume(&event));
  CHECK(event.row == 2);
  CHECK(event.delay == 2);
}

TEST_CASE_FIXTURE(RecordingFixture, "Simplified recording resolves pattern bars rather than classic chains") {
  auto& project = state->project;
  project.viewMode = ViewMode::simplified;
  project.patterns[0].length = 2;
  project.patterns[0].bars[1].phrase = 7;
  tick(100000, 3, 2, 1);
  recording.record(0, 48, 0, 100, 100000, MidiRecordMode::free);
  MidiRecordEvent event{};
  REQUIRE(recording.consume(&event));
  CHECK(event.phrase == 7);
  CHECK(event.row == 3);
  CHECK(event.delay == 2);
}

TEST_CASE_FIXTURE(RecordingFixture, "Recording never silently clamps stale input or unrepresentable DEL") {
  tick(100000, 0, 256);
  MidiRecordEvent event{};
  recording.record(0, 48, 0, 100, 100000, MidiRecordMode::free);
  CHECK_FALSE(recording.consume(&event));
  CHECK(recording.overflow());
  recording.clear();
  tick(100000, 0, 0);
  recording.record(0, 48, 0, 100, 99999, MidiRecordMode::quantized);
  CHECK_FALSE(recording.consume(&event));
  CHECK(recording.overflow());
}

TEST_CASE_FIXTURE(RecordingFixture, "Record queue is bounded and overflow is reported") {
  tick(100000, 0, 0);
  for (int i = 0; i < 80; ++i)
    recording.record(0, 48, 0, 100, 100000, MidiRecordMode::quantized);
  CHECK(recording.overflow());
  MidiRecordEvent event{};
  int count = 0;
  while (recording.consume(&event)) ++count;
  CHECK(count == 63);
}

TEST_CASE("MIDI recording replaces one note per step, manages DEL, and preserves unrelated FX") {
  Phrase phrase;
  phraseClear(&phrase);
  auto& row = phrase.rows[3];
  row.fx[0] = {fxPAN, 80};
  REQUIRE(midiRecordWriteRow(row, 48, 1, 92, 3));
  CHECK(row.fx[2].command == fxDEL);
  CHECK(row.fx[2].value == 3);
  REQUIRE(midiRecordWriteRow(row, 52, 2, 101, 4));
  CHECK(row.note == 52);
  CHECK(row.instrument == 2);
  CHECK(row.volume == 101);
  CHECK(row.fx[0].command == fxPAN);
  CHECK(row.fx[2].value == 4);
  REQUIRE(midiRecordWriteRow(row, 55, 2, 110, 0));
  CHECK(row.fx[2].command == FX_ID_EMPTY);
  row.fx[1] = {fxPIT, 1};
  row.fx[2] = {fxVOL, 1};
  PhraseRow saved = row;
  CHECK_FALSE(midiRecordWriteRow(row, 60, 3, 127, 2));
  CHECK(std::memcmp(&row, &saved, sizeof(row)) == 0);
}

TEST_CASE("MIDI step entry advances once per note and stays at the last row") {
  Phrase phrase;
  phraseClear(&phrase);
  int cursor = 14;
  midiRecordStep(phrase, cursor, 48, 1, 96);
  CHECK(cursor == 15);
  CHECK(phrase.rows[14].note == 48);
  CHECK(phrase.rows[14].instrument == 1);
  CHECK(phrase.rows[14].volume == 96);
  midiRecordStep(phrase, cursor, 52, 2, 100);
  midiRecordStep(phrase, cursor, 55, 2, 110);
  CHECK(cursor == 15);
  CHECK(phrase.rows[15].note == 55);
}

TEST_CASE("MIDI Note Off fills empty steps, with musical notes taking priority in either arrival order") {
  Phrase phrase;
  phraseClear(&phrase);
  auto& row = phrase.rows[0];
  MidiRecordEvent off{0, 0, NOTE_OFF, 1, 0, 2};
  REQUIRE(midiRecordApply(row, off));
  CHECK(row.note == NOTE_OFF);
  CHECK(row.instrument == EMPTY_VALUE_8);
  CHECK(row.volume == EMPTY_VALUE_16);
  CHECK(row.fx[2].command == fxDEL);
  CHECK(row.fx[2].value == 2);
  MidiRecordEvent on{0, 0, 52, 1, 100, 1};
  REQUIRE(midiRecordApply(row, on));
  REQUIRE(midiRecordApply(row, off));
  CHECK(row.note == 52);
  CHECK(row.volume == 100);
  CHECK(row.fx[2].value == 1);
  off.delay = 0;
  REQUIRE(midiRecordApply(phrase.rows[1], off));
  CHECK(phrase.rows[1].note == NOTE_OFF);
  CHECK(phrase.rows[1].fx[2].command == FX_ID_EMPTY);
}

TEST_CASE_FIXTURE(RecordingFixture, "Observing playback never queues MIDI edits without recorded notes") {
  tick(100000, 3, 0);
  tick(120000, 3, 1);
  tick(140000, 4, 0);
  MidiRecordEvent event{};
  CHECK_FALSE(recording.consume(&event));
  tick(150000, 0, 0, 1);
  CHECK_FALSE(recording.consume(&event));
  CHECK_FALSE(recording.overflow());
}

TEST_CASE_FIXTURE(AudioFixture, "Audio records a MIDI release as NOTE OFF at its arrival step and tick") {
  REQUIRE(chipnomadQueuePlaybackStartSong(state, 0, 0, 1));
  for (int i = 0; i < 10; ++i) render();
  REQUIRE(chipnomadQueueMidiInput(state, 0, 60, 1, 100, now, 0, MidiRecordMode::free));
  render();
  REQUIRE(chipnomadQueueMidiInput(state, 0, 0, 0, 0, 1151000, 1, MidiRecordMode::free));
  render();
  MidiRecordEvent event{};
  REQUIRE(chipnomadConsumeMidiRecordEvent(state, &event));
  CHECK(event.note == NOTE_OFF);
  CHECK(event.row == 1);
  CHECK(event.delay == 1);
}

TEST_CASE_FIXTURE(AudioFixture, "External notes win over internal triggers without moving the playhead") {
  REQUIRE(chipnomadQueuePlaybackStartSong(state, 0, 0, 1));
  render();
  auto& track = state->playbackState.tracks[0];
  CHECK(track.note.instrument == 0);
  REQUIRE(chipnomadQueueMidiInput(state, 0, 60, 1, 100, now, 0, MidiRecordMode::quantized));
  render();
  CHECK(track.note.instrument == 1);
  CHECK(track.note.pitchBase == 60);
  CHECK(track.mode == PlaybackMode::song);
  for (int i = 0; i < 12; ++i) render();
  CHECK(track.phraseRow >= 2);
  CHECK(track.note.instrument == 1);
  CHECK(track.note.pitchBase == 60);
  REQUIRE(chipnomadQueueMidiInput(state, 0, 0, 0, 0, now, 0, MidiRecordMode::quantized));
  render();
  CHECK_FALSE(track.midiHeld);
  for (int i = 0; i < 6; ++i) render();
  CHECK(track.note.instrument == 0);
}

TEST_CASE_FIXTURE(AudioFixture, "MIDI notes and releases apply in the next block without waiting for a tracker tick") {
  REQUIRE(chipnomadQueuePlaybackStartSong(state, 0, 0, 1));
  REQUIRE(chipnomadRender(state, audio, 17) == 17);
  auto& track = state->playbackState.tracks[0];
  const int row = track.phraseRow, tick = track.frameCounter;
  const float remaining = state->frameSampleCounter;
  REQUIRE(remaining > 64);
  REQUIRE(chipnomadQueueMidiInput(state, 0, 60, 1, 100, now + 500, 1, MidiRecordMode::free));
  REQUIRE(chipnomadRender(state, audio, 32) == 32);
  CHECK(track.note.instrument == 1);
  CHECK(track.note.pitchFinal == 60);
  CHECK(track.midiHeld);
  CHECK(track.phraseRow == row);
  CHECK(track.frameCounter == tick);
  CHECK(state->frameSampleCounter == doctest::Approx(remaining - 32));
  MidiRecordEvent event{};
  REQUIRE(chipnomadConsumeMidiRecordEvent(state, &event));
  CHECK(event.note == 60);
  CHECK(event.row == row);
  CHECK(event.delay == tick);
  REQUIRE(chipnomadQueueMidiInput(state, 0, 0, 0, 0, now + 1000, 1, MidiRecordMode::free));
  REQUIRE(chipnomadRender(state, audio, 32) == 32);
  CHECK_FALSE(track.midiHeld);
  CHECK(track.frameCounter == tick);
  REQUIRE(chipnomadConsumeMidiRecordEvent(state, &event));
  CHECK(event.note == NOTE_OFF);
  CHECK(event.row == row);
  CHECK(event.delay == tick);
}

TEST_CASE_FIXTURE(AudioFixture, "MIDI overflow releases held tracks and does not lose a final Note Off") {
  REQUIRE(chipnomadQueuePlaybackStartSong(state, 0, 0, 1));
  REQUIRE(chipnomadRender(state, audio, 17) == 17);
  for (int i = 0; i < 63; ++i)
    REQUIRE(chipnomadQueueMidiInput(state, 0, 60, 1, 100, now, 0, MidiRecordMode::free));
  CHECK_FALSE(chipnomadQueueMidiInput(state, 0, 0, 0, 0, now, 0, MidiRecordMode::free));
  CHECK(chipnomadGetCommandOverflow(state));
  REQUIRE(chipnomadRender(state, audio, 32) == 32);
  CHECK_FALSE(state->playbackState.tracks[0].midiHeld);
  REQUIRE(chipnomadQueueMidiInput(state, 0, 61, 1, 100, now, 0, MidiRecordMode::free));
  REQUIRE(chipnomadRender(state, audio, 32) == 32);
  CHECK(state->playbackState.tracks[0].midiHeld);
  CHECK(state->playbackState.tracks[0].note.pitchFinal == 61);
}

TEST_CASE_FIXTURE(AudioFixture, "Off-tick MIDI retriggers do not advance free-running LFOs or tables") {
  state->project.instruments[1].modulation[0] =
    {ModulationType::LFO, 2, 64, 0, static_cast<uint8_t>(LFOTrigger::free), 24, 0, 0};
  chipnomadInitChips(state, 48000, nullptr);
  REQUIRE(chipnomadQueueMidiInput(state, 0, 60, 1, 100, now, 0, MidiRecordMode::free));
  REQUIRE(chipnomadRender(state, audio, 17) == 17);
  auto& track = state->playbackState.tracks[0];
  const auto counter = track.note.modulation[0].counter;
  const auto value = track.note.modulation[0].outValue;
  const auto tableCounter = track.note.instrumentTable.counters[0];
  REQUIRE(chipnomadQueueMidiInput(state, 0, 62, 1, 100, now, 0, MidiRecordMode::free));
  REQUIRE(chipnomadRender(state, audio, 32) == 32);
  CHECK(track.note.pitchBase == 62);
  CHECK(track.note.modulation[0].counter == counter);
  CHECK(track.note.modulation[0].outValue == value);
  CHECK(track.note.instrumentTable.counters[0] == tableCounter);
}

TEST_CASE_FIXTURE(AudioFixture, "Off-tick MIDI audition replaces a standalone chip preview immediately") {
  Instrument candidate{};
  getInstrumentFunctions(InstrumentType::OPLL).init(&candidate);
  REQUIRE(chipnomadQueueOPLLPreview(state, 0, &candidate.chip.opll));
  REQUIRE(chipnomadRender(state, audio, 17) == 17);
  REQUIRE(state->opllPreviewTrack == 0);
  REQUIRE(state->frameSampleCounter > 32);
  REQUIRE(chipnomadQueueMidiInput(state, 0, 60, 1, 100, now, 0, MidiRecordMode::free));
  REQUIRE(chipnomadRender(state, audio, 32) == 32);
  CHECK(state->playbackState.tracks[0].midiHeld);
  CHECK(state->opllPreviewTrack == -1);
}

TEST_CASE_FIXTURE(AudioFixture, "Audio thread records delayed input against past ticks, without editing the UI project") {
  REQUIRE(chipnomadQueuePlaybackStartSong(state, 0, 0, 1));
  for (int i = 0; i < 10; ++i) render();
  REQUIRE(chipnomadQueueMidiInput(state, 0, 60, 1, 97, 1051000, 1, MidiRecordMode::free));
  render();
  MidiRecordEvent event{};
  REQUIRE(chipnomadConsumeMidiRecordEvent(state, &event));
  CHECK(event.phrase == 0);
  CHECK(event.row == 0);
  CHECK(event.delay == 2);
  CHECK(event.note == 60);
  CHECK(event.velocity == 97);
  CHECK(state->project.phrases[0].rows[0].note == 40);
}

TEST_CASE_FIXTURE(AudioFixture, "MIDI note preview does not start transport and stop clears external ownership") {
  REQUIRE(chipnomadQueueMidiInput(state, 0, 60, 1, 90, now, 0, MidiRecordMode::quantized));
  render();
  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::phraseRow);
  CHECK(state->playbackState.tracks[0].midiHeld);
  for (int i = 0; i < 8; ++i) render();
  CHECK_FALSE(chipnomadGetPlaybackStatus(state)->transportPlaying);
  CHECK(chipnomadGetPlaybackStatus(state)->linkPhaseTicks == 0);
  chipnomadQueuePlaybackStop(state);
  render();
  CHECK_FALSE(state->playbackState.tracks[0].midiHeld);
  CHECK(state->playbackState.tracks[0].mode == PlaybackMode::stopped);
}

TEST_CASE_FIXTURE(AudioFixture, "Stopped MIDI audition switches instruments and releases volume ADSR without recording") {
  for (int instrument = 0; instrument < 2; ++instrument) {
    auto& inst = state->project.instruments[instrument];
    getInstrumentFunctions(InstrumentType::AY1).init(&inst);
    inst.modulation[0] = {ModulationType::ADSR, 1, 127, 0, 0, 255, 2, 0};
  }
  REQUIRE(chipnomadQueueProjectRefresh(state));
  auto& track = state->playbackState.tracks[0];
  for (int instrument : {1, 0, 1}) {
    REQUIRE(chipnomadQueueMidiInput(state, 0, 48, instrument, 100, now, 0, MidiRecordMode::quantized));
    for (int i = 0; i < 8; ++i) render();
    CHECK(track.note.instrument == instrument);
    CHECK(track.note.modulation[0].outValue > 0);
    REQUIRE(chipnomadQueueMidiInput(state, 0, 0, 0, 0, now, 0, MidiRecordMode::quantized));
    for (int i = 0; i < 8; ++i) render();
    CHECK_FALSE(track.midiHeld);
    CHECK(track.note.modulation[0].outValue == 0);
    MidiRecordEvent recorded{};
    CHECK_FALSE(chipnomadConsumeMidiRecordEvent(state, &recorded));
  }
}

TEST_CASE_FIXTURE(AudioFixture, "Stopped MIDI audition releases Braids and MME internal ADSR") {
  for (auto type : {InstrumentType::Braids, InstrumentType::MME}) {
    CAPTURE((int)type);
    auto& p = state->project;
    getInstrumentFunctions(type).init(&p.instruments[1]);
    InstrumentVoicePostSettings& envelope = type == InstrumentType::Braids
      ? static_cast<InstrumentVoicePostSettings&>(p.instruments[1].chip.braids)
      : static_cast<InstrumentVoicePostSettings&>(p.instruments[1].chip.mme);
    envelope.attack = envelope.decay = 0;
    envelope.sustain = 255;
    envelope.release = 16;
    p.linearPitch = 1;
    calculateLinearPitchTable12TET(&p);
    chipnomadInitChips(state, 48000, nullptr);
    REQUIRE(chipnomadQueueMidiInput(state, 0, 48, 1, 100, now, 0, MidiRecordMode::quantized));
    for (int i = 0; i < 8; ++i) render();
    auto level = [&]() {
      return type == InstrumentType::Braids ? state->braidsVoices[0][0]->envelopeLevel()
                                           : state->mmeVoices[0][0]->envelopeLevel();
    };
    REQUIRE(level() > .5f);
    REQUIRE(chipnomadQueueMidiInput(state, 0, 0, 0, 0, now, 0, MidiRecordMode::quantized));
    for (int i = 0; i < 8; ++i) render();
    CHECK_FALSE(state->playbackState.tracks[0].midiHeld);
    CHECK(level() == 0);
    chipnomadQueuePlaybackStop(state);
    render();
  }
}

TEST_CASE_FIXTURE(AudioFixture, "Recorded note instrument velocity and DEL survive project save and load") {
  struct SavedProject {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
      ("cct-midi-record-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".cct");
    ~SavedProject() { std::filesystem::remove(path); }
  } saved;
  auto& row = state->project.phrases[0].rows[4];
  REQUIRE(midiRecordWriteRow(row, 57, 1, 103, 3));
  REQUIRE(projectSave(&state->project, saved.path.string().c_str()) == 0);
  row.note = EMPTY_VALUE_8;
  REQUIRE(projectLoad(&state->project, saved.path.string().c_str()) == 0);
  const auto& loaded = state->project.phrases[0].rows[4];
  CHECK(loaded.note == 57);
  CHECK(loaded.instrument == 1);
  CHECK(loaded.volume == 103);
  CHECK(loaded.fx[2].command == fxDEL);
  CHECK(loaded.fx[2].value == 3);
}
}
