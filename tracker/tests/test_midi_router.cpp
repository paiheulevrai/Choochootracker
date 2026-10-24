#include "doctest.h"
#include "midi/midi_router.h"

#include <vector>
#include <cstdint>
#include <cstring>

// Router-level tests through a hand-built fake MidiBackend: this is the
// "testable with a fake backend" requirement from the upstream review, and
// closes a real gap - none of this logic (legato, panic, Program/Bank
// caching) could be exercised by the automated suite before, since the real
// desktop backend is only live under DESKTOP_BUILD, which the test build
// never defines.

namespace {

struct SentMidiEvent { uint8_t type, channel, data1, data2; };
std::vector<SentMidiEvent> g_sent;
std::vector<MidiEvent> g_incoming;
size_t g_incomingIndex;

void resetFake() {
  g_sent.clear();
  g_incoming.clear();
  g_incomingIndex = 0;
}

void pushIncoming(uint8_t type, uint8_t channel, uint8_t data1, uint8_t data2) {
  g_incoming.push_back({0, type, channel, data1, data2});
}

int fakePollInput(void*, MidiEvent* outEvent) {
  if (g_incomingIndex >= g_incoming.size()) return 0;
  *outEvent = g_incoming[g_incomingIndex++];
  return 1;
}

void fakeScheduleOutput(void*, const MidiEvent* event, uint64_t) {
  g_sent.push_back({event->type, event->channel, event->data1, event->data2});
}

void fakeFlushOutputQueue(void*) {}
unsigned int fakeDroppedCount(void*) { return 42; } // fixed sentinel to verify the passthrough
uint64_t fakeNowMicros(void*) { return 12345; }
int g_accessRequests;
void fakeRequestAccess(void*) { g_accessRequests++; }
const char* fakeStatusMessage(void*) { return "Fake MIDI ready"; }

const MidiBackend kFakeBackend = {
  nullptr, // userdata
  nullptr, nullptr, nullptr, nullptr, // port enumeration - unused here
  nullptr, nullptr, nullptr, nullptr, // open/close - unused here
  fakePollInput,
  fakeScheduleOutput,
  fakeFlushOutputQueue,
  fakeDroppedCount,
  fakeNowMicros,
  fakeRequestAccess,
  fakeStatusMessage,
  1, 1, 0, 0,
};

// -1 for every channel: Auto mode falls back to whatever instrument the
// caller passes to midiRouterTick.
const int8_t kNoChannelMap[16] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

}

TEST_SUITE("MIDI router") {

TEST_CASE("Backend access request and status are forwarded") {
  g_accessRequests = 0;
  midiRouterSetBackend(&kFakeBackend);
  midiRouterRequestAccess();
  CHECK(g_accessRequests == 1);
  CHECK(strcmp(midiRouterStatusMessage(), "Fake MIDI ready") == 0);
}

TEST_CASE("Incoming CC values are retained independently of note preview") {
  resetFake();
  midiRouterSetBackend(&kFakeBackend);
  MidiRouterState* router = midiRouterCreate();
  MidiPreviewIntent intents[1];
  pushIncoming(0xB0, 3, 74, 96);
  CHECK(midiRouterTick(router, 0, intents, 1) == 0);
  uint8_t value = 0; uint32_t valueSerial = 0;
  CHECK(midiRouterGetCCValue(router, 3, 74, &value, &valueSerial));
  CHECK(value == 96);
  CHECK(valueSerial == 1);
  CHECK_FALSE(midiRouterGetCCValue(router, 3, 75, &value, &valueSerial));
  MidiCCIntent last; uint32_t serial = 0;
  REQUIRE(midiRouterGetLastCC(router, &last, &serial));
  CHECK(last.channel == 3);
  CHECK(last.cc == 74);
  CHECK(last.value == 96);
  CHECK(serial == 1);
  midiRouterDestroy(router);
}

TEST_CASE("Auto mode legato: releasing the current note resumes the previous one") {
  resetFake();
  midiRouterSetBackend(&kFakeBackend);
  MidiRouterState* router = midiRouterCreate();
  midiRouterSetChannelInstrumentMap(router, kNoChannelMap);
  MidiPreviewIntent intents[16];

  pushIncoming(0x90, 0, 60, 100); // Note On C
  REQUIRE(midiRouterTick(router, 5, intents, 16) == 1);
  CHECK_FALSE(intents[0].stop);
  CHECK(intents[0].note == 48); // data1 - 12
  CHECK(intents[0].instrument == 5); // unmapped channel -> fallback

  g_incoming.clear(); g_incomingIndex = 0;
  pushIncoming(0x90, 0, 64, 100); // Note On E, while C is still held
  REQUIRE(midiRouterTick(router, 5, intents, 16) == 1);
  CHECK(intents[0].note == 52);

  g_incoming.clear(); g_incomingIndex = 0;
  pushIncoming(0x80, 0, 64, 0); // Note Off E (the current note)
  REQUIRE(midiRouterTick(router, 5, intents, 16) == 1);
  CHECK_FALSE(intents[0].stop);
  CHECK(intents[0].note == 48); // resumes C

  midiRouterDestroy(router);
}

TEST_CASE("Auto mode legato: releasing a buried (non-current) note does nothing") {
  resetFake();
  midiRouterSetBackend(&kFakeBackend);
  MidiRouterState* router = midiRouterCreate();
  midiRouterSetChannelInstrumentMap(router, kNoChannelMap);
  MidiPreviewIntent intents[16];

  pushIncoming(0x90, 0, 60, 100); // Hold C
  midiRouterTick(router, 5, intents, 16);
  g_incoming.clear(); g_incomingIndex = 0;
  pushIncoming(0x90, 0, 64, 100); // Hold E (now current)
  midiRouterTick(router, 5, intents, 16);

  g_incoming.clear(); g_incomingIndex = 0;
  pushIncoming(0x80, 0, 60, 0); // Release C - buried under E, not current
  CHECK(midiRouterTick(router, 5, intents, 16) == 0);

  g_incoming.clear(); g_incomingIndex = 0;
  pushIncoming(0x80, 0, 64, 0); // Release E - now nothing remains held
  REQUIRE(midiRouterTick(router, 5, intents, 16) == 1);
  CHECK(intents[0].stop);

  midiRouterDestroy(router);
}

TEST_CASE("Auto mode keeps identical notes on separate MIDI channels independent") {
  resetFake();
  midiRouterSetBackend(&kFakeBackend);
  MidiRouterState* router = midiRouterCreate();
  midiRouterSetChannelInstrumentMap(router, kNoChannelMap);
  MidiPreviewIntent intents[16];

  pushIncoming(0x90, 0, 60, 100);
  pushIncoming(0x90, 1, 60, 100);
  REQUIRE(midiRouterTick(router, 5, intents, 16) == 2);

  g_incoming.clear(); g_incomingIndex = 0;
  pushIncoming(0x80, 0, 60, 0); // Must not release channel 1's note.
  CHECK(midiRouterTick(router, 5, intents, 16) == 0);

  g_incoming.clear(); g_incomingIndex = 0;
  pushIncoming(0x80, 1, 60, 0);
  REQUIRE(midiRouterTick(router, 5, intents, 16) == 1);
  CHECK(intents[0].stop);

  midiRouterDestroy(router);
}

TEST_CASE("Auto mode applies Note Offs after its preview buffer is full") {
  resetFake();
  midiRouterSetBackend(&kFakeBackend);
  MidiRouterState* router = midiRouterCreate();
  midiRouterSetChannelInstrumentMap(router, kNoChannelMap);
  MidiPreviewIntent intent;

  pushIncoming(0x90, 0, 60, 100);
  pushIncoming(0x90, 0, 64, 100);
  pushIncoming(0x80, 0, 64, 0); // This used to be discarded at capacity.
  REQUIRE(midiRouterTick(router, 5, &intent, 1) == 1);
  CHECK_FALSE(intent.stop);
  CHECK(intent.note == 48);

  g_incoming.clear(); g_incomingIndex = 0;
  pushIncoming(0x80, 0, 60, 0);
  REQUIRE(midiRouterTick(router, 5, &intent, 1) == 1);
  CHECK(intent.stop);

  midiRouterDestroy(router);
}

TEST_CASE("Channel mapping overrides the fallback instrument, per channel") {
  resetFake();
  midiRouterSetBackend(&kFakeBackend);
  MidiRouterState* router = midiRouterCreate();
  int8_t channelMap[16];
  for (int i = 0; i < 16; i++) channelMap[i] = -1;
  channelMap[2] = 7;
  midiRouterSetChannelInstrumentMap(router, channelMap);
  MidiPreviewIntent intents[16];

  pushIncoming(0x90, 2, 60, 100); // Mapped channel
  REQUIRE(midiRouterTick(router, 5, intents, 16) == 1);
  CHECK(intents[0].instrument == 7);

  g_incoming.clear(); g_incomingIndex = 0;
  pushIncoming(0x90, 3, 60, 100); // Unmapped channel
  REQUIRE(midiRouterTick(router, 5, intents, 16) == 1);
  CHECK(intents[0].instrument == 5);

  midiRouterDestroy(router);
}

TEST_CASE("A Note On while a slot is still active releases the old note first") {
  resetFake();
  midiRouterSetBackend(&kFakeBackend);
  MidiRouterState* router = midiRouterCreate();

  midiRouterEmitNoteOn(router, 0, 0, 2, 60, 100, 0);
  g_sent.clear();
  midiRouterEmitNoteOn(router, 0, 0, 2, 64, 100, 0); // same slot, no explicit Off first

  REQUIRE(g_sent.size() == 2);
  CHECK(g_sent[0].type == 0x80);
  CHECK(g_sent[0].data1 == 60);
  CHECK(g_sent[1].type == 0x90);
  CHECK(g_sent[1].data1 == 64);

  midiRouterDestroy(router);
}

TEST_CASE("Panic sends Note Off for active notes and resets the Program/Bank cache") {
  resetFake();
  midiRouterSetBackend(&kFakeBackend);
  MidiRouterState* router = midiRouterCreate();

  midiRouterEmitProgramBank(router, 1, 10, EMPTY_VALUE_8, EMPTY_VALUE_8, 0);
  REQUIRE(g_sent.size() == 1);
  CHECK(g_sent[0].type == 0xC0);
  CHECK(g_sent[0].data1 == 10);

  g_sent.clear();
  midiRouterEmitProgramBank(router, 1, 10, EMPTY_VALUE_8, EMPTY_VALUE_8, 0); // unchanged - cached, no resend
  CHECK(g_sent.empty());

  midiRouterEmitNoteOn(router, 0, 0, 1, 60, 100, 0);
  g_sent.clear();
  midiRouterPanic(router);

  int sawNoteOff = 0, ccCount = 0;
  for (const SentMidiEvent& e : g_sent) {
    if (e.type == 0x80 && e.channel == 1 && e.data1 == 60) sawNoteOff = 1;
    if (e.type == 0xB0) ccCount++;
  }
  CHECK(sawNoteOff);
  CHECK(ccCount == 32); // CC123 + CC120 on each of 16 channels

  g_sent.clear();
  midiRouterEmitProgramBank(router, 1, 10, EMPTY_VALUE_8, EMPTY_VALUE_8, 0); // resends: panic cleared the cache
  REQUIRE(g_sent.size() == 1);
  CHECK(g_sent[0].type == 0xC0);

  midiRouterDestroy(router);
}

TEST_CASE("Two independent router instances don't share active-note state") {
  resetFake();
  midiRouterSetBackend(&kFakeBackend);
  MidiRouterState* r1 = midiRouterCreate();
  MidiRouterState* r2 = midiRouterCreate();

  midiRouterEmitNoteOn(r1, 0, 0, 1, 60, 100, 0);
  g_sent.clear();
  midiRouterPanic(r2); // must not know anything about r1's active note

  int sawR1NoteOff = 0;
  for (const SentMidiEvent& e : g_sent) if (e.type == 0x80 && e.data1 == 60) sawR1NoteOff = 1;
  CHECK_FALSE(sawR1NoteOff);

  midiRouterDestroy(r1);
  midiRouterDestroy(r2);
}

TEST_CASE("Dropped-message count passes through to the registered backend") {
  midiRouterSetBackend(&kFakeBackend);
  CHECK(midiRouterGetDroppedCount() == 42);
}

}
