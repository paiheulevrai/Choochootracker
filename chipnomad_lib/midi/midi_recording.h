#ifndef CHIPNOMAD_MIDI_RECORDING_H
#define CHIPNOMAD_MIDI_RECORDING_H

#include "../playback.h"
#include <atomic>

enum class MidiRecordMode : uint8_t { quantized, free };

struct MidiRecordEvent {
  uint16_t phrase;
  uint8_t row;
  uint8_t note;
  uint8_t instrument;
  uint8_t velocity;
  uint8_t delay;
  uint8_t erase = 0;
};

// Audio owns the bounded tick history; completed edits cross to the UI via SPSC.
class MidiRecording {
 public:
  void observe(const PlaybackState& playback, uint64_t timestamp);
  void record(int track, uint8_t note, uint8_t instrument, uint8_t velocity,
              uint64_t timestamp, MidiRecordMode mode);
  int consume(MidiRecordEvent* event);
  bool overflow() const { return overflow_.load(std::memory_order_relaxed); }
  void clear();
  void setEraseTrack(int track) { eraseTrack_.store(track, std::memory_order_relaxed); }
 private:
  static constexpr unsigned capacity = 64;
  struct Position { uint16_t phrase; uint8_t row; uint16_t tick; };
  struct Tick { uint64_t timestamp; Position tracks[PROJECT_MAX_TRACKS]; };
  Tick ticks_[capacity]{};
  unsigned head_ = 0, count_ = 0;
  MidiRecordEvent events_[capacity]{};
  std::atomic<unsigned> write_{0}, read_{0};
  std::atomic<bool> overflow_{false};
  std::atomic<int> eraseTrack_{-1};
  uint16_t erasedPhrase_ = EMPTY_VALUE_16;
  uint8_t erasedRow_ = 0;
  int erasedTrack_ = -1;
};

// Returns false without modifying the row if DEL cannot fit.
bool midiRecordWriteRow(PhraseRow& row, uint8_t note, uint8_t instrument,
                        uint8_t velocity, uint8_t delay);
void midiRecordStep(Phrase& phrase, int& cursor, uint8_t note, uint8_t instrument, uint8_t velocity);
bool midiRecordApply(PhraseRow& row, const MidiRecordEvent& event);

#endif
