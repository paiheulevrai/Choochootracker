#include "doctest.h"
#include "audio_monitor.h"
#include <thread>
#include <atomic>

TEST_CASE("Audio monitor sums voices per track and measures stereo peaks") {
  AudioMonitor monitor;
  float mix[512]{};
  monitor.beginRender();
  monitor.beginChunk(256);
  for (int i = 0; i < 256; ++i) {
    monitor.add(0, i * 2, 0.3f);
    monitor.add(0, i * 2, 0.2f);
    monitor.add(0, i * 2 + 1, -0.5f);
    monitor.add(1, i * 2, 0.25f);
    monitor.add(1, i * 2 + 1, 0.25f);
    mix[i * 2] = mix[i * 2 + 1] = 0.7f; // Final mix may include master FX.
  }
  monitor.finishChunk(mix, 256, 12000);
  monitor.publish();
  AudioMonitorSnapshot snapshot;
  REQUIRE(monitor.receive(snapshot));
  CHECK_FALSE(monitor.receive(snapshot));
  CHECK(snapshot.peaks[0][0] == doctest::Approx(0.5f));
  CHECK(snapshot.peaks[0][1] == doctest::Approx(0.5f));
  CHECK(snapshot.peaks[1][0] == doctest::Approx(0.25f));
  CHECK(snapshot.peaks[1][1] == doctest::Approx(0.25f));
  for (int i = 0; i < 256; ++i) {
    CHECK(snapshot.mix[i] == doctest::Approx(0.7f));
    CHECK(snapshot.mixStereo[i][0] == doctest::Approx(0.7f));
    CHECK(snapshot.mixStereo[i][1] == doctest::Approx(0.7f));
    CHECK(snapshot.tracks[0][i] == 0.0f); // Opposite stereo polarity cancels in scope, not meter.
    CHECK(snapshot.tracks[1][i] == 0.25f);
    CHECK(snapshot.tracks[2][i] == 0.0f);
  }
  monitor.beginRender(); monitor.beginChunk(256);
  float silent[512]{};
  monitor.finishChunk(silent, 256, 12000); monitor.publish();
  REQUIRE(monitor.receive(snapshot));
  CHECK(snapshot.peaks[0][0] == 0.0f);
  CHECK(snapshot.peaks[0][1] == 0.0f);
  CHECK(snapshot.mix[255] == 0.0f);
  CHECK(snapshot.tracks[0][255] == 0.0f);
}

TEST_CASE("Audio monitor history is independent of render chunk boundaries") {
  AudioMonitor whole, split;
  float mix[2048];
  for (int i = 0; i < 2048; ++i) mix[i] = i * 0.001f;
  whole.beginRender(); whole.beginChunk(1024);
  whole.finishChunk(mix, 1024, 48000); whole.publish();
  split.beginRender();
  for (int i = 0; i < 1024; ++i) {
    split.beginChunk(1); split.finishChunk(mix + i * 2, 1, 48000);
    if (i == 399) { split.publish(); split.beginRender(); }
  }
  split.publish();
  AudioMonitorSnapshot a, b;
  REQUIRE(whole.receive(a)); REQUIRE(split.receive(b));
  for (int i = 0; i < 256; ++i) CHECK(a.mix[i] == b.mix[i]);
  CHECK_FALSE(split.reserve(-1));
  CHECK(split.reserve(8192));
}

TEST_CASE("Audio monitor drops telemetry while disabled and resets on resume") {
  AudioMonitor monitor;
  float mix[2] = {0.5f, -0.5f};
  monitor.setEnabled(false);
  monitor.beginRender(); monitor.beginChunk(1); monitor.add(0, 0, 1.0f);
  monitor.finishChunk(mix, 1, 12000); monitor.publish();
  AudioMonitorSnapshot snapshot;
  CHECK_FALSE(monitor.receive(snapshot));

  monitor.setEnabled(true);
  monitor.beginRender(); monitor.beginChunk(1); monitor.finishChunk(mix, 1, 12000); monitor.publish();
  REQUIRE(monitor.receive(snapshot));
  CHECK(snapshot.peaks[0][0] == 0.0f);
  CHECK(snapshot.mix[255] == 0.0f);
}

TEST_CASE("Audio monitor snapshots remain coherent with a concurrent producer") {
  AudioMonitor monitor;
  std::atomic<bool> done{false};
  std::thread producer([&] {
    float mix[512];
    for (int n = 1; n <= 2000; ++n) {
      for (float& v : mix) v = (float)n;
      monitor.beginRender(); monitor.beginChunk(256);
      monitor.finishChunk(mix, 256, 12000); monitor.publish();
    }
    done.store(true);
  });
  int received = 0;
  bool coherent = true;
  AudioMonitorSnapshot snapshot;
  do {
    if (monitor.receive(snapshot)) {
      ++received;
      for (float v : snapshot.mix) if (v != snapshot.mix[0]) coherent = false;
    }
  } while (!done.load());
  producer.join();
  CHECK(received > 0);
  CHECK(coherent);
}
