#include "midi_recording.h"
#include "../playback_internal.h"

void MidiRecording::clear() {
  head_ = count_ = 0;
  write_.store(0, std::memory_order_relaxed);
  read_.store(0, std::memory_order_relaxed);
  overflow_.store(false, std::memory_order_relaxed);
  eraseTrack_.store(-1, std::memory_order_relaxed);
  erasedPhrase_ = EMPTY_VALUE_16;
  erasedTrack_ = -1;
}

void MidiRecording::observe(const PlaybackState& playback, uint64_t timestamp) {
  Tick& tick = ticks_[head_];
  tick.timestamp = timestamp;
  for (int t = 0; t < PROJECT_MAX_TRACKS; ++t) {
    Position& position = tick.tracks[t];
    position = {EMPTY_VALUE_16, 0, 0};
    if (t >= playback.p->tracksCount) continue;
    const auto& track = playback.tracks[t];
    if (track.mode == PlaybackMode::none || track.mode == PlaybackMode::stopped || track.mode == PlaybackMode::phraseRow ||
        track.songRow < 0 || track.songRow >= PROJECT_MAX_LENGTH ||
        track.chainRow < 0 || track.chainRow >= 16 || track.phraseRow < 0 || track.phraseRow >= 16) continue;
    int phrase = playbackResolvePhraseAt(playback.p, track.songRow, t, track.chainRow);
    if (phrase >= PROJECT_MAX_PHRASES) continue;
    position = {(uint16_t)phrase, (uint8_t)track.phraseRow,
                (uint16_t)(track.frameCounter > 65535 ? 65535 : track.frameCounter)};
  }
  int eraseTrack = eraseTrack_.load(std::memory_order_relaxed);
  if (eraseTrack >= 0 && eraseTrack < PROJECT_MAX_TRACKS) {
    const auto& position = tick.tracks[eraseTrack];
    if (position.phrase != EMPTY_VALUE_16 &&
        (erasedTrack_ != eraseTrack || erasedPhrase_ != position.phrase || erasedRow_ != position.row)) {
      unsigned write = write_.load(std::memory_order_relaxed);
      unsigned next = (write + 1) % capacity;
      if (next == read_.load(std::memory_order_acquire)) overflow_.store(true, std::memory_order_relaxed);
      else {
        events_[write] = {position.phrase, position.row, EMPTY_VALUE_8, EMPTY_VALUE_8, 0, 0, 1};
        write_.store(next, std::memory_order_release);
        erasedTrack_ = eraseTrack;
        erasedPhrase_ = position.phrase;
        erasedRow_ = position.row;
      }
    }
  } else {
    erasedTrack_ = -1;
    erasedPhrase_ = EMPTY_VALUE_16;
  }
  head_ = (head_ + 1) % capacity;
  if (count_ < capacity) ++count_;
}

void MidiRecording::record(int track, uint8_t note, uint8_t instrument, uint8_t velocity,
                           uint64_t timestamp, MidiRecordMode mode) {
  if (!count_ || track < 0 || track >= PROJECT_MAX_TRACKS) return;
  unsigned oldest = (head_ + capacity - count_) % capacity;
  if (timestamp < ticks_[oldest].timestamp) {
    overflow_.store(true, std::memory_order_relaxed);
    return;
  }
  unsigned chosen = oldest;
  for (unsigned i = 1; i < count_; ++i) {
    unsigned next = (oldest + i) % capacity;
    if (ticks_[next].timestamp > timestamp) break;
    chosen = next;
  }
  const Position& position = ticks_[chosen].tracks[track];
  if (position.phrase == EMPTY_VALUE_16) return;
  if (mode == MidiRecordMode::free && position.tick > 255) {
    overflow_.store(true, std::memory_order_relaxed);
    return;
  }
  unsigned write = write_.load(std::memory_order_relaxed);
  unsigned next = (write + 1) % capacity;
  if (next == read_.load(std::memory_order_acquire)) {
    overflow_.store(true, std::memory_order_relaxed);
    return;
  }
  events_[write] = {position.phrase, position.row, note, instrument, velocity,
                   (uint8_t)(mode == MidiRecordMode::free ? position.tick : 0)};
  write_.store(next, std::memory_order_release);
}

int MidiRecording::consume(MidiRecordEvent* event) {
  unsigned read = read_.load(std::memory_order_relaxed);
  if (read == write_.load(std::memory_order_acquire)) return 0;
  *event = events_[read];
  read_.store((read + 1) % capacity, std::memory_order_release);
  return 1;
}

bool midiRecordWriteRow(PhraseRow& row, uint8_t note, uint8_t instrument,
                        uint8_t velocity, uint8_t delay) {
  int column = -1;
  for (int i = 2; i >= 0; --i) if (row.fx[i].command == fxDEL) column = i;
  if (delay && column < 0) {
    for (int i = 2; i >= 0; --i) if (row.fx[i].command == FX_ID_EMPTY) { column = i; break; }
    if (column < 0) return false;
  }
  for (auto& fx : row.fx)
    if (fx.command == fxDEL) fx = {FX_ID_EMPTY, 0};
  if (delay) row.fx[column] = {fxDEL, delay};
  row.note = note;
  row.instrument = instrument;
  row.volume = velocity;
  return true;
}

void midiRecordStep(Phrase& phrase, int& cursor, uint8_t note, uint8_t instrument, uint8_t velocity) {
  midiRecordWriteRow(phrase.rows[cursor], note, instrument, velocity, 0);
  if (cursor < 15) ++cursor;
}

bool midiRecordApply(PhraseRow& row, const MidiRecordEvent& event) {
  if (event.erase) {
    row.note = row.instrument = EMPTY_VALUE_8;
    row.volume = EMPTY_VALUE_16;
    for (auto& fx : row.fx) if (fx.command == fxDEL) fx = {FX_ID_EMPTY, 0};
    return true;
  }
  if (event.note == NOTE_OFF) {
    if (row.note != EMPTY_VALUE_8 && row.note != NOTE_OFF) return true;
    if (!midiRecordWriteRow(row, NOTE_OFF, EMPTY_VALUE_8, 0, event.delay)) return false;
    row.volume = EMPTY_VALUE_16;
    return true;
  }
  return midiRecordWriteRow(row, event.note, event.instrument, event.velocity, event.delay);
}
