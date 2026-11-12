#include "doctest.h"
#include "link/link_audio_receive_buffer.h"
#include "link/link_audio_clock.h"

#include <memory>
#include <thread>
#include <vector>

TEST_CASE("Link Audio compensation aligns local and remote impulses across callbacks") {
  for (const int rate : {32000, 44100, 48000}) {
    for (const int millis : {0, 1, 37, 100}) {
      CAPTURE(rate);
      CAPTURE(millis);
      auto queue = std::make_unique<LinkAudioReceiveBuffer>();
      auto delay = std::make_unique<LinkAudioLocalDelay>();
      const int delayFrames = LinkAudioLocalDelay::framesFor(rate, millis);
      constexpr int frames = 32;
      for (int start = 0; start < delayFrames + frames * 2; start += frames) {
        float local[frames * 2] = {};
        int16_t remote[frames * 2] = {};
        if (!start) {
          local[0] = 0.25f; local[1] = -0.25f;
          remote[0] = 8192; remote[1] = -8192;
        }
        REQUIRE(queue->push(remote, frames, rate, start * 1000000.0 / rate));
        delay->process(local, frames, rate, millis);
        queue->mix(local, frames, rate, (start - delayFrames) * 1000000.0 / rate);
        for (int frame = 0; frame < frames; ++frame) {
          const float expected = start + frame == delayFrames ? 0.5f : 0.0f;
          CHECK(local[frame * 2] == doctest::Approx(expected).epsilon(0.0001).scale(1));
          CHECK(local[frame * 2 + 1] == doctest::Approx(-expected).epsilon(0.0001).scale(1));
        }
      }
    }
  }
}

TEST_CASE("Link Audio local compensation survives missing packets and clears stale history") {
  auto delay = std::make_unique<LinkAudioLocalDelay>();
  float output[96] = {};
  output[0] = 0.25f; output[1] = -0.25f;
  delay->process(output, 48, 48000, 1);
  CHECK(output[0] == 0);
  std::fill(std::begin(output), std::end(output), 0.0f);
  delay->process(output, 48, 48000, 1);
  CHECK(output[0] == 0.25f);
  CHECK(output[1] == -0.25f);
  output[0] = 0.75f;
  delay->process(output, 48, 48000, 1);
  delay->reset();
  std::fill(std::begin(output), std::end(output), 0.0f);
  delay->process(output, 48, 48000, 1);
  CHECK(output[0] == 0);
  output[0] = 0.5f;
  delay->process(output, 48, 48000, 0);
  CHECK(output[0] == 0.5f);
  delay->process(output, 48, 44100, 1);
  CHECK(output[0] == 0);
  delay->process(output, 48, 44100, 2);
  CHECK(output[0] == 0);
}

TEST_CASE("Link Audio sample clock removes batched callback timestamp overlaps") {
  LinkAudioClock clock;
  constexpr double duration = 512.0 * 1000000.0 / 48000;
  double previous = 0;
  double maxError = 0;
  for (int callback = 0; callback < 1000; ++callback) {
    const double scheduled = 1000000 + callback * duration;
    const double arrived = scheduled - (callback % 2 ? duration : 0);
    const double time = clock.time(callback * 512, 48000, arrived);
    CHECK(clock.time(callback * 512, 48000, arrived) == time);
    if (callback) maxError = std::max(maxError, std::fabs(time - previous - duration));
    previous = time;
  }
  CHECK(maxError < 11);
}

TEST_CASE("Link Audio sample clock follows hardware drift and resets on reconfiguration") {
  LinkAudioClock clock;
  constexpr double duration = 512.0 * 1000000.0 / 48000;
  double time = 0, host = 0;
  for (int callback = 0; callback < 20000; ++callback) {
    host = 1000000 + callback * duration * 1.0003;
    time = clock.time(callback * 512, 48000, host);
  }
  CHECK(std::fabs(time - host) < 4000);
  CHECK(clock.time(0, 44100, 5000000) == 5000000);
  clock.reset();
  CHECK(clock.time(100, 44100, 6000000) == 6000000);
  CHECK(clock.time(200, 44100, 9000000) == 9000000);
}

TEST_CASE("Link Audio continuous tone survives independently batched sender and receiver callbacks") {
  auto queue = std::make_unique<LinkAudioReceiveBuffer>();
  LinkAudioClock senderClock, receiverClock;
  constexpr int frames = 512, rate = 48000;
  constexpr double duration = frames * 1000000.0 / rate;
  constexpr double step = 6.283185307179586 * 440 / rate;
  double residual = 0, power = 0;
  float previous = 0, previous2 = 0;
  int gaps = 0, measured = 0;
  for (int callback = 0; callback < 1000; ++callback) {
    int16_t input[frames * 2];
    for (int frame = 0; frame < frames; ++frame) {
      input[frame * 2] = static_cast<int16_t>(10000 * std::sin((callback * frames + frame) * step));
      input[frame * 2 + 1] = -input[frame * 2];
    }
    const double now = 1000000 + callback * duration;
    const double send = senderClock.time(callback * frames, rate,
      now - (callback % 2 ? duration : 0));
    REQUIRE(queue->push(input, frames, rate, send));
    float output[frames * 2] = {};
    const double receive = receiverClock.time(callback * frames, rate,
      now - (callback % 4) * duration);
    const int mixed = queue->mix(output, frames, rate, receive - 100000);
    if (callback > 20) {
      gaps += frames - mixed;
      for (int frame = 0; frame < frames; ++frame) {
        const float x = output[frame * 2];
        const double error = x - 2 * std::cos(step) * previous + previous2;
        if (measured > 2) { residual += error * error; power += x * x; }
        previous2 = previous; previous = x; ++measured;
        REQUIRE(output[frame * 2 + 1] == -x);
      }
    }
  }
  INFO("missing frames=", gaps, " measured frames=", measured);
  CHECK(gaps == 0);
  CHECK(10 * std::log10(power / residual) > 40);
}

TEST_CASE("Link Audio receive schedules stereo samples without early playback") {
  auto queue = std::make_unique<LinkAudioReceiveBuffer>();
  const int16_t samples[] = {8192, -8192, 16384, -16384, 24576, -24576};
  REQUIRE(queue->push(samples, 3, 48000, 1000000));
  float output[8] = {};
  CHECK(queue->mix(output, 1, 48000, 999000) == 0);
  CHECK(output[0] == 0);
  CHECK(queue->mix(output, 3, 48000, 1000000) == 3);
  CHECK(output[0] == doctest::Approx(0.25));
  CHECK(output[1] == doctest::Approx(-0.25));
  CHECK(output[4] == doctest::Approx(0.75));
  CHECK(queue->mix(output, 1, 48000, 1001000) == 0);
}

TEST_CASE("Link Audio receive resamples and adds to the local stereo mix") {
  auto queue = std::make_unique<LinkAudioReceiveBuffer>();
  const int16_t samples[] = {0, 0, 16384, -16384, 0, 0};
  REQUIRE(queue->push(samples, 3, 24000, 0));
  float output[8];
  for (auto& sample : output) sample = 0.1f;
  CHECK(queue->mix(output, 4, 48000, 0) == 4);
  CHECK(output[0] == doctest::Approx(0.1));
  CHECK(output[2] == doctest::Approx(0.35));
  CHECK(output[3] == doctest::Approx(-0.15));
  CHECK(output[4] == doctest::Approx(0.6));
}

TEST_CASE("Link Audio receive interpolates across packets without sub-sample silence") {
  auto queue = std::make_unique<LinkAudioReceiveBuffer>();
  const int16_t first[] = {0, 0, 8192, -8192};
  const int16_t second[] = {16384, -16384, 24576, -24576};
  constexpr double sampleTime = 1000000.0 / 48000;
  REQUIRE(queue->push(first, 2, 48000, 0));
  REQUIRE(queue->push(second, 2, 48000, 2 * sampleTime + 0.1));
  float output[2] = {};
  CHECK(queue->mix(output, 1, 48000, 1.5 * sampleTime) == 1);
  CHECK(output[0] == doctest::Approx(0.375));
  CHECK(output[1] == doctest::Approx(-0.375));
  output[0] = output[1] = 0;
  CHECK(queue->mix(output, 1, 48000, 2 * sampleTime) == 1);
  CHECK(output[0] == doctest::Approx(0.5));
}

TEST_CASE("Link Audio receive bounds memory, splits packets and recovers after overflow") {
  auto queue = std::make_unique<LinkAudioReceiveBuffer>();
  std::vector<int16_t> samples(32768 * 2, 4096);
  CHECK_FALSE(queue->push(nullptr, 1, 48000, 0));
  CHECK_FALSE(queue->push(samples.data(), 0, 48000, 0));
  CHECK_FALSE(queue->push(samples.data(), 1, 0, 0));
  CHECK_FALSE(queue->push(samples.data(), 1, 48000, INFINITY));
  REQUIRE(queue->push(samples.data(), 32768, 48000, 0));
  CHECK_FALSE(queue->push(samples.data(), 1, 48000, 1000000));
  float output[8] = {};
  CHECK(queue->mix(output, 4, 48000, 500000) == 4);
  CHECK(output[0] == doctest::Approx(0.125));
  queue->discard();
  CHECK(queue->mix(output, 4, 48000, 500000) == 0);
  REQUIRE(queue->push(samples.data(), 4, 48000, 2000000));
  CHECK(queue->mix(output, 4, 48000, 2000000) == 4);
}

TEST_CASE("Link Audio receive skips stale packets and leaves gaps silent") {
  auto queue = std::make_unique<LinkAudioReceiveBuffer>();
  const int16_t samples[] = {16384, -16384};
  REQUIRE(queue->push(samples, 1, 48000, 0));
  REQUIRE(queue->push(samples, 1, 48000, 1000000));
  float output[2] = {};
  CHECK(queue->mix(output, 1, 48000, 500000) == 0);
  CHECK(output[0] == 0);
  CHECK(queue->mix(output, 1, 48000, 1000000) == 1);
  CHECK(output[0] == doctest::Approx(0.5));
}

TEST_CASE("Link Audio receive holds 100 ms plus a full callback of minimum-size CCT packets") {
  CHECK(sizeof(LinkAudioReceiveBuffer) <= 136 * 1024);
  auto queue = std::make_unique<LinkAudioReceiveBuffer>();
  const int packets = (4800 + 2048 + 31) / 32;
  int16_t samples[64];
  for (auto& sample : samples) sample = 8192;
  for (int i = 0; i < packets; ++i)
    REQUIRE(queue->push(samples, 32, 48000, i * 32.0 * 1000000.0 / 48000));
  float output[64] = {};
  CHECK(queue->mix(output, 32, 48000, 0) == 32);
  CHECK(output[0] == doctest::Approx(0.25));
}

TEST_CASE("Link Audio receive exchanges packets across producer and audio threads") {
  auto queue = std::make_unique<LinkAudioReceiveBuffer>();
  constexpr int count = 20000;
  std::thread producer([&]() {
    const int16_t samples[] = {16384, -16384};
    for (int i = 0; i < count; ++i)
      while (!queue->push(samples, 1, 48000, i * 1000.0)) std::this_thread::yield();
  });
  int errors = 0;
  for (int i = 0; i < count; ++i) {
    float output[2] = {};
    while (!queue->mix(output, 1, 48000, i * 1000.0)) std::this_thread::yield();
    if (output[0] != 0.5f || output[1] != -0.5f) ++errors;
  }
  producer.join();
  CHECK(errors == 0);
}
