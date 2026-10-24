#include "midi_backend_desktop.h"
#include "../midi_io.h"

// Thin adapters: userdata is unused throughout since midi_io.h is itself a
// free-function singleton (one process-wide input port, one output port),
// not an instantiable object.
static int inputPortCount(void*) { return midiIoInputPortCount(); }
static int inputPortName(void*, int index, char* buffer, int bufferSize) { return midiIoInputPortName(index, buffer, bufferSize); }
static int outputPortCount(void*) { return midiIoOutputPortCount(); }
static int outputPortName(void*, int index, char* buffer, int bufferSize) { return midiIoOutputPortName(index, buffer, bufferSize); }
static int openInput(void*, int portIndex) { return midiIoOpenInput(portIndex); }
static void closeInput(void*) { midiIoCloseInput(); }
static int openOutput(void*, int portIndex) { return midiIoOpenOutput(portIndex); }
static void closeOutput(void*) { midiIoCloseOutput(); }

static int pollInput(void*, MidiEvent* outEvent) {
  uint8_t status, data1, data2;
  uint64_t timestampMicros = 0;
  if (!midiIoPollInput(&status, &data1, &data2, &timestampMicros)) return 0;
  // Realtime bytes (0xF8 clock, 0xFA/0xFB/0xFC transport) keep their full
  // status so the router can tell them apart; channel messages keep the
  // existing type/channel split.
  outEvent->timestampMicros = timestampMicros;
  outEvent->type = status >= 0xf8 ? status : (status & 0xf0);
  outEvent->channel = status & 0x0f;
  outEvent->data1 = data1;
  outEvent->data2 = data2;
  return 1;
}

static void scheduleOutput(void*, const MidiEvent* event, uint64_t dueMicros) {
  midiIoScheduleMessage((uint8_t)(event->type | (event->channel & 0x0f)), event->data1, event->data2, dueMicros);
}

static void flushOutputQueue(void*) { midiIoFlushOutputQueue(); }
static unsigned int droppedCount(void*) { return midiIoGetDroppedCount(); }
static uint64_t nowMicros(void*) { return midiIoNowMicros(); }

static const MidiBackend kDesktopBackend = {
  nullptr, // userdata
  inputPortCount, inputPortName, outputPortCount, outputPortName,
  openInput, closeInput, openOutput, closeOutput,
  pollInput, scheduleOutput, flushOutputQueue, droppedCount, nowMicros,
  nullptr, nullptr,
#if defined(DESKTOP_BUILD) || defined(PORTMASTER_BUILD)
  1, 1, 1, 0, // hasInput, hasOutput, hasClockInput, hasClockOutput
#else
  0, 0, 0, 0,
#endif
};

const MidiBackend* midiBackendDesktopGet(void) {
  return &kDesktopBackend;
}
