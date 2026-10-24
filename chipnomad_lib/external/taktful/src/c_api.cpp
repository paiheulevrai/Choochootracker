#include <taktful/c_api.h>
#include <taktful/api.h>
#include <taktful/stream.h>
#include <taktful/wav_io.h>

#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

namespace {

taktful::Params from_c(const taktful_params* p) {
    taktful::Params o;
    if (!p) {
        return o;
    }
    o.fft_size = p->fft_size ? p->fft_size : o.fft_size;
    o.hop_size = p->hop_size ? p->hop_size : o.hop_size;
    o.target_sample_rate = p->target_sample_rate ? p->target_sample_rate : o.target_sample_rate;
    o.onset_method = (p->onset_method == TAKTFUL_ONSET_SPECTRAL_FLUX)
        ? taktful::Params::OnsetMethod::SpectralFlux
        : taktful::Params::OnsetMethod::SpectralFluxHFC;
    if (p->tempo_min_bpm > 0.0f) o.tempo_min_bpm = p->tempo_min_bpm;
    if (p->tempo_max_bpm > 0.0f) o.tempo_max_bpm = p->tempo_max_bpm;
    o.enable_dp_refinement = p->enable_dp_refinement != 0;
    if (p->chroma_ref_freq > 0.0f) o.chroma_ref_freq = p->chroma_ref_freq;
    switch (p->chroma_method) {
    case TAKTFUL_CHROMA_NAIVE_PCP:
        o.use_hpcp = false;
        o.chroma_method = taktful::Params::ChromaMethod::NaivePcp;
        break;
    case TAKTFUL_CHROMA_CLP:
        o.use_hpcp = true;
        o.chroma_method = taktful::Params::ChromaMethod::Clp;
        break;
    default:
        o.use_hpcp = true;
        o.chroma_method = taktful::Params::ChromaMethod::Hpcp;
        break;
    }
    o.apply_pre_filter = p->apply_pre_filter != 0;
    if (p->hpcp_peak_threshold > 0.0f) o.hpcp_peak_threshold = p->hpcp_peak_threshold;
    switch (p->key_profiles) {
    case TAKTFUL_PROFILES_KRUMHANSL:
        o.key_profiles = taktful::Params::KeyProfiles::KrumhanslOriginal;
        break;
    case TAKTFUL_PROFILES_TEMPERLEY2007:
        o.key_profiles = taktful::Params::KeyProfiles::Temperley2007;
        break;
    default:
        o.key_profiles = taktful::Params::KeyProfiles::Temperley1999;
        break;
    }
    o.return_alternatives = p->return_alternatives != 0;
    o.num_alternatives = p->num_alternatives;
    o.fast_mode = p->fast_mode != 0;
    return o;
}

taktful_error from_wav_error(taktful::WavError e) {
    switch (e) {
    case taktful::WavError::OK: return TAKTFUL_OK;
    case taktful::WavError::FileNotFound: return TAKTFUL_ERR_FILE_NOT_FOUND;
    case taktful::WavError::NotAWav: return TAKTFUL_ERR_NOT_A_WAV;
    case taktful::WavError::UnsupportedFormat: return TAKTFUL_ERR_UNSUPPORTED_FORMAT;
    case taktful::WavError::UnsupportedBitDepth: return TAKTFUL_ERR_UNSUPPORTED_BIT_DEPTH;
    case taktful::WavError::OutOfMemory: return TAKTFUL_ERR_OUT_OF_MEMORY;
    case taktful::WavError::Truncated: return TAKTFUL_ERR_TRUNCATED;
    }
    return TAKTFUL_ERR_INVALID_ARG;
}

void fill_result(const taktful::AnalysisResult& r, taktful_result* out) {
    std::memset(out, 0, sizeof(*out));
    out->bpm = r.tempo.bpm;
    out->tempo_confidence = r.tempo.confidence;
    out->n_beats = static_cast<uint32_t>(r.tempo.beat_times_sec.size());
    if (out->n_beats > 0) {
        out->beat_times_sec = static_cast<float*>(std::malloc(out->n_beats * sizeof(float)));
        if (out->beat_times_sec) {
            for (uint32_t i = 0; i < out->n_beats; ++i) {
                out->beat_times_sec[i] = r.tempo.beat_times_sec[i];
            }
        } else {
            out->n_beats = 0;
        }
    }
    out->key = static_cast<taktful_key>(r.key.key);
    out->key_confidence = r.key.confidence;
    out->key_name = taktful::to_string(r.key.key);
    out->camelot = taktful::to_camelot(r.key.key);
    out->n_alternatives = static_cast<uint32_t>(r.key.alternatives.size());
    if (out->n_alternatives > 24) {
        out->n_alternatives = 24;
    }
    for (uint32_t i = 0; i < out->n_alternatives; ++i) {
        out->alternatives[i].key = static_cast<taktful_key>(r.key.alternatives[i].key);
        out->alternatives[i].confidence = r.key.alternatives[i].confidence;
        out->alternatives[i].camelot = taktful::to_camelot(r.key.alternatives[i].key);
    }
}

} // namespace

extern "C" {

const char* taktful_version(void) {
    return TAKTFUL_VERSION_STRING;
}

const char* taktful_error_string(taktful_error err) {
    switch (err) {
    case TAKTFUL_OK: return "ok";
    case TAKTFUL_ERR_FILE_NOT_FOUND: return "file not found";
    case TAKTFUL_ERR_NOT_A_WAV: return "not a WAV file";
    case TAKTFUL_ERR_UNSUPPORTED_FORMAT: return "unsupported WAV format";
    case TAKTFUL_ERR_UNSUPPORTED_BIT_DEPTH: return "unsupported bit depth";
    case TAKTFUL_ERR_OUT_OF_MEMORY: return "out of memory";
    case TAKTFUL_ERR_TRUNCATED: return "truncated WAV file";
    case TAKTFUL_ERR_INVALID_ARG: return "invalid argument";
    case TAKTFUL_ERR_ANALYSIS: return "analysis failed";
    }
    return "unknown error";
}

const char* taktful_key_name(taktful_key key) {
    return taktful::to_string(static_cast<taktful::Key>(key));
}

const char* taktful_key_camelot(taktful_key key) {
    return taktful::to_camelot(static_cast<taktful::Key>(key));
}

void taktful_params_default(taktful_params* params) {
    if (!params) return;
    std::memset(params, 0, sizeof(*params));
    params->fft_size = 4096;
    params->hop_size = 512;
    params->target_sample_rate = 11025;
    params->onset_method = TAKTFUL_ONSET_SPECTRAL_FLUX_HFC;
    params->tempo_min_bpm = 60.0f;
    params->tempo_max_bpm = 200.0f;
    params->enable_dp_refinement = 1;
    params->chroma_ref_freq = 440.0f;
    params->chroma_method = TAKTFUL_CHROMA_HPCP;
    params->apply_pre_filter = 1;
    params->hpcp_peak_threshold = 1.0e-4f;
    params->key_profiles = TAKTFUL_PROFILES_TEMPERLEY1999;
    params->return_alternatives = 1;
    params->num_alternatives = 5;
    params->fast_mode = 0;
}

taktful_error taktful_load_wav(const char* path, taktful_wav* out) {
    if (!out) return TAKTFUL_ERR_INVALID_ARG;
    std::memset(out, 0, sizeof(*out));
    taktful::WavData w;
    const taktful::WavError e = taktful::load_wav(path, w);
    if (e != taktful::WavError::OK) {
        return from_wav_error(e);
    }
    out->sample_rate = w.sample_rate;
    out->channels = w.channels;
    out->bits_per_sample = w.bits_per_sample;
    out->n_samples = static_cast<uint32_t>(w.samples.size());
    if (out->n_samples) {
        out->samples = static_cast<float*>(std::malloc(out->n_samples * sizeof(float)));
        if (!out->samples) {
            return TAKTFUL_ERR_OUT_OF_MEMORY;
        }
        std::memcpy(out->samples, w.samples.data(), out->n_samples * sizeof(float));
    }
    return TAKTFUL_OK;
}

taktful_error taktful_load_wav_mem(const void* data, size_t nbytes, taktful_wav* out) {
    if (!out || !data) return TAKTFUL_ERR_INVALID_ARG;
    std::memset(out, 0, sizeof(*out));
    taktful::WavData w;
    const taktful::WavError e = taktful::load_wav_mem(data, nbytes, w);
    if (e != taktful::WavError::OK) {
        return from_wav_error(e);
    }
    out->sample_rate = w.sample_rate;
    out->channels = w.channels;
    out->bits_per_sample = w.bits_per_sample;
    out->n_samples = static_cast<uint32_t>(w.samples.size());
    if (out->n_samples) {
        out->samples = static_cast<float*>(std::malloc(out->n_samples * sizeof(float)));
        if (!out->samples) {
            return TAKTFUL_ERR_OUT_OF_MEMORY;
        }
        std::memcpy(out->samples, w.samples.data(), out->n_samples * sizeof(float));
    }
    return TAKTFUL_OK;
}

void taktful_wav_free(taktful_wav* wav) {
    if (!wav) return;
    std::free(wav->samples);
    std::memset(wav, 0, sizeof(*wav));
}

taktful_error taktful_analyze(const taktful_wav* wav, const taktful_params* params, taktful_result* out) {
    if (!wav || !out) return TAKTFUL_ERR_INVALID_ARG;
    return taktful_analyze_buffer(wav->samples, wav->n_samples, wav->sample_rate, params, out);
}

taktful_error taktful_analyze_buffer(const float* samples, uint32_t n_samples, uint32_t sample_rate,
                               const taktful_params* params, taktful_result* out) {
    if (!out) return TAKTFUL_ERR_INVALID_ARG;
    taktful::Params p = from_c(params);
    taktful::AnalysisResult r = taktful::analyze_buffer(samples, n_samples, sample_rate, p);
    fill_result(r, out);
    return TAKTFUL_OK;
}

void taktful_result_free(taktful_result* result) {
    if (!result) return;
    std::free(result->beat_times_sec);
    result->beat_times_sec = 0;
    result->n_beats = 0;
}

struct taktful_stream {
    taktful::StreamAnalyzer impl;
    explicit taktful_stream(const taktful::Params& p) : impl(p) {}
};

taktful_stream* taktful_stream_create(const taktful_params* params) {
    try {
        return new taktful_stream(from_c(params));
    } catch (...) {
        return 0;
    }
}

void taktful_stream_reset(taktful_stream* s) {
    if (s) s->impl.reset();
}

taktful_error taktful_stream_push(taktful_stream* s, const float* samples, uint32_t n, uint32_t sample_rate) {
    if (!s) return TAKTFUL_ERR_INVALID_ARG;
    s->impl.push(samples, n, sample_rate);
    return TAKTFUL_OK;
}

taktful_error taktful_stream_finish(taktful_stream* s, taktful_result* out) {
    if (!s || !out) return TAKTFUL_ERR_INVALID_ARG;
    fill_result(s->impl.finish(), out);
    return TAKTFUL_OK;
}

void taktful_stream_destroy(taktful_stream* s) {
    delete s;
}

} // extern "C"
