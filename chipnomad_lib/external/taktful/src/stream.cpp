#include <taktful/stream.h>
#include <taktful/chroma.h>
#include <taktful/dsp.h>
#include <taktful/key.h>
#include <taktful/onset.h>
#include <taktful/tempo.h>

#include <cmath>
#include <cstddef>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace taktful {

StreamAnalyzer::StreamAnalyzer(const Params& params)
    : params_(params)
    , input_sr_(0)
    , analysis_sr_(0)
    , pending_pos_(0)
    , have_prev_(false)
    , finished_(false) {
    validate_params(params_);
    analysis_sr_ = params_.target_sample_rate;
}

void StreamAnalyzer::reset() {
    in_hold_.clear();
    pending_.clear();
    pending_pos_ = 0;
    prev_db_.clear();
    onset_.clear();
    chroma_.clear();
    have_prev_ = false;
    finished_ = false;
    input_sr_ = 0;
}

uint32_t StreamAnalyzer::frames_processed() const {
    return static_cast<uint32_t>(onset_.size());
}

uint32_t StreamAnalyzer::sample_rate() const {
    return analysis_sr_;
}

void StreamAnalyzer::ensure_window() {
    if (window_.size() == params_.fft_size) {
        return;
    }
    window_.assign(params_.fft_size, 0.0f);
    make_window(WindowType::Hann, window_.data(), params_.fft_size);
}

void StreamAnalyzer::process_pending() {
    ensure_window();
    const uint32_t fft = params_.fft_size;
    const uint32_t hop = params_.hop_size;
    const uint32_t bins = fft / 2 + 1;
    if (prev_db_.size() != bins) {
        prev_db_.assign(bins, 0.0f);
        have_prev_ = false;
    }
    std::vector<float> frame(fft);
    std::vector<float> mag(bins);
    const bool hfc = (params_.onset_method == Params::OnsetMethod::SpectralFluxHFC);
    const float eps = 1.0e-6f;

    while (pending_.size() - pending_pos_ >= fft) {
        std::memcpy(frame.data(), pending_.data() + pending_pos_, fft * sizeof(float));
        apply_window(frame.data(), window_.data(), fft);
        rfft_mag(frame.data(), mag.data(), fft);

        float sf = 0.0f;
        for (uint32_t k = 0; k < bins; ++k) {
            const float m = 20.0f * std::log10(mag[k] + eps);
            if (have_prev_) {
                const float d = m - prev_db_[k];
                if (d > 0.0f) {
                    float w = 1.0f;
                    if (hfc && bins > 1) {
                        w = static_cast<float>(k) / static_cast<float>(bins - 1);
                    }
                    sf += d * w;
                }
            }
            prev_db_[k] = m;
        }
        have_prev_ = true;
        onset_.push_back(sf);

        std::vector<std::vector<float> > one(1, mag);
        std::vector<std::vector<float> > ch = compute_chroma(one, fft, analysis_sr_, params_);
        if (!ch.empty()) {
            chroma_.push_back(ch[0]);
        }

        pending_pos_ += hop;
        if (pending_pos_ > 8192 && pending_pos_ * 2 > pending_.size()) {
            pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(pending_pos_));
            pending_pos_ = 0;
        }
    }
}

void StreamAnalyzer::push(const float* samples, uint32_t n, uint32_t sample_rate) {
    if (!samples || n == 0 || sample_rate == 0 || finished_) {
        return;
    }
    if (input_sr_ == 0) {
        input_sr_ = sample_rate;
    }
    if (sample_rate != input_sr_) {
        return;
    }

    if (input_sr_ == analysis_sr_) {
        pending_.insert(pending_.end(), samples, samples + n);
    } else if (input_sr_ < analysis_sr_) {
        pending_.insert(pending_.end(), samples, samples + n);
        analysis_sr_ = input_sr_;
    } else {
        const double ratio = static_cast<double>(input_sr_) / static_cast<double>(analysis_sr_);
        in_hold_.insert(in_hold_.end(), samples, samples + n);
        if (in_hold_.size() < 2) {
            return;
        }
        const uint32_t max_out = static_cast<uint32_t>(
            std::floor(static_cast<double>(in_hold_.size() - 1) / ratio));
        const size_t old = pending_.size();
        pending_.resize(old + max_out);
        for (uint32_t i = 0; i < max_out; ++i) {
            const double src = static_cast<double>(i) * ratio;
            const uint32_t i0 = static_cast<uint32_t>(src);
            const uint32_t i1 = i0 + 1;
            const float t = static_cast<float>(src - static_cast<double>(i0));
            pending_[old + i] = in_hold_[i0] * (1.0f - t) + in_hold_[i1] * t;
        }
        const uint32_t consumed = static_cast<uint32_t>(std::floor(static_cast<double>(max_out) * ratio));
        if (consumed > 0 && consumed < in_hold_.size()) {
            in_hold_.erase(in_hold_.begin(), in_hold_.begin() + static_cast<std::ptrdiff_t>(consumed));
        } else if (consumed >= in_hold_.size()) {
            in_hold_.clear();
        }
    }
    process_pending();
}

AnalysisResult StreamAnalyzer::finish() {
    finished_ = true;
    if (pending_.size() > pending_pos_ &&
        pending_.size() - pending_pos_ < params_.fft_size) {
        pending_.resize(pending_pos_ + params_.fft_size, 0.0f);
        process_pending();
    }

    AnalysisResult result;
    if (onset_.size() < 2) {
        return result;
    }
    if (onset_.size() >= 3) {
        std::vector<float> sm(onset_.size());
        sm[0] = (onset_[0] + onset_[1]) * 0.5f;
        sm[onset_.size() - 1] = (onset_[onset_.size() - 2] + onset_[onset_.size() - 1]) * 0.5f;
        for (size_t i = 1; i + 1 < onset_.size(); ++i) {
            sm[i] = (onset_[i - 1] + onset_[i] + onset_[i + 1]) / 3.0f;
        }
        onset_.swap(sm);
    }
    const float hop_sec = static_cast<float>(params_.hop_size) / static_cast<float>(analysis_sr_);
    result.tempo = estimate_tempo(onset_, hop_sec, params_);
    const std::vector<float> pcp = aggregate_pcp(chroma_);
    result.key = estimate_key(pcp, params_);
    return result;
}

IntermediateFeatures StreamAnalyzer::features() const {
    IntermediateFeatures f;
    f.onset_envelope = onset_;
    f.chroma = chroma_;
    return f;
}

AnalysisResult analyze_buffer(const float* samples,
                              uint32_t n_samples,
                              uint32_t sample_rate,
                              const Params& params) {
    WavData w;
    w.sample_rate = sample_rate;
    w.channels = 1;
    w.bits_per_sample = 32;
    if (samples && n_samples) {
        w.samples.assign(samples, samples + n_samples);
    }
    return analyze(w, params);
}

std::vector<AnalysisResult> analyze_sections(const WavData& audio,
                                             float section_sec,
                                             const Params& params) {
    std::vector<AnalysisResult> out;
    if (audio.samples.empty() || audio.sample_rate == 0 || section_sec <= 0.0f) {
        return out;
    }
    const uint32_t sec_n = static_cast<uint32_t>(section_sec * static_cast<float>(audio.sample_rate));
    if (sec_n == 0) {
        return out;
    }
    for (uint32_t start = 0; start < audio.samples.size(); start += sec_n) {
        uint32_t n = sec_n;
        if (start + n > audio.samples.size()) {
            n = static_cast<uint32_t>(audio.samples.size() - start);
        }
        WavData slice;
        slice.sample_rate = audio.sample_rate;
        slice.channels = audio.channels;
        slice.bits_per_sample = audio.bits_per_sample;
        slice.samples.assign(audio.samples.begin() + static_cast<std::ptrdiff_t>(start),
                             audio.samples.begin() + static_cast<std::ptrdiff_t>(start + n));
        out.push_back(analyze(slice, params));
    }
    return out;
}

} // namespace taktful
