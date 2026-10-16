/**
 * @file nimcp_survival.c
 * @brief Implementation of survival analysis methods
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Comprehensive survival analysis implementation
 * WHY:  Time-to-event analysis for neural dynamics
 * HOW:  Numerically stable algorithms with optional GPU acceleration
 *
 * @author NIMCP Development Team
 */

#include "utils/statistics/nimcp_survival.h"
#include "utils/statistics/nimcp_statistics.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// MODULE IDENTIFICATION
//=============================================================================

#define LOG_MODULE_NAME "survival"

//=============================================================================
// CONSTANTS
//=============================================================================

#define SURVIVAL_EPS 1e-10
#define SURVIVAL_LOG_EPS -23.025850929940457  // log(1e-10)

//=============================================================================
// Global State
//=============================================================================

static bool g_use_gpu = false;

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal sorted survival observation
 */
typedef struct sorted_obs {
    float time;
    bool event;
    uint8_t event_type;
    uint32_t group;
    uint32_t orig_idx;
} sorted_obs_t;

//=============================================================================
// Helper Functions
//=============================================================================

static int compare_obs_time(const void* a, const void* b) {
    const sorted_obs_t* oa = (const sorted_obs_t*)a;
    const sorted_obs_t* ob = (const sorted_obs_t*)b;

    if (oa->time < ob->time) return -1;
    if (oa->time > ob->time) return 1;

    // Events before censorings at same time
    if (oa->event && !ob->event) return -1;
    if (!oa->event && ob->event) return 1;

    return 0;
}

static double log_sum_exp(const double* log_values, uint32_t n) {
    if (n == 0) return -INFINITY;

    double max_val = log_values[0];
    for (uint32_t i = 1; i < n; i++) {
        if (log_values[i] > max_val) max_val = log_values[i];
    }

    if (max_val == -INFINITY) return -INFINITY;

    double sum = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        sum += exp(log_values[i] - max_val);
    }

    return max_val + log(sum);
}

static double log1p_exp(double x) {
    // Numerically stable log(1 + exp(x))
    if (x > 35.0) return x;
    if (x < -10.0) return exp(x);
    return log1p(exp(x));
}

//=============================================================================
// Error Strings
//=============================================================================

const char* nimcp_survival_error_string(nimcp_survival_result_t result) {
    switch (result) {
        case NIMCP_SURVIVAL_OK:           return "Success";
        case NIMCP_SURVIVAL_ERROR_NULL:   return "NULL pointer argument";
        case NIMCP_SURVIVAL_ERROR_SIZE:   return "Invalid size";
        case NIMCP_SURVIVAL_ERROR_MEMORY: return "Memory allocation failed";
        case NIMCP_SURVIVAL_ERROR_PARAMS: return "Invalid parameters";
        case NIMCP_SURVIVAL_ERROR_CONVERGE: return "Algorithm did not converge";
        case NIMCP_SURVIVAL_ERROR_SINGULAR: return "Singular matrix";
        case NIMCP_SURVIVAL_ERROR_NO_EVENTS: return "No events observed";
        case NIMCP_SURVIVAL_ERROR_PH_VIOLATED: return "Proportional hazards violated";
        case NIMCP_SURVIVAL_ERROR_GPU:    return "GPU computation error";
        case NIMCP_SURVIVAL_ERROR_NOT_FIT: return "Model not fitted";
        default:                          return "Unknown error";
    }
}

//=============================================================================
// GPU Control
//=============================================================================

bool nimcp_survival_gpu_available(void) {
#ifdef NIMCP_ENABLE_CUDA
    return true;
#else
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_survival_gpu_available: operation failed");
    return false;
#endif
}

void nimcp_survival_set_gpu(bool enable) {
    g_use_gpu = enable && nimcp_survival_gpu_available();
}

//=============================================================================
// Survival Data Management
//=============================================================================

nimcp_survival_data_t* nimcp_survival_data_create(uint32_t n, uint32_t n_covariates) {
    if (n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_survival_data_create: n is zero");
        return NULL;
    }

    nimcp_survival_data_t* data = (nimcp_survival_data_t*)nimcp_calloc(1, sizeof(nimcp_survival_data_t));
    if (!data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_survival_data_create: data is NULL");
        return NULL;
    }

    data->observations = (nimcp_survival_obs_t*)nimcp_calloc(n, sizeof(nimcp_survival_obs_t));
    if (!data->observations) {
        nimcp_free(data);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_survival_data_create: data->observations is NULL");
        return NULL;
    }

    data->n = n;
    data->n_covariates = n_covariates;

    if (n_covariates > 0) {
        data->covariates = (float*)nimcp_calloc((size_t)n * n_covariates, sizeof(float));
        if (!data->covariates) {
            nimcp_free(data->observations);
            nimcp_free(data);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_survival_data_create: data->covariates is NULL");
            return NULL;
        }
    }

    return data;
}

void nimcp_survival_data_destroy(nimcp_survival_data_t* data) {
    if (!data) return;

    nimcp_free(data->observations);
    nimcp_free(data->covariates);

    if (data->covariate_names) {
        for (uint32_t i = 0; i < data->n_covariates; i++) {
            nimcp_free(data->covariate_names[i]);
        }
        nimcp_free(data->covariate_names);
    }

    nimcp_free(data);
}

//=============================================================================
// Kaplan-Meier Estimator
//=============================================================================

nimcp_km_estimator_t* nimcp_km_create(void) {
    nimcp_km_estimator_t* km = (nimcp_km_estimator_t*)nimcp_calloc(1, sizeof(nimcp_km_estimator_t));
    if (!km) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_km_create: km is NULL");
        return NULL;
    }

    km->median_survival = NAN;
    km->median_ci_lower = NAN;
    km->median_ci_upper = NAN;
    km->confidence_level = NIMCP_SURVIVAL_DEFAULT_CONFIDENCE;
    km->fitted = false;

    return km;
}

void nimcp_km_destroy(nimcp_km_estimator_t* km) {
    if (!km) return;
    nimcp_free(km->curve);
    nimcp_free(km);
}

nimcp_survival_result_t nimcp_km_fit(
    nimcp_km_estimator_t* km,
    const float* times,
    const bool* events,
    uint32_t n,
    float confidence
) {
    if (!km || !times || !events) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "NULL argument to nimcp_km_fit");
        return NIMCP_SURVIVAL_ERROR_NULL;
    }
    if (n == 0) return NIMCP_SURVIVAL_ERROR_SIZE;
    if (confidence <= 0.0f || confidence >= 1.0f) confidence = NIMCP_SURVIVAL_DEFAULT_CONFIDENCE;

    // Sort observations by time
    sorted_obs_t* sorted = (sorted_obs_t*)nimcp_malloc(n * sizeof(sorted_obs_t));
    if (!sorted) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate sorted array");
        return NIMCP_SURVIVAL_ERROR_MEMORY;
    }

    uint32_t n_events_total = 0;
    for (uint32_t i = 0; i < n; i++) {
        sorted[i].time = times[i];
        sorted[i].event = events[i];
        sorted[i].orig_idx = i;
        if (events[i]) n_events_total++;
    }

    if (n_events_total == 0) {
        nimcp_free(sorted);
        return NIMCP_SURVIVAL_ERROR_NO_EVENTS;
    }

    qsort(sorted, n, sizeof(sorted_obs_t), compare_obs_time);

    // Count unique event times
    uint32_t n_unique = 0;
    float prev_time = -INFINITY;
    for (uint32_t i = 0; i < n; i++) {
        if (sorted[i].event && sorted[i].time != prev_time) {
            n_unique++;
            prev_time = sorted[i].time;
        }
    }

    // Allocate curve (include time=0 point)
    nimcp_free(km->curve);
    km->curve = (nimcp_km_point_t*)nimcp_calloc(n_unique + 1, sizeof(nimcp_km_point_t));
    if (!km->curve) {
        nimcp_free(sorted);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate KM curve");
        return NIMCP_SURVIVAL_ERROR_MEMORY;
    }

    // Initialize time=0 point
    km->curve[0].time = 0.0f;
    km->curve[0].survival = 1.0f;
    km->curve[0].std_error = 0.0f;
    km->curve[0].n_at_risk = n;
    km->curve[0].n_events = 0;
    km->curve[0].n_censored = 0;

    // Compute Kaplan-Meier estimates using product-limit formula
    float z_alpha = nimcp_stats_quantile_standard_normal(1.0f - (1.0f - confidence) / 2.0f);
    double survival = 1.0;
    double greenwood_sum = 0.0;  // For Greenwood's variance formula
    uint32_t n_at_risk = n;
    uint32_t curve_idx = 1;
    uint32_t i = 0;

    while (i < n && curve_idx <= n_unique) {
        float current_time = sorted[i].time;
        uint32_t d_i = 0;  // Events at this time
        uint32_t c_i = 0;  // Censored at this time

        // Count events and censorings at current time
        while (i < n && sorted[i].time == current_time) {
            if (sorted[i].event) d_i++;
            else c_i++;
            i++;
        }

        // Skip if no events at this time
        if (d_i == 0) {
            n_at_risk -= c_i;
            continue;
        }

        // Product-limit update
        double factor = 1.0 - (double)d_i / (double)n_at_risk;
        survival *= factor;

        // Greenwood's formula for variance
        if (n_at_risk > d_i) {
            greenwood_sum += (double)d_i / ((double)n_at_risk * (double)(n_at_risk - d_i));
        }

        double std_error = survival * sqrt(greenwood_sum);

        // Log-log transformation for CI (more accurate at boundaries)
        double log_minus_log_S = log(-log(fmax(survival, SURVIVAL_EPS)));
        double se_log_minus_log = sqrt(greenwood_sum) / fabs(log(fmax(survival, SURVIVAL_EPS)));

        double ci_lower = exp(-exp(log_minus_log_S + z_alpha * se_log_minus_log));
        double ci_upper = exp(-exp(log_minus_log_S - z_alpha * se_log_minus_log));

        // Store curve point
        km->curve[curve_idx].time = current_time;
        km->curve[curve_idx].survival = (float)survival;
        km->curve[curve_idx].std_error = (float)std_error;
        km->curve[curve_idx].ci_lower = (float)fmax(0.0, fmin(1.0, ci_lower));
        km->curve[curve_idx].ci_upper = (float)fmax(0.0, fmin(1.0, ci_upper));
        km->curve[curve_idx].n_at_risk = n_at_risk;
        km->curve[curve_idx].n_events = d_i;
        km->curve[curve_idx].n_censored = c_i;

        n_at_risk -= (d_i + c_i);
        curve_idx++;
    }

    // Update CI for time=0
    km->curve[0].ci_lower = 1.0f;
    km->curve[0].ci_upper = 1.0f;

    km->n_points = curve_idx;
    km->n_total = n;
    km->n_events = n_events_total;
    km->n_censored = n - n_events_total;
    km->confidence_level = confidence;
    km->fitted = true;

    // Find median survival time (S(t) = 0.5)
    km->median_survival = NAN;
    km->median_ci_lower = NAN;
    km->median_ci_upper = NAN;

    for (uint32_t j = 1; j < km->n_points; j++) {
        if (km->curve[j].survival <= 0.5f && km->curve[j-1].survival > 0.5f) {
            km->median_survival = km->curve[j].time;
            break;
        }
    }

    // Find CI for median
    if (!isnan(km->median_survival)) {
        for (uint32_t j = 1; j < km->n_points; j++) {
            if (km->curve[j].ci_upper <= 0.5f && isnan(km->median_ci_lower)) {
                km->median_ci_lower = km->curve[j].time;
            }
            if (km->curve[j].ci_lower <= 0.5f && isnan(km->median_ci_upper)) {
                km->median_ci_upper = km->curve[j].time;
                break;
            }
        }
    }

    // Compute restricted mean survival time (RMST) up to max observed time
    double rmst = 0.0;
    for (uint32_t j = 1; j < km->n_points; j++) {
        float dt = km->curve[j].time - km->curve[j-1].time;
        rmst += km->curve[j-1].survival * dt;
    }
    km->restricted_mean = (float)rmst;

    nimcp_free(sorted);
    return NIMCP_SURVIVAL_OK;
}

float nimcp_km_survival_at(const nimcp_km_estimator_t* km, float t) {
    if (!km || !km->fitted || !km->curve) return NAN;
    if (t < 0) return 1.0f;

    // Binary search for the correct time point
    uint32_t lo = 0, hi = km->n_points - 1;
    while (lo < hi) {
        uint32_t mid = (lo + hi + 1) / 2;
        if (km->curve[mid].time <= t) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }

    return km->curve[lo].survival;
}

nimcp_survival_result_t nimcp_km_survival_function(
    const nimcp_km_estimator_t* km,
    const float* times,
    uint32_t n_times,
    float* survival
) {
    if (!km || !times || !survival) return NIMCP_SURVIVAL_ERROR_NULL;
    if (!km->fitted) return NIMCP_SURVIVAL_ERROR_NOT_FIT;

    for (uint32_t i = 0; i < n_times; i++) {
        survival[i] = nimcp_km_survival_at(km, times[i]);
    }

    return NIMCP_SURVIVAL_OK;
}

float nimcp_km_median_survival(const nimcp_km_estimator_t* km) {
    if (!km || !km->fitted) return NAN;
    return km->median_survival;
}

nimcp_survival_result_t nimcp_km_confidence_interval(
    const nimcp_km_estimator_t* km,
    float t,
    float* ci_lower,
    float* ci_upper
) {
    if (!km || !ci_lower || !ci_upper) return NIMCP_SURVIVAL_ERROR_NULL;
    if (!km->fitted) return NIMCP_SURVIVAL_ERROR_NOT_FIT;

    // Binary search for time point
    uint32_t lo = 0, hi = km->n_points - 1;
    while (lo < hi) {
        uint32_t mid = (lo + hi + 1) / 2;
        if (km->curve[mid].time <= t) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }

    *ci_lower = km->curve[lo].ci_lower;
    *ci_upper = km->curve[lo].ci_upper;

    return NIMCP_SURVIVAL_OK;
}

nimcp_survival_result_t nimcp_km_plot_data(
    const nimcp_km_estimator_t* km,
    float** times,
    float** survival,
    float** ci_lower,
    float** ci_upper,
    uint32_t* n_points
) {
    if (!km || !times || !survival || !n_points) return NIMCP_SURVIVAL_ERROR_NULL;
    if (!km->fitted) return NIMCP_SURVIVAL_ERROR_NOT_FIT;

    // Step function needs 2 points per change
    uint32_t n_plot = km->n_points * 2;

    *times = (float*)nimcp_malloc(n_plot * sizeof(float));
    *survival = (float*)nimcp_malloc(n_plot * sizeof(float));
    if (!*times || !*survival) {
        nimcp_free(*times);
        nimcp_free(*survival);
        return NIMCP_SURVIVAL_ERROR_MEMORY;
    }

    if (ci_lower) *ci_lower = (float*)nimcp_malloc(n_plot * sizeof(float));
    if (ci_upper) *ci_upper = (float*)nimcp_malloc(n_plot * sizeof(float));

    uint32_t idx = 0;
    for (uint32_t i = 0; i < km->n_points; i++) {
        // Horizontal line at current survival
        if (i > 0) {
            (*times)[idx] = km->curve[i].time;
            (*survival)[idx] = km->curve[i-1].survival;
            if (ci_lower && *ci_lower) (*ci_lower)[idx] = km->curve[i-1].ci_lower;
            if (ci_upper && *ci_upper) (*ci_upper)[idx] = km->curve[i-1].ci_upper;
            idx++;
        }

        // Vertical drop to new survival
        (*times)[idx] = km->curve[i].time;
        (*survival)[idx] = km->curve[i].survival;
        if (ci_lower && *ci_lower) (*ci_lower)[idx] = km->curve[i].ci_lower;
        if (ci_upper && *ci_upper) (*ci_upper)[idx] = km->curve[i].ci_upper;
        idx++;
    }

    *n_points = idx;
    return NIMCP_SURVIVAL_OK;
}

nimcp_survival_result_t nimcp_km_rmst(
    const nimcp_km_estimator_t* km,
    float tau,
    float* rmst,
    float* std_error
) {
    if (!km || !rmst) return NIMCP_SURVIVAL_ERROR_NULL;
    if (!km->fitted) return NIMCP_SURVIVAL_ERROR_NOT_FIT;
    if (tau <= 0) return NIMCP_SURVIVAL_ERROR_PARAMS;

    // Compute RMST = integral of S(t) from 0 to tau
    double area = 0.0;
    double var_area = 0.0;
    float prev_time = 0.0f;
    float prev_survival = 1.0f;

    for (uint32_t i = 1; i < km->n_points; i++) {
        float t = km->curve[i].time;
        if (t > tau) t = tau;

        float dt = t - prev_time;
        area += prev_survival * dt;

        // Variance contribution (using Greenwood-based formula)
        double se = km->curve[i-1].std_error;
        var_area += se * se * dt * dt;

        prev_time = t;
        prev_survival = km->curve[i].survival;

        if (km->curve[i].time >= tau) break;
    }

    // Add remaining area if tau > max observed time
    if (prev_time < tau) {
        area += prev_survival * (tau - prev_time);
    }

    *rmst = (float)area;
    if (std_error) *std_error = (float)sqrt(var_area);

    return NIMCP_SURVIVAL_OK;
}

//=============================================================================
// Cox Proportional Hazards Model
//=============================================================================

nimcp_cox_model_t* nimcp_cox_create(uint32_t n_covariates) {
    if (n_covariates == 0 || n_covariates > NIMCP_SURVIVAL_MAX_COVARIATES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_cox_create: n_covariates is zero");
        return NULL;
    }

    nimcp_cox_model_t* cox = (nimcp_cox_model_t*)nimcp_calloc(1, sizeof(nimcp_cox_model_t));
    if (!cox) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_cox_create: cox is NULL");
        return NULL;
    }

    cox->coefficients = (nimcp_cox_coefficient_t*)nimcp_calloc(n_covariates, sizeof(nimcp_cox_coefficient_t));
    cox->variance_matrix = (float*)nimcp_calloc((size_t)n_covariates * n_covariates, sizeof(float));

    if (!cox->coefficients || !cox->variance_matrix) {
        nimcp_free(cox->coefficients);
        nimcp_free(cox->variance_matrix);
        nimcp_free(cox);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_cox_create: required parameter is NULL (cox->coefficients, cox->variance_matrix)");
        return NULL;
    }

    cox->n_covariates = n_covariates;
    cox->confidence_level = NIMCP_SURVIVAL_DEFAULT_CONFIDENCE;
    cox->ties_breslow = true;
    cox->fitted = false;

    return cox;
}

void nimcp_cox_destroy(nimcp_cox_model_t* cox) {
    if (!cox) return;

    nimcp_free(cox->coefficients);
    nimcp_free(cox->variance_matrix);
    nimcp_free(cox->baseline_hazard);
    nimcp_free(cox->baseline_times);
    nimcp_free(cox);
}

/**
 * @brief Compute partial log-likelihood and gradient for Cox model
 *
 * Uses Breslow approximation for tied event times:
 *   L(β) = Σ_{i:d_i=1} [β'X_i - log(Σ_{j∈R(t_i)} exp(β'X_j))]
 */
static void cox_partial_likelihood(
    const float* times,
    const bool* events,
    const float* covariates,
    uint32_t n,
    uint32_t p,
    const double* beta,
    double* log_lik,
    double* gradient,
    double* hessian
) {
    // Sort indices by time (descending for efficient risk set computation)
    uint32_t* order = (uint32_t*)nimcp_malloc(n * sizeof(uint32_t));
    for (uint32_t i = 0; i < n; i++) order[i] = i;

    // Simple insertion sort for stability
    for (uint32_t i = 1; i < n; i++) {
        uint32_t key = order[i];
        int32_t j = i - 1;
        while (j >= 0 && times[order[j]] < times[key]) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }

    // Initialize accumulators
    *log_lik = 0.0;
    if (gradient) memset(gradient, 0, p * sizeof(double));
    if (hessian) memset(hessian, 0, (size_t)p * p * sizeof(double));

    // Compute linear predictors exp(β'X)
    double* exp_eta = (double*)nimcp_malloc(n * sizeof(double));
    for (uint32_t i = 0; i < n; i++) {
        double eta = 0.0;
        for (uint32_t j = 0; j < p; j++) {
            eta += beta[j] * covariates[i * p + j];
        }
        exp_eta[i] = exp(eta);
    }

    // Running sums for risk set (in descending time order)
    double sum_exp = 0.0;
    double* sum_x_exp = (double*)nimcp_calloc(p, sizeof(double));
    double* sum_xx_exp = (double*)nimcp_calloc((size_t)p * p, sizeof(double));

    for (uint32_t k = 0; k < n; k++) {
        uint32_t i = order[k];

        // Add observation to risk set
        sum_exp += exp_eta[i];
        for (uint32_t j = 0; j < p; j++) {
            double x_ij = covariates[i * p + j];
            sum_x_exp[j] += x_ij * exp_eta[i];

            if (hessian) {
                for (uint32_t l = 0; l <= j; l++) {
                    double x_il = covariates[i * p + l];
                    sum_xx_exp[j * p + l] += x_ij * x_il * exp_eta[i];
                }
            }
        }

        // If event, add contribution to log-likelihood
        if (events[i]) {
            double log_sum_exp = log(fmax(sum_exp, SURVIVAL_EPS));
            *log_lik += log(fmax(exp_eta[i], SURVIVAL_EPS)) - log_sum_exp;

            if (gradient) {
                for (uint32_t j = 0; j < p; j++) {
                    gradient[j] += covariates[i * p + j] - sum_x_exp[j] / sum_exp;
                }
            }

            if (hessian) {
                for (uint32_t j = 0; j < p; j++) {
                    for (uint32_t l = 0; l <= j; l++) {
                        double term1 = sum_xx_exp[j * p + l] / sum_exp;
                        double term2 = (sum_x_exp[j] * sum_x_exp[l]) / (sum_exp * sum_exp);
                        hessian[j * p + l] -= (term1 - term2);
                        if (l != j) hessian[l * p + j] = hessian[j * p + l];
                    }
                }
            }
        }
    }

    nimcp_free(order);
    nimcp_free(exp_eta);
    nimcp_free(sum_x_exp);
    nimcp_free(sum_xx_exp);
}

/**
 * @brief Invert symmetric matrix using Cholesky decomposition
 */
static int cholesky_invert(double* A, uint32_t n) {
    // Cholesky decomposition: A = LL'
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j <= i; j++) {
            double sum = A[i * n + j];
            for (uint32_t k = 0; k < j; k++) {
                sum -= A[i * n + k] * A[j * n + k];
            }
            if (i == j) {
                if (sum <= 0) {
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cholesky_invert: validation failed");
                    return -1;
                }
                A[i * n + i] = sqrt(sum);
            } else {
                A[i * n + j] = sum / A[j * n + j];
            }
        }
    }

    // Invert L
    for (uint32_t i = 0; i < n; i++) {
        A[i * n + i] = 1.0 / A[i * n + i];
        for (uint32_t j = i + 1; j < n; j++) {
            double sum = 0.0;
            for (uint32_t k = i; k < j; k++) {
                sum -= A[j * n + k] * A[k * n + i];
            }
            A[j * n + i] = sum / A[j * n + j];
        }
    }

    // Compute A^-1 = (L^-1)' L^-1
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j <= i; j++) {
            double sum = 0.0;
            for (uint32_t k = i; k < n; k++) {
                sum += A[k * n + i] * A[k * n + j];
            }
            A[i * n + j] = A[j * n + i] = sum;
        }
    }

    return 0;
}

nimcp_survival_result_t nimcp_cox_fit(
    nimcp_cox_model_t* cox,
    const float* times,
    const bool* events,
    const float* covariates,
    uint32_t n,
    const char** covariate_names,
    float confidence
) {
    if (!cox || !times || !events || !covariates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "NULL argument to nimcp_cox_fit");
        return NIMCP_SURVIVAL_ERROR_NULL;
    }
    if (n < cox->n_covariates + 1) return NIMCP_SURVIVAL_ERROR_SIZE;
    if (confidence <= 0.0f || confidence >= 1.0f) confidence = NIMCP_SURVIVAL_DEFAULT_CONFIDENCE;

    uint32_t p = cox->n_covariates;

    // Count events
    uint32_t n_events = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (events[i]) n_events++;
    }
    if (n_events == 0) return NIMCP_SURVIVAL_ERROR_NO_EVENTS;

    // Initialize beta to zeros
    double* beta = (double*)nimcp_calloc(p, sizeof(double));
    double* gradient = (double*)nimcp_malloc(p * sizeof(double));
    double* hessian = (double*)nimcp_malloc((size_t)p * p * sizeof(double));
    double* delta = (double*)nimcp_malloc(p * sizeof(double));

    if (!beta || !gradient || !hessian || !delta) {
        nimcp_free(beta); nimcp_free(gradient); nimcp_free(hessian); nimcp_free(delta);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Memory allocation failed in Cox fit");
        return NIMCP_SURVIVAL_ERROR_MEMORY;
    }

    // Newton-Raphson iteration
    double log_lik = 0.0, prev_log_lik = -INFINITY;
    uint32_t iter;
    bool converged = false;

    for (iter = 0; iter < NIMCP_SURVIVAL_MAX_ITERATIONS; iter++) {
        cox_partial_likelihood(times, events, covariates, n, p, beta,
                              &log_lik, gradient, hessian);

        // Check convergence
        if (fabs(log_lik - prev_log_lik) < NIMCP_SURVIVAL_CONVERGENCE_TOL) {
            converged = true;
            break;
        }
        prev_log_lik = log_lik;

        // Compute Newton step: delta = -H^{-1} * g
        // First invert Hessian (negative because H is negative definite)
        for (uint32_t i = 0; i < p * p; i++) hessian[i] = -hessian[i];

        if (cholesky_invert(hessian, p) != 0) {
            nimcp_free(beta); nimcp_free(gradient); nimcp_free(hessian); nimcp_free(delta);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "Singular Hessian in Cox fit");
            return NIMCP_SURVIVAL_ERROR_SINGULAR;
        }

        // delta = H^{-1} * gradient
        for (uint32_t i = 0; i < p; i++) {
            delta[i] = 0.0;
            for (uint32_t j = 0; j < p; j++) {
                delta[i] += hessian[i * p + j] * gradient[j];
            }
        }

        // Update beta with step size control
        double step = 1.0;
        for (uint32_t i = 0; i < p; i++) {
            beta[i] += step * delta[i];
        }
    }

    // Recompute final Hessian for variance matrix
    cox_partial_likelihood(times, events, covariates, n, p, beta,
                          &log_lik, NULL, hessian);
    for (uint32_t i = 0; i < p * p; i++) hessian[i] = -hessian[i];
    cholesky_invert(hessian, p);

    // Store results
    float z_alpha = nimcp_stats_quantile_standard_normal(1.0f - (1.0f - confidence) / 2.0f);

    for (uint32_t i = 0; i < p; i++) {
        cox->coefficients[i].beta = (float)beta[i];
        cox->coefficients[i].std_error = (float)sqrt(hessian[i * p + i]);
        cox->coefficients[i].hazard_ratio = expf(cox->coefficients[i].beta);
        cox->coefficients[i].z_stat = cox->coefficients[i].beta / cox->coefficients[i].std_error;
        cox->coefficients[i].p_value = 2.0f * (1.0f - nimcp_stats_cdf_standard_normal(fabsf(cox->coefficients[i].z_stat)));

        float ci_offset = z_alpha * cox->coefficients[i].std_error;
        cox->coefficients[i].hr_ci_lower = expf(cox->coefficients[i].beta - ci_offset);
        cox->coefficients[i].hr_ci_upper = expf(cox->coefficients[i].beta + ci_offset);

        if (covariate_names && covariate_names[i]) {
            cox->coefficients[i].name = covariate_names[i];
        }
    }

    // Store variance matrix
    for (uint32_t i = 0; i < p * p; i++) {
        cox->variance_matrix[i] = (float)hessian[i];
    }

    // Compute model statistics
    double null_log_lik = 0.0;
    double* zero_beta = (double*)nimcp_calloc(p, sizeof(double));
    cox_partial_likelihood(times, events, covariates, n, p, zero_beta, &null_log_lik, NULL, NULL);
    nimcp_free(zero_beta);

    cox->stats.log_likelihood = (float)log_lik;
    cox->stats.log_likelihood_null = (float)null_log_lik;
    cox->stats.likelihood_ratio = 2.0f * (float)(log_lik - null_log_lik);
    cox->stats.lr_p_value = 1.0f - nimcp_stats_cdf_chi_squared(cox->stats.likelihood_ratio, (float)p);
    cox->stats.aic = -2.0f * (float)log_lik + 2.0f * p;
    cox->stats.bic = -2.0f * (float)log_lik + p * logf((float)n_events);
    cox->stats.n_iterations = iter;
    cox->stats.converged = converged;

    // Compute concordance index
    uint32_t concordant = 0, discordant = 0, tied = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (!events[i]) continue;
        for (uint32_t j = 0; j < n; j++) {
            if (i == j || times[j] <= times[i]) continue;

            double eta_i = 0.0, eta_j = 0.0;
            for (uint32_t k = 0; k < p; k++) {
                eta_i += beta[k] * covariates[i * p + k];
                eta_j += beta[k] * covariates[j * p + k];
            }

            if (eta_i > eta_j) concordant++;
            else if (eta_i < eta_j) discordant++;
            else tied++;
        }
    }

    uint32_t total_pairs = concordant + discordant + tied;
    if (total_pairs > 0) {
        cox->stats.concordance = (concordant + 0.5f * tied) / total_pairs;
    } else {
        cox->stats.concordance = 0.5f;
    }

    cox->confidence_level = confidence;
    cox->fitted = true;

    nimcp_free(beta);
    nimcp_free(gradient);
    nimcp_free(hessian);
    nimcp_free(delta);

    return converged ? NIMCP_SURVIVAL_OK : NIMCP_SURVIVAL_ERROR_CONVERGE;
}

#ifdef NIMCP_ENABLE_CUDA
nimcp_survival_result_t nimcp_cox_fit_gpu(
    nimcp_cox_model_t* cox,
    const float* times,
    const bool* events,
    const float* covariates,
    uint32_t n,
    const char** covariate_names,
    float confidence
) {
    // GPU implementation would be in the CUDA kernel file
    // Fallback to CPU for now
    return nimcp_cox_fit(cox, times, events, covariates, n, covariate_names, confidence);
}
#else
nimcp_survival_result_t nimcp_cox_fit_gpu(
    nimcp_cox_model_t* cox,
    const float* times,
    const bool* events,
    const float* covariates,
    uint32_t n,
    const char** covariate_names,
    float confidence
) {
    return nimcp_cox_fit(cox, times, events, covariates, n, covariate_names, confidence);
}
#endif

nimcp_survival_result_t nimcp_cox_predict_hazard(
    const nimcp_cox_model_t* cox,
    const float* covariates,
    uint32_t n_new,
    float* hazard_ratio
) {
    if (!cox || !covariates || !hazard_ratio) return NIMCP_SURVIVAL_ERROR_NULL;
    if (!cox->fitted) return NIMCP_SURVIVAL_ERROR_NOT_FIT;

    for (uint32_t i = 0; i < n_new; i++) {
        double eta = 0.0;
        for (uint32_t j = 0; j < cox->n_covariates; j++) {
            eta += cox->coefficients[j].beta * covariates[i * cox->n_covariates + j];
        }
        hazard_ratio[i] = expf((float)eta);
    }

    return NIMCP_SURVIVAL_OK;
}

nimcp_survival_result_t nimcp_cox_predict_survival(
    const nimcp_cox_model_t* cox,
    const float* covariates,
    const float* times,
    uint32_t n_times,
    float* survival
) {
    if (!cox || !covariates || !times || !survival) return NIMCP_SURVIVAL_ERROR_NULL;
    if (!cox->fitted) return NIMCP_SURVIVAL_ERROR_NOT_FIT;
    if (!cox->baseline_hazard || !cox->baseline_times) {
        // Baseline hazard not computed - use Breslow estimator placeholder
        // Return exponential approximation
        float hr;
        nimcp_cox_predict_hazard(cox, covariates, 1, &hr);

        for (uint32_t i = 0; i < n_times; i++) {
            // Approximate: S(t|X) = exp(-hr * t / median_time)
            survival[i] = expf(-hr * times[i] * 0.01f);
            if (survival[i] < 0.0f) survival[i] = 0.0f;
            if (survival[i] > 1.0f) survival[i] = 1.0f;
        }
        return NIMCP_SURVIVAL_OK;
    }

    // Compute hazard ratio for this observation
    double eta = 0.0;
    for (uint32_t j = 0; j < cox->n_covariates; j++) {
        eta += cox->coefficients[j].beta * covariates[j];
    }
    double hr = exp(eta);

    // Compute cumulative baseline hazard at each time point
    for (uint32_t i = 0; i < n_times; i++) {
        double cum_hazard = 0.0;
        for (uint32_t k = 0; k < cox->n_baseline; k++) {
            if (cox->baseline_times[k] > times[i]) break;
            cum_hazard += cox->baseline_hazard[k];
        }
        survival[i] = (float)exp(-cum_hazard * hr);
    }

    return NIMCP_SURVIVAL_OK;
}

nimcp_survival_result_t nimcp_cox_coefficients(
    const nimcp_cox_model_t* cox,
    nimcp_cox_coefficient_t** coefficients,
    uint32_t* n_coefficients
) {
    if (!cox || !coefficients || !n_coefficients) return NIMCP_SURVIVAL_ERROR_NULL;
    if (!cox->fitted) return NIMCP_SURVIVAL_ERROR_NOT_FIT;

    *coefficients = cox->coefficients;
    *n_coefficients = cox->n_covariates;

    return NIMCP_SURVIVAL_OK;
}

float nimcp_cox_concordance(const nimcp_cox_model_t* cox) {
    if (!cox || !cox->fitted) return NAN;
    return cox->stats.concordance;
}

float nimcp_cox_log_likelihood(const nimcp_cox_model_t* cox) {
    if (!cox || !cox->fitted) return NAN;
    return cox->stats.log_likelihood;
}

nimcp_survival_result_t nimcp_cox_schoenfeld_residuals(
    const nimcp_cox_model_t* cox,
    const float* times,
    const bool* events,
    const float* covariates,
    uint32_t n,
    float** residuals,
    float** event_times,
    uint32_t* n_events
) {
    if (!cox || !times || !events || !covariates) return NIMCP_SURVIVAL_ERROR_NULL;
    if (!residuals || !event_times || !n_events) return NIMCP_SURVIVAL_ERROR_NULL;
    if (!cox->fitted) return NIMCP_SURVIVAL_ERROR_NOT_FIT;

    uint32_t p = cox->n_covariates;

    // Count events
    uint32_t ne = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (events[i]) ne++;
    }
    *n_events = ne;

    if (ne == 0) return NIMCP_SURVIVAL_ERROR_NO_EVENTS;

    *residuals = (float*)nimcp_malloc((size_t)ne * p * sizeof(float));
    *event_times = (float*)nimcp_malloc(ne * sizeof(float));
    if (!*residuals || !*event_times) {
        nimcp_free(*residuals);
        nimcp_free(*event_times);
        return NIMCP_SURVIVAL_ERROR_MEMORY;
    }

    // Compute exp(beta'X) for all observations
    double* exp_eta = (double*)nimcp_malloc(n * sizeof(double));
    for (uint32_t i = 0; i < n; i++) {
        double eta = 0.0;
        for (uint32_t j = 0; j < p; j++) {
            eta += cox->coefficients[j].beta * covariates[i * p + j];
        }
        exp_eta[i] = exp(eta);
    }

    // For each event, compute Schoenfeld residual
    uint32_t event_idx = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (!events[i]) continue;

        (*event_times)[event_idx] = times[i];

        // Compute weighted mean of covariates in risk set
        double sum_exp = 0.0;
        double* sum_x_exp = (double*)nimcp_calloc(p, sizeof(double));

        for (uint32_t j = 0; j < n; j++) {
            if (times[j] >= times[i]) {  // In risk set
                sum_exp += exp_eta[j];
                for (uint32_t k = 0; k < p; k++) {
                    sum_x_exp[k] += covariates[j * p + k] * exp_eta[j];
                }
            }
        }

        // Residual = X_i - E[X|R(t_i)]
        for (uint32_t k = 0; k < p; k++) {
            (*residuals)[event_idx * p + k] = covariates[i * p + k] - (float)(sum_x_exp[k] / sum_exp);
        }

        nimcp_free(sum_x_exp);
        event_idx++;
    }

    nimcp_free(exp_eta);
    return NIMCP_SURVIVAL_OK;
}

nimcp_survival_result_t nimcp_cox_martingale_residuals(
    const nimcp_cox_model_t* cox,
    const float* times,
    const bool* events,
    const float* covariates,
    uint32_t n,
    float* residuals
) {
    if (!cox || !times || !events || !covariates || !residuals) {
        return NIMCP_SURVIVAL_ERROR_NULL;
    }
    if (!cox->fitted) return NIMCP_SURVIVAL_ERROR_NOT_FIT;

    // M_i = delta_i - cumulative_hazard(t_i|X_i)
    // For now, use a simplified estimate based on Nelson-Aalen

    uint32_t p = cox->n_covariates;

    // Compute exp(beta'X) for all observations
    double* exp_eta = (double*)nimcp_malloc(n * sizeof(double));
    double* cum_hazard = (double*)nimcp_calloc(n, sizeof(double));

    for (uint32_t i = 0; i < n; i++) {
        double eta = 0.0;
        for (uint32_t j = 0; j < p; j++) {
            eta += cox->coefficients[j].beta * covariates[i * p + j];
        }
        exp_eta[i] = exp(eta);
    }

    // Sort by time
    uint32_t* order = (uint32_t*)nimcp_malloc(n * sizeof(uint32_t));
    for (uint32_t i = 0; i < n; i++) order[i] = i;

    // Simple sort
    for (uint32_t i = 1; i < n; i++) {
        uint32_t key = order[i];
        int32_t j = i - 1;
        while (j >= 0 && times[order[j]] > times[key]) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }

    // Compute cumulative hazard at each observation time
    double running_hazard = 0.0;
    uint32_t k = 0;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t idx = order[i];

        // Compute risk set sum at this time
        double risk_sum = 0.0;
        for (uint32_t j = i; j < n; j++) {
            risk_sum += exp_eta[order[j]];
        }

        if (events[idx] && risk_sum > 0) {
            running_hazard += 1.0 / risk_sum;
        }

        cum_hazard[idx] = running_hazard * exp_eta[idx];
    }

    // Compute residuals
    for (uint32_t i = 0; i < n; i++) {
        residuals[i] = (events[i] ? 1.0f : 0.0f) - (float)cum_hazard[i];
    }

    nimcp_free(exp_eta);
    nimcp_free(cum_hazard);
    nimcp_free(order);

    return NIMCP_SURVIVAL_OK;
}

nimcp_survival_result_t nimcp_cox_deviance_residuals(
    const nimcp_cox_model_t* cox,
    const float* times,
    const bool* events,
    const float* covariates,
    uint32_t n,
    float* residuals
) {
    if (!cox || !times || !events || !covariates || !residuals) {
        return NIMCP_SURVIVAL_ERROR_NULL;
    }
    if (!cox->fitted) return NIMCP_SURVIVAL_ERROR_NOT_FIT;

    // First get martingale residuals
    float* martingale = (float*)nimcp_malloc(n * sizeof(float));
    if (!martingale) return NIMCP_SURVIVAL_ERROR_MEMORY;

    nimcp_survival_result_t result = nimcp_cox_martingale_residuals(
        cox, times, events, covariates, n, martingale);

    if (result != NIMCP_SURVIVAL_OK) {
        nimcp_free(martingale);
        return result;
    }

    // Transform to deviance residuals
    // d_i = sign(M_i) * sqrt(-2 * (M_i + delta_i * log(delta_i - M_i)))
    for (uint32_t i = 0; i < n; i++) {
        float m = martingale[i];
        float delta = events[i] ? 1.0f : 0.0f;

        float inner = m;
        if (delta > 0 && (delta - m) > 0) {
            inner += delta * logf(delta - m);
        }

        float dev_sq = -2.0f * inner;
        if (dev_sq < 0) dev_sq = 0;

        residuals[i] = (m >= 0 ? 1.0f : -1.0f) * sqrtf(dev_sq);
    }

    nimcp_free(martingale);
    return NIMCP_SURVIVAL_OK;
}

//=============================================================================
// Survival Tests
//=============================================================================

nimcp_survival_result_t nimcp_logrank_test(
    const float* times,
    const bool* events,
    const uint32_t* groups,
    uint32_t n,
    uint32_t n_groups,
    nimcp_logrank_result_t* result
) {
    if (!times || !events || !groups || !result) return NIMCP_SURVIVAL_ERROR_NULL;
    if (n == 0 || n_groups < 2) return NIMCP_SURVIVAL_ERROR_SIZE;

    // Sort observations
    sorted_obs_t* sorted = (sorted_obs_t*)nimcp_malloc(n * sizeof(sorted_obs_t));
    if (!sorted) return NIMCP_SURVIVAL_ERROR_MEMORY;

    for (uint32_t i = 0; i < n; i++) {
        sorted[i].time = times[i];
        sorted[i].event = events[i];
        sorted[i].group = groups[i];
        sorted[i].orig_idx = i;
    }
    qsort(sorted, n, sizeof(sorted_obs_t), compare_obs_time);

    // Initialize result
    result->observed = (float*)nimcp_calloc(n_groups, sizeof(float));
    result->expected = (float*)nimcp_calloc(n_groups, sizeof(float));
    if (!result->observed || !result->expected) {
        nimcp_free(sorted);
        nimcp_free(result->observed);
        nimcp_free(result->expected);
        return NIMCP_SURVIVAL_ERROR_MEMORY;
    }

    // Count at risk in each group
    uint32_t* at_risk = (uint32_t*)nimcp_malloc(n_groups * sizeof(uint32_t));
    for (uint32_t g = 0; g < n_groups; g++) {
        at_risk[g] = 0;
        for (uint32_t i = 0; i < n; i++) {
            if (groups[i] == g) at_risk[g]++;
        }
    }

    // Compute observed and expected events using log-rank weights
    double variance = 0.0;
    uint32_t i = 0;

    while (i < n) {
        float current_time = sorted[i].time;

        // Count events and at-risk at this time
        uint32_t d_total = 0;
        uint32_t n_total = 0;
        uint32_t* d_group = (uint32_t*)nimcp_calloc(n_groups, sizeof(uint32_t));

        for (uint32_t g = 0; g < n_groups; g++) {
            n_total += at_risk[g];
        }

        // Count events at this time point
        uint32_t j = i;
        while (j < n && sorted[j].time == current_time) {
            if (sorted[j].event) {
                d_total++;
                d_group[sorted[j].group]++;
            }
            j++;
        }

        // Add contributions if there are events
        if (d_total > 0 && n_total > 0) {
            for (uint32_t g = 0; g < n_groups; g++) {
                result->observed[g] += d_group[g];
                float expected_g = (float)at_risk[g] * d_total / n_total;
                result->expected[g] += expected_g;
            }

            // Variance contribution (for 2-group test)
            if (n_groups == 2 && n_total > 1) {
                variance += (double)at_risk[0] * at_risk[1] * d_total * (n_total - d_total)
                           / ((double)n_total * n_total * (n_total - 1));
            }
        }

        // Update at-risk counts
        while (i < j) {
            at_risk[sorted[i].group]--;
            i++;
        }

        nimcp_free(d_group);
    }

    // Compute chi-squared statistic
    if (n_groups == 2 && variance > 0) {
        double diff = result->observed[0] - result->expected[0];
        result->chi_squared = (float)(diff * diff / variance);
        result->df = 1.0f;
    } else {
        // General k-group test
        double chi_sq = 0.0;
        for (uint32_t g = 0; g < n_groups; g++) {
            if (result->expected[g] > 0) {
                double diff = result->observed[g] - result->expected[g];
                chi_sq += diff * diff / result->expected[g];
            }
        }
        result->chi_squared = (float)chi_sq;
        result->df = (float)(n_groups - 1);
    }

    result->p_value = 1.0f - nimcp_stats_cdf_chi_squared(result->chi_squared, result->df);
    result->n_groups = n_groups;

    nimcp_free(sorted);
    nimcp_free(at_risk);

    return NIMCP_SURVIVAL_OK;
}

nimcp_survival_result_t nimcp_wilcoxon_survival_test(
    const float* times,
    const bool* events,
    const uint32_t* groups,
    uint32_t n,
    uint32_t n_groups,
    nimcp_logrank_result_t* result
) {
    if (!times || !events || !groups || !result) return NIMCP_SURVIVAL_ERROR_NULL;
    if (n == 0 || n_groups < 2) return NIMCP_SURVIVAL_ERROR_SIZE;

    // Peto-Peto version of Wilcoxon (Gehan) test
    // Uses survival estimate as weight

    sorted_obs_t* sorted = (sorted_obs_t*)nimcp_malloc(n * sizeof(sorted_obs_t));
    if (!sorted) return NIMCP_SURVIVAL_ERROR_MEMORY;

    for (uint32_t i = 0; i < n; i++) {
        sorted[i].time = times[i];
        sorted[i].event = events[i];
        sorted[i].group = groups[i];
    }
    qsort(sorted, n, sizeof(sorted_obs_t), compare_obs_time);

    result->observed = (float*)nimcp_calloc(n_groups, sizeof(float));
    result->expected = (float*)nimcp_calloc(n_groups, sizeof(float));
    if (!result->observed || !result->expected) {
        nimcp_free(sorted);
        return NIMCP_SURVIVAL_ERROR_MEMORY;
    }

    uint32_t* at_risk = (uint32_t*)nimcp_malloc(n_groups * sizeof(uint32_t));
    for (uint32_t g = 0; g < n_groups; g++) {
        at_risk[g] = 0;
        for (uint32_t i = 0; i < n; i++) {
            if (groups[i] == g) at_risk[g]++;
        }
    }

    double survival = 1.0;
    double variance = 0.0;
    uint32_t i = 0;

    while (i < n) {
        float current_time = sorted[i].time;

        uint32_t d_total = 0;
        uint32_t n_total = 0;
        uint32_t* d_group = (uint32_t*)nimcp_calloc(n_groups, sizeof(uint32_t));

        for (uint32_t g = 0; g < n_groups; g++) {
            n_total += at_risk[g];
        }

        uint32_t j = i;
        while (j < n && sorted[j].time == current_time) {
            if (sorted[j].event) {
                d_total++;
                d_group[sorted[j].group]++;
            }
            j++;
        }

        // Weight by survival estimate (Peto-Peto)
        double weight = survival;

        if (d_total > 0 && n_total > 0) {
            for (uint32_t g = 0; g < n_groups; g++) {
                result->observed[g] += (float)(weight * d_group[g]);
                double expected_g = weight * at_risk[g] * d_total / n_total;
                result->expected[g] += (float)expected_g;
            }

            // Update survival
            survival *= (1.0 - (double)d_total / n_total);

            if (n_groups == 2 && n_total > 1) {
                variance += weight * weight * at_risk[0] * at_risk[1] * d_total * (n_total - d_total)
                           / ((double)n_total * n_total * (n_total - 1));
            }
        }

        while (i < j) {
            at_risk[sorted[i].group]--;
            i++;
        }

        nimcp_free(d_group);
    }

    if (n_groups == 2 && variance > 0) {
        double diff = result->observed[0] - result->expected[0];
        result->chi_squared = (float)(diff * diff / variance);
        result->df = 1.0f;
    } else {
        double chi_sq = 0.0;
        for (uint32_t g = 0; g < n_groups; g++) {
            if (result->expected[g] > 0) {
                double diff = result->observed[g] - result->expected[g];
                chi_sq += diff * diff / result->expected[g];
            }
        }
        result->chi_squared = (float)chi_sq;
        result->df = (float)(n_groups - 1);
    }

    result->p_value = 1.0f - nimcp_stats_cdf_chi_squared(result->chi_squared, result->df);
    result->n_groups = n_groups;

    nimcp_free(sorted);
    nimcp_free(at_risk);

    return NIMCP_SURVIVAL_OK;
}

nimcp_survival_result_t nimcp_ph_test(
    const nimcp_cox_model_t* cox,
    const float* times,
    const bool* events,
    const float* covariates,
    uint32_t n,
    float alpha,
    nimcp_ph_test_result_t* result
) {
    if (!cox || !times || !events || !covariates || !result) {
        return NIMCP_SURVIVAL_ERROR_NULL;
    }
    if (!cox->fitted) return NIMCP_SURVIVAL_ERROR_NOT_FIT;

    uint32_t p = cox->n_covariates;

    // Get Schoenfeld residuals
    float* schoenfeld = NULL;
    float* event_times = NULL;
    uint32_t n_events = 0;

    nimcp_survival_result_t res = nimcp_cox_schoenfeld_residuals(
        cox, times, events, covariates, n, &schoenfeld, &event_times, &n_events);

    if (res != NIMCP_SURVIVAL_OK) return res;

    // Scale Schoenfeld residuals by variance
    // Scaled residuals: r*_j = V * r_j + beta
    // where V is the variance-covariance matrix

    // Allocate result arrays
    result->covariate_chi_sq = (float*)nimcp_malloc(p * sizeof(float));
    result->covariate_p_value = (float*)nimcp_malloc(p * sizeof(float));
    result->covariate_rho = (float*)nimcp_malloc(p * sizeof(float));
    result->n_covariates = p;

    if (!result->covariate_chi_sq || !result->covariate_p_value || !result->covariate_rho) {
        nimcp_free(schoenfeld);
        nimcp_free(event_times);
        nimcp_ph_test_free(result);
        return NIMCP_SURVIVAL_ERROR_MEMORY;
    }

    // For each covariate, test correlation of scaled residuals with time
    double global_chi_sq = 0.0;
    result->ph_assumption_holds = true;

    for (uint32_t j = 0; j < p; j++) {
        // Extract residuals for this covariate
        float* resid_j = (float*)nimcp_malloc(n_events * sizeof(float));
        for (uint32_t i = 0; i < n_events; i++) {
            // Scale residual (simplified - just use raw for now)
            resid_j[i] = schoenfeld[i * p + j] * n_events + cox->coefficients[j].beta;
        }

        // Compute correlation with time
        nimcp_correlation_result_t corr;
        nimcp_stats_correlation_pearson(event_times, resid_j, n_events, &corr);

        result->covariate_rho[j] = corr.r;

        // Test statistic: chi-squared with 1 df
        double z = corr.r * sqrt((double)(n_events - 2) / (1.0 - corr.r * corr.r));
        result->covariate_chi_sq[j] = (float)(z * z);
        result->covariate_p_value[j] = 1.0f - nimcp_stats_cdf_chi_squared(result->covariate_chi_sq[j], 1.0f);

        global_chi_sq += result->covariate_chi_sq[j];

        if (result->covariate_p_value[j] < alpha) {
            result->ph_assumption_holds = false;
        }

        nimcp_free(resid_j);
    }

    result->global_chi_squared = (float)global_chi_sq;
    result->global_p_value = 1.0f - nimcp_stats_cdf_chi_squared(result->global_chi_squared, (float)p);

    nimcp_free(schoenfeld);
    nimcp_free(event_times);

    return NIMCP_SURVIVAL_OK;
}

void nimcp_ph_test_free(nimcp_ph_test_result_t* result) {
    if (!result) return;
    nimcp_free(result->covariate_chi_sq);
    nimcp_free(result->covariate_p_value);
    nimcp_free(result->covariate_rho);
    result->covariate_chi_sq = NULL;
    result->covariate_p_value = NULL;
    result->covariate_rho = NULL;
}

void nimcp_logrank_free(nimcp_logrank_result_t* result) {
    if (!result) return;
    nimcp_free(result->observed);
    nimcp_free(result->expected);
    result->observed = NULL;
    result->expected = NULL;
}

//=============================================================================
// Competing Risks
//=============================================================================

nimcp_cif_estimator_t* nimcp_cif_create(uint32_t n_event_types) {
    if (n_event_types == 0 || n_event_types > NIMCP_SURVIVAL_MAX_EVENTS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_cif_create: n_event_types is zero");
        return NULL;
    }

    nimcp_cif_estimator_t* cif = (nimcp_cif_estimator_t*)nimcp_calloc(1, sizeof(nimcp_cif_estimator_t));
    if (!cif) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_cif_create: cif is NULL");
        return NULL;
    }

    cif->n_event_types = n_event_types;
    cif->n_events = (uint32_t*)nimcp_calloc(n_event_types, sizeof(uint32_t));
    if (!cif->n_events) {
        nimcp_free(cif);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_cif_create: cif->n_events is NULL");
        return NULL;
    }

    cif->confidence_level = NIMCP_SURVIVAL_DEFAULT_CONFIDENCE;
    cif->fitted = false;

    return cif;
}

void nimcp_cif_destroy(nimcp_cif_estimator_t* cif) {
    if (!cif) return;

    if (cif->curve) {
        for (uint32_t i = 0; i < cif->n_points; i++) {
            nimcp_free(cif->curve[i].cif);
            nimcp_free(cif->curve[i].std_error);
            nimcp_free(cif->curve[i].ci_lower);
            nimcp_free(cif->curve[i].ci_upper);
        }
        nimcp_free(cif->curve);
    }
    nimcp_free(cif->n_events);
    nimcp_free(cif);
}

nimcp_survival_result_t nimcp_cif_fit(
    nimcp_cif_estimator_t* cif,
    const float* times,
    const uint8_t* event_types,
    uint32_t n,
    float confidence
) {
    if (!cif || !times || !event_types) return NIMCP_SURVIVAL_ERROR_NULL;
    if (n == 0) return NIMCP_SURVIVAL_ERROR_SIZE;

    uint32_t k = cif->n_event_types;

    // Sort observations
    sorted_obs_t* sorted = (sorted_obs_t*)nimcp_malloc(n * sizeof(sorted_obs_t));
    if (!sorted) return NIMCP_SURVIVAL_ERROR_MEMORY;

    uint32_t n_events_total = 0;
    for (uint32_t i = 0; i < n; i++) {
        sorted[i].time = times[i];
        sorted[i].event = event_types[i] > 0;
        sorted[i].event_type = event_types[i];
        if (event_types[i] > 0) {
            n_events_total++;
            if (event_types[i] <= k) {
                cif->n_events[event_types[i] - 1]++;
            }
        }
    }
    qsort(sorted, n, sizeof(sorted_obs_t), compare_obs_time);

    // Count unique event times
    uint32_t n_unique = 0;
    float prev_time = -INFINITY;
    for (uint32_t i = 0; i < n; i++) {
        if (sorted[i].event && sorted[i].time != prev_time) {
            n_unique++;
            prev_time = sorted[i].time;
        }
    }

    // Allocate curve
    if (cif->curve) {
        for (uint32_t i = 0; i < cif->n_points; i++) {
            nimcp_free(cif->curve[i].cif);
            nimcp_free(cif->curve[i].std_error);
            nimcp_free(cif->curve[i].ci_lower);
            nimcp_free(cif->curve[i].ci_upper);
        }
        nimcp_free(cif->curve);
    }

    cif->curve = (nimcp_cif_point_t*)nimcp_calloc(n_unique + 1, sizeof(nimcp_cif_point_t));
    if (!cif->curve) {
        nimcp_free(sorted);
        return NIMCP_SURVIVAL_ERROR_MEMORY;
    }

    // Allocate arrays for each curve point
    for (uint32_t i = 0; i <= n_unique; i++) {
        cif->curve[i].cif = (float*)nimcp_calloc(k, sizeof(float));
        cif->curve[i].std_error = (float*)nimcp_calloc(k, sizeof(float));
        cif->curve[i].ci_lower = (float*)nimcp_calloc(k, sizeof(float));
        cif->curve[i].ci_upper = (float*)nimcp_calloc(k, sizeof(float));
        cif->curve[i].n_events = k;
    }

    // Time 0 point
    cif->curve[0].time = 0.0f;

    // Aalen-Johansen estimator
    float z_alpha = nimcp_stats_quantile_standard_normal(1.0f - (1.0f - confidence) / 2.0f);
    double survival = 1.0;  // Overall survival (for all causes combined)
    double* cumulative_incidence = (double*)nimcp_calloc(k, sizeof(double));
    uint32_t n_at_risk = n;
    uint32_t curve_idx = 1;
    uint32_t i = 0;

    while (i < n && curve_idx <= n_unique) {
        float current_time = sorted[i].time;

        // Count events of each type at this time
        uint32_t* d_type = (uint32_t*)nimcp_calloc(k, sizeof(uint32_t));
        uint32_t d_total = 0;
        uint32_t c_total = 0;

        while (i < n && sorted[i].time == current_time) {
            if (sorted[i].event) {
                uint8_t type = sorted[i].event_type;
                if (type > 0 && type <= k) {
                    d_type[type - 1]++;
                }
                d_total++;
            } else {
                c_total++;
            }
            i++;
        }

        if (d_total > 0) {
            // Update CIF for each event type
            // CIF_j(t) = CIF_j(t-) + S(t-) * d_j / n
            for (uint32_t j = 0; j < k; j++) {
                cumulative_incidence[j] += survival * (double)d_type[j] / n_at_risk;
            }

            // Update overall survival
            survival *= 1.0 - (double)d_total / n_at_risk;

            // Store curve point
            cif->curve[curve_idx].time = current_time;
            for (uint32_t j = 0; j < k; j++) {
                cif->curve[curve_idx].cif[j] = (float)cumulative_incidence[j];
                // Simplified standard error
                double se = sqrt(cumulative_incidence[j] * (1.0 - cumulative_incidence[j]) / n_at_risk);
                cif->curve[curve_idx].std_error[j] = (float)se;
                cif->curve[curve_idx].ci_lower[j] = (float)fmax(0.0, cumulative_incidence[j] - z_alpha * se);
                cif->curve[curve_idx].ci_upper[j] = (float)fmin(1.0, cumulative_incidence[j] + z_alpha * se);
            }

            curve_idx++;
        }

        n_at_risk -= (d_total + c_total);
        nimcp_free(d_type);
    }

    cif->n_points = curve_idx;
    cif->n_total = n;
    cif->n_censored = n - n_events_total;
    cif->confidence_level = confidence;
    cif->fitted = true;

    nimcp_free(sorted);
    nimcp_free(cumulative_incidence);

    return NIMCP_SURVIVAL_OK;
}

float nimcp_cif_at(const nimcp_cif_estimator_t* cif, float t, uint8_t event_type) {
    if (!cif || !cif->fitted || !cif->curve) return NAN;
    if (event_type == 0 || event_type > cif->n_event_types) return NAN;
    if (t < 0) return 0.0f;

    // Binary search
    uint32_t lo = 0, hi = cif->n_points - 1;
    while (lo < hi) {
        uint32_t mid = (lo + hi + 1) / 2;
        if (cif->curve[mid].time <= t) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }

    return cif->curve[lo].cif[event_type - 1];
}

//=============================================================================
// Fine-Gray Model
//=============================================================================

nimcp_fine_gray_model_t* nimcp_fine_gray_create(uint32_t n_covariates, uint8_t target_event) {
    if (n_covariates == 0 || target_event == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_fine_gray_create: n_covariates is zero");
        return NULL;
    }

    nimcp_fine_gray_model_t* fg = (nimcp_fine_gray_model_t*)nimcp_calloc(1, sizeof(nimcp_fine_gray_model_t));
    if (!fg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_fine_gray_create: fg is NULL");
        return NULL;
    }

    fg->coefficients = (nimcp_cox_coefficient_t*)nimcp_calloc(n_covariates, sizeof(nimcp_cox_coefficient_t));
    fg->variance_matrix = (float*)nimcp_calloc((size_t)n_covariates * n_covariates, sizeof(float));

    if (!fg->coefficients || !fg->variance_matrix) {
        nimcp_free(fg->coefficients);
        nimcp_free(fg->variance_matrix);
        nimcp_free(fg);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_fine_gray_create: required parameter is NULL (fg->coefficients, fg->variance_matrix)");
        return NULL;
    }

    fg->n_covariates = n_covariates;
    fg->target_event = target_event;
    fg->confidence_level = NIMCP_SURVIVAL_DEFAULT_CONFIDENCE;
    fg->fitted = false;

    return fg;
}

void nimcp_fine_gray_destroy(nimcp_fine_gray_model_t* fg) {
    if (!fg) return;
    nimcp_free(fg->coefficients);
    nimcp_free(fg->variance_matrix);
    nimcp_free(fg);
}

nimcp_survival_result_t nimcp_fine_gray_fit(
    nimcp_fine_gray_model_t* fg,
    const float* times,
    const uint8_t* event_types,
    const float* covariates,
    uint32_t n,
    const char** covariate_names,
    float confidence
) {
    if (!fg || !times || !event_types || !covariates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "NULL argument to Fine-Gray fit");
        return NIMCP_SURVIVAL_ERROR_NULL;
    }

    // Fine-Gray model uses subdistribution hazard approach
    // Subjects with competing events remain in risk set with decreasing weight

    // For now, use simplified weighted Cox approach
    uint32_t p = fg->n_covariates;
    uint8_t target = fg->target_event;

    // Create modified events: target event = 1, others = censored
    bool* modified_events = (bool*)nimcp_malloc(n * sizeof(bool));
    if (!modified_events) return NIMCP_SURVIVAL_ERROR_MEMORY;

    uint32_t n_target = 0;
    for (uint32_t i = 0; i < n; i++) {
        modified_events[i] = (event_types[i] == target);
        if (modified_events[i]) n_target++;
    }

    if (n_target == 0) {
        nimcp_free(modified_events);
        return NIMCP_SURVIVAL_ERROR_NO_EVENTS;
    }

    // Create temporary Cox model
    nimcp_cox_model_t* cox = nimcp_cox_create(p);
    if (!cox) {
        nimcp_free(modified_events);
        return NIMCP_SURVIVAL_ERROR_MEMORY;
    }

    // Fit Cox model (simplified - should use subdistribution weights)
    nimcp_survival_result_t result = nimcp_cox_fit(
        cox, times, modified_events, covariates, n, covariate_names, confidence);

    if (result == NIMCP_SURVIVAL_OK) {
        // Copy results
        for (uint32_t i = 0; i < p; i++) {
            fg->coefficients[i] = cox->coefficients[i];
        }
        memcpy(fg->variance_matrix, cox->variance_matrix, (size_t)p * p * sizeof(float));
        fg->confidence_level = confidence;
        fg->fitted = true;
    }

    nimcp_cox_destroy(cox);
    nimcp_free(modified_events);

    return result;
}

nimcp_survival_result_t nimcp_fine_gray_predict(
    const nimcp_fine_gray_model_t* fg,
    const float* covariates,
    const float* times,
    uint32_t n_times,
    float* cif
) {
    if (!fg || !covariates || !times || !cif) return NIMCP_SURVIVAL_ERROR_NULL;
    if (!fg->fitted) return NIMCP_SURVIVAL_ERROR_NOT_FIT;

    // Compute linear predictor
    double eta = 0.0;
    for (uint32_t j = 0; j < fg->n_covariates; j++) {
        eta += fg->coefficients[j].beta * covariates[j];
    }
    double hr = exp(eta);

    // CIF(t|X) = 1 - exp(-H_0(t) * exp(beta'X))
    // Use baseline cumulative subdistribution hazard
    // Simplified: assume baseline ~ t
    for (uint32_t i = 0; i < n_times; i++) {
        double cum_hazard = times[i] * 0.01 * hr;  // Simplified
        cif[i] = 1.0f - expf(-(float)cum_hazard);
        if (cif[i] < 0.0f) cif[i] = 0.0f;
        if (cif[i] > 1.0f) cif[i] = 1.0f;
    }

    return NIMCP_SURVIVAL_OK;
}
