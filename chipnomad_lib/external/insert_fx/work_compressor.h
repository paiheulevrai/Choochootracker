#pragma once
#include <algorithm>
#include <cmath>
// Extracted Work compressor. Native log-domain detector/SVF, with 8-bit
// mappings, rate-aware cached coefficients. Copyright 2026 Tim Cox; LICENSE-Work.
struct WorkCompressor {
  float rate = 48000, env = 0, ic1 = 0, ic2 = 0, thr = 0, atk = 0, rel = 0, mup = 1, ratio = 4,
        mix = 1;
  float a1 = 1, a2 = 0, a3 = 0, k = 1.414427f;
  int source = 0, filter = 0;
  void set(const float* p) {
    thr = p[0];
    atk = 1 - expf(-6.28318530718f * p[1] / rate);
    rel = 1 - expf(-6.28318530718f * p[2] / rate);
    mup = powf(10, p[3] / 20);
    static const float ratios[] = {1.5f, 2, 3, 4, 6, 8, 16, 20};
    ratio = ratios[(int)p[4]];
    source = p[5];
    mix = p[7];
    filter = p[6] < -0.02f ? -1 : p[6] > 0.02f ? 1 : 0;
    float fc = filter < 0 ? 60 * powf(66, 1 + p[6]) : 40 * powf(75, p[6]);
    fc = std::max(10.0f, std::min(rate * 0.45f, fc));
    float g = tanf(3.14159265359f * fc / rate);
    k = 1 / 0.707f;
    a1 = 1 / (1 + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
  }
  void process(float* l, float* r, int n, const float* sidechain = nullptr) {
    for (int i = 0; i < n; ++i) {
      float sc = source >= 3 ? (sidechain ? sidechain[i] : 0.0f) :
                 source == 1 ? l[i] : source == 2 ? r[i] : (l[i] + r[i]) * 0.5f;
      if (filter) {
        float v3 = sc - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1;
        ic2 = 2 * v2 - ic2;
        sc = filter < 0 ? v2 : sc - k * v1 - v2;
      }
      float level = fabsf(sc);
      env += (level > env ? atk : rel) * (level - env);
      float over = 20 * log10f(std::max(env, 1e-6f)) - thr;
      float g = powf(10, (over > 0 ? over * (1 / ratio - 1) : 0) / 20) * mup;
      l[i] = l[i] * (1 - mix) + (l[i] * g) * mix;
      r[i] = r[i] * (1 - mix) + (r[i] * g) * mix;
    }
  }
};
