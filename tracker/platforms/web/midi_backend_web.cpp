#include "midi/midi_backend_web.h"

#include <atomic>
#include <emscripten/emscripten.h>

namespace {

constexpr unsigned kInputCapacity = 2048;
constexpr unsigned kOutputCapacity = 1024;

struct RawMessage { uint8_t status, data1, data2; };
struct ScheduledMessage { RawMessage message; uint64_t dueMicros; };

RawMessage inputQueue[kInputCapacity];
std::atomic<unsigned> inputHead{0}, inputTail{0};
ScheduledMessage outputQueue[kOutputCapacity];
std::atomic<unsigned> outputHead{0}, outputTail{0};
std::atomic<unsigned> outputDropped{0};

EM_JS(int, webMidiPortCount, (int isInput), {
  return window.choochooWebMidi ? window.choochooWebMidi.portCount(isInput) : 0;
});
EM_JS(int, webMidiPortName, (int isInput, int index, char* buffer, int bufferSize), {
  if (!window.choochooWebMidi) return -1;
  const name = window.choochooWebMidi.portName(isInput, index);
  if (name === null || name === undefined) return -1;
  stringToUTF8(name, buffer, bufferSize);
  return 0;
});
EM_JS(int, webMidiOpenPort, (int isInput, int index), {
  return window.choochooWebMidi && window.choochooWebMidi.openPort(isInput, index) ? 0 : -1;
});
EM_JS(void, webMidiClosePort, (int isInput), {
  if (window.choochooWebMidi) window.choochooWebMidi.closePort(isInput);
});
EM_JS(void, webMidiRequestAccessJs, (), {
  if (window.choochooWebMidi) window.choochooWebMidi.requestAccess();
});
EM_JS(int, webMidiStatus, (), {
  return window.choochooWebMidi ? window.choochooWebMidi.status() : 2;
});
EM_JS(void, webMidiSend, (int status, int data1, int data2, double dueMillis), {
  if (window.choochooWebMidi) window.choochooWebMidi.send(status, data1, data2, dueMillis);
});

bool pushInput(uint8_t status, uint8_t data1, uint8_t data2) {
  unsigned head = inputHead.load(std::memory_order_relaxed);
  unsigned next = (head + 1) % kInputCapacity;
  if (next == inputTail.load(std::memory_order_acquire)) return false;
  inputQueue[head] = {status, data1, data2};
  inputHead.store(next, std::memory_order_release);
  return true;
}

void drainOutput() {
  unsigned tail = outputTail.load(std::memory_order_relaxed);
  unsigned head = outputHead.load(std::memory_order_acquire);
  while (tail != head) {
    const ScheduledMessage item = outputQueue[tail];
    webMidiSend(item.message.status, item.message.data1, item.message.data2,
                (double)item.dueMicros / 1000.0);
    tail = (tail + 1) % kOutputCapacity;
  }
  outputTail.store(tail, std::memory_order_release);
}

int inputPortCount(void*) { return webMidiPortCount(1); }
int outputPortCount(void*) { return webMidiPortCount(0); }
int inputPortName(void*, int i, char* b, int n) { return webMidiPortName(1, i, b, n); }
int outputPortName(void*, int i, char* b, int n) { return webMidiPortName(0, i, b, n); }
int openInput(void*, int i) {
  inputTail.store(inputHead.load(std::memory_order_acquire), std::memory_order_release);
  return webMidiOpenPort(1, i);
}
void closeInput(void*) {
  webMidiClosePort(1);
  inputTail.store(inputHead.load(std::memory_order_acquire), std::memory_order_release);
}
int openOutput(void*, int i) { return webMidiOpenPort(0, i); }
void closeOutput(void*) { webMidiClosePort(0); }
int pollInput(void*, MidiEvent* event) {
  drainOutput();
  unsigned tail = inputTail.load(std::memory_order_relaxed);
  if (tail == inputHead.load(std::memory_order_acquire)) return 0;
  const RawMessage message = inputQueue[tail];
  inputTail.store((tail + 1) % kInputCapacity, std::memory_order_release);
  *event = {0, (uint8_t)(message.status & 0xf0), (uint8_t)(message.status & 0x0f), message.data1, message.data2};
  return 1;
}
void scheduleOutput(void*, const MidiEvent* event, uint64_t dueMicros) {
  unsigned head = outputHead.load(std::memory_order_relaxed);
  unsigned next = (head + 1) % kOutputCapacity;
  if (next == outputTail.load(std::memory_order_acquire)) {
    outputDropped.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  outputQueue[head] = {{(uint8_t)(event->type | (event->channel & 0x0f)), event->data1, event->data2}, dueMicros};
  outputHead.store(next, std::memory_order_release);
}
void flushOutput(void*) { outputTail.store(outputHead.load(std::memory_order_acquire), std::memory_order_release); }
unsigned droppedCount(void*) { return outputDropped.load(std::memory_order_relaxed); }
uint64_t nowMicros(void*) { return (uint64_t)(emscripten_get_now() * 1000.0); }
void requestAccess(void*) { webMidiRequestAccessJs(); }
const char* statusMessage(void*) {
  switch (webMidiStatus()) {
    case 1: return "Waiting for Web MIDI";
    case 2: return "Web MIDI unavailable";
    case 3: return "Web MIDI permission denied";
    case 4: return "Web MIDI failed";
    default: return nullptr;
  }
}

const MidiBackend kWebBackend = {
  nullptr,
  inputPortCount, inputPortName, outputPortCount, outputPortName,
  openInput, closeInput, openOutput, closeOutput,
  pollInput, scheduleOutput, flushOutput, droppedCount, nowMicros,
  requestAccess, statusMessage,
  1, 1, 0, 0,
};

} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void webMidiMessage(int status, int data1, int data2) {
  if (status < 0x80 || status >= 0xf0) return;
  pushInput((uint8_t)status, (uint8_t)(data1 & 0x7f), (uint8_t)(data2 & 0x7f));
}

const MidiBackend* midiBackendWebGet(void) { return &kWebBackend; }
