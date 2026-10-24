#include <taktful/onset.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace taktful {

std::vector<float> compute_onset_envelope(const std::vector<std::vector<float> >& mag_frames,
                                          bool hfc_weight) {
    const size_t n_frames = mag_frames.size();
    std::vector<float> env(n_frames, 0.0f);
    if (n_frames == 0) {
        return env;
    }

    const size_t n_bins = mag_frames[0].size();
    std::vector<float> prev(n_bins, 0.0f);
    const float eps = 1.0e-6f;

    for (size_t t = 0; t < n_frames; ++t) {
        const std::vector<float>& mag = mag_frames[t];
        const size_t bins = mag.size() < n_bins ? mag.size() : n_bins;
        float sf = 0.0f;
        for (size_t k = 0; k < bins; ++k) {
            const float m = 20.0f * std::log10(mag[k] + eps);
            const float d = m - prev[k];
            if (d > 0.0f) {
                float w = 1.0f;
                if (hfc_weight && n_bins > 1) {
                    w = static_cast<float>(k) / static_cast<float>(n_bins - 1);
                }
                sf += d * w;
            }
            prev[k] = m;
        }
        env[t] = sf;
    }

    if (n_frames >= 3) {
        std::vector<float> sm(n_frames);
        sm[0] = (env[0] + env[1]) * 0.5f;
        sm[n_frames - 1] = (env[n_frames - 2] + env[n_frames - 1]) * 0.5f;
        for (size_t i = 1; i + 1 < n_frames; ++i) {
            sm[i] = (env[i - 1] + env[i] + env[i + 1]) / 3.0f;
        }
        env.swap(sm);
    }
    return env;
}

} // namespace taktful
