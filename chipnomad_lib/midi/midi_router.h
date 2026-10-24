#ifndef __CHIPNOMAD_LIB__MIDI_ROUTER_H__
#define __CHIPNOMAD_LIB__MIDI_ROUTER_H__

#include <stdint.h>
#include "../chord.h"
#include "../project_constants.h"

// Generic MIDI router: sits between the tracker/engine and a platform MIDI
// backend (desktop RtMidi today - see midi_backend_desktop.h - future
// Android USB MIDI, future Web MIDI). It owns musical MIDI semantics
// (event routing, input-mode selection, active-note tracking, panic,
// transport/clock master-slave); a backend only talks to the OS (enumerate/
// open/close/send/receive raw bytes, report capabilities). No RtMidi/
// Android/Web MIDI/ALSA/SDL/platform SDK type appears anywhere in this
// header or in midi_router.cpp.

// A platform-neutral MIDI event: what a backend receives/sends, decoupled
// from any driver's own representation. type mirrors a MIDI status byte's
// high nibble (0x80 Note Off, 0x90 Note On, 0xB0 CC, 0xC0 Program Change...).
struct MidiEvent {
  uint64_t timestampMicros;
  uint8_t type;
  uint8_t channel;
  uint8_t data1;
  uint8_t data2;
};

// What a platform backend must provide. A backend never decides what a
// byte means (which tracker track receives channel 3, what a CC means, how
// Clock affects playback) - it only enumerates ports, opens/closes them,
// moves raw bytes, and reports what it can do; the router above it owns
// every routing decision.
struct MidiBackend {
  void* userdata;
  int (*inputPortCount)(void* userdata);
  int (*inputPortName)(void* userdata, int index, char* buffer, int bufferSize);
  int (*outputPortCount)(void* userdata);
  int (*outputPortName)(void* userdata, int index, char* buffer, int bufferSize);
  int (*openInput)(void* userdata, int portIndex);
  void (*closeInput)(void* userdata);
  int (*openOutput)(void* userdata, int portIndex);
  void (*closeOutput)(void* userdata);
  // Returns 1 and fills outEvent if a message was waiting, 0 otherwise.
  int (*pollInput)(void* userdata, MidiEvent* outEvent);
  // Must be non-blocking and safe to call from the audio thread: queues for
  // the backend's own drain mechanism rather than touching the OS/driver
  // directly (see midiRouterEmitNoteOn's doc comment for the contract this
  // guarantees callers).
  void (*scheduleOutput)(void* userdata, const MidiEvent* event, uint64_t dueMicros);
  void (*flushOutputQueue)(void* userdata);
  unsigned int (*droppedCount)(void* userdata);
  uint64_t (*nowMicros)(void* userdata);
  // Starts any platform permission flow needed before ports can be listed.
  // Native backends leave this null; Web MIDI completes it asynchronously.
  void (*requestAccess)(void* userdata);
  // Optional human-readable backend state for the device settings screen.
  // The returned text remains valid until the next call.
  const char* (*statusMessage)(void* userdata);
  int hasInput;
  int hasOutput;
  int hasClockInput;
  int hasClockOutput;
};

// Registers the active backend. Global rather than per-router-state: there
// is one real MIDI port set per process regardless of how many
// MidiRouterState/ChipNomadState instances exist (mirrors midi_io.cpp's own
// process-wide port handles). Platform init calls this once at startup;
// tests call it with a fake backend per test case.
void midiRouterSetBackend(const MidiBackend* backend);

// --- Backend pass-through ---
// Port enumeration/open/close have no musical semantics of their own, so
// the router just forwards to whichever backend is registered. Callers
// (the Settings UI) never depend on a concrete backend header this way -
// registering a different backend is enough to make them work with it,
// keeping a future Android/Web backend a drop-in, not a UI rewrite.
int midiRouterInputPortCount(void);
int midiRouterInputPortName(int index, char* buffer, int bufferSize);
int midiRouterOutputPortCount(void);
int midiRouterOutputPortName(int index, char* buffer, int bufferSize);
int midiRouterOpenInput(int portIndex);
void midiRouterCloseInput(void);
int midiRouterOpenOutput(int portIndex);
void midiRouterCloseOutput(void);
unsigned int midiRouterGetDroppedCount(void);
// Monotonic microsecond clock on the same scale the backend schedules
// output against (see midiRouterEmitNoteOn's dueMicros) - callers that
// compute a due time ahead of when they call it (e.g. one audio callback
// computing several tracker rows' worth of events) read "now" through this
// once, then add each row's offset to it.
uint64_t midiRouterNowMicros(void);
void midiRouterRequestAccess(void);
const char* midiRouterStatusMessage(void);

// Which incoming MIDI channels drive what. Only auto_ is implemented;
// tracks8 is reserved so multitimbral 8-track input can be added later
// without another API break.
enum class MidiInputMode {
  auto_,   // Incoming notes preview the mapped (or fallback) instrument.
  tracks8, // Reserved, not implemented: channels 1-8 -> tracker tracks 1-8.
};

// MIDI clock sync. off: realtime bytes are ignored entirely (internal
// clock). slave: follow an external device's Clock (0xF8) and transport
// (0xFA Start / 0xFB Continue / 0xFC Stop) - the app layer reads the
// measured tempo and transport state via the getters below and drives
// playback from them. master (generating clock for other gear) is still
// reserved for a future PR.
enum class MidiClockMode {
  off,
  master,
  slave,
};

// One instance per ChipNomadState (see chipnomadCreate/Destroy in
// chipnomad_lib.h), so tests that create multiple independent engine
// states don't share MIDI-Out bookkeeping through a hidden global.
struct MidiRouterState;

MidiRouterState* midiRouterCreate(void);
void midiRouterDestroy(MidiRouterState* router);

// --- Out path (audio thread) ---
// Sends a Note On at dueMicros and tracks it as active on [trackIdx][slot],
// so a later midiRouterEmitNoteOff or midiRouterPanic can target the right
// note even if the instrument/channel changes in between (e.g. after a
// pitch slide, or an instrument/channel reassignment before release).
// Non-blocking and audio-thread-safe: this function does no OS/driver call
// itself, only bookkeeping plus backend->scheduleOutput, which the backend
// contract requires to be equally non-blocking.
void midiRouterEmitNoteOn(MidiRouterState* router, int trackIdx, int slot, uint8_t channel, uint8_t note, uint8_t velocity, uint64_t dueMicros);
// No-op if [trackIdx][slot] isn't currently active.
void midiRouterEmitNoteOff(MidiRouterState* router, int trackIdx, int slot, uint64_t dueMicros);
// Program Change / Bank Select, only (re-)sent if they'd actually change
// what the channel is currently set to - see MidiRouterState's cache.
// programOrBank == EMPTY_VALUE_8 (0xFF) skips that part of the message, as
// for InstrumentMidi's own fields.
void midiRouterEmitProgramBank(MidiRouterState* router, uint8_t channel, uint8_t program, uint8_t bankHigh, uint8_t bankLow, uint64_t dueMicros);
void midiRouterEmitCC(MidiRouterState* router, uint8_t channel, uint8_t ccNumber, uint8_t value, uint64_t dueMicros);

// Sends Note Off for every note midiRouterEmitNoteOn left active, then a
// blanket CC123 (All Notes Off) / CC120 (All Sound Off) on every channel as
// a final fallback, flushes anything still queued but not yet sent first,
// and clears the Program/Bank cache so the next note re-sends it (covers a
// port reconnect no longer matching what a device remembers).
void midiRouterPanic(MidiRouterState* router);

// --- In path (UI tick, not audio thread) ---
struct MidiPreviewIntent {
  uint8_t note;
  int instrument;
  int stop; // 1 = stop preview (note/instrument unused), 0 = play note on instrument
};

struct MidiCCIntent {
  uint8_t channel;
  uint8_t cc;
  uint8_t value;
};

// Sets the per-channel instrument mapping Auto mode input uses (-1 =
// channel not assigned, falls back to midiRouterTick's fallbackInstrument).
// Call once at startup and whenever it changes.
void midiRouterSetChannelInstrumentMap(MidiRouterState* router, const int8_t channelInstrument[16]);

// Clears the legato held-note stack. Call on input device close/change and
// app shutdown, so a note held across the change can't leave a phantom
// entry once a (possibly different) device resumes.
void midiRouterResetHeldNotes(MidiRouterState* router);

// Switches clock following on/off live (app startup, Project screen).
// Resets tempo measurement and transport state but NOT the change serials,
// so a caller comparing serials never re-applies a stale event after the
// switch.
void midiRouterSetClockMode(MidiRouterState* router, MidiClockMode mode);

// Slave-mode clock state, polled by the app layer after midiRouterTick
// (same serial pattern as midiRouterGetCCValue: serials let callers apply
// each change exactly once). Transport: returns 1 once any transport
// message has been seen in slave mode, with *running the latest state
// (1 = Start/Continue, 0 = Stop) and *serial its change counter.
int midiRouterGetClockTransport(const MidiRouterState* router, int* running, uint32_t* serial);
// Tempo: returns 1 once a tempo has been measured in slave mode, with *bpm
// the smoothed tempo (24 clocks/beat) and *serial bumped whenever the
// quantized (whole BPM) value changes.
int midiRouterGetClockBpm(const MidiRouterState* router, float* bpm, uint32_t* serial);
// Monotonic MIDI-clock pulse count. Start/Continue reset the musical phase to
// zero; callers can compare it with their own 24-ticks-per-beat phase.
uint64_t midiRouterGetClockPulseCount(const MidiRouterState* router);

// Polls the backend and applies the active input mode (only auto_ is
// implemented: last-note-priority legato, channel-mapped or
// fallbackInstrument preview). Returns the number of preview intents
// written to outIntents (capped at maxIntents); the caller applies each one
// via its own playback API (e.g. chipnomadQueuePlaybackPreviewNote/
// StopPreview) - the router never calls into chipnomad_lib.h itself,
// keeping it decoupled from the engine's playback API so a future 8-track
// mode only needs to also decide a track index, not reach into engine
// internals to do it.
int midiRouterTick(MidiRouterState* router, int fallbackInstrument, MidiPreviewIntent* outIntents, int maxIntents);
// Returns whether this exact CC has been received, and optionally its value
// and the serial of that receipt.  A zero value is meaningful, so callers
// must not confuse an unseen CC with a CC explicitly sent at zero.
int midiRouterGetCCValue(const MidiRouterState* router, uint8_t channel, uint8_t cc,
                         uint8_t* value, uint32_t* serial);
int midiRouterGetLastCC(const MidiRouterState* router, MidiCCIntent* out, uint32_t* serial);

#endif // __CHIPNOMAD_LIB__MIDI_ROUTER_H__
