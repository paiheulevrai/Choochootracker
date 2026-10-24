#include "midi_router.h"
#include <string.h>

#define MIDI_ROUTER_HELD_NOTES_MAX (16 * 128)

// See midi_router.h for the overall contract. All state here is per
// ChipNomadState (via midiRouterCreate/Destroy), except the backend pointer
// below, which is process-global on purpose (see midiRouterSetBackend).
struct MidiRouterState {
  // Out-path (audio thread): a MIDI-out instrument has no voice object of
  // its own (see chipnomad_lib.cpp's applyVoiceEvents), so this is the
  // minimal per-slot state needed to send a matching Note Off later even if
  // the chord's pitch, instrument or channel has since changed.
  uint8_t noteActive[PROJECT_MAX_TRACKS][CHORD_MAX_VOICES];
  uint8_t activeNote[PROJECT_MAX_TRACKS][CHORD_MAX_VOICES];
  uint8_t activeChannel[PROJECT_MAX_TRACKS][CHORD_MAX_VOICES];
  // Per MIDI channel (not per track: two tracks can share a channel), so
  // Program Change/Bank Select are only (re-)sent when they'd actually
  // change what the receiving device is set to.
  uint8_t channelSetupSent[16];
  uint8_t channelProgram[16];
  uint8_t channelBankHigh[16];
  uint8_t channelBankLow[16];

  // In-path (UI tick): Auto mode's channel->instrument mapping and
  // last-note-priority legato held-note stack. -1 = channel not assigned.
  int8_t channelInstrument[16];
  uint8_t heldNotes[MIDI_ROUTER_HELD_NOTES_MAX];
  uint8_t heldChannels[MIDI_ROUTER_HELD_NOTES_MAX];
  int heldInstrument[MIDI_ROUTER_HELD_NOTES_MAX];
  int heldCount;
  uint8_t ccValues[16][128];
  uint32_t ccSerials[16][128];
  MidiCCIntent lastCC{};
  uint32_t ccSerial;

  // Reserved for future PRs - not branched on anywhere yet.
  MidiInputMode inputMode;
  MidiClockMode clockMode;

  // Slave-mode clock sync (only read when clockMode == slave). Transport:
  // running mirrors the last Start/Continue (1) / Stop (0); every transport
  // message bumps clockTransportSerial so the app layer applies each
  // transition exactly once. Tempo: clock intervals are clamped to
  // [4000, 300000] us (8.3-625 BPM), converted with 24 clocks/beat, EMA
  // smoothed, and quantized to whole BPM before publishing - the serial
  // only moves when the quantized value changes, so a jittery stream
  // doesn't churn the app layer (and the project-refresh path) every tick.
  int clockRunning;
  uint32_t clockTransportSerial;
  uint64_t lastClockArrival;
  int haveLastClockArrival;
  float clockBpmEma;
  int haveClockBpm;
  float clockBpm;
  uint32_t clockBpmSerial;
  uint64_t clockPulseCount;
};

static const MidiBackend* g_backend = nullptr;

void midiRouterSetBackend(const MidiBackend* backend) {
  g_backend = backend;
}

int midiRouterInputPortCount(void) {
  return (g_backend && g_backend->inputPortCount) ? g_backend->inputPortCount(g_backend->userdata) : 0;
}
int midiRouterInputPortName(int index, char* buffer, int bufferSize) {
  return (g_backend && g_backend->inputPortName) ? g_backend->inputPortName(g_backend->userdata, index, buffer, bufferSize) : -1;
}
int midiRouterOutputPortCount(void) {
  return (g_backend && g_backend->outputPortCount) ? g_backend->outputPortCount(g_backend->userdata) : 0;
}
int midiRouterOutputPortName(int index, char* buffer, int bufferSize) {
  return (g_backend && g_backend->outputPortName) ? g_backend->outputPortName(g_backend->userdata, index, buffer, bufferSize) : -1;
}
int midiRouterOpenInput(int portIndex) {
  return (g_backend && g_backend->openInput) ? g_backend->openInput(g_backend->userdata, portIndex) : -1;
}
void midiRouterCloseInput(void) {
  if (g_backend && g_backend->closeInput) g_backend->closeInput(g_backend->userdata);
}
int midiRouterOpenOutput(int portIndex) {
  return (g_backend && g_backend->openOutput) ? g_backend->openOutput(g_backend->userdata, portIndex) : -1;
}
void midiRouterCloseOutput(void) {
  if (g_backend && g_backend->closeOutput) g_backend->closeOutput(g_backend->userdata);
}
unsigned int midiRouterGetDroppedCount(void) {
  return (g_backend && g_backend->droppedCount) ? g_backend->droppedCount(g_backend->userdata) : 0;
}
uint64_t midiRouterNowMicros(void) {
  return (g_backend && g_backend->nowMicros) ? g_backend->nowMicros(g_backend->userdata) : 0;
}
void midiRouterRequestAccess(void) {
  if (g_backend && g_backend->requestAccess) g_backend->requestAccess(g_backend->userdata);
}
const char* midiRouterStatusMessage(void) {
  return (g_backend && g_backend->statusMessage) ? g_backend->statusMessage(g_backend->userdata) : nullptr;
}

MidiRouterState* midiRouterCreate(void) {
  MidiRouterState* router = new MidiRouterState();
  memset(router, 0, sizeof(MidiRouterState));
  for (int i = 0; i < 16; i++) router->channelInstrument[i] = -1;
  router->inputMode = MidiInputMode::auto_;
  router->clockMode = MidiClockMode::off;
  return router;
}

void midiRouterDestroy(MidiRouterState* router) {
  delete router;
}

static void sendEvent(uint8_t type, uint8_t channel, uint8_t data1, uint8_t data2, uint64_t dueMicros) {
  MidiEvent event = {dueMicros, type, channel, data1, data2};
  g_backend->scheduleOutput(g_backend->userdata, &event, dueMicros);
}

void midiRouterEmitNoteOn(MidiRouterState* router, int trackIdx, int slot, uint8_t channel, uint8_t note, uint8_t velocity, uint64_t dueMicros) {
  if (!router || !g_backend) return;
  if (trackIdx < 0 || trackIdx >= PROJECT_MAX_TRACKS || slot < 0 || slot >= CHORD_MAX_VOICES) return;
  // A slot retriggering without an intervening release (e.g. a chord voice
  // reused for a new note) must release the old note first, or it would be
  // silently overwritten below and left stuck on the external device.
  if (router->noteActive[trackIdx][slot]) {
    sendEvent(0x80, router->activeChannel[trackIdx][slot], router->activeNote[trackIdx][slot], 0, dueMicros);
  }
  sendEvent(0x90, channel, note, velocity, dueMicros);
  router->activeNote[trackIdx][slot] = note;
  router->activeChannel[trackIdx][slot] = channel;
  router->noteActive[trackIdx][slot] = 1;
}

void midiRouterEmitNoteOff(MidiRouterState* router, int trackIdx, int slot, uint64_t dueMicros) {
  if (!router || !g_backend) return;
  if (trackIdx < 0 || trackIdx >= PROJECT_MAX_TRACKS || slot < 0 || slot >= CHORD_MAX_VOICES) return;
  if (!router->noteActive[trackIdx][slot]) return;
  sendEvent(0x80, router->activeChannel[trackIdx][slot], router->activeNote[trackIdx][slot], 0, dueMicros);
  router->noteActive[trackIdx][slot] = 0;
}

void midiRouterEmitProgramBank(MidiRouterState* router, uint8_t channel, uint8_t program, uint8_t bankHigh, uint8_t bankLow, uint64_t dueMicros) {
  if (!router || !g_backend) return;
  channel &= 0x0f;
  uint8_t* sent = &router->channelSetupSent[channel];
  uint8_t* lastProgram = &router->channelProgram[channel];
  uint8_t* lastBankHigh = &router->channelBankHigh[channel];
  uint8_t* lastBankLow = &router->channelBankLow[channel];
  if (*sent && *lastProgram == program && *lastBankHigh == bankHigh && *lastBankLow == bankLow) return;
  if (bankHigh != EMPTY_VALUE_8) sendEvent(0xB0, channel, 0, bankHigh, dueMicros);
  if (bankLow != EMPTY_VALUE_8) sendEvent(0xB0, channel, 32, bankLow, dueMicros);
  if (program != EMPTY_VALUE_8) sendEvent(0xC0, channel, program, 0, dueMicros);
  *sent = 1;
  *lastProgram = program;
  *lastBankHigh = bankHigh;
  *lastBankLow = bankLow;
}

void midiRouterEmitCC(MidiRouterState* router, uint8_t channel, uint8_t ccNumber, uint8_t value, uint64_t dueMicros) {
  if (!router || !g_backend) return;
  sendEvent(0xB0, channel & 0x0f, ccNumber & 0x7f, value, dueMicros);
}

void midiRouterPanic(MidiRouterState* router) {
  if (!router || !g_backend) return;
  // Drop anything still queued but not yet sent first, so a stale note or
  // CC computed before this transition can't fire late after it - then
  // this sweep's own Note Offs/panic CCs below are scheduled fresh on top.
  if (g_backend->flushOutputQueue) g_backend->flushOutputQueue(g_backend->userdata);
  uint64_t now = g_backend->nowMicros ? g_backend->nowMicros(g_backend->userdata) : 0;
  for (int trackIdx = 0; trackIdx < PROJECT_MAX_TRACKS; ++trackIdx) {
    for (int slot = 0; slot < CHORD_MAX_VOICES; ++slot) {
      if (!router->noteActive[trackIdx][slot]) continue;
      sendEvent(0x80, router->activeChannel[trackIdx][slot], router->activeNote[trackIdx][slot], 0, now);
      router->noteActive[trackIdx][slot] = 0;
    }
  }
  // Final fallback, unconditionally on every channel: cheap insurance
  // against any note this sweep doesn't know about.
  for (uint8_t channel = 0; channel < 16; ++channel) {
    sendEvent(0xB0, channel, 123, 0, now); // All Notes Off
    sendEvent(0xB0, channel, 120, 0, now); // All Sound Off
  }
  memset(router->channelSetupSent, 0, sizeof(router->channelSetupSent));
}

void midiRouterSetChannelInstrumentMap(MidiRouterState* router, const int8_t channelInstrument[16]) {
  if (!router) return;
  memcpy(router->channelInstrument, channelInstrument, sizeof(router->channelInstrument));
}

void midiRouterResetHeldNotes(MidiRouterState* router) {
  if (!router) return;
  router->heldCount = 0;
}

void midiRouterSetClockMode(MidiRouterState* router, MidiClockMode mode) {
  if (!router) return;
  router->clockMode = mode;
  // Fresh measurement state on every switch; serials are deliberately NOT
  // reset (see header) so callers comparing serials don't re-apply stale
  // transport/tempo events from before the switch.
  router->clockRunning = 0;
  router->lastClockArrival = 0;
  router->haveLastClockArrival = 0;
  router->clockBpmEma = 0;
  router->haveClockBpm = 0;
  router->clockBpm = 0;
  router->clockPulseCount = 0;
}

int midiRouterGetClockTransport(const MidiRouterState* router, int* running, uint32_t* serial) {
  if (!router) return 0;
  if (running) *running = router->clockRunning;
  if (serial) *serial = router->clockTransportSerial;
  return router->clockTransportSerial != 0;
}

int midiRouterGetClockBpm(const MidiRouterState* router, float* bpm, uint32_t* serial) {
  if (!router) return 0;
  if (bpm) *bpm = router->clockBpm;
  if (serial) *serial = router->clockBpmSerial;
  return router->haveClockBpm;
}

uint64_t midiRouterGetClockPulseCount(const MidiRouterState* router) {
  return router ? router->clockPulseCount : 0;
}

// One incoming Clock byte (0xF8) in slave mode: measure the interval since
// the previous clock, smooth it, and publish the result once per beat.
static void handleClockTick(MidiRouterState* router, uint64_t arrival) {
  router->clockPulseCount++;
  if (router->haveLastClockArrival) {
    uint64_t interval = arrival - router->lastClockArrival;
    // Clamp to a sane musical range (~8.3-625 BPM): the first clock after a
    // transport start, a paused stream, or a burst of catch-up clocks would
    // otherwise produce a wild interval and a bogus tempo.
    if (interval >= 4000 && interval <= 300000) {
      float bpm = 60000000.0f / ((float)interval * 24.0f);
      router->clockBpmEma = router->haveClockBpm
        ? router->clockBpmEma + (bpm - router->clockBpmEma) * 0.2f
        : bpm;
      // Start following as soon as the first interval is known. After that,
      // a USB MIDI pulse is far too noisy a rate for retiming a sequencer and
      // its tempo-synchronised effects directly, so publish once per beat.
      if (!router->haveClockBpm || (router->clockPulseCount % 24) == 0) {
        router->clockBpm = router->clockBpmEma;
        router->clockBpmSerial++;
      }
      router->haveClockBpm = 1;
    }
  }
  router->lastClockArrival = arrival;
  router->haveLastClockArrival = 1;
}

int midiRouterTick(MidiRouterState* router, int fallbackInstrument, MidiPreviewIntent* outIntents, int maxIntents) {
  if (!router || !g_backend || !g_backend->pollInput || !outIntents || maxIntents <= 0) return 0;
  int count = 0;
  MidiEvent event;
  // Always drain and apply the whole poll queue. Keep its final intent when
  // the caller's buffer fills: dropping a Note Off here leaves a stuck note.
  while (g_backend->pollInput(g_backend->userdata, &event)) {
    uint8_t messageType = event.type & 0xf0;
    uint8_t channel = event.channel & 0x0f;
    // Realtime bytes (full status >= 0xF8, preserved by the backends) are
    // handled before any channel-voice branch: in slave mode they drive
    // transport/tempo, in every mode they must not fall into the channel
    // branches below (0xF8 & 0xf0 == 0xF0 never matches 0xb0/0x90/0x80,
    // but being explicit keeps the routing contract clear).
    if (event.type >= 0xf8) {
      if (router->clockMode == MidiClockMode::slave) {
        switch (event.type) {
          case 0xfa: // Start
          case 0xfb: // Continue
            router->clockRunning = 1;
            router->clockTransportSerial++;
            router->haveLastClockArrival = 0;
            router->clockPulseCount = 0;
            break;
          case 0xfc: // Stop
            router->clockRunning = 0;
            router->clockTransportSerial++;
            router->haveLastClockArrival = 0;
            break;
          case 0xf8: // Clock
            handleClockTick(router,
              event.timestampMicros ? event.timestampMicros : g_backend->nowMicros(g_backend->userdata));
            break;
          default: // 0xFE active sensing, 0xFF reset: ignore
            break;
        }
      }
      continue;
    }
    if (messageType == 0xb0) {
      router->ccValues[channel][event.data1 & 0x7f] = event.data2 & 0x7f;
      router->ccSerials[channel][event.data1 & 0x7f] = router->ccSerial + 1;
      router->lastCC = {channel, (uint8_t)(event.data1 & 0x7f), (uint8_t)(event.data2 & 0x7f)};
      router->ccSerial++;
      continue;
    }
    int8_t mapped = router->channelInstrument[channel];
    int instrument = mapped >= 0 ? mapped : fallbackInstrument;

    // Last-note priority (legato): releasing a note that isn't the one
    // currently sounding must not cut the preview - only resume the next
    // most recently held note (or stop if none remain) when the *current*
    // note is released. A velocity-0 Note On is a Note Off by MIDI
    // convention, handled by the messageType==0x90&&data2==0 leg below.
    if (messageType == 0x90 && event.data2 > 0) {
      int note = (int)event.data1 - 12;
      if (note < 0 || note >= 128) continue;
      if (router->heldCount < MIDI_ROUTER_HELD_NOTES_MAX) {
        router->heldNotes[router->heldCount] = (uint8_t)note;
        router->heldChannels[router->heldCount] = channel;
        router->heldInstrument[router->heldCount] = instrument;
        router->heldCount++;
      }
      int outIndex = count < maxIntents ? count++ : maxIntents - 1;
      outIntents[outIndex].note = (uint8_t)note;
      outIntents[outIndex].instrument = instrument;
      outIntents[outIndex].stop = 0;
    } else if (messageType == 0x80 || (messageType == 0x90 && event.data2 == 0)) {
      int note = (int)event.data1 - 12;
      int foundIdx = -1;
      for (int i = router->heldCount - 1; i >= 0; i--) {
        if (router->heldNotes[i] == (uint8_t)note && router->heldChannels[i] == channel) { foundIdx = i; break; }
      }
      if (foundIdx < 0) continue;
      int wasCurrent = foundIdx == router->heldCount - 1;
      for (int i = foundIdx; i < router->heldCount - 1; i++) {
        router->heldNotes[i] = router->heldNotes[i + 1];
        router->heldChannels[i] = router->heldChannels[i + 1];
        router->heldInstrument[i] = router->heldInstrument[i + 1];
      }
      router->heldCount--;
      if (!wasCurrent) continue;
      int outIndex = count < maxIntents ? count++ : maxIntents - 1;
      if (router->heldCount > 0) {
        outIntents[outIndex].note = router->heldNotes[router->heldCount - 1];
        outIntents[outIndex].instrument = router->heldInstrument[router->heldCount - 1];
        outIntents[outIndex].stop = 0;
      } else {
        outIntents[outIndex].note = 0;
        outIntents[outIndex].instrument = 0;
        outIntents[outIndex].stop = 1;
      }
    }
  }
  return count;
}

int midiRouterGetCCValue(const MidiRouterState* router, uint8_t channel, uint8_t cc,
                         uint8_t* value, uint32_t* serial) {
  if (!router) return 0;
  channel &= 0x0f; cc &= 0x7f;
  if (value) *value = router->ccValues[channel][cc];
  if (serial) *serial = router->ccSerials[channel][cc];
  return router->ccSerials[channel][cc] != 0;
}

int midiRouterGetLastCC(const MidiRouterState* router, MidiCCIntent* out, uint32_t* serial) {
  if (!router || !out || !serial) return 0;
  *out = router->lastCC;
  *serial = router->ccSerial;
  return router->ccSerial != 0;
}
