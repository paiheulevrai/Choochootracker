#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "chipnomad_lib.h"
#include "playback.h"
#include "project.h"

namespace {
constexpr int kBlockFrames = 2048;
constexpr int kWarmupSeconds = 3;
constexpr int kMeasuredSeconds = 20;
constexpr int kRounds = 5;

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
  }
}

double render(Project* project, int sampleRate, int seconds, volatile double* checksum) {
  ChipNomadState* state = chipnomadCreate();
  if (!state) return -1.0;
  state->ownsProjectResources = 0;
  state->project = *project;
  state->audioProject = *project;
  playbackInit(&state->playbackState, &state->audioProject);
  chipnomadInitChips(state, sampleRate, nullptr);
  chipnomadReserveRenderBuffers(state, kBlockFrames);
  playbackStartSong(&state->playbackState, 0, 0, 1);

  std::vector<float> buffer(kBlockFrames * 2);
  int remaining = seconds * sampleRate;
  const auto started = std::chrono::steady_clock::now();
  while (remaining > 0) {
    int frames = std::min(remaining, kBlockFrames);
    if (chipnomadRender(state, buffer.data(), frames) != frames) {
      chipnomadDestroy(state);
      return -1.0;
    }
    *checksum += buffer[(remaining / kBlockFrames) % (frames * 2)];
    remaining -= frames;
  }
  const double elapsed = std::chrono::duration<double>(
    std::chrono::steady_clock::now() - started).count();
  chipnomadDestroy(state);
  return elapsed;
}
}  // namespace

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "projects/alf dance.cct";
  const int sampleRate = argc > 2 ? std::atoi(argv[2]) : 96000;
  const char* variant = argc > 3 ? argv[3] : "full";
  if (sampleRate != 48000 && sampleRate != 96000) {
    std::fprintf(stderr, "Sample rate must be 48000 or 96000\n");
    return 1;
  }
  Project project;
  projectInit(&project);
  if (projectLoad(&project, path)) {
    std::fprintf(stderr, "Cannot load %s: %s\n", path, projectFileError);
    return 1;
  }
  configureVariant(&project, variant);
  std::printf("variant: %s, sample rate: %d\n", variant, sampleRate);

  volatile double checksum = 0.0;
  if (render(&project, sampleRate, kWarmupSeconds, &checksum) < 0.0) return 1;
  std::vector<double> results;
  for (int round = 0; round < kRounds; ++round) {
    double elapsed = render(&project, sampleRate, kMeasuredSeconds, &checksum);
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
