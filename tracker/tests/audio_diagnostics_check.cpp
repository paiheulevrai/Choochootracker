// Standalone host check of the Android SDL callback, using SDL's dummy device.
// See docs/android-audio-debugging.md for the compile command.
#define SDL_MAIN_HANDLED
#include <assert.h>
#include <algorithm>
#include <vector>
#include "../platforms/sdl2/corelib_audio.cpp"

static int forwardedFrames;
static void renderConstant(int16_t* buffer, int frames) {
  forwardedFrames += frames;
  std::fill(buffer, buffer + 2 * frames, 123);
}

int main() {
  SDL_SetMainReady();
  SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
  SDL_setenv("CCT_AUDIO_DIAG", "1", 1);
  SDL_setenv("CCT_AUDIO_TONE", "1", 1);
  assert(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) == 0);
  for (int frames : {512, 1024, 2048, 4906}) {
    assert(audioSetup(renderConstant, 48000, frames) == 0);
    std::vector<int16_t> whole(2 * frames + 2, 30000), split(whole);
    sdlAudioCallback((void*)renderConstant, (uint8_t*)(whole.data() + 1), frames * 4);
    assert(forwardedFrames == 0);
    assert(whole.front() == 30000 && whole.back() == 30000);
    for (int i = 0; i < frames; ++i) {
      assert(whole[2 * i + 1] == whole[2 * i + 2]);
      assert(abs(whole[2 * i + 1]) <= 2048);
    }
    // Splitting a callback must preserve the oscillator's sample sequence.
    tonePhase = 0;
    const int first = 137;
    sdlAudioCallback((void*)renderConstant, (uint8_t*)(split.data() + 1), first * 4);
    sdlAudioCallback((void*)renderConstant, (uint8_t*)(split.data() + 1 + 2 * first), (frames - first) * 4);
    assert(whole == split);
    assert(callbackCount == 3);
    assert(lastFrames == (unsigned)(frames - first));
    assert(audioDiagnostics::invalidBuffers == 0 && audioDiagnostics::renderFailures == 0);
    audioCleanup();
  }
  SDL_setenv("CCT_AUDIO_TONE", "0", 1);
  assert(audioSetup(renderConstant, 48000, 2048) == 0);
  int16_t samples[256] = {};
  sdlAudioCallback((void*)renderConstant, (uint8_t*)samples, sizeof(samples));
  assert(forwardedFrames == 128);
  assert(std::all_of(std::begin(samples), std::end(samples), [](int16_t s) { return s == 123; }));
  audioCleanup();
  SDL_setenv("CCT_AUDIO_DIAG", "0", 1);
  SDL_setenv("CCT_AUDIO_TONE", "1", 1);
  assert(audioSetup(renderConstant, 48000, 2048) == 0);
  const auto recorded = callbackCount.load();
  sdlAudioCallback((void*)renderConstant, (uint8_t*)samples, sizeof(samples));
  assert(forwardedFrames == 256 && callbackCount == recorded);
  assert(!audioDiagnostics::enabled && !diagnosticThread);
  audioCleanup();
  SDL_Quit();
  puts("Android audio callback: forwarding, stereo, bounds and phase continuity passed.");
}
