/* ott_dsp.h - OTT multiband compressor DSP core (no LV2 dependency). */

#ifndef OTT_DSP_H
#define OTT_DSP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* OTT instance (process one mono channel, or paired for stereo linking). */
typedef struct ott_dsp ott_dsp_t;

/* Create an OTT instance. sample_rate in Hz. */
ott_dsp_t *ott_dsp_new(double sample_rate);

/* Free an OTT instance. NULL-safe. */
void ott_dsp_free(ott_dsp_t *ott);

/* Reset internal state (filter states, envelopes, RMS followers). */
void ott_dsp_reset(ott_dsp_t *ott);

/* Parameters. 0.0 to 1.0 for macro controls, dB for gains. */
typedef struct {
    float depth;          /* wet/dry mix, 0.0=dry, 1.0=wet (default 1.0) */
    float time_scale;     /* attack/release scaling, 0.1 to 2.0 (default 1.0) */
    float upward;         /* upward compression amount, 0.0 to 1.0 (default 1.0) */
    float downward;       /* downward compression amount, 0.0 to 1.0 (default 1.0) */
    float input_gain;     /* dB, default +5.2 */
    float output_gain;    /* dB, default 0.0 */
    float band_thresh[3]; /* dB per band, default -30.0 */
    float band_gain[3];   /* dB output gain per band, default +10.3, +5.7, +10.3 */
    int   bypass;         /* 1 = bypass, 0 = process */
} ott_params_t;

/* Return the default parameter set. */
ott_params_t ott_dsp_default_params(void);

/* Set parameters. Recomputes cached coefficients. */
void ott_dsp_set_params(ott_dsp_t *ott, const ott_params_t *params);

/* Get the current parameters (copy). */
void ott_dsp_get_params(ott_dsp_t *ott, ott_params_t *out);

/* Process a single sample. Returns the processed sample (mono path). */
float ott_dsp_process(ott_dsp_t *ott, float input);

/* Process a buffer of samples (in-place or out-of-place, mono path). */
void ott_dsp_process_block(ott_dsp_t *ott, const float *input,
                           float *output, size_t n);

/* Process a stereo block with linked detection: envelope is derived from
 * max(level_L, level_R) and the same gain is applied to both channels.
 * Uses the left instance's envelope state for the linked detector.
 * Each instance keeps its own crossover filter state. */
void ott_dsp_process_stereo(ott_dsp_t *left, ott_dsp_t *right,
                            const float *inL, const float *inR,
                            float *outL, float *outR, size_t n);

/* Split a single input sample into 3 bands using the LR4 crossover, with the
 * current input gain applied. No compression. Intended for unit tests that
 * need to verify the crossover sums flat. bands[0]=low, [1]=mid, [2]=high. */
void ott_dsp_crossover_split(ott_dsp_t *ott, float input, float bands[3]);

/* Report the net compression gain currently applied to each band, in dB,
 * from the most recently processed sample. bands[0]=low, [1]=mid, [2]=high,
 * matching the order used throughout this header. Positive values mean net
 * attenuation (downward compression is dominant); negative values mean a
 * net boost (upward compression is dominant); 0 means the band is sitting
 * at its threshold with neither compressor active. Intended for driving a
 * UI meter, not for audio-rate use. */
void ott_dsp_get_band_meter(ott_dsp_t *ott, float meter_db[3]);

#ifdef __cplusplus
}
#endif

#endif /* OTT_DSP_H */
