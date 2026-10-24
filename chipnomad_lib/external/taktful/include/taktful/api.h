#ifndef TAKTFUL_API_H
#define TAKTFUL_API_H

#include <cstdint>
#include <vector>

#include <taktful/version.h>
#include <taktful/wav_io.h>

namespace taktful {

struct Params {
    uint32_t fft_size = 4096;
    uint32_t hop_size = 512;
    uint32_t target_sample_rate = 11025;

    enum class OnsetMethod {
        SpectralFlux,
        SpectralFluxHFC,
    };
    OnsetMethod onset_method = OnsetMethod::SpectralFluxHFC;

    float tempo_min_bpm = 60.0f;
    float tempo_max_bpm = 200.0f;
    bool enable_dp_refinement = true;

    float chroma_ref_freq = 440.0f;
    bool use_hpcp = true;
    enum class ChromaMethod {
        Hpcp,
        NaivePcp,
        Clp,
    };
    ChromaMethod chroma_method = ChromaMethod::Hpcp;
    bool apply_pre_filter = true;
    float hpcp_peak_threshold = 1.0e-4f;

    enum class KeyProfiles {
        KrumhanslOriginal,
        Temperley1999,
        Temperley2007,
    };
    KeyProfiles key_profiles = KeyProfiles::Temperley1999;
    bool return_alternatives = true;
    uint8_t num_alternatives = 5;

    bool fast_mode = false;
};

enum class Key {
    CMaj, CsMaj, DMaj, DsMaj, EMaj, FMaj, FsMaj, GMaj, GsMaj, AMaj, AsMaj, BMaj,
    CMin, CsMin, DMin, DsMin, EMin, FMin, FsMin, GMin, GsMin, AMin, AsMin, BMin,
    Unknown,
};

const char* to_string(Key key);
const char* to_camelot(Key key);

struct KeyResult {
    Key key = Key::Unknown;
    float confidence = 0.0f;
    const char* camelot = "";
    struct Alternative {
        Key key;
        float confidence;
        const char* camelot;
    };
    std::vector<Alternative> alternatives;
};

struct TempoResult {
    float bpm = 0.0f;
    float confidence = 0.0f;
    std::vector<float> beat_times_sec;
};

struct AnalysisResult {
    TempoResult tempo;
    KeyResult key;
};

AnalysisResult analyze(const WavData& audio, const Params& params = Params());

struct IntermediateFeatures {
    std::vector<float> onset_envelope;
    std::vector<std::vector<float> > chroma;
};

IntermediateFeatures compute_features(const WavData& audio, const Params& params = Params());

bool validate_params(Params& params);

} // namespace taktful

#include <taktful/notes.h>

#endif
