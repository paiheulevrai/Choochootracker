#include "doctest.h"
#include "project.h"
#include "synth/master_effects.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
constexpr int rate = 8000;
constexpr int repeat = 800;

Project echoProject() {
  Project p;
  projectInit(&p);
  p.delayAlgorithm = 2;
  p.tickRate = 100.0f;
  p.delayTicks = 10;
  p.delayFeedback = 90;
  p.delayReturn = 100;
  p.delayReverbSend = 0;
  p.reverbReturn = 0;
  p.perceptualEffects = false;
  p.delayFilterCutoffHz = 20000;
  p.echoHighpassCutoffHz = 20;
  p.echoDiffuse = 0;
  p.echoAge = 0;
  return p;
}

std::vector<float> render(Project p, const std::vector<float>& input) {
  MasterEffects effects{};
  effects.init(rate);
  std::vector<float> bus(input), output(input.size(), 0.0f);
  effects.process(nullptr, bus.data(), output.data(), bus.size() / 2, &p);
  return output;
}

double energy(const std::vector<float>& audio, int first, int count) {
  double sum = 0;
  for (int i = first; i < first + count; ++i)
    sum += audio[2 * i] * audio[2 * i];
  return sum;
}

double occupancy(const std::vector<float>& audio, int first, int count) {
  double sum = 0, squares = 0;
  for (int i = first; i < first + count; ++i) {
    double x = std::fabs(audio[2 * i]);
    sum += x;
    squares += x * x;
  }
  return squares > 0 ? sum * sum / squares : 0;
}
}

TEST_CASE("Echoverb first repeat preserves stereo and routes wet sends") {
  Project p = echoProject();
  p.echoDiffuse = p.echoAge = 255;
  p.delayReverbSend = 50;
  MasterEffects effects{};
  effects.init(rate);
  std::vector<float> bus(2 * (repeat + 10), 0.0f), output(bus.size(), 0.0f),
                     reverb(bus.size(), 0.0f);
  bus[0] = 0.75f;
  bus[1] = -0.25f;
  effects.process(reverb.data(), bus.data(), output.data(), bus.size() / 2, &p);
  for (int i = 0; i < repeat * 2; ++i) {
    CHECK(output[i] == 0.0f);
    CHECK(reverb[i] == 0.0f);
  }
  CHECK(output[repeat * 2] == doctest::Approx(0.75f));
  CHECK(output[repeat * 2 + 1] == doctest::Approx(-0.25f));
  CHECK(reverb[repeat * 2] == doctest::Approx(0.375f));
  CHECK(reverb[repeat * 2 + 1] == doctest::Approx(-0.125f));
}

TEST_CASE("Echoverb diffusion grows over circulations independently of age") {
  std::vector<float> impulse(2 * repeat * 5, 0.0f);
  impulse[0] = 0.8f;
  Project p = echoProject();
  auto separated = render(p, impulse);
  p.echoDiffuse = 255;
  auto cloud = render(p, impulse);
  p.echoAge = 255;
  auto agedCloud = render(p, impulse);
  p.echoDiffuse = 0;
  auto agedSeparated = render(p, impulse);
  CHECK(cloud[repeat * 2] == doctest::Approx(separated[repeat * 2]));
  CHECK(std::fabs(cloud[repeat * 4]) > 0.1f);
  CHECK(std::fabs(cloud[repeat * 6]) > 0.01f);
  for (size_t i = 1; i < cloud.size(); i += 2)
    CHECK(cloud[i] == 0.0f);
  CHECK(occupancy(cloud, repeat * 2, repeat) > occupancy(separated, repeat * 2, repeat) * 2);
  CHECK(occupancy(cloud, repeat * 3, repeat) > occupancy(cloud, repeat * 2, repeat));
  CHECK(occupancy(agedCloud, repeat * 2, repeat) > occupancy(agedSeparated, repeat * 2, repeat) * 1.5);
  CHECK(energy(agedSeparated, repeat * 2, repeat) > 0.0);
}

TEST_CASE("Echoverb ageing accumulates inside the feedback loop") {
  std::vector<float> burst(2 * repeat * 5, 0.0f);
  for (int i = 0; i < 128; ++i)
    burst[i * 2] = i & 1 ? -0.6f : 0.6f;
  Project p = echoProject();
  auto clean = render(p, burst);
  p.echoAge = 128;
  auto aged = render(p, burst);
  CHECK(energy(aged, repeat, 128) == doctest::Approx(energy(clean, repeat, 128)));
  const double cleanRatio2 = energy(clean, 2 * repeat, 128) / energy(clean, repeat, 128);
  const double agedRatio2 = energy(aged, 2 * repeat, 128) / energy(aged, repeat, 128);
  const double cleanRatio3 = energy(clean, 3 * repeat, 128) / energy(clean, 2 * repeat, 128);
  const double agedRatio3 = energy(aged, 3 * repeat, 128) / energy(aged, 2 * repeat, 128);
  CHECK(agedRatio2 > cleanRatio2 * 0.75);
  CHECK(agedRatio2 < cleanRatio2 * 0.98);
  CHECK(agedRatio3 > cleanRatio3 * 0.75);
  CHECK(agedRatio3 < cleanRatio3 * 0.98);
}

TEST_CASE("Echoverb age changes feedback resolution and saturates without another filter") {
  std::vector<float> impulse(2 * repeat * 5, 0.0f);
  impulse[0] = 0.8f;
  Project p = echoProject();
  auto clean = render(p, impulse);
  p.echoAge = 128;
  auto moderate = render(p, impulse);
  p.echoAge = 255;
  auto aged = render(p, impulse);

  const float lp = 1.0f - std::exp(-2.0f * 3.14159265358979323846f * 0.45f);
  const float hp = std::exp(-2.0f * 3.14159265358979323846f * 20.0f / rate);
  const float filtered = 0.8f * lp * hp;
  int exponent = 0;
  const float mantissa = std::frexp(filtered, &exponent);
  const float scaled = mantissa * 2.0f;
  const float edge = std::max(0.0f, (scaled - (int)scaled - 0.75f) * 4.0f);
  const float quantized = std::ldexp(((int)scaled + edge) / 2.0f, exponent);
  const float crushed = (filtered + quantized) * 0.5f;
  const float compensated = crushed * 0.95f + 0.05f * crushed / (1.0f + 32.0f * crushed);
  CHECK(clean[repeat * 4] == doctest::Approx(filtered * 0.9f));
  CHECK(aged[repeat * 4] == doctest::Approx(0.9f * compensated));
  CHECK(aged[repeat * 2] == doctest::Approx(clean[repeat * 2]));
  CHECK(std::fabs(aged[repeat * 4]) > std::fabs(clean[repeat * 4]) * 0.7f);
  CHECK(std::fabs(aged[repeat * 4]) < std::fabs(clean[repeat * 4]));
  CHECK(std::fabs(moderate[repeat * 4]) > std::fabs(aged[repeat * 4]));
  CHECK(energy(moderate, repeat * 4, repeat) > 0.001);
  const double cleanDecay = energy(clean, repeat * 3, repeat) / energy(clean, repeat * 2, repeat);
  CHECK(energy(aged, repeat * 3, repeat) / energy(aged, repeat * 2, repeat) > cleanDecay * 0.5);

  impulse[0] = 0.2f;
  auto quietAged = render(p, impulse);
  CHECK(std::fabs(aged[repeat * 4] / 0.8f) <
        std::fabs(quietAged[repeat * 4] / 0.2f));
}

TEST_CASE("Echoverb age produces no self-excitation or sustained hiss") {
  Project p = echoProject();
  p.delayFeedback = 95;
  p.echoAge = 255;
  p.echoDiffuse = 255;
  std::vector<float> silence(2 * rate * 32, 0.0f);
  auto empty = render(p, silence);
  CHECK(energy(empty, 0, rate * 32) == 0.0);
  silence[0] = 1.0f;
  auto tail = render(p, silence);
  CHECK(energy(tail, repeat * 2, repeat) > 0.00001);
  CHECK(energy(tail, rate * 31, rate) < 0.0000000001);
}

TEST_CASE("Echoverb AGE retains low-level repeats without feedback gain") {
  for (int age : {128, 255})
  for (float amplitude : {0.8f, 0.02f, 0.0002f}) {
    std::vector<float> burst(2 * repeat * 8, 0.0f);
    for (int n = 0; n < 128; ++n) {
      burst[n * 2] = amplitude * std::sin(n * 2.0f * 3.14159265358979323846f / 16);
      burst[n * 2 + 1] = -burst[n * 2];
    }
    Project p = echoProject();
    auto clean = render(p, burst);
    p.echoAge = age;
    auto aged = render(p, burst);
    CHECK(energy(aged, repeat, 128) == doctest::Approx(energy(clean, repeat, 128)));
    CHECK(energy(aged, repeat * 5, 128) > energy(clean, repeat * 5, 128) * (age == 255 ? 0.1 : 0.3));
    for (int cycle = 2; cycle < 8; ++cycle) {
      CHECK(energy(aged, repeat * cycle, 128) > 0);
      CHECK(energy(aged, repeat * cycle, 128) <= energy(clean, repeat * cycle, 128) * 1.01);
    }
    for (size_t n = 0; n < aged.size(); n += 2) {
      CHECK(std::isfinite(aged[n]));
      CHECK(aged[n] == doctest::Approx(-aged[n + 1]));
    }
  }
}

TEST_CASE("Echoverb extreme AGE adds distortion rather than just reducing volume") {
  std::vector<float> burst(2 * repeat * 4, 0.0f);
  for (int n = 0; n < 128; ++n)
    burst[n * 2] = 0.4f * std::sin(n * 2.0f * 3.14159265358979323846f / 16);
  Project p = echoProject();
  auto clean = render(p, burst);
  p.echoAge = 128;
  auto moderate = render(p, burst);
  p.echoAge = 255;
  auto extreme = render(p, burst);
  const auto distortion = [&](const std::vector<float>& signal) {
    double dot = 0, reference = 0, error = 0;
    for (int n = 2 * repeat; n < 2 * repeat + 128; ++n) {
      dot += signal[n * 2] * clean[n * 2];
      reference += clean[n * 2] * clean[n * 2];
    }
    const double gain = dot / reference;
    for (int n = 2 * repeat; n < 2 * repeat + 128; ++n) {
      const double difference = signal[n * 2] - gain * clean[n * 2];
      error += difference * difference;
    }
    return std::sqrt(error / (gain * gain * reference));
  };
  CHECK(distortion(extreme) > 0.06);
  CHECK(distortion(extreme) > distortion(moderate) * 3);
  CHECK(energy(extreme, 2 * repeat, 128) > energy(clean, 2 * repeat, 128) * 0.5);
  CHECK(energy(extreme, repeat, 128) == doctest::Approx(energy(clean, repeat, 128)));
}

TEST_CASE("Echoverb feedback is bounded and removes DC") {
  Project p = echoProject();
  p.delayFeedback = 95;
  std::vector<float> dc(2 * rate * 8, 0.5f);
  for (int diffuse : {0, 255}) for (int age : {0, 128, 255}) {
    p.echoDiffuse = diffuse;
    p.echoAge = age;
    auto output = render(p, dc);
    double lateMean = 0;
    float peak = 0;
    bool finite = true;
    for (size_t i = 0; i < output.size(); i += 2) {
      finite &= std::isfinite(output[i]);
      peak = std::max(peak, std::fabs(output[i]));
      if (i >= output.size() - 2 * rate) lateMean += output[i] / rate;
    }
    CHECK(finite);
    CHECK(peak < 4.0f);
    CHECK(std::fabs(lateMean - 0.5) < 0.1);
  }
}

TEST_CASE("Echoverb tails survive silence and clear on reset and algorithm change") {
  Project p = echoProject();
  p.echoDiffuse = 255;
  MasterEffects effects{};
  effects.init(rate);
  std::vector<float> bus(2 * repeat, 0.0f), output(bus.size(), 0.0f);
  bus[0] = 1.0f;
  effects.process(nullptr, bus.data(), output.data(), repeat, &p);
  CHECK(effects.delayTailActive());
  std::fill(bus.begin(), bus.end(), 0.0f);
  effects.process(nullptr, bus.data(), output.data(), repeat, &p);
  CHECK(effects.delayTailActive());
  std::fill(output.begin(), output.end(), 0.0f);
  effects.process(nullptr, bus.data(), output.data(), repeat, &p);
  CHECK(energy(output, 0, repeat) > 0.000001);
  effects.reset();
  CHECK_FALSE(effects.delayTailActive());
  std::fill(output.begin(), output.end(), 0.0f);
  effects.process(nullptr, bus.data(), output.data(), repeat, &p);
  CHECK(energy(output, 0, repeat) == 0.0);
  bus[0] = 1.0f;
  effects.process(nullptr, bus.data(), output.data(), repeat, &p);
  std::fill(bus.begin(), bus.end(), 0.0f);
  p.delayAlgorithm = 0;
  effects.process(nullptr, bus.data(), output.data(), repeat, &p);
  p.delayAlgorithm = 2;
  std::fill(output.begin(), output.end(), 0.0f);
  effects.process(nullptr, bus.data(), output.data(), repeat, &p);
  CHECK(energy(output, 0, repeat) == 0.0);
}

TEST_CASE("Echoverb smooths automation identically across block sizes") {
  Project p = echoProject();
  MasterEffects a{}, b{};
  a.init(rate);
  b.init(rate);
  const int count = 5000, change = 2000;
  std::vector<float> input(2 * count), outA(2 * count, 0.0f), outB(2 * count, 0.0f),
                     reverbA(2 * count, 0.0f), reverbB(2 * count, 0.0f);
  for (int i = 0; i < count; ++i) input[2 * i] = input[2 * i + 1] = 0.25f;
  a.process(reverbA.data(), input.data(), outA.data(), change, &p);
  for (int pos = 0; pos < change;) {
    int n = std::min(37, change - pos);
    b.process(reverbB.data() + 2 * pos, input.data() + 2 * pos, outB.data() + 2 * pos, n, &p);
    pos += n;
  }
  p.delayTicks = 12;
  p.delayFeedback = 35;
  p.delayFilterCutoffHz = 1200;
  p.echoHighpassCutoffHz = 500;
  p.delayReturn = 20;
  p.delayReverbSend = 60;
  p.echoDiffuse = 180;
  p.echoAge = 200;
  a.process(reverbA.data() + 2 * change, input.data() + 2 * change,
            outA.data() + 2 * change, count - change, &p);
  for (int pos = change; pos < count;) {
    int n = std::min(37, count - pos);
    b.process(reverbB.data() + 2 * pos, input.data() + 2 * pos, outB.data() + 2 * pos, n, &p);
    pos += n;
  }
  CHECK(std::fabs(outA[2 * change] - outA[2 * (change - 1)]) < 0.02f);
  CHECK(std::fabs(reverbA[2 * change]) < 0.01f);
  for (size_t i = 0; i < outA.size(); ++i) {
    CHECK(outA[i] == doctest::Approx(outB[i]).epsilon(0.00001));
    CHECK(reverbA[i] == doctest::Approx(reverbB[i]).epsilon(0.00001));
  }
}
