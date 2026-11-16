#include "midi_recording.h"
#include "../playback_internal.h"

void MidiRecording::clear() {
  head_ = count_ = 0;
  for (auto& held : held_) held = {};
  heldMask_ = 0;
  write_.store(0, std::memory_order_relaxed);
  read_.store(0, std::memory_order_relaxed);
  overflow_.store(false, std::memory_order_relaxed);
}

void MidiRecording::addHeldPosition(Held& held, const Position& position) {
  if (position.phrase == EMPTY_VALUE_16 ||
      (position.phrase == held.origin.phrase && position.row == held.origin.row)) return;
  unsigned i = 0;
  while (i < held.count && held.spans[i].phrase != position.phrase) ++i;
  if (i == held.count) {
    if (held.count == capacity) { held.overflow = true; return; }
    held.spans[held.count++] = {position.phrase, 0};
  }
  held.spans[i].rows |= uint16_t(1u << position.row);
}

void MidiRecording::observe(const PlaybackState& playback, uint64_t timestamp, bool recording) {
  if (heldMask_)
    for (int t = 0; t < PROJECT_MAX_TRACKS; ++t) {
      if ((heldMask_ & (1u << t)) && (!recording || t >= playback.p->tracksCount ||
          playback.tracks[t].mode == PlaybackMode::none ||
          playback.tracks[t].mode == PlaybackMode::stopped ||
          playback.tracks[t].mode == PlaybackMode::phraseRow))
        finish(t, timestamp);
    }
  if (heldMask_ && count_ == capacity) {
    const Tick& old = ticks_[head_];
    for (int t = 0; t < PROJECT_MAX_TRACKS; ++t)
      if (held_[t].active && old.timestamp >= held_[t].started)
        addHeldPosition(held_[t], old.tracks[t]);
  }
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
  head_ = (head_ + 1) % capacity;
  if (count_ < capacity) ++count_;
}

void MidiRecording::cancel(int track) {
  if (track >= 0 && track < PROJECT_MAX_TRACKS) {
    held_[track] = {};
    heldMask_ &= ~(1u << track);
  }
}

bool MidiRecording::finish(int track, uint64_t timestamp, unsigned reserved) {
  return finishAt(track, timestamp, reserved, nullptr);
}

bool MidiRecording::finishAt(int track, uint64_t timestamp, unsigned reserved, const Position* nextPosition) {
  if (track < 0 || track >= PROJECT_MAX_TRACKS || !held_[track].active) return false;
  Held& held = held_[track];
  unsigned oldest = (head_ + capacity - count_) % capacity;
  bool replace = false;
  if (count_ && timestamp < ticks_[oldest].timestamp) held.overflow = true;
  if (!held.overflow) {
    for (unsigned i = 0; i < count_; ++i) {
      const Tick& tick = ticks_[(oldest + i) % capacity];
      if (tick.timestamp >= held.started && tick.timestamp < timestamp)
        addHeldPosition(held, tick.tracks[track]);
    }
    if (!held.overflow && nextPosition) {
      for (unsigned i = 0; i < held.count; ++i) {
        if (held.spans[i].phrase != nextPosition->phrase) continue;
        const uint16_t bit = uint16_t(1u << nextPosition->row);
        if (!(held.spans[i].rows & bit)) break;
        replace = true;
        held.spans[i].rows &= ~bit;
        if (!held.spans[i].rows) held.spans[i] = held.spans[--held.count];
        break;
      }
    }
  }
  unsigned write = write_.load(std::memory_order_relaxed);
  const unsigned read = read_.load(std::memory_order_acquire);
  const unsigned available = (read + capacity - write - 1) % capacity;
  if (held.overflow || held.count + reserved > available) {
    overflow_.store(true, std::memory_order_relaxed);
    replace = false;
  } else {
    for (unsigned i = 0; i < held.count; ++i) {
      events_[write] = {held.spans[i].phrase, 0, EMPTY_VALUE_8, 0, 0, 0,
                        held.spans[i].rows, held.sequence, (uint8_t)track};
      write = (write + 1) % capacity;
    }
    write_.store(write, std::memory_order_release);
  }
  held.active = false;
  heldMask_ &= ~(1u << track);
  return replace;
}

void MidiRecording::record(int track, uint8_t note, uint8_t instrument, uint8_t velocity,
                           uint64_t timestamp, MidiRecordMode mode) {
  if (!count_ || track < 0 || track >= PROJECT_MAX_TRACKS) return;
  unsigned oldest = (head_ + capacity - count_) % capacity;
  if (timestamp < ticks_[oldest].timestamp) {
    overflow_.store(true, std::memory_order_relaxed);
    if (note == NOTE_OFF) cancel(track);
    return;
  }
  unsigned chosen = oldest;
  for (unsigned i = 1; i < count_; ++i) {
    unsigned next = (oldest + i) % capacity;
    if (ticks_[next].timestamp > timestamp) break;
    chosen = next;
  }
  const Position& position = ticks_[chosen].tracks[track];
  if (position.phrase == EMPTY_VALUE_16) {
    if (note == NOTE_OFF) cancel(track);
    return;
  }
  if (mode == MidiRecordMode::free && position.tick > 255) {
    overflow_.store(true, std::memory_order_relaxed);
    if (note == NOTE_OFF) cancel(track);
    return;
  }
  const uint32_t previousSequence = held_[track].active ? held_[track].sequence : 0;
  bool replace = finishAt(track, timestamp, 1, &position);
  unsigned write = write_.load(std::memory_order_relaxed);
  unsigned next = (write + 1) % capacity;
  if (next == read_.load(std::memory_order_acquire)) {
    overflow_.store(true, std::memory_order_relaxed);
    return;
  }
  uint32_t sequence = previousSequence;
  if (note != NOTE_OFF) {
    if (++sequence_ == 0) ++sequence_;
    sequence = sequence_;
  }
  events_[write] = {position.phrase, position.row, note, instrument, velocity,
                   (uint8_t)(mode == MidiRecordMode::free ? position.tick : 0), 0, sequence,
                   (uint8_t)track, note == NOTE_OFF && replace};
  write_.store(next, std::memory_order_release);
  if (note != NOTE_OFF) {
    held_[track] = {};
    held_[track].active = true;
    heldMask_ |= 1u << track;
    held_[track].started = timestamp;
    held_[track].origin = position;
    held_[track].sequence = sequence;
  }
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
  if (event.note == NOTE_OFF) {
    if (!event.replaceExisting && row.note != EMPTY_VALUE_8 && row.note != NOTE_OFF) return true;
    if (!midiRecordWriteRow(row, NOTE_OFF, EMPTY_VALUE_8, 0, event.delay)) return false;
    row.volume = EMPTY_VALUE_16;
    return true;
  }

  return midiRecordWriteRow(row, event.note, event.instrument, event.velocity, event.delay);
}

void midiRecordEraseNote(PhraseRow& row) {
  if (row.note == EMPTY_VALUE_8) return;
  row.note = EMPTY_VALUE_8;
  row.instrument = EMPTY_VALUE_8;
  row.volume = EMPTY_VALUE_16;
  for (auto& fx : row.fx)
    if (fx.command == fxDEL) fx = {FX_ID_EMPTY, 0};
}
