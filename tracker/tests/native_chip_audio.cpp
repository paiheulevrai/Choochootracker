// Developer-only device audio callback check; never linked into the application.
#include <SDL2/SDL.h>
#include "chipnomad_lib.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

struct AudioCheck {
  ChipNomadState* state = nullptr;
  std::vector<float> renderBuffer;
  double times[16000]{};
  unsigned callbacks = 0;
  unsigned bad = 0;
  double energy = 0;
  float peak = 0;
};

static void callback(void* opaque, Uint8* bytes, int length) {
  auto& check = *static_cast<AudioCheck*>(opaque);
  const int frames = length / (2 * sizeof(int16_t));
  if (frames <= 0 || size_t(frames) * 2 > check.renderBuffer.size()) {
    std::memset(bytes, 0, size_t(std::max(length, 0))); ++check.bad; return;
  }
  auto* audio = check.renderBuffer.data();
  auto* output = reinterpret_cast<int16_t*>(bytes);
  const auto start = std::chrono::steady_clock::now();
  chipnomadRender(check.state, audio, frames);
  for (int i = 0; i < frames * 2; ++i) {
    if (!std::isfinite(audio[i])) { ++check.bad; output[i] = 0; }
    else {
      check.energy += double(audio[i]) * audio[i];
      check.peak = std::max(check.peak, std::abs(audio[i]));
      const float sample = std::clamp(audio[i] * 32767.f, -32768.f, 32767.f);
      output[i] = static_cast<int16_t>(sample);
    }
  }
  const double elapsed = std::chrono::duration<double, std::micro>(
      std::chrono::steady_clock::now() - start).count();
  if (check.callbacks < 16000) check.times[check.callbacks] = elapsed;
  ++check.callbacks;
}

int main(int argc, char** argv) {
  int frames = 512;
  if (argc == 3) {
    char* end = nullptr; const long parsed = std::strtol(argv[2], &end, 10);
    if (!end || *end || parsed < 64 || parsed > 16384) return 1;
    frames = int(parsed);
  }
  if ((argc != 2 && argc != 3) || SDL_Init(SDL_INIT_AUDIO) != 0) {
    std::fprintf(stderr, "Audio setup: %s\n", SDL_GetError());
    return 1;
  }
  AudioCheck check;
  check.renderBuffer.resize(16384 * 2);
  check.state = chipnomadCreate();
  if (!check.state || projectLoad(&check.state->project, argv[1])) return 2;
  // Test-only master gain leaves headroom for the deliberately dense fixture.
  // It changes neither stored patches nor the installed user's mix setting.
  check.state->mixVolume = .4f;
  SDL_AudioSpec desired{}, obtained{};
  desired.freq = 48000;
  desired.format = AUDIO_S16SYS;
  desired.channels = 2;
  desired.samples = frames;
  desired.callback = callback;
  desired.userdata = &check;
  const auto device = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
  if (!device) {
    std::fprintf(stderr, "Open audio: %s\n", SDL_GetError());
    chipnomadDestroy(check.state);
    SDL_Quit();
    return 3;
  }
  chipnomadInitChips(check.state, obtained.freq, nullptr);
  chipnomadReserveRenderBuffers(check.state, obtained.samples);
  chipnomadQueueProjectRefresh(check.state);
  chipnomadQueuePlaybackStartSong(check.state, 0, 0, 1);
  SDL_PauseAudioDevice(device, 0);
  SDL_Delay(70000);
  SDL_PauseAudioDevice(device, 1);
  SDL_CloseAudioDevice(device);
  const unsigned count = std::min(check.callbacks, 16000u);
  std::vector<double> times;
  if (count > 100) times.assign(check.times + 100, check.times + count);
  std::sort(times.begin(), times.end());
  const double deadline = obtained.samples * 1e6 / obtained.freq;
  unsigned misses = 0;
  for (double time : times) misses += time > deadline;
  if (!times.empty()) std::printf(
      "driver=%s format=S16 master_gain=0.4 rate=%d frames=%d callbacks=%u p95_us=%.3f p99_us=%.3f "
      "worst_us=%.3f render_deadline_misses=%u energy=%.6f peak=%.6f nonfinite=%u\n",
      SDL_GetCurrentAudioDriver(), obtained.freq, obtained.samples, check.callbacks,
      times[size_t(times.size() * .95)], times[size_t(times.size() * .99)],
      times.back(), misses, check.energy, check.peak, check.bad);
  chipnomadDestroy(check.state);
  SDL_Quit();
  return times.empty() || check.bad || check.energy < .001 ? 4 : 0;
}
