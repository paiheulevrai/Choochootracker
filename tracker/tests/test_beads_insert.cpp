#include "doctest.h"
#include "beads_insert.h"
#include "insert_fx.h"
#include "chipnomad_lib.h"
#include "playback_internal.h"
#include "external/mutable/stmlib/utils/random.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

namespace {
void defaults(float* p) {
  for (int i = 0; i < 8; ++i)
    p[i] = insertMap(insertBeads, i, insertDescriptor(insertBeads).parameters[i].initial);
  p[0] = .02f;
  p[7] = 1;
}
std::vector<float> render(BeadsInsert& fx, float rate, int frames, float* p,
                          int block = 127, float frequency = 220) {
  std::vector<float> result(frames * 2);
  float l[256], r[256];
  for (int offset = 0; offset < frames;) {
    int n = std::min(block, frames - offset);
    for (int i = 0; i < n; ++i) {
      l[i] = .3f * std::sin(6.28318530718f * frequency * (offset + i) / rate);
      r[i] = .2f * std::sin(6.28318530718f * frequency * 1.5f * (offset + i) / rate);
    }
    fx.process(l, r, n, p);
    for (int i = 0; i < n; ++i) {
      result[2 * (offset + i)] = l[i];
      result[2 * (offset + i) + 1] = r[i];
    }
    offset += n;
  }
  return result;
}
double power(const std::vector<float>& x, size_t start = 0) {
  double sum = 0;
  for (size_t i = start; i < x.size(); ++i) sum += x[i] * x[i];
  return sum / (x.size() - start);
}
}

TEST_SUITE("Beads insert") {
TEST_CASE("pool is lazy bounded and keeps leases alive until producer collection") {
  BeadsPool pool;
  CHECK(pool.allocated() == 0);
  CHECK_FALSE(pool.prepare(-1, 48000));
  CHECK_FALSE(pool.prepare(BeadsPool::capacity, 48000));
  BeadsPool::Handle handles[BeadsPool::capacity];
  for (int i = 0; i < BeadsPool::capacity; ++i) {
    handles[i] = pool.prepare(i, 48000);
    REQUIRE(handles[i]);
    CHECK(pool.allocated() == i + 1);
    CHECK(pool.prepare(i, 48000).get() == handles[i].get());
    for (int j = 0; j < i; ++j) CHECK(handles[i].get() != handles[j].get());
  }
  auto snapshot = handles[0];
  for (auto& handle : handles) handle = {};
  CHECK(pool.allocated() == BeadsPool::capacity);
  pool.collect();
  CHECK(pool.allocated() == 1);
  snapshot = {};
  CHECK(pool.allocated() == 1);
  pool.collect();
  CHECK(pool.allocated() == 0);
}
TEST_CASE("chains keep Beads through fade out and allocation absence affects only Beads") {
  BeadsPool pool;
  InsertChain chain(48000), reference(48000);
  REQUIRE(chain.ready());
  InsertConfig config[2]{};
  insertSelect(&config[0], insertBeads);
  insertSelect(&config[1], insertDistortion);
  InsertAutomation a{}, b{};
  chain.sync(config, &a); // Missing Beads instance must not disable distortion.
  config[0] = {};
  reference.sync(config, &b);
  uint8_t v[2][8]{};
  std::memcpy(v[1], config[1].values, 8);
  float actual[2048], expected[2048];
  std::fill_n(actual, 2048, .7f);
  std::fill_n(expected, 2048, .7f);
  chain.process(actual, 1024, v);
  reference.process(expected, 1024, v);
  CHECK(std::equal(actual, actual + 2048, expected));
  CHECK(actual[2047] != .7f);

  insertSelect(&config[0], insertBeads);
  std::memcpy(v[0], config[0].values, 8);
  chain.attachBeads(0, pool.prepare(0, 48000));
  chain.sync(config, &a);
  chain.process(actual, 1024, v);
  insertSelect(&config[0], insertOff);
  chain.sync(config, &a);
  pool.collect();
  CHECK(pool.allocated() == 1);
  chain.process(actual, 1, v);
  pool.collect();
  CHECK(pool.allocated() == 1);
  chain.process(actual, 1024, v);
  CHECK(pool.allocated() == 1); // No destruction on the processing thread.
  pool.collect();
  CHECK(pool.allocated() == 0);
}
TEST_CASE("eight existing addresses include independently packed shape and freeze") {
  CHECK(insertDescriptor(insertBeads).count == 8);
  char a[64], b[64];
  for (int shape = 0; shape < 128; ++shape) {
    insertDescribe(a, sizeof(a), insertBeads, 6, shape);
    insertDescribe(b, sizeof(b), insertBeads, 6, shape | 128);
    CHECK(std::strncmp(a, b, 4) == 0);
    CHECK(std::strstr(a, "Recording") != nullptr);
    CHECK(std::strstr(b, "Frozen") != nullptr);
    CHECK(insertMap(insertBeads, 6, shape | 128) == (shape | 128));
  }
  auto project = std::make_unique<Project>();
  projectInit(project.get());
  insertSelect(&project->trackInserts[0][1], insertBeads);
  auto state = std::make_unique<PlaybackState>();
  playbackInit(state.get(), project.get());
  uint8_t fx[] = {fxF27, 0xC0};
  initFX(state.get(), 0, fx, nullptr, 0);
  CHECK(state->tracks[0].inserts.values[1][6] == 0xC0);
  CHECK(state->tracks[0].inserts.valid[1] == 64);
  CHECK(state->tracks[0].inserts.valid[0] == 0);
  CHECK(project->trackInserts[0][1].values[6] == 38);
  fx[1] = 0x40;
  initFX(state.get(), 0, fx, nullptr, 0);
  CHECK(state->tracks[0].inserts.values[1][6] == 0x40);
}
TEST_CASE("continuous wet grains sound at supported host rates without FX automation") {
  for (float rate : {8000.f, 32000.f, 44100.f, 48000.f, 96000.f, 192000.f}) {
    CAPTURE(rate);
    BeadsInsert fx(rate);
    REQUIRE(fx.ready());
    float p[8]; defaults(p);
    auto audio = render(fx, rate, int(rate * .75f), p);
    bool finite = true;
    for (float x : audio) finite &= std::isfinite(x) && std::abs(x) < 4;
    CHECK(finite);
    CHECK(power(audio, audio.size() / 2) > .00001);
  }
}
TEST_CASE("scatter runs internally and host instances do not share RNG or histories") {
  BeadsInsert a(48000), b(48000), dryScatter(48000);
  float p[8], plain[8]; defaults(p); defaults(plain); p[4] = .8f;
  uint32_t before = stmlib::Random::state();
  auto first = render(a, 48000, 96000, p, 1);
  auto second = render(b, 48000, 96000, p, 251);
  CHECK(first == second);
  CHECK(stmlib::Random::state() == before);
  auto third = render(dryScatter, 48000, 96000, plain);
  double difference = 0;
  for (size_t i = first.size() / 2; i < first.size(); ++i)
    difference += std::abs(first[i] - third[i]);
  CHECK(difference / (first.size() / 2) > .001);
  a.reset();
  auto restarted = render(a, 48000, 96000, p, 73);
  CHECK(restarted == first);
}
TEST_CASE("frozen grains retain captured audio and ignore subsequent input") {
  BeadsInsert a(48000), b(48000);
  float p[8]; defaults(p); p[4] = .25f;
  render(a, 48000, 96000, p);
  render(b, 48000, 96000, p);
  p[6] = int(p[6]) | 128;
  auto first = render(a, 48000, 96000, p, 127, 330);
  auto second = render(b, 48000, 96000, p, 73, 660);
  CHECK(power(first, 96000) > .00001);
  double maximum = 0;
  for (size_t i = 96000; i < first.size(); ++i)
    maximum = std::max(maximum, double(std::abs(first[i] - second[i])));
  CHECK(maximum < .00001);
  p[6] = int(p[6]) & 127;
  auto resumedA = render(a, 48000, 96000, p, 127, 330);
  auto resumedB = render(b, 48000, 96000, p, 73, 660);
  CHECK(resumedA != resumedB);
}
TEST_CASE("dry mix is exact and reset removes captured audio including freeze") {
  BeadsInsert fx(44100);
  float p[8]; defaults(p); p[7] = 0;
  float l[73], r[73];
  std::fill_n(l, 73, .3f); std::fill_n(r, 73, -.2f);
  fx.process(l, r, 73, p);
  for (int i = 0; i < 73; ++i) { CHECK(l[i] == .3f); CHECK(r[i] == -.2f); }
  p[7] = 1;
  render(fx, 44100, 88200, p);
  fx.reset();
  p[6] = int(p[6]) | 128;
  auto silence = render(fx, 44100, 44100, p, 127, 0);
  CHECK(power(silence) == 0);
}
TEST_CASE("full parameter extremes and delay-mode transitions stay finite") {
  BeadsInsert fx(48000);
  float p[8]; defaults(p);
  for (int control : {0, 1, 2, 3, 4, 5, 6}) {
    for (int value : {0, 128, 255}) {
      CAPTURE(control); CAPTURE(value);
      p[control] = insertMap(insertBeads, control, value);
      auto audio = render(fx, 48000, 96000, p);
      CHECK(std::all_of(audio.begin(), audio.end(), [](float x) { return std::isfinite(x) && std::abs(x) < 8; }));
    }
  }
}
}
