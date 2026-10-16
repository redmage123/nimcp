//=============================================================================
// nimcp_temperature_scaling.c - Post-hoc confidence calibration (Layer C)
//=============================================================================
/**
 * Implements temperature scaling for confidence calibration.
 *
 * Reference: Guo et al. 2017, "On Calibration of Modern Neural Networks."
 *
 * Calibration strategy:
 *   1. Coarse grid over T in [0.5, 5.0] with step 0.1, evaluating NLL on the
 *      provided (logits, labels) validation set.
 *   2. Identify the bracket containing the grid minimum and refine using
 *      golden-section search to ~1e-3 precision.
 *   3. Compute 15-bin ECE at the optimum for record-keeping.
 *
 * Numerical stability: log-sum-exp trick subtracts max(logits) before exp,
 * preventing overflow at large logit magnitudes. NLL eps clamp 1e-30 prevents
 * log(0) blowup on perfectly-confident wrong answers.
 *
 * Default-path identity guarantee: nimcp_apply_temperature with T within
 * 1e-7 of 1.0 is a no-op (does not even touch logits), so the inference
 * path is bit-for-bit identical until calibration is invoked.
 */

#include "core/brain/calibration/nimcp_temperature_scaling.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"

#include <math.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* --------------------------------------------------------------------------
 * Constants
 * -------------------------------------------------------------------------- */

/** Identity epsilon — within this distance of 1.0, apply_temperature is a no-op. */
#define NIMCP_TEMP_IDENTITY_EPS  1e-7F

/** Search bounds for calibration. Mirrors Guo et al. 2017 Table 2 spread. */
#define NIMCP_TEMP_MIN           0.5F
#define NIMCP_TEMP_MAX           5.0F
#define NIMCP_TEMP_GRID_STEP     0.1F

/** Minimum positive temperature accepted by apply/softmax. */
#define NIMCP_TEMP_FLOOR         1e-3F

/** ECE bin count. 15 matches the canonical reliability-diagram convention. */
#define NIMCP_TEMP_ECE_BINS      15

/** Golden-section ratio: (sqrt(5) - 1) / 2. */
#define NIMCP_TEMP_GOLDEN_R      0.6180339887498949F

/** Golden-section absolute tolerance. */
#define NIMCP_TEMP_GOLDEN_TOL    1e-3F

/** NLL eps to clamp probabilities away from zero before log. */
#define NIMCP_TEMP_LOG_EPS       1e-30F

/* --------------------------------------------------------------------------
 * Microsecond clock helper
 * -------------------------------------------------------------------------- */

static uint64_t calibration_now_us(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)(ts.tv_nsec / 1000);
}

/* --------------------------------------------------------------------------
 * Public: apply temperature in place
 * -------------------------------------------------------------------------- */

nimcp_error_t nimcp_apply_temperature(float* logits, uint32_t n, float T)
{
    if (!logits) return NIMCP_ERROR_NULL_POINTER;
    if (n == 0)  return NIMCP_ERROR_INVALID_PARAM;
    if (!isfinite(T) || T < NIMCP_TEMP_FLOOR) return NIMCP_ERROR_INVALID_PARAM;

    /* Identity fast-path: T == 1.0 must be bit-for-bit no-op so the default
     * inference path stays exactly identical to pre-calibration behavior. */
    if (fabsf(T - 1.0F) <= NIMCP_TEMP_IDENTITY_EPS) {
        return NIMCP_OK;
    }

    const float invT = 1.0F / T;
    for (uint32_t i = 0; i < n; i++) {
        logits[i] *= invT;
    }
    return NIMCP_OK;
}

/* --------------------------------------------------------------------------
 * Public: numerically stable softmax with temperature
 * -------------------------------------------------------------------------- */

nimcp_error_t nimcp_softmax_with_temperature(const float* logits,
                                             uint32_t n,
                                             float T,
                                             float* out_probs)
{
    if (!logits || !out_probs) return NIMCP_ERROR_NULL_POINTER;
    if (n == 0) return NIMCP_ERROR_INVALID_PARAM;
    if (!isfinite(T) || T < NIMCP_TEMP_FLOOR) return NIMCP_ERROR_INVALID_PARAM;

    const float invT = 1.0F / T;

    /* Pass 1: scaled max for log-sum-exp stabilization. */
    float maxval = logits[0] * invT;
    for (uint32_t i = 1; i < n; i++) {
        const float v = logits[i] * invT;
        if (v > maxval) maxval = v;
    }

    /* Pass 2: exp((scaled_logit) - max). */
    double sum = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        const float e = expf(logits[i] * invT - maxval);
        out_probs[i] = e;
        sum += (double)e;
    }

    /* Pass 3: normalize. sum >= 1.0 by construction (max term contributes 1). */
    if (sum <= 0.0) {
        /* Pathological — uniform distribution. */
        const float u = 1.0F / (float)n;
        for (uint32_t i = 0; i < n; i++) out_probs[i] = u;
        return NIMCP_OK;
    }
    const float inv_sum = (float)(1.0 / sum);
    for (uint32_t i = 0; i < n; i++) {
        out_probs[i] *= inv_sum;
    }
    return NIMCP_OK;
}

/* --------------------------------------------------------------------------
 * Internal: NLL on held-out (logits, labels) at temperature T
 * -------------------------------------------------------------------------- */

static double nll_at_temperature(const float* logits_array,
                                 const uint32_t* labels,
                                 uint32_t n_samples,
                                 uint32_t n_classes,
                                 float T,
                                 float* scratch_probs /* size n_classes */)
{
    double total_nll = 0.0;
    for (uint32_t i = 0; i < n_samples; i++) {
        const float* row = logits_array + (size_t)i * (size_t)n_classes;
        nimcp_softmax_with_temperature(row, n_classes, T, scratch_probs);
        const uint32_t lbl = labels[i];
        const float p = (lbl < n_classes) ? scratch_probs[lbl] : 0.0F;
        const float p_safe = (p > NIMCP_TEMP_LOG_EPS) ? p : NIMCP_TEMP_LOG_EPS;
        total_nll -= (double)logf(p_safe);
    }
    return total_nll / (double)n_samples;
}

/* --------------------------------------------------------------------------
 * Internal: 15-bin ECE on held-out set at temperature T
 *
 * For each sample i, predict argmax probability p_i and predicted label.
 * Bin samples by p_i into B equal-width bins covering [0, 1]. ECE is the
 * weighted average gap |bin_accuracy - bin_avg_confidence|.
 * -------------------------------------------------------------------------- */

static double ece_at_temperature(const float* logits_array,
                                 const uint32_t* labels,
                                 uint32_t n_samples,
                                 uint32_t n_classes,
                                 float T,
                                 float* scratch_probs /* size n_classes */)
{
    const uint32_t B = NIMCP_TEMP_ECE_BINS;
    double bin_conf_sum[NIMCP_TEMP_ECE_BINS] = {0};
    double bin_acc_sum[NIMCP_TEMP_ECE_BINS]  = {0};
    uint32_t bin_count[NIMCP_TEMP_ECE_BINS]  = {0};

    for (uint32_t i = 0; i < n_samples; i++) {
        const float* row = logits_array + (size_t)i * (size_t)n_classes;
        nimcp_softmax_with_temperature(row, n_classes, T, scratch_probs);

        /* argmax + max prob */
        uint32_t pred = 0;
        float maxp = scratch_probs[0];
        for (uint32_t k = 1; k < n_classes; k++) {
            if (scratch_probs[k] > maxp) { maxp = scratch_probs[k]; pred = k; }
        }
        const uint32_t lbl = labels[i];
        const int correct = (pred == lbl) ? 1 : 0;

        /* Bin index — clamp to [0, B-1]. */
        int bin_idx = (int)(maxp * (float)B);
        if (bin_idx < 0) bin_idx = 0;
        if (bin_idx >= (int)B) bin_idx = (int)B - 1;

        bin_conf_sum[bin_idx] += (double)maxp;
        bin_acc_sum[bin_idx]  += (double)correct;
        bin_count[bin_idx]    += 1;
    }

    double ece = 0.0;
    for (uint32_t b = 0; b < B; b++) {
        if (bin_count[b] == 0) continue;
        const double avg_conf = bin_conf_sum[b] / (double)bin_count[b];
        const double avg_acc  = bin_acc_sum[b]  / (double)bin_count[b];
        const double gap      = fabs(avg_acc - avg_conf);
        const double weight   = (double)bin_count[b] / (double)n_samples;
        ece += weight * gap;
    }
    return ece;
}

/* --------------------------------------------------------------------------
 * Public: calibrate temperature
 * -------------------------------------------------------------------------- */

nimcp_error_t nimcp_calibrate_temperature(const float* logits_array,
                                          const uint32_t* labels,
                                          uint32_t n_samples,
                                          uint32_t n_classes,
                                          float* out_T,
                                          float* out_ece)
{
    if (!logits_array || !labels || !out_T || !out_ece) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (n_samples == 0 || n_classes < 2) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    float* scratch = (float*)nimcp_malloc(sizeof(float) * (size_t)n_classes);
    if (!scratch) return NIMCP_ERROR_NO_MEMORY;

    /* --- Coarse grid search over [TEMP_MIN, TEMP_MAX] with TEMP_GRID_STEP. --- */
    float best_T = 1.0F;
    double best_nll = nll_at_temperature(logits_array, labels, n_samples,
                                         n_classes, best_T, scratch);

    for (float T = NIMCP_TEMP_MIN; T <= NIMCP_TEMP_MAX + 1e-6F; T += NIMCP_TEMP_GRID_STEP) {
        const double nll = nll_at_temperature(logits_array, labels, n_samples,
                                              n_classes, T, scratch);
        if (nll < best_nll) {
            best_nll = nll;
            best_T = T;
        }
    }

    /* --- Golden-section refine inside [best_T - step, best_T + step]. --- */
    float lo = best_T - NIMCP_TEMP_GRID_STEP;
    float hi = best_T + NIMCP_TEMP_GRID_STEP;
    if (lo < NIMCP_TEMP_MIN) lo = NIMCP_TEMP_MIN;
    if (hi > NIMCP_TEMP_MAX) hi = NIMCP_TEMP_MAX;

    float a = lo;
    float b = hi;
    float c = b - (b - a) * NIMCP_TEMP_GOLDEN_R;
    float d = a + (b - a) * NIMCP_TEMP_GOLDEN_R;

    while (fabsf(b - a) > NIMCP_TEMP_GOLDEN_TOL) {
        const double f_c = nll_at_temperature(logits_array, labels, n_samples,
                                              n_classes, c, scratch);
        const double f_d = nll_at_temperature(logits_array, labels, n_samples,
                                              n_classes, d, scratch);
        if (f_c < f_d) {
            b = d;
        } else {
            a = c;
        }
        c = b - (b - a) * NIMCP_TEMP_GOLDEN_R;
        d = a + (b - a) * NIMCP_TEMP_GOLDEN_R;
    }
    const float T_refined = 0.5F * (a + b);

    /* Compare refined T against grid winner — keep whichever has lower NLL. */
    const double nll_refined = nll_at_temperature(logits_array, labels, n_samples,
                                                  n_classes, T_refined, scratch);
    if (nll_refined < best_nll) {
        best_T = T_refined;
        best_nll = nll_refined;
    }

    /* Final clamp into [TEMP_MIN, TEMP_MAX]. */
    if (best_T < NIMCP_TEMP_MIN) best_T = NIMCP_TEMP_MIN;
    if (best_T > NIMCP_TEMP_MAX) best_T = NIMCP_TEMP_MAX;

    /* ECE at optimum. */
    const double ece = ece_at_temperature(logits_array, labels, n_samples,
                                          n_classes, best_T, scratch);

    *out_T   = best_T;
    *out_ece = (float)ece;

    nimcp_free(scratch);
    (void)best_nll; /* documented in NLL log later if needed */
    return NIMCP_OK;
}

/* --------------------------------------------------------------------------
 * Public: brain-aware wrapper that stores the calibration result
 * -------------------------------------------------------------------------- */

nimcp_error_t nimcp_brain_calibrate_temperature(brain_t brain,
                                                const float** logits_array,
                                                const uint32_t* labels,
                                                uint32_t n_samples,
                                                uint32_t n_classes)
{
    if (!brain || !logits_array || !labels) return NIMCP_ERROR_NULL_POINTER;
    if (n_samples == 0 || n_classes < 2)    return NIMCP_ERROR_INVALID_PARAM;

    /* Pack the row-pointer array into a contiguous [n_samples * n_classes]
     * buffer so the calibration helper can index it as flat row-major. */
    const size_t total = (size_t)n_samples * (size_t)n_classes;
    float* flat = (float*)nimcp_malloc(sizeof(float) * total);
    if (!flat) return NIMCP_ERROR_NO_MEMORY;

    for (uint32_t i = 0; i < n_samples; i++) {
        if (!logits_array[i]) {
            nimcp_free(flat);
            return NIMCP_ERROR_NULL_POINTER;
        }
        memcpy(flat + (size_t)i * (size_t)n_classes,
               logits_array[i],
               sizeof(float) * (size_t)n_classes);
    }

    float T_opt = 1.0F;
    float ece   = -1.0F;
    const nimcp_error_t err = nimcp_calibrate_temperature(flat, labels,
                                                          n_samples, n_classes,
                                                          &T_opt, &ece);
    nimcp_free(flat);
    if (err != NIMCP_OK) return err;

    brain->decoder_temperature                  = T_opt;
    brain->decoder_temperature_calibrated_ece   = ece;
    brain->decoder_temperature_calibrated_at_us = calibration_now_us();
    return NIMCP_OK;
}
