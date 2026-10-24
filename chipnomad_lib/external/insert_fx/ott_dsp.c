/* ott_dsp.c - OTT multiband compressor DSP core (no LV2 dependency).
 *
 * Three-band Linkwitz-Riley 4th-order crossover at 88.3 Hz and 2500 Hz.
 * Each band runs a downward compressor (high ratio, soft knee) followed by
 * an upward compressor (4:1, soft knee). Stereo linking uses the max of the
 * left and right per-band RMS levels so both channels get identical gain.
 *
 * See RESEARCH.md for the full design notes and sources.
 */

#include "ott_dsp.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2 0.70710678118654752440
#endif

/* ----------------------------------------------------------------------- */
/* Constants from RESEARCH.md                                              */
/* ----------------------------------------------------------------------- */

#define OTT_XO_LOW   88.3     /* Hz, low/mid crossover                       */
#define OTT_XO_HIGH  2500.0   /* Hz, mid/high crossover                      */

#define OTT_ATTACK_MS    5.0  /* ms, at time_scale = 1.0                     */
#define OTT_RELEASE_MS  50.0  /* ms, at time_scale = 1.0                     */
#define OTT_RMS_MS       2.5  /* ms, RMS detector window (one-pole tau)      */
#define OTT_KNEE_DB      6.0  /* dB, soft knee width                         */

#define OTT_UP_RATIO     4.0f   /* upward base ratio (all bands)             */
#define OTT_DOWN_RATIO_LM 66.7f /* downward base ratio, low and mid bands    */
/* high band: limiter, slope = 1.0 (effectively infinity:1)                 */

#define OTT_DB_FLOOR  1e-10f   /* prevents log(0)                            */
#define OTT_DENORMAL_OFFSET 1e-20f  /* flush denormals in filters           */
#define OTT_GR_MIN  -24.0f   /* max upward boost: +24 dB                   */
#define OTT_GR_MAX   60.0f   /* max downward reduction: -60 dB             */
#define OTT_GAIN_MIN 1e-6f   /* don't let gain go below -120 dB            */
#define OTT_GAIN_MAX 100.0f  /* don't let gain go above +40 dB             */

/* ----------------------------------------------------------------------- */
/* Biquad (Direct Form I) and LR4 (two cascaded Butterworth biquads)       */
/* ----------------------------------------------------------------------- */

typedef struct {
    float b0, b1, b2;   /* feedforward (a0 normalised to 1) */
    float a1, a2;       /* feedback */
    float x1, x2, y1, y2;
} biquad_t;

typedef struct {
    biquad_t s1, s2;
} lr4_t;

static void biquad_reset(biquad_t *bq) {
    bq->x1 = bq->x2 = bq->y1 = bq->y2 = 0.0f;
}

static void biquad_set_lpf(biquad_t *bq, double fc, double fs) {
    double omega = 2.0 * M_PI * fc / fs;
    double cos_o = cos(omega);
    double sin_o = sin(omega);
    double alpha = sin_o / (2.0 * M_SQRT1_2);
    double a0 = 1.0 + alpha;
    bq->b0 = (float)((1.0 - cos_o) * 0.5 / a0);
    bq->b1 = (float)((1.0 - cos_o) / a0);
    bq->b2 = (float)((1.0 - cos_o) * 0.5 / a0);
    bq->a1 = (float)(-2.0 * cos_o / a0);
    bq->a2 = (float)((1.0 - alpha) / a0);
    biquad_reset(bq);
}

static void biquad_set_hpf(biquad_t *bq, double fc, double fs) {
    double omega = 2.0 * M_PI * fc / fs;
    double cos_o = cos(omega);
    double sin_o = sin(omega);
    double alpha = sin_o / (2.0 * M_SQRT1_2);
    double a0 = 1.0 + alpha;
    bq->b0 = (float)((1.0 + cos_o) * 0.5 / a0);
    bq->b1 = (float)(-(1.0 + cos_o) / a0);
    bq->b2 = (float)((1.0 + cos_o) * 0.5 / a0);
    bq->a1 = (float)(-2.0 * cos_o / a0);
    bq->a2 = (float)((1.0 - alpha) / a0);
    biquad_reset(bq);
}

static inline float biquad_process(const biquad_t *bq, biquad_t *state, float x) {
    /* Read coefficients from bq (could be shared), state from `state`. */
    float y = bq->b0 * x
            + bq->b1 * state->x1
            + bq->b2 * state->x2
            - bq->a1 * state->y1
            - bq->a2 * state->y2;
    state->x2 = state->x1;
    state->x1 = x;
    state->y2 = state->y1;
    state->y1 = y;
    return y;
}

/* LR4 stage: holds coefficients and two biquad states. */
typedef struct {
    biquad_t coef;       /* shared coefficients for both stages */
    biquad_t state1;
    biquad_t state2;
} lr4_stage_t;

static void lr4_set_lpf(lr4_stage_t *f, double fc, double fs) {
    biquad_set_lpf(&f->coef, fc, fs);
    biquad_reset(&f->state1);
    biquad_reset(&f->state2);
}

static void lr4_set_hpf(lr4_stage_t *f, double fc, double fs) {
    biquad_set_hpf(&f->coef, fc, fs);
    biquad_reset(&f->state1);
    biquad_reset(&f->state2);
}

static inline float lr4_process(lr4_stage_t *f, float x) {
    float y1 = biquad_process(&f->coef, &f->state1, x);
    return biquad_process(&f->coef, &f->state2, y1);
}

static void lr4_reset(lr4_stage_t *f) {
    biquad_reset(&f->state1);
    biquad_reset(&f->state2);
}

/* ----------------------------------------------------------------------- */
/* Per-band compressor state                                               */
/* ----------------------------------------------------------------------- */

typedef struct {
    /* Downward detector + ballistics */
    float rms_down_sq;     /* one-pole running mean of x^2 */
    float env_down_ac;     /* smooth-decoupled: attack-coupled state */
    float env_down_ra;     /* smooth-decoupled: release-accented state */

    /* Upward detector + ballistics (operates on the post-downward signal) */
    float rms_up_sq;
    float env_up_ac;
    float env_up_ra;
} band_state_t;

/* ----------------------------------------------------------------------- */
/* ott_dsp instance                                                        */
/* ----------------------------------------------------------------------- */

struct ott_dsp {
    double sample_rate;
    ott_params_t params;

    /* Crossover: 4 LR4 stages for 3 bands.
     *   lp_low  : LP at 88.3 Hz  -> low band
     *   hp_low  : HP at 88.3 Hz  -> (mid + high) path
     *   lp_mid  : LP at 2500 Hz  -> mid band (applied to hp_low output)
     *   hp_high : HP at 2500 Hz  -> high band (applied to hp_low output)
     */
    lr4_stage_t lp_low, hp_low, lp_mid, hp_high;

    band_state_t band[3];

    /* Net compression gain applied to each band on the most recently
     * processed sample, in dB (positive = attenuation, negative = boost).
     * Updated by process_band_sample (mono) and the stereo loop. Read by
     * ott_dsp_get_band_meter() for UI display; not used in the audio path
     * itself. */
    float band_net_gr_db[3];

    /* Cached derived parameters (recomputed on set_params). */
    float alpha_a;          /* attack coefficient */
    float alpha_r;          /* release coefficient */
    float alpha_rms;        /* RMS detector coefficient */
    float input_gain_lin;
    float output_gain_lin;
    float band_gain_lin[3];
    float slope_down[3];    /* downward slope per band (0..1) */
    float slope_up[3];      /* upward slope per band (0..1) */
};

/* ----------------------------------------------------------------------- */
/* Helpers                                                                 */
/* ----------------------------------------------------------------------- */

static float db_to_lin(float db) {
    return expf(db * (float)(M_LN10 / 20.0));
}

/* Flush denormals to zero. Call on every output sample. */
static inline float flush_denormal(float x) {
    if (fabsf(x) < OTT_DENORMAL_OFFSET) return 0.0f;
    return x;
}

/* Clamp gain to a safe range. Prevents NaN propagation and extreme values. */
static inline float clamp_gain(float g) {
    if (isnan(g) || isinf(g)) return 0.0f;
    if (g < OTT_GAIN_MIN) return OTT_GAIN_MIN;
    if (g > OTT_GAIN_MAX) return OTT_GAIN_MAX;
    return g;
}

/* Clamp gain reduction in dB. */
static inline float clamp_gr(float gr) {
    if (isnan(gr) || isinf(gr)) return 0.0f;
    if (gr < OTT_GR_MIN) return OTT_GR_MIN;
    if (gr > OTT_GR_MAX) return OTT_GR_MAX;
    return gr;
}

static float gain_computer_down(float level_db, float T, float slope, float W) {
    /* Returns gain reduction in dB (>= 0). Soft knee of width W around T. */
    float overshoot = level_db - T;
    float knee_half = W * 0.5f;
    if (overshoot <= -knee_half) return 0.0f;
    if (overshoot < knee_half) {
        float t = overshoot + knee_half;
        return 0.5f * slope * t * t / W;
    }
    return slope * overshoot;
}

static float gain_computer_up(float level_db, float T, float slope, float W) {
    /* Returns gain reduction in dB (<= 0, i.e. a boost). Soft knee.
     * No boost when the signal is more than 30 dB below threshold.
     * This prevents the compressor from trying to boost silence or
     * near-silence, which causes the muting-on-startup bug. */
    float undershoot = T - level_db;
    if (undershoot <= 0.0f) return 0.0f;       /* signal at/above threshold: no boost */
    if (undershoot > 30.0f) return 0.0f;        /* signal way below threshold: no boost */
    float knee_half = W * 0.5f;
    if (undershoot < knee_half) {
        float t = undershoot + knee_half;
        return -0.5f * slope * t * t / W;
    }
    return -slope * undershoot;
}

/* Smooth-decoupled ballistics, per Giannoulis/Reiss.
 * target is in dB of gain reduction (positive = reduce, negative = boost).
 * state1 is the attack-smoothed output, state2 is the release-accented state.
 */
static inline float ballistics_decoupled(float target, float *state1,
                                         float *state2,
                                         float alpha_a, float alpha_r) {
    float released = alpha_r * (*state2) + (1.0f - alpha_r) * target;
    *state2 = (target > released) ? target : released;
    *state1 = alpha_a * (*state1) + (1.0f - alpha_a) * (*state2);
    return *state1;
}

/* ----------------------------------------------------------------------- */
/* Public API                                                              */
/* ----------------------------------------------------------------------- */

ott_params_t ott_dsp_default_params(void) {
    ott_params_t p;
    p.depth = 1.0f;
    p.time_scale = 1.0f;
    p.upward = 1.0f;
    p.downward = 1.0f;
    p.input_gain = 5.2f;
    p.output_gain = 0.0f;
    p.band_thresh[0] = -30.0f;
    p.band_thresh[1] = -30.0f;
    p.band_thresh[2] = -30.0f;
    p.band_gain[0] = 10.3f;
    p.band_gain[1] = 5.7f;
    p.band_gain[2] = 10.3f;
    p.bypass = 0;
    return p;
}

ott_dsp_t *ott_dsp_new(double sample_rate) {
    if (sample_rate <= 0.0) return NULL;
    ott_dsp_t *ott = (ott_dsp_t *)calloc(1, sizeof(*ott));
    if (!ott) return NULL;
    ott->sample_rate = sample_rate;
    ott->params = ott_dsp_default_params();

    /* Build the crossover filters once. Their coefficients are fixed
     * (crossover frequencies are not user-adjustable). */
    lr4_set_lpf(&ott->lp_low,  OTT_XO_LOW,  sample_rate);
    lr4_set_hpf(&ott->hp_low,  OTT_XO_LOW,  sample_rate);
    lr4_set_lpf(&ott->lp_mid,  OTT_XO_HIGH, sample_rate);
    lr4_set_hpf(&ott->hp_high, OTT_XO_HIGH, sample_rate);

    ott_dsp_reset(ott);
    /* Recompute cached coefficients from defaults. */
    ott_dsp_set_params(ott, &ott->params);
    return ott;
}

void ott_dsp_free(ott_dsp_t *ott) {
    free(ott);
}

void ott_dsp_reset(ott_dsp_t *ott) {
    if (!ott) return;
    lr4_reset(&ott->lp_low);
    lr4_reset(&ott->hp_low);
    lr4_reset(&ott->lp_mid);
    lr4_reset(&ott->hp_high);
    /* Initialize RMS detector to the threshold level so silence at
     * startup does not trigger massive upward compression. Without
     * this, the RMS starts at 0 (-> -inf dB), the upward compressor
     * sees a signal 200 dB below threshold and slams gain to max,
     * which then causes the downward compressor to overreact when
     * the first note arrives, creating a muting effect. */
    float thresh_lin = db_to_lin(ott->params.band_thresh[0]);
    float thresh_sq = thresh_lin * thresh_lin;
    for (int b = 0; b < 3; ++b) {
        ott->band[b].rms_down_sq = thresh_sq;
        ott->band[b].env_down_ac = 0.0f;
        ott->band[b].env_down_ra = 0.0f;
        ott->band[b].rms_up_sq   = thresh_sq;
        ott->band[b].env_up_ac   = 0.0f;
        ott->band[b].env_up_ra   = 0.0f;
        ott->band_net_gr_db[b]   = 0.0f;
    }
}

void ott_dsp_set_params(ott_dsp_t *ott, const ott_params_t *params) {
    if (!ott || !params) return;
    ott->params = *params;

    /* Clamp macro controls. */
    float ts = params->time_scale;
    if (ts < 0.1f) ts = 0.1f;
    if (ts > 2.0f) ts = 2.0f;
    float up = params->upward;
    if (up < 0.0f) up = 0.0f;
    if (up > 1.0f) up = 1.0f;
    float dn = params->downward;
    if (dn < 0.0f) dn = 0.0f;
    if (dn > 1.0f) dn = 1.0f;
    float depth = params->depth;
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;

    double fs = ott->sample_rate;

    /* Time constants scaled by time_scale. */
    double tau_a = (OTT_ATTACK_MS  * 1e-3) * ts;
    double tau_r = (OTT_RELEASE_MS * 1e-3) * ts;
    double tau_rms = OTT_RMS_MS * 1e-3;
    ott->alpha_a   = (float)exp(-1.0 / (tau_a   * fs));
    ott->alpha_r   = (float)exp(-1.0 / (tau_r   * fs));
    ott->alpha_rms = (float)exp(-1.0 / (tau_rms * fs));

    ott->input_gain_lin  = db_to_lin(params->input_gain);
    ott->output_gain_lin = db_to_lin(params->output_gain);
    for (int b = 0; b < 3; ++b) {
        ott->band_gain_lin[b] = db_to_lin(params->band_gain[b]);
    }

    /* Per-band downward slopes. Low/mid use 66.7:1, high uses limiter (1.0).
     * The `downward` macro scales the slope linearly from 0 (no compression)
     * to the full ratio. */
    float down_slope_lm = 1.0f - 1.0f / OTT_DOWN_RATIO_LM; /* ~0.985 */
    ott->slope_down[0] = down_slope_lm * dn;
    ott->slope_down[1] = down_slope_lm * dn;
    ott->slope_down[2] = 1.0f * dn;                        /* limiter */

    /* Per-band upward slope (4:1, scaled by `upward`). */
    float up_slope = 1.0f - 1.0f / OTT_UP_RATIO;           /* 0.75 */
    for (int b = 0; b < 3; ++b) {
        ott->slope_up[b] = up_slope * up;
    }
}

void ott_dsp_get_params(ott_dsp_t *ott, ott_params_t *out) {
    if (ott && out) *out = ott->params;
}

void ott_dsp_get_band_meter(ott_dsp_t *ott, float meter_db[3]) {
    if (!ott || !meter_db) return;
    for (int b = 0; b < 3; ++b) {
        meter_db[b] = ott->band_net_gr_db[b];
    }
}

/* ----------------------------------------------------------------------- */
/* Per-sample processing core (shared by mono and stereo paths).           */
/* ----------------------------------------------------------------------- */

/* Process one band of one sample. Reads the band input sample, applies the
 * downward then upward compressor, returns the processed sample. Updates the
 * band state in `bs`. The detector level is provided by the caller so that
 * stereo linking (max of L and R) can be done before this runs. */
static inline float process_band_sample(band_state_t *bs,
                                        float x,
                                        float T,
                                        float slope_down,
                                        float slope_up,
                                        float alpha_a,
                                        float alpha_r,
                                        float alpha_rms,
                                        float *out_gr_down,
                                        float *out_gr_up) {
    /* Downward: RMS detector on input. */
    float sq_down = x * x;
    bs->rms_down_sq = alpha_rms * bs->rms_down_sq + (1.0f - alpha_rms) * sq_down;
    float level_down = sqrtf(bs->rms_down_sq);
    float level_db_down = 20.0f * log10f(level_down + OTT_DB_FLOOR);
    float gr_down_raw = gain_computer_down(level_db_down, T, slope_down, OTT_KNEE_DB);
    float gr_down = ballistics_decoupled(gr_down_raw,
                                         &bs->env_down_ac, &bs->env_down_ra,
                                         alpha_a, alpha_r);
    /* gain = 10^(-gr_down / 20): positive gr_down -> attenuation. */
    gr_down = clamp_gr(gr_down);
    float gain_down = clamp_gain(expf(-gr_down * (float)(M_LN10 / 20.0)));
    x *= gain_down;
    x = flush_denormal(x);

    /* Upward: RMS detector on the post-downward signal. */
    float sq_up = x * x;
    bs->rms_up_sq = alpha_rms * bs->rms_up_sq + (1.0f - alpha_rms) * sq_up;
    float level_up = sqrtf(bs->rms_up_sq);
    float level_db_up = 20.0f * log10f(level_up + OTT_DB_FLOOR);
    float gr_up_raw = gain_computer_up(level_db_up, T, slope_up, OTT_KNEE_DB);
    float gr_up = ballistics_decoupled(gr_up_raw,
                                       &bs->env_up_ac, &bs->env_up_ra,
                                       alpha_a, alpha_r);
    /* gain = 10^(-gr_up / 20): negative gr_up -> boost. */
    gr_up = clamp_gr(gr_up);
    float gain_up = clamp_gain(expf(-gr_up * (float)(M_LN10 / 20.0)));
    x *= gain_up;
    x = flush_denormal(x);
    if (out_gr_down) *out_gr_down = gr_down;
    if (out_gr_up)   *out_gr_up   = gr_up;
    return x;
}

/* Process one sample for a single instance (mono path). */
static float process_mono_sample(ott_dsp_t *ott, float input) {
    if (ott->params.bypass) return input;

    const float depth = ott->params.depth;
    const float in_g = ott->input_gain_lin;
    const float out_g = ott->output_gain_lin;
    const float alpha_a = ott->alpha_a;
    const float alpha_r = ott->alpha_r;
    const float alpha_rms = ott->alpha_rms;

    /* Apply input gain, then split into 3 bands. */
    float x = flush_denormal(input) * in_g;
    float low = lr4_process(&ott->lp_low,  x);
    float mh  = lr4_process(&ott->hp_low,  x);
    float mid = lr4_process(&ott->lp_mid,  mh);
    float high = lr4_process(&ott->hp_high, mh);

    float band_in[3] = { low, mid, high };
    float wet = 0.0f;
    for (int b = 0; b < 3; ++b) {
        float gr_down = 0.0f, gr_up = 0.0f;
        float y = process_band_sample(&ott->band[b], band_in[b],
                                      ott->params.band_thresh[b],
                                      ott->slope_down[b],
                                      ott->slope_up[b],
                                      alpha_a, alpha_r, alpha_rms,
                                      &gr_down, &gr_up);
        ott->band_net_gr_db[b] = gr_down + gr_up;
        wet += y * ott->band_gain_lin[b];
    }

    /* Master output gain on the wet signal. */
    wet *= out_g;

    /* Dry/wet mix. Dry is the original input. */
    float out = input * (1.0f - depth) + wet * depth;
    if (isnan(out) || isinf(out)) return 0.0f;
    return flush_denormal(out);
}

float ott_dsp_process(ott_dsp_t *ott, float input) {
    if (!ott) return input;
    return process_mono_sample(ott, input);
}

void ott_dsp_process_block(ott_dsp_t *ott, const float *input,
                           float *output, size_t n) {
    if (!ott || !input || !output) return;
    for (size_t i = 0; i < n; ++i) {
        output[i] = process_mono_sample(ott, input[i]);
    }
}

void ott_dsp_crossover_split(ott_dsp_t *ott, float input, float bands[3]) {
    if (!ott || !bands) return;
    float x = input * ott->input_gain_lin;
    bands[0] = lr4_process(&ott->lp_low,  x);
    float mh = lr4_process(&ott->hp_low,  x);
    bands[1] = lr4_process(&ott->lp_mid,  mh);
    bands[2] = lr4_process(&ott->hp_high, mh);
}

/* ----------------------------------------------------------------------- */
/* Stereo linked processing                                                */
/* ----------------------------------------------------------------------- */

/* For stereo linking, the detector level for each band is the max of the
 * left and right RMS levels. The gain computed from that max level is applied
 * to both channels. We use the left instance's band state to host the
 * envelope follower so the gain history is consistent across calls; the right
 * instance's band state is updated identically so that running it solo later
 * (mono mode) does not start from a stale envelope.
 *
 * The crossover filters run independently per channel (each instance keeps
 * its own filter state). */

void ott_dsp_process_stereo(ott_dsp_t *left, ott_dsp_t *right,
                            const float *inL, const float *inR,
                            float *outL, float *outR, size_t n) {
    if (!left || !inL || !outL) return;
    int have_r = (right && inR && outR);

    /* Cache reads. */
    const float depth = left->params.depth;
    const float in_g = left->input_gain_lin;
    const float out_g = left->output_gain_lin;
    const float alpha_a = left->alpha_a;
    const float alpha_r = left->alpha_r;
    const float alpha_rms = left->alpha_rms;

    if (left->params.bypass) {
        for (size_t i = 0; i < n; ++i) {
            outL[i] = inL[i];
            if (have_r) outR[i] = inR[i];
        }
        return;
    }

    for (size_t i = 0; i < n; ++i) {
        /* Crossover, per channel. */
        float xL = inL[i] * in_g;
        float lowL = lr4_process(&left->lp_low,  xL);
        float mhL  = lr4_process(&left->hp_low,  xL);
        float midL = lr4_process(&left->lp_mid,  mhL);
        float highL = lr4_process(&left->hp_high, mhL);

        float lowR = 0.0f, midR = 0.0f, highR = 0.0f;
        if (have_r) {
            float xR = inR[i] * in_g;
            lowR  = lr4_process(&right->lp_low,  xR);
            float mhR  = lr4_process(&right->hp_low,  xR);
            midR  = lr4_process(&right->lp_mid,  mhR);
            highR = lr4_process(&right->hp_high, mhR);
        }

        float bandL[3] = { lowL, midL, highL };
        float bandR[3] = { lowR, midR, highR };

        float wetL = 0.0f, wetR = 0.0f;
        for (int b = 0; b < 3; ++b) {
            float x_l = bandL[b];
            float x_r = have_r ? bandR[b] : x_l;
            float band_gr_down = 0.0f;
            float band_gr_up = 0.0f;

            /* Downward: linked RMS detector. */
            float sq_l = x_l * x_l;
            float sq_r = x_r * x_r;
            left->band[b].rms_down_sq =
                alpha_rms * left->band[b].rms_down_sq + (1.0f - alpha_rms) * sq_l;
            if (have_r) {
                right->band[b].rms_down_sq =
                    alpha_rms * right->band[b].rms_down_sq + (1.0f - alpha_rms) * sq_r;
                /* Use the louder channel for the linked detector. */
                float lev_l = sqrtf(left->band[b].rms_down_sq);
                float lev_r = sqrtf(right->band[b].rms_down_sq);
                float level = (lev_l > lev_r) ? lev_l : lev_r;
                float level_db = 20.0f * log10f(level + OTT_DB_FLOOR);
                float gr_raw = gain_computer_down(level_db,
                                                  left->params.band_thresh[b],
                                                  left->slope_down[b],
                                                  OTT_KNEE_DB);
                float gr = ballistics_decoupled(gr_raw,
                                                &left->band[b].env_down_ac,
                                                &left->band[b].env_down_ra,
                                                alpha_a, alpha_r);
                /* Mirror to right so its envelope tracks identically. */
                right->band[b].env_down_ac = left->band[b].env_down_ac;
                right->band[b].env_down_ra = left->band[b].env_down_ra;
                gr = clamp_gr(gr);
                band_gr_down = gr;
                float gain = clamp_gain(expf(-gr * (float)(M_LN10 / 20.0)));
                x_l *= gain; x_l = flush_denormal(x_l);
                x_r *= gain; x_r = flush_denormal(x_r);
            } else {
                float level = sqrtf(left->band[b].rms_down_sq);
                float level_db = 20.0f * log10f(level + OTT_DB_FLOOR);
                float gr_raw = gain_computer_down(level_db,
                                                  left->params.band_thresh[b],
                                                  left->slope_down[b],
                                                  OTT_KNEE_DB);
                float gr = ballistics_decoupled(gr_raw,
                                                &left->band[b].env_down_ac,
                                                &left->band[b].env_down_ra,
                                                alpha_a, alpha_r);
                gr = clamp_gr(gr);
                band_gr_down = gr;
                float gain = clamp_gain(expf(-gr * (float)(M_LN10 / 20.0)));
                x_l *= gain; x_l = flush_denormal(x_l);
            }

            /* Upward: linked RMS detector on the post-downward signal. */
            float su_l = x_l * x_l;
            float su_r = x_r * x_r;
            left->band[b].rms_up_sq =
                alpha_rms * left->band[b].rms_up_sq + (1.0f - alpha_rms) * su_l;
            if (have_r) {
                right->band[b].rms_up_sq =
                    alpha_rms * right->band[b].rms_up_sq + (1.0f - alpha_rms) * su_r;
                float lev_l = sqrtf(left->band[b].rms_up_sq);
                float lev_r = sqrtf(right->band[b].rms_up_sq);
                float level = (lev_l > lev_r) ? lev_l : lev_r;
                float level_db = 20.0f * log10f(level + OTT_DB_FLOOR);
                float gr_raw = gain_computer_up(level_db,
                                                left->params.band_thresh[b],
                                                left->slope_up[b],
                                                OTT_KNEE_DB);
                float gr = ballistics_decoupled(gr_raw,
                                                &left->band[b].env_up_ac,
                                                &left->band[b].env_up_ra,
                                                alpha_a, alpha_r);
                right->band[b].env_up_ac = left->band[b].env_up_ac;
                right->band[b].env_up_ra = left->band[b].env_up_ra;
                gr = clamp_gr(gr);
                band_gr_up = gr;
                float gain = clamp_gain(expf(-gr * (float)(M_LN10 / 20.0)));
                x_l *= gain; x_l = flush_denormal(x_l);
                x_r *= gain; x_r = flush_denormal(x_r);
            } else {
                float level = sqrtf(left->band[b].rms_up_sq);
                float level_db = 20.0f * log10f(level + OTT_DB_FLOOR);
                float gr_raw = gain_computer_up(level_db,
                                                left->params.band_thresh[b],
                                                left->slope_up[b],
                                                OTT_KNEE_DB);
                float gr = ballistics_decoupled(gr_raw,
                                                &left->band[b].env_up_ac,
                                                &left->band[b].env_up_ra,
                                                alpha_a, alpha_r);
                gr = clamp_gr(gr);
                band_gr_up = gr;
                float gain = clamp_gain(expf(-gr * (float)(M_LN10 / 20.0)));
                x_l *= gain; x_l = flush_denormal(x_l);
            }

            left->band_net_gr_db[b] = band_gr_down + band_gr_up;

            wetL += x_l * left->band_gain_lin[b];
            if (have_r) wetR += x_r * right->band_gain_lin[b];
        }

        wetL *= out_g;
        if (have_r) wetR *= out_g;

        float oL = inL[i] * (1.0f - depth) + wetL * depth;
        outL[i] = (isnan(oL) || isinf(oL)) ? 0.0f : flush_denormal(oL);
        if (have_r) {
            float oR = inR[i] * (1.0f - depth) + wetR * depth;
            outR[i] = (isnan(oR) || isinf(oR)) ? 0.0f : flush_denormal(oR);
        }
    }
}
