#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {
constexpr int kValues = 65536;
constexpr int kPasses = 400;
float table[1025];

float pade(float x) {
  const float x2 = x * x;
  float result = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2))) /
    (135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f)));
  return result < -1.0f ? -1.0f : (result > 1.0f ? 1.0f : result);
}

float lut(float x) {
  if (x <= -8.0f) return -1.0f;
  if (x >= 8.0f) return 1.0f;
  float position = (x + 8.0f) * 64.0f;
  int index = static_cast<int>(position);
  float fraction = position - index;
  return table[index] + (table[index + 1] - table[index]) * fraction;
}

template <typename Function>
double run(const std::vector<float>& input, Function function, float* checksum) {
  float sum = 0.0f;
  const auto started = std::chrono::steady_clock::now();
  for (int pass = 0; pass < kPasses; ++pass)
    for (float value : input)
      sum += function(value + sum * 1.0e-9f);
  double elapsed = std::chrono::duration<double>(
    std::chrono::steady_clock::now() - started).count();
  *checksum += sum;
  return elapsed;
}
}  // namespace

int main() {
  for (int i = 0; i <= 1024; ++i) table[i] = std::tanh(i / 64.0f - 8.0f);
  std::vector<float> input(kValues);
  for (int i = 0; i < kValues; ++i)
    input[i] = -4.0f + 8.0f * i / (kValues - 1.0f);

  for (int round = 0; round < 5; ++round) {
    float checksum = 0.0f;
    double system = run(input, [](float x) { return std::tanh(x); }, &checksum);
    double rational = run(input, pade, &checksum);
    double lookup = run(input, lut, &checksum);
    std::printf("round %d: tanhf %.6f, pade %.6f (%+.1f%%), lut %.6f (%+.1f%%), sum %.3f\n",
      round + 1, system, rational, (rational / system - 1.0) * 100.0,
      lookup, (lookup / system - 1.0) * 100.0, checksum);
  }
}
