#include "formant_filter.h"
#include "insert_fx.h"
#include "beads_insert.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_LN10
#define M_LN10 2.30258509299404568402
#endif
#include "external/insert_fx/Distortion.h"
#include "external/insert_fx/StereoDoubler.h"
#include "external/insert_fx/ott_dsp.c"
#include "external/insert_fx/tapescam.h"
using M = InsertMapping;
static const InsertDescriptor descriptors[] = {
    {"OFF", 0, {}},
    {"Compressor",
     8,
     {{"Thresh", "Threshold", "dB", -60, 0, 153, M::linear},
      {"Attack", "Attack", "Hz", 800, 3, 96, M::exponential},
      {"Release", "Release", "Hz", 60, 0.4f, 128, M::exponential},
      {"Makeup", "Makeup", "dB", 0, 24, 0, M::linear},
      {"Ratio", "Ratio", "", 0, 7, 3, M::discrete},
      {"Detect", "Detector source", "", 0, 10, 0, M::discrete},
      {"DetFilt", "Detector filter", "", -1, 1, 128, M::bipolar},
      {"Mix", "Mix", "%", 0, 1, 255, M::linear}}},
    {"Distortion",
     4,
     {{"Input", "Input", "dB", -12, 12, 128, M::bipolar},
      {"Mode", "Mode", "", 0, 4, 0, M::discrete},
      {"Output", "Output", "dB", -12, 12, 128, M::bipolar},
      {"Mix", "Mix", "%", 0, 1, 255, M::linear}}},
    {"Doubler",
     2,
     {{"Detune", "Detune", "%", 0, 1, 153, M::linear}, {"Mix", "Mix", "%", 0, 1, 102, M::linear}}},
    {"TAPESCAM",
     6,
     {{"Input", "Input", "%", 0, 1, 179, M::linear},
      {"Drive", "Drive", "%", 0, 1, 0, M::linear},
      {"Color", "Color", "%", 0, 1, 0, M::linear},
      {"Wobble", "Wobble", "%", 0, 1, 0, M::linear},
      {"Tone", "Tone", "", -1, 1, 128, M::bipolar},
      {"Output", "Output", "%", 0, 1, 255, M::linear}}},
    {"OTT",
     6,
     {{"Depth", "Depth", "%", 0, 1, 64, M::linear},
      {"Time", "Time", "x", 0.1f, 2, 128, M::exponential},
      {"Upward", "Upward", "%", 0, 1, 64, M::linear},
      {"Down", "Downward", "%", 0, 1, 128, M::linear},
      {"Input", "Input", "dB", -24, 24, 128, M::bipolar},
      {"Output", "Output", "dB", -24, 24, 112, M::bipolar}}},
    {"Chorus",
     5,
     {{"Rate", "Rate", "Hz", 0.05f, 20, 99, M::exponential},
      {"Depth", "Depth", "ms", 0.2f, 50, 145, M::exponential},
      {"Tone", "Tone", "Hz", 800, 18000, 210, M::exponential},
      {"Mix", "Mix", "%", 0, 1, 128, M::linear},
      {"Feedback", "Feedback", "%", -0.98f, 0.98f, 140, M::bipolar}}},
    {"Flanger",
     4,
     {{"Rate", "Rate", "Hz", 0.02f, 20, 100, M::exponential},
      {"Depth", "Depth", "ms", 0.1f, 20, 120, M::exponential},
      {"Feedback", "Feedback", "%", -0.98f, 0.98f, 154, M::bipolar},
      {"Mix", "Mix", "%", 0, 1, 100, M::linear}}},
    {"Phaser",
     5,
     {{"Rate", "Rate", "Hz", 0.02f, 15, 92, M::exponential},
      {"Depth", "Depth", "%", 0, 2, 180, M::linear},
      {"Feedback", "Feedback", "%", -0.98f, 0.98f, 160, M::bipolar},
      {"Mix", "Mix", "%", 0, 1, 144, M::linear},
      {"Stages", "Stages", "", 0, 4, 1, M::discrete}}},
    {"Rotary",
     4,
     {{"Speed", "Speed", "Hz", 0.01f, 25, 143, M::exponential},
      {"Depth", "Depth", "%", 0, 1, 128, M::linear},
      {"Drive", "Drive", "dB", 0, 12, 0, M::linear},
      {"Mix", "Mix", "%", 0, 1, 128, M::linear}}},
    {"Saturation",
     4,
     {{"Drive", "Drive", "dB", 0, 18, 80, M::linear},
      {"Tone", "Tone", "Hz", 500, 18000, 220, M::exponential},
      {"Level", "Level", "dB", -12, 12, 128, M::bipolar},
      {"Mix", "Mix", "%", 0, 1, 255, M::linear}}},
    {"Bitcrusher",
     4,
     {{"Bits", "Bit depth", "", 0, 12, 8, M::discrete},
      {"Rate", "Rate reduction", "x", 0, 31, 3, M::discrete},
      {"Tone", "Tone", "Hz", 600, 18000, 220, M::exponential},
      {"Mix", "Mix", "%", 0, 1, 192, M::linear}}},
    {"Destruction",
     4,
     {{"Mode", "Mode", "", 0, 2, 0, M::discrete},
      {"Amount", "Amount", "%", 0, 1, 64, M::linear},
      {"Tone", "Tone", "Hz", 500, 18000, 220, M::exponential},
      {"Mix", "Mix", "%", 0, 1, 160, M::linear}}},
    {"Parametric EQ",
     6,
     {{"Low G", "Low gain", "dB", -24, 24, 128, M::bipolar},
      {"Low F", "Low frequency", "Hz", 60, 500, 84, M::exponential},
      {"Mid G", "Mid gain", "dB", -24, 24, 128, M::bipolar},
      {"Mid F", "Mid frequency", "Hz", 250, 5000, 118, M::exponential},
      {"High G", "High gain", "dB", -24, 24, 128, M::bipolar},
      {"High F", "High frequency", "Hz", 2000, 14000, 182, M::exponential}}},
    {"Trance Gate",
     8,
     {{"Pattern", "Pattern", "", 0, 63, 0, M::discrete},
      {"Rate", "Rate", "", 0, 14, 9, M::discrete},
      {"Length", "Length", "steps", 0, 15, 15, M::discrete},
      {"Gate", "Gate width", "%", 0, 1, 128, M::linear},
      {"Phase", "Phase", "", 0, 15, 0, M::discrete},
      {"Mode", "Mode", "", 0, 3, 0, M::discrete},
      {"Smooth", "Smooth", "%", 0, 1, 20, M::linear},
      {"Mix", "Mix", "%", 0, 1, 255, M::linear}}},
    {"Beads", 8,
     {{"Time", "Grain position", "%", 0, 1, 26, M::linear},
      {"Size", "Grain size/direction", "%", 0, 1, 191, M::linear},
      {"Density", "Grain density", "%", 0, 1, 179, M::linear},
      {"Pitch", "Grain pitch", "st", -24, 24, 128, M::bipolar},
      {"Scatter", "Grain scatter", "%", 0, 1, 0, M::linear},
      {"Feedback", "Feedback", "%", 0, 1, 0, M::linear},
      {"Shp/Frz", "Shape / Freeze", "", 0, 255, 38, M::discrete},
      {"Mix", "Mix", "%", 0, 1, 128, M::linear}}},
    {"FORMANT", 6,
     {{"MODE", "Mode", "", 0, 1, 0, M::discrete},
      {"VOW", "Vowel", "", 0, 1, 0, M::linear},
      {"F1", "First formant", "Hz", 200, 1200, 206, M::exponential},
      {"F2", "Second formant", "Hz", 500, 2800, 173, M::exponential},
      {"RES", "Resonance", "Q", 2, 16, 85, M::exponential},
      {"MIX", "Mix", "%", 0, 1, 255, M::linear}}}};
static_assert(sizeof(descriptors) / sizeof(descriptors[0]) == insertModuleCount,
              "Stable module catalogue");
const InsertDescriptor& insertDescriptor(int m) {
  return descriptors[m >= 0 && m < insertModuleCount ? m : 0];
}
int insertParameterMaximum(int m, int p) {
  const auto& d = insertDescriptor(m);
  if (p < 0 || p >= d.count) return 0;
  if (m == insertFormant && p == 1) return 127;
  return d.parameters[p].mapping == M::discrete ? (int)d.parameters[p].maximum : 255;
}
bool insertParameterActive(int m, int p, int mode) {
  return p >= 0 && p < insertDescriptor(m).count &&
    (m != insertFormant || (mode ? p != 1 : p != 2 && p != 3));
}
bool insertParameterModulatable(int m, int p, int mode) {
  return insertParameterActive(m, p, mode) && !(m == insertFormant && p == 0);
}
uint8_t insertClamp(int m, int p, int v) {
  return std::clamp(v, 0, insertParameterMaximum(m, p));
}
float insertMap(int m, int p, int v) {
  const auto& d = insertDescriptor(m);
  if (p < 0 || p >= d.count) return 0;
  const auto& a = d.parameters[p];
  v = insertClamp(m, p, v);
  float x = v / float(std::max(1, insertParameterMaximum(m, p)));
  if (a.mapping == M::discrete) return v;
  if (a.mapping == M::exponential) return a.minimum * powf(a.maximum / a.minimum, x);
  if (a.mapping == M::bipolar)
    return v >= 128 ? a.maximum * (v - 128) / 127.0f : -a.minimum * (v - 128) / 128.0f;
  return a.minimum + (a.maximum - a.minimum) * x;
}
void insertSelect(InsertConfig* c, int m) {
  c->module = m >= 0 && m < insertModuleCount ? m : 0;
  c->bypass = 0;
  ++c->selection;
  for (int i = 0; i < 8; ++i) {
    c->values[i] = insertDescriptor(c->module).parameters[i].initial;
    ++c->edits[i];
  }
}
void insertEdit(InsertConfig* c, int p, int v) {
  if (p < 0 || p >= insertDescriptor(c->module).count) return;
  c->values[p] = insertClamp(c->module, p, v);
  ++c->edits[p];
}
void insertDescribe(char* text, size_t size, int m, int p, int value) {
  const auto& d = insertDescriptor(m);
  if (p < 0 || p >= d.count) {
    snprintf(text, size, "Inactive");
    return;
  }
  float v = insertMap(m, p, value);
  if (m == insertFormant && p == 0) {
    snprintf(text, size, "%s", value ? "CUSTOM" : "VOW");
  } else if (m == insertFormant && p == 1) {
    const int anchors[] = {0, 32, 64, 96, 127};
    const char* names[] = {"A", "E", "I", "O", "U"};
    value = insertClamp(m, p, value);
    int i = 0;
    while (i < 4 && value >= anchors[i + 1]) ++i;
    if (value == anchors[i]) snprintf(text, size, "%s", names[i]);
    else snprintf(text, size, "%s>%s %d%%", names[i], names[i+1],
                  (value-anchors[i])*100/(anchors[i+1]-anchors[i]));
  } else if (m == insertBeads && p == 6) {
    int packed = (int)v;
    snprintf(text, size, "%03d%% %s", (packed & 127) * 100 / 127,
             packed & 128 ? "Frozen" : "Recording");
  } else if (m == insertBeads && p == 1 && value == 255) {
    snprintf(text, size, "Delay mode");
  } else if (m == insertBeads && p == 2) {
    snprintf(text, size, "%s %.0f%%", v < .48f ? "Regular" : v > .52f ? "Random" : "Stopped",
             v * 100);
  } else if (m == insertDistortion && p == 1) {
    static const char* n[] = {"Density", "Drive", "Spiral", "Mojo", "Dyno"};
    snprintf(text, size, "%s", n[(int)v]);
  } else if (m == insertRotary && p == 0) {
    // Keep neighboring very slow rates distinguishable in the edit hint.
    snprintf(text, size, "%.4g Hz", v);
  } else if (m == insertBitcrusher && p == 0) {
    snprintf(text, size, "%d bit", (int)v + 4);
  } else if (m == insertBitcrusher && p == 1) {
    snprintf(text, size, "%dx", (int)v + 1);
  } else if (m == insertPhaser && p == 4) {
    snprintf(text, size, "%d stages", (int)v + 4);
  } else if (m == insertDestruction && p == 0) {
    static const char* n[] = {"Fold", "Clip", "Crush"};
    snprintf(text, size, "%s", n[(int)v]);
  } else if (m == insertCompressor && p == 4) {
    static const float n[] = {1.5f, 2, 3, 4, 6, 8, 16, 20};
    snprintf(text, size, "%.1f:1", n[(int)v]);
  } else if (m == insertCompressor && p == 5) {
    static const char* n[] = {"Stereo", "Left", "Right"};
    if (v < 3) snprintf(text, size, "%s", n[(int)v]);
    else snprintf(text, size, "Track %d", (int)v - 2);
  } else if (m == insertTranceGate && p == 0) {
    static const char* n[] = {
        "Full 1/16", "Downbeat 4/4", "8th Note Pulse", "Double Pump",
        "Minimal Stab", "Heavy Stomp", "Half Bar Heavy", "Triple Push",
        "Psy Offbeat", "Triplet Galop", "Rolling Lead", "Goa Flow",
        "Triple Staccato", "Classic Uplifting", "Gate Sweep", "Rave Chord",
        "Build-Up Ramp", "Split Bar", "Measure Anchor", "Double Center Beat",
        "Psy Accent", "Staggered Pump", "Slow Answer", "Odd Skank",
        "Polyrhythmic 3/4", "Sparse Scatter", "Broken Beat", "Industrial Stutter",
        "Random Flutter", "Lone Stab", "Front Load", "Polystep",
        "Fractal Drift", "Glass Engine", "Binary Bloom", "Orbit Shift",
        "Hexa Pulse", "Razor Roll", "Lunar Skip", "Neon Scatter",
        "Offset Drive", "Phase Fray", "Woven Steps", "Pulse Mirage",
        "Fractured Rise", "Void Stabs", "Shuffled Flux", "Spiral Cut",
        "Prism Runner", "Jagged Echo", "Solar Static", "Crosswire",
        "Metric Flux", "Machine Prayer", "Ghost Train", "Overclock",
        "Warp Lattice", "Kinetic Dust", "Chrome Rattle", "Tension Field",
        "Rift Walker", "Echo Mosaic", "Shiver Grid", "Final Orbit"};
    snprintf(text, size, "%02X %s", (unsigned)v, n[(int)v]);
  } else if (m == insertTranceGate && p == 1) {
    static const char* n[] = {"1/2", "1/2T", "1/2.", "1/4", "1/4T", "1/4.", "1/8", "1/8T", "1/8.", "1/16", "1/16T", "1/16.", "1/32", "1/32T", "1/32."};
    snprintf(text, size, "%s", n[(int)v]);
  } else if (m == insertTranceGate && p == 2) {
    snprintf(text, size, "%d steps", (int)v + 1);
  } else if (m == insertTranceGate && p == 4) {
    snprintf(text, size, "%d steps", (int)v);
  } else if (m == insertTranceGate && p == 5) {
    static const char* n[] = {"Forward", "Reverse", "Ping-Pong", "Chaos"};
    snprintf(text, size, "%s", n[(int)v]);
  } else if (m == insertCompressor && (p == 1 || p == 2))
    snprintf(text, size, "%.2f ms", 1000.0f / (6.28318530718f * v));
  else
    snprintf(text, size, "%.2f %s", d.parameters[p].units[0] == '%' ? v * 100 : v,
             d.parameters[p].units);
}
static constexpr uint16_t tranceGatePatterns[] = {
    // 00-1F: immediate techno, house, trance and psy foundations.
    0xffff, 0x8888, 0xaaaa, 0xc0c0, 0x1010, 0x9090, 0xf0f0, 0xe0e0,
    0x7777, 0x6db6, 0xb6da, 0x3f3f, 0xa8a8, 0xf1f1, 0x1ee1, 0xf888,
    0x7f00, 0xf00f, 0x8081, 0x6666, 0x3a3a, 0xc888, 0xcc80, 0xa490,
    0x9248, 0x4210, 0xb4b4, 0x6868, 0xd3d3, 0x8000, 0xf800, 0xe248,
    // 20-3F: asymmetric, broken and deliberately stranger material.
    0x391c, 0x3c7a, 0x0c42, 0x8912, 0xca49, 0xf29b, 0x6097, 0x9a04,
    0x9034, 0x33e4, 0x69c1, 0x3f48, 0xdbe0, 0x1970, 0xd5d6, 0xa32f,
    0xe85a, 0x61b4, 0xe847, 0x42e2, 0x8b83, 0x5d8b, 0xe1b0, 0xc1fb,
    0xb994, 0xc291, 0xcf61, 0x715a, 0xab68, 0xaf19, 0x57c5, 0xf58a};
constexpr uint16_t rotateTranceGate(uint16_t value, int amount) {
  return amount ? (uint16_t)((value << amount) | (value >> (16 - amount))) : value;
}
constexpr uint16_t reverseTranceGate(uint16_t value) {
  uint16_t result = 0;
  for (int bit = 0; bit < 16; ++bit) result |= ((value >> bit) & 1) << (15 - bit);
  return result;
}
constexpr bool tranceGatePatternsAreUnique() {
  for (int i = 0; i < (int)(sizeof(tranceGatePatterns) / sizeof(tranceGatePatterns[0])); ++i)
    for (int j = 0; j < i; ++j)
    for (int rotation = 0; rotation < 16; ++rotation)
      if (rotateTranceGate(tranceGatePatterns[i], rotation) == tranceGatePatterns[j] ||
          rotateTranceGate(reverseTranceGate(tranceGatePatterns[i]), rotation) == tranceGatePatterns[j])
        return false;
  return true;
}
static_assert(tranceGatePatternsAreUnique(), "Trance Gate patterns must not be rotations or mirrors");
bool tranceGatePatternOpen(uint8_t pattern, uint8_t length, uint32_t step) {
  length = std::max<uint8_t>(1, std::min<uint8_t>(16, length));
  return (tranceGatePatterns[pattern & 63] & (0x8000u >> (step % length))) != 0;
}
float tranceGateTicksPerStep(uint8_t rate) {
  rate = std::min<uint8_t>(14, rate);
  float ticks = 48.0f / (1 << (rate / 3));
  if (rate % 3 == 1) ticks *= 2.0f / 3.0f;
  if (rate % 3 == 2) ticks *= 1.5f;
  return ticks;
}
static uint32_t gateRandom(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  return value ^ (value >> 16);
}
static bool tranceGateOpen(const uint8_t* v, uint64_t step) {
  const int length = v[2] + 1;
  const int mode = v[5];
  const int span = mode == 2 && length > 1 ? length * 2 - 2 : length;
  const uint64_t loop = step / span;
  int position = (int)(step % span);
  if (mode == 1) position = length - 1 - position;
  else if (mode == 2 && position >= length) position = span - position;
  bool bits[16];
  int opened = 0;
  for (int i = 0; i < length; ++i) {
    bits[i] = (tranceGatePatterns[v[0]] & (0x8000u >> ((i - v[4] + 16) & 15))) != 0;
    opened += bits[i];
  }
  if (mode == 3 && opened && opened < length) {
    int open[16], closed[16], openCount = 0, closedCount = 0;
    for (int i = 0; i < length; ++i) {
      if (bits[i]) open[openCount++] = i;
      else closed[closedCount++] = i;
    }
    for (int move = 0; move < 2 && openCount && closedCount; ++move) {
      uint32_t random = gateRandom((uint32_t)v[0] | ((uint32_t)v[4] << 8) |
                                   ((uint32_t)(loop / 4) << 16) | ((uint32_t)move << 28));
      int openPick = random % openCount, closedPick = (random >> 8) % closedCount;
      bits[open[openPick]] = false;
      bits[closed[closedPick]] = true;
      open[openPick] = open[--openCount];
      closed[closedPick] = closed[--closedCount];
    }
  }
  return bits[position];
}

#include "external/insert_fx/work_compressor.h"
struct InsertChain::Impl {
  static constexpr int block = 128;
  static constexpr int delaySize = 4096;
  struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void reset() { x1 = x2 = y1 = y2 = 0; }
    float process(float x) {
      float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
      x2 = x1; x1 = x; y2 = y1; y1 = y;
      return y;
    }
  };
  struct Slot {
    InsertConfig config{};
    uint8_t previous[8]{};
    bool dirty = true;
    int module = 0, pending = 0;
    float wet = 0;
    bool switching = false;
    BeadsPool::Handle beads;
    FormantFilter formant;
    WorkCompressor work;
    insert_dsp::Distortion distortion;
    insert_dsp::StereoDoubler doubler;
    insert_tape::tapescam_instance_t tape;
    ott_dsp_t* ott[2]{};
    float delay[2][delaySize]{};
    float filter[2]{};
    float allpassX[2][8]{}, allpassY[2][8]{}, phaserFeedback[2]{};
    // Long LFO cycles need enough precision to accumulate sub-microradian steps.
    double phase = 0;
    double gateFrames = 0;
    float gateLevel = 0;
    float feedback[2]{}, held[2]{};
    Biquad eq[2][3];
    int delayWrite = 0, holdCounter = 0;
    float toneCoefficient = 1;
    bool eqNeutral = true;
  } slots[2];
  float rate;
  float tickRate = 50;
  explicit Impl(float sr) : rate(sr) {
    for (auto& s : slots) {
      s.ott[0] = ott_dsp_new(sr);
      s.ott[1] = ott_dsp_new(sr);
    }
  }
  ~Impl() {
    for (auto& s : slots) {
      for (auto* o : s.ott) ott_dsp_free(o);
    }
  }
  void resetDSP(Slot& s, int m) {
    if (m != insertBeads) s.beads = {};
    s.module = m;
    s.pending = m;
    s.dirty = true;
    switch (m) {
      case insertFormant:
        s.formant.reset(rate);
        break;
      case insertBeads:
        if (s.beads) s.beads->reset();
        break;
      case insertCompressor:
        s.work = WorkCompressor{};
        s.work.rate = rate;
        break;
      case insertDistortion:
        s.distortion.reset(rate);
        break;
      case insertDoubler:
        s.doubler.reset(rate);
        break;
      case insertTape:
        insert_tape::reset(&s.tape, rate);
        break;
      case insertOTT:
        for (auto* o : s.ott) ott_dsp_reset(o);
        break;
    }
    memset(s.delay, 0, sizeof(s.delay));
    memset(s.filter, 0, sizeof(s.filter));
    memset(s.allpassX, 0, sizeof(s.allpassX));
    memset(s.allpassY, 0, sizeof(s.allpassY));
    memset(s.phaserFeedback, 0, sizeof(s.phaserFeedback));
    memset(s.feedback, 0, sizeof(s.feedback));
    memset(s.held, 0, sizeof(s.held));
    for (auto& channel : s.eq) for (auto& band : channel) band.reset();
    s.eqNeutral = true;
    s.phase = 0;
    s.gateFrames = 0;
    s.gateLevel = 0;
    s.delayWrite = s.holdCounter = 0;
  }
  float readDelay(const Slot& s, int channel, float samples) const {
    samples = std::max(1.0f, std::min(samples, (float)delaySize - 2));
    int whole = (int)samples;
    float fraction = samples - whole;
    int index = s.delayWrite - whole;
    if (index < 0) index += delaySize;
    int previous = index ? index - 1 : delaySize - 1;
    return s.delay[channel][index] * (1 - fraction) + s.delay[channel][previous] * fraction;
  }
  float lowpass(Slot& s, int channel, float input) const {
    s.filter[channel] += s.toneCoefficient * (input - s.filter[channel]);
    return s.filter[channel];
  }
  static float coefficient(float cutoff, float sampleRate) {
    return 1.0f - expf(-6.28318530718f * cutoff / sampleRate);
  }
  static void setBiquad(Biquad& b, float b0, float b1, float b2, float a0, float a1, float a2) {
    b.b0 = b0 / a0; b.b1 = b1 / a0; b.b2 = b2 / a0;
    b.a1 = a1 / a0; b.a2 = a2 / a0;
  }
  static void setEQBand(Biquad& b, int type, float gain, float frequency, float rate) {
    const float A = powf(10.0f, gain / 40.0f);
    const float w = 6.28318530718f * std::min(frequency, rate * 0.45f) / rate;
    const float c = cosf(w), s = sinf(w);
    const float alpha = type == 1 ? s / (2.0f * 0.9f)
                                  : s * 0.5f * sqrtf((A + 1.0f / A) * 2.0f);
    const float beta = 2.0f * sqrtf(A) * alpha;
    if (type == 0) {
      setBiquad(b, A * ((A + 1) - (A - 1) * c + beta),
                2 * A * ((A - 1) - (A + 1) * c),
                A * ((A + 1) - (A - 1) * c - beta),
                (A + 1) + (A - 1) * c + beta,
                -2 * ((A - 1) + (A + 1) * c),
                (A + 1) + (A - 1) * c - beta);
    } else if (type == 1) {
      setBiquad(b, 1 + alpha * A, -2 * c, 1 - alpha * A,
                1 + alpha / A, -2 * c, 1 - alpha / A);
    } else {
      setBiquad(b, A * ((A + 1) + (A - 1) * c + beta),
                -2 * A * ((A - 1) + (A + 1) * c),
                A * ((A + 1) + (A - 1) * c - beta),
                (A + 1) - (A - 1) * c + beta,
                2 * ((A - 1) - (A + 1) * c),
                (A + 1) - (A - 1) * c - beta);
    }
  }
  static float softClip(float x) {
    float magnitude = fabsf(x);
    if (magnitude <= 1.0f) return x;
    if (magnitude >= 2.0f) return copysignf(1.5f, x);
    magnitude -= 1.0f;
    return copysignf(1.0f + magnitude - 0.5f * magnitude * magnitude, x);
  }
  void parameters(Slot& s, const uint8_t* v) {
    if (!s.dirty && !memcmp(v, s.previous, 8)) return;
    float p[8];
    for (int i = 0; i < 8; ++i) p[i] = insertMap(s.module, i, v[i]);
    switch (s.module) {
      case insertFormant:
        s.formant.set(v);
        break;
      case insertCompressor:
        s.work.set(p);
        break;
      case insertDistortion:
        s.distortion.A = p[0] / 24 + 0.5f;
        s.distortion.B = p[1] / 4;
        s.distortion.C = p[2] / 24 + 0.5f;
        s.distortion.D = p[3];
        break;
      case insertDoubler:
        s.doubler.A = p[0];
        s.doubler.B = p[1];
        break;
      case insertTape:
        p[4] = 0.5f + 0.5f * p[4];
        insert_tape::set(&s.tape, p);
        break;
      case insertOTT: {
        auto q = ott_dsp_default_params();
        q.depth = p[0];
        q.time_scale = p[1];
        q.upward = p[2];
        q.downward = p[3];
        q.input_gain = p[4];
        q.output_gain = p[5];
        for (auto* o : s.ott) ott_dsp_set_params(o, &q);
        break;
      }
      case insertChorus:
      case insertSaturation:
        s.toneCoefficient = coefficient(p[s.module == insertChorus ? 2 : 1], rate);
        break;
      case insertDestruction:
        s.toneCoefficient = coefficient(p[2], rate);
        break;
      case insertBitcrusher:
        s.toneCoefficient = coefficient(p[2], rate);
        break;
      case insertParametricEQ: {
        bool neutral = v[0] == 128 && v[2] == 128 && v[4] == 128;
        if (neutral) {
          if (!s.eqNeutral)
            for (auto& channel : s.eq) for (auto& band : channel) band.reset();
          s.eqNeutral = true;
          break;
        }
        if (s.eqNeutral)
          for (auto& channel : s.eq) for (auto& band : channel) band.reset();
        s.eqNeutral = false;
        for (int channel = 0; channel < 2; ++channel) {
          setEQBand(s.eq[channel][0], 0, p[0], p[1], rate);
          setEQBand(s.eq[channel][1], 1, p[2], p[3], rate);
          setEQBand(s.eq[channel][2], 2, p[4], p[5], rate);
        }
        break;
      }
    }
    memcpy(s.previous, v, 8);
    s.dirty = false;
  }
  void process(Slot& s, float* data, int frames, const uint8_t* values, const float* sidechain) {
    float increment = 1.0f / std::max(1.0f, rate * 0.005f);
    for (int offset = 0; offset < frames;) {
      if (s.switching && s.wet <= 0) {
        resetDSP(s, s.pending);
        s.switching = false;
      }
      float target = s.switching || s.config.bypass || s.module == insertOff ? 0 : 1;
      if (s.module == insertOff || (target == 0 && s.wet == 0)) {
        offset = frames;
        continue;
      }
      int n = std::min(block, frames - offset);
      if (s.switching) n = std::min(n, std::max(1, (int)ceilf(s.wet / increment)));
      float l[block], r[block];
      for (int i = 0; i < n; ++i) {
        l[i] = data[2 * (offset + i)];
        r[i] = data[2 * (offset + i) + 1];
        if (!std::isfinite(l[i])) l[i] = data[2 * (offset + i)] = 0;
        if (!std::isfinite(r[i])) r[i] = data[2 * (offset + i) + 1] = 0;
        l[i] = std::max(-32.0f, std::min(32.0f, l[i]));
        r[i] = std::max(-32.0f, std::min(32.0f, r[i]));
      }
      parameters(s, s.switching ? s.previous : values);
      float p[8];
      for (int i = 0; i < 8; ++i)
        p[i] = insertMap(s.module, i, s.switching ? s.previous[i] : values[i]);
      float* channels[] = {l, r};
      switch (s.module) {
        case insertFormant:
          s.formant.process(l, r, n);
          break;
        case insertBeads:
          if (s.beads) s.beads->process(l, r, n, p);
          break;
        case insertCompressor:
          s.work.process(l, r, n, sidechain ? sidechain + offset : nullptr);
          break;
        case insertDistortion:
          s.distortion.processReplacing(channels, channels, n);
          break;
        case insertDoubler:
          s.doubler.processReplacing(channels, channels, n);
          break;
        case insertTape:
          insert_tape::process(&s.tape, l, r, n);
          break;
        case insertOTT:
          ott_dsp_process_stereo(s.ott[0], s.ott[1], l, r, l, r, n);
          break;
        case insertChorus:
        case insertFlanger:
        case insertRotary: {
          const float rateHz = p[0];
          const float depthMs = s.module == insertChorus ? p[1] :
                                s.module == insertFlanger ? p[1] : 1.5f + p[1] * 8.0f;
          const float feedback = s.module == insertFlanger ? p[2] :
                                 s.module == insertChorus ? p[4] : 0.0f;
          const float drive = s.module == insertRotary ? powf(10.0f, p[2] / 20.0f) : 1.0f;
          for (int i = 0; i < n; ++i) {
            float dryL = l[i], dryR = r[i];
            float left = dryL, right = dryR;
            if (s.module == insertRotary) {
              left = tanhf(left * drive) / drive;
              right = tanhf(right * drive) / drive;
            }
            float modL = sinf(s.phase), modR = sinf(s.phase + 3.14159265359f);
            float nominalBase = s.module == insertChorus ? 15.0f :
                                s.module == insertFlanger ? 2.5f : 9.0f;
            float base = std::max(nominalBase, depthMs + 1.0f);
            float dL = (base + modL * depthMs) * rate / 1000.0f;
            float dR = (base + modR * depthMs) * rate / 1000.0f;
            s.delay[0][s.delayWrite] = left + s.feedback[0] * feedback;
            s.delay[1][s.delayWrite] = right + s.feedback[1] * feedback;
            float wetL = readDelay(s, 0, dL);
            float wetR = readDelay(s, 1, dR);
            s.feedback[0] = wetL;
            s.feedback[1] = wetR;
            if (s.module == insertChorus) {
              wetL = lowpass(s, 0, wetL);
              wetR = lowpass(s, 1, wetR);
            } else if (s.module == insertRotary) {
              float pan = sinf(s.phase) * p[1] * 0.5f;
              wetL *= 1.0f - pan;
              wetR *= 1.0f + pan;
            }
            l[i] = dryL + p[3] * (wetL - dryL);
            r[i] = dryR + p[3] * (wetR - dryR);
            s.phase += 6.28318530718f * rateHz / rate;
            if (s.phase >= 6.28318530718f) s.phase -= 6.28318530718f;
            if (++s.delayWrite == delaySize) s.delayWrite = 0;
          }
          break;
        }
        case insertPhaser: {
          static const float stageScale[8] = {0.45f, 0.65f, 0.85f, 1.05f,
                                              1.3f, 1.6f, 1.95f, 2.35f};
          int stages = 4 + (int)p[4];
          for (int i = 0; i < n; ++i) {
            float dryL = l[i], dryR = r[i];
            for (int ch = 0; ch < 2; ++ch) {
              float x = (ch ? r[i] : l[i]) + s.phaserFeedback[ch] * p[2];
              float sidePhase = s.phase + (ch ? 3.14159265359f : 0.0f);
              for (int stage = 0; stage < stages; ++stage) {
                float sweep = 0.5f + 0.5f * sinf(sidePhase + stage * 0.7f);
                float cutoff = 80.0f + sweep * p[1] * 10000.0f * stageScale[stage];
                cutoff = std::min(cutoff, rate * 0.45f);
                float g = tanf((float)M_PI * cutoff / rate);
                float a = (1.0f - g) / (1.0f + g);
                float y = a * x + s.allpassX[ch][stage] - a * s.allpassY[ch][stage];
                s.allpassX[ch][stage] = x;
                s.allpassY[ch][stage] = y;
                x = y;
              }
              s.phaserFeedback[ch] = x;
              if (ch) r[i] = x;
              else l[i] = x;
            }
            s.phase += 6.28318530718f * p[0] / rate;
            if (s.phase >= 6.28318530718f) s.phase -= 6.28318530718f;
            l[i] = dryL + p[3] * (l[i] - dryL);
            r[i] = dryR + p[3] * (r[i] - dryR);
          }
          break;
        }
        case insertSaturation: {
          float drive = powf(10.0f, p[0] / 20.0f);
          float level = powf(10.0f, p[2] / 20.0f);
          for (int i = 0; i < n; ++i) {
            float dryL = l[i], dryR = r[i];
            float wetL = lowpass(s, 0, tanhf(dryL * drive) / drive) * level;
            float wetR = lowpass(s, 1, tanhf(dryR * drive) / drive) * level;
            l[i] = dryL + p[3] * (wetL - dryL);
            r[i] = dryR + p[3] * (wetR - dryR);
          }
          break;
        }
        case insertBitcrusher: {
          int hold = (int)p[1] + 1;
          float levels = (float)((1 << ((int)p[0] + 4)) - 1);
          for (int i = 0; i < n; ++i) {
            float dryL = l[i], dryR = r[i];
            if (s.holdCounter == 0) {
              s.held[0] = roundf(std::max(-1.0f, std::min(1.0f, l[i])) * levels) / levels;
              s.held[1] = roundf(std::max(-1.0f, std::min(1.0f, r[i])) * levels) / levels;
              s.holdCounter = hold;
            }
            float wetL = lowpass(s, 0, s.held[0]);
            float wetR = lowpass(s, 1, s.held[1]);
            l[i] = dryL + p[3] * (wetL - dryL);
            r[i] = dryR + p[3] * (wetR - dryR);
            --s.holdCounter;
          }
          break;
        }
        case insertDestruction: {
          int mode = (int)p[0];
          float amount = p[1];
          float drive = 1.0f + amount * 15.0f;
          float levels = (float)((1 << (16 - (int)roundf(amount * 12.0f))) - 1);
          for (int i = 0; i < n; ++i) {
            float dry[2] = {l[i], r[i]};
            for (int ch = 0; ch < 2; ++ch) {
              float x = dry[ch];
              if (mode == 0) {
                x *= drive;
                x = 1.0f - fabsf(fmodf(x + 1.0f, 4.0f) - 2.0f);
              } else if (mode == 1) {
                x = tanhf(x * drive) / drive;
              } else {
                x = roundf(std::max(-1.0f, std::min(1.0f, x)) * levels) / levels;
              }
              x = lowpass(s, ch, x);
              if (ch) r[i] = dry[ch] + p[3] * (x - dry[ch]);
              else l[i] = dry[ch] + p[3] * (x - dry[ch]);
            }
          }
          break;
        }
        case insertParametricEQ:
          if (!s.eqNeutral) for (int i = 0; i < n; ++i) {
            l[i] = softClip(s.eq[0][2].process(s.eq[0][1].process(s.eq[0][0].process(l[i]))));
            r[i] = softClip(s.eq[1][2].process(s.eq[1][1].process(s.eq[1][0].process(r[i]))));
          }
          break;
        case insertTranceGate: {
          // 24 tracker ticks make a quarter note; Rate selects a note value,
          // then the triplet/dotted variants scale it by 2/3 or 3/2.
          const int rateIndex = values[1];
          float ticks = 48.0f / (1 << (rateIndex / 3));
          if (rateIndex % 3 == 1) ticks *= 2.0f / 3.0f;
          if (rateIndex % 3 == 2) ticks *= 1.5f;
          const double stepFrames = std::max(1.0, (double)rate * ticks / std::max(1.0f, tickRate));
          const float gate = values[3] / 255.0f;
          const float smoothSeconds = 0.0005f + 0.0300f * p[6] * p[6];
          const float smooth = 1.0f - expf(-1.0f / (rate * smoothSeconds));
          const float mix = p[7];
          for (int i = 0; i < n; ++i) {
            uint64_t step = (uint64_t)(s.gateFrames / stepFrames);
            double position = fmod(s.gateFrames, stepFrames) / stepFrames;
            float targetGate = tranceGateOpen(values, step) && position < gate ? 1.0f : 0.0f;
            s.gateLevel += smooth * (targetGate - s.gateLevel);
            float gain = 1.0f + mix * (s.gateLevel - 1.0f);
            l[i] *= gain;
            r[i] *= gain;
            s.gateFrames += 1.0;
          }
          break;
        }
      }
      for (int i = 0; i < n; ++i) {
        s.wet = target > s.wet ? std::min(target, s.wet + increment)
                               : std::max(target, s.wet - increment);
        for (int c = 0; c < 2; ++c) {
          float v = c ? r[i] : l[i];
          if (!std::isfinite(v)) v = 0;
          v = std::max(-32.0f, std::min(32.0f, v));
          float& dry = data[2 * (offset + i) + c];
          dry += s.wet * (v - dry);
        }
      }
      offset += n;
    }
  }
};
InsertChain::InsertChain(float rate) : impl_(new (std::nothrow) Impl(rate)) {}
InsertChain::~InsertChain() { delete impl_; }
bool InsertChain::ready() const {
  return impl_ && impl_->slots[0].ott[0] && impl_->slots[0].ott[1] && impl_->slots[1].ott[0] &&
         impl_->slots[1].ott[1];
}
void InsertChain::attachBeads(int slot, const BeadsPool::Handle& beads) {
  if (impl_ && slot >= 0 && slot < 2) impl_->slots[slot].beads = beads;
}
void InsertChain::reset() {
  if (!ready()) return;
  for (auto& s : impl_->slots) {
    impl_->resetDSP(s, s.config.module);
    s.wet = 0;
    s.switching = false;
  }
}
void InsertChain::setTempo(float tickRate) {
  if (impl_) impl_->tickRate = std::max(1.0f, tickRate);
}
void InsertChain::trigger() {
  if (!ready()) return;
  for (auto& s : impl_->slots) {
    s.gateFrames = 0;
    s.gateLevel = 0;
  }
}
uint16_t InsertChain::sync(const InsertConfig c[2], InsertAutomation* a, bool rendered) {
  if (!ready()) return 0;
  uint16_t cleared = 0;
  for (int slot = 0; slot < 2; ++slot) {
    auto& s = impl_->slots[slot];
    // Muted/hidden tracks do not run process(), so a removed instance cannot
    // wait for a fade there. Keep selected/bypassed Beads and its history intact.
    if (c[slot].module != insertBeads && (!rendered || s.wet == 0)) s.beads = {};
    if (c[slot].module != s.config.module || c[slot].selection != s.config.selection) {
      a->valid[slot] = 0;
      cleared |= 0xffu << (slot * 8);
      s.pending = c[slot].module;
      s.switching = true;
    }
    for (int i = 0; i < 8; ++i)
      if (c[slot].values[i] != s.config.values[i] || c[slot].edits[i] != s.config.edits[i]) {
        a->valid[slot] &= ~(1 << i);
        cleared |= 1u << (slot * 8 + i);
      }
    s.config = c[slot];
  }
  return cleared;
}
bool InsertChain::active() const {
  if (!ready()) return false;
  for (auto& s : impl_->slots)
    if (s.config.module || s.module || s.switching) return true;
  return false;
}
void InsertChain::process(float* data, int frames, const uint8_t v[2][8], const float* sidechain[2]) {
  if (!ready()) return;
  for (int slot = 0; slot < 2; ++slot)
    impl_->process(impl_->slots[slot], data, frames, v[slot], sidechain ? sidechain[slot] : nullptr);
}
