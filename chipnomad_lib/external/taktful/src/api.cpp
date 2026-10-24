#include <taktful/api.h>
#include <taktful/chroma.h>
#include <taktful/dsp.h>
#include <taktful/key.h>
#include <taktful/onset.h>
#include <taktful/tempo.h>

namespace taktful {

bool validate_params(Params& params) {
    if (!is_power_of_two(params.fft_size) ||
        params.fft_size < 256 || params.fft_size > 16384) {
        params.fft_size = 4096;
    }
    if (params.hop_size == 0 || params.hop_size > params.fft_size) {
        params.hop_size = params.fft_size / 2;
    }
    if (params.target_sample_rate == 0) {
        params.target_sample_rate = 11025;
    }
    if (params.tempo_min_bpm < 20.0f) {
        params.tempo_min_bpm = 20.0f;
    }
    if (params.tempo_max_bpm <= params.tempo_min_bpm) {
        params.tempo_max_bpm = params.tempo_min_bpm + 40.0f;
    }
    if (params.chroma_ref_freq <= 0.0f) {
        params.chroma_ref_freq = 440.0f;
    }
    if (params.num_alternatives == 0) {
        params.num_alternatives = 5;
    }
    if (params.fast_mode) {
        params.enable_dp_refinement = false;
        params.apply_pre_filter = false;
    }
    if (params.chroma_method == Params::ChromaMethod::NaivePcp ||
        params.chroma_method == Params::ChromaMethod::Clp) {
        params.use_hpcp = false;
    } else if (!params.use_hpcp) {
        params.chroma_method = Params::ChromaMethod::NaivePcp;
    }
    return true;
}

namespace {

struct Prepared {
    std::vector<float> samples;
    uint32_t sample_rate = 0;
    StftFrames stft;
};

Prepared prepare(const WavData& audio, Params params) {
    validate_params(params);
    Prepared p;
    if (audio.samples.empty() || audio.sample_rate == 0) {
        return p;
    }
    if (audio.sample_rate != params.target_sample_rate) {
        downsample_linear(audio.samples.data(),
                          static_cast<uint32_t>(audio.samples.size()),
                          audio.sample_rate,
                          params.target_sample_rate,
                          p.samples);
        p.sample_rate = params.target_sample_rate;
    } else {
        p.samples = audio.samples;
        p.sample_rate = audio.sample_rate;
    }
    p.stft = compute_stft(p.samples.data(),
                          static_cast<uint32_t>(p.samples.size()),
                          p.sample_rate,
                          params.fft_size,
                          params.hop_size,
                          WindowType::Hann);
    return p;
}

} // namespace

AnalysisResult analyze(const WavData& audio, const Params& params_in) {
    Params params = params_in;
    validate_params(params);
    AnalysisResult result;
    if (audio.samples.empty() || audio.sample_rate == 0) {
        return result;
    }
    if (signal_rms(audio.samples.data(), static_cast<uint32_t>(audio.samples.size())) < 1.0e-6f) {
        return result;
    }

    Prepared p = prepare(audio, params);
    if (p.stft.mag.empty()) {
        return result;
    }

    const bool hfc = (params.onset_method == Params::OnsetMethod::SpectralFluxHFC);
    const std::vector<float> onset = compute_onset_envelope(p.stft.mag, hfc);
    const float hop_sec = static_cast<float>(params.hop_size) / static_cast<float>(p.sample_rate);
    result.tempo = estimate_tempo(onset, hop_sec, params);

    const std::vector<std::vector<float> > chroma =
        compute_chroma(p.stft.mag, params.fft_size, p.sample_rate, params);
    const std::vector<float> pcp = aggregate_pcp(chroma);
    result.key = estimate_key(pcp, params);
    return result;
}

IntermediateFeatures compute_features(const WavData& audio, const Params& params_in) {
    Params params = params_in;
    validate_params(params);
    IntermediateFeatures feat;
    if (audio.samples.empty() || audio.sample_rate == 0) {
        return feat;
    }
    Prepared p = prepare(audio, params);
    const bool hfc = (params.onset_method == Params::OnsetMethod::SpectralFluxHFC);
    feat.onset_envelope = compute_onset_envelope(p.stft.mag, hfc);
    feat.chroma = compute_chroma(p.stft.mag, params.fft_size, p.sample_rate, params);
    return feat;
}

} // namespace taktful
