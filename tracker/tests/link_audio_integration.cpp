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
#include <algorithm>
#include <cmath>

static bool continuousToneTest() {
  using namespace std::chrono;
  auto* sender = linkClientCreate();
  auto* receiver = linkClientCreate();
  linkClientSelect(sender); linkClientSelect(receiver);
  if (!linkClientSetAudioEnabled(sender, 1) || !linkClientSetAudioReceiveEnabled(receiver, 1)) return false;
  linkClientConnect(sender); linkClientConnect(receiver);
  constexpr int frames = 512, rate = 48000;
  constexpr double step = 6.283185307179586 * 440 / rate;
  const auto start = steady_clock::now();
  uint64_t sendCounter = 0, receiveCounter = 0;
  double power = 0, residual = 0;
  float previous = 0, previous2 = 0;
  int measured = 0, missing = 0;
  for (int batch = 0; batch < 240; ++batch) {
    linkClientPoll(sender); linkClientPoll(receiver);
    // ALSA can supply callbacks in bursts rather than evenly spaced.
    for (int block = 0; block < 2; ++block) {
      int16_t input[frames * 2];
      for (int i = 0; i < frames; ++i) {
        input[i * 2] = static_cast<int16_t>(10000 * std::sin((sendCounter + i) * step));
        input[i * 2 + 1] = -input[i * 2];
      }
      const auto now = duration_cast<microseconds>(steady_clock::now().time_since_epoch());
      linkClientNotifyAudioFrame(sender, sendCounter, now.count());
      linkClientSendAudio(sender, input, frames, rate);
      sendCounter += frames;
    }
    if (batch % 2 == 1) {
      for (int block = 0; block < 4; ++block) {
        float output[frames * 2] = {};
        const auto now = duration_cast<microseconds>(steady_clock::now().time_since_epoch());
        linkClientNotifyAudioFrame(receiver, receiveCounter, now.count());
        const int mixed = linkClientMixReceivedAudio(receiver, output, frames, rate);
        receiveCounter += frames;
        if (batch < 48) continue;
        if (mixed != frames)
          std::fprintf(stderr, "Tone gap: batch=%d block=%d mixed=%d status=%s\n",
            batch, block, mixed, linkClientAudioReceiveStatus(receiver));
        missing += frames - mixed;
        for (int i = 0; i < frames; ++i) {
          const float x = output[i * 2];
          const double error = x - 2 * std::cos(step) * previous + previous2;
          if (measured > 2) { power += x * x; residual += error * error; }
          previous2 = previous; previous = x; ++measured;
        }
      }
    }
    std::this_thread::sleep_until(start + microseconds(static_cast<int64_t>(
      (batch + 1) * 2.0 * frames * 1000000.0 / rate)));
  }
  linkClientDestroy(receiver); linkClientDestroy(sender);
  const double snr = power > 0 && residual > 0 ? 10 * std::log10(power / residual) : 0;
  std::printf("Continuous network tone: missing=%d/%d frames, discontinuity residual=%.1f dB\n",
    missing, measured, snr);
  return missing == 0 && snr > 40;
}

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
  if (!linkClientSetAudioReceiveEnabled(sender, 1)) return 1;
  for (int i = 0; i < 20; ++i) {
    linkClientPoll(sender);
    std::this_thread::sleep_for(milliseconds(10));
  }
  if (std::string(linkClientAudioReceiveStatus(sender)) != "Waiting for CCT" &&
      linkClientGetPeerCount(sender) == 0) {
    std::fprintf(stderr, "Link Audio must not subscribe to its own Master: %s\n",
                 linkClientAudioReceiveStatus(sender));
    linkClientDestroy(sender);
    return 1;
  }
  LinkClientState* cctReceiver = linkClientCreate();
  linkClientSelect(cctReceiver);
  if (!linkClientSetAudioReceiveEnabled(cctReceiver, 1)) return 1;
  linkClientConnect(cctReceiver);

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
  int cctReceived = 0;
  while (steady_clock::now() < deadline &&
         (received.load() < 128 || receivedLarge.load() < 2048 || cctReceived < 2048)) {
    linkClientPoll(sender);
    linkClientPoll(cctReceiver);
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
    linkClientNotifyAudioFrame(cctReceiver, counter, static_cast<uint64_t>(now.count()));
    float mix[1024] = {};
    linkClientMixReceivedAudio(cctReceiver, mix, 512, 44100);
    for (int i = 0; i < 512; ++i) {
      if (mix[i * 2] == 0) continue;
      const float sample = mix[i * 2] * 32768.0f;
      if ((std::fabs(sample - 1234) > 0.1f && std::fabs(sample - 2345) > 0.1f) ||
          mix[i * 2 + 1] != -mix[i * 2]) ++invalid;
      else ++cctReceived;
    }
    counter += frames;
    std::this_thread::sleep_for(milliseconds(10));
  }
  source.reset();
  receiver.enableLinkAudio(false);
  receiver.enable(false);
  // Both CCT peers have identical names. Each must subscribe to the other,
  // and disabling Send must not disable the independent receive path.
  bool reverseReceived = false;
  if (cctReceived >= 2048 && linkClientSetAudioEnabled(cctReceiver, 1)) {
    linkClientSetAudioEnabled(sender, 0);
    for (auto end = steady_clock::now() + seconds(10); steady_clock::now() < end && !reverseReceived;) {
      linkClientPoll(sender); linkClientPoll(cctReceiver);
      const auto now = duration_cast<microseconds>(steady_clock::now().time_since_epoch());
      linkClientNotifyAudioFrame(cctReceiver, counter, static_cast<uint64_t>(now.count()));
      linkClientSendAudio(cctReceiver, largeSamples, 512, 48000);
      linkClientNotifyAudioFrame(sender, counter, static_cast<uint64_t>(now.count()));
      float mix[1024] = {};
      linkClientMixReceivedAudio(sender, mix, 512, 48000);
      for (int i = 0; i < 512; ++i)
        if (std::fabs(mix[i * 2] * 32768.0f - 2345) < 0.1f &&
            mix[i * 2 + 1] == -mix[i * 2]) reverseReceived = true;
      counter += 512;
      std::this_thread::sleep_for(milliseconds(10));
    }
  }
  linkClientSetAudioReceiveEnabled(sender, 0);
  float disabledMix[4] = {};
  const bool receiveDisabled = linkClientMixReceivedAudio(sender, disabledMix, 2, 48000) == 0 &&
    disabledMix[0] == 0 && disabledMix[1] == 0;
  linkClientDisconnect(cctReceiver);
  linkClientSetAudioReceiveEnabled(cctReceiver, 0);
  linkClientDestroy(cctReceiver);
  linkClientDestroy(sender);
  if (received.load() < 128 || receivedLarge.load() < 2048 || cctReceived < 2048 ||
      !reverseReceived || !receiveDisabled || invalid.load()) {
    std::fprintf(stderr, "Link Audio integration failed: small=%d large=%d cct=%d reverse=%d off=%d invalid=%d first=%d frames, %d channels, %d Hz, %d/%d\n",
                 received.load(), receivedLarge.load(), cctReceived, reverseReceived, receiveDisabled, invalid.load(), firstFrames.load(),
                 firstChannels.load(), firstRate.load(), firstLeft.load(), firstRight.load());
    return 1;
  }
  std::printf("PASS: stereo 128/2048, CCT-to-CCT receive/resampling, identical peer names, reverse direction, receive Off.\n");
  return continuousToneTest() ? 0 : 1;
}
