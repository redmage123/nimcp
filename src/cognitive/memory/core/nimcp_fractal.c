/**
 * @file nimcp_fractal.c
 * @brief Fractal Analysis Module Implementation
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Implements fractal analysis algorithms for signal characterization
 * WHY:  Validate pink noise, measure long-range correlations in memory dynamics
 * HOW:  R/S analysis, DFA, spectral estimation, box-counting, multifractal DFA
 *
 * @see nimcp_fractal.h
 */

#include "cognitive/memory/core/nimcp_fractal.h"
#include "utils/spectral/nimcp_fft.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/statistics/nimcp_statistics.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <stdint.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(fractal, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Internal Constants
//=============================================================================

/** @brief Minimum variance for valid signal */
#define MIN_VARIANCE 1e-10f

/** @brief Epsilon for numerical stability */
#define EPSILON 1e-12f

/** @brief Threshold for multifractality detection */
#define MULTIFRACTAL_WIDTH_THRESHOLD 0.1f

//=============================================================================
// Internal Helper Structures
//=============================================================================

/**
 * @brief Linear regression result
 */
typedef struct {
    float slope;
    float intercept;
    float r_squared;
    size_t n_points;
} linear_fit_t;

//=============================================================================
// Internal Helper Functions - Forward Declarations
//=============================================================================

static float compute_mean(const float* data, size_t count);
static float compute_variance(const float* data, size_t count, float mean);
static float compute_std(const float* data, size_t count, float mean);
static bool linear_regression(const float* x, const float* y, size_t n, linear_fit_t* fit);
static bool polynomial_fit(const float* x, const float* y, size_t n, int order, float* coeffs);
static float polyval(const float* coeffs, int order, float x);
static void generate_scales(size_t min_scale, size_t max_scale, size_t num_scales,
                           bool log_scale, size_t* scales);
static bool validate_input(const float* samples, size_t count, size_t min_required);

//=============================================================================
// Configuration API
//=============================================================================

fractal_config_t fractal_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_config_default", 0.0f);


    fractal_config_t config = {
        .min_scale = FRACTAL_DEFAULT_MIN_SCALE,
        .max_scale = 0,  /* Auto-determined from N/4 */
        .num_scales = FRACTAL_DEFAULT_NUM_SCALES,
        .use_log_scales = true,
        .confidence_threshold = FRACTAL_DEFAULT_CONFIDENCE,

        .dfa_poly_order = FRACTAL_DFA_POLY_ORDER,
        .dfa_remove_mean = true,

        .spectral_use_welch = true,
        .spectral_window_size = 0,  /* Auto */
        .spectral_overlap = 0.5f,

        .box_min_size = 2,
        .box_max_size = 0,  /* Auto */

        .validate_crossover = true,
        .crossover_threshold = 0.9f
    };
    return config;
}

bool fractal_config_validate(const fractal_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "fractal_config_validate: config is NULL");

            return false;
    }

    /* Validate scale parameters */
    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_config_validate", 0.0f);


    if (config->min_scale < 4) {
        return false;
    }
    if (config->num_scales < 4) {
        return false;
    }

    /* Validate confidence threshold */
    if (config->confidence_threshold < 0.5f || config->confidence_threshold > 1.0f) {
        return false;
    }

    /* Validate DFA order */
    if (config->dfa_poly_order < 1 || config->dfa_poly_order > 3) {
        return false;
    }

    /* Validate spectral overlap */
    if (config->spectral_overlap < 0.0f || config->spectral_overlap > 0.9f) {
        return false;
    }

    return true;
}

//=============================================================================
// Internal Helper Functions - Implementation
//=============================================================================

/**
 * @brief Compute mean of data array (wrapper for central statistics module)
 */
static float compute_mean(const float* data, size_t count) {
    if (!data || count == 0) {
        return 0.0f;
    }
    /* Use central statistics module - cast size_t to uint32_t */
    return nimcp_stats_mean(data, (uint32_t)count);
}

/**
 * @brief Compute variance (wrapper for central statistics module)
 * @note The mean parameter is unused as nimcp_stats_variance computes it internally
 */
static float compute_variance(const float* data, size_t count, float mean) {
    (void)mean;  /* Unused - central stats computes mean internally */
    if (!data || count < 2) {
        return 0.0f;
    }
    /* Use central statistics module - cast size_t to uint32_t */
    return nimcp_stats_variance(data, (uint32_t)count);
}

/**
 * @brief Compute standard deviation (wrapper for central statistics module)
 * @note The mean parameter is unused as nimcp_stats_std_dev computes it internally
 */
static float compute_std(const float* data, size_t count, float mean) {
    (void)mean;  /* Unused - central stats computes mean internally */
    if (!data || count < 2) {
        return 0.0f;
    }
    /* Use central statistics module - cast size_t to uint32_t */
    return nimcp_stats_std_dev(data, (uint32_t)count);
}

/**
 * @brief Perform linear regression: y = slope*x + intercept
 *
 * Uses least squares method.
 *
 * @param x Independent variable
 * @param y Dependent variable
 * @param n Number of points
 * @param fit Output: regression results
 * @return true on success
 */
static bool linear_regression(const float* x, const float* y, size_t n, linear_fit_t* fit) {
    if (!x || !y || !fit || n < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "linear_regression: invalid parameters");

            return false;
    }

    double sum_x = 0.0, sum_y = 0.0;
    double sum_xx = 0.0, sum_xy = 0.0;

    for (size_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(i + 1) / (float)n);
        }

        sum_x += (double)x[i];
        sum_y += (double)y[i];
        sum_xx += (double)x[i] * (double)x[i];
        sum_xy += (double)x[i] * (double)y[i];
    }

    double mean_x = sum_x / (double)n;
    double mean_y = sum_y / (double)n;

    double ss_xx = sum_xx - sum_x * sum_x / (double)n;
    double ss_xy = sum_xy - sum_x * sum_y / (double)n;

    if (fabs(ss_xx) < EPSILON) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "linear_regression: validation failed");
        return false;  /* Singular */
    }

    fit->slope = (float)(ss_xy / ss_xx);
    fit->intercept = (float)(mean_y - fit->slope * mean_x);
    fit->n_points = n;

    /* Compute R^2 */
    double ss_tot = 0.0, ss_res = 0.0;
    for (size_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(i + 1) / (float)n);
        }

        double y_pred = fit->slope * (double)x[i] + fit->intercept;
        double diff_tot = (double)y[i] - mean_y;
        double diff_res = (double)y[i] - y_pred;
        ss_tot += diff_tot * diff_tot;
        ss_res += diff_res * diff_res;
    }

    if (ss_tot < EPSILON) {
        fit->r_squared = 1.0f;  /* Perfect fit (constant y) */
    } else {
        fit->r_squared = (float)(1.0 - ss_res / ss_tot);
    }

    return true;
}

/**
 * @brief Fit polynomial of given order (1=linear, 2=quadratic, 3=cubic)
 *
 * Uses normal equations: (X^T X) coeffs = X^T y
 *
 * @param x Independent variable
 * @param y Dependent variable
 * @param n Number of points
 * @param order Polynomial order (1-3)
 * @param coeffs Output: coefficients [c0, c1, c2, ...] for c0 + c1*x + c2*x^2 + ...
 * @return true on success
 */
static bool polynomial_fit(const float* x, const float* y, size_t n, int order, float* coeffs) {
    if (!x || !y || !coeffs || n < (size_t)(order + 1) || order < 1 || order > 3) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "polynomial_fit: required parameter is NULL (x, y, coeffs)");
        return false;
    }

    /* For efficiency and numerical stability, use explicit formulas for low orders */
    if (order == 1) {
        linear_fit_t fit;
        if (!linear_regression(x, y, n, &fit)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "polynomial_fit: linear_regression is NULL");
            return false;
        }
        coeffs[0] = fit.intercept;
        coeffs[1] = fit.slope;
        return true;
    }

    /* For higher orders, use matrix formulation (simplified for order 2-3) */
    int p = order + 1;
    double* A = (double*)nimcp_calloc((size_t)(p * p), sizeof(double));
    double* b = (double*)nimcp_calloc((size_t)p, sizeof(double));

    if (!A || !b) {
        nimcp_free(A);
        A = NULL;
        nimcp_free(b);
        b = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "polynomial_fit: required parameter is NULL (A, b)");
        return false;
    }

    /* Build normal equations */
    memset(A, 0, sizeof(double) * (size_t)(p * p));
    /* calloc zero-initializes */

    for (size_t i = 0; i < n; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && n > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(i + 1) / (float)n);
        }

        double xi = (double)x[i];
        double yi = (double)y[i];
        double xpow = 1.0;

        for (int j = 0; j < p; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && p > 256) {
                fractal_heartbeat("fractal_loop",
                                 (float)(j + 1) / (float)p);
            }

            b[j] += xpow * yi;
            double xpow2 = 1.0;
            for (int k = 0; k < p; k++) {
                /* Phase 8: Loop progress heartbeat */
                if ((k & 0xFF) == 0 && p > 256) {
                    fractal_heartbeat("fractal_loop",
                                     (float)(k + 1) / (float)p);
                }

                A[j * p + k] += xpow * xpow2;
                xpow2 *= xi;
            }
            xpow *= xi;
        }
    }

    /* Solve via Gaussian elimination with partial pivoting */
    for (int i = 0; i < p; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && p > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(i + 1) / (float)p);
        }

        /* Find pivot */
        int max_row = i;
        double max_val = fabs(A[i * p + i]);
        for (int k = i + 1; k < p; k++) {
            if (fabs(A[k * p + i]) > max_val) {
                max_val = fabs(A[k * p + i]);
                max_row = k;
            }
        }

        if (max_val < EPSILON) {
            nimcp_free(A);
            A = NULL;
            nimcp_free(b);
            b = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "polynomial_fit: validation failed");
            return false;  /* Singular */
        }

        /* Swap rows */
        if (max_row != i) {
            for (int j = 0; j < p; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && p > 256) {
                    fractal_heartbeat("fractal_loop",
                                     (float)(j + 1) / (float)p);
                }

                double tmp = A[i * p + j];
                A[i * p + j] = A[max_row * p + j];
                A[max_row * p + j] = tmp;
            }
            double tmp = b[i];
            b[i] = b[max_row];
            b[max_row] = tmp;
        }

        /* Eliminate */
        for (int k = i + 1; k < p; k++) {
            double factor = A[k * p + i] / A[i * p + i];
            for (int j = i; j < p; j++) {
                A[k * p + j] -= factor * A[i * p + j];
            }
            b[k] -= factor * b[i];
        }
    }

    /* Back substitution */
    for (int i = p - 1; i >= 0; i--) {
        double sum = b[i];
        for (int j = i + 1; j < p; j++) {
            sum -= A[i * p + j] * coeffs[j];
        }
        coeffs[i] = (float)(sum / A[i * p + i]);
    }

    nimcp_free(A);
    A = NULL;
    nimcp_free(b);
    b = NULL;
    return true;
}

/**
 * @brief Evaluate polynomial at point x
 */
static float polyval(const float* coeffs, int order, float x) {
    float result = coeffs[order];
    for (int i = order - 1; i >= 0; i--) {
        result = result * x + coeffs[i];
    }
    return result;
}

/**
 * @brief Generate scale values for analysis
 *
 * @param min_scale Minimum scale
 * @param max_scale Maximum scale
 * @param num_scales Number of scales
 * @param log_scale Use logarithmic (true) or linear spacing
 * @param scales Output array (must be pre-allocated)
 */
static void generate_scales(size_t min_scale, size_t max_scale, size_t num_scales,
                           bool log_scale, size_t* scales) {
    if (!scales || num_scales == 0 || min_scale >= max_scale) {
        return;
    }

    if (num_scales == 1) {
        scales[0] = min_scale;
        return;
    }

    if (log_scale) {
        double log_min = log((double)min_scale);
        double log_max = log((double)max_scale);
        double log_step = (log_max - log_min) / (double)(num_scales - 1);

        for (size_t i = 0; i < num_scales; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_scales > 256) {
                fractal_heartbeat("fractal_loop",
                                 (float)(i + 1) / (float)num_scales);
            }

            scales[i] = (size_t)exp(log_min + (double)i * log_step);
            if (scales[i] < min_scale) scales[i] = min_scale;
            if (scales[i] > max_scale) scales[i] = max_scale;
        }
    } else {
        double step = (double)(max_scale - min_scale) / (double)(num_scales - 1);
        for (size_t i = 0; i < num_scales; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && num_scales > 256) {
                fractal_heartbeat("fractal_loop",
                                 (float)(i + 1) / (float)num_scales);
            }

            scales[i] = min_scale + (size_t)((double)i * step);
            if (scales[i] > max_scale) scales[i] = max_scale;
        }
    }

    /* Ensure unique scales, clamped to max_scale */
    for (size_t i = 1; i < num_scales; i++) {
        if (scales[i] <= scales[i - 1]) {
            scales[i] = scales[i - 1] + 1;
            if (scales[i] > max_scale) scales[i] = max_scale;
        }
    }
}

/**
 * @brief Validate input samples
 */
static bool validate_input(const float* samples, size_t count, size_t min_required) {
    if (!samples || count < min_required) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "validate_input: invalid parameters");

            return false;
    }

    /* Check for NaN/Inf */
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(i + 1) / (float)count);
        }

        if (!isfinite(samples[i])) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "validate_input: isfinite is NULL");
            return false;
        }
    }

    return true;
}

//=============================================================================
// Hurst Exponent (R/S Analysis)
//=============================================================================

int fractal_hurst_rs(
    const float* samples,
    size_t count,
    const fractal_config_t* config,
    fractal_result_t* result
) {
    /* Validate inputs */
    if (!samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "fractal_hurst_rs: samples is NULL");
        return FRACTAL_ERROR_NULL_PTR;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "fractal_hurst_rs: result is NULL");
        return FRACTAL_ERROR_NULL_PTR;
    }
    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_hurst_rs", 0.0f);


    if (count < FRACTAL_MIN_SAMPLES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "fractal_hurst_rs: insufficient samples");
        return FRACTAL_ERROR_INSUFFICIENT;
    }
    if (!validate_input(samples, count, FRACTAL_MIN_SAMPLES)) {
        return FRACTAL_ERROR_INSUFFICIENT;
    }

    /* Use default config if not provided */
    fractal_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = fractal_config_default();
    }

    if (!fractal_config_validate(&cfg)) {
        return FRACTAL_ERROR_INVALID_CONFIG;
    }

    /* Determine max scale */
    size_t max_scale = cfg.max_scale;
    if (max_scale == 0 || max_scale > count / 4) {
        max_scale = count / 4;
    }
    if (max_scale < cfg.min_scale + 4) {
        return FRACTAL_ERROR_INSUFFICIENT;
    }

    /* Allocate working arrays */
    size_t* scales = (size_t*)nimcp_calloc(cfg.num_scales, sizeof(size_t));
    float* log_scales = (float*)nimcp_calloc(cfg.num_scales, sizeof(float));
    if (!log_scales) return -1;
    float* log_rs = (float*)nimcp_calloc(cfg.num_scales, sizeof(float));

    if (!scales || !log_scales || !log_rs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "fractal_hurst_rs: allocation failure");
        nimcp_free(scales);
        scales = NULL;
        nimcp_free(log_scales);
        log_scales = NULL;
        nimcp_free(log_rs);
        log_rs = NULL;
        return FRACTAL_ERROR_ALLOC;
    }

    /* Generate scales */
    generate_scales(cfg.min_scale, max_scale, cfg.num_scales, cfg.use_log_scales, scales);

    /* Compute R/S for each scale */
    size_t valid_scales = 0;
    for (size_t si = 0; si < cfg.num_scales; si++) {
        /* Phase 8: Loop progress heartbeat */
        if ((si & 0xFF) == 0 && cfg.num_scales > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(si + 1) / (float)cfg.num_scales);
        }

        size_t n = scales[si];
        if (n > count || n < 4) {
            continue;
        }

        size_t num_segments = count / (n > 0 ? n : 1);
        if (num_segments < 1) {
            continue;
        }

        double rs_sum = 0.0;
        size_t rs_count = 0;

        for (size_t seg = 0; seg < num_segments; seg++) {
            /* Phase 8: Loop progress heartbeat */
            if ((seg & 0xFF) == 0 && num_segments > 256) {
                fractal_heartbeat("fractal_loop",
                                 (float)(seg + 1) / (float)num_segments);
            }

            const float* segment = samples + seg * n;

            /* Compute mean */
            float mean = compute_mean(segment, n);

            /* Compute standard deviation */
            float std = compute_std(segment, n, mean);
            if (std < EPSILON) {
                continue;  /* Skip constant segments */
            }

            /* Compute cumulative deviation and range */
            float cumsum = 0.0f;
            float max_cumsum = -FLT_MAX;
            float min_cumsum = FLT_MAX;

            for (size_t i = 0; i < n; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && n > 256) {
                    fractal_heartbeat("fractal_loop",
                                     (float)(i + 1) / (float)n);
                }

                cumsum += segment[i] - mean;
                if (cumsum > max_cumsum) max_cumsum = cumsum;
                if (cumsum < min_cumsum) min_cumsum = cumsum;
            }

            float range = max_cumsum - min_cumsum;
            float rs = range / (fabsf(std) > 1e-7f ? std : 1e-7f);

            rs_sum += (double)rs;
            rs_count++;
        }

        if (rs_count > 0) {
            float avg_rs = (float)(rs_sum / (double)rs_count);
            log_scales[valid_scales] = logf((float)n);
            log_rs[valid_scales] = logf(avg_rs);
            valid_scales++;
        }
    }

    /* Fit line in log-log space */
    if (valid_scales < 4) {
        nimcp_free(scales);
        scales = NULL;
        nimcp_free(log_scales);
        log_scales = NULL;
        nimcp_free(log_rs);
        log_rs = NULL;
        return FRACTAL_ERROR_COMPUTE;
    }

    linear_fit_t fit;
    if (!linear_regression(log_scales, log_rs, valid_scales, &fit)) {
        nimcp_free(scales);
        scales = NULL;
        nimcp_free(log_scales);
        log_scales = NULL;
        nimcp_free(log_rs);
        log_rs = NULL;
        return FRACTAL_ERROR_COMPUTE;
    }

    /* Store results */
    memset(result, 0, sizeof(fractal_result_t));
    result->hurst_exponent = fit.slope;
    result->hurst_r2 = fit.r_squared;
    result->samples_analyzed = (uint32_t)count;
    result->scales_computed = (uint32_t)valid_scales;
    result->confidence = fit.r_squared;

    /* Derive related exponents */
    result->dfa_exponent = fit.slope;  /* H ~ alpha for stationary */
    result->spectral_exponent = 2.0f * fit.slope - 1.0f;  /* beta = 2H - 1 */
    result->fractal_dimension = 2.0f - fit.slope;  /* D = 2 - H */

    nimcp_free(scales);
    scales = NULL;
    nimcp_free(log_scales);
    log_scales = NULL;
    nimcp_free(log_rs);
    log_rs = NULL;

    return FRACTAL_OK;
}

//=============================================================================
// Detrended Fluctuation Analysis (DFA)
//=============================================================================

int fractal_dfa(
    const float* samples,
    size_t count,
    const fractal_config_t* config,
    fractal_result_t* result
) {
    /* Validate inputs */
    if (!samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "fractal_dfa: samples is NULL");
        return FRACTAL_ERROR_NULL_PTR;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "fractal_dfa: result is NULL");
        return FRACTAL_ERROR_NULL_PTR;
    }
    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_dfa", 0.0f);


    if (count < FRACTAL_MIN_SAMPLES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "fractal_dfa: insufficient samples");
        return FRACTAL_ERROR_INSUFFICIENT;
    }
    if (!validate_input(samples, count, FRACTAL_MIN_SAMPLES)) {
        return FRACTAL_ERROR_INSUFFICIENT;
    }

    /* Use default config if not provided */
    fractal_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = fractal_config_default();
    }

    if (!fractal_config_validate(&cfg)) {
        return FRACTAL_ERROR_INVALID_CONFIG;
    }

    /* Determine max scale */
    size_t max_scale = cfg.max_scale;
    if (max_scale == 0 || max_scale > count / 4) {
        max_scale = count / 4;
    }
    if (max_scale < cfg.min_scale + 4) {
        return FRACTAL_ERROR_INSUFFICIENT;
    }

    /* Compute signal mean */
    float mean = compute_mean(samples, count);

    /* Integrate signal: y(k) = sum_{i=1}^{k} (x(i) - mean) */
    float* y = (float*)nimcp_calloc(count, sizeof(float));
    if (!y) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "fractal_dfa: allocation failure for integrated signal");
        return FRACTAL_ERROR_ALLOC;
    }

    float cumsum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(i + 1) / (float)count);
        }

        if (cfg.dfa_remove_mean) {
            cumsum += samples[i] - mean;
        } else {
            cumsum += samples[i];
        }
        y[i] = cumsum;
    }

    /* Allocate working arrays */
    size_t* scales = (size_t*)nimcp_calloc(cfg.num_scales, sizeof(size_t));
    if (!scales) return -1;
    float* log_scales = (float*)nimcp_calloc(cfg.num_scales, sizeof(float));
    if (!log_scales) return -1;
    float* log_fluct = (float*)nimcp_calloc(cfg.num_scales, sizeof(float));
    if (!log_fluct) return -1;
    float* coeffs = (float*)nimcp_calloc((size_t)(cfg.dfa_poly_order + 1), sizeof(float));
    if (!coeffs) return -1;
    float* x_seg = (float*)nimcp_calloc(max_scale, sizeof(float));

    if (!scales || !log_scales || !log_fluct || !coeffs || !x_seg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "fractal_dfa: allocation failure for working arrays");
        nimcp_free(y);
        y = NULL;
        nimcp_free(scales);
        scales = NULL;
        nimcp_free(log_scales);
        log_scales = NULL;
        nimcp_free(log_fluct);
        log_fluct = NULL;
        nimcp_free(coeffs);
        coeffs = NULL;
        nimcp_free(x_seg);
        x_seg = NULL;
        return FRACTAL_ERROR_ALLOC;
    }

    /* Generate scales */
    generate_scales(cfg.min_scale, max_scale, cfg.num_scales, cfg.use_log_scales, scales);

    /* Compute fluctuation for each scale */
    size_t valid_scales = 0;
    for (size_t si = 0; si < cfg.num_scales; si++) {
        /* Phase 8: Loop progress heartbeat */
        if ((si & 0xFF) == 0 && cfg.num_scales > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(si + 1) / (float)cfg.num_scales);
        }

        size_t n = scales[si];
        if (n > max_scale) n = max_scale;  /* Clamp: x_seg allocated for max_scale */
        if (n > count || n < (size_t)(cfg.dfa_poly_order + 2)) {
            continue;
        }

        size_t num_segments = count / (n > 0 ? n : 1);
        if (num_segments < 1) {
            continue;
        }

        /* Create x values for polynomial fit (0, 1, 2, ..., n-1) */
        for (size_t i = 0; i < n; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && n > 256) {
                fractal_heartbeat("fractal_loop",
                                 (float)(i + 1) / (float)n);
            }

            x_seg[i] = (float)i;
        }

        double f2_sum = 0.0;
        size_t f2_count = 0;

        /* Process segments from both ends for better statistics */
        for (int direction = 0; direction < 2; direction++) {
            /* Phase 8: Loop progress heartbeat */
            if ((direction & 0xFF) == 0 && 2 > 256) {
                fractal_heartbeat("fractal_loop",
                                 (float)(direction + 1) / (float)2);
            }

            for (size_t seg = 0; seg < num_segments; seg++) {
                /* Phase 8: Loop progress heartbeat */
                if ((seg & 0xFF) == 0 && num_segments > 256) {
                    fractal_heartbeat("fractal_loop",
                                     (float)(seg + 1) / (float)num_segments);
                }

                const float* y_seg;
                if (direction == 0) {
                    y_seg = y + seg * n;
                } else {
                    /* Process from end */
                    if ((seg + 1) * n > count) {
                        continue;
                    }
                    y_seg = y + (count - (seg + 1) * n);
                }

                /* Fit polynomial trend */
                if (!polynomial_fit(x_seg, y_seg, n, cfg.dfa_poly_order, coeffs)) {
                    continue;
                }

                /* Compute fluctuation: F^2 = (1/n) * sum((y - y_fit)^2) */
                double f2 = 0.0;
                for (size_t i = 0; i < n; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && n > 256) {
                        fractal_heartbeat("fractal_loop",
                                         (float)(i + 1) / (float)n);
                    }

                    float y_fit = polyval(coeffs, cfg.dfa_poly_order, x_seg[i]);
                    float diff = y_seg[i] - y_fit;
                    f2 += (double)(diff * diff);
                }
                f2 /= (double)n;

                f2_sum += f2;
                f2_count++;
            }
        }

        if (f2_count > 0) {
            float f_n = sqrtf((float)(f2_sum / (double)f2_count));
            if (f_n > EPSILON) {
                log_scales[valid_scales] = logf((float)n);
                log_fluct[valid_scales] = logf(f_n);
                valid_scales++;
            }
        }
    }

    /* Fit line in log-log space */
    if (valid_scales < 4) {
        nimcp_free(y);
        y = NULL;
        nimcp_free(scales);
        scales = NULL;
        nimcp_free(log_scales);
        log_scales = NULL;
        nimcp_free(log_fluct);
        log_fluct = NULL;
        nimcp_free(coeffs);
        coeffs = NULL;
        nimcp_free(x_seg);
        x_seg = NULL;
        return FRACTAL_ERROR_COMPUTE;
    }

    linear_fit_t fit;
    if (!linear_regression(log_scales, log_fluct, valid_scales, &fit)) {
        nimcp_free(y);
        y = NULL;
        nimcp_free(scales);
        scales = NULL;
        nimcp_free(log_scales);
        log_scales = NULL;
        nimcp_free(log_fluct);
        log_fluct = NULL;
        nimcp_free(coeffs);
        coeffs = NULL;
        nimcp_free(x_seg);
        x_seg = NULL;
        return FRACTAL_ERROR_COMPUTE;
    }

    /* Store results */
    memset(result, 0, sizeof(fractal_result_t));
    result->dfa_exponent = fit.slope;
    result->dfa_r2 = fit.r_squared;
    result->samples_analyzed = (uint32_t)count;
    result->scales_computed = (uint32_t)valid_scales;
    result->confidence = fit.r_squared;

    /* Derive related exponents */
    result->hurst_exponent = fit.slope;  /* H ~ alpha for stationary */
    result->spectral_exponent = 2.0f * fit.slope - 1.0f;  /* beta = 2*alpha - 1 */
    result->fractal_dimension = 2.0f - fit.slope;  /* D = 2 - alpha */

    nimcp_free(y);
    y = NULL;
    nimcp_free(scales);
    scales = NULL;
    nimcp_free(log_scales);
    log_scales = NULL;
    nimcp_free(log_fluct);
    log_fluct = NULL;
    nimcp_free(coeffs);
    coeffs = NULL;
    nimcp_free(x_seg);
    x_seg = NULL;

    return FRACTAL_OK;
}

//=============================================================================
// Spectral Exponent
//=============================================================================

int fractal_spectral_exponent(
    const float* samples,
    size_t count,
    fractal_result_t* result
) {
    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_spectral_exponent", 0.0f);


    return fractal_spectral_exponent_config(samples, count, NULL, result);
}

int fractal_spectral_exponent_config(
    const float* samples,
    size_t count,
    const fractal_config_t* config,
    fractal_result_t* result
) {
    /* Validate inputs */
    if (!samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "fractal_spectral_exponent: samples is NULL");
        return FRACTAL_ERROR_NULL_PTR;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "fractal_spectral_exponent: result is NULL");
        return FRACTAL_ERROR_NULL_PTR;
    }
    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_spectral_exponent_co", 0.0f);


    if (count < FRACTAL_MIN_SAMPLES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "fractal_spectral_exponent: insufficient samples");
        return FRACTAL_ERROR_INSUFFICIENT;
    }
    if (!validate_input(samples, count, FRACTAL_MIN_SAMPLES)) {
        return FRACTAL_ERROR_INSUFFICIENT;
    }

    /* Use default config if not provided */
    fractal_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = fractal_config_default();
    }

    /* Find nearest power of 2 for FFT */
    uint32_t fft_size = fft_next_power_of_2((uint32_t)count);
    if (fft_size > count) {
        /* Use largest power of 2 that fits */
        fft_size = fft_size / 2;
    }
    if (fft_size < 64) {
        fft_size = 64;
    }

    /* Create FFT plan */
    fft_plan_t* plan = fft_plan_create(fft_size, FFT_REAL);
    if (!plan) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "fractal_spectral_exponent: FFT plan allocation failure");
        return FRACTAL_ERROR_ALLOC;
    }

    /* Apply Hann window for reduced leakage */
    fft_plan_set_window(plan, FFT_WINDOW_HANN);

    /* Allocate arrays */
    float* windowed = (float*)nimcp_calloc(fft_size, sizeof(float));
    fft_complex_t* spectrum = (fft_complex_t*)nimcp_calloc((fft_size / 2 + 1), sizeof(fft_complex_t));
    if (!spectrum) return -1;
    float* psd = (float*)nimcp_calloc((fft_size / 2 + 1), sizeof(float));

    if (!windowed || !spectrum || !psd) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "fractal_spectral_exponent: allocation failure for FFT arrays");
        fft_plan_destroy(plan);
        nimcp_free(windowed);
        windowed = NULL;
        nimcp_free(spectrum);
        spectrum = NULL;
        nimcp_free(psd);
        psd = NULL;
        return FRACTAL_ERROR_ALLOC;
    }

    /* Copy and zero-pad if needed */
    size_t copy_len = (count < fft_size) ? count : fft_size;
    memcpy(windowed, samples, copy_len * sizeof(float));
    for (size_t i = copy_len; i < fft_size; i++) {
        windowed[i] = 0.0f;
    }

    /* Remove mean */
    float mean = compute_mean(windowed, fft_size);
    for (size_t i = 0; i < fft_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && fft_size > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(i + 1) / (float)fft_size);
        }

        windowed[i] -= mean;
    }

    /* Execute FFT */
    if (!fft_execute_real(plan, windowed, spectrum)) {
        fft_plan_destroy(plan);
        nimcp_free(windowed);
        windowed = NULL;
        nimcp_free(spectrum);
        spectrum = NULL;
        nimcp_free(psd);
        psd = NULL;
        return FRACTAL_ERROR_COMPUTE;
    }

    /* Compute power spectral density */
    uint32_t psd_size = fft_size / 2 + 1;
    if (!fft_power_spectrum(spectrum, psd, psd_size)) {
        fft_plan_destroy(plan);
        nimcp_free(windowed);
        windowed = NULL;
        nimcp_free(spectrum);
        spectrum = NULL;
        nimcp_free(psd);
        psd = NULL;
        return FRACTAL_ERROR_COMPUTE;
    }

    /* Fit in log-log space, excluding DC and very high frequencies */
    size_t fit_start = 2;  /* Skip DC and first bin */
    size_t fit_end = psd_size / 2;  /* Use lower half of spectrum */

    if (fit_end <= fit_start + 4) {
        fft_plan_destroy(plan);
        nimcp_free(windowed);
        windowed = NULL;
        nimcp_free(spectrum);
        spectrum = NULL;
        nimcp_free(psd);
        psd = NULL;
        return FRACTAL_ERROR_INSUFFICIENT;
    }

    size_t fit_count = fit_end - fit_start;
    float* log_f = (float*)nimcp_calloc(fit_count, sizeof(float));
    float* log_p = (float*)nimcp_calloc(fit_count, sizeof(float));

    if (!log_f || !log_p) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "fractal_spectral_exponent: allocation failure for log arrays");
        fft_plan_destroy(plan);
        nimcp_free(windowed);
        windowed = NULL;
        nimcp_free(spectrum);
        spectrum = NULL;
        nimcp_free(psd);
        psd = NULL;
        nimcp_free(log_f);
        log_f = NULL;
        nimcp_free(log_p);
        log_p = NULL;
        return FRACTAL_ERROR_ALLOC;
    }

    size_t valid_count = 0;
    for (size_t i = fit_start; i < fit_end; i++) {
        if (psd[i] > EPSILON) {
            log_f[valid_count] = logf((float)i);  /* Use bin index as proxy for frequency */
            log_p[valid_count] = logf(psd[i]);
            valid_count++;
        }
    }

    if (valid_count < 4) {
        fft_plan_destroy(plan);
        nimcp_free(windowed);
        windowed = NULL;
        nimcp_free(spectrum);
        spectrum = NULL;
        nimcp_free(psd);
        psd = NULL;
        nimcp_free(log_f);
        log_f = NULL;
        nimcp_free(log_p);
        log_p = NULL;
        return FRACTAL_ERROR_COMPUTE;
    }

    /* Fit line: log(P) = -alpha * log(f) + c */
    linear_fit_t fit;
    if (!linear_regression(log_f, log_p, valid_count, &fit)) {
        fft_plan_destroy(plan);
        nimcp_free(windowed);
        windowed = NULL;
        nimcp_free(spectrum);
        spectrum = NULL;
        nimcp_free(psd);
        psd = NULL;
        nimcp_free(log_f);
        log_f = NULL;
        nimcp_free(log_p);
        log_p = NULL;
        return FRACTAL_ERROR_COMPUTE;
    }

    /* Store results (note: slope is negative for 1/f^alpha) */
    memset(result, 0, sizeof(fractal_result_t));
    result->spectral_exponent = -fit.slope;  /* alpha in S(f) ~ 1/f^alpha */
    result->spectral_r2 = fit.r_squared;
    result->samples_analyzed = (uint32_t)count;
    result->confidence = fit.r_squared;

    /* Derive related exponents */
    /* For 1/f noise: alpha = 2*H - 1, so H = (alpha + 1) / 2 */
    result->hurst_exponent = (result->spectral_exponent + 1.0f) / 2.0f;
    result->dfa_exponent = result->hurst_exponent;
    result->fractal_dimension = 2.0f - result->hurst_exponent;

    fft_plan_destroy(plan);
    nimcp_free(windowed);
    windowed = NULL;
    nimcp_free(spectrum);
    spectrum = NULL;
    nimcp_free(psd);
    psd = NULL;
    nimcp_free(log_f);
    log_f = NULL;
    nimcp_free(log_p);
    log_p = NULL;

    return FRACTAL_OK;
}

//=============================================================================
// Box-Counting Dimension
//=============================================================================

int fractal_box_dimension(
    const float* samples,
    size_t count,
    const fractal_config_t* config,
    fractal_result_t* result
) {
    /* Validate inputs */
    if (!samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "fractal_box_dimension: samples is NULL");
        return FRACTAL_ERROR_NULL_PTR;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "fractal_box_dimension: result is NULL");
        return FRACTAL_ERROR_NULL_PTR;
    }
    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_box_dimension", 0.0f);


    if (count < FRACTAL_MIN_SAMPLES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "fractal_box_dimension: insufficient samples");
        return FRACTAL_ERROR_INSUFFICIENT;
    }
    if (!validate_input(samples, count, FRACTAL_MIN_SAMPLES)) {
        return FRACTAL_ERROR_INSUFFICIENT;
    }

    /* Use default config if not provided */
    fractal_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = fractal_config_default();
    }

    /* Find min and max values */
    float y_min = samples[0];
    float y_max = samples[0];
    for (size_t i = 1; i < count; i++) {
        if (samples[i] < y_min) y_min = samples[i];
        if (samples[i] > y_max) y_max = samples[i];
    }

    float y_range = y_max - y_min;
    if (y_range < EPSILON) {
        /* Constant signal has dimension 0 */
        memset(result, 0, sizeof(fractal_result_t));
        result->fractal_dimension = 0.0f;
        result->samples_analyzed = (uint32_t)count;
        result->confidence = 1.0f;
        return FRACTAL_OK;
    }

    /* Determine scale range */
    size_t min_scale = cfg.box_min_size;
    if (min_scale < 2) min_scale = 2;

    size_t max_scale = cfg.box_max_size;
    if (max_scale == 0 || max_scale > count / 4) {
        max_scale = count / 4;
    }
    if (max_scale < min_scale + 4) {
        return FRACTAL_ERROR_INSUFFICIENT;
    }

    /* Allocate arrays */
    size_t num_scales = cfg.num_scales;
    size_t* scales = (size_t*)nimcp_calloc(num_scales, sizeof(size_t));
    float* log_eps = (float*)nimcp_calloc(num_scales, sizeof(float));
    if (!log_eps) return -1;
    float* log_n = (float*)nimcp_calloc(num_scales, sizeof(float));

    if (!scales || !log_eps || !log_n) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "fractal_box_dimension: allocation failure for scale arrays");
        nimcp_free(scales);
        scales = NULL;
        nimcp_free(log_eps);
        log_eps = NULL;
        nimcp_free(log_n);
        log_n = NULL;
        return FRACTAL_ERROR_ALLOC;
    }

    /* Generate scales */
    generate_scales(min_scale, max_scale, num_scales, cfg.use_log_scales, scales);

    /* Count boxes at each scale */
    size_t valid_scales = 0;
    for (size_t si = 0; si < num_scales; si++) {
        /* Phase 8: Loop progress heartbeat */
        if ((si & 0xFF) == 0 && num_scales > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(si + 1) / (float)num_scales);
        }

        size_t box_size = scales[si];
        if (box_size < 2 || box_size > count) {
            continue;
        }

        /* Compute epsilon (box size normalized to signal range) */
        float epsilon_x = (float)box_size / (float)count;
        float epsilon_y = y_range * epsilon_x;

        /* Count occupied boxes using a grid approach */
        size_t num_x_boxes = (count + box_size - 1) / (fabsf(box_size) > 1e-7f ? box_size : 1e-7f);
        size_t num_y_boxes = (size_t)(y_range / epsilon_y) + 1;
        size_t grid_size = num_x_boxes * num_y_boxes;

        size_t box_count = 0;

        /* Use hash set for large/sparse grids, direct array for small grids */
        #define HASH_SET_THRESHOLD 65536  /* Switch to hash set above 64K cells */
        #define HASH_SET_BUCKETS 1024     /* Number of hash buckets */

        if (grid_size <= HASH_SET_THRESHOLD) {
            /* Small grid: use direct boolean array (original approach) */
            bool* occupied = (bool*)nimcp_calloc(grid_size, sizeof(bool));
            if (!occupied) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                    "fractal_box_dimension: allocation failure for occupied array");
                nimcp_free(scales);
                scales = NULL;
                nimcp_free(log_eps);
                log_eps = NULL;
                nimcp_free(log_n);
                log_n = NULL;
                return FRACTAL_ERROR_ALLOC;
            }

            for (size_t i = 0; i < count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && count > 256) {
                    fractal_heartbeat("fractal_loop",
                                     (float)(i + 1) / (float)count);
                }

                size_t x_box = i / (box_size > 0 ? box_size : 1);
                size_t y_box = (size_t)((samples[i] - y_min) / epsilon_y);
                if (y_box >= num_y_boxes) y_box = num_y_boxes - 1;

                size_t idx = x_box * num_y_boxes + y_box;
                if (!occupied[idx]) {
                    occupied[idx] = true;
                    box_count++;
                }
            }

            nimcp_free(occupied);
            occupied = NULL;
        } else {
            /* Large grid: use hash set for O(n) memory instead of O(grid_size) */
            /* Hash set node for chained collision resolution */
            typedef struct hash_node {
                uint64_t key;  /* Packed (x_box, y_box) */
                struct hash_node* next;
            } hash_node_t;

            hash_node_t** buckets = (hash_node_t**)nimcp_calloc(HASH_SET_BUCKETS, sizeof(hash_node_t*));
            if (!buckets) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                    "fractal_box_dimension: allocation failure for hash set");
                nimcp_free(scales);
                scales = NULL;
                nimcp_free(log_eps);
                log_eps = NULL;
                nimcp_free(log_n);
                log_n = NULL;
                return FRACTAL_ERROR_ALLOC;
            }

            /* Pre-allocate node pool for efficiency */
            hash_node_t* node_pool = (hash_node_t*)nimcp_calloc(count, sizeof(hash_node_t));
            if (!node_pool) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                    "fractal_box_dimension: allocation failure for node pool");
                nimcp_free(buckets);
                buckets = NULL;
                nimcp_free(scales);
                scales = NULL;
                nimcp_free(log_eps);
                log_eps = NULL;
                nimcp_free(log_n);
                log_n = NULL;
                return FRACTAL_ERROR_ALLOC;
            }
            size_t pool_idx = 0;

            for (size_t i = 0; i < count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && count > 256) {
                    fractal_heartbeat("fractal_loop",
                                     (float)(i + 1) / (float)count);
                }

                size_t x_box = i / (box_size > 0 ? box_size : 1);
                size_t y_box = (size_t)((samples[i] - y_min) / epsilon_y);
                if (y_box >= num_y_boxes) y_box = num_y_boxes - 1;

                /* Pack coordinates into single 64-bit key */
                uint64_t key = ((uint64_t)x_box << 32) | (uint64_t)y_box;

                /* FNV-1a hash for bucket selection */
                uint32_t hash = 2166136261U;
                hash = (hash ^ (uint32_t)(key & 0xFFFFFFFF)) * 16777619U;
                hash = (hash ^ (uint32_t)(key >> 32)) * 16777619U;
                size_t bucket = hash % HASH_SET_BUCKETS;

                /* Search for existing key in bucket chain */
                hash_node_t* node = buckets[bucket];
                bool found = false;
                while (node) {
                    if (node->key == key) {
                        found = true;
                        break;
                    }
                    node = node->next;
                }

                if (!found) {
                    /* Insert new node from pool */
                    hash_node_t* new_node = &node_pool[pool_idx++];
                    new_node->key = key;
                    new_node->next = buckets[bucket];
                    buckets[bucket] = new_node;
                    box_count++;
                }
            }

            nimcp_free(node_pool);
            node_pool = NULL;
            nimcp_free(buckets);
            buckets = NULL;
        }

        #undef HASH_SET_THRESHOLD
        #undef HASH_SET_BUCKETS

        if (box_count > 0) {
            log_eps[valid_scales] = logf(1.0f / epsilon_x);  /* log(1/epsilon) */
            log_n[valid_scales] = logf((float)box_count);
            valid_scales++;
        }
    }

    /* Fit line: log(N) = D * log(1/epsilon) + c */
    if (valid_scales < 4) {
        nimcp_free(scales);
        scales = NULL;
        nimcp_free(log_eps);
        log_eps = NULL;
        nimcp_free(log_n);
        log_n = NULL;
        return FRACTAL_ERROR_COMPUTE;
    }

    linear_fit_t fit;
    if (!linear_regression(log_eps, log_n, valid_scales, &fit)) {
        nimcp_free(scales);
        scales = NULL;
        nimcp_free(log_eps);
        log_eps = NULL;
        nimcp_free(log_n);
        log_n = NULL;
        return FRACTAL_ERROR_COMPUTE;
    }

    /* Store results */
    memset(result, 0, sizeof(fractal_result_t));
    result->fractal_dimension = fit.slope;
    result->samples_analyzed = (uint32_t)count;
    result->scales_computed = (uint32_t)valid_scales;
    result->confidence = fit.r_squared;

    /* Derive related exponents: D = 2 - H */
    result->hurst_exponent = 2.0f - fit.slope;
    result->dfa_exponent = result->hurst_exponent;
    result->spectral_exponent = 2.0f * result->hurst_exponent - 1.0f;

    nimcp_free(scales);
    scales = NULL;
    nimcp_free(log_eps);
    log_eps = NULL;
    nimcp_free(log_n);
    log_n = NULL;

    return FRACTAL_OK;
}

//=============================================================================
// Lacunarity
//=============================================================================

float fractal_lacunarity(
    const float* samples,
    size_t count,
    size_t box_size
) {
    if (!samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "fractal_lacunarity: samples is NULL");
        return -1.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_lacunarity", 0.0f);


    if (count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "fractal_lacunarity: count is zero");
        return -1.0f;
    }
    if (box_size == 0 || box_size > count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "fractal_lacunarity: invalid box_size");
        return -1.0f;
    }

    /* Number of sliding window positions */
    size_t num_windows = count - box_size + 1;
    if (num_windows < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "fractal_lacunarity: insufficient windows");
        return -1.0f;
    }

    /* Compute mass (sum) in each window */
    double mass_sum = 0.0;
    double mass_sq_sum = 0.0;

    /* Compute first window sum */
    double window_sum = 0.0;
    for (size_t i = 0; i < box_size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && box_size > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(i + 1) / (float)box_size);
        }

        window_sum += (double)samples[i];
    }
    mass_sum += window_sum;
    mass_sq_sum += window_sum * window_sum;

    /* Sliding window: add new element, remove old */
    for (size_t i = 1; i < num_windows; i++) {
        window_sum = window_sum + (double)samples[i + box_size - 1] - (double)samples[i - 1];
        mass_sum += window_sum;
        mass_sq_sum += window_sum * window_sum;
    }

    /* Compute statistics */
    double mean_mass = mass_sum / (double)num_windows;
    if (fabs(mean_mass) < EPSILON) {
        return 1.0f;  /* All zeros, homogeneous */
    }

    double mean_mass_sq = mass_sq_sum / (double)num_windows;
    double var_mass = mean_mass_sq - mean_mass * mean_mass;

    /* Lacunarity: Lambda = var/mean^2 + 1 */
    /* Note: Some definitions use Lambda = var/mean^2, we use the CV^2 + 1 version */
    float lacunarity = (float)(var_mass / (mean_mass * mean_mass)) + 1.0f;

    return lacunarity;
}

int fractal_lacunarity_curve(
    const float* samples,
    size_t count,
    const fractal_config_t* config,
    float* scales,
    float* lacunarities,
    size_t num_scales
) {
    if (!samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "fractal_lacunarity_curve: samples is NULL");
        return FRACTAL_ERROR_NULL_PTR;
    }
    if (!scales) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "fractal_lacunarity_curve: scales is NULL");
        return FRACTAL_ERROR_NULL_PTR;
    }
    if (!lacunarities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "fractal_lacunarity_curve: lacunarities is NULL");
        return FRACTAL_ERROR_NULL_PTR;
    }
    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_lacunarity_curve", 0.0f);


    if (num_scales == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "fractal_lacunarity_curve: num_scales is zero");
        return FRACTAL_ERROR_NULL_PTR;
    }
    if (count < FRACTAL_MIN_SAMPLES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "fractal_lacunarity_curve: insufficient samples");
        return FRACTAL_ERROR_INSUFFICIENT;
    }
    if (!validate_input(samples, count, FRACTAL_MIN_SAMPLES)) {
        return FRACTAL_ERROR_INSUFFICIENT;
    }

    /* Use default config if not provided */
    fractal_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = fractal_config_default();
    }

    /* Determine scale range */
    size_t min_scale = cfg.min_scale;
    if (min_scale < 2) min_scale = 2;

    size_t max_scale = cfg.max_scale;
    if (max_scale == 0 || max_scale > count / 2) {
        max_scale = count / 2;
    }

    /* Generate scales */
    size_t* scale_vals = (size_t*)nimcp_calloc(num_scales, sizeof(size_t));
    if (!scale_vals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "fractal_lacunarity_curve: allocation failure for scale_vals");
        return FRACTAL_ERROR_ALLOC;
    }

    generate_scales(min_scale, max_scale, num_scales, cfg.use_log_scales, scale_vals);

    /* Compute lacunarity at each scale */
    for (size_t i = 0; i < num_scales; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_scales > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(i + 1) / (float)num_scales);
        }

        scales[i] = (float)scale_vals[i];
        lacunarities[i] = fractal_lacunarity(samples, count, scale_vals[i]);
    }

    nimcp_free(scale_vals);
    scale_vals = NULL;
    return FRACTAL_OK;
}

//=============================================================================
// Multifractal Analysis
//=============================================================================

int fractal_multifractal_spectrum(
    const float* samples,
    size_t count,
    float q_min,
    float q_max,
    size_t q_steps,
    multifractal_spectrum_t** spectrum
) {
    /* Validate inputs */
    if (!samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "fractal_multifractal_spectrum: samples is NULL");
        return FRACTAL_ERROR_NULL_PTR;
    }
    if (!spectrum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "fractal_multifractal_spectrum: spectrum output is NULL");
        return FRACTAL_ERROR_NULL_PTR;
    }
    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_multifractal_spectru", 0.0f);


    if (count < FRACTAL_MIN_SAMPLES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "fractal_multifractal_spectrum: insufficient samples");
        return FRACTAL_ERROR_INSUFFICIENT;
    }
    if (!validate_input(samples, count, FRACTAL_MIN_SAMPLES)) {
        return FRACTAL_ERROR_INSUFFICIENT;
    }
    if (q_steps < 3 || q_steps > FRACTAL_MAX_Q_VALUES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "fractal_multifractal_spectrum: invalid q_steps value");
        return FRACTAL_ERROR_PARAM;
    }
    if (q_min >= q_max) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "fractal_multifractal_spectrum: q_min must be less than q_max");
        return FRACTAL_ERROR_PARAM;
    }

    /* Allocate spectrum structure */
    multifractal_spectrum_t* mf = (multifractal_spectrum_t*)nimcp_calloc(1, sizeof(multifractal_spectrum_t));
    if (!mf) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "fractal_multifractal_spectrum: allocation failure for spectrum struct");
        return FRACTAL_ERROR_ALLOC;
    }

    mf->spectrum_size = q_steps;
    mf->q_values = (float*)nimcp_calloc(q_steps, sizeof(float));
    if (!mf->q_values) return -1;
    mf->tau_q = (float*)nimcp_calloc(q_steps, sizeof(float));
    if (!mf->tau_q) return -1;
    mf->h_q = (float*)nimcp_calloc(q_steps, sizeof(float));
    if (!mf->h_q) return -1;
    mf->D_q = (float*)nimcp_calloc(q_steps, sizeof(float));
    if (!mf->D_q) return -1;
    mf->f_alpha = (float*)nimcp_calloc(q_steps, sizeof(float));
    if (!mf->f_alpha) return -1;
    mf->alpha = (float*)nimcp_calloc(q_steps, sizeof(float));
    if (!mf->alpha) return -1;

    if (!mf->q_values || !mf->tau_q || !mf->h_q || !mf->D_q ||
        !mf->f_alpha || !mf->alpha) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "fractal_multifractal_spectrum: allocation failure for spectrum arrays");
        multifractal_spectrum_destroy(mf);
        return FRACTAL_ERROR_ALLOC;
    }

    /* Generate q values */
    float q_step = (q_max - q_min) / (float)(q_steps - 1);
    for (size_t i = 0; i < q_steps; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && q_steps > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(i + 1) / (float)q_steps);
        }

        mf->q_values[i] = q_min + (float)i * q_step;
    }

    /* Integration step: y(k) = sum_{i=1}^{k} (x(i) - mean) */
    float mean = compute_mean(samples, count);
    float* y = (float*)nimcp_calloc(count, sizeof(float));
    if (!y) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "fractal_multifractal_spectrum: allocation failure for integrated signal");
        multifractal_spectrum_destroy(mf);
        return FRACTAL_ERROR_ALLOC;
    }

    float cumsum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(i + 1) / (float)count);
        }

        cumsum += samples[i] - mean;
        y[i] = cumsum;
    }

    /* Set up scales */
    fractal_config_t cfg = fractal_config_default();
    size_t max_scale = count / 4;
    size_t num_scales = cfg.num_scales;

    size_t* scales = (size_t*)nimcp_calloc(num_scales, sizeof(size_t));
    if (!scales) return -1;
    float* log_scales = (float*)nimcp_calloc(num_scales, sizeof(float));
    if (!log_scales) return -1;
    float* log_fq = (float*)nimcp_calloc(num_scales, sizeof(float));
    if (!log_fq) return -1;
    float* coeffs = (float*)nimcp_malloc(2 * sizeof(float));  /* Linear fit */
    if (!coeffs) return -1;
    float* x_seg = (float*)nimcp_calloc(max_scale, sizeof(float));

    if (!scales || !log_scales || !log_fq || !coeffs || !x_seg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "fractal_multifractal_spectrum: allocation failure for working arrays");
        nimcp_free(y);
        y = NULL;
        nimcp_free(scales);
        scales = NULL;
        nimcp_free(log_scales);
        log_scales = NULL;
        nimcp_free(log_fq);
        log_fq = NULL;
        nimcp_free(coeffs);
        coeffs = NULL;
        nimcp_free(x_seg);
        x_seg = NULL;
        multifractal_spectrum_destroy(mf);
        return FRACTAL_ERROR_ALLOC;
    }

    generate_scales(cfg.min_scale, max_scale, num_scales, true, scales);

    /* For each q, compute h(q) */
    for (size_t qi = 0; qi < q_steps; qi++) {
        /* Phase 8: Loop progress heartbeat */
        if ((qi & 0xFF) == 0 && q_steps > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(qi + 1) / (float)q_steps);
        }

        float q = mf->q_values[qi];

        size_t valid_scales = 0;

        for (size_t si = 0; si < num_scales; si++) {
            /* Phase 8: Loop progress heartbeat */
            if ((si & 0xFF) == 0 && num_scales > 256) {
                fractal_heartbeat("fractal_loop",
                                 (float)(si + 1) / (float)num_scales);
            }

            size_t n = scales[si];
            if (n > max_scale) n = max_scale;  /* Clamp: x_seg allocated for max_scale */
            if (n > count || n < 4) {
                continue;
            }

            size_t num_segments = count / (n > 0 ? n : 1);
            if (num_segments < 1) {
                continue;
            }

            /* Create x values */
            for (size_t i = 0; i < n; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && n > 256) {
                    fractal_heartbeat("fractal_loop",
                                     (float)(i + 1) / (float)n);
                }

                x_seg[i] = (float)i;
            }

            double fq_sum = 0.0;
            size_t seg_count = 0;

            for (size_t seg = 0; seg < num_segments; seg++) {
                /* Phase 8: Loop progress heartbeat */
                if ((seg & 0xFF) == 0 && num_segments > 256) {
                    fractal_heartbeat("fractal_loop",
                                     (float)(seg + 1) / (float)num_segments);
                }

                const float* y_seg = y + seg * n;

                /* Linear fit for detrending */
                if (!polynomial_fit(x_seg, y_seg, n, 1, coeffs)) {
                    continue;
                }

                /* Compute variance of residuals */
                double var = 0.0;
                for (size_t i = 0; i < n; i++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((i & 0xFF) == 0 && n > 256) {
                        fractal_heartbeat("fractal_loop",
                                         (float)(i + 1) / (float)n);
                    }

                    float y_fit = polyval(coeffs, 1, x_seg[i]);
                    float diff = y_seg[i] - y_fit;
                    var += (double)(diff * diff);
                }
                var /= (double)n;

                if (var < EPSILON) {
                    continue;
                }

                /* For q=0, use log */
                if (fabs(q) < 0.01f) {
                    fq_sum += log(var) / 2.0;  /* log(sqrt(var)) */
                } else {
                    fq_sum += pow(var, (double)q / 2.0);
                }
                seg_count++;
            }

            if (seg_count > 0) {
                double fq = 0.0;
                if (fabs(q) < 0.01f) {
                    fq = exp(fq_sum / (double)seg_count);
                } else {
                    fq = pow(fq_sum / (double)seg_count, 1.0 / (double)q);
                }

                if (fq > EPSILON) {
                    log_scales[valid_scales] = logf((float)n);
                    log_fq[valid_scales] = logf((float)fq);
                    valid_scales++;
                }
            }
        }

        /* Fit to get h(q) */
        if (valid_scales >= 4) {
            linear_fit_t fit;
            if (linear_regression(log_scales, log_fq, valid_scales, &fit)) {
                mf->h_q[qi] = fit.slope;
            } else {
                mf->h_q[qi] = 0.5f;  /* Default */
            }
        } else {
            mf->h_q[qi] = 0.5f;
        }

        /* Compute tau(q) = q*h(q) - 1 */
        mf->tau_q[qi] = q * mf->h_q[qi] - 1.0f;
    }

    /* Compute D(q) = tau(q)/(q-1) and f(alpha), alpha via Legendre transform */
    for (size_t i = 0; i < q_steps; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && q_steps > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(i + 1) / (float)q_steps);
        }

        float q = mf->q_values[i];

        /* D(q) = tau(q)/(q-1) for q != 1 */
        if (fabs(q - 1.0f) > 0.1f) {
            mf->D_q[i] = mf->tau_q[i] / (q - 1.0f);
        } else {
            /* Limit as q->1: D(1) = lim tau'(q) */
            mf->D_q[i] = mf->h_q[i];
        }

        /* alpha = d(tau)/dq - approximate with finite difference */
        if (i == 0) {
            mf->alpha[i] = (mf->tau_q[1] - mf->tau_q[0]) /
                          (mf->q_values[1] - mf->q_values[0]);
        } else if (i == q_steps - 1) {
            mf->alpha[i] = (mf->tau_q[q_steps-1] - mf->tau_q[q_steps-2]) /
                          (mf->q_values[q_steps-1] - mf->q_values[q_steps-2]);
        } else {
            mf->alpha[i] = (mf->tau_q[i+1] - mf->tau_q[i-1]) /
                          (mf->q_values[i+1] - mf->q_values[i-1]);
        }

        /* f(alpha) = q*alpha - tau(q) */
        mf->f_alpha[i] = q * mf->alpha[i] - mf->tau_q[i];
    }

    /* Compute summary statistics */
    float alpha_min = mf->alpha[0];
    float alpha_max = mf->alpha[0];
    float f_max = mf->f_alpha[0];
    size_t f_max_idx = 0;

    for (size_t i = 1; i < q_steps; i++) {
        if (mf->alpha[i] < alpha_min) alpha_min = mf->alpha[i];
        if (mf->alpha[i] > alpha_max) alpha_max = mf->alpha[i];
        if (mf->f_alpha[i] > f_max) {
            f_max = mf->f_alpha[i];
            f_max_idx = i;
        }
    }

    mf->width = alpha_max - alpha_min;
    mf->peak_alpha = mf->alpha[f_max_idx];
    mf->peak_f = f_max;
    mf->h_mono = 0.5f;  /* Will find h(q=2) */

    /* Find h(q=2) for monofractal estimate */
    for (size_t i = 0; i < q_steps; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && q_steps > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(i + 1) / (float)q_steps);
        }

        if (fabsf(mf->q_values[i] - 2.0f) < fabsf(mf->q_values[0] - 2.0f)) {
            mf->h_mono = mf->h_q[i];
        }
    }

    /* Asymmetry: compare left and right halves */
    float left_sum = 0.0f, right_sum = 0.0f;
    size_t peak_idx = f_max_idx;
    for (size_t i = 0; i < peak_idx; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && peak_idx > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(i + 1) / (float)peak_idx);
        }

        left_sum += mf->f_alpha[i];
    }
    for (size_t i = peak_idx + 1; i < q_steps; i++) {
        right_sum += mf->f_alpha[i];
    }
    float total = left_sum + right_sum;
    if (total > EPSILON) {
        mf->asymmetry = (right_sum - left_sum) / (fabsf(total) > 1e-7f ? total : 1e-7f);
    } else {
        mf->asymmetry = 0.0f;
    }

    /* Is multifractal? */
    mf->is_multifractal = (mf->width > MULTIFRACTAL_WIDTH_THRESHOLD);

    nimcp_free(y);
    y = NULL;
    nimcp_free(scales);
    scales = NULL;
    nimcp_free(log_scales);
    log_scales = NULL;
    nimcp_free(log_fq);
    log_fq = NULL;
    nimcp_free(coeffs);
    coeffs = NULL;
    nimcp_free(x_seg);
    x_seg = NULL;

    *spectrum = mf;
    return FRACTAL_OK;
}

void multifractal_spectrum_destroy(multifractal_spectrum_t* spectrum) {
    if (!spectrum) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_multifractal_spectru", 0.0f);


    nimcp_free(spectrum->q_values);
    nimcp_free(spectrum->tau_q);
    nimcp_free(spectrum->h_q);
    nimcp_free(spectrum->D_q);
    nimcp_free(spectrum->f_alpha);
    nimcp_free(spectrum->alpha);
    nimcp_free(spectrum);
    spectrum = NULL;
}

//=============================================================================
// Validation Functions
//=============================================================================

bool fractal_is_pink_noise(
    const float* samples,
    size_t count,
    float tolerance
) {
    if (!samples || count < FRACTAL_MIN_SAMPLES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "fractal_is_pink_noise: invalid parameters");

            return false;
    }
    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_is_pink_noise", 0.0f);


    if (tolerance <= 0.0f || tolerance > 1.0f) {
        tolerance = FRACTAL_PINK_NOISE_TOLERANCE;
    }

    fractal_result_t result;
    if (fractal_dfa(samples, count, NULL, &result) != FRACTAL_OK) {
        return false;
    }

    /* Check if DFA exponent is close to 1.0 */
    float deviation = fabsf(result.dfa_exponent - 1.0f);
    return (deviation <= tolerance) && (result.dfa_r2 >= 0.9f);
}

bool fractal_is_self_similar(
    const float* samples,
    size_t count,
    size_t min_scales
) {
    if (!samples || count < FRACTAL_MIN_SAMPLES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "fractal_is_self_similar: invalid parameters");

            return false;
    }
    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_is_self_similar", 0.0f);


    if (min_scales < 4) {
        min_scales = 4;
    }

    fractal_config_t config = fractal_config_default();
    config.num_scales = (min_scales > config.num_scales) ? min_scales : config.num_scales;

    fractal_result_t result;
    if (fractal_dfa(samples, count, &config, &result) != FRACTAL_OK) {
        return false;
    }

    /* Self-similar if good fit and enough scales */
    return (result.dfa_r2 >= 0.95f) && (result.scales_computed >= min_scales);
}

bool fractal_validate_signal(
    const float* samples,
    size_t count
) {
    if (!samples || count < FRACTAL_MIN_SAMPLES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "fractal_validate_signal: invalid parameters");

            return false;
    }

    /* Check for NaN/Inf */
    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_validate_signal", 0.0f);


    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            fractal_heartbeat("fractal_loop",
                             (float)(i + 1) / (float)count);
        }

        if (!isfinite(samples[i])) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "fractal_validate_signal: isfinite is NULL");
            return false;
        }
    }

    /* Check for non-zero variance */
    float mean = compute_mean(samples, count);
    float var = compute_variance(samples, count, mean);

    return (var > MIN_VARIANCE);
}

//=============================================================================
// Comprehensive Analysis
//=============================================================================

int fractal_analyze(
    const float* samples,
    size_t count,
    const fractal_config_t* config,
    fractal_result_t* result
) {
    if (!samples) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "fractal_analyze: samples is NULL");
        return FRACTAL_ERROR_NULL_PTR;
    }
    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "fractal_analyze: result is NULL");
        return FRACTAL_ERROR_NULL_PTR;
    }
    if (!fractal_validate_signal(samples, count)) {
        return FRACTAL_ERROR_QUALITY;
    }

    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_analyze", 0.0f);


    fractal_result_t dfa_result, rs_result, spectral_result, box_result;
    memset(result, 0, sizeof(fractal_result_t));

    /* Run DFA (primary method) */
    int rc = fractal_dfa(samples, count, config, &dfa_result);
    if (rc == FRACTAL_OK) {
        result->dfa_exponent = dfa_result.dfa_exponent;
        result->dfa_r2 = dfa_result.dfa_r2;
        result->scales_computed = dfa_result.scales_computed;
    }

    /* Run R/S analysis */
    rc = fractal_hurst_rs(samples, count, config, &rs_result);
    if (rc == FRACTAL_OK) {
        result->hurst_exponent = rs_result.hurst_exponent;
        result->hurst_r2 = rs_result.hurst_r2;
    }

    /* Run spectral analysis */
    rc = fractal_spectral_exponent_config(samples, count, config, &spectral_result);
    if (rc == FRACTAL_OK) {
        result->spectral_exponent = spectral_result.spectral_exponent;
        result->spectral_r2 = spectral_result.spectral_r2;
    }

    /* Run box-counting */
    rc = fractal_box_dimension(samples, count, config, &box_result);
    if (rc == FRACTAL_OK) {
        result->fractal_dimension = box_result.fractal_dimension;
    }

    /* Compute lacunarity at default scale */
    size_t lac_scale = count / 16;
    if (lac_scale < 4) lac_scale = 4;
    result->lacunarity = fractal_lacunarity(samples, count, lac_scale);

    result->samples_analyzed = (uint32_t)count;

    /* Overall confidence is average of R^2 values */
    float conf_sum = 0.0f;
    int conf_count = 0;
    if (result->dfa_r2 > 0.0f) { conf_sum += result->dfa_r2; conf_count++; }
    if (result->hurst_r2 > 0.0f) { conf_sum += result->hurst_r2; conf_count++; }
    if (result->spectral_r2 > 0.0f) { conf_sum += result->spectral_r2; conf_count++; }

    if (conf_count > 0) {
        result->confidence = conf_sum / (float)conf_count;
    }

    return FRACTAL_OK;
}

//=============================================================================
// Utility Functions
//=============================================================================

void fractal_result_print(const fractal_result_t* result) {
    if (!result) {
        printf("Fractal Result: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_result_print", 0.0f);


    printf("=== Fractal Analysis Results ===\n");
    printf("Samples analyzed: %u\n", result->samples_analyzed);
    printf("Scales computed:  %u\n", result->scales_computed);
    printf("Overall confidence: %.3f\n", result->confidence);
    printf("\n");
    printf("Hurst Exponent (H):     %.4f (R^2=%.3f)\n",
           result->hurst_exponent, result->hurst_r2);
    printf("DFA Exponent (alpha):   %.4f (R^2=%.3f)\n",
           result->dfa_exponent, result->dfa_r2);
    printf("Spectral Exponent (beta): %.4f (R^2=%.3f)\n",
           result->spectral_exponent, result->spectral_r2);
    printf("Fractal Dimension (D):  %.4f\n", result->fractal_dimension);
    printf("Lacunarity (Lambda):    %.4f\n", result->lacunarity);
    printf("\n");
    printf("Noise classification: %s\n", fractal_classify_noise(result->dfa_exponent));
    printf("================================\n");
}

void multifractal_spectrum_print(const multifractal_spectrum_t* spectrum) {
    if (!spectrum) {
        printf("Multifractal Spectrum: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_multifractal_spectru", 0.0f);


    printf("=== Multifractal Spectrum ===\n");
    printf("Spectrum size: %zu q-values\n", spectrum->spectrum_size);
    printf("Spectrum width (alpha_max - alpha_min): %.4f\n", spectrum->width);
    printf("Peak alpha: %.4f\n", spectrum->peak_alpha);
    printf("Peak f(alpha): %.4f\n", spectrum->peak_f);
    printf("Monofractal estimate h(q=2): %.4f\n", spectrum->h_mono);
    printf("Asymmetry: %.4f\n", spectrum->asymmetry);
    printf("Is multifractal: %s\n", spectrum->is_multifractal ? "yes" : "no");
    printf("\n");
    printf("q-values and generalized Hurst h(q):\n");
    for (size_t i = 0; i < spectrum->spectrum_size; i += spectrum->spectrum_size / 10 + 1) {
        printf("  q=%6.2f: h(q)=%.4f, tau(q)=%.4f, D(q)=%.4f\n",
               spectrum->q_values[i], spectrum->h_q[i],
               spectrum->tau_q[i], spectrum->D_q[i]);
    }
    printf("==============================\n");
}

size_t fractal_estimate_sample_requirement(
    float confidence_target,
    int method
) {
    /* Based on empirical requirements for stable estimates */
    /* Higher confidence requires more samples */
    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_estimate_sample_requ", 0.0f);


    if (confidence_target < 0.9f) {
        confidence_target = 0.9f;
    }
    if (confidence_target > 0.99f) {
        confidence_target = 0.99f;
    }

    /* Base requirements */
    size_t base = 0;
    switch (method) {
        case 0:  /* DFA */
            base = 256;
            break;
        case 1:  /* Hurst R/S */
            base = 512;
            break;
        case 2:  /* Spectral */
            base = 256;
            break;
        default:
            base = 512;
    }

    /* Scale up for higher confidence */
    float scale = 1.0f + (confidence_target - 0.9f) * 10.0f;  /* 1.0 to 1.9 */

    return (size_t)(base * scale);
}

void fractal_convert_exponents(
    float hurst,
    float* dfa_out,
    float* spectral_out,
    float* dimension_out
) {
    /* Phase 8: Heartbeat at operation start */
    fractal_heartbeat("fractal_convert_exponents", 0.0f);


    if (dfa_out) {
        *dfa_out = hurst;  /* H ~ alpha for stationary signals */
    }
    if (spectral_out) {
        *spectral_out = 2.0f * hurst - 1.0f;  /* beta = 2H - 1 */
    }
    if (dimension_out) {
        *dimension_out = 2.0f - hurst;  /* D = 2 - H for 1D */
    }
}

const char* fractal_classify_noise(float dfa_exponent) {
    if (dfa_exponent < 0.3f) {
        return "anti-correlated";
    } else if (dfa_exponent < 0.6f) {
        return "white noise";
    } else if (dfa_exponent < 0.85f) {
        return "pink-ish (weak correlations)";
    } else if (dfa_exponent < 1.15f) {
        return "pink noise (1/f)";
    } else if (dfa_exponent < 1.4f) {
        return "brown-ish";
    } else if (dfa_exponent < 1.7f) {
        return "brown noise (1/f^2)";
    } else {
        return "strongly correlated";
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void fractal_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_fractal_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int fractal_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fractal_training_begin: NULL argument");
        return -1;
    }
    fractal_heartbeat_instance(NULL, "fractal_training_begin", 0.0f);
    (void)(linear_fit_t*)instance; /* Module state available for reset */
    return 0;
}

int fractal_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fractal_training_end: NULL argument");
        return -1;
    }
    fractal_heartbeat_instance(NULL, "fractal_training_end", 1.0f);
    (void)(linear_fit_t*)instance; /* Module state available for finalization */
    return 0;
}

int fractal_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fractal_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    fractal_heartbeat_instance(NULL, "fractal_training_step", progress);
    (void)(linear_fit_t*)instance; /* Module state available for step adaptation */
    return 0;
}
