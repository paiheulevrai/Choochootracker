// Standalone SID preparation probe; never linked into ChooChooTracker.
// Project-authored MIT code. The unmodified emulator retains its zlib notice.
#include <cstdint>
#define CHIPS_IMPL
#include "vendor/m6581.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

constexpr int clockHz = 985248, sampleHz = 48000;
static void writeReg(m6581_t& sid, int address, int value) {
  m6581_tick(&sid, M6581_CS | uint64_t(address) | (uint64_t(value) << 16));
}
static void init(m6581_t& sid) {
  m6581_desc_t desc{clockHz, sampleHz, 0.25f};
  m6581_init(&sid, &desc);
}
static double micros() {
  return std::chrono::duration<double, std::micro>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
static void word(std::ofstream& f, uint32_t value, int bytes) {
  for (int i = 0; i < bytes; ++i) f.put(char(value >> (8 * i)));
}
int main(int argc, char** argv) {
  if (argc == 1) {
    // All three oscillators and the shared filter run in each instance.
    // This is a CPU probe, without SDL, tracker UI, FX, mixing or ALSA.
    for (int count : {1, 2, 4, 8}) {
      std::vector<m6581_t> chips(count);
      for (auto& sid : chips) {
        init(sid);
        for (int voice = 0; voice < 3; ++voice) {
          writeReg(sid, voice * 7, 90 + voice * 30);
          writeReg(sid, voice * 7 + 1, 20 + voice * 3);
          writeReg(sid, voice * 7 + 3, 8);
          writeReg(sid, voice * 7 + 5, 0x08);
          writeReg(sid, voice * 7 + 6, 0xa5);
          writeReg(sid, voice * 7 + 4, 0x41);
        }
        writeReg(sid, 0x16, 100); writeReg(sid, 0x17, 0x87);
        writeReg(sid, 0x18, 0x1f);
      }
      std::vector<double> times;
      double energy = 0; float peak = 0;
      constexpr int blocks = 100, blockCycles = clockHz * 1024 / sampleHz;
      for (int b = 0; b < blocks; ++b) {
        const double start = micros();
        for (auto& sid : chips) for (int c = 0; c < blockCycles; ++c) {
          if (m6581_tick(&sid, 0) & M6581_SAMPLE) {
            if (!std::isfinite(sid.sample)) return 2;
            energy += double(sid.sample) * sid.sample;
            peak = std::max(peak, std::abs(sid.sample));
          }
        }
        times.push_back(micros() - start);
      }
      double total = 0; for (double t : times) total += t;
      std::sort(times.begin(), times.end());
      std::printf("instances=%d state_bytes=%zu emulated_seconds=%.6f cpu_percent=%.3f p95_us=%.3f worst_us=%.3f energy=%.9f peak=%.6f\n",
          count, sizeof(m6581_t) * count, double(blockCycles) * blocks / clockHz,
          total / (double(blockCycles) * blocks / clockHz * 1e4),
          times[95], times.back(), energy, peak);
      if (energy <= 0) return 3;
    }
    return 0;
  }
  if (argc != 4) {
    std::fprintf(stderr, "Usage: probe [register-events.txt output.wav seconds]\n");
    return 1;
  }
  const int seconds = std::atoi(argv[3]);
  if (seconds < 1 || seconds > 30) return 1;
  struct Event { int frame, address, value; };
  std::ifstream input(argv[1]); std::vector<Event> events; Event e;
  while (input >> e.frame >> e.address >> e.value) {
    if (e.frame < 0 || e.address < 0 || e.address > 24 || e.value < 0 || e.value > 255 ||
        (!events.empty() && e.frame < events.back().frame)) return 1;
    events.push_back(e);
    if (events.size() > 100000) return 1;
  }
  if (!input.eof() || events.empty()) return 1;
  m6581_t sid; init(sid); size_t event = 0;
  std::vector<int16_t> audio; audio.reserve(seconds * sampleHz);
  double energy = 0; float peak = 0;
  // Frames here are an explicit 50 Hz macro clock, not tracker song tempo.
  for (int cycle = 0; cycle < seconds * clockHz; ++cycle) {
    const int frame = int(int64_t(cycle) * 50 / clockHz);
    uint64_t pins = 0;
    // One write per chip clock preserves SID write ordering.
    if (event < events.size() && events[event].frame <= frame) {
      const auto& next = events[event++];
      pins = M6581_CS | uint64_t(next.address) | (uint64_t(next.value) << 16);
    }
    if (m6581_tick(&sid, pins) & M6581_SAMPLE) {
      if (!std::isfinite(sid.sample)) return 2;
      energy += double(sid.sample) * sid.sample;
      peak = std::max(peak, std::abs(sid.sample));
      audio.push_back(int16_t(std::clamp(sid.sample, -1.f, 1.f) * 32767.f));
    }
  }
  if (event != events.size() || energy <= 0 || peak >= 1) return 3;
  std::ofstream out(argv[2], std::ios::binary);
  out.write("RIFF", 4); word(out, 36 + audio.size() * 2, 4);
  out.write("WAVEfmt ", 8); word(out, 16, 4); word(out, 1, 2); word(out, 1, 2);
  word(out, sampleHz, 4); word(out, sampleHz * 2, 4); word(out, 2, 2); word(out, 16, 2);
  out.write("data", 4); word(out, audio.size() * 2, 4);
  for (int16_t sample : audio) word(out, uint16_t(sample), 2);
  std::printf("samples=%zu energy=%.9f peak=%.6f\n", audio.size(), energy, peak);
  return out ? 0 : 4;
}
