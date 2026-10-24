#ifndef TAKTFUL_C_API_H
#define TAKTFUL_C_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum taktful_error {
    TAKTFUL_OK = 0,
    TAKTFUL_ERR_FILE_NOT_FOUND = 1,
    TAKTFUL_ERR_NOT_A_WAV = 2,
    TAKTFUL_ERR_UNSUPPORTED_FORMAT = 3,
    TAKTFUL_ERR_UNSUPPORTED_BIT_DEPTH = 4,
    TAKTFUL_ERR_OUT_OF_MEMORY = 5,
    TAKTFUL_ERR_TRUNCATED = 6,
    TAKTFUL_ERR_INVALID_ARG = 7,
    TAKTFUL_ERR_ANALYSIS = 8
} taktful_error;

typedef enum taktful_key {
    TAKTFUL_KEY_C_MAJ = 0,  TAKTFUL_KEY_CS_MAJ, TAKTFUL_KEY_D_MAJ,  TAKTFUL_KEY_DS_MAJ,
    TAKTFUL_KEY_E_MAJ,      TAKTFUL_KEY_F_MAJ,  TAKTFUL_KEY_FS_MAJ, TAKTFUL_KEY_G_MAJ,
    TAKTFUL_KEY_GS_MAJ,     TAKTFUL_KEY_A_MAJ,  TAKTFUL_KEY_AS_MAJ, TAKTFUL_KEY_B_MAJ,
    TAKTFUL_KEY_C_MIN,      TAKTFUL_KEY_CS_MIN, TAKTFUL_KEY_D_MIN,  TAKTFUL_KEY_DS_MIN,
    TAKTFUL_KEY_E_MIN,      TAKTFUL_KEY_F_MIN,  TAKTFUL_KEY_FS_MIN, TAKTFUL_KEY_G_MIN,
    TAKTFUL_KEY_GS_MIN,     TAKTFUL_KEY_A_MIN,  TAKTFUL_KEY_AS_MIN, TAKTFUL_KEY_B_MIN,
    TAKTFUL_KEY_UNKNOWN
} taktful_key;

typedef enum taktful_onset_method {
    TAKTFUL_ONSET_SPECTRAL_FLUX = 0,
    TAKTFUL_ONSET_SPECTRAL_FLUX_HFC = 1
} taktful_onset_method;

typedef enum taktful_key_profiles {
    TAKTFUL_PROFILES_KRUMHANSL = 0,
    TAKTFUL_PROFILES_TEMPERLEY1999 = 1,
    TAKTFUL_PROFILES_TEMPERLEY2007 = 2
} taktful_key_profiles;

typedef enum taktful_chroma_method {
    TAKTFUL_CHROMA_HPCP = 0,
    TAKTFUL_CHROMA_NAIVE_PCP = 1,
    TAKTFUL_CHROMA_CLP = 2
} taktful_chroma_method;

typedef struct taktful_wav {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    float* samples;
    uint32_t n_samples;
} taktful_wav;

typedef struct taktful_params {
    uint32_t fft_size;
    uint32_t hop_size;
    uint32_t target_sample_rate;
    taktful_onset_method onset_method;
    float tempo_min_bpm;
    float tempo_max_bpm;
    int enable_dp_refinement;
    float chroma_ref_freq;
    taktful_chroma_method chroma_method;
    int apply_pre_filter;
    float hpcp_peak_threshold;
    taktful_key_profiles key_profiles;
    int return_alternatives;
    uint8_t num_alternatives;
    int fast_mode;
} taktful_params;

typedef struct taktful_alt {
    taktful_key key;
    float confidence;
    const char* camelot;
} taktful_alt;

typedef struct taktful_result {
    float bpm;
    float tempo_confidence;
    float* beat_times_sec;
    uint32_t n_beats;
    taktful_key key;
    float key_confidence;
    const char* key_name;
    const char* camelot;
    taktful_alt alternatives[24];
    uint32_t n_alternatives;
} taktful_result;

typedef struct taktful_stream taktful_stream;

const char* taktful_version(void);
const char* taktful_error_string(taktful_error err);
const char* taktful_key_name(taktful_key key);
const char* taktful_key_camelot(taktful_key key);

void taktful_params_default(taktful_params* params);

taktful_error taktful_load_wav(const char* path, taktful_wav* out);
taktful_error taktful_load_wav_mem(const void* data, size_t nbytes, taktful_wav* out);
void taktful_wav_free(taktful_wav* wav);

taktful_error taktful_analyze(const taktful_wav* wav, const taktful_params* params, taktful_result* out);
taktful_error taktful_analyze_buffer(const float* samples, uint32_t n_samples, uint32_t sample_rate,
                               const taktful_params* params, taktful_result* out);
void taktful_result_free(taktful_result* result);

taktful_stream* taktful_stream_create(const taktful_params* params);
void taktful_stream_reset(taktful_stream* s);
taktful_error taktful_stream_push(taktful_stream* s, const float* samples, uint32_t n, uint32_t sample_rate);
taktful_error taktful_stream_finish(taktful_stream* s, taktful_result* out);
void taktful_stream_destroy(taktful_stream* s);

#ifdef __cplusplus
}
#endif

#endif
