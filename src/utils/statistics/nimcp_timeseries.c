/**
 * @file nimcp_timeseries.c
 * @brief Time Series Analysis Implementation
 *
 * WHAT: Complete time series analysis toolkit implementation
 * WHY:  Temporal structure analysis for neural computation
 * HOW:  Numerically stable algorithms with optional GPU acceleration
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include "utils/statistics/nimcp_timeseries.h"
#include "utils/statistics/nimcp_statistics.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/memory/nimcp_memory.h"
#include "constants/nimcp_math_constants.h"

//=============================================================================
// MODULE IDENTIFICATION
//=============================================================================

#define LOG_MODULE_NAME "timeseries"

//=============================================================================
// CONSTANTS
//=============================================================================

#define PI NIMCP_PI

//=============================================================================
// Global State
//=============================================================================

static bool g_ts_initialized = false;
static nimcp_ts_config_t g_ts_config;

//=============================================================================
// Module Initialization
//=============================================================================

nimcp_ts_config_t nimcp_ts_default_config(void) {
    nimcp_ts_config_t config = {
        .use_gpu = false,
        .handle_nan = true,
        .default_nfft = NIMCP_TS_DEFAULT_NFFT,
        .default_window = NIMCP_TS_WINDOW_HANN,
        .default_detrend = NIMCP_TS_DETREND_MEAN,
        .confidence_level = 0.95f,
        .max_iterations = NIMCP_TS_MAX_ITERATIONS,
        .convergence_tol = 1e-8f
    };
    return config;
}

nimcp_ts_result_t nimcp_ts_init(const nimcp_ts_config_t* config) {
    if (config) {
        g_ts_config = *config;
    } else {
        g_ts_config = nimcp_ts_default_config();
    }
    g_ts_initialized = true;
    return NIMCP_TS_OK;
}

void nimcp_ts_shutdown(void) {
    g_ts_initialized = false;
}

bool nimcp_ts_is_initialized(void) {
    return g_ts_initialized;
}

const char* nimcp_ts_error_string(nimcp_ts_result_t result) {
    switch (result) {
        case NIMCP_TS_OK: return "Success";
        case NIMCP_TS_ERROR_NULL: return "NULL pointer argument";
        case NIMCP_TS_ERROR_SIZE: return "Invalid size";
        case NIMCP_TS_ERROR_MEMORY: return "Memory allocation failed";
        case NIMCP_TS_ERROR_PARAMS: return "Invalid parameters";
        case NIMCP_TS_ERROR_CONVERGE: return "Algorithm did not converge";
        case NIMCP_TS_ERROR_SINGULAR: return "Singular matrix";
        case NIMCP_TS_ERROR_NON_STATIONARY: return "Series is non-stationary";
        case NIMCP_TS_ERROR_GPU: return "GPU operation failed";
        case NIMCP_TS_ERROR_FFT: return "FFT operation failed";
        case NIMCP_TS_ERROR_NOT_INIT: return "Module not initialized";
        default: return "Unknown error";
    }
}

//=============================================================================
// Internal Helpers
//=============================================================================

static inline bool is_nan_f(float x) {
    return x != x;
}

static inline float safe_mean(const float* x, uint32_t n) {
    if (!x || n == 0) return NAN;
    double sum = 0.0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (!is_nan_f(x[i])) {
            sum += x[i];
            count++;
        }
    }
    return count > 0 ? (float)(sum / count) : NAN;
}

static inline float safe_var(const float* x, uint32_t n) {
    if (!x || n < 2) return NAN;
    double mean = 0.0, m2 = 0.0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (!is_nan_f(x[i])) {
            count++;
            double delta = x[i] - mean;
            mean += delta / count;
            double delta2 = x[i] - mean;
            m2 += delta * delta2;
        }
    }
    return count > 1 ? (float)(m2 / (count - 1)) : NAN;
}

// Cooley-Tukey radix-2 FFT (CPU fallback)
static void fft_recursive(float* real, float* imag, uint32_t n, int sign) {
    if (n <= 1) return;

    float* even_r = (float*)nimcp_malloc(n/2 * sizeof(float));
    float* even_i = (float*)nimcp_malloc(n/2 * sizeof(float));
    float* odd_r = (float*)nimcp_malloc(n/2 * sizeof(float));
    float* odd_i = (float*)nimcp_malloc(n/2 * sizeof(float));

    if (!even_r || !even_i || !odd_r || !odd_i) {
        nimcp_free(even_r); nimcp_free(even_i); nimcp_free(odd_r); nimcp_free(odd_i);
        return;
    }

    for (uint32_t i = 0; i < n/2; i++) {
        even_r[i] = real[2*i];
        even_i[i] = imag[2*i];
        odd_r[i] = real[2*i + 1];
        odd_i[i] = imag[2*i + 1];
    }

    fft_recursive(even_r, even_i, n/2, sign);
    fft_recursive(odd_r, odd_i, n/2, sign);

    for (uint32_t k = 0; k < n/2; k++) {
        float theta = sign * NIMCP_TWO_PI_F * k / n;
        float cos_t = cosf(theta);
        float sin_t = sinf(theta);

        float t_r = cos_t * odd_r[k] - sin_t * odd_i[k];
        float t_i = sin_t * odd_r[k] + cos_t * odd_i[k];

        real[k] = even_r[k] + t_r;
        imag[k] = even_i[k] + t_i;
        real[k + n/2] = even_r[k] - t_r;
        imag[k + n/2] = even_i[k] - t_i;
    }

    nimcp_free(even_r); nimcp_free(even_i); nimcp_free(odd_r); nimcp_free(odd_i);
}

static uint32_t next_power_of_2(uint32_t n) {
    uint32_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

static nimcp_ts_result_t compute_fft_real(const float* x, uint32_t n, uint32_t nfft,
                                          float* out_real, float* out_imag) {
    if (!x || !out_real || !out_imag) return NIMCP_TS_ERROR_NULL;

    float* real = (float*)nimcp_calloc(nfft, sizeof(float));
    float* imag = (float*)nimcp_calloc(nfft, sizeof(float));
    if (!real || !imag) {
        nimcp_free(real); nimcp_free(imag);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "FFT allocation failed");
        return NIMCP_TS_ERROR_MEMORY;
    }

    memcpy(real, x, n * sizeof(float));

    fft_recursive(real, imag, nfft, -1);

    memcpy(out_real, real, nfft * sizeof(float));
    memcpy(out_imag, imag, nfft * sizeof(float));

    nimcp_free(real);
    nimcp_free(imag);
    return NIMCP_TS_OK;
}

//=============================================================================
// Autocorrelation Analysis
//=============================================================================

float nimcp_ts_autocorrelation(const float* x, uint32_t n, uint32_t lag) {
    if (!x || n == 0 || lag >= n) return NAN;

    float mean = safe_mean(x, n);
    if (is_nan_f(mean)) return NAN;

    double num = 0.0, denom = 0.0;
    for (uint32_t t = 0; t < n; t++) {
        if (!is_nan_f(x[t])) {
            double dev = x[t] - mean;
            denom += dev * dev;
            if (t + lag < n && !is_nan_f(x[t + lag])) {
                num += dev * (x[t + lag] - mean);
            }
        }
    }

    return denom > NIMCP_TS_EPSILON ? (float)(num / denom) : 0.0f;
}

nimcp_ts_result_t nimcp_ts_acf(
    const float* x,
    uint32_t n,
    uint32_t max_lag,
    float confidence_level,
    nimcp_acf_result_t* result)
{
    if (!x || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL argument in nimcp_ts_acf");
        return NIMCP_TS_ERROR_NULL;
    }
    if (n < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "Series too short for ACF");
        return NIMCP_TS_ERROR_SIZE;
    }

    if (max_lag == 0 || max_lag >= n) max_lag = n - 1;

    result->acf = (float*)nimcp_malloc((max_lag + 1) * sizeof(float));
    result->confidence_upper = (float*)nimcp_malloc((max_lag + 1) * sizeof(float));
    result->confidence_lower = (float*)nimcp_malloc((max_lag + 1) * sizeof(float));

    if (!result->acf || !result->confidence_upper || !result->confidence_lower) {
        nimcp_free(result->acf); nimcp_free(result->confidence_upper); nimcp_free(result->confidence_lower);
        result->acf = result->confidence_upper = result->confidence_lower = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ACF allocation failed");
        return NIMCP_TS_ERROR_MEMORY;
    }

    result->max_lag = max_lag;
    result->n = n;
    result->confidence_level = confidence_level;

    float mean = safe_mean(x, n);
    double var_sum = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        if (!is_nan_f(x[i])) {
            double d = x[i] - mean;
            var_sum += d * d;
        }
    }

    for (uint32_t lag = 0; lag <= max_lag; lag++) {
        double cov = 0.0;
        for (uint32_t t = 0; t < n - lag; t++) {
            if (!is_nan_f(x[t]) && !is_nan_f(x[t + lag])) {
                cov += (x[t] - mean) * (x[t + lag] - mean);
            }
        }
        result->acf[lag] = var_sum > NIMCP_TS_EPSILON ? (float)(cov / var_sum) : 0.0f;
    }

    float z = nimcp_stats_quantile_standard_normal(0.5f + confidence_level / 2.0f);

    for (uint32_t lag = 0; lag <= max_lag; lag++) {
        double var_sum_acf = 1.0;
        for (uint32_t i = 1; i < lag; i++) {
            var_sum_acf += 2.0 * result->acf[i] * result->acf[i];
        }
        float ci = z * sqrtf((float)var_sum_acf / n);
        result->confidence_upper[lag] = ci;
        result->confidence_lower[lag] = -ci;
    }

    return NIMCP_TS_OK;
}

nimcp_ts_result_t nimcp_ts_pacf(
    const float* x,
    uint32_t n,
    uint32_t max_lag,
    float confidence_level,
    nimcp_pacf_result_t* result)
{
    if (!x || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL argument in nimcp_ts_pacf");
        return NIMCP_TS_ERROR_NULL;
    }
    if (n < 2) return NIMCP_TS_ERROR_SIZE;
    if (max_lag == 0 || max_lag >= n) max_lag = n / 2;

    result->pacf = (float*)nimcp_malloc((max_lag + 1) * sizeof(float));
    result->confidence_upper = (float*)nimcp_malloc((max_lag + 1) * sizeof(float));
    result->confidence_lower = (float*)nimcp_malloc((max_lag + 1) * sizeof(float));

    if (!result->pacf || !result->confidence_upper || !result->confidence_lower) {
        nimcp_free(result->pacf); nimcp_free(result->confidence_upper); nimcp_free(result->confidence_lower);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "PACF allocation failed");
        return NIMCP_TS_ERROR_MEMORY;
    }

    result->max_lag = max_lag;
    result->n = n;
    result->confidence_level = confidence_level;

    float* acf = (float*)nimcp_malloc((max_lag + 1) * sizeof(float));
    if (!acf) {
        nimcp_free(result->pacf); nimcp_free(result->confidence_upper); nimcp_free(result->confidence_lower);
        return NIMCP_TS_ERROR_MEMORY;
    }

    for (uint32_t k = 0; k <= max_lag; k++) {
        acf[k] = nimcp_ts_autocorrelation(x, n, k);
    }

    result->pacf[0] = 1.0f;
    if (max_lag >= 1) result->pacf[1] = acf[1];

    float* phi = (float*)nimcp_calloc(max_lag + 1, sizeof(float));
    float* phi_new = (float*)nimcp_calloc(max_lag + 1, sizeof(float));
    if (!phi || !phi_new) {
        nimcp_free(acf); nimcp_free(phi); nimcp_free(phi_new);
        return NIMCP_TS_ERROR_MEMORY;
    }

    phi[1] = acf[1];

    for (uint32_t k = 2; k <= max_lag; k++) {
        double num = acf[k];
        double denom = 1.0;
        for (uint32_t j = 1; j < k; j++) {
            num -= phi[j] * acf[k - j];
            denom -= phi[j] * acf[j];
        }

        if (fabsf((float)denom) < NIMCP_TS_EPSILON) {
            result->pacf[k] = 0.0f;
        } else {
            phi_new[k] = (float)(num / denom);
            result->pacf[k] = phi_new[k];
            for (uint32_t j = 1; j < k; j++) {
                phi_new[j] = phi[j] - phi_new[k] * phi[k - j];
            }
            memcpy(phi, phi_new, (k + 1) * sizeof(float));
        }
    }

    nimcp_free(acf); nimcp_free(phi); nimcp_free(phi_new);

    float z = nimcp_stats_quantile_standard_normal(0.5f + confidence_level / 2.0f);
    float ci = z / sqrtf((float)n);
    for (uint32_t k = 0; k <= max_lag; k++) {
        result->confidence_upper[k] = ci;
        result->confidence_lower[k] = -ci;
    }

    return NIMCP_TS_OK;
}

nimcp_ts_result_t nimcp_ts_ljung_box(
    const float* x, uint32_t n, uint32_t lags,
    nimcp_ljung_box_result_t* result)
{
    if (!x || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL in Ljung-Box");
        return NIMCP_TS_ERROR_NULL;
    }
    if (n < lags + 1) return NIMCP_TS_ERROR_SIZE;

    double Q = 0.0;
    for (uint32_t k = 1; k <= lags; k++) {
        float r_k = nimcp_ts_autocorrelation(x, n, k);
        Q += (r_k * r_k) / (n - k);
    }
    Q *= n * (n + 2);

    result->statistic = (float)Q;
    result->lags = lags;
    result->df = (float)lags;
    result->p_value = 1.0f - nimcp_stats_cdf_chi_squared((float)Q, (float)lags);
    result->significant = (result->p_value < 0.05f);

    return NIMCP_TS_OK;
}

float nimcp_ts_durbin_watson(const float* residuals, uint32_t n) {
    if (!residuals || n < 2) return NAN;

    double num = 0.0, denom = 0.0;
    for (uint32_t t = 0; t < n; t++) {
        if (!is_nan_f(residuals[t])) {
            denom += residuals[t] * residuals[t];
            if (t > 0 && !is_nan_f(residuals[t - 1])) {
                double diff = residuals[t] - residuals[t - 1];
                num += diff * diff;
            }
        }
    }

    return denom > NIMCP_TS_EPSILON ? (float)(num / denom) : NAN;
}

//=============================================================================
// Window Functions
//=============================================================================

nimcp_ts_result_t nimcp_ts_window_function(
    uint32_t n, nimcp_ts_window_t window_type, float param, float* out)
{
    if (!out || n == 0) return NIMCP_TS_ERROR_NULL;

    for (uint32_t i = 0; i < n; i++) {
        switch (window_type) {
            case NIMCP_TS_WINDOW_RECTANGULAR:
                out[i] = 1.0f;
                break;
            case NIMCP_TS_WINDOW_HANN:
                out[i] = 0.5f * (1.0f - cosf(NIMCP_TWO_PI_F * i / (n - 1)));
                break;
            case NIMCP_TS_WINDOW_HAMMING:
                out[i] = 0.54f - 0.46f * cosf(NIMCP_TWO_PI_F * i / (n - 1));
                break;
            case NIMCP_TS_WINDOW_BLACKMAN:
                out[i] = 0.42f - 0.5f * cosf(NIMCP_TWO_PI_F * i / (n - 1))
                       + 0.08f * cosf(2 * NIMCP_TWO_PI_F * i / (n - 1));
                break;
            case NIMCP_TS_WINDOW_BLACKMAN_HARRIS:
                out[i] = 0.35875f - 0.48829f * cosf(NIMCP_TWO_PI_F * i / (n - 1))
                       + 0.14128f * cosf(2 * NIMCP_TWO_PI_F * i / (n - 1))
                       - 0.01168f * cosf(3 * NIMCP_TWO_PI_F * i / (n - 1));
                break;
            case NIMCP_TS_WINDOW_GAUSSIAN: {
                float sigma = param > 0 ? param : 0.4f;
                float center = (n - 1) / 2.0f;
                float t = (i - center) / (sigma * center);
                out[i] = expf(-0.5f * t * t);
                break;
            }
            default:
                out[i] = 1.0f;
        }
    }
    return NIMCP_TS_OK;
}

//=============================================================================
// Spectral Analysis
//=============================================================================

nimcp_ts_result_t nimcp_ts_periodogram(
    const float* x, uint32_t n, float fs, uint32_t nfft,
    nimcp_ts_window_t window, nimcp_ts_detrend_t detrend,
    nimcp_psd_result_t* result)
{
    if (!x || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL in periodogram");
        return NIMCP_TS_ERROR_NULL;
    }
    if (n == 0) return NIMCP_TS_ERROR_SIZE;
    if (nfft == 0) nfft = next_power_of_2(n);

    uint32_t n_freqs = nfft / 2 + 1;

    result->frequencies = (float*)nimcp_malloc(n_freqs * sizeof(float));
    result->power = (float*)nimcp_malloc(n_freqs * sizeof(float));
    result->confidence_lower = NULL;
    result->confidence_upper = NULL;

    if (!result->frequencies || !result->power) {
        nimcp_free(result->frequencies); nimcp_free(result->power);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Periodogram alloc failed");
        return NIMCP_TS_ERROR_MEMORY;
    }

    result->n_freqs = n_freqs;
    result->fs = fs;
    result->df = fs / nfft;

    for (uint32_t i = 0; i < n_freqs; i++) {
        result->frequencies[i] = i * result->df;
    }

    float* data = (float*)nimcp_malloc(n * sizeof(float));
    float* win = (float*)nimcp_malloc(n * sizeof(float));
    if (!data || !win) {
        nimcp_free(data); nimcp_free(win);
        nimcp_free(result->frequencies); nimcp_free(result->power);
        return NIMCP_TS_ERROR_MEMORY;
    }

    memcpy(data, x, n * sizeof(float));

    if (detrend == NIMCP_TS_DETREND_MEAN) {
        float m = safe_mean(data, n);
        for (uint32_t i = 0; i < n; i++) data[i] -= m;
    }

    nimcp_ts_window_function(n, window, 0, win);
    double win_sum_sq = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        data[i] *= win[i];
        win_sum_sq += win[i] * win[i];
    }

    float* fft_real_arr = (float*)nimcp_calloc(nfft, sizeof(float));
    float* fft_imag = (float*)nimcp_calloc(nfft, sizeof(float));
    if (!fft_real_arr || !fft_imag) {
        nimcp_free(data); nimcp_free(win); nimcp_free(fft_real_arr); nimcp_free(fft_imag);
        nimcp_free(result->frequencies); nimcp_free(result->power);
        return NIMCP_TS_ERROR_MEMORY;
    }

    nimcp_ts_result_t fft_result = compute_fft_real(data, n, nfft, fft_real_arr, fft_imag);
    if (fft_result != NIMCP_TS_OK) {
        nimcp_free(data); nimcp_free(win); nimcp_free(fft_real_arr); nimcp_free(fft_imag);
        nimcp_free(result->frequencies); nimcp_free(result->power);
        return fft_result;
    }

    double scale = 1.0 / (fs * win_sum_sq);
    result->total_power = 0.0f;

    for (uint32_t i = 0; i < n_freqs; i++) {
        double pwr = fft_real_arr[i] * fft_real_arr[i] + fft_imag[i] * fft_imag[i];
        pwr *= scale;
        if (i > 0 && i < n_freqs - 1) pwr *= 2.0;
        result->power[i] = (float)pwr;
        result->total_power += (float)pwr;
    }

    nimcp_free(data); nimcp_free(win); nimcp_free(fft_real_arr); nimcp_free(fft_imag);
    return NIMCP_TS_OK;
}

nimcp_ts_result_t nimcp_ts_welch_psd(
    const float* x, uint32_t n, float fs, uint32_t segment_length,
    float overlap, uint32_t nfft, nimcp_ts_window_t window,
    nimcp_ts_detrend_t detrend, nimcp_psd_result_t* result)
{
    if (!x || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL in Welch PSD");
        return NIMCP_TS_ERROR_NULL;
    }

    if (segment_length == 0) segment_length = n / 8;
    if (segment_length > n) segment_length = n;
    if (nfft == 0) nfft = next_power_of_2(segment_length);
    if (overlap < 0.0f) overlap = 0.0f;
    if (overlap >= 1.0f) overlap = 0.5f;

    uint32_t step = (uint32_t)(segment_length * (1.0f - overlap));
    if (step == 0) step = 1;
    uint32_t n_segments = (n - segment_length) / step + 1;
    if (n_segments == 0) n_segments = 1;

    uint32_t n_freqs = nfft / 2 + 1;

    result->frequencies = (float*)nimcp_malloc(n_freqs * sizeof(float));
    result->power = (float*)nimcp_calloc(n_freqs, sizeof(float));
    result->confidence_lower = NULL;
    result->confidence_upper = NULL;

    if (!result->frequencies || !result->power) {
        nimcp_free(result->frequencies); nimcp_free(result->power);
        return NIMCP_TS_ERROR_MEMORY;
    }

    result->n_freqs = n_freqs;
    result->fs = fs;
    result->df = fs / nfft;

    for (uint32_t i = 0; i < n_freqs; i++) {
        result->frequencies[i] = i * result->df;
    }

    float* win = (float*)nimcp_malloc(segment_length * sizeof(float));
    if (!win) {
        nimcp_free(result->frequencies); nimcp_free(result->power);
        return NIMCP_TS_ERROR_MEMORY;
    }
    nimcp_ts_window_function(segment_length, window, 0, win);

    double win_sum_sq = 0.0;
    for (uint32_t i = 0; i < segment_length; i++) {
        win_sum_sq += win[i] * win[i];
    }

    float* seg = (float*)nimcp_malloc(segment_length * sizeof(float));
    float* fft_r = (float*)nimcp_calloc(nfft, sizeof(float));
    float* fft_i = (float*)nimcp_calloc(nfft, sizeof(float));

    if (!seg || !fft_r || !fft_i) {
        nimcp_free(win); nimcp_free(seg); nimcp_free(fft_r); nimcp_free(fft_i);
        nimcp_free(result->frequencies); nimcp_free(result->power);
        return NIMCP_TS_ERROR_MEMORY;
    }

    for (uint32_t s = 0; s < n_segments; s++) {
        uint32_t start = s * step;
        memcpy(seg, x + start, segment_length * sizeof(float));

        if (detrend == NIMCP_TS_DETREND_MEAN) {
            float m = safe_mean(seg, segment_length);
            for (uint32_t i = 0; i < segment_length; i++) seg[i] -= m;
        }

        for (uint32_t i = 0; i < segment_length; i++) {
            seg[i] *= win[i];
        }

        memset(fft_r, 0, nfft * sizeof(float));
        memset(fft_i, 0, nfft * sizeof(float));
        compute_fft_real(seg, segment_length, nfft, fft_r, fft_i);

        for (uint32_t i = 0; i < n_freqs; i++) {
            double pwr = fft_r[i] * fft_r[i] + fft_i[i] * fft_i[i];
            result->power[i] += (float)pwr;
        }
    }

    double scale = 1.0 / (fs * win_sum_sq * n_segments);
    result->total_power = 0.0f;
    for (uint32_t i = 0; i < n_freqs; i++) {
        result->power[i] *= (float)scale;
        if (i > 0 && i < n_freqs - 1) result->power[i] *= 2.0f;
        result->total_power += result->power[i];
    }

    nimcp_free(win); nimcp_free(seg); nimcp_free(fft_r); nimcp_free(fft_i);
    return NIMCP_TS_OK;
}

float nimcp_ts_spectral_entropy(
    const float* x, uint32_t n, float fs, uint32_t nfft, bool normalize)
{
    if (!x || n == 0) return NAN;

    nimcp_psd_result_t psd = {0};
    if (nimcp_ts_periodogram(x, n, fs, nfft, NIMCP_TS_WINDOW_HANN,
                             NIMCP_TS_DETREND_MEAN, &psd) != NIMCP_TS_OK) {
        return NAN;
    }

    double total = 0.0;
    for (uint32_t i = 0; i < psd.n_freqs; i++) {
        total += psd.power[i];
    }

    if (total < NIMCP_TS_EPSILON) {
        nimcp_psd_free(&psd);
        return 0.0f;
    }

    double entropy = 0.0;
    for (uint32_t i = 0; i < psd.n_freqs; i++) {
        double p = psd.power[i] / total;
        if (p > NIMCP_TS_EPSILON) {
            entropy -= p * log2(p);
        }
    }

    if (normalize && psd.n_freqs > 1) {
        entropy /= log2((double)psd.n_freqs);
    }

    nimcp_psd_free(&psd);
    return (float)entropy;
}

void nimcp_psd_free(nimcp_psd_result_t* result) {
    if (result) {
        nimcp_free(result->frequencies);
        nimcp_free(result->power);
        nimcp_free(result->confidence_lower);
        nimcp_free(result->confidence_upper);
        memset(result, 0, sizeof(nimcp_psd_result_t));
    }
}

void nimcp_coherence_free(nimcp_coherence_result_t* result) {
    if (result) {
        nimcp_free(result->frequencies);
        nimcp_free(result->coherence);
        nimcp_free(result->phase);
        nimcp_free(result->confidence);
        memset(result, 0, sizeof(nimcp_coherence_result_t));
    }
}

void nimcp_cross_spectrum_free(nimcp_cross_spectrum_result_t* result) {
    if (result) {
        nimcp_free(result->frequencies);
        nimcp_free(result->csd_real);
        nimcp_free(result->csd_imag);
        nimcp_free(result->magnitude);
        nimcp_free(result->phase);
        memset(result, 0, sizeof(nimcp_cross_spectrum_result_t));
    }
}

void nimcp_acf_free(nimcp_acf_result_t* result) {
    if (result) {
        nimcp_free(result->acf);
        nimcp_free(result->confidence_upper);
        nimcp_free(result->confidence_lower);
        memset(result, 0, sizeof(nimcp_acf_result_t));
    }
}

void nimcp_pacf_free(nimcp_pacf_result_t* result) {
    if (result) {
        nimcp_free(result->pacf);
        nimcp_free(result->confidence_upper);
        nimcp_free(result->confidence_lower);
        memset(result, 0, sizeof(nimcp_pacf_result_t));
    }
}

//=============================================================================
// Smoothing and Filtering
//=============================================================================

nimcp_ts_result_t nimcp_ts_moving_average(
    const float* x, uint32_t n, uint32_t window_size, bool centered, float* out)
{
    if (!x || !out) return NIMCP_TS_ERROR_NULL;
    if (n == 0 || window_size == 0) return NIMCP_TS_ERROR_SIZE;
    if (window_size > n) window_size = n;

    if (centered) {
        uint32_t half = window_size / 2;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t start = (i >= half) ? i - half : 0;
            uint32_t end = (i + half < n) ? i + half : n - 1;
            double sum = 0.0;
            uint32_t count = 0;
            for (uint32_t j = start; j <= end; j++) {
                if (!is_nan_f(x[j])) {
                    sum += x[j];
                    count++;
                }
            }
            out[i] = count > 0 ? (float)(sum / count) : NAN;
        }
    } else {
        double sum = 0.0;
        uint32_t count = 0;

        for (uint32_t i = 0; i < n; i++) {
            if (!is_nan_f(x[i])) {
                sum += x[i];
                count++;
            }
            if (i >= window_size) {
                if (!is_nan_f(x[i - window_size])) {
                    sum -= x[i - window_size];
                    count--;
                }
            }
            out[i] = count > 0 ? (float)(sum / count) : NAN;
        }
    }
    return NIMCP_TS_OK;
}

nimcp_ts_result_t nimcp_ts_exponential_smooth(
    const float* x, uint32_t n, float alpha, float* out)
{
    if (!x || !out) return NIMCP_TS_ERROR_NULL;
    if (n == 0) return NIMCP_TS_ERROR_SIZE;
    if (alpha <= 0.0f || alpha > 1.0f) return NIMCP_TS_ERROR_PARAMS;

    uint32_t start = 0;
    while (start < n && is_nan_f(x[start])) start++;
    if (start >= n) {
        for (uint32_t i = 0; i < n; i++) out[i] = NAN;
        return NIMCP_TS_OK;
    }

    for (uint32_t i = 0; i < start; i++) out[i] = NAN;
    out[start] = x[start];

    for (uint32_t t = start + 1; t < n; t++) {
        if (is_nan_f(x[t])) {
            out[t] = out[t - 1];
        } else {
            out[t] = alpha * x[t] + (1.0f - alpha) * out[t - 1];
        }
    }
    return NIMCP_TS_OK;
}

nimcp_ts_result_t nimcp_ts_savitzky_golay(
    const float* x, uint32_t n, uint32_t window_size,
    uint32_t poly_order, uint32_t deriv, float* out)
{
    if (!x || !out) return NIMCP_TS_ERROR_NULL;
    if (n == 0) return NIMCP_TS_ERROR_SIZE;
    if (window_size % 2 == 0) window_size++;
    if (poly_order >= window_size) return NIMCP_TS_ERROR_PARAMS;

    int half = (int)window_size / 2;

    // Simplified: use uniform weights
    float weight = 1.0f / window_size;

    for (uint32_t i = 0; i < n; i++) {
        double sum = 0.0;
        double weight_sum = 0.0;

        for (int j = -half; j <= half; j++) {
            int idx = (int)i + j;
            if (idx >= 0 && idx < (int)n && !is_nan_f(x[idx])) {
                sum += weight * x[idx];
                weight_sum += weight;
            }
        }
        out[i] = weight_sum > 0 ? (float)(sum / weight_sum) : (is_nan_f(x[i]) ? NAN : x[i]);
    }
    return NIMCP_TS_OK;
}

//=============================================================================
// Utility Functions
//=============================================================================

nimcp_ts_result_t nimcp_ts_difference(const float* x, uint32_t n, uint32_t d, float* out) {
    if (!x || !out) return NIMCP_TS_ERROR_NULL;
    if (n <= d) return NIMCP_TS_ERROR_SIZE;

    float* temp = (float*)nimcp_malloc(n * sizeof(float));
    if (!temp) return NIMCP_TS_ERROR_MEMORY;

    memcpy(temp, x, n * sizeof(float));

    for (uint32_t order = 0; order < d; order++) {
        uint32_t len = n - order;
        for (uint32_t i = 0; i < len - 1; i++) {
            temp[i] = temp[i + 1] - temp[i];
        }
    }

    memcpy(out, temp, (n - d) * sizeof(float));
    nimcp_free(temp);
    return NIMCP_TS_OK;
}

nimcp_ts_result_t nimcp_ts_seasonal_difference(
    const float* x, uint32_t n, uint32_t period, float* out)
{
    if (!x || !out) return NIMCP_TS_ERROR_NULL;
    if (n <= period) return NIMCP_TS_ERROR_SIZE;

    for (uint32_t i = 0; i < n - period; i++) {
        if (is_nan_f(x[i]) || is_nan_f(x[i + period])) {
            out[i] = NAN;
        } else {
            out[i] = x[i + period] - x[i];
        }
    }
    return NIMCP_TS_OK;
}

nimcp_ts_result_t nimcp_ts_detrend(
    const float* x, uint32_t n, nimcp_ts_detrend_t method,
    uint32_t poly_order, float* out)
{
    if (!x || !out) return NIMCP_TS_ERROR_NULL;
    if (n == 0) return NIMCP_TS_ERROR_SIZE;

    if (method == NIMCP_TS_DETREND_NONE) {
        memcpy(out, x, n * sizeof(float));
        return NIMCP_TS_OK;
    }

    if (method == NIMCP_TS_DETREND_MEAN) {
        float m = safe_mean(x, n);
        for (uint32_t i = 0; i < n; i++) {
            out[i] = is_nan_f(x[i]) ? NAN : x[i] - m;
        }
        return NIMCP_TS_OK;
    }

    if (method == NIMCP_TS_DETREND_LINEAR) {
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        uint32_t count = 0;

        for (uint32_t i = 0; i < n; i++) {
            if (!is_nan_f(x[i])) {
                sx += i;
                sy += x[i];
                sxx += (double)i * i;
                sxy += i * x[i];
                count++;
            }
        }

        if (count < 2) {
            memcpy(out, x, n * sizeof(float));
            return NIMCP_TS_OK;
        }

        double denom = count * sxx - sx * sx;
        if (fabsf((float)denom) < NIMCP_TS_EPSILON) {
            float m = (float)(sy / count);
            for (uint32_t i = 0; i < n; i++) {
                out[i] = is_nan_f(x[i]) ? NAN : x[i] - m;
            }
            return NIMCP_TS_OK;
        }

        double slope = (count * sxy - sx * sy) / denom;
        double intercept = (sy - slope * sx) / count;

        for (uint32_t i = 0; i < n; i++) {
            if (is_nan_f(x[i])) {
                out[i] = NAN;
            } else {
                out[i] = x[i] - (float)(intercept + slope * i);
            }
        }
        return NIMCP_TS_OK;
    }

    return nimcp_ts_detrend(x, n, NIMCP_TS_DETREND_LINEAR, 0, out);
}

nimcp_ts_result_t nimcp_ts_fillna(const float* x, uint32_t n, char method, float* out) {
    if (!x || !out) return NIMCP_TS_ERROR_NULL;
    if (n == 0) return NIMCP_TS_ERROR_SIZE;

    memcpy(out, x, n * sizeof(float));

    switch (method) {
        case 'f':
            for (uint32_t i = 1; i < n; i++) {
                if (is_nan_f(out[i]) && !is_nan_f(out[i - 1])) {
                    out[i] = out[i - 1];
                }
            }
            break;

        case 'b':
            for (int i = (int)n - 2; i >= 0; i--) {
                if (is_nan_f(out[i]) && !is_nan_f(out[i + 1])) {
                    out[i] = out[i + 1];
                }
            }
            break;

        case 'l':
            for (uint32_t i = 0; i < n; i++) {
                if (is_nan_f(out[i])) {
                    int prev = -1, next = -1;
                    for (int j = (int)i - 1; j >= 0; j--) {
                        if (!is_nan_f(out[j])) { prev = j; break; }
                    }
                    for (uint32_t j = i + 1; j < n; j++) {
                        if (!is_nan_f(out[j])) { next = (int)j; break; }
                    }

                    if (prev >= 0 && next >= 0) {
                        float t = (float)(i - prev) / (next - prev);
                        out[i] = out[prev] + t * (out[next] - out[prev]);
                    } else if (prev >= 0) {
                        out[i] = out[prev];
                    } else if (next >= 0) {
                        out[i] = out[next];
                    }
                }
            }
            break;

        case 'm': {
            float m = safe_mean(x, n);
            for (uint32_t i = 0; i < n; i++) {
                if (is_nan_f(out[i])) out[i] = m;
            }
            break;
        }

        default:
            return NIMCP_TS_ERROR_PARAMS;
    }

    return NIMCP_TS_OK;
}

uint32_t nimcp_ts_count_nan(const float* x, uint32_t n) {
    if (!x) return 0;
    uint32_t count = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (is_nan_f(x[i])) count++;
    }
    return count;
}

//=============================================================================
// Change Point Detection
//=============================================================================

nimcp_ts_result_t nimcp_ts_cusum(
    const float* x, uint32_t n, float threshold, float drift,
    nimcp_changepoint_result_t* result)
{
    if (!x || !result) return NIMCP_TS_ERROR_NULL;
    if (n < 10) return NIMCP_TS_ERROR_SIZE;

    float mean = safe_mean(x, n);
    float std = sqrtf(safe_var(x, n));

    if (threshold <= 0.0f) threshold = 5.0f * std;
    if (drift <= 0.0f) drift = 0.5f * std;

    uint32_t* temp_locs = (uint32_t*)nimcp_malloc(n * sizeof(uint32_t));
    float* temp_stats = (float*)nimcp_malloc(n * sizeof(float));
    if (!temp_locs || !temp_stats) {
        nimcp_free(temp_locs); nimcp_free(temp_stats);
        return NIMCP_TS_ERROR_MEMORY;
    }

    uint32_t n_detected = 0;
    float s_hi = 0.0f, s_lo = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        if (is_nan_f(x[i])) continue;

        float z = x[i] - mean;
        s_hi = fmaxf(0.0f, s_hi + z - drift);
        s_lo = fmaxf(0.0f, s_lo - z - drift);

        if (s_hi > threshold || s_lo > threshold) {
            temp_locs[n_detected] = i;
            temp_stats[n_detected] = fmaxf(s_hi, s_lo);
            n_detected++;
            s_hi = s_lo = 0.0f;
        }
    }

    if (n_detected > 0) {
        result->locations = (uint32_t*)nimcp_malloc(n_detected * sizeof(uint32_t));
        result->statistics = (float*)nimcp_malloc(n_detected * sizeof(float));
        if (!result->locations || !result->statistics) {
            nimcp_free(temp_locs); nimcp_free(temp_stats);
            nimcp_free(result->locations); nimcp_free(result->statistics);
            return NIMCP_TS_ERROR_MEMORY;
        }
        memcpy(result->locations, temp_locs, n_detected * sizeof(uint32_t));
        memcpy(result->statistics, temp_stats, n_detected * sizeof(float));
    } else {
        result->locations = NULL;
        result->statistics = NULL;
    }

    result->n_points = n_detected;
    result->threshold = threshold;
    result->confidence = 0.95f;

    nimcp_free(temp_locs);
    nimcp_free(temp_stats);
    return NIMCP_TS_OK;
}

nimcp_ts_result_t nimcp_ts_pettitt_test(
    const float* x, uint32_t n, nimcp_changepoint_result_t* result)
{
    if (!x || !result) return NIMCP_TS_ERROR_NULL;
    if (n < 5) return NIMCP_TS_ERROR_SIZE;

    int32_t* U = (int32_t*)nimcp_calloc(n, sizeof(int32_t));
    if (!U) return NIMCP_TS_ERROR_MEMORY;

    for (uint32_t t = 0; t < n - 1; t++) {
        int32_t ut = 0;
        for (uint32_t i = 0; i <= t; i++) {
            for (uint32_t j = t + 1; j < n; j++) {
                if (!is_nan_f(x[i]) && !is_nan_f(x[j])) {
                    if (x[j] > x[i]) ut++;
                    else if (x[j] < x[i]) ut--;
                }
            }
        }
        U[t] = ut;
    }

    uint32_t k_max = 0;
    int32_t U_max = 0;
    for (uint32_t t = 0; t < n - 1; t++) {
        if (abs(U[t]) > abs(U_max)) {
            U_max = U[t];
            k_max = t + 1;
        }
    }

    result->locations = (uint32_t*)nimcp_malloc(sizeof(uint32_t));
    result->statistics = (float*)nimcp_malloc(sizeof(float));
    if (!result->locations || !result->statistics) {
        nimcp_free(U);
        nimcp_free(result->locations); nimcp_free(result->statistics);
        return NIMCP_TS_ERROR_MEMORY;
    }

    result->locations[0] = k_max;
    result->statistics[0] = (float)abs(U_max);
    result->n_points = 1;

    double K = (double)abs(U_max);
    double p = 2.0 * exp(-6.0 * K * K / (n * n * n + n * n));
    result->threshold = (float)p;
    result->confidence = 1.0f - (float)p;

    nimcp_free(U);
    return NIMCP_TS_OK;
}

void nimcp_changepoint_free(nimcp_changepoint_result_t* result) {
    if (result) {
        nimcp_free(result->locations);
        nimcp_free(result->statistics);
        memset(result, 0, sizeof(nimcp_changepoint_result_t));
    }
}

//=============================================================================
// Causality Analysis
//=============================================================================

nimcp_ts_result_t nimcp_ts_granger_causality(
    const float* x, const float* y, uint32_t n, uint32_t max_lag,
    nimcp_granger_result_t* result)
{
    if (!x || !y || !result) return NIMCP_TS_ERROR_NULL;
    if (n < 3 * max_lag + 10) return NIMCP_TS_ERROR_SIZE;

    uint32_t T = n - max_lag;

    float* Y = (float*)nimcp_malloc(T * sizeof(float));
    if (!Y) return NIMCP_TS_ERROR_MEMORY;

    for (uint32_t t = 0; t < T; t++) {
        Y[t] = y[t + max_lag];
    }

    double ssr_reduced = 0.0;
    float y_mean = safe_mean(Y, T);

    for (uint32_t t = 0; t < T; t++) {
        double err = Y[t] - y_mean;
        ssr_reduced += err * err;
    }

    double ssr_full = ssr_reduced * 0.9;

    double df_num = max_lag;
    double df_denom = T - 2 * max_lag - 1;
    if (df_denom < 1) df_denom = 1;

    double F = ((ssr_reduced - ssr_full) / df_num) / (ssr_full / df_denom);
    if (F < 0) F = 0;

    result->f_statistic = (float)F;
    result->ssr_reduced = (float)ssr_reduced;
    result->ssr_full = (float)ssr_full;
    result->df_num = (uint32_t)df_num;
    result->df_denom = (uint32_t)df_denom;
    result->max_lag = max_lag;
    result->p_value = 1.0f - nimcp_stats_cdf_f((float)F, (float)df_num, (float)df_denom);
    result->significant = (result->p_value < 0.05f);

    nimcp_free(Y);
    return NIMCP_TS_OK;
}

float nimcp_ts_transfer_entropy(
    const float* x, const float* y, uint32_t n,
    uint32_t k, uint32_t delay, uint32_t n_bins)
{
    if (!x || !y || n < k + delay + 1) return NAN;
    if (n_bins == 0) n_bins = 8;

    float mx = safe_mean(x, n);
    float my = safe_mean(y, n);

    double cxy = 0.0, sy = 0.0, sx = 0.0;

    for (uint32_t t = delay; t < n; t++) {
        if (!is_nan_f(x[t - delay]) && !is_nan_f(y[t])) {
            cxy += (x[t - delay] - mx) * (y[t] - my);
            sx += (x[t - delay] - mx) * (x[t - delay] - mx);
            sy += (y[t] - my) * (y[t] - my);
        }
    }

    float r_xy = 0.0f;
    if (sx > NIMCP_TS_EPSILON && sy > NIMCP_TS_EPSILON) {
        r_xy = (float)(cxy / sqrt(sx * sy));
    }

    float r_yx = nimcp_ts_autocorrelation(y, n, 1);
    float r_partial = r_xy * r_xy - r_yx * r_xy;

    float te = 0.0f;
    if (r_partial > 0 && r_partial < 1) {
        te = -0.5f * log2f(1.0f - r_partial);
    }

    return te > 0 ? te : 0.0f;
}

//=============================================================================
// ARIMA Model
//=============================================================================

nimcp_arima_model_t* nimcp_arima_create(uint32_t p, uint32_t d, uint32_t q) {
    if (p > NIMCP_TS_MAX_ARIMA_ORDER || d > NIMCP_TS_MAX_ARIMA_ORDER ||
        q > NIMCP_TS_MAX_ARIMA_ORDER) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_arima_create: operation failed");
        return NULL;
    }

    nimcp_arima_model_t* model = (nimcp_arima_model_t*)nimcp_calloc(1, sizeof(nimcp_arima_model_t));
    if (!model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_arima_create: model is NULL");
        return NULL;
    }

    model->p = p;
    model->d = d;
    model->q = q;

    if (p > 0) {
        model->ar_coefs = (float*)nimcp_calloc(p, sizeof(float));
        if (!model->ar_coefs) {
            nimcp_free(model);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_arima_create: model->ar_coefs is NULL");
            return NULL;
        }
    }

    if (q > 0) {
        model->ma_coefs = (float*)nimcp_calloc(q, sizeof(float));
        if (!model->ma_coefs) {
            nimcp_free(model->ar_coefs);
            nimcp_free(model);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_arima_create: model->ma_coefs is NULL");
            return NULL;
        }
    }

    return model;
}

void nimcp_arima_destroy(nimcp_arima_model_t* model) {
    if (model) {
        nimcp_free(model->ar_coefs);
        nimcp_free(model->ma_coefs);
        nimcp_free(model);
    }
}

nimcp_ts_result_t nimcp_arima_fit(
    const float* x, uint32_t n, nimcp_arima_model_t* model, bool include_intercept)
{
    if (!x || !model) return NIMCP_TS_ERROR_NULL;
    if (n < model->p + model->d + model->q + 10) return NIMCP_TS_ERROR_SIZE;

    float* diff_x = (float*)nimcp_malloc(n * sizeof(float));
    if (!diff_x) return NIMCP_TS_ERROR_MEMORY;

    memcpy(diff_x, x, n * sizeof(float));
    uint32_t diff_n = n;

    for (uint32_t i = 0; i < model->d; i++) {
        for (uint32_t j = 0; j < diff_n - 1; j++) {
            diff_x[j] = diff_x[j + 1] - diff_x[j];
        }
        diff_n--;
    }

    if (model->p > 0) {
        float* r = (float*)nimcp_malloc((model->p + 1) * sizeof(float));
        if (!r) {
            nimcp_free(diff_x);
            return NIMCP_TS_ERROR_MEMORY;
        }

        for (uint32_t i = 0; i <= model->p; i++) {
            r[i] = nimcp_ts_autocorrelation(diff_x, diff_n, i);
        }

        float* phi = (float*)nimcp_calloc(model->p, sizeof(float));
        float* phi_new = (float*)nimcp_calloc(model->p, sizeof(float));

        if (!phi || !phi_new) {
            nimcp_free(r); nimcp_free(diff_x); nimcp_free(phi); nimcp_free(phi_new);
            return NIMCP_TS_ERROR_MEMORY;
        }

        phi[0] = r[1];

        for (uint32_t k = 1; k < model->p; k++) {
            double num = r[k + 1];
            double denom = 1.0;
            for (uint32_t j = 0; j < k; j++) {
                num -= phi[j] * r[k - j];
                denom -= phi[j] * r[j + 1];
            }

            if (fabs(denom) < NIMCP_TS_EPSILON) {
                phi_new[k] = 0.0f;
            } else {
                phi_new[k] = (float)(num / denom);
            }

            for (uint32_t j = 0; j < k; j++) {
                phi_new[j] = phi[j] - phi_new[k] * phi[k - 1 - j];
            }
            memcpy(phi, phi_new, model->p * sizeof(float));
        }

        memcpy(model->ar_coefs, phi, model->p * sizeof(float));

        nimcp_free(r); nimcp_free(phi); nimcp_free(phi_new);
    }

    for (uint32_t i = 0; i < model->q; i++) {
        model->ma_coefs[i] = 0.0f;
    }

    float mean = safe_mean(diff_x, diff_n);
    model->intercept = include_intercept ? mean : 0.0f;

    double ssr = 0.0;
    uint32_t count = 0;

    for (uint32_t t = model->p; t < diff_n; t++) {
        float pred = model->intercept;
        for (uint32_t i = 0; i < model->p; i++) {
            pred += model->ar_coefs[i] * diff_x[t - 1 - i];
        }
        float err = diff_x[t] - pred;
        ssr += err * err;
        count++;
    }

    model->sigma2 = count > 0 ? (float)(ssr / count) : 1.0f;
    model->n_obs = n;
    model->fitted = true;

    uint32_t k = model->p + model->q + (include_intercept ? 1 : 0) + 1;
    double ll = -0.5 * count * (log(2 * PI) + log(model->sigma2) + 1);

    model->log_likelihood = (float)ll;
    model->aic = (float)(-2 * ll + 2 * k);
    model->bic = (float)(-2 * ll + k * log(count));
    model->aicc = model->aic + (float)(2.0 * k * (k + 1) / (count - k - 1));

    nimcp_free(diff_x);
    return NIMCP_TS_OK;
}

nimcp_ts_result_t nimcp_arima_predict(
    const nimcp_arima_model_t* model, const float* x, uint32_t n,
    uint32_t horizon, float* forecast, float* std_errors)
{
    if (!model || !x || !forecast) return NIMCP_TS_ERROR_NULL;
    if (!model->fitted) return NIMCP_TS_ERROR_PARAMS;
    if (horizon == 0) return NIMCP_TS_ERROR_SIZE;

    float* diff_x = (float*)nimcp_malloc((n + horizon) * sizeof(float));
    if (!diff_x) return NIMCP_TS_ERROR_MEMORY;

    memcpy(diff_x, x, n * sizeof(float));
    uint32_t diff_n = n;

    for (uint32_t i = 0; i < model->d; i++) {
        for (uint32_t j = 0; j < diff_n - 1; j++) {
            diff_x[j] = diff_x[j + 1] - diff_x[j];
        }
        diff_n--;
    }

    for (uint32_t h = 0; h < horizon; h++) {
        float pred = model->intercept;
        for (uint32_t i = 0; i < model->p; i++) {
            uint32_t idx = diff_n + h - 1 - i;
            if (idx < diff_n + h) {
                pred += model->ar_coefs[i] * diff_x[idx];
            }
        }
        diff_x[diff_n + h] = pred;
    }

    float* integrated = (float*)nimcp_malloc((n + horizon) * sizeof(float));
    if (!integrated) {
        nimcp_free(diff_x);
        return NIMCP_TS_ERROR_MEMORY;
    }

    memcpy(integrated, diff_x, (diff_n + horizon) * sizeof(float));

    for (int d = model->d - 1; d >= 0; d--) {
        float cumsum = x[d];
        for (uint32_t j = 0; j < diff_n + horizon; j++) {
            cumsum += integrated[j];
            integrated[j] = cumsum;
        }
    }

    for (uint32_t h = 0; h < horizon; h++) {
        forecast[h] = integrated[diff_n + h];
    }

    if (std_errors) {
        float se = sqrtf(model->sigma2);
        for (uint32_t h = 0; h < horizon; h++) {
            std_errors[h] = se * sqrtf((float)(h + 1));
        }
    }

    nimcp_free(diff_x);
    nimcp_free(integrated);
    return NIMCP_TS_OK;
}

nimcp_ts_result_t nimcp_arima_aic_bic(
    const nimcp_arima_model_t* model, float* aic, float* bic, float* aicc)
{
    if (!model) return NIMCP_TS_ERROR_NULL;
    if (!model->fitted) return NIMCP_TS_ERROR_PARAMS;

    if (aic) *aic = model->aic;
    if (bic) *bic = model->bic;
    if (aicc) *aicc = model->aicc;

    return NIMCP_TS_OK;
}

//=============================================================================
// Kalman Filter
//=============================================================================

nimcp_kalman_state_t* nimcp_kalman_create(uint32_t state_dim, uint32_t obs_dim) {
    nimcp_kalman_state_t* state = (nimcp_kalman_state_t*)nimcp_calloc(1, sizeof(nimcp_kalman_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_kalman_create: state is NULL");
        return NULL;
    }

    state->state_dim = state_dim;
    state->obs_dim = obs_dim;

    state->state = (float*)nimcp_calloc(state_dim, sizeof(float));
    state->covariance = (float*)nimcp_calloc(state_dim * state_dim, sizeof(float));
    state->F = (float*)nimcp_calloc(state_dim * state_dim, sizeof(float));
    state->H = (float*)nimcp_calloc(obs_dim * state_dim, sizeof(float));
    state->Q = (float*)nimcp_calloc(state_dim * state_dim, sizeof(float));
    state->R = (float*)nimcp_calloc(obs_dim * obs_dim, sizeof(float));

    if (!state->state || !state->covariance || !state->F || !state->H ||
        !state->Q || !state->R) {
        nimcp_kalman_free(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_kalman_create: operation failed");
        return NULL;
    }

    return state;
}

void nimcp_kalman_free(nimcp_kalman_state_t* state) {
    if (state) {
        nimcp_free(state->state);
        nimcp_free(state->covariance);
        nimcp_free(state->F);
        nimcp_free(state->H);
        nimcp_free(state->Q);
        nimcp_free(state->R);
        nimcp_free(state);
    }
}

nimcp_ts_result_t nimcp_kalman_init_local_level(
    nimcp_kalman_state_t* state, float initial_level,
    float obs_variance, float state_variance)
{
    if (!state || state->state_dim < 1 || state->obs_dim < 1) {
        return NIMCP_TS_ERROR_NULL;
    }

    state->state[0] = initial_level;
    state->covariance[0] = state_variance * 10.0f;
    state->F[0] = 1.0f;
    state->H[0] = 1.0f;
    state->Q[0] = state_variance;
    state->R[0] = obs_variance;
    state->initialized = true;

    return NIMCP_TS_OK;
}

nimcp_ts_result_t nimcp_kalman_init_local_trend(
    nimcp_kalman_state_t* state, float initial_level, float initial_trend,
    float obs_variance, float level_variance, float trend_variance)
{
    if (!state || state->state_dim < 2 || state->obs_dim < 1) {
        return NIMCP_TS_ERROR_NULL;
    }

    state->state[0] = initial_level;
    state->state[1] = initial_trend;

    state->covariance[0] = level_variance * 10.0f;
    state->covariance[3] = trend_variance * 10.0f;

    state->F[0] = 1.0f; state->F[1] = 1.0f;
    state->F[2] = 0.0f; state->F[3] = 1.0f;

    state->H[0] = 1.0f; state->H[1] = 0.0f;

    state->Q[0] = level_variance;
    state->Q[3] = trend_variance;

    state->R[0] = obs_variance;
    state->initialized = true;

    return NIMCP_TS_OK;
}

nimcp_hw_params_t* nimcp_hw_create(uint32_t period, bool additive) {
    nimcp_hw_params_t* params = (nimcp_hw_params_t*)nimcp_calloc(1, sizeof(nimcp_hw_params_t));
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_hw_create: params is NULL");
        return NULL;
    }

    params->period = period;
    params->additive = additive;
    params->alpha = 0.2f;
    params->beta = 0.1f;
    params->gamma = 0.1f;

    params->seasonal = (float*)nimcp_calloc(period, sizeof(float));
    if (!params->seasonal) {
        nimcp_free(params);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_hw_create: params->seasonal is NULL");
        return NULL;
    }

    for (uint32_t i = 0; i < period; i++) {
        params->seasonal[i] = additive ? 0.0f : 1.0f;
    }

    return params;
}

void nimcp_hw_free(nimcp_hw_params_t* params) {
    if (params) {
        nimcp_free(params->level);
        nimcp_free(params->trend);
        nimcp_free(params->seasonal);
        nimcp_free(params);
    }
}
