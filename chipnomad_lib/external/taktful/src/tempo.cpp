#include <taktful/tempo.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace taktful {
namespace {

std::vector<float> resample_envelope(const std::vector<float>& env, float hop_sec, float dt) {
    std::vector<float> out;
    if (env.empty() || hop_sec <= 0.0f || dt <= 0.0f) {
        return out;
    }
    const float duration = static_cast<float>(env.size()) * hop_sec;
    const int n_out = static_cast<int>(std::floor(duration / dt));
    if (n_out <= 1) {
        return out;
    }
    out.resize(static_cast<size_t>(n_out), 0.0f);
    for (int i = 0; i < n_out; ++i) {
        const float t = static_cast<float>(i) * dt;
        const float src = t / hop_sec;
        const int i0 = static_cast<int>(src);
        const int i1 = (i0 + 1 < static_cast<int>(env.size())) ? (i0 + 1) : i0;
        const float u = src - static_cast<float>(i0);
        const float a = (i0 >= 0 && i0 < static_cast<int>(env.size())) ? env[static_cast<size_t>(i0)] : 0.0f;
        const float b = env[static_cast<size_t>(i1)];
        out[static_cast<size_t>(i)] = a * (1.0f - u) + b * u;
    }
    return out;
}

float mean_of(const std::vector<float>& x) {
    if (x.empty()) {
        return 0.0f;
    }
    double s = 0.0;
    for (size_t i = 0; i < x.size(); ++i) {
        s += static_cast<double>(x[i]);
    }
    return static_cast<float>(s / static_cast<double>(x.size()));
}

float parabolic_tau(const std::vector<float>& r, int tau) {
    if (tau <= 0 || tau + 1 >= static_cast<int>(r.size())) {
        return static_cast<float>(tau);
    }
    const float a = r[static_cast<size_t>(tau - 1)];
    const float b = r[static_cast<size_t>(tau)];
    const float c = r[static_cast<size_t>(tau + 1)];
    const float den = a - 2.0f * b + c;
    if (std::fabs(den) < 1.0e-12f) {
        return static_cast<float>(tau);
    }
    const float delta = 0.5f * (a - c) / den;
    if (delta < -1.0f || delta > 1.0f) {
        return static_cast<float>(tau);
    }
    return static_cast<float>(tau) + delta;
}

TempoResult ellis_dp(const std::vector<float>& onset, float dt, float bpm) {
    TempoResult res;
    res.bpm = bpm;
    const int n = static_cast<int>(onset.size());
    if (n < 8 || dt <= 0.0f || bpm <= 1.0f) {
        return res;
    }

    const float tau_sec = 60.0f / bpm;
    const float tau = tau_sec / dt;
    if (tau < 2.0f) {
        return res;
    }
    const float sigma = 0.5f * tau;
    const float alpha = 100.0f;
    const int max_back = static_cast<int>(std::ceil(2.0f * tau));

    std::vector<float> o = onset;
    float mx = 0.0f;
    for (int i = 0; i < n; ++i) {
        if (o[static_cast<size_t>(i)] > mx) {
            mx = o[static_cast<size_t>(i)];
        }
    }
    if (mx > 0.0f) {
        for (int i = 0; i < n; ++i) {
            o[static_cast<size_t>(i)] /= mx;
        }
    }

    std::vector<float> C(static_cast<size_t>(n), 0.0f);
    std::vector<int> prev(static_cast<size_t>(n), -1);
    for (int t = 0; t < n; ++t) {
        float best = o[static_cast<size_t>(t)];
        int best_p = -1;
        const int t0 = (t > max_back) ? (t - max_back) : 0;
        for (int tp = t0; tp < t; ++tp) {
            const float d = static_cast<float>(t - tp) - tau;
            const float trans = - (d * d) / (2.0f * sigma * sigma);
            const float score = C[static_cast<size_t>(tp)] + alpha * trans;
            if (score > best - o[static_cast<size_t>(t)] || best_p < 0) {
                const float cand = o[static_cast<size_t>(t)] + score;
                if (best_p < 0 || cand > best) {
                    best = cand;
                    best_p = tp;
                }
            }
        }
        C[static_cast<size_t>(t)] = best;
        prev[static_cast<size_t>(t)] = best_p;
    }

    int t_end = 0;
    float c_end = C[0];
    for (int t = 1; t < n; ++t) {
        if (C[static_cast<size_t>(t)] >= c_end) {
            c_end = C[static_cast<size_t>(t)];
            t_end = t;
        }
    }

    std::vector<int> beats_rev;
    for (int t = t_end; t >= 0; t = prev[static_cast<size_t>(t)]) {
        beats_rev.push_back(t);
        if (prev[static_cast<size_t>(t)] < 0) {
            break;
        }
    }
    std::reverse(beats_rev.begin(), beats_rev.end());

    if (beats_rev.size() < 2) {
        return res;
    }

    res.beat_times_sec.reserve(beats_rev.size());
    for (size_t i = 0; i < beats_rev.size(); ++i) {
        res.beat_times_sec.push_back(static_cast<float>(beats_rev[i]) * dt);
    }

    std::vector<float> ibi;
    ibi.reserve(beats_rev.size() - 1);
    for (size_t i = 1; i < beats_rev.size(); ++i) {
        const float d = static_cast<float>(beats_rev[i] - beats_rev[i - 1]) * dt;
        if (d > 0.05f) {
            ibi.push_back(d);
        }
    }
    if (ibi.empty()) {
        return res;
    }
    float mean_ibi = 0.0f;
    for (size_t i = 0; i < ibi.size(); ++i) {
        mean_ibi += ibi[i];
    }
    mean_ibi /= static_cast<float>(ibi.size());
    if (mean_ibi > 0.0f) {
        res.bpm = 60.0f / mean_ibi;
    }
    float var = 0.0f;
    for (size_t i = 0; i < ibi.size(); ++i) {
        const float d = ibi[i] - mean_ibi;
        var += d * d;
    }
    var /= static_cast<float>(ibi.size());
    const float cv = (mean_ibi > 0.0f) ? (std::sqrt(var) / mean_ibi) : 1.0f;
    res.confidence = 1.0f / (1.0f + 4.0f * cv);
    return res;
}

} // namespace

TempoDebug estimate_tempo_debug(const std::vector<float>& onset_env,
                                float hop_sec,
                                const Params& params) {
    TempoDebug dbg;
    if (onset_env.size() < 4 || hop_sec <= 0.0f) {
        return dbg;
    }

    const float dt = 0.01f;
    std::vector<float> e = resample_envelope(onset_env, hop_sec, dt);
    dbg.resampled_onset = e;
    dbg.acorr_dt = dt;
    if (e.size() < 16) {
        return dbg;
    }

    const float mean = mean_of(e);
    for (size_t i = 0; i < e.size(); ++i) {
        e[i] = std::max(0.0f, e[i] - mean);
    }

    const float min_bpm = params.tempo_min_bpm > 1.0f ? params.tempo_min_bpm : 60.0f;
    const float max_bpm = params.tempo_max_bpm > min_bpm ? params.tempo_max_bpm : 200.0f;
    int tau_min = static_cast<int>(std::floor(60.0f / max_bpm / dt));
    int tau_max = static_cast<int>(std::ceil(60.0f / min_bpm / dt));
    if (tau_min < 2) {
        tau_min = 2;
    }
    const int n_e = static_cast<int>(e.size());
    int tau_hi = tau_max + 2;
    if (tau_hi >= n_e) {
        tau_hi = n_e - 1;
    }
    if (tau_max >= tau_hi) {
        tau_max = tau_hi;
    }
    if (tau_max <= tau_min) {
        return dbg;
    }
    dbg.tau_min = tau_min;
    dbg.tau_max = tau_max;

    std::vector<float> r(static_cast<size_t>(tau_hi + 1), 0.0f);
    double r0 = 0.0;
    for (size_t t = 0; t < e.size(); ++t) {
        r0 += static_cast<double>(e[t]) * static_cast<double>(e[t]);
    }
    r[0] = static_cast<float>(r0);
    for (int tau = 1; tau <= tau_hi; ++tau) {
        double acc = 0.0;
        const size_t n = e.size() - static_cast<size_t>(tau);
        for (size_t t = 0; t < n; ++t) {
            acc += static_cast<double>(e[t]) * static_cast<double>(e[t + static_cast<size_t>(tau)]);
        }
        r[static_cast<size_t>(tau)] = static_cast<float>(acc);
    }
    dbg.acorr = r;

    if (r[0] <= 1.0e-12f) {
        return dbg;
    }

    int best_tau = tau_min;
    float best_val = r[static_cast<size_t>(tau_min)];
    for (int tau = tau_min; tau <= tau_max; ++tau) {
        if (r[static_cast<size_t>(tau)] > best_val) {
            best_val = r[static_cast<size_t>(tau)];
            best_tau = tau;
        }
    }

    const float bpm_raw = 60.0f / (static_cast<float>(best_tau) * dt);
    const int twice = best_tau * 2;
    if (bpm_raw > 130.0f && twice <= tau_max) {
        const float v2 = r[static_cast<size_t>(twice)];
        if (v2 >= 0.92f * best_val) {
            best_tau = twice;
            best_val = v2;
        }
    }

    const float tau_hat = parabolic_tau(r, best_tau);
    float bpm = 60.0f / (tau_hat * dt);
    if (bpm > max_bpm && (bpm * 0.5f) >= min_bpm) {
        bpm *= 0.5f;
    } else if (bpm < min_bpm && (bpm * 2.0f) <= max_bpm) {
        bpm *= 2.0f;
    }
    if (bpm < min_bpm) {
        bpm = min_bpm;
    }
    if (bpm > max_bpm) {
        bpm = max_bpm;
    }

    float conf = best_val / r[0];
    if (conf < 0.0f) {
        conf = 0.0f;
    }
    if (conf > 1.0f) {
        conf = 1.0f;
    }

    TempoResult ac;
    ac.bpm = bpm;
    ac.confidence = conf;

    const bool do_dp = params.enable_dp_refinement && !params.fast_mode;
    if (do_dp) {
        TempoResult dp = ellis_dp(e, dt, bpm);
        if (dp.bpm > 0.0f && !dp.beat_times_sec.empty()) {
            if (dp.bpm < min_bpm && dp.bpm * 2.0f <= max_bpm) {
                dp.bpm *= 2.0f;
            }
            if (dp.bpm > max_bpm && dp.bpm * 0.5f >= min_bpm) {
                dp.bpm *= 0.5f;
            }
            ac.bpm = dp.bpm;
            ac.beat_times_sec.swap(dp.beat_times_sec);
            ac.confidence = 0.5f * conf + 0.5f * dp.confidence;
        }
    }

    dbg.result = ac;
    return dbg;
}

TempoResult estimate_tempo(const std::vector<float>& onset_env,
                           float hop_sec,
                           const Params& params) {
    return estimate_tempo_debug(onset_env, hop_sec, params).result;
}

} // namespace taktful
