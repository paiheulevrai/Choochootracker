#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>

#include "chipnomad_lib.h"
#include "audio_monitor.h"
#include "playback.h"
#include "project.h"
#include "screens/screens.h"
#include "output_safety_limiter.h"
#include "audio_input_manager.h"

namespace {
constexpr int kBlockFrames = 1024;
constexpr int kWarmupSeconds = 3;
#ifdef CCT_BENCHMARK_SECONDS
constexpr int kMeasuredSeconds = CCT_BENCHMARK_SECONDS;
#else
constexpr int kMeasuredSeconds = 20;
#endif
constexpr int kRounds = 5;

class LegacyOutputLimiter {
 public:
  void reset() {
    gain_ = 1.0f;
    previousInput_[0] = previousInput_[1] = 0.0f;
    previousOutput_[0] = previousOutput_[1] = 0.0f;
  }

  void process(const float* mix, int16_t* output, float* scratch, int frames, int sampleRate) {
    const int count = frames * 2;
    for (int i = 0; i < count; ++i) {
      const float sample = mix[i];
      output[i] = !std::isfinite(sample) ? 0 :
        (int16_t)(sample >= 1.0f ? 32767 : sample <= -1.0f ? -32768 : sample * 32767.0f);
    }
    const float dcDecay = std::exp(-94.24778f / sampleRate);
    double power = 0.0;
    float peak = 0.0f;
    for (int i = 0; i < count; ++i) {
      const float sample = output[i] / 32768.0f;
      const int channel = i & 1;
      const float filtered = sample - previousInput_[channel] + dcDecay * previousOutput_[channel];
      previousInput_[channel] = sample;
      previousOutput_[channel] = filtered;
      scratch[i] = filtered;
      power += filtered * filtered;
      peak = std::max(peak, std::fabs(filtered));
    }
    const float rms = std::sqrt(power / count);
    float target = 1.0f;
    if (peak > 0.8f) target = 0.8f / peak;
    if (rms > 0.35f) target = std::min(target, 0.35f / rms);
    gain_ = std::min(target, gain_ + frames / (0.25f * sampleRate));
    for (int i = 0; i < count; ++i)
      output[i] = static_cast<int16_t>(scratch[i] * gain_ * 32767.0f);
  }

 private:
  float gain_ = 1.0f;
  float previousInput_[2] = {};
  float previousOutput_[2] = {};
};

void configureVariant(Project* project, const char* variant) {
  if (!std::strcmp(variant, "no-effects")) {
    std::fill(project->trackReverbSend,
              project->trackReverbSend + PROJECT_MAX_TRACKS, 0);
    std::fill(project->trackDelaySend,
              project->trackDelaySend + PROJECT_MAX_TRACKS, 0);
    project->delayReverbSend = 0;
  } else if (!std::strcmp(variant, "no-tilt")) {
    std::fill(project->trackTilt,
              project->trackTilt + PROJECT_MAX_TRACKS, 0x80);
  } else if (!std::strcmp(variant, "no-drive")) {
    for (int i = 0; i < PROJECT_MAX_INSTRUMENTS; ++i) {
      Instrument& instrument = project->instruments[i];
      uint8_t* character = nullptr;
      if (instrument.type == InstrumentType::Braids)
        character = &instrument.chip.braids.filterCharacter;
      else if (instrument.type == InstrumentType::Sample)
        character = &instrument.chip.sample.filterCharacter;
      else if (instrument.type == InstrumentType::Plaits ||
               instrument.type == InstrumentType::PlaitsAlt)
        character = &instrument.chip.plaits.filterCharacter;
      else if (instrument.type == InstrumentType::SCWF)
        character = &instrument.chip.scwf.filterCharacter;
      else if (instrument.type == InstrumentType::BYOWTBL)
        character = &instrument.chip.byowtbl.filterCharacter;
      if (character && *character >= 3) *character = 1;
    }
  } else if (!std::strcmp(variant, "no-inserts")) {
    for (auto& inserts : project->trackInserts)
      for (auto& insert : inserts) insert.module = insertOff;
    for (auto& insert : project->masterInserts) insert.module = insertOff;
  }
}

double render(Project* project, int sampleRate, int seconds, const char* variant,
              const char* pipeline, int startRow, volatile double* checksum) {
  ChipNomadState* state = chipnomadCreate();
  if (!state) return -1.0;
  state->ownsProjectResources = 0;
  state->project = *project;
  state->audioProject = *project;
  playbackInit(&state->playbackState, &state->audioProject);
  chipnomadInitChips(state, sampleRate, nullptr);
  chipnomadReserveRenderBuffers(state, kBlockFrames);
  state->benchmarkBypassPan = !std::strcmp(variant, "no-pan");
  if (!std::strcmp(variant, "no-monitor")) state->audioMonitor->setEnabled(false);
  playbackStartSong(&state->playbackState, startRow, 0, 1);

  std::vector<float> buffer(kBlockFrames * 2);
  std::vector<float> scratch(kBlockFrames * 2);
  std::vector<int16_t> output(kBlockFrames * 2);
  OutputSafetyLimiter limiter;
  LegacyOutputLimiter legacyLimiter;
  limiter.reset(sampleRate);
  legacyLimiter.reset();
  int remaining = seconds * sampleRate;
  const auto started = std::chrono::steady_clock::now();
  while (remaining > 0) {
    int frames = std::min(remaining, kBlockFrames);
    if (chipnomadRender(state, buffer.data(), frames) != frames) {
      chipnomadDestroy(state);
      return -1.0;
    }
    if (!std::strcmp(pipeline, "optimized"))
      limiter.process(buffer.data(), output.data(), frames);
    else if (!std::strcmp(pipeline, "legacy"))
      legacyLimiter.process(buffer.data(), output.data(), scratch.data(), frames, sampleRate);
    const int sample = (remaining / kBlockFrames) % (frames * 2);
    *checksum += !std::strcmp(pipeline, "none") ? buffer[sample] : output[sample];
    remaining -= frames;
  }
  const double elapsed = std::chrono::duration<double>(
    std::chrono::steady_clock::now() - started).count();
  chipnomadDestroy(state);
  return elapsed;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc > 1 && !std::strcmp(argv[1], "--audio-ring")) {
    AudioInputRing ring;
    int16_t input[kBlockFrames * 2], output[kBlockFrames * 2];
    for (int i = 0; i < kBlockFrames * 2; ++i) input[i] = (int16_t)(i * 31);
    constexpr int rounds = 20000;
    volatile int checksum = 0;
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < rounds; ++i) {
      if (!ring.push(input, kBlockFrames) || ring.pop(output, kBlockFrames) != kBlockFrames) return 1;
      checksum += output[i & (kBlockFrames * 2 - 1)];
    }
    const double elapsed = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - started).count();
    std::printf("audio ring: %.6f s, %.2f ns/frame, checksum %d\n", elapsed,
      elapsed * 1.0e9 / (rounds * kBlockFrames), (int)checksum);
    return 0;
  }
  const char* path = argc > 1 ? argv[1] : "packaging/common/projects/alf dance.cct";
  const int sampleRate = argc > 2 ? std::atoi(argv[2]) : 96000;
  const char* variant = argc > 3 ? argv[3] : "full";
  const char* pipeline = argc > 4 ? argv[4] : "optimized";
  const int startRow = argc > 5 ? (int)std::strtol(argv[5], nullptr, 0) : 0;
  if (sampleRate != 32000 && sampleRate != 44100 && sampleRate != 48000 && sampleRate != 96000) {
    std::fprintf(stderr, "Sample rate must be 32000, 44100, 48000 or 96000\n");
    return 1;
  }
  Project project;
  projectInit(&project);
  if (projectLoad(&project, path)) {
    std::fprintf(stderr, "Cannot load %s: %s\n", path, projectFileError);
    return 1;
  }
  const char* variants[] = {"full", "no-effects", "no-tilt", "no-drive", "no-inserts", "no-pan", "no-monitor"};
  bool knownVariant = false;
  for (const char* candidate : variants) knownVariant |= !std::strcmp(candidate, variant);
  if (!knownVariant) {
    std::fprintf(stderr, "Unknown variant: %s\n", variant);
    return 1;
  }
  if (std::strcmp(pipeline, "optimized") && std::strcmp(pipeline, "legacy") &&
      std::strcmp(pipeline, "none")) {
    std::fprintf(stderr, "Output pipeline must be optimized, legacy or none\n");
    return 1;
  }
  configureVariant(&project, variant);
  std::printf("variant: %s, output: %s, sample rate: %d, start row: %02X\n",
              variant, pipeline, sampleRate, startRow);

  volatile double checksum = 0.0;
  if (render(&project, sampleRate, kWarmupSeconds, variant, pipeline, startRow, &checksum) < 0.0)
    return 1;
  std::vector<double> results;
  for (int round = 0; round < kRounds; ++round) {
    double elapsed = render(&project, sampleRate, kMeasuredSeconds, variant, pipeline, startRow, &checksum);
    if (elapsed < 0.0) return 1;
    results.push_back(elapsed);
    std::printf("round %d: %.6f s (%.2f%% realtime)\n", round + 1, elapsed,
                elapsed * 100.0 / kMeasuredSeconds);
  }
  std::sort(results.begin(), results.end());
  double median = results[results.size() / 2];
  std::printf("median: %.6f s, %.2f%% realtime, %.2f ns/frame, checksum %.9f\n",
              median, median * 100.0 / kMeasuredSeconds,
              median * 1.0e9 / (kMeasuredSeconds * sampleRate),
              static_cast<double>(checksum));
  projectFree(&project);
  return 0;
}
