#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <set>

#include "soundbank_index.h"
#include "synth/soundbank_voice.h"
using namespace soundbank;
static std::string json(const std::string& s) {
  std::string out = "\"";
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n')
      out += "\\n";
    else if (c == '\r')
      out += "\\r";
    else if (c == '\t')
      out += "\\t";
    else if (c >= 32)
      out += c;
  }
  return out + '"';
}
int main(int argc, char** argv) {
  if (argc < 2) return 2;
  bool metadata = false, instrumentsOnly = false;
  size_t cacheBytes = 256 * 1024 * 1024;
  std::string root;
  for (int n = 2; n < argc; ++n) {
    std::string option = argv[n];
    if (option == "--metadata")
      metadata = true;
    else if (option == "--instruments")
      instrumentsOnly = true;
    else if (option.rfind("--root=", 0) == 0)
      root = option.substr(7);
    else if (option.rfind("--cache-mb=", 0) == 0) {
      int mb = std::atoi(option.c_str() + 11);
      if (mb < 8 || mb > 1024) return 2;
      cacheBytes = size_t(mb) * 1024 * 1024;
    } else
      return 2;
  }
  if (std::filesystem::is_directory(argv[1])) {
    try {
      auto start = std::chrono::steady_clock::now();
      auto entries = BankIndex(argv[1]).scan();
      size_t presets = 0, errors = 0, warnings = 0;
      for (const auto& e : entries) {
        presets += e.presets.size();
        errors += !e.error.empty();
        warnings += !e.warnings.empty();
      }
      std::cout << "{\"banks\":" << entries.size() << ",\"presets\":" << presets
                << ",\"bank_errors\":" << errors << ",\"banks_with_warnings\":" << warnings
                << ",\"index_ms\":"
                << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                             start)
                       .count()
                << "}\n";
      return 0;
    } catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
      return 1;
    }
  }
  std::vector<Location> sources;
  try {
    std::string path = argv[1];
    auto member = path.find("::");
    if(member != std::string::npos) {
      sources.push_back({path.substr(0,member),path.substr(member+2)});
    } else if (extension(path) == ".zip") {
      Archive zip(path);
      for (auto& e : zip.entries()) {
        if (isPlatformMetadata(e.name)) continue;
        auto x = extension(e.name);
        if (instrumentsOnly && (x == ".wav" || x == ".aif" || x == ".aiff")) continue;
        if (x == ".sf2" || x == ".sf3" || x == ".sfz" || x == ".exs" || x == ".wav" ||
            x == ".aif" || x == ".aiff")
          sources.push_back({path, e.name});
      }
    } else
      sources.push_back({path, ""});
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  for (auto& source : sources) {
    source.root = root;
    auto start = std::chrono::steady_clock::now();
    try {
      auto b = parseBank(source);
      auto parsed = std::chrono::steady_clock::now();
      for (auto& p : b.presets) {
        std::string error;
        std::vector<std::string> preparedWarnings;
        size_t bytes = 0;
        double energy = 0, loadMs = 0;
        bool loaded = false;
        auto prepareStart = std::chrono::steady_clock::now();
        try {
          if (!metadata) {
            SampleCache cache(cacheBytes);
            auto preparedPreset = cache.prepare(b, p.id);
            bytes = preparedPreset->bytes;
            preparedWarnings = preparedPreset->warnings;
            auto handle = publish(preparedPreset);
            SoundbankVoice voice;
            voice.init(48000);
            auto z = preparedPreset->zones.front().zone;
            voice.configure(handle, 100.f * z.root, 1, std::max(z.loVel, std::min(z.hiVel, 100)));
            voice.noteOn();
            float output[2048];
            voice.render(output, 1024);
            for (float f : output) {
              require(std::isfinite(f), "Nonfinite rendered sample");
              energy += double(f) * f;
            }
            voice.kill();
            collect([](uint32_t) { return false; });
            loaded = true;
            loadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                               prepareStart)
                         .count();
          }
        } catch (const std::exception& e) {
          error = e.what();
        }
        std::cout << "{\"source\":" << json(source.id()) << ",\"preset\":" << json(p.id)
                  << ",\"name\":" << json(p.name) << ",\"format\":" << json(b.format)
                  << ",\"zones\":" << p.zones.size()
                  << ",\"loaded\":" << (loaded ? "true" : "false") << ",\"bytes\":" << bytes
                  << ",\"energy\":" << energy << ",\"parse_ms\":"
                  << std::chrono::duration<double, std::milli>(parsed - start).count()
                  << ",\"prepare_render_ms\":" << loadMs << ",\"error\":" << json(error)
                  << ",\"warnings\":[";
        bool first = true;
        for (auto list : {b.warnings, p.warnings, preparedWarnings})
          for (auto& w : list) {
            if (!first) std::cout << ',';
            first = false;
            std::cout << json(w);
          }
        std::cout << "]}\n" << std::flush;
      }
    } catch (const std::exception& e) {
      std::cout << "{\"source\":" << json(source.id()) << ",\"error\":" << json(e.what())
                << ",\"loaded\":false}\n"
                << std::flush;
    }
  }
}
