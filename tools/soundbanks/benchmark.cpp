// Offline throughput measurement, not a live SDL audio-underrun test.
#include <sys/resource.h>

#include <chrono>
#include <ctime>
#include <iostream>
#include <vector>

#include "soundbank_index.h"
#include "synth/soundbank_voice.h"
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  try {
    using Clock = std::chrono::steady_clock;
    auto t = Clock::now();
    auto bank = soundbank::parseBank({argv[1], ""});
    double parseMs = std::chrono::duration<double, std::milli>(Clock::now() - t).count();
    soundbank::SampleCache cache(64 * 1024 * 1024);
    t = Clock::now();
    auto p = cache.prepare(bank, bank.presets.front().id);
    double loadMs = std::chrono::duration<double, std::milli>(Clock::now() - t).count();
    auto handle = soundbank::publish(p);
    for (int count : {1, 8, 16, 32}) {
      std::vector<SoundbankVoice> voices(count);
      for (auto& v : voices) {
        v.init(48000);
        v.configure(handle, 6000, 1, 127);
        v.noteOn();
      }
      float out[512];
      double energy = 0;
      auto cpu = std::clock();
      t = Clock::now();
      for (int block = 0; block < 938; ++block)
        for (auto& v : voices) {
          v.render(out, 256);
          for (float x : out) energy += double(x) * x;
        }
      double wall = std::chrono::duration<double>(Clock::now() - t).count();
      double cpuSec = double(std::clock() - cpu) / CLOCKS_PER_SEC;
      struct rusage usage {};
      getrusage(RUSAGE_SELF, &usage);
      long long rss = usage.ru_maxrss;
#ifndef __APPLE__
      rss *= 1024;
#endif
      std::cout << "{\"voices\":" << count
                << ",\"frames_per_voice\":240128,\"render_wall_seconds\":" << wall
                << ",\"render_cpu_seconds\":" << cpuSec << ",\"peak_rss_bytes\":" << rss
                << ",\"parse_ms\":" << parseMs << ",\"prepare_ms\":" << loadMs
                << ",\"pcm_bytes\":" << p->bytes << ",\"energy\":" << energy << "}\n";
    }
    soundbank::collect([](uint32_t) { return false; });
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
