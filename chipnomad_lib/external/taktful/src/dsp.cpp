#include <taktful/dsp.h>
#include "simd.h"

#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace taktful {

void make_window(WindowType type, float* dst, uint32_t n) {
    if (n == 0) {
        return;
    }
    const float nm1 = static_cast<float>(n - 1);
    for (uint32_t i = 0; i < n; ++i) {
        const float x = static_cast<float>(i) / nm1;
        switch (type) {
        case WindowType::Hann:
            dst[i] = 0.5f * (1.0f - std::cos(static_cast<float>(2.0 * M_PI) * x));
            break;
        case WindowType::Hamming:
            dst[i] = 0.54f - 0.46f * std::cos(static_cast<float>(2.0 * M_PI) * x);
            break;
        case WindowType::BlackmanHarris:
            dst[i] = 0.35875f
                   - 0.48829f * std::cos(static_cast<float>(2.0 * M_PI) * x)
                   + 0.14128f * std::cos(static_cast<float>(4.0 * M_PI) * x)
                   - 0.01168f * std::cos(static_cast<float>(6.0 * M_PI) * x);
            break;
        }
    }
}

void apply_window(float* x, const float* w, uint32_t n) {
    uint32_t i = 0;
#if TAKTFUL_NEON
    for (; i + 4 <= n; i += 4) {
        const float32x4_t xv = vld1q_f32(x + i);
        const float32x4_t wv = vld1q_f32(w + i);
        vst1q_f32(x + i, vmulq_f32(xv, wv));
    }
#endif
    for (; i < n; ++i) {
        x[i] *= w[i];
    }
}

StftFrames compute_stft(const float* samples,
                        uint32_t n_samples,
                        uint32_t sample_rate,
                        uint32_t fft_size,
                        uint32_t hop_size,
                        WindowType window) {
    StftFrames out;
    out.fft_size = fft_size;
    out.hop_size = hop_size;
    out.sample_rate = sample_rate;
    if (!samples || n_samples == 0 || !is_power_of_two(fft_size) || hop_size == 0) {
        return out;
    }

    std::vector<float> win(fft_size);
    make_window(window, win.data(), fft_size);

    const uint32_t bins = fft_size / 2 + 1;
    std::vector<float> frame(fft_size);
    std::vector<float> mag(bins);

    uint32_t n_frames = 0;
    if (n_samples + fft_size > hop_size) {
        n_frames = (n_samples + hop_size - 1) / hop_size;
    }
    out.mag.reserve(n_frames);

    for (uint32_t start = 0; start < n_samples; start += hop_size) {
        std::memset(frame.data(), 0, fft_size * sizeof(float));
        const uint32_t avail = n_samples - start;
        const uint32_t copy_n = avail < fft_size ? avail : fft_size;
        std::memcpy(frame.data(), samples + start, copy_n * sizeof(float));
        apply_window(frame.data(), win.data(), fft_size);
        rfft_mag(frame.data(), mag.data(), fft_size);
        out.mag.push_back(mag);
    }
    return out;
}

void downsample_linear(const float* in,
                       uint32_t n_in,
                       uint32_t sr_in,
                       uint32_t sr_out,
                       std::vector<float>& out) {
    out.clear();
    if (!in || n_in == 0 || sr_in == 0 || sr_out == 0) {
        return;
    }
    if (sr_out >= sr_in) {
        out.assign(in, in + n_in);
        return;
    }
    const double ratio = static_cast<double>(sr_in) / static_cast<double>(sr_out);
    const uint32_t n_out = static_cast<uint32_t>(std::floor(static_cast<double>(n_in) / ratio));
    out.resize(n_out);
    for (uint32_t i = 0; i < n_out; ++i) {
        const double src = static_cast<double>(i) * ratio;
        const uint32_t i0 = static_cast<uint32_t>(src);
        const uint32_t i1 = (i0 + 1 < n_in) ? (i0 + 1) : i0;
        const float t = static_cast<float>(src - static_cast<double>(i0));
        out[i] = in[i0] * (1.0f - t) + in[i1] * t;
    }
}

float hz_to_midi(float hz, float ref_freq) {
    if (hz <= 0.0f || ref_freq <= 0.0f) {
        return 0.0f;
    }
    return 69.0f + 12.0f * std::log2(hz / ref_freq);
}

int midi_to_pitch_class(float midi) {
    int n = static_cast<int>(std::lround(midi));
    n %= 12;
    if (n < 0) {
        n += 12;
    }
    return n;
}

float signal_rms(const float* x, uint32_t n) {
    if (!x || n == 0) {
        return 0.0f;
    }
    uint32_t i = 0;
    double acc = 0.0;
#if TAKTFUL_NEON
    float32x4_t vacc = vdupq_n_f32(0.0f);
    for (; i + 4 <= n; i += 4) {
        const float32x4_t v = vld1q_f32(x + i);
        vacc = vmlaq_f32(vacc, v, v);
    }
    float tmp[4];
    vst1q_f32(tmp, vacc);
    acc = static_cast<double>(tmp[0]) + static_cast<double>(tmp[1])
        + static_cast<double>(tmp[2]) + static_cast<double>(tmp[3]);
#endif
    for (; i < n; ++i) {
        acc += static_cast<double>(x[i]) * static_cast<double>(x[i]);
    }
    return static_cast<float>(std::sqrt(acc / static_cast<double>(n)));
}

} // namespace taktful
