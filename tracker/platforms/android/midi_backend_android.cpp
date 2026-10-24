#include "midi/midi_backend_android.h"

#include <SDL2/SDL.h>
#include <jni.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

namespace {

constexpr unsigned kInputCapacity = 2048;
constexpr unsigned kOutputCapacity = 1024;

struct RawMessage { uint8_t status, data1, data2; };
static RawMessage inputQueue[kInputCapacity];
static std::atomic<unsigned> inputHead{0}, inputTail{0};

struct ScheduledMessage { RawMessage message; uint64_t dueMicros; };
static ScheduledMessage outputQueue[kOutputCapacity];
static std::atomic<unsigned> outputHead{0}, outputTail{0};
static std::atomic<unsigned> outputDropped{0};
static std::atomic<bool> outputRunning{false};
static std::thread outputThread;

static JavaVM* vm = nullptr;
static jobject activity = nullptr;
static jmethodID initializeMidi = nullptr;
static jmethodID inputCount = nullptr;
static jmethodID inputName = nullptr;
static jmethodID outputCount = nullptr;
static jmethodID outputName = nullptr;
static jmethodID openInput = nullptr;
static jmethodID closeInput = nullptr;
static jmethodID openOutput = nullptr;
static jmethodID closeOutput = nullptr;
static jmethodID sendOutput = nullptr;

struct AttachedEnv {
  JNIEnv* env = nullptr;
  bool attached = false;
  bool get(void) {
    if (!vm) return false;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) return true;
    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return false;
    attached = true;
    return true;
  }
  ~AttachedEnv(void) { if (attached) vm->DetachCurrentThread(); }
};

static uint64_t nowMicros(void*) {
  using Clock = std::chrono::steady_clock;
  return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(
    Clock::now().time_since_epoch()).count();
}

static bool ensureJava(void) {
  if (activity) return true;
  JNIEnv* env = reinterpret_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
  jobject current = reinterpret_cast<jobject>(SDL_AndroidGetActivity());
  if (!env || !current) return false;
  env->GetJavaVM(&vm);
  activity = env->NewGlobalRef(current);
  jclass cls = env->GetObjectClass(activity);
  initializeMidi = env->GetMethodID(cls, "initializeMidi", "()V");
  inputCount = env->GetMethodID(cls, "midiInputPortCount", "()I");
  inputName = env->GetMethodID(cls, "midiInputPortName", "(I)Ljava/lang/String;");
  outputCount = env->GetMethodID(cls, "midiOutputPortCount", "()I");
  outputName = env->GetMethodID(cls, "midiOutputPortName", "(I)Ljava/lang/String;");
  openInput = env->GetMethodID(cls, "openMidiInput", "(I)Z");
  closeInput = env->GetMethodID(cls, "closeMidiInput", "()V");
  openOutput = env->GetMethodID(cls, "openMidiOutput", "(I)Z");
  closeOutput = env->GetMethodID(cls, "closeMidiOutput", "()V");
  sendOutput = env->GetMethodID(cls, "sendMidiOutput", "([B)V");
  env->DeleteLocalRef(cls);
  if (!initializeMidi || !inputCount || !inputName || !outputCount || !outputName ||
      !openInput || !closeInput || !openOutput || !closeOutput || !sendOutput) return false;
  env->CallVoidMethod(activity, initializeMidi);
  return !env->ExceptionCheck();
}

static bool pushInput(uint8_t status, uint8_t data1, uint8_t data2) {
  unsigned head = inputHead.load(std::memory_order_relaxed);
  unsigned next = (head + 1) % kInputCapacity;
  if (next == inputTail.load(std::memory_order_acquire)) return false;
  inputQueue[head] = {status, data1, data2};
  inputHead.store(next, std::memory_order_release);
  return true;
}

static void parseInput(const uint8_t* bytes, int length) {
  static uint8_t status = 0, data[2] = {}, dataCount = 0, expected = 0;
  for (int i = 0; i < length; ++i) {
    uint8_t byte = bytes[i];
    if (byte >= 0xf8) {
      // Realtime bytes can interleave anywhere, even mid-message, and must
      // never disturb the running-status state machine. Clock (0xF8) and
      // transport (0xFA/0xFB/0xFC) are forwarded for MIDI clock sync;
      // active sensing (0xFE) and reset (0xFF) are dropped.
      if (byte == 0xf8 || byte == 0xfa || byte == 0xfb || byte == 0xfc)
        pushInput(byte, 0, 0);
      continue;
    }
    if (byte & 0x80) {
      if (byte < 0xf0) {
        status = byte;
        dataCount = 0;
        expected = ((byte & 0xf0) == 0xc0 || (byte & 0xf0) == 0xd0) ? 1 : 2;
      } else {
        status = dataCount = expected = 0;
      }
      continue;
    }
    if (!status || !expected) continue;
    data[dataCount++] = byte & 0x7f;
    if (dataCount == expected) {
      pushInput(status, data[0], expected == 2 ? data[1] : 0);
      dataCount = 0;
    }
  }
}

static void drainOutput(void) {
  AttachedEnv attached;
  if (!attached.get()) return;
  JNIEnv* env = attached.env;
  while (outputRunning.load(std::memory_order_acquire)) {
    unsigned tail = outputTail.load(std::memory_order_relaxed);
    unsigned head = outputHead.load(std::memory_order_acquire);
    if (tail == head) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }
    ScheduledMessage item = outputQueue[tail];
    uint64_t now = nowMicros(nullptr);
    if (item.dueMicros > now) {
      uint64_t delay = item.dueMicros - now;
      std::this_thread::sleep_for(std::chrono::microseconds(delay > 2000 ? 1000 : delay));
      continue;
    }
    jbyte bytes[3] = {(jbyte)item.message.status, (jbyte)item.message.data1, (jbyte)item.message.data2};
    jbyteArray array = env->NewByteArray(3);
    if (array) {
      env->SetByteArrayRegion(array, 0, 3, bytes);
      env->CallVoidMethod(activity, sendOutput, array);
      env->DeleteLocalRef(array);
      if (env->ExceptionCheck()) env->ExceptionClear();
    }
    outputTail.store((tail + 1) % kOutputCapacity, std::memory_order_release);
  }
}

static int portCount(jmethodID method) {
  if (!ensureJava()) return 0;
  AttachedEnv attached;
  if (!attached.get()) return 0;
  return attached.env->CallIntMethod(activity, method);
}

static int portName(jmethodID method, int index, char* buffer, int bufferSize) {
  if (!buffer || bufferSize <= 0 || !ensureJava()) return -1;
  AttachedEnv attached;
  if (!attached.get()) return -1;
  JNIEnv* env = attached.env;
  jstring value = reinterpret_cast<jstring>(env->CallObjectMethod(activity, method, index));
  if (!value || env->ExceptionCheck()) {
    if (env->ExceptionCheck()) env->ExceptionClear();
    return -1;
  }
  const char* text = env->GetStringUTFChars(value, nullptr);
  if (!text) { env->DeleteLocalRef(value); return -1; }
  std::strncpy(buffer, text, (size_t)bufferSize - 1);
  buffer[bufferSize - 1] = '\0';
  env->ReleaseStringUTFChars(value, text);
  env->DeleteLocalRef(value);
  return 0;
}

static int callBool(jmethodID method, int index) {
  if (!ensureJava()) return -1;
  AttachedEnv attached;
  if (!attached.get()) return -1;
  jboolean result = attached.env->CallBooleanMethod(activity, method, index);
  if (attached.env->ExceptionCheck()) { attached.env->ExceptionClear(); return -1; }
  return result ? 0 : -1;
}

static void closeJava(jmethodID method) {
  if (!activity || !method) return;
  AttachedEnv attached;
  if (attached.get()) attached.env->CallVoidMethod(activity, method);
}

static int inputPortCount(void*) { return portCount(inputCount); }
static int outputPortCount(void*) { return portCount(outputCount); }
static int inputPortName(void*, int i, char* b, int n) { return portName(inputName, i, b, n); }
static int outputPortName(void*, int i, char* b, int n) { return portName(outputName, i, b, n); }

static int openInputPort(void*, int i) {
  inputTail.store(inputHead.load(std::memory_order_acquire), std::memory_order_release);
  return callBool(openInput, i);
}
static void closeInputPort(void*) {
  closeJava(closeInput);
  inputTail.store(inputHead.load(std::memory_order_acquire), std::memory_order_release);
}
static int openOutputPort(void*, int i) {
  // Switching MIDI Out from the settings screen can happen while the
  // previous drain thread is still alive. Join it before replacing the
  // std::thread object; assigning to a joinable thread terminates the app.
  if (outputRunning.load(std::memory_order_acquire) || outputThread.joinable()) {
    outputRunning.store(false, std::memory_order_release);
    if (outputThread.joinable()) outputThread.join();
    closeJava(closeOutput);
  }
  int result = callBool(openOutput, i);
  if (result != 0) return result;
  outputHead.store(0, std::memory_order_release);
  outputTail.store(0, std::memory_order_release);
  outputDropped.store(0, std::memory_order_release);
  outputRunning.store(true, std::memory_order_release);
  outputThread = std::thread(drainOutput);
  return 0;
}
static void closeOutputPort(void*) {
  outputRunning.store(false, std::memory_order_release);
  if (outputThread.joinable()) outputThread.join();
  closeJava(closeOutput);
  outputTail.store(outputHead.load(std::memory_order_acquire), std::memory_order_release);
}
static int pollInput(void*, MidiEvent* event) {
  unsigned tail = inputTail.load(std::memory_order_relaxed);
  if (tail == inputHead.load(std::memory_order_acquire)) return 0;
  RawMessage message = inputQueue[tail];
  inputTail.store((tail + 1) % kInputCapacity, std::memory_order_release);
  event->timestampMicros = 0;
  // Realtime bytes keep their full status so the router can tell them
  // apart; channel messages keep the existing type/channel split.
  event->type = message.status >= 0xf8 ? message.status : (message.status & 0xf0);
  event->channel = message.status & 0x0f;
  event->data1 = message.data1;
  event->data2 = message.data2;
  return 1;
}
static void scheduleOutput(void*, const MidiEvent* event, uint64_t dueMicros) {
  if (!outputRunning.load(std::memory_order_acquire)) return;
  unsigned head = outputHead.load(std::memory_order_relaxed);
  unsigned next = (head + 1) % kOutputCapacity;
  if (next == outputTail.load(std::memory_order_acquire)) {
    outputDropped.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  outputQueue[head] = {{(uint8_t)(event->type | (event->channel & 0x0f)), event->data1, event->data2}, dueMicros};
  outputHead.store(next, std::memory_order_release);
}
static void flushOutput(void*) { outputTail.store(outputHead.load(std::memory_order_acquire), std::memory_order_release); }
static unsigned droppedCount(void*) { return outputDropped.load(std::memory_order_relaxed); }

static const MidiBackend backend = {
  nullptr,
  inputPortCount, inputPortName, outputPortCount, outputPortName,
  openInputPort, closeInputPort, openOutputPort, closeOutputPort,
  pollInput, scheduleOutput, flushOutput, droppedCount, nowMicros,
  nullptr, nullptr,
  1, 1, 1, 0,
};

} // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_paiheulevrai_choochootracker_ChooChooTrackerActivity_nativeMidiMessage(
    JNIEnv* env, jclass, jbyteArray bytes, jlong) {
  if (!bytes) return;
  jsize length = env->GetArrayLength(bytes);
  if (length <= 0) return;
  jbyte* data = env->GetByteArrayElements(bytes, nullptr);
  if (data) {
    parseInput(reinterpret_cast<const uint8_t*>(data), length);
    env->ReleaseByteArrayElements(bytes, data, JNI_ABORT);
  }
}

const MidiBackend* midiBackendAndroidGet(void) {
  ensureJava();
  return &backend;
}
