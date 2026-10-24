#include <taktful/chroma.h>
#include <taktful/dsp.h>

#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace taktful {
namespace {

void spectral_whiten(std::vector<float>& mag, uint32_t fft_size, uint32_t sample_rate) {
    const size_t n = mag.size();
    if (n < 8 || sample_rate == 0 || fft_size == 0) {
        return;
    }
    std::vector<float> sm(n, 0.0f);
    for (size_t k = 1; k < n; ++k) {
        const float f = static_cast<float>(k) * static_cast<float>(sample_rate) / static_cast<float>(fft_size);
        const float lo = f / std::pow(2.0f, 1.0f / 6.0f);
        const float hi = f * std::pow(2.0f, 1.0f / 6.0f);
        const int k0 = std::max(1, static_cast<int>(std::floor(lo * static_cast<float>(fft_size) / static_cast<float>(sample_rate))));
        const int k1 = std::min(static_cast<int>(n) - 1,
                                static_cast<int>(std::ceil(hi * static_cast<float>(fft_size) / static_cast<float>(sample_rate))));
        float acc = 0.0f;
        int cnt = 0;
        for (int i = k0; i <= k1; ++i) {
            acc += mag[static_cast<size_t>(i)];
            ++cnt;
        }
        sm[k] = (cnt > 0) ? (acc / static_cast<float>(cnt)) : mag[k];
    }
    for (size_t k = 1; k < n; ++k) {
        mag[k] /= (sm[k] + 1.0e-8f);
    }
}

void l2_normalize(std::vector<float>& v) {
    double acc = 0.0;
    for (size_t i = 0; i < v.size(); ++i) {
        acc += static_cast<double>(v[i]) * static_cast<double>(v[i]);
    }
    const float nrm = static_cast<float>(std::sqrt(acc));
    if (nrm > 1.0e-12f) {
        for (size_t i = 0; i < v.size(); ++i) {
            v[i] /= nrm;
        }
    }
}

std::vector<float> naive_pcp_frame(const std::vector<float>& mag,
                                   uint32_t fft_size,
                                   uint32_t sample_rate,
                                   float ref_freq) {
    std::vector<float> chroma(12, 0.0f);
    const size_t n = mag.size();
    for (size_t k = 1; k < n; ++k) {
        const float f = static_cast<float>(k) * static_cast<float>(sample_rate) / static_cast<float>(fft_size);
        if (f < 50.0f || f > 5000.0f) {
            continue;
        }
        const float midi = hz_to_midi(f, ref_freq);
        const int pc = midi_to_pitch_class(midi);
        chroma[static_cast<size_t>(pc)] += mag[k];
    }
    l2_normalize(chroma);
    return chroma;
}

std::vector<float> clp_frame(const std::vector<float>& mag,
                             uint32_t fft_size,
                             uint32_t sample_rate,
                             float ref_freq) {
    std::vector<float> chroma(12, 0.0f);
    const size_t n = mag.size();
    for (size_t k = 1; k < n; ++k) {
        const float f = static_cast<float>(k) * static_cast<float>(sample_rate) / static_cast<float>(fft_size);
        if (f < 50.0f || f > 5000.0f) {
            continue;
        }
        const float midi = hz_to_midi(f, ref_freq);
        const int pc = midi_to_pitch_class(midi);
        chroma[static_cast<size_t>(pc)] += std::log1p(mag[k]);
    }
    l2_normalize(chroma);
    return chroma;
}

std::vector<float> hpcp_frame(const std::vector<float>& mag_in,
                              uint32_t fft_size,
                              uint32_t sample_rate,
                              const Params& params) {
    std::vector<float> mag = mag_in;
    if (params.apply_pre_filter && !params.fast_mode) {
        spectral_whiten(mag, fft_size, sample_rate);
    }

    std::vector<float> chroma(12, 0.0f);
    const int n = static_cast<int>(mag.size());
    const float thresh = params.hpcp_peak_threshold;
    const int n_harm = 8;
    for (int k = 2; k + 2 < n; ++k) {
        const float v = mag[static_cast<size_t>(k)];
        if (v < thresh) {
            continue;
        }
        if (v <= mag[static_cast<size_t>(k - 1)] || v <= mag[static_cast<size_t>(k + 1)] ||
            v <= mag[static_cast<size_t>(k - 2)] || v <= mag[static_cast<size_t>(k + 2)]) {
            continue;
        }
        const float f = static_cast<float>(k) * static_cast<float>(sample_rate) / static_cast<float>(fft_size);
        if (f < 50.0f || f > 5000.0f) {
            continue;
        }
        for (int h = 1; h <= n_harm; ++h) {
            const float f0 = f / static_cast<float>(h);
            if (f0 < 50.0f) {
                break;
            }
            const float midi = hz_to_midi(f0, params.chroma_ref_freq);
            const float midi_r = std::round(midi);
            const float dist = std::fabs(midi - midi_r);
            if (dist > 0.5f) {
                continue;
            }
            const float spread = std::cos(static_cast<float>(M_PI) * dist);
            const float hw = std::pow(0.6f, static_cast<float>(h - 1));
            const int pc = midi_to_pitch_class(midi_r);
            chroma[static_cast<size_t>(pc)] += v * hw * spread * spread;
        }
    }
    l2_normalize(chroma);
    return chroma;
}

void median_filter_chroma(std::vector<std::vector<float> >& chroma, int width) {
    if (chroma.size() < 3 || width < 3) {
        return;
    }
    const int half = width / 2;
    std::vector<std::vector<float> > out = chroma;
    std::vector<float> buf;
    buf.reserve(static_cast<size_t>(width));
    for (int t = 0; t < static_cast<int>(chroma.size()); ++t) {
        for (int p = 0; p < 12; ++p) {
            buf.clear();
            for (int d = -half; d <= half; ++d) {
                int i = t + d;
                if (i < 0) i = 0;
                if (i >= static_cast<int>(chroma.size())) i = static_cast<int>(chroma.size()) - 1;
                buf.push_back(chroma[static_cast<size_t>(i)][static_cast<size_t>(p)]);
            }
            std::sort(buf.begin(), buf.end());
            out[static_cast<size_t>(t)][static_cast<size_t>(p)] = buf[buf.size() / 2];
        }
    }
    chroma.swap(out);
}

} // namespace

std::vector<std::vector<float> > compute_chroma(const std::vector<std::vector<float> >& mag_frames,
                                                uint32_t fft_size,
                                                uint32_t sample_rate,
                                                const Params& params) {
    std::vector<std::vector<float> > chroma;
    chroma.reserve(mag_frames.size());
    Params::ChromaMethod method = params.chroma_method;
    if (!params.use_hpcp && method == Params::ChromaMethod::Hpcp) {
        method = Params::ChromaMethod::NaivePcp;
    }
    for (size_t t = 0; t < mag_frames.size(); ++t) {
        switch (method) {
        case Params::ChromaMethod::Clp:
            chroma.push_back(clp_frame(mag_frames[t], fft_size, sample_rate, params.chroma_ref_freq));
            break;
        case Params::ChromaMethod::NaivePcp:
            chroma.push_back(naive_pcp_frame(mag_frames[t], fft_size, sample_rate, params.chroma_ref_freq));
            break;
        case Params::ChromaMethod::Hpcp:
        default:
            chroma.push_back(hpcp_frame(mag_frames[t], fft_size, sample_rate, params));
            break;
        }
    }
    if (!params.fast_mode) {
        median_filter_chroma(chroma, 5);
    }
    return chroma;
}

std::vector<float> aggregate_pcp(const std::vector<std::vector<float> >& chroma) {
    std::vector<float> pcp(12, 0.0f);
    if (chroma.empty()) {
        return pcp;
    }
    for (size_t t = 0; t < chroma.size(); ++t) {
        const std::vector<float>& c = chroma[t];
        const size_t n = c.size() < 12 ? c.size() : 12;
        for (size_t i = 0; i < n; ++i) {
            pcp[i] += c[i];
        }
    }
    const float inv = 1.0f / static_cast<float>(chroma.size());
    for (size_t i = 0; i < 12; ++i) {
        pcp[i] *= inv;
    }
    return pcp;
}

} // namespace taktful
