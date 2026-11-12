#include "midi_io.h"
#include <chrono>

uint64_t midiIoNowMicros(void) {
  return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();
}

#if defined(DESKTOP_BUILD) || defined(PORTMASTER_BUILD)

#include "external/rtmidi/RtMidi.h"
#ifdef PORTMASTER_BUILD
#include <alsa/asoundlib.h>
#endif
#include <stdio.h>
#include <string.h>
#include <vector>
#include <atomic>
#include <thread>

static RtMidiIn* g_midiIn = NULL;
static RtMidiOut* g_midiOut = NULL;
#ifdef PORTMASTER_BUILD
static RtMidiIn* g_launchpadIn = NULL;
static RtMidiOut* g_launchpadOut = NULL;
static uint64_t g_launchpadRetryAt = 0;
static uint64_t g_launchpadLastPresenceCheck = 0;
static std::string g_launchpadInputIdentity;
static std::string g_launchpadOutputIdentity;

static bool launchpadPortIdentity(const std::string& expectedName, std::string* identity) {
  snd_seq_t* seq = NULL;
  if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) return false;
  snd_seq_client_info_t* clientInfo;
  snd_seq_port_info_t* portInfo;
  snd_seq_client_info_alloca(&clientInfo);
  snd_seq_port_info_alloca(&portInfo);
  bool found = false;
  snd_seq_client_info_set_client(clientInfo, -1);
  while (snd_seq_query_next_client(seq, clientInfo) >= 0 && !found) {
    const int client = snd_seq_client_info_get_client(clientInfo);
    const char* clientName = snd_seq_client_info_get_name(clientInfo);
    snd_seq_port_info_set_client(portInfo, client);
    snd_seq_port_info_set_port(portInfo, -1);
    while (snd_seq_query_next_port(seq, portInfo) >= 0) {
      if (strcmp(snd_seq_port_info_get_name(portInfo), expectedName.c_str()) == 0) {
        char key[128];
        snprintf(key, sizeof(key), "%s:%d", clientName, snd_seq_port_info_get_port(portInfo));
        *identity = key;
        found = true;
        break;
      }
    }
  }
  snd_seq_close(seq);
  return found;
}

static int findRtMidiPort(bool input, const std::string& identity) {
  try {
    if (input) {
      RtMidiIn probe;
      for (unsigned int i = 0; i < probe.getPortCount(); ++i)
        if (probe.getPortName(i) == identity) return (int)i;
    } else {
      RtMidiOut probe;
      for (unsigned int i = 0; i < probe.getPortCount(); ++i)
        if (probe.getPortName(i) == identity) return (int)i;
    }
  } catch (...) {}
  return -1;
}

static int findLaunchpadPort(bool input, std::string* identity) {
  if (!launchpadPortIdentity("Launchpad Mini MK3 MIDI 2", identity)) return -1;
  return findRtMidiPort(input, *identity);
}

void midiIoLaunchpadStop(void) {
  if (g_launchpadOut) {
    try {
      std::vector<unsigned char> liveMode{0xf0, 0x00, 0x20, 0x29, 0x02, 0x0d, 0x0e, 0x00, 0xf7};
      g_launchpadOut->sendMessage(&liveMode);
    } catch (...) {}
  }
  if (g_launchpadIn) {
    try { g_launchpadIn->closePort(); } catch (...) {}
    delete g_launchpadIn;
    g_launchpadIn = NULL;
  }
  if (g_launchpadOut) {
    try { g_launchpadOut->closePort(); } catch (...) {}
    delete g_launchpadOut;
    g_launchpadOut = NULL;
  }
  g_launchpadInputIdentity.clear();
  g_launchpadOutputIdentity.clear();
}

int midiIoLaunchpadConnected(void) {
  return g_launchpadIn && g_launchpadOut;
}

int midiIoLaunchpadStart(void) {
  if (midiIoLaunchpadConnected()) return 1;
  const uint64_t now = midiIoNowMicros();
  if (now < g_launchpadRetryAt) return 0;
  g_launchpadRetryAt = now + 1000000;
  std::string inputIdentity, outputIdentity;
  const int inputPort = findLaunchpadPort(true, &inputIdentity);
  const int outputPort = findLaunchpadPort(false, &outputIdentity);
  if (inputPort < 0 || outputPort < 0) return 0;
  try {
    g_launchpadIn = new RtMidiIn();
    g_launchpadIn->ignoreTypes(false, false, false);
    g_launchpadIn->openPort((unsigned int)inputPort, "ChooChooTracker Launchpad In");
    g_launchpadOut = new RtMidiOut();
    g_launchpadOut->openPort((unsigned int)outputPort, "ChooChooTracker Launchpad Out");
    g_launchpadInputIdentity = inputIdentity;
    g_launchpadOutputIdentity = outputIdentity;
    std::vector<unsigned char> programmer{0xf0, 0x00, 0x20, 0x29, 0x02, 0x0d, 0x0e, 0x01, 0xf7};
    g_launchpadOut->sendMessage(&programmer);
    g_launchpadLastPresenceCheck = now;
    return 1;
  } catch (...) {
    midiIoLaunchpadStop();
    return 0;
  }
}

int midiIoLaunchpadPoll(uint8_t* status, uint8_t* note, uint8_t* velocity) {
  if (!midiIoLaunchpadConnected()) return 0;
  try {
    const uint64_t now = midiIoNowMicros();
    if (now - g_launchpadLastPresenceCheck > 1000000) {
      std::string inIdentity, outIdentity;
      if (findLaunchpadPort(true, &inIdentity) < 0 || findLaunchpadPort(false, &outIdentity) < 0 ||
          inIdentity != g_launchpadInputIdentity || outIdentity != g_launchpadOutputIdentity) {
        midiIoLaunchpadStop();
        return -1;
      }
      g_launchpadLastPresenceCheck = now;
    }
    std::vector<unsigned char> message;
    if (g_launchpadIn->getMessage(&message) <= 0.0 || message.empty()) return 0;
    if (message.size() >= 3) {
      *status = message[0]; *note = message[1]; *velocity = message[2];
      return 1;
    }
    return 0;
  } catch (...) {
    midiIoLaunchpadStop();
    return -1;
  }
}

void midiIoLaunchpadSetPad(uint8_t note, uint8_t colour, int flashing) {
  if (!g_launchpadOut) return;
  try {
    std::vector<unsigned char> message{(unsigned char)(flashing ? 0x91 : 0x90), note, colour};
    g_launchpadOut->sendMessage(&message);
  } catch (...) { midiIoLaunchpadStop(); }
}
void midiIoLaunchpadSetButton(uint8_t control, uint8_t colour) {
  if (!g_launchpadOut) return;
  try {
    std::vector<unsigned char> message{0xb0, control, colour};
    g_launchpadOut->sendMessage(&message);
  } catch (...) { midiIoLaunchpadStop(); }
}
#else
int midiIoLaunchpadStart(void) { return 0; }
int midiIoLaunchpadConnected(void) { return 0; }
int midiIoLaunchpadPoll(uint8_t*, uint8_t*, uint8_t*) { return 0; }
void midiIoLaunchpadSetPad(uint8_t, uint8_t, int) {}
void midiIoLaunchpadSetButton(uint8_t, uint8_t) {}
void midiIoLaunchpadStop(void) {}
#endif // PORTMASTER_BUILD

// Program Change and Channel Pressure are 2-byte channel voice messages;
// every other one we send (Note On/Off, CC) is 3 bytes. Sending a spurious
// 3rd byte after a Program Change would be read as the start of an
// unrelated running-status data byte by some receivers.
static int channelMessageLength(uint8_t status) {
  if (status >= 0xf8) return 1;
  uint8_t type = status & 0xf0;
  return (type == 0xC0 || type == 0xD0) ? 2 : 3;
}

// See midi_io.h's midiIoScheduleMessage for why this queue+thread exist.
// Single producer (the audio thread), single consumer (drainThreadLoop), so
// this plain head/tail ring buffer needs no lock - same pattern as
// chipnomad_lib.cpp's own AudioCommandQueue.
struct ScheduledMidiMessage { uint8_t status, data1, data2; uint64_t dueMicros; };
static constexpr unsigned int kMidiOutQueueCapacity = 512;
static ScheduledMidiMessage g_midiOutQueue[kMidiOutQueueCapacity];
static std::atomic<unsigned int> g_midiOutHead{0};
static std::atomic<unsigned int> g_midiOutTail{0};
static std::atomic<bool> g_midiOutThreadRunning{false};
static std::thread* g_midiOutThread = NULL;
static std::atomic<unsigned int> g_midiOutDropped{0};

void midiIoFlushOutputQueue(void) {
  g_midiOutTail.store(g_midiOutHead.load(std::memory_order_acquire), std::memory_order_release);
}

unsigned int midiIoGetDroppedCount(void) {
  return g_midiOutDropped.load(std::memory_order_relaxed);
}

void midiIoScheduleMessage(uint8_t status, uint8_t data1, uint8_t data2, uint64_t dueMicros) {
  // No output open means no drain thread running to ever consume this
  // queue: without this guard, a project with MIDI instruments but no
  // output device selected would silently fill the queue and start
  // dropping messages instead of just doing nothing.
  if (!g_midiOut) return;
  unsigned int head = g_midiOutHead.load(std::memory_order_relaxed);
  unsigned int next = (head + 1) % kMidiOutQueueCapacity;
  if (next == g_midiOutTail.load(std::memory_order_acquire)) {
    g_midiOutDropped.fetch_add(1, std::memory_order_relaxed); // full: drop rather than block the audio thread
    return;
  }
  g_midiOutQueue[head] = {status, data1, data2, dueMicros};
  g_midiOutHead.store(next, std::memory_order_release);
}

// Polls at ~1ms resolution - far tighter than an audio callback (which can
// represent tens of milliseconds of "musical time" computed all at once)
// without needing sample-accurate OS scheduling support that RtMidi doesn't
// offer.
static void midiOutDrainThreadLoop() {
  while (g_midiOutThreadRunning.load(std::memory_order_relaxed)) {
    unsigned int tail = g_midiOutTail.load(std::memory_order_relaxed);
    uint64_t now = midiIoNowMicros();
    while (tail != g_midiOutHead.load(std::memory_order_acquire) && g_midiOutQueue[tail].dueMicros <= now) {
      ScheduledMidiMessage msg = g_midiOutQueue[tail];
      if (g_midiOut) {
        std::vector<unsigned char> bytes = {msg.status, msg.data1, msg.data2};
        bytes.resize(channelMessageLength(msg.status));
        try { g_midiOut->sendMessage(&bytes); } catch (...) {}
      }
      tail = (tail + 1) % kMidiOutQueueCapacity;
      g_midiOutTail.store(tail, std::memory_order_release);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

int midiIoAvailable(void) { return 1; }

template <typename Device>
static int portCount(Device* probe) {
  try {
    return (int)probe->getPortCount();
  } catch (...) {
    return 0;
  }
}

int midiIoInputPortCount(void) {
  try {
    RtMidiIn probe;
    return portCount(&probe);
  } catch (...) {
    return 0;
  }
}

int midiIoInputPortName(int index, char* buffer, int bufferSize) {
  try {
    RtMidiIn probe;
    std::string name = probe.getPortName((unsigned int)index);
    snprintf(buffer, bufferSize, "%s", name.c_str());
    return 0;
  } catch (...) {
    return -1;
  }
}

int midiIoOutputPortCount(void) {
  try {
    RtMidiOut probe;
    return portCount(&probe);
  } catch (...) {
    return 0;
  }
}

int midiIoOutputPortName(int index, char* buffer, int bufferSize) {
  try {
    RtMidiOut probe;
    std::string name = probe.getPortName((unsigned int)index);
    snprintf(buffer, bufferSize, "%s", name.c_str());
    return 0;
  } catch (...) {
    return -1;
  }
}

// RtMidi's getMessage() reports each message's delta time relative to the
// PREVIOUS dequeued message (driver timestamp on ALSA/CoreMIDI, MM timer on
// WinMM), not an absolute time - and we poll in bursts, so consecutive
// getMessage() calls can be far apart in wall time while the messages they
// return arrived microseconds apart. To recover true arrival times we chain
// the deltas: arrival(n) = arrival(n-1) + delta(n), anchored to the wall
// clock on the first message after open. This matters for MIDI clock tempo
// measurement, where clock bytes arrive 24x per beat and poll bursts would
// otherwise make every interval look like one poll period.
static uint64_t g_lastArrivalMicros = 0;
static int g_haveLastArrival = 0;

int midiIoOpenInput(int portIndex) {
  midiIoCloseInput();
  try {
    g_midiIn = new RtMidiIn();
    g_midiIn->openPort((unsigned int)portIndex, "ChooChooTracker In");
    // MIDI clock sync needs the realtime bytes (0xF8 clock, 0xFA/0xFB/0xFC
    // transport) to reach our poll loop, so nothing is filtered at the
    // driver level. Sysex/active-sensing that still get queued are dropped
    // by midiIoPollInput's size/status filter below.
    // All three flags must be clear because RtMidi's ALSA handler falls
    // through from its TICK case into the SENSING and SYSEX cases: a 0xF8
    // is dropped if ANY of the three ignore flags is set, not just the
    // clock one.
    g_midiIn->ignoreTypes(false, false, false);
    // Timestamps restart with the port: the first message has no reliable
    // delta to chain from.
    g_haveLastArrival = 0;
    g_lastArrivalMicros = 0;
    return 0;
  } catch (...) {
    delete g_midiIn;
    g_midiIn = NULL;
    return -1;
  }
}

void midiIoCloseInput(void) {
  if (!g_midiIn) return;
  g_midiIn->closePort();
  delete g_midiIn;
  g_midiIn = NULL;
}

int midiIoIsInputOpen(void) { return g_midiIn != NULL; }

int midiIoPollInput(uint8_t* outStatus, uint8_t* outData1, uint8_t* outData2,                    uint64_t* outTimestampMicros) {
  if (!g_midiIn) return 0;
  std::vector<unsigned char> message;
  try {
    for (;;) {
      // getMessage() returns the message's delta time in seconds (time
      // since the previous dequeued message); there is no separate
      // timestamp getter in this RtMidi version.
      double delta = g_midiIn->getMessage(&message);
      if (message.empty()) return 0;
      uint8_t status = message[0];
      // Accept: realtime clock/transport bytes (always 1 byte), or plain
      // 2/3-byte channel voice messages. Everything else (sysex, 0xF1/0xF2/
      // 0xF3 system common, 0xFE active sensing, 0xFF reset) is dropped
      // rather than misread as note/CC data.
      int isRealtime = message.size() == 1 &&
        (status == 0xF8 || status == 0xFA || status == 0xFB || status == 0xFC);
      int isChannel = (message.size() == 2 || message.size() == 3) && status < 0xF0;
      if (!isRealtime && !isChannel) continue;

      uint64_t now = midiIoNowMicros();
      uint64_t arrival = (delta > 0.0 && g_haveLastArrival)
        ? g_lastArrivalMicros + (uint64_t)(delta * 1000000.0)
        : now;
      if (arrival > now) arrival = now; // clock skew guard
      g_lastArrivalMicros = arrival;
      g_haveLastArrival = 1;

      *outStatus = status;
      *outData1 = message.size() > 1 ? message[1] : 0;
      *outData2 = message.size() > 2 ? message[2] : 0;
      *outTimestampMicros = arrival;
      return 1;
    }
  } catch (...) { return 0; }
}

int midiIoOpenOutput(int portIndex) {
  midiIoCloseOutput();
  try {
    g_midiOut = new RtMidiOut();
    g_midiOut->openPort((unsigned int)portIndex, "ChooChooTracker Out");
    g_midiOutHead.store(0, std::memory_order_relaxed);
    g_midiOutTail.store(0, std::memory_order_relaxed);
    g_midiOutThreadRunning.store(true, std::memory_order_relaxed);
    g_midiOutThread = new std::thread(midiOutDrainThreadLoop);
    return 0;
  } catch (...) {
    delete g_midiOut;
    g_midiOut = NULL;
    return -1;
  }
}

void midiIoCloseOutput(void) {
  // Stop and join the drain thread before touching g_midiOut, so it can
  // never run against a port that's mid-close/deleted.
  if (g_midiOutThreadRunning.exchange(false, std::memory_order_relaxed)) {
    if (g_midiOutThread) { g_midiOutThread->join(); delete g_midiOutThread; g_midiOutThread = NULL; }
  }
  if (!g_midiOut) return;
  g_midiOut->closePort();
  delete g_midiOut;
  g_midiOut = NULL;
}

int midiIoIsOutputOpen(void) { return g_midiOut != NULL; }

void midiIoSendMessage(uint8_t status, uint8_t data1, uint8_t data2) {
  if (!g_midiOut) return;
  std::vector<unsigned char> message;
  message.push_back(status);
  if (channelMessageLength(status) >= 2) message.push_back(data1);
  if (channelMessageLength(status) == 3) message.push_back(data2);
  try {
    g_midiOut->sendMessage(&message);
  } catch (...) {
    // Device unplugged mid-stream or similar: drop the message, keep playing.
  }
}

#else // !DESKTOP_BUILD && !PORTMASTER_BUILD

int midiIoAvailable(void) { return 0; }
int midiIoInputPortCount(void) { return 0; }
int midiIoInputPortName(int, char*, int) { return -1; }
int midiIoOutputPortCount(void) { return 0; }
int midiIoOutputPortName(int, char*, int) { return -1; }
int midiIoOpenInput(int) { return -1; }
void midiIoCloseInput(void) {}
int midiIoIsInputOpen(void) { return 0; }
int midiIoPollInput(uint8_t*, uint8_t*, uint8_t*, uint64_t*) { return 0; }
int midiIoOpenOutput(int) { return -1; }
void midiIoCloseOutput(void) {}
int midiIoIsOutputOpen(void) { return 0; }
void midiIoSendMessage(uint8_t, uint8_t, uint8_t) {}
void midiIoScheduleMessage(uint8_t, uint8_t, uint8_t, uint64_t) {}
void midiIoFlushOutputQueue(void) {}
unsigned int midiIoGetDroppedCount(void) { return 0; }
int midiIoLaunchpadStart(void) { return 0; }
int midiIoLaunchpadConnected(void) { return 0; }
int midiIoLaunchpadPoll(uint8_t*, uint8_t*, uint8_t*) { return 0; }
void midiIoLaunchpadSetPad(uint8_t, uint8_t, int) {}
void midiIoLaunchpadSetButton(uint8_t, uint8_t) {}
void midiIoLaunchpadStop(void) {}

#endif
