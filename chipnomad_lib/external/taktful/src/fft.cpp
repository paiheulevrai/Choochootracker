#include <taktful/dsp.h>
#include "simd.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace taktful {
namespace {

uint32_t reverse_bits(uint32_t x, uint32_t bits) {
    uint32_t y = 0;
    for (uint32_t i = 0; i < bits; ++i) {
        y = (y << 1) | (x & 1u);
        x >>= 1;
    }
    return y;
}

uint32_t log2u(uint32_t n) {
    uint32_t b = 0;
    while (n > 1) {
        n >>= 1;
        ++b;
    }
    return b;
}

struct FftPlan {
    uint32_t n;
    uint32_t bits;
    std::vector<uint32_t> rev;
    std::vector<float> wr;
    std::vector<float> wi;
    std::vector<float> re;
    std::vector<float> im;
};

FftPlan& plan_for(uint32_t n) {
    static thread_local FftPlan plan = {
        0, 0,
        std::vector<uint32_t>(),
        std::vector<float>(),
        std::vector<float>(),
        std::vector<float>(),
        std::vector<float>()
    };
    if (plan.n != n) {
        plan.n = n;
        plan.bits = log2u(n);
        plan.rev.resize(n);
        for (uint32_t i = 0; i < n; ++i) {
            plan.rev[i] = reverse_bits(i, plan.bits);
        }
        plan.wr.resize(n / 2);
        plan.wi.resize(n / 2);
        for (uint32_t k = 0; k < n / 2; ++k) {
            const double ang = -2.0 * M_PI * static_cast<double>(k) / static_cast<double>(n);
            plan.wr[k] = static_cast<float>(std::cos(ang));
            plan.wi[k] = static_cast<float>(std::sin(ang));
        }
        plan.re.resize(n);
        plan.im.resize(n);
    }
    return plan;
}

void bit_reverse(float* re, float* im, const uint32_t* rev, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t j = rev[i];
        if (j > i) {
            const float tr = re[i];
            re[i] = re[j];
            re[j] = tr;
            const float ti = im[i];
            im[i] = im[j];
            im[j] = ti;
        }
    }
}

void butterfly_scalar(float* re,
                      float* im,
                      const float* wr,
                      const float* wi,
                      uint32_t n,
                      uint32_t len) {
    const uint32_t half = len / 2;
    const uint32_t step = n / len;
    for (uint32_t i = 0; i < n; i += len) {
        uint32_t tw_idx = 0;
        for (uint32_t j = 0; j < half; ++j) {
            const float w_re = wr[tw_idx];
            const float w_im = wi[tw_idx];
            const uint32_t u = i + j;
            const uint32_t v = u + half;
            const float tr = w_re * re[v] - w_im * im[v];
            const float ti = w_re * im[v] + w_im * re[v];
            re[v] = re[u] - tr;
            im[v] = im[u] - ti;
            re[u] = re[u] + tr;
            im[u] = im[u] + ti;
            tw_idx += step;
        }
    }
}

#if TAKTFUL_NEON
void butterfly_neon(float* re,
                    float* im,
                    const float* wr,
                    const float* wi,
                    uint32_t n,
                    uint32_t len) {
    const uint32_t half = len / 2;
    const uint32_t step = n / len;
    for (uint32_t i = 0; i < n; i += len) {
        uint32_t tw_idx = 0;
        uint32_t j = 0;
        for (; j + 4 <= half; j += 4) {
            float32x4_t w_re;
            float32x4_t w_im;
            if (step == 1) {
                w_re = vld1q_f32(wr + tw_idx);
                w_im = vld1q_f32(wi + tw_idx);
            } else {
                const float wr4[4] = {
                    wr[tw_idx],
                    wr[tw_idx + step],
                    wr[tw_idx + 2u * step],
                    wr[tw_idx + 3u * step]
                };
                const float wi4[4] = {
                    wi[tw_idx],
                    wi[tw_idx + step],
                    wi[tw_idx + 2u * step],
                    wi[tw_idx + 3u * step]
                };
                w_re = vld1q_f32(wr4);
                w_im = vld1q_f32(wi4);
            }
            const uint32_t u = i + j;
            const uint32_t v = u + half;
            const float32x4_t re_u = vld1q_f32(re + u);
            const float32x4_t im_u = vld1q_f32(im + u);
            const float32x4_t re_v = vld1q_f32(re + v);
            const float32x4_t im_v = vld1q_f32(im + v);
            const float32x4_t tr = vmlsq_f32(vmulq_f32(w_re, re_v), w_im, im_v);
            const float32x4_t ti = vmlaq_f32(vmulq_f32(w_re, im_v), w_im, re_v);
            vst1q_f32(re + v, vsubq_f32(re_u, tr));
            vst1q_f32(im + v, vsubq_f32(im_u, ti));
            vst1q_f32(re + u, vaddq_f32(re_u, tr));
            vst1q_f32(im + u, vaddq_f32(im_u, ti));
            tw_idx += 4u * step;
        }
        for (; j < half; ++j) {
            const float w_re = wr[tw_idx];
            const float w_im = wi[tw_idx];
            const uint32_t u = i + j;
            const uint32_t v = u + half;
            const float tr = w_re * re[v] - w_im * im[v];
            const float ti = w_re * im[v] + w_im * re[v];
            re[v] = re[u] - tr;
            im[v] = im[u] - ti;
            re[u] = re[u] + tr;
            im[u] = im[u] + ti;
            tw_idx += step;
        }
    }
}
#endif

#if TAKTFUL_NEON
float32x4_t vsqrtq_f32_compat(float32x4_t s) {
#if defined(__aarch64__)
    return vsqrtq_f32(s);
#else
    float32x4_t rec = vrsqrteq_f32(s);
    rec = vmulq_f32(vrsqrtsq_f32(vmulq_f32(s, rec), rec), rec);
    rec = vmulq_f32(vrsqrtsq_f32(vmulq_f32(s, rec), rec), rec);
    const uint32x4_t nz = vcgtq_f32(s, vdupq_n_f32(0.0f));
    return vbslq_f32(nz, vmulq_f32(s, rec), vdupq_n_f32(0.0f));
#endif
}
#endif

void mag_spectrum(const float* re, const float* im, float* mag_out, uint32_t bins) {
    uint32_t k = 0;
#if TAKTFUL_NEON
    for (; k + 4 <= bins; k += 4) {
        const float32x4_t r = vld1q_f32(re + k);
        const float32x4_t m = vld1q_f32(im + k);
        const float32x4_t s = vmlaq_f32(vmulq_f32(r, r), m, m);
        vst1q_f32(mag_out + k, vsqrtq_f32_compat(s));
    }
#endif
    for (; k < bins; ++k) {
        mag_out[k] = std::sqrt(re[k] * re[k] + im[k] * im[k]);
    }
}

void cfft_inplace(float* re, float* im, const FftPlan& plan) {
    const uint32_t n = plan.n;
    bit_reverse(re, im, plan.rev.data(), n);
    for (uint32_t len = 2; len <= n; len <<= 1) {
#if TAKTFUL_NEON
        if (len >= 8) {
            butterfly_neon(re, im, plan.wr.data(), plan.wi.data(), n, len);
            continue;
        }
#endif
        butterfly_scalar(re, im, plan.wr.data(), plan.wi.data(), n, len);
    }
}

} // namespace

bool is_power_of_two(uint32_t n) {
    return n >= 2 && (n & (n - 1)) == 0;
}

void rfft_mag(const float* in, float* mag_out, uint32_t n) {
    FftPlan& plan = plan_for(n);
    std::memcpy(plan.re.data(), in, n * sizeof(float));
    std::memset(plan.im.data(), 0, n * sizeof(float));
    cfft_inplace(plan.re.data(), plan.im.data(), plan);
    mag_spectrum(plan.re.data(), plan.im.data(), mag_out, n / 2 + 1);
}

} // namespace taktful
