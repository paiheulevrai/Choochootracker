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
  uint16_t eraseMask = 0;
  uint32_t sequence = 0;
  uint8_t track = 0;
  bool replaceExisting = false;
};

// Audio owns the bounded tick history; completed edits cross to the UI via SPSC.
class MidiRecording {
 public:
  void observe(const PlaybackState& playback, uint64_t timestamp, bool recording = true);
  void record(int track, uint8_t note, uint8_t instrument, uint8_t velocity,
              uint64_t timestamp, MidiRecordMode mode);
  bool finish(int track, uint64_t timestamp, unsigned reserved = 0);
  void cancel(int track);
  int consume(MidiRecordEvent* event);
  bool overflow() const { return overflow_.load(std::memory_order_relaxed); }
  void clear();
 private:
  static constexpr unsigned capacity = 64;
  struct Position { uint16_t phrase; uint8_t row; uint16_t tick; };
  struct Tick { uint64_t timestamp; Position tracks[PROJECT_MAX_TRACKS]; };
  struct Span { uint16_t phrase; uint16_t rows; };
  struct Held {
    bool active = false, overflow = false;
    uint64_t started = 0;
    Position origin{};
    uint32_t sequence = 0;
    Span spans[capacity]{};
    unsigned count = 0;
  };
  void addHeldPosition(Held& held, const Position& position);
  bool finishAt(int track, uint64_t timestamp, unsigned reserved, const Position* next);
  Tick ticks_[capacity]{};
  unsigned head_ = 0, count_ = 0;
  Held held_[PROJECT_MAX_TRACKS]{};
  uint8_t heldMask_ = 0;
  uint32_t sequence_ = 0;
  MidiRecordEvent events_[capacity]{};
  std::atomic<unsigned> write_{0}, read_{0};
  std::atomic<bool> overflow_{false};
};

// Returns false without modifying the row if DEL cannot fit.
bool midiRecordWriteRow(PhraseRow& row, uint8_t note, uint8_t instrument,
                        uint8_t velocity, uint8_t delay);
void midiRecordStep(Phrase& phrase, int& cursor, uint8_t note, uint8_t instrument, uint8_t velocity);
bool midiRecordApply(PhraseRow& row, const MidiRecordEvent& event);
void midiRecordEraseNote(PhraseRow& row);

#endif
