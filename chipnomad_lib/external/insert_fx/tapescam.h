// Adapted standalone DSP; copyright Charles Vestal. See LICENSE-TAPESCAM.
#pragma once
/*
 * TAPESCAM Audio FX Plugin - Authentic Tape Degradation
 *
 * Port from TapeScamVST - CVCHothouse/TapeScam shared_dsp
 *
 * VST Signal Chain:
 * Input Level -> GainStage (drive/clipping) -> TapeSat -> WowFlutter -> Tone -> Output Level
 *
 * 6 Main Knobs (mapped to Move knobs 2-7):
 * Knob 2: INPUT  - Input level (pre-gain)
 * Knob 3: DRIVE  - GainStage drive amount
 * Knob 4: COLOR  - Saturation character (affects both GainStage and TapeSat)
 * Knob 5: WOBBLE - Wow/Flutter amount
 * Knob 6: TONE   - Tilt EQ (dark to bright)
 * Knob 7: OUTPUT - Output level
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>




#define kTwoPi 6.283185307179586476925286766559f
#define kPi 3.14159265358979323846f

static inline float Clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float dBToLin(float dB) {
    return powf(10.0f, dB / 20.0f);
}

/* ============================================================================
 * GAIN STAGE MODULE - Port from GainStageModule
 *
 * This is the main distortion engine with multi-stage clipping
 * ============================================================================ */

/* Biquad filter state for pre/post filtering */
typedef struct {
    float b0, b1, b2;
    float a1, a2;
    float z1, z2;
} GS_Biquad;

static void GS_BiquadSetHighpass(GS_Biquad *bq, float sampleRate, float freq, float q) {
    float w0 = kTwoPi * freq / sampleRate;
    float cosw0 = cosf(w0);
    float sinw0 = sinf(w0);
    float alpha = sinw0 / (2.0f * q);

    float b0 = (1.0f + cosw0) * 0.5f;
    float b1 = -(1.0f + cosw0);
    float b2 = (1.0f + cosw0) * 0.5f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cosw0;
    float a2 = 1.0f - alpha;

    bq->b0 = b0 / a0;
    bq->b1 = b1 / a0;
    bq->b2 = b2 / a0;
    bq->a1 = a1 / a0;
    bq->a2 = a2 / a0;
}

static void GS_BiquadSetLowpass(GS_Biquad *bq, float sampleRate, float freq, float q) {
    float w0 = kTwoPi * freq / sampleRate;
    float cosw0 = cosf(w0);
    float sinw0 = sinf(w0);
    float alpha = sinw0 / (2.0f * q);

    float b0 = (1.0f - cosw0) * 0.5f;
    float b1 = 1.0f - cosw0;
    float b2 = (1.0f - cosw0) * 0.5f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cosw0;
    float a2 = 1.0f - alpha;

    bq->b0 = b0 / a0;
    bq->b1 = b1 / a0;
    bq->b2 = b2 / a0;
    bq->a1 = a1 / a0;
    bq->a2 = a2 / a0;
}

static float GS_BiquadProcess(GS_Biquad *bq, float x) {
    float y = bq->b0 * x + bq->z1;
    bq->z1 = bq->b1 * x - bq->a1 * y + bq->z2;
    bq->z2 = bq->b2 * x - bq->a2 * y;
    return y;
}


/* ============================================================================
 * WOW/FLUTTER MODULE - Constants and shared utilities
 * ============================================================================ */

#define kMaxDelaySamples 8192
#define kBaseDelayMsL 5.0f
#define kBaseDelayMsR 5.5f

static float WF_InterpolateCubic(const float *buf, size_t size, float index) {
    int idx1 = (int)floorf(index);
    float frac = index - (float)idx1;
    int idx0 = idx1 - 1;
    int idx2 = idx1 + 1;
    int idx3 = idx1 + 2;

    while (idx0 < 0) idx0 += (int)size;
    while (idx1 < 0) idx1 += (int)size;
    while (idx2 < 0) idx2 += (int)size;
    while (idx3 < 0) idx3 += (int)size;
    idx0 %= (int)size;
    idx1 %= (int)size;
    idx2 %= (int)size;
    idx3 %= (int)size;

    float y0 = buf[idx0], y1 = buf[idx1], y2 = buf[idx2], y3 = buf[idx3];
    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

/* ============================================================================
 * TONE MODULE - Shared utilities
 * ============================================================================ */

typedef struct {
    float b0, b1, b2, a1, a2, z1, z2;
} ToneBiquad;

static void Tone_ComputeShelf(ToneBiquad *state, float gainDb, float cutoffHz, int highShelf, float sampleRate) {
    float A = powf(10.0f, gainDb / 40.0f);
    float w0 = kTwoPi * cutoffHz / sampleRate;
    float cosw0 = cosf(w0), sinw0 = sinf(w0);
    float sqrtA = sqrtf(A);
    float alpha = sinw0 / 2.0f * sqrtf(2.0f);

    float b0, b1, b2, a0, a1, a2;
    if (!highShelf) {
        b0 = A * ((A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha);
        b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw0);
        b2 = A * ((A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha);
        a0 = (A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha;
        a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw0);
        a2 = (A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha;
    } else {
        b0 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha);
        b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw0);
        b2 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha);
        a0 = (A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * sqrtA * alpha;
        a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw0);
        a2 = (A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * sqrtA * alpha;
    }

    float invA0 = 1.0f / a0;
    state->b0 = b0 * invA0;
    state->b1 = b1 * invA0;
    state->b2 = b2 * invA0;
    state->a1 = a1 * invA0;
    state->a2 = a2 * invA0;
}

static inline float Tone_ProcessBiquad(ToneBiquad *bq, float x) {
    float y = x * bq->b0 + bq->z1;
    bq->z1 = x * bq->b1 + bq->z2 - bq->a1 * y;
    bq->z2 = x * bq->b2 - bq->a2 * y;
    return y;
}


namespace insert_tape {
/* Instance structure for v2 API */
typedef struct {
    float sampleRate;
    int delayLength, controlRemaining;
    bool controlsDirty;

    /* User parameters (0-1) */
    float param_input;
    float param_drive;
    float param_color;
    float param_wobble;
    float param_tone;
    float param_output;
    float param_outputLevelLin;  /* Computed linear output level */
    float param_noise;           /* 0-1: tape hiss amount */
    int param_age;               /* 0=NEW, 1=USED, 2=WORN */
    int param_speed;             /* 0=HIGH, 1=STD, 2=LOW */
    int param_compression;       /* 0=OFF, 1=LITE, 2=HEAVY */
    int param_widen;             /* 0=OFF, 1=ON */

    /* GainStage state */
    float gs_trimGainLin;
    float gs_channelGainLin;
    float gs_masterVolLin;
    float gs_driveNorm;
    float gs_character;
    float gs_characterDriveScale;
    float gs_stage1Softness;
    float gs_stage2Softness;
    float gs_stage2Asymmetry;
    GS_Biquad gs_preHP[2];
    GS_Biquad gs_preLP[2];
    GS_Biquad gs_postLP[2];

    /* TapeSat state */
    float sat_targetDrive;
    float sat_smoothedDrive;
    float sat_factor;

    /* WowFlutter state */
    float delayBufL[kMaxDelaySamples];
    float delayBufR[kMaxDelaySamples];
    size_t writeIndex;
    float wf_targetAmount;
    float wf_smoothedAmount;
    float wf_activationRamp;
    float wf_crossfadeGain;
    float wf_depthShape;
    float wf_slewAlpha;
    float wf_wowAmount;
    float wf_flutterAmount;
    float wf_wowRateHz;
    float wf_flutterRateHz;
    float wf_wowPhaseL;
    float wf_wowPhaseR;
    float wf_fltPhaseL;
    float wf_fltPhaseR;
    float wf_readStateL;
    float wf_readStateR;
    float wf_prevReadPosL;
    float wf_prevReadPosR;
    float wf_baseDelaySamplesL;
    float wf_baseDelaySamplesR;
    float wf_wowNoiseStateL;
    float wf_wowNoiseStateR;
    float wf_flutterNoiseStateL;
    float wf_flutterNoiseStateR;
    uint32_t randState;

    /* Tone state */
    ToneBiquad tone_lowShelfL;
    ToneBiquad tone_lowShelfR;
    ToneBiquad tone_highShelfL;
    ToneBiquad tone_highShelfR;
    float tone_targetAmount;
    float tone_smoothedAmount;
    float tone_bassGainDb;
    float tone_trebleGainDb;

    /* HissDrop state */
    float hiss_targetAmount;
    float hiss_smoothedAmount;
    float hiss_levelLin;
    float hiss_noiseColorFactor;
    float hiss_pinkState[2][7];  /* 7 pink noise state vars per channel */

    /* LoFi Compressor state */
    int comp_mode;
    float comp_envelopeL;
    float comp_envelopeR;
    float comp_smoothedGainL;
    float comp_smoothedGainR;
    float comp_attackCoeff;
    float comp_releaseCoeff;
    float comp_threshold;
    float comp_ratio;
    float comp_makeupGain;
    float comp_maxBoost;

    /* Age/Speed modifiers */
    float gs_headroomAdjDb;
    float mod_saturationMul;
    float mod_wowDepthScale;
    float mod_flutterDepthScale;

    /* GainStage speed-dependent filter frequencies */
    float gs_preHpCutHz;
    float gs_preLpCutHz;
    float gs_postLpCutHz;

    /* TapeSat HF rolloff */
    GS_Biquad sat_hfRolloff[2];
    float sat_hfRolloffCutoff;

    /* WowFlutter baseline motion */
    float wf_baseAgeDriftDepth;
    float wf_baseSpeedFlutterDepth;
    float wf_ageDriftAlpha;
    float wf_flutterDriftAlpha;
    float wf_driftL;
    float wf_driftR;
    float wf_flutterJitL;
    float wf_flutterJitR;
    float wf_depthMultiplier;
    float wf_wowRandomness;
    float wf_flutterTurbulence;

    /* Stereo blend */
    float hiss_stereoBlend;
    float wf_stereoBlend;

    /* Dropout state per channel */
    struct {
        int active;
        int stage;           /* 0=attack, 1=hold, 2=release */
        int stage_samples;
        int attack_samples;
        int hold_samples;
        int release_samples;
        float min_gain;
        float env;           /* current envelope value 1.0→min_gain→1.0 */
    } dropout_state[2];

    /* Dropout parameters */
    float dropout_rate_hz;
    float dropout_depth_min_db;
    float dropout_depth_max_db;
    float dropout_dur_min_ms;
    float dropout_dur_max_ms;
    float dropout_cluster_prob;
    float dropout_mono_link;
    float dropout_min_rest_sec;
    float dropout_rest_samples;
    float dropout_cluster_window;
} tapescam_instance_t;

static void v2_GS_SetSpeedMode(tapescam_instance_t *inst, int speed, float sampleRate) {
    switch (speed) {
        case 0: /* HIGH - bright/clean */
            inst->gs_preHpCutHz = 144.0f;   /* 160 * 0.9 */
            inst->gs_preLpCutHz = 5720.0f;  /* 5200 * 1.1 */
            inst->gs_postLpCutHz = 17500.0f;
            break;
        case 1: /* STD */
            inst->gs_preHpCutHz = 160.0f;
            inst->gs_preLpCutHz = 4680.0f;  /* 5200 * 0.9 */
            inst->gs_postLpCutHz = 14000.0f;
            break;
        case 2: /* LOW - lo-fi */
            inst->gs_preHpCutHz = 112.0f;   /* 160 * 0.7 */
            inst->gs_preLpCutHz = 3380.0f;  /* 5200 * 0.65 */
            inst->gs_postLpCutHz = 9000.0f;
            break;
        default:
            inst->gs_preHpCutHz = 144.0f;
            inst->gs_preLpCutHz = 5720.0f;
            inst->gs_postLpCutHz = 17500.0f;
            break;
    }

    /* Recompute filter coefficients */
    for (int ch = 0; ch < 2; ch++) {
        GS_BiquadSetHighpass(&inst->gs_preHP[ch], sampleRate, inst->gs_preHpCutHz, 0.7071f);
        GS_BiquadSetLowpass(&inst->gs_preLP[ch], sampleRate, inst->gs_preLpCutHz, 0.7071f);
        GS_BiquadSetLowpass(&inst->gs_postLP[ch], sampleRate, inst->gs_postLpCutHz, 0.7071f);
    }
}

static void v2_GS_Init(tapescam_instance_t *inst, float sampleRate) {
    inst->gs_trimGainLin = 1.0f;
    inst->gs_channelGainLin = 1.0f;
    inst->gs_masterVolLin = 1.0f;
    inst->gs_driveNorm = 0.0f;
    inst->gs_character = 0.0f;
    inst->gs_characterDriveScale = 1.0f;
    inst->gs_stage1Softness = 1.0f;
    inst->gs_stage2Softness = 1.2f;
    inst->gs_stage2Asymmetry = 0.05f;

    /* Initialize filter state */
    for (int ch = 0; ch < 2; ch++) {
        memset(&inst->gs_preHP[ch], 0, sizeof(GS_Biquad));
        memset(&inst->gs_preLP[ch], 0, sizeof(GS_Biquad));
        memset(&inst->gs_postLP[ch], 0, sizeof(GS_Biquad));
        inst->gs_preHP[ch].b0 = 1.0f;
        inst->gs_preLP[ch].b0 = 1.0f;
        inst->gs_postLP[ch].b0 = 1.0f;
    }

    /* Default to HIGH speed (brightest/cleanest) */
    v2_GS_SetSpeedMode(inst, 0, sampleRate);
}

static void v2_GS_SetParams(tapescam_instance_t *inst, float drive, float color) {
    float driveNorm = Clamp(drive, 0.0f, 1.0f);
    float driveShaped = driveNorm * driveNorm;
    float colorNorm = Clamp(color, 0.0f, 1.0f);

    float trimDb = -2.0f + driveShaped * 34.0f;
    float channelDb = -2.0f + driveNorm * 26.0f;

    inst->gs_trimGainLin = dBToLin(trimDb + inst->gs_headroomAdjDb);
    inst->gs_channelGainLin = dBToLin(channelDb + inst->gs_headroomAdjDb);
    inst->gs_driveNorm = driveNorm;
    inst->gs_character = colorNorm;
    inst->gs_characterDriveScale = 0.8f + 1.2f * colorNorm;
    inst->gs_stage1Softness = 0.8f + 0.4f * (1.0f - colorNorm);
    inst->gs_stage2Softness = 1.05f + 1.1f * colorNorm;
    inst->gs_stage2Asymmetry = 0.02f + 0.08f * colorNorm;
}

static void v2_GS_SetOutputLevel(tapescam_instance_t *inst, float level) {
    /* Store for post-chain application - range: -12dB to +6dB */
    float levelDb = -12.0f + level * 18.0f;
    inst->param_outputLevelLin = dBToLin(levelDb);
    /* GainStage now runs at unity */
    inst->gs_masterVolLin = 1.0f;
}

static void v2_GS_AdjustHeadroom(tapescam_instance_t *inst, float headroomDb) {
    inst->gs_headroomAdjDb = Clamp(headroomDb, -12.0f, 6.0f);
}

static float v2_GS_ApplyClipping(tapescam_instance_t *inst, float x, float softness) {
    float drive = 1.0f + inst->gs_driveNorm * inst->gs_characterDriveScale;
    float v = x * drive;
    v = tanhf(v * softness);
    float asymBase = 0.01f + 0.03f * inst->gs_character;
    v += asymBase * v * v * (v >= 0.0f ? 1.0f : -1.0f);
    return v;
}

static void v2_GS_Process(tapescam_instance_t *inst, float *left, float *right, int size) {
    for (int i = 0; i < size; i++) {
        float stage0L = left[i];
        float stage0R = right[i];

        float stage1InL = stage0L * inst->gs_trimGainLin;
        float stage1InR = stage0R * inst->gs_trimGainLin;
        float stage1OutL = v2_GS_ApplyClipping(inst, stage1InL, inst->gs_stage1Softness);
        float stage1OutR = v2_GS_ApplyClipping(inst, stage1InR, inst->gs_stage1Softness);

        float preEqL = GS_BiquadProcess(&inst->gs_preLP[0], GS_BiquadProcess(&inst->gs_preHP[0], stage1OutL));
        float preEqR = GS_BiquadProcess(&inst->gs_preLP[1], GS_BiquadProcess(&inst->gs_preHP[1], stage1OutR));

        float stage2InL = preEqL * inst->gs_channelGainLin;
        float stage2InR = preEqR * inst->gs_channelGainLin;
        float stage2OutL = v2_GS_ApplyClipping(inst, stage2InL, inst->gs_stage2Softness);
        float stage2OutR = v2_GS_ApplyClipping(inst, stage2InR, inst->gs_stage2Softness);
        float asym0 = inst->gs_stage2Asymmetry;
        float asym1 = -inst->gs_stage2Asymmetry;
        stage2OutL += asym0 * stage2OutL * stage2OutL;
        stage2OutR += asym1 * stage2OutR * stage2OutR;

        float postL = GS_BiquadProcess(&inst->gs_postLP[0], stage2OutL);
        float postR = GS_BiquadProcess(&inst->gs_postLP[1], stage2OutR);

        left[i] = Clamp(postL * inst->gs_masterVolLin, -0.98f, 0.98f);
        right[i] = Clamp(postR * inst->gs_masterVolLin, -0.98f, 0.98f);
    }
}

static void v2_TapeSat_Init(tapescam_instance_t *inst, float sampleRate) {
    inst->sat_targetDrive = 0.0f;
    inst->sat_smoothedDrive = 0.0f;
    inst->sat_factor = 0.0f;
    inst->sat_hfRolloffCutoff = 18000.0f;

    /* Initialize HF rolloff filters */
    for (int ch = 0; ch < 2; ch++) {
        memset(&inst->sat_hfRolloff[ch], 0, sizeof(GS_Biquad));
        inst->sat_hfRolloff[ch].b0 = 1.0f;
        GS_BiquadSetLowpass(&inst->sat_hfRolloff[ch], sampleRate, inst->sat_hfRolloffCutoff, 0.7071f);
    }
}

static void v2_TapeSat_SetDrive(tapescam_instance_t *inst, float normalizedDrive) {
    float shaped = Clamp(normalizedDrive, 0.0f, 1.0f);
    shaped = powf(shaped, 0.8f);
    inst->sat_targetDrive = shaped;
}

/* HF rolloff cutoff calculation (from VST) */
static void v2_TapeSat_UpdateHFRolloff(tapescam_instance_t *inst, float sampleRate) {
    /* Drive-dependent: 18kHz at drive=0 → 10kHz at drive=1 */
    float drive = inst->sat_smoothedDrive;
    float cutoff = 18000.0f - drive * 8000.0f;

    /* Apply speed override if set */
    if (inst->sat_hfRolloffCutoff > 0.0f && inst->sat_hfRolloffCutoff < cutoff) {
        cutoff = inst->sat_hfRolloffCutoff;
    }

    GS_BiquadSetLowpass(&inst->sat_hfRolloff[0], sampleRate, cutoff, 0.7071f);
    GS_BiquadSetLowpass(&inst->sat_hfRolloff[1], sampleRate, cutoff, 0.7071f);
}

static void v2_TapeSat_UpdateControls(tapescam_instance_t *inst) {
    float delta = inst->sat_targetDrive - inst->sat_smoothedDrive;
    inst->sat_smoothedDrive += delta * 0.3f;
    if (fabsf(inst->sat_targetDrive - inst->sat_smoothedDrive) < 1e-4f) {
        inst->sat_smoothedDrive = inst->sat_targetDrive;
    }
    inst->sat_factor = Clamp(inst->sat_smoothedDrive * 1.35f * inst->mod_saturationMul, 0.0f, 2.0f);

    /* Update HF rolloff based on smoothed drive */
    v2_TapeSat_UpdateHFRolloff(inst, inst->sampleRate);
}

static void v2_TapeSat_Process(tapescam_instance_t *inst, float *left, float *right, int size) {
    if (inst->sat_smoothedDrive < 0.01f) return;
    float satAmount = inst->sat_factor;
    if (satAmount < 0.01f) return;

    float drive = 1.0f + satAmount * 2.5f;
    for (int i = 0; i < size; i++) {
        /* Saturation */
        left[i] = tanhf(left[i] * drive);
        right[i] = tanhf(right[i] * drive);

        /* HF rolloff (anti-aliasing) */
        left[i] = GS_BiquadProcess(&inst->sat_hfRolloff[0], left[i]);
        right[i] = GS_BiquadProcess(&inst->sat_hfRolloff[1], right[i]);
    }
}

static float v2_WF_NextRand(tapescam_instance_t *inst) {
    inst->randState = inst->randState * 1664525u + 1013904223u;
    return (float)inst->randState * 2.3283064365386963e-10f;
}

static float v2_WF_NextRandCentered(tapescam_instance_t *inst) {
    return v2_WF_NextRand(inst) * 2.0f - 1.0f;
}

/* Constants for baseline motion (from VST) */
#define kMaxAgeDriftDepthSec 0.0015f
#define kMaxSpeedFlutterDepthSec 0.0006f

static void v2_WF_Init(tapescam_instance_t *inst, float sampleRate) {
    inst->writeIndex = 0;

    inst->wf_baseDelaySamplesL = fminf(sampleRate * (kBaseDelayMsL * 0.001f), (float)inst->delayLength - 4.0f);
    inst->wf_baseDelaySamplesR = fminf(sampleRate * (kBaseDelayMsR * 0.001f), (float)inst->delayLength - 4.0f);
    inst->wf_readStateL = inst->wf_baseDelaySamplesL;
    inst->wf_readStateR = inst->wf_baseDelaySamplesR;

    inst->wf_targetAmount = 0.0f;
    inst->wf_smoothedAmount = 0.0f;
    inst->wf_activationRamp = 0.0f;
    inst->wf_crossfadeGain = 0.0f;
    inst->wf_depthShape = 0.0f;
    inst->wf_slewAlpha = 0.9f;
    inst->wf_wowAmount = 0.0f;
    inst->wf_flutterAmount = 0.0f;
    inst->wf_wowRateHz = 0.3f;
    inst->wf_flutterRateHz = 4.0f;
    inst->wf_wowPhaseL = 0.0f;
    inst->wf_wowPhaseR = 0.25f;
    inst->wf_fltPhaseL = 0.0f;
    inst->wf_fltPhaseR = 0.33f;
    inst->wf_wowNoiseStateL = 0.0f;
    inst->wf_wowNoiseStateR = 0.0f;
    inst->wf_flutterNoiseStateL = 0.0f;
    inst->wf_flutterNoiseStateR = 0.0f;

    /* Initialize baseline motion state */
    inst->wf_baseAgeDriftDepth = 0.0f;
    inst->wf_baseSpeedFlutterDepth = 0.0f;
    inst->wf_ageDriftAlpha = 0.0f;
    inst->wf_flutterDriftAlpha = 0.0f;
    inst->wf_driftL = 0.0f;
    inst->wf_driftR = 0.0f;
    inst->wf_flutterJitL = 0.0f;
    inst->wf_flutterJitR = 0.0f;
    inst->wf_depthMultiplier = 1.0f;
    inst->wf_wowRandomness = 0.4f;
    inst->wf_flutterTurbulence = 0.2f;
    inst->wf_stereoBlend = 1.0f;

    inst->randState ^= (uint32_t)sampleRate;
}

/* SetBaselineMotion (from VST) */
static void v2_WF_SetBaselineMotion(tapescam_instance_t *inst, float ageAmount, float speedAmount) {
    inst->wf_baseAgeDriftDepth = ageAmount * kMaxAgeDriftDepthSec;
    inst->wf_baseSpeedFlutterDepth = speedAmount * kMaxSpeedFlutterDepthSec;

    inst->wf_ageDriftAlpha = (inst->wf_baseAgeDriftDepth > 0.0f)
        ? (0.0002f + 0.0012f * ageAmount) : 0.0f;
    inst->wf_flutterDriftAlpha = (inst->wf_baseSpeedFlutterDepth > 0.0f)
        ? (0.0008f + 0.0025f * speedAmount) : 0.0f;
}

/* SetDepthMultiplier (from VST) */
static void v2_WF_SetDepthMultiplier(tapescam_instance_t *inst, float multiplier) {
    inst->wf_depthMultiplier = Clamp(multiplier, 0.5f, 2.0f);

    /* Tie randomness to tape age */
    if (multiplier <= 0.85f) {
        inst->wf_wowRandomness = 0.2f;
        inst->wf_flutterTurbulence = 0.1f;
    } else if (multiplier <= 1.15f) {
        inst->wf_wowRandomness = 0.4f;
        inst->wf_flutterTurbulence = 0.2f;
    } else {
        inst->wf_wowRandomness = 0.75f;
        inst->wf_flutterTurbulence = 0.45f;
    }
}

static void v2_WF_SetAmount(tapescam_instance_t *inst, float amount) {
    inst->wf_targetAmount = Clamp(amount, 0.0f, 1.0f);
}

static void v2_WF_UpdateControls(tapescam_instance_t *inst) {
    inst->wf_smoothedAmount += (inst->wf_targetAmount - inst->wf_smoothedAmount) * 0.3f;
    float targetActive = (inst->wf_targetAmount > 0.0005f) ? 1.0f : 0.0f;
    inst->wf_activationRamp += (targetActive - inst->wf_activationRamp) * 0.4f;

    float amt = inst->wf_smoothedAmount * inst->wf_activationRamp;
    inst->wf_depthShape = amt * amt;

    float depthBlendWow = 0.05f + 0.95f * inst->wf_depthShape;
    float depthBlendFlutter = 0.03f + 0.97f * inst->wf_depthShape;

    float targetSlew = (amt <= 0.0005f) ? 0.97f : Clamp(0.94f - 0.28f * inst->wf_depthShape, 0.7f, 0.95f);
    inst->wf_slewAlpha += (targetSlew - inst->wf_slewAlpha) * 0.2f;

    inst->wf_wowAmount = depthBlendWow * inst->wf_activationRamp;
    inst->wf_flutterAmount = depthBlendFlutter * inst->wf_activationRamp;
}

static void v2_WF_Process(tapescam_instance_t *inst, float *inL, float *inR, float *outL, float *outR, int size, float sampleRate) {
    float wowAmt = inst->wf_wowAmount * inst->wf_depthMultiplier * inst->mod_wowDepthScale;
    float flutterAmt = inst->wf_flutterAmount * inst->wf_depthMultiplier * inst->mod_flutterDepthScale;
    float invSr = 1.0f / sampleRate;

    /* Check if we have any modulation (including baseline drift) */
    int hasBaselineDrift = (inst->wf_ageDriftAlpha > 0.0f || inst->wf_flutterDriftAlpha > 0.0f);
    int bypassDelay = (!hasBaselineDrift && inst->wf_activationRamp < 1.0e-4f && wowAmt <= 1.0e-5f && flutterAmt <= 1.0e-5f);

    if (bypassDelay) {
        for (int i = 0; i < size; i++) {
            outL[i] = inL[i];
            outR[i] = inR[i];
            inst->delayBufL[inst->writeIndex] = inL[i];
            inst->delayBufR[inst->writeIndex] = inR[i];
            inst->writeIndex++;
            if (inst->writeIndex >= inst->delayLength) inst->writeIndex = 0;
        }
        return;
    }

    float crossfadeAlpha = 0.004f;
    float crossfadeGain = inst->wf_crossfadeGain;
    float maxWowDepthSec = 0.035f;
    float maxFlutterDepthSec = 0.002f;
    float maxDevSec = 0.0042f;

    /* Use randomness/turbulence values based on depth multiplier */
    float wowRandomness = inst->wf_wowRandomness;
    float flutterTurbulence = inst->wf_flutterTurbulence;

    for (int i = 0; i < size; i++) {
        float xL = inL[i], xR = inR[i];

        /* Update crossfade (1.0 when active, accounts for baseline drift) */
        float targetCrossfade = (inst->wf_activationRamp > 0.001f || hasBaselineDrift) ? 1.0f : inst->wf_activationRamp;
        crossfadeGain += (targetCrossfade - crossfadeGain) * crossfadeAlpha;

        /* Update noise states with turbulence-scaled smoothing */
        float wowNoiseAlpha = 0.00018f * (1.0f + wowRandomness);
        float flutterNoiseAlpha = 0.00025f * (1.0f + flutterTurbulence);
        inst->wf_wowNoiseStateL += (v2_WF_NextRandCentered(inst) - inst->wf_wowNoiseStateL) * wowNoiseAlpha;
        inst->wf_wowNoiseStateR += (v2_WF_NextRandCentered(inst) - inst->wf_wowNoiseStateR) * wowNoiseAlpha;
        inst->wf_flutterNoiseStateL += (v2_WF_NextRandCentered(inst) - inst->wf_flutterNoiseStateL) * flutterNoiseAlpha;
        inst->wf_flutterNoiseStateR += (v2_WF_NextRandCentered(inst) - inst->wf_flutterNoiseStateR) * flutterNoiseAlpha;

        /* Update baseline drift (age-related constant drift) */
        if (inst->wf_ageDriftAlpha > 0.0f) {
            float targetL = v2_WF_NextRandCentered(inst) * inst->wf_baseAgeDriftDepth;
            float targetR = targetL + 0.12f * v2_WF_NextRandCentered(inst) * inst->wf_baseAgeDriftDepth;
            inst->wf_driftL += (targetL - inst->wf_driftL) * inst->wf_ageDriftAlpha;
            inst->wf_driftR += (targetR - inst->wf_driftR) * inst->wf_ageDriftAlpha;
        } else {
            inst->wf_driftL *= 0.9996f;
            inst->wf_driftR *= 0.9996f;
        }

        /* Update flutter jitter (speed-related flutter) */
        if (inst->wf_flutterDriftAlpha > 0.0f) {
            float targetL = v2_WF_NextRandCentered(inst) * inst->wf_baseSpeedFlutterDepth;
            float targetR = targetL + 0.08f * v2_WF_NextRandCentered(inst) * inst->wf_baseSpeedFlutterDepth;
            inst->wf_flutterJitL += (targetL - inst->wf_flutterJitL) * inst->wf_flutterDriftAlpha;
            inst->wf_flutterJitR += (targetR - inst->wf_flutterJitR) * inst->wf_flutterDriftAlpha;
        } else {
            inst->wf_flutterJitL *= 0.9996f;
            inst->wf_flutterJitR *= 0.9996f;
        }

        /* Update oscillator phases */
        inst->wf_wowPhaseL += inst->wf_wowRateHz * invSr;
        if (inst->wf_wowPhaseL >= 1.0f) inst->wf_wowPhaseL -= 1.0f;
        inst->wf_fltPhaseL += inst->wf_flutterRateHz * invSr;
        if (inst->wf_fltPhaseL >= 1.0f) inst->wf_fltPhaseL -= 1.0f;

        inst->wf_wowPhaseR += inst->wf_wowRateHz * 1.03f * invSr;
        if (inst->wf_wowPhaseR >= 1.0f) inst->wf_wowPhaseR -= 1.0f;
        inst->wf_fltPhaseR += inst->wf_flutterRateHz * 0.97f * invSr;
        if (inst->wf_fltPhaseR >= 1.0f) inst->wf_fltPhaseR -= 1.0f;

        /* Calculate modulation shapes with randomness */
        float depthSlow = sqrtf(fmaxf(inst->wf_depthShape, 0.0f));
        float wowNoiseMix = Clamp(wowRandomness * (0.10f + 0.45f * depthSlow), 0.0f, 0.65f);
        float flutterNoiseMix = Clamp(flutterTurbulence * (0.12f + 0.45f * depthSlow), 0.0f, 0.7f);

        float wowShapeL = (1.0f - wowNoiseMix) * sinf(kTwoPi * inst->wf_wowPhaseL) + wowNoiseMix * inst->wf_wowNoiseStateL;
        float flutterShapeL = (1.0f - flutterNoiseMix) * sinf(kTwoPi * inst->wf_fltPhaseL) + flutterNoiseMix * inst->wf_flutterNoiseStateL;
        float wowShapeR = (1.0f - wowNoiseMix) * sinf(kTwoPi * inst->wf_wowPhaseR) + wowNoiseMix * inst->wf_wowNoiseStateR;
        float flutterShapeR = (1.0f - flutterNoiseMix) * sinf(kTwoPi * inst->wf_fltPhaseR) + flutterNoiseMix * inst->wf_flutterNoiseStateR;

        /* Calculate deviation including baseline drift */
        float wowDepthL = maxWowDepthSec * wowAmt;
        float wowDepthR = maxWowDepthSec * wowAmt * 1.03f;
        float flutterDepthL = maxFlutterDepthSec * flutterAmt;
        float flutterDepthR = maxFlutterDepthSec * flutterAmt * 0.97f;

        /* Total deviation = drift + wow + flutter (in seconds) */
        float devSecL = inst->wf_driftL + inst->wf_flutterJitL + wowDepthL * wowShapeL + flutterDepthL * flutterShapeL;
        float devSecR = inst->wf_driftR + inst->wf_flutterJitR + wowDepthR * wowShapeR + flutterDepthR * flutterShapeR;

        float devSamplesL = devSecL * sampleRate;
        float devSamplesR = devSecR * sampleRate;

        float maxDevSamples = maxDevSec * sampleRate;
        devSamplesL = Clamp(devSamplesL, -maxDevSamples, maxDevSamples);
        devSamplesR = Clamp(devSamplesR, -maxDevSamples, maxDevSamples);

        float targetSamplesL = fmaxf(inst->wf_baseDelaySamplesL + devSamplesL, 0.0002f * sampleRate);
        float targetSamplesR = fmaxf(inst->wf_baseDelaySamplesR + devSamplesR, 0.0002f * sampleRate);

        inst->wf_readStateL = inst->wf_slewAlpha * inst->wf_readStateL + (1.0f - inst->wf_slewAlpha) * targetSamplesL;
        inst->wf_readStateR = inst->wf_slewAlpha * inst->wf_readStateR + (1.0f - inst->wf_slewAlpha) * targetSamplesR;

        float readIndexL = (float)inst->writeIndex - inst->wf_readStateL;
        float readIndexR = (float)inst->writeIndex - inst->wf_readStateR;
        if (readIndexL < 0.0f) readIndexL += (float)inst->delayLength;
        if (readIndexR < 0.0f) readIndexR += (float)inst->delayLength;

        float delayedL = WF_InterpolateCubic(inst->delayBufL, inst->delayLength, readIndexL);
        float delayedR = WF_InterpolateCubic(inst->delayBufR, inst->delayLength, readIndexR);

        inst->delayBufL[inst->writeIndex] = xL;
        inst->delayBufR[inst->writeIndex] = xR;
        inst->writeIndex++;
        if (inst->writeIndex >= inst->delayLength) inst->writeIndex = 0;

        float wet = crossfadeGain, dry = 1.0f - wet;
        outL[i] = dry * xL + wet * delayedL;
        outR[i] = dry * xR + wet * delayedR;
    }
    inst->wf_crossfadeGain = crossfadeGain;
}

static void v2_Tone_Init(tapescam_instance_t *inst) {
    memset(&inst->tone_lowShelfL, 0, sizeof(ToneBiquad));
    memset(&inst->tone_lowShelfR, 0, sizeof(ToneBiquad));
    memset(&inst->tone_highShelfL, 0, sizeof(ToneBiquad));
    memset(&inst->tone_highShelfR, 0, sizeof(ToneBiquad));
    inst->tone_lowShelfL.b0 = inst->tone_lowShelfR.b0 = 1.0f;
    inst->tone_highShelfL.b0 = inst->tone_highShelfR.b0 = 1.0f;
    inst->tone_targetAmount = 0.5f;
    inst->tone_smoothedAmount = 0.5f;
}

static void v2_Tone_SetAmount(tapescam_instance_t *inst, float amount) {
    inst->tone_targetAmount = Clamp(amount, 0.0f, 1.0f);
}

static void v2_Tone_UpdateControls(tapescam_instance_t *inst, float sampleRate) {
    inst->tone_smoothedAmount += (inst->tone_targetAmount - inst->tone_smoothedAmount) * 0.5f;
    float normalized = inst->tone_smoothedAmount * 2.0f - 1.0f;
    float shaped = normalized * (1.0f + 0.9f * fabsf(normalized));
    inst->tone_bassGainDb = -shaped * 15.0f;
    inst->tone_trebleGainDb = shaped * 15.0f;

    Tone_ComputeShelf(&inst->tone_lowShelfL, inst->tone_bassGainDb, 120.0f, 0, sampleRate);
    Tone_ComputeShelf(&inst->tone_lowShelfR, inst->tone_bassGainDb, 120.0f, 0, sampleRate);
    Tone_ComputeShelf(&inst->tone_highShelfL, inst->tone_trebleGainDb, 6000.0f, 1, sampleRate);
    Tone_ComputeShelf(&inst->tone_highShelfR, inst->tone_trebleGainDb, 6000.0f, 1, sampleRate);
}

static void v2_Tone_Process(tapescam_instance_t *inst, float *bufL, float *bufR, int size) {
    for (int i = 0; i < size; i++) {
        float y = Tone_ProcessBiquad(&inst->tone_lowShelfL, bufL[i]);
        bufL[i] = Tone_ProcessBiquad(&inst->tone_highShelfL, y);
        y = Tone_ProcessBiquad(&inst->tone_lowShelfR, bufR[i]);
        bufR[i] = Tone_ProcessBiquad(&inst->tone_highShelfR, y);
    }
}

static void v2_Hiss_Init(tapescam_instance_t *inst) {
    inst->hiss_targetAmount = 0.0f;
    inst->hiss_smoothedAmount = 0.0f;
    inst->hiss_levelLin = 0.0f;
    inst->hiss_noiseColorFactor = 0.0f;
    inst->hiss_stereoBlend = 1.0f;  /* Default to stereo */
    memset(inst->hiss_pinkState, 0, sizeof(inst->hiss_pinkState));
}

static void v2_Hiss_SetAmount(tapescam_instance_t *inst, float amount) {
    inst->hiss_targetAmount = Clamp(amount, 0.0f, 1.0f);
}

static void v2_Hiss_UpdateControls(tapescam_instance_t *inst) {
    float delta = inst->hiss_targetAmount - inst->hiss_smoothedAmount;
    inst->hiss_smoothedAmount += delta * 0.5f;

    float amt = inst->hiss_smoothedAmount;
    if (amt < 0.0005f) {
        inst->hiss_levelLin = 0.0f;
        inst->hiss_noiseColorFactor = 0.0f;
        return;
    }

    /* Map 0-1 to -60dB to -6dB */
    float hissDb = -60.0f + amt * 54.0f;
    inst->hiss_levelLin = dBToLin(hissDb);
    inst->hiss_noiseColorFactor = amt;
}

static float v2_Hiss_ProcessPink(tapescam_instance_t *inst, int ch, float white) {
    float *s = inst->hiss_pinkState[ch];
    s[0] = 0.99886f * s[0] + white * 0.0555179f;
    s[1] = 0.99332f * s[1] + white * 0.0750759f;
    s[2] = 0.96900f * s[2] + white * 0.1538520f;
    s[3] = 0.86650f * s[3] + white * 0.3104856f;
    s[4] = 0.55000f * s[4] + white * 0.5329522f;
    s[5] = -0.7616f * s[5] - white * 0.0168980f;
    float pink = s[0] + s[1] + s[2] + s[3] + s[4] + s[5] + s[6] + white * 0.5362f;
    s[6] = white * 0.115926f;
    return pink * 0.11f;
}

static void v2_Hiss_Process(tapescam_instance_t *inst, float *left, float *right, int size) {
    if (inst->hiss_levelLin <= 0.0f) return;

    float colorFactor = inst->hiss_noiseColorFactor;
    float level = inst->hiss_levelLin;
    float blend = inst->hiss_stereoBlend;  /* 0=mono, 1=stereo */

    for (int i = 0; i < size; i++) {
        float whiteL = v2_WF_NextRandCentered(inst);
        float whiteR = v2_WF_NextRandCentered(inst);

        float noiseL = whiteL;
        float noiseR = whiteR;
        if (colorFactor >= 0.001f) {
            noiseL = (1.0f - colorFactor) * whiteL + colorFactor * v2_Hiss_ProcessPink(inst, 0, whiteL);
            noiseR = (1.0f - colorFactor) * whiteR + colorFactor * v2_Hiss_ProcessPink(inst, 1, whiteR);
        }

        /* Apply stereo blend (0=mono, 1=full stereo) */
        float monoNoise = 0.5f * (noiseL + noiseR);
        noiseL = monoNoise + blend * (noiseL - monoNoise);
        noiseR = monoNoise + blend * (noiseR - monoNoise);

        left[i] += noiseL * level;
        right[i] += noiseR * level;
    }
}

static void v2_Comp_SetMode(tapescam_instance_t *inst, int mode, float sampleRate) {
    inst->comp_mode = mode < 0 ? 0 : (mode > 2 ? 2 : mode);

    float attackMs = inst->comp_mode == 2 ? 2.0f : 5.0f;
    float releaseMs = inst->comp_mode == 2 ? 800.0f : 400.0f;

    inst->comp_attackCoeff = expf(-1.0f / (sampleRate * attackMs * 0.001f));
    inst->comp_releaseCoeff = expf(-1.0f / (sampleRate * releaseMs * 0.001f));

    switch (inst->comp_mode) {
        case 0:
            inst->comp_threshold = 1.0f;
            inst->comp_ratio = 1.0f;
            inst->comp_makeupGain = 1.0f;
            inst->comp_maxBoost = 1.0f;
            break;
        case 1:
            inst->comp_threshold = 0.45f;
            inst->comp_ratio = 8.0f;
            inst->comp_makeupGain = 0.7f;
            inst->comp_maxBoost = 6.0f;
            break;
        case 2:
            inst->comp_threshold = 0.65f;
            inst->comp_ratio = 18.0f;
            inst->comp_makeupGain = 0.5f;
            inst->comp_maxBoost = 12.0f;
            break;
    }
}

static void v2_Comp_Init(tapescam_instance_t *inst, float sampleRate) {
    inst->comp_mode = 0;
    inst->comp_envelopeL = 0.0f;
    inst->comp_envelopeR = 0.0f;
    inst->comp_smoothedGainL = 1.0f;
    inst->comp_smoothedGainR = 1.0f;
    v2_Comp_SetMode(inst, 0, sampleRate);
}

static void v2_Comp_Process(tapescam_instance_t *inst, float *left, float *right, int size) {
    if (inst->comp_mode == 0) return;

    for (int i = 0; i < size; i++) {
        float inL = left[i];
        float inR = right[i];
        float absL = fabsf(inL);
        float absR = fabsf(inR);

        /* Envelope follower */
        float envCoeffL = (absL > inst->comp_envelopeL) ? inst->comp_attackCoeff : inst->comp_releaseCoeff;
        float envCoeffR = (absR > inst->comp_envelopeR) ? inst->comp_attackCoeff : inst->comp_releaseCoeff;
        inst->comp_envelopeL = envCoeffL * inst->comp_envelopeL + (1.0f - envCoeffL) * absL;
        inst->comp_envelopeR = envCoeffR * inst->comp_envelopeR + (1.0f - envCoeffR) * absR;

        /* Calculate upward compression gain */
        float targetGainL = 1.0f, targetGainR = 1.0f;
        if (inst->comp_envelopeL < inst->comp_threshold) {
            float reduction = inst->comp_threshold - inst->comp_envelopeL;
            float normalizedReduction = reduction / inst->comp_threshold;
            targetGainL = (1.0f + (inst->comp_maxBoost - 1.0f) * normalizedReduction) * inst->comp_makeupGain;
        } else {
            targetGainL = inst->comp_makeupGain;
        }
        if (inst->comp_envelopeR < inst->comp_threshold) {
            float reduction = inst->comp_threshold - inst->comp_envelopeR;
            float normalizedReduction = reduction / inst->comp_threshold;
            targetGainR = (1.0f + (inst->comp_maxBoost - 1.0f) * normalizedReduction) * inst->comp_makeupGain;
        } else {
            targetGainR = inst->comp_makeupGain;
        }

        /* Smooth gain changes */
        float smoothCoeffL = (targetGainL < inst->comp_smoothedGainL) ? 0.2f : 0.000125f;
        float smoothCoeffR = (targetGainR < inst->comp_smoothedGainR) ? 0.2f : 0.000125f;
        inst->comp_smoothedGainL += (targetGainL - inst->comp_smoothedGainL) * smoothCoeffL;
        inst->comp_smoothedGainR += (targetGainR - inst->comp_smoothedGainR) * smoothCoeffR;

        /* Apply with safety limiting */
        float maxAllowedGain = inst->comp_mode == 2 ? 15.0f : 8.0f;
        float cappedGainL = fminf(inst->comp_smoothedGainL, maxAllowedGain);
        float cappedGainR = fminf(inst->comp_smoothedGainR, maxAllowedGain);

        float maxGainL = absL > 0.001f ? 0.9f / absL : cappedGainL;
        float maxGainR = absR > 0.001f ? 0.9f / absR : cappedGainR;

        left[i] = inL * fminf(cappedGainL, maxGainL);
        right[i] = inR * fminf(cappedGainR, maxGainR);
    }
}

/* ============================================================================
 * DROPOUT MODULE - Simulates tape dropouts (brief gain dips)
 * ============================================================================ */

static void v2_Dropout_Init(tapescam_instance_t *inst) {
    for (int ch = 0; ch < 2; ch++) {
        inst->dropout_state[ch].active = 0;
        inst->dropout_state[ch].stage = 0;
        inst->dropout_state[ch].stage_samples = 0;
        inst->dropout_state[ch].attack_samples = 0;
        inst->dropout_state[ch].hold_samples = 0;
        inst->dropout_state[ch].release_samples = 0;
        inst->dropout_state[ch].min_gain = 1.0f;
        inst->dropout_state[ch].env = 1.0f;
    }
    inst->dropout_rate_hz = 0.0f;
    inst->dropout_depth_min_db = 0.0f;
    inst->dropout_depth_max_db = 0.0f;
    inst->dropout_dur_min_ms = 0.0f;
    inst->dropout_dur_max_ms = 0.0f;
    inst->dropout_cluster_prob = 0.0f;
    inst->dropout_mono_link = 0.0f;
    inst->dropout_min_rest_sec = 0.0f;
    inst->dropout_rest_samples = 0.0f;
    inst->dropout_cluster_window = 0.0f;
}

static void v2_Dropout_SetParamsForAge(tapescam_instance_t *inst, int age) {
    switch (age) {
        case 0: /* NEW - no dropouts */
            inst->dropout_rate_hz = 0.0f;
            inst->dropout_depth_min_db = 0.0f;
            inst->dropout_depth_max_db = 0.0f;
            inst->dropout_dur_min_ms = 0.0f;
            inst->dropout_dur_max_ms = 0.0f;
            inst->dropout_cluster_prob = 0.0f;
            inst->dropout_mono_link = 1.0f;
            inst->dropout_min_rest_sec = 0.0f;
            break;
        case 1: /* USED */
            inst->dropout_rate_hz = 0.08f;
            inst->dropout_depth_min_db = -2.5f;
            inst->dropout_depth_max_db = -0.8f;
            inst->dropout_dur_min_ms = 40.0f;
            inst->dropout_dur_max_ms = 120.0f;
            inst->dropout_cluster_prob = 0.10f;
            inst->dropout_mono_link = 0.9f;
            inst->dropout_min_rest_sec = 1.5f;
            break;
        case 2: /* WORN */
            inst->dropout_rate_hz = 0.18f;
            inst->dropout_depth_min_db = -6.0f;
            inst->dropout_depth_max_db = -2.5f;
            inst->dropout_dur_min_ms = 60.0f;
            inst->dropout_dur_max_ms = 220.0f;
            inst->dropout_cluster_prob = 0.15f;
            inst->dropout_mono_link = 0.7f;
            inst->dropout_min_rest_sec = 0.8f;
            break;
    }
    inst->dropout_rest_samples = inst->dropout_min_rest_sec * inst->sampleRate;
    inst->dropout_cluster_window = 0.0f;
}

static void v2_Dropout_TriggerEvent(tapescam_instance_t *inst, int ch) {
    /* Random depth between min and max */
    float depthDb = inst->dropout_depth_min_db +
        v2_WF_NextRand(inst) * (inst->dropout_depth_max_db - inst->dropout_depth_min_db);
    float minGain = dBToLin(depthDb);

    /* Random duration */
    float durMs = inst->dropout_dur_min_ms +
        v2_WF_NextRand(inst) * (inst->dropout_dur_max_ms - inst->dropout_dur_min_ms);
    int totalSamples = (int)(durMs * 0.001f * inst->sampleRate);

    /* Divide into attack (20%), hold (50%), release (30%) */
    int attackSamples = totalSamples / 5;
    int holdSamples = totalSamples / 2;
    int releaseSamples = totalSamples - attackSamples - holdSamples;

    inst->dropout_state[ch].active = 1;
    inst->dropout_state[ch].stage = 0;
    inst->dropout_state[ch].stage_samples = 0;
    inst->dropout_state[ch].attack_samples = attackSamples;
    inst->dropout_state[ch].hold_samples = holdSamples;
    inst->dropout_state[ch].release_samples = releaseSamples;
    inst->dropout_state[ch].min_gain = minGain;
    inst->dropout_state[ch].env = 1.0f;
}

static float v2_Dropout_AdvanceState(tapescam_instance_t *inst, int ch) {
    if (!inst->dropout_state[ch].active) {
        return 1.0f;
    }

    float env = inst->dropout_state[ch].env;
    float minGain = inst->dropout_state[ch].min_gain;
    int stage = inst->dropout_state[ch].stage;

    inst->dropout_state[ch].stage_samples++;

    if (stage == 0) { /* Attack: ramp down to min_gain */
        if (inst->dropout_state[ch].attack_samples > 0) {
            float t = (float)inst->dropout_state[ch].stage_samples /
                      (float)inst->dropout_state[ch].attack_samples;
            env = 1.0f - t * (1.0f - minGain);
        }
        if (inst->dropout_state[ch].stage_samples >= inst->dropout_state[ch].attack_samples) {
            inst->dropout_state[ch].stage = 1;
            inst->dropout_state[ch].stage_samples = 0;
            env = minGain;
        }
    } else if (stage == 1) { /* Hold: stay at min_gain */
        env = minGain;
        if (inst->dropout_state[ch].stage_samples >= inst->dropout_state[ch].hold_samples) {
            inst->dropout_state[ch].stage = 2;
            inst->dropout_state[ch].stage_samples = 0;
        }
    } else { /* Release: ramp back to 1.0 */
        if (inst->dropout_state[ch].release_samples > 0) {
            float t = (float)inst->dropout_state[ch].stage_samples /
                      (float)inst->dropout_state[ch].release_samples;
            env = minGain + t * (1.0f - minGain);
        }
        if (inst->dropout_state[ch].stage_samples >= inst->dropout_state[ch].release_samples) {
            inst->dropout_state[ch].active = 0;
            env = 1.0f;
        }
    }

    inst->dropout_state[ch].env = env;
    return env;
}

static void v2_Dropout_Process(tapescam_instance_t *inst, float *left, float *right, int size) {
    if (inst->dropout_rate_hz <= 0.0f) return;

    float triggerProb = inst->dropout_rate_hz / inst->sampleRate;

    for (int i = 0; i < size; i++) {
        /* Decrement rest counter */
        if (inst->dropout_rest_samples > 0.0f) {
            inst->dropout_rest_samples -= 1.0f;
        }

        /* Decrement cluster window */
        if (inst->dropout_cluster_window > 0.0f) {
            inst->dropout_cluster_window -= 1.0f;
        }

        /* Check for new dropout trigger */
        if (inst->dropout_rest_samples <= 0.0f &&
            !inst->dropout_state[0].active && !inst->dropout_state[1].active) {

            float effectiveProb = triggerProb;
            if (inst->dropout_cluster_window > 0.0f) {
                effectiveProb *= (1.0f + inst->dropout_cluster_prob * 10.0f);
            }

            if (v2_WF_NextRand(inst) < effectiveProb) {
                /* Trigger on left channel */
                v2_Dropout_TriggerEvent(inst, 0);

                /* Maybe trigger on right channel too (mono link) */
                if (v2_WF_NextRand(inst) < inst->dropout_mono_link) {
                    v2_Dropout_TriggerEvent(inst, 1);
                }

                /* Set rest period and cluster window */
                inst->dropout_rest_samples = inst->dropout_min_rest_sec * inst->sampleRate;
                inst->dropout_cluster_window = 0.3f * inst->sampleRate; /* 300ms cluster window */
            }
        }

        /* Process dropout envelopes */
        float gainL = v2_Dropout_AdvanceState(inst, 0);
        float gainR = v2_Dropout_AdvanceState(inst, 1);

        left[i] *= gainL;
        right[i] *= gainR;
    }
}

static void v2_ApplyAgeSpeedModifiers(tapescam_instance_t *inst) {
    int age = inst->param_age;
    int speed = inst->param_speed;

    /* Age: 0=NEW, 1=USED, 2=WORN */
    float ageNorm = Clamp(age / 2.0f, 0.0f, 1.0f);

    /* === Age effects === */

    /* Headroom: worn tape has less headroom */
    float ageHeadroomDb = (ageNorm - 0.5f) * 1.5f;

    /* Saturation multiplier */
    float ageSatMul = 1.0f + 0.4f * ageNorm;  /* 1.0→1.4 */

    /* WowFlutter depth multiplier */
    float ageWowMul = 0.8f + 0.5f * ageNorm;  /* 0.8→1.3 */

    /* Dropout parameters */
    v2_Dropout_SetParamsForAge(inst, age);

    /* === Speed effects === */

    float speedHeadroomDb = 0.0f;
    float speedSatMul = 1.0f;
    float wowDepthScale = 1.0f;
    float flutterDepthScale = 1.0f;
    float hfRolloffCutoff = 18000.0f;
    float speedDriftAmount = 0.3f;

    switch (speed) {
        case 0:  /* HIGH - bright/clean */
            speedHeadroomDb = 2.0f;
            speedSatMul = 0.9f;
            wowDepthScale = 0.85f;
            flutterDepthScale = 0.85f;
            hfRolloffCutoff = 18000.0f;
            speedDriftAmount = 0.15f;
            break;
        case 1:  /* STD */
            speedHeadroomDb = -0.5f;
            speedSatMul = 1.0f;
            wowDepthScale = 1.3f;
            flutterDepthScale = 1.0f;
            hfRolloffCutoff = 12000.0f;
            speedDriftAmount = 0.5f;
            break;
        case 2:  /* LOW - lo-fi */
            speedHeadroomDb = -2.0f;
            speedSatMul = 1.25f;
            wowDepthScale = 1.8f;
            flutterDepthScale = 1.1f;
            hfRolloffCutoff = 8000.0f;
            speedDriftAmount = 1.0f;
            break;
    }

    /* Age wow/flutter scales (combined differently from plan for better results) */
    float ageWowScale = 1.0f + 1.6f * ageNorm;
    float ageFlutterScale = 1.0f - 0.2f * ageNorm;

    /* === Apply combined effects === */

    /* Headroom */
    v2_GS_AdjustHeadroom(inst, ageHeadroomDb + speedHeadroomDb);

    /* GainStage speed-dependent filters */
    v2_GS_SetSpeedMode(inst, speed, inst->sampleRate);

    /* Saturation */
    inst->mod_saturationMul = ageSatMul * speedSatMul;

    /* TapeSat HF rolloff cutoff */
    inst->sat_hfRolloffCutoff = hfRolloffCutoff;

    /* WowFlutter */
    inst->mod_wowDepthScale = ageWowScale * wowDepthScale;
    inst->mod_flutterDepthScale = ageFlutterScale * flutterDepthScale;

    /* WowFlutter depth multiplier and baseline motion */
    v2_WF_SetDepthMultiplier(inst, ageWowMul);
    v2_WF_SetBaselineMotion(inst, ageNorm, inst->param_wobble * speedDriftAmount);

    /* Stereo blend (from widen toggle) */
    inst->hiss_stereoBlend = inst->param_widen ? 1.0f : 0.0f;
    inst->wf_stereoBlend = inst->param_widen ? 1.0f : 0.0f;
}

static void v2_Width_Process(tapescam_instance_t *inst, float *left, float *right, int size) {
    if (!inst->param_widen) return;  /* Skip if widen is off */

    const float width = 1.35f;  /* Default widen amount from VST */

    for (int i = 0; i < size; i++) {
        float mid = 0.5f * (left[i] + right[i]);
        float side = 0.5f * (left[i] - right[i]);
        side *= width;
        left[i] = mid + side;
        right[i] = mid - side;
    }
}


static void reset(tapescam_instance_t *inst, float rate) {
 memset(inst,0,sizeof(*inst)); inst->sampleRate=rate;
 inst->delayLength=(int)fminf(8192.0f,fmaxf(16.0f,ceilf(2048.0f*rate/48000.0f))); inst->controlsDirty=true;
    /* Set default parameters (matching VST defaults) */
    inst->param_input = 0.7f;   /* Slight reduction for headroom, matching VST */
    inst->param_drive = 0.0f;
    inst->param_color = 0.0f;
    inst->param_wobble = 0.0f;
    inst->param_tone = 0.5f;
    inst->param_output = 1.0f;
    inst->param_outputLevelLin = 1.0f;  /* Unity default */
    inst->param_noise = 0.0f;
    inst->param_age = 0;          /* NEW */
    inst->param_speed = 0;        /* HIGH (cleanest) */
    inst->param_compression = 0;  /* OFF */
    inst->param_widen = 1;        /* ON by default */
    inst->randState = 0x1234567u;

    /* Initialize DSP modules */
    v2_GS_Init(inst, inst->sampleRate);
    v2_GS_SetParams(inst, inst->param_drive, inst->param_color);
    v2_GS_SetOutputLevel(inst, inst->param_output);

    v2_TapeSat_Init(inst, inst->sampleRate);
    v2_TapeSat_SetDrive(inst, inst->param_color);

    v2_WF_Init(inst, inst->sampleRate);
    v2_WF_SetAmount(inst, inst->param_wobble);

    v2_Tone_Init(inst);
    v2_Tone_SetAmount(inst, inst->param_tone);

    v2_Hiss_Init(inst);
    v2_Hiss_SetAmount(inst, inst->param_noise);

    v2_Comp_Init(inst, inst->sampleRate);

    v2_Dropout_Init(inst);

    inst->gs_headroomAdjDb = 0.0f;
    inst->mod_saturationMul = 1.0f;
    inst->mod_wowDepthScale = 1.0f;
    inst->mod_flutterDepthScale = 1.0f;
    v2_ApplyAgeSpeedModifiers(inst);

}
static void set(tapescam_instance_t *inst, const float *p) {
 inst->param_input=p[0]; inst->param_drive=p[1]; inst->param_color=p[2];
 inst->param_wobble=p[3]; inst->param_tone=p[4]; inst->param_output=p[5];
 v2_GS_SetParams(inst,p[1],p[2]); v2_TapeSat_SetDrive(inst,p[2]);
 v2_WF_SetAmount(inst,p[3]); v2_Tone_SetAmount(inst,p[4]); v2_GS_SetOutputLevel(inst,p[5]);
 inst->controlsDirty=true;
}
static void processChunk(tapescam_instance_t *inst,float *l,float *r,int n) {
 for(int i=0;i<n;++i) {l[i]*=inst->param_input; r[i]*=inst->param_input;}
 v2_GS_Process(inst,l,r,n); v2_TapeSat_Process(inst,l,r,n);
 v2_WF_Process(inst,l,r,l,r,n,inst->sampleRate);
 v2_Hiss_Process(inst,l,r,n); v2_Dropout_Process(inst,l,r,n);
 v2_Tone_Process(inst,l,r,n); v2_Comp_Process(inst,l,r,n); v2_Width_Process(inst,l,r,n);
 for(int i=0;i<n;++i) {l[i]=tanhf(l[i]*inst->param_outputLevelLin*0.95f); r[i]=tanhf(r[i]*inst->param_outputLevelLin*0.95f);}
}
// Original Move host calls the controls once per 128 frames at 48 kHz.
// Keep that cadence independent of caller block size; cache settled filters.
static void process(tapescam_instance_t *inst,float *l,float *r,int n) {
 while(n>0) {
  if(inst->controlRemaining==0) {
   if(inst->controlsDirty || fabsf(inst->sat_targetDrive-inst->sat_smoothedDrive)>1e-7f) v2_TapeSat_UpdateControls(inst);
   v2_WF_UpdateControls(inst);
   if(inst->controlsDirty || fabsf(inst->tone_targetAmount-inst->tone_smoothedAmount)>1e-7f) v2_Tone_UpdateControls(inst,inst->sampleRate);
   if(inst->controlsDirty || fabsf(inst->hiss_targetAmount-inst->hiss_smoothedAmount)>1e-7f) v2_Hiss_UpdateControls(inst);
   inst->controlsDirty=false;
   inst->controlRemaining=(int)fmaxf(1.0f,roundf(128.0f*inst->sampleRate/48000.0f));
  }
  int count=n<inst->controlRemaining?n:inst->controlRemaining;
  processChunk(inst,l,r,count);inst->controlRemaining-=count;l+=count;r+=count;n-=count;
 }
}

}
#undef kTwoPi
#undef kPi
#undef kMaxDelaySamples
#undef kBaseDelayMsL
#undef kBaseDelayMsR
