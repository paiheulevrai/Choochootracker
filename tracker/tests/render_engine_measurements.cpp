#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <vector>

#include "synth/braids_voice.h"
#include "synth/plaits_alt_voice.h"
#include "synth/plaits_voice.h"

namespace fs = std::filesystem;

namespace {
constexpr int kSampleRate = 96000;
constexpr size_t kFrames = kSampleRate;  // one second; first/last 100 ms are excluded by the analyser.
constexpr size_t kDrumFrames = kSampleRate / 4;
constexpr uint16_t kHarmonics[] = {2048, 7168, 12288, 17408, 22528};
constexpr uint16_t kTimbres[] = {4096, 12288, 20480};
constexpr uint16_t kMorphs[] = {6144, 16384};
constexpr uint16_t kBraidsTimbres[] = {2048, 8192, 16384, 24576, 30720};
constexpr uint16_t kBraidsColors[] = {2048, 8192, 14336, 20480, 26624, 30720};
constexpr float kNotes[] = {48.0f, 60.0f};
constexpr float kPi = 3.14159265358979323846f;

struct MacroPatch { uint16_t harmonics, timbre, morph; };

MacroPatch macroPatch(size_t index) {
  return {kHarmonics[index / 6], kTimbres[(index / 2) % 3], kMorphs[index % 2]};
}

constexpr size_t kPatchCount = std::size(kHarmonics) * std::size(kTimbres) * std::size(kMorphs);

bool writeWav(const fs::path& path, const std::vector<float>& data) {
  FILE* file = std::fopen(path.string().c_str(), "wb");
  if (!file) return false;
  const uint32_t dataSize = static_cast<uint32_t>(data.size() * sizeof(int16_t));
  const uint32_t riffSize = 36 + dataSize;
  const uint16_t channels = 1, bits = 16, blockAlign = 2;
  const uint32_t byteRate = kSampleRate * blockAlign;
  auto u16 = [&](uint16_t value) { std::fwrite(&value, sizeof(value), 1, file); };
  auto u32 = [&](uint32_t value) { std::fwrite(&value, sizeof(value), 1, file); };
  std::fwrite("RIFF", 1, 4, file); u32(riffSize); std::fwrite("WAVEfmt ", 1, 8, file);
  u32(16); u16(1); u16(channels); u32(kSampleRate); u32(byteRate); u16(blockAlign); u16(bits);
  std::fwrite("data", 1, 4, file); u32(dataSize);
  for (float sample : data) {
    sample = std::isfinite(sample) ? std::clamp(sample, -1.0f, 1.0f) : 0.0f;
    const int16_t pcm = static_cast<int16_t>(std::lrintf(sample * 32767.0f));
    std::fwrite(&pcm, sizeof(pcm), 1, file);
  }
  return std::fclose(file) == 0;
}

template <typename Voice>
bool renderPlaitsFamily(const char* family, bool vcaValidation = false) {
  const fs::path directory = fs::path("measurements") / family;
  fs::create_directories(directory);
  std::vector<float> buffer(kFrames);
  for (int engine = 0; engine < 24; ++engine) {
    for (size_t patch = 0; patch < kPatchCount; ++patch) {
      for (size_t note = 0; note < std::size(kNotes); ++note) {
        const MacroPatch settings = macroPatch(patch);
        const bool percussion = engine >= 21;
        const bool vca = vcaValidation && !percussion;
        const size_t frames = percussion ? kDrumFrames : kFrames;
        Voice voice;
        voice.init(kSampleRate);
        // Native TRIG/LPG is the calibration baseline. VCA is a separate
        // diagnostic because internally articulated engines can be silenced by it.
        voice.configure(engine, settings.harmonics, settings.timbre, settings.morph,
                        0, vca ? 2 : 0, vca ? 128 : 255, 255, kNotes[note], 0.9f);
        if (vca) voice.setEnvelope(0.005f, 0.0f, 1.0f, 0.005f);
        voice.noteOn();
        voice.render(buffer.data(), frames);
        char name[80];
        std::snprintf(name, sizeof(name), "%s_%02d_p%zu_n%.0f_%s.wav", family, engine, patch,
                      kNotes[note], vca ? "vca" : "trig");
        if (!writeWav(directory / name, std::vector<float>(buffer.begin(), buffer.begin() + frames))) return false;
      }
    }
  }
  return true;
}

bool renderBraids() {
  const fs::path directory = fs::path("measurements") / "braids";
  fs::create_directories(directory);
  std::vector<float> buffer(kFrames);
  for (int model = 0; model <= braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META; ++model) {
    for (size_t patch = 0; patch < kPatchCount; ++patch) {
      for (size_t note = 0; note < std::size(kNotes); ++note) {
        BraidsVoice voice;
        voice.init();
        if (!voice.setModel(model)) return false;
        voice.setPitch(static_cast<int16_t>(kNotes[note] * 128));
        voice.setParameters(kBraidsTimbres[patch % std::size(kBraidsTimbres)],
                            kBraidsColors[patch / std::size(kBraidsTimbres)]);
        voice.setGain(0.9f);
        voice.setEnvelope(true, 0.005f, 0.0f, 1.0f, 0.005f);
        voice.noteOn();
        voice.render(buffer.data(), buffer.size());
        char name[80];
        std::snprintf(name, sizeof(name), "braids_%02d_p%zu_n%.0f.wav", model, patch, kNotes[note]);
        if (!writeWav(directory / name, buffer)) return false;
      }
    }
  }
  return true;
}

bool renderPcmReference() {
  const fs::path directory = fs::path("measurements") / "pcm";
  fs::create_directories(directory);
  for (float note : kNotes) {
    std::vector<float> buffer(kFrames);
    const float frequency = 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
    for (size_t i = 0; i < buffer.size(); ++i) buffer[i] = 0.9f * std::sin(2.0f * kPi * frequency * i / kSampleRate);
    char name[48];
    std::snprintf(name, sizeof(name), "pcm_sine_n%.0f.wav", note);
    if (!writeWav(directory / name, buffer)) return false;
  }
  return true;
}
}  // namespace

int main(int argc, char** argv) {
  const char* family = argc == 2 ? argv[1] : "all";
  if (argc > 2 || (std::strcmp(family, "all") && std::strcmp(family, "braids") &&
                   std::strcmp(family, "plaits") && std::strcmp(family, "plaits-alt") &&
                   std::strcmp(family, "plaits-vca") && std::strcmp(family, "plaits-alt-vca") &&
                   std::strcmp(family, "pcm"))) {
    std::fputs("Usage: render_engine_measurements [all|braids|plaits|plaits-alt|pcm]\n", stderr);
    return 2;
  }
  std::printf("Rendering %s measurement WAVs...\n", family);
  const bool all = !std::strcmp(family, "all");
  if ((all && (!renderBraids() || !renderPlaitsFamily<PlaitsVoice>("plaits") ||
               !renderPlaitsFamily<PlaitsAltVoice>("plaits-alt") || !renderPcmReference())) ||
      (!all && !std::strcmp(family, "braids") && !renderBraids()) ||
      (!all && !std::strcmp(family, "plaits") && !renderPlaitsFamily<PlaitsVoice>("plaits")) ||
      (!all && !std::strcmp(family, "plaits-alt") && !renderPlaitsFamily<PlaitsAltVoice>("plaits-alt")) ||
      (!all && !std::strcmp(family, "plaits-vca") && !renderPlaitsFamily<PlaitsVoice>("plaits-vca", true)) ||
      (!all && !std::strcmp(family, "plaits-alt-vca") && !renderPlaitsFamily<PlaitsAltVoice>("plaits-alt-vca", true)) ||
      (!all && !std::strcmp(family, "pcm") && !renderPcmReference())) {
    std::fputs("Could not write measurement WAVs.\n", stderr);
    return 1;
  }
  std::puts("Done: measurements/");
  return 0;
}
