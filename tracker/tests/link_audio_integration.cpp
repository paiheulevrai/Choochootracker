// From tracker: make -f Makefile.windows link-audio-integration
#if defined(LINK_PLATFORM_WINDOWS)
#include <ableton/platforms/Config.hpp>
#undef interface
#endif
#include <ableton/LinkAudio.hpp>
#include "link/link_client.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

int main() {
  using namespace std::chrono;
  LinkClientState* sender = linkClientCreate();
  linkClientSelect(sender);
  if (!linkClientSetAudioEnabled(sender, 1)) {
    std::fprintf(stderr, "Link Audio sender unavailable\n");
    linkClientDestroy(sender);
    return 1;
  }
  linkClientConnect(sender);

  std::atomic<int> received{0};
  std::atomic<int> receivedLarge{0};
  std::atomic<int> invalid{0};
  std::atomic<int> firstFrames{0}, firstChannels{0}, firstRate{0}, firstLeft{0}, firstRight{0};
  ableton::LinkAudio receiver(125.0, "CCT integration test");
  receiver.enable(true);
  receiver.enableLinkAudio(true);
  std::unique_ptr<ableton::LinkAudioSource> source;
  const auto deadline = steady_clock::now() + seconds(20);
  int16_t samples[256];
  int16_t largeSamples[4096];
  for (int i = 0; i < 128; ++i) {
    samples[i * 2] = 1234;
    samples[i * 2 + 1] = -1234;
  }
  for (int i = 0; i < 2048; ++i) {
    largeSamples[i * 2] = 2345;
    largeSamples[i * 2 + 1] = -2345;
  }
  uint64_t counter = 0;
  while (steady_clock::now() < deadline &&
         (received.load() < 128 || receivedLarge.load() < 2048)) {
    if (!source) {
      for (const auto& channel : receiver.channels()) {
        if (channel.peerName == "ChooChooTracker" && channel.name == "Master") {
          source.reset(new ableton::LinkAudioSource(receiver, channel.id,
            [&](ableton::LinkAudioSource::BufferHandle buffer) {
              bool valid = buffer.info.numFrames > 0 && buffer.info.numChannels == 2 &&
                           buffer.info.sampleRate == 48000;
              const int16_t expected = valid ? buffer.samples[0] : 0;
              valid = valid && (expected == 1234 || expected == 2345);
              for (size_t i = 0; valid && i < buffer.info.numFrames; ++i)
                valid = buffer.samples[i * 2] == expected && buffer.samples[i * 2 + 1] == -expected;
              if (!valid) {
                if (invalid.load() == 0) {
                  firstFrames = static_cast<int>(buffer.info.numFrames);
                  firstChannels = static_cast<int>(buffer.info.numChannels);
                  firstRate = static_cast<int>(buffer.info.sampleRate);
                  if (buffer.info.numFrames && buffer.info.numChannels >= 2) {
                    firstLeft = buffer.samples[0];
                    firstRight = buffer.samples[1];
                  }
                }
                ++invalid;
              } else if (expected == 1234)
                received += static_cast<int>(buffer.info.numFrames);
              else
                receivedLarge += static_cast<int>(buffer.info.numFrames);
            }));
          break;
        }
      }
    }
    const auto now = duration_cast<microseconds>(steady_clock::now().time_since_epoch());
    linkClientNotifyAudioFrame(sender, counter, static_cast<uint64_t>(now.count()));
    const bool large = received.load() >= 128;
    const int frames = large ? 2048 : 128;
    linkClientSendAudio(sender, large ? largeSamples : samples, frames, 48000);
    counter += frames;
    std::this_thread::sleep_for(milliseconds(10));
  }
  source.reset();
  receiver.enableLinkAudio(false);
  receiver.enable(false);
  linkClientDestroy(sender);
  if (received.load() < 128 || receivedLarge.load() < 2048 || invalid.load()) {
    std::fprintf(stderr, "Link Audio integration failed: small=%d large=%d invalid=%d first=%d frames, %d channels, %d Hz, %d/%d\n",
                 received.load(), receivedLarge.load(), invalid.load(), firstFrames.load(),
                 firstChannels.load(), firstRate.load(), firstLeft.load(), firstRight.load());
    return 1;
  }
  std::printf("Link Audio stereo received successfully\n");
  return 0;
}
