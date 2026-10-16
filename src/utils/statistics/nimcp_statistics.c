/**
 * @file nimcp_statistics.c
 * @brief Implementation of statistical and probability methods
 *
 * WHAT: Complete statistical toolkit implementation
 * WHY:  Probabilistic computation foundation for neural systems
 * HOW:  Numerically stable algorithms with SIMD optimization where applicable
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include "utils/statistics/nimcp_statistics.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include "constants/nimcp_math_constants.h"

//=============================================================================
// MODULE IDENTIFICATION
//=============================================================================

#define LOG_MODULE_NAME "statistics"

//=============================================================================
// CONSTANTS
//=============================================================================

#define PI NIMCP_PI
#define SQRT_2PI 2.5066282746310002
#define LN_SQRT_2PI 0.9189385332046727
#define EULER_GAMMA 0.5772156649015329

//=============================================================================
// Global State
//=============================================================================

/* P3: Use _Atomic for thread-safe access to initialization flag */
static _Atomic bool g_stats_initialized = false;
/* P3-U8: g_stats_config is IMMUTABLE after nimcp_stats_init() returns.
 * All fields are set once during initialization and only read thereafter.
 * No mutex is needed because there is a happens-before relationship:
 * g_stats_initialized (atomic store with release) guarantees visibility
 * of g_stats_config writes to all threads that observe initialized==true. */
static nimcp_stats_config_t g_stats_config;

//=============================================================================
// Module Initialization
//=============================================================================

nimcp_stats_config_t nimcp_stats_default_config(void) {
    nimcp_stats_config_t config = {
        .enable_simd = false,
        .enable_parallel = false,
        .parallel_threshold = 10000,
        .bootstrap_default = 1000,
        .random_seed = 0
    };
    return config;
}

nimcp_stats_result_t nimcp_stats_init(const nimcp_stats_config_t* config) {
    if (config) {
        g_stats_config = *config;
    } else {
        g_stats_config = nimcp_stats_default_config();
    }
    g_stats_initialized = true;
    return NIMCP_STATS_OK;
}

void nimcp_stats_shutdown(void) {
    g_stats_initialized = false;
}

bool nimcp_stats_is_initialized(void) {
    return g_stats_initialized;
}

//=============================================================================
// Helper Functions
//=============================================================================

static inline float kahan_sum(const float* data, uint32_t n) {
    float sum = 0.0f;
    float c = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        // Handle special values: NaN propagates, infinity propagates
        if (isnan(data[i])) return NAN;
        if (isinf(data[i])) {
            // Check for inf - inf = NaN situation
            if (isinf(sum) && ((data[i] > 0) != (sum > 0))) return NAN;
            sum += data[i];
            continue;
        }
        if (isinf(sum)) continue;  // inf + finite = inf, skip Kahan compensation

        float y = data[i] - c;
        float t = sum + y;
        c = (t - sum) - y;
        sum = t;
    }
    return sum;
}

static int float_compare(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

static float quickselect(float* arr, uint32_t n, uint32_t k) {
    if (n == 1) return arr[0];

    uint32_t left = 0, right = n - 1;
    while (left < right) {
        float pivot = arr[(left + right) / 2];
        uint32_t i = left, j = right;

        while (i <= j) {
            while (arr[i] < pivot) i++;
            while (arr[j] > pivot) j--;
            if (i <= j) {
                float tmp = arr[i];
                arr[i] = arr[j];
                arr[j] = tmp;
                i++;
                if (j > 0) j--;
            }
        }

        if (k <= j) right = j;
        else if (k >= i) left = i;
        else break;
    }
    return arr[k];
}

//=============================================================================
// Descriptive Statistics - Single Values
//=============================================================================

float nimcp_stats_mean(const float* data, uint32_t n) {
    if (!data || n == 0) return NAN;
    return kahan_sum(data, n) / (float)n;
}

float nimcp_stats_variance(const float* data, uint32_t n) {
    if (!data || n < 2) return NAN;

    // Welford's algorithm
    double mean = 0.0;
    double m2 = 0.0;

    for (uint32_t i = 0; i < n; i++) {
        double delta = data[i] - mean;
        mean += delta / (i + 1);
        double delta2 = data[i] - mean;
        m2 += delta * delta2;
    }

    return (float)(m2 / (n - 1));
}

float nimcp_stats_variance_population(const float* data, uint32_t n) {
    if (!data || n == 0) return NAN;

    double mean = 0.0;
    double m2 = 0.0;

    for (uint32_t i = 0; i < n; i++) {
        double delta = data[i] - mean;
        mean += delta / (i + 1);
        double delta2 = data[i] - mean;
        m2 += delta * delta2;
    }

    return (float)(m2 / n);
}

float nimcp_stats_std_dev(const float* data, uint32_t n) {
    float var = nimcp_stats_variance(data, n);
    return isnan(var) ? NAN : sqrtf(var);
}

float nimcp_stats_std_dev_population(const float* data, uint32_t n) {
    float var = nimcp_stats_variance_population(data, n);
    return isnan(var) ? NAN : sqrtf(var);
}

float nimcp_stats_std_error(const float* data, uint32_t n) {
    if (!data || n < 2) return NAN;
    return nimcp_stats_std_dev(data, n) / sqrtf((float)n);
}

float nimcp_stats_min(const float* data, uint32_t n) {
    if (!data || n == 0) return NAN;
    float min_val = data[0];
    for (uint32_t i = 1; i < n; i++) {
        if (data[i] < min_val) min_val = data[i];
    }
    return min_val;
}

float nimcp_stats_max(const float* data, uint32_t n) {
    if (!data || n == 0) return NAN;
    float max_val = data[0];
    for (uint32_t i = 1; i < n; i++) {
        if (data[i] > max_val) max_val = data[i];
    }
    return max_val;
}

float nimcp_stats_range(const float* data, uint32_t n) {
    if (!data || n == 0) return NAN;
    float min_val = data[0], max_val = data[0];
    for (uint32_t i = 1; i < n; i++) {
        if (data[i] < min_val) min_val = data[i];
        if (data[i] > max_val) max_val = data[i];
    }
    return max_val - min_val;
}

float nimcp_stats_sum(const float* data, uint32_t n) {
    if (!data || n == 0) return NAN;
    return kahan_sum(data, n);
}

float nimcp_stats_median(const float* data, uint32_t n) {
    if (!data || n == 0) return NAN;

    float* sorted = (float*)nimcp_malloc(n * sizeof(float));
    if (!sorted) return NAN;
    memcpy(sorted, data, n * sizeof(float));

    float result;
    if (n % 2 == 1) {
        result = quickselect(sorted, n, n / 2);
    } else {
        float a = quickselect(sorted, n, n / 2 - 1);
        float b = quickselect(sorted, n, n / 2);
        result = (a + b) / 2.0f;
    }

    nimcp_free(sorted);
    return result;
}

float nimcp_stats_quantile(const float* data, uint32_t n, float p) {
    if (!data || n == 0 || p < 0.0f || p > 1.0f) return NAN;

    float* sorted = (float*)nimcp_malloc(n * sizeof(float));
    if (!sorted) return NAN;
    memcpy(sorted, data, n * sizeof(float));
    qsort(sorted, n, sizeof(float), float_compare);

    float index = p * (n - 1);
    uint32_t lower = (uint32_t)floorf(index);
    uint32_t upper = (uint32_t)ceilf(index);

    float result;
    if (lower == upper) {
        result = sorted[lower];
    } else {
        float frac = index - lower;
        result = sorted[lower] * (1.0f - frac) + sorted[upper] * frac;
    }

    nimcp_free(sorted);
    return result;
}

float nimcp_stats_iqr(const float* data, uint32_t n) {
    if (!data || n < 4) return NAN;
    float q1 = nimcp_stats_quantile(data, n, 0.25f);
    float q3 = nimcp_stats_quantile(data, n, 0.75f);
    return q3 - q1;
}

float nimcp_stats_skewness(const float* data, uint32_t n) {
    if (!data || n < 3) return NAN;

    double mean = nimcp_stats_mean(data, n);
    double m2 = 0.0, m3 = 0.0;

    for (uint32_t i = 0; i < n; i++) {
        double d = data[i] - mean;
        double d2 = d * d;
        m2 += d2;
        m3 += d2 * d;
    }

    m2 /= n;
    m3 /= n;

    if (m2 == 0.0) return 0.0f;
    return (float)(m3 / pow(m2, 1.5));
}

float nimcp_stats_kurtosis(const float* data, uint32_t n) {
    if (!data || n < 4) return NAN;

    double mean = nimcp_stats_mean(data, n);
    double m2 = 0.0, m4 = 0.0;

    for (uint32_t i = 0; i < n; i++) {
        double d = data[i] - mean;
        double d2 = d * d;
        m2 += d2;
        m4 += d2 * d2;
    }

    m2 /= n;
    m4 /= n;

    if (m2 == 0.0) return 0.0f;
    return (float)(m4 / (m2 * m2) - 3.0);
}

float nimcp_stats_geometric_mean(const float* data, uint32_t n) {
    if (!data || n == 0) return NAN;

    double log_sum = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        if (data[i] <= 0.0f) return NAN;
        log_sum += log((double)data[i]);
    }

    return (float)exp(log_sum / n);
}

float nimcp_stats_harmonic_mean(const float* data, uint32_t n) {
    if (!data || n == 0) return NAN;

    double inv_sum = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        if (data[i] <= 0.0f) return NAN;
        inv_sum += 1.0 / data[i];
    }

    return (float)(n / inv_sum);
}

float nimcp_stats_mode(const float* data, uint32_t n, float bin_width) {
    if (!data || n == 0) return NAN;

    float min_val = nimcp_stats_min(data, n);
    float max_val = nimcp_stats_max(data, n);

    if (bin_width <= 0.0f) {
        bin_width = (max_val - min_val) / NIMCP_STATS_DEFAULT_BINS;
        if (bin_width == 0.0f) return data[0];
    }

    uint32_t n_bins = (uint32_t)ceilf((max_val - min_val) / bin_width) + 1;
    uint32_t* counts = (uint32_t*)nimcp_calloc(n_bins, sizeof(uint32_t));
    if (!counts) return NAN;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t bin = (uint32_t)((data[i] - min_val) / bin_width);
        if (bin < n_bins) counts[bin]++;
    }

    uint32_t max_count = 0, max_bin = 0;
    for (uint32_t i = 0; i < n_bins; i++) {
        if (counts[i] > max_count) {
            max_count = counts[i];
            max_bin = i;
        }
    }

    nimcp_free(counts);
    return min_val + (max_bin + 0.5f) * bin_width;
}

float nimcp_stats_coef_variation(const float* data, uint32_t n) {
    if (!data || n < 2) return NAN;
    float mean = nimcp_stats_mean(data, n);
    if (mean == 0.0f) return NAN;
    return nimcp_stats_std_dev(data, n) / fabsf(mean);
}

//=============================================================================
// Descriptive Statistics - Comprehensive
//=============================================================================

nimcp_stats_result_t nimcp_stats_describe(
    const float* data,
    uint32_t n,
    nimcp_descriptive_stats_t* stats
) {
    if (!data || !stats) return NIMCP_STATS_ERROR_NULL;
    if (n == 0) return NIMCP_STATS_ERROR_SIZE;

    stats->n = n;
    stats->sum = kahan_sum(data, n);
    stats->mean = stats->sum / n;

    // Single pass for min, max, variance, skewness, kurtosis
    double m2 = 0.0, m3 = 0.0, m4 = 0.0;
    stats->min = data[0];
    stats->max = data[0];

    for (uint32_t i = 0; i < n; i++) {
        if (data[i] < stats->min) stats->min = data[i];
        if (data[i] > stats->max) stats->max = data[i];

        double d = data[i] - stats->mean;
        double d2 = d * d;
        m2 += d2;
        m3 += d2 * d;
        m4 += d2 * d2;
    }

    stats->range = stats->max - stats->min;

    if (n > 1) {
        stats->variance = (float)(m2 / (n - 1));
        stats->std_dev = sqrtf(stats->variance);
        stats->std_error = stats->std_dev / sqrtf((float)n);
    } else {
        stats->variance = 0.0f;
        stats->std_dev = 0.0f;
        stats->std_error = 0.0f;
    }

    double pop_var = m2 / n;
    if (pop_var > 0.0 && n >= 3) {
        stats->skewness = (float)((m3 / n) / pow(pop_var, 1.5));
    } else {
        stats->skewness = 0.0f;
    }

    if (pop_var > 0.0 && n >= 4) {
        stats->kurtosis = (float)((m4 / n) / (pop_var * pop_var) - 3.0);
    } else {
        stats->kurtosis = 0.0f;
    }

    // Quantiles (requires sorting)
    stats->median = nimcp_stats_quantile(data, n, 0.5f);
    stats->q1 = nimcp_stats_quantile(data, n, 0.25f);
    stats->q3 = nimcp_stats_quantile(data, n, 0.75f);
    stats->iqr = stats->q3 - stats->q1;

    // Geometric and harmonic means
    stats->geometric_mean = nimcp_stats_geometric_mean(data, n);
    stats->harmonic_mean = nimcp_stats_harmonic_mean(data, n);

    return NIMCP_STATS_OK;
}

//=============================================================================
// Running Statistics
//=============================================================================

void nimcp_stats_running_init(nimcp_running_stats_t* stats) {
    if (!stats) return;
    memset(stats, 0, sizeof(nimcp_running_stats_t));
    stats->min = DBL_MAX;
    stats->max = -DBL_MAX;
}

void nimcp_stats_running_add(nimcp_running_stats_t* stats, double x) {
    if (!stats) return;

    stats->n++;
    stats->sum += x;

    if (x < stats->min) stats->min = x;
    if (x > stats->max) stats->max = x;

    double n = (double)stats->n;
    double delta = x - stats->mean;
    double delta_n = delta / n;
    double delta_n2 = delta_n * delta_n;
    double term1 = delta * delta_n * (n - 1);

    stats->mean += delta_n;
    stats->m4 += term1 * delta_n2 * (n * n - 3 * n + 3) +
                 6 * delta_n2 * stats->m2 - 4 * delta_n * stats->m3;
    stats->m3 += term1 * delta_n * (n - 2) - 3 * delta_n * stats->m2;
    stats->m2 += term1;
}

void nimcp_stats_running_add_array(
    nimcp_running_stats_t* stats,
    const float* data,
    uint32_t n
) {
    if (!stats || !data) return;
    for (uint32_t i = 0; i < n; i++) {
        nimcp_stats_running_add(stats, (double)data[i]);
    }
}

double nimcp_stats_running_mean(const nimcp_running_stats_t* stats) {
    if (!stats || stats->n == 0) return NAN;
    return stats->mean;
}

double nimcp_stats_running_variance(const nimcp_running_stats_t* stats) {
    if (!stats || stats->n < 2) return NAN;
    return stats->m2 / (stats->n - 1);
}

double nimcp_stats_running_std_dev(const nimcp_running_stats_t* stats) {
    double var = nimcp_stats_running_variance(stats);
    return isnan(var) ? NAN : sqrt(var);
}

double nimcp_stats_running_skewness(const nimcp_running_stats_t* stats) {
    if (!stats || stats->n < 3) return NAN;
    double m2 = stats->m2 / stats->n;
    if (m2 == 0.0) return 0.0;
    return (stats->m3 / stats->n) / pow(m2, 1.5);
}

double nimcp_stats_running_kurtosis(const nimcp_running_stats_t* stats) {
    if (!stats || stats->n < 4) return NAN;
    double m2 = stats->m2 / stats->n;
    if (m2 == 0.0) return 0.0;
    return (stats->m4 / stats->n) / (m2 * m2) - 3.0;
}

void nimcp_stats_running_merge(
    nimcp_running_stats_t* a,
    const nimcp_running_stats_t* b
) {
    if (!a || !b || b->n == 0) return;
    if (a->n == 0) {
        *a = *b;
        return;
    }

    uint64_t n = a->n + b->n;
    double delta = b->mean - a->mean;
    double delta2 = delta * delta;
    double delta3 = delta * delta2;
    double delta4 = delta2 * delta2;

    double m2 = a->m2 + b->m2 + delta2 * a->n * b->n / n;
    double m3 = a->m3 + b->m3 + delta3 * a->n * b->n * (a->n - b->n) / (n * n) +
                3.0 * delta * (a->n * b->m2 - b->n * a->m2) / n;
    double m4 = a->m4 + b->m4 +
                delta4 * a->n * b->n * (a->n * a->n - a->n * b->n + b->n * b->n) / (n * n * n) +
                6.0 * delta2 * (a->n * a->n * b->m2 + b->n * b->n * a->m2) / (n * n) +
                4.0 * delta * (a->n * b->m3 - b->n * a->m3) / n;

    a->mean = (a->n * a->mean + b->n * b->mean) / n;
    a->m2 = m2;
    a->m3 = m3;
    a->m4 = m4;
    a->n = n;
    a->sum = a->sum + b->sum;
    if (b->min < a->min) a->min = b->min;
    if (b->max > a->max) a->max = b->max;
}

//=============================================================================
// Special Mathematical Functions
//=============================================================================

double nimcp_stats_lgamma(double x) {
    if (x <= 0.0) return INFINITY;

    // Lanczos approximation
    static const double g = 7.0;
    static const double c[8] = {
        0.99999999999980993,
        676.5203681218851,
        -1259.1392167224028,
        771.32342877765313,
        -176.61502916214059,
        12.507343278686905,
        -0.13857109526572012,
        9.9843695780195716e-6
    };

    if (x < 0.5) {
        return log(PI / sin(PI * x)) - nimcp_stats_lgamma(1.0 - x);
    }

    x -= 1.0;
    double a = c[0];
    for (int i = 1; i < 8; i++) {
        a += c[i] / (x + i);
    }

    double t = x + g + 0.5;
    return LN_SQRT_2PI + (x + 0.5) * log(t) - t + log(a);
}

double nimcp_stats_beta_fn(double a, double b) {
    return exp(nimcp_stats_lgamma(a) + nimcp_stats_lgamma(b) - nimcp_stats_lgamma(a + b));
}

double nimcp_stats_erf(double x) {
    // Abramowitz and Stegun approximation
    double t = 1.0 / (1.0 + 0.5 * fabs(x));
    double tau = t * exp(-x * x - 1.26551223 +
                         t * (1.00002368 +
                         t * (0.37409196 +
                         t * (0.09678418 +
                         t * (-0.18628806 +
                         t * (0.27886807 +
                         t * (-1.13520398 +
                         t * (1.48851587 +
                         t * (-0.82215223 +
                         t * 0.17087277)))))))));
    return x >= 0 ? 1.0 - tau : tau - 1.0;
}

double nimcp_stats_erfc(double x) {
    return 1.0 - nimcp_stats_erf(x);
}

double nimcp_stats_gammainc(double a, double x) {
    if (x < 0.0 || a <= 0.0) return NAN;
    if (x == 0.0) return 0.0;

    // Series expansion for small x
    if (x < a + 1.0) {
        double sum = 1.0 / a;
        double term = sum;
        for (int n = 1; n < NIMCP_STATS_MAX_ITERATIONS; n++) {
            term *= x / (a + n);
            sum += term;
            if (fabs(term) < NIMCP_STATS_EPSILON * fabs(sum)) break;
        }
        return sum * exp(-x + a * log(x) - nimcp_stats_lgamma(a));
    }

    // Continued fraction for large x
    double f = 1.0, c = 1.0, d = 1.0 / (x + 1.0 - a);
    double h = d;
    for (int n = 1; n < NIMCP_STATS_MAX_ITERATIONS; n++) {
        double an = n * (a - n);
        double bn = x + 2.0 * n + 1.0 - a;
        d = bn + an * d;
        if (fabs(d) < NIMCP_STATS_EPSILON) d = NIMCP_STATS_EPSILON;
        c = bn + an / c;
        if (fabs(c) < NIMCP_STATS_EPSILON) c = NIMCP_STATS_EPSILON;
        d = 1.0 / d;
        double delta = d * c;
        h *= delta;
        if (fabs(delta - 1.0) < NIMCP_STATS_EPSILON) break;
    }
    return 1.0 - exp(-x + a * log(x) - nimcp_stats_lgamma(a)) * h;
}

// Helper: continued fraction for incomplete beta
static double betacf(double a, double b, double x) {
    const int MAXIT = 200;
    const double EPS = 3.0e-12;
    const double FPMIN = 1.0e-30;

    double qab = a + b;
    double qap = a + 1.0;
    double qam = a - 1.0;
    double c = 1.0;
    double d = 1.0 - qab * x / qap;
    if (fabs(d) < FPMIN) d = FPMIN;
    d = 1.0 / d;
    double h = d;

    for (int m = 1; m <= MAXIT; m++) {
        int m2 = 2 * m;
        double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
        d = 1.0 + aa * d;
        if (fabs(d) < FPMIN) d = FPMIN;
        c = 1.0 + aa / c;
        if (fabs(c) < FPMIN) c = FPMIN;
        d = 1.0 / d;
        h *= d * c;

        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
        d = 1.0 + aa * d;
        if (fabs(d) < FPMIN) d = FPMIN;
        c = 1.0 + aa / c;
        if (fabs(c) < FPMIN) c = FPMIN;
        d = 1.0 / d;
        double del = d * c;
        h *= del;

        if (fabs(del - 1.0) < EPS) break;
    }
    return h;
}

double nimcp_stats_betainc(double x, double a, double b) {
    if (x < 0.0 || x > 1.0 || a <= 0.0 || b <= 0.0) return NAN;
    if (x == 0.0) return 0.0;
    if (x == 1.0) return 1.0;

    // Compute the factor in front of the continued fraction
    double bt = exp(nimcp_stats_lgamma(a + b) - nimcp_stats_lgamma(a) - nimcp_stats_lgamma(b) +
                    a * log(x) + b * log(1.0 - x));

    // Use symmetry for numerical stability
    if (x < (a + 1.0) / (a + b + 2.0)) {
        return bt * betacf(a, b, x) / a;
    } else {
        return 1.0 - bt * betacf(b, a, 1.0 - x) / b;
    }
}

double nimcp_stats_binomial_coef(uint32_t n, uint32_t k) {
    if (k > n) return 0.0;
    if (k == 0 || k == n) return 1.0;
    if (k > n - k) k = n - k;

    double result = 1.0;
    for (uint32_t i = 0; i < k; i++) {
        result *= (double)(n - i) / (double)(i + 1);
    }
    return result;
}

double nimcp_stats_log_binomial_coef(uint32_t n, uint32_t k) {
    if (k > n) return -INFINITY;
    if (k == 0 || k == n) return 0.0;
    return nimcp_stats_lgamma(n + 1) - nimcp_stats_lgamma(k + 1) - nimcp_stats_lgamma(n - k + 1);
}

//=============================================================================
// Probability Distributions - PDF
//=============================================================================

float nimcp_stats_pdf_standard_normal(float x) {
    return (float)(exp(-0.5 * x * x) / SQRT_2PI);
}

float nimcp_stats_pdf_normal(float x, float mu, float sigma) {
    if (sigma <= 0.0f) return NAN;
    float z = (x - mu) / sigma;
    return nimcp_stats_pdf_standard_normal(z) / sigma;
}

float nimcp_stats_pdf_exponential(float x, float lambda) {
    if (lambda <= 0.0f || x < 0.0f) return 0.0f;
    return lambda * expf(-lambda * x);
}

float nimcp_stats_pdf_gamma(float x, float shape, float scale) {
    if (shape <= 0.0f || scale <= 0.0f) return NAN;  // Invalid distribution params
    if (x < 0.0f) return 0.0f;  // PDF is 0 for negative x
    if (x == 0.0f) return (shape < 1.0f) ? INFINITY : (shape == 1.0f ? 1.0f / scale : 0.0f);
    double k = shape, theta = scale;
    return (float)(pow(x, k - 1) * exp(-x / theta) / (pow(theta, k) * exp(nimcp_stats_lgamma(k))));
}

float nimcp_stats_pdf_beta(float x, float alpha, float beta) {
    if (alpha <= 0.0f || beta <= 0.0f) return NAN;
    if (x < 0.0f || x > 1.0f) return 0.0f;
    if (x == 0.0f) return (alpha < 1.0f) ? INFINITY : (alpha == 1.0f ? (float)beta : 0.0f);
    if (x == 1.0f) return (beta < 1.0f) ? INFINITY : (beta == 1.0f ? (float)alpha : 0.0f);

    double log_pdf = (alpha - 1) * log(x) + (beta - 1) * log(1 - x) -
                     nimcp_stats_lgamma(alpha) - nimcp_stats_lgamma(beta) +
                     nimcp_stats_lgamma(alpha + beta);
    return (float)exp(log_pdf);
}

float nimcp_stats_pmf_poisson(uint32_t k, float lambda) {
    if (lambda <= 0.0f) return NAN;
    return (float)exp(k * log(lambda) - lambda - nimcp_stats_lgamma(k + 1));
}

float nimcp_stats_pmf_binomial(uint32_t k, uint32_t n, float p) {
    if (p < 0.0f || p > 1.0f || k > n) return 0.0f;
    if (p == 0.0f) return (k == 0) ? 1.0f : 0.0f;
    if (p == 1.0f) return (k == n) ? 1.0f : 0.0f;

    double log_pmf = nimcp_stats_log_binomial_coef(n, k) + k * log(p) + (n - k) * log(1 - p);
    return (float)exp(log_pmf);
}

float nimcp_stats_pdf_student_t(float x, float df) {
    if (df <= 0.0f) return NAN;
    double v = df;
    double coef = exp(nimcp_stats_lgamma((v + 1) / 2) - nimcp_stats_lgamma(v / 2)) / sqrt(v * PI);
    return (float)(coef * pow(1 + x * x / v, -(v + 1) / 2));
}

float nimcp_stats_pdf_chi_squared(float x, float df) {
    if (df <= 0.0f || x < 0.0f) return 0.0f;
    return nimcp_stats_pdf_gamma(x, df / 2.0f, 2.0f);
}

float nimcp_stats_pdf_f(float x, float df1, float df2) {
    if (df1 <= 0.0f || df2 <= 0.0f || x < 0.0f) return 0.0f;
    double d1 = df1, d2 = df2;
    double num = pow(d1 * x, d1 / 2) * pow(d2, d2 / 2);
    double den = pow(d1 * x + d2, (d1 + d2) / 2);
    double b = nimcp_stats_beta_fn(d1 / 2, d2 / 2);
    return (float)(num / (x * den * b));
}

float nimcp_stats_pdf(float x, const nimcp_distribution_params_t* params) {
    if (!params) return NAN;
    switch (params->type) {
        case NIMCP_DIST_NORMAL:
            return nimcp_stats_pdf_normal(x, params->params.normal.mu, params->params.normal.sigma);
        case NIMCP_DIST_EXPONENTIAL:
            return nimcp_stats_pdf_exponential(x, params->params.exponential.lambda);
        case NIMCP_DIST_GAMMA:
            return nimcp_stats_pdf_gamma(x, params->params.gamma.shape, params->params.gamma.scale);
        case NIMCP_DIST_BETA:
            return nimcp_stats_pdf_beta(x, params->params.beta.alpha, params->params.beta.beta);
        case NIMCP_DIST_STUDENT_T:
            return nimcp_stats_pdf_student_t(x, params->params.student_t.df);
        case NIMCP_DIST_CHI_SQUARED:
            return nimcp_stats_pdf_chi_squared(x, params->params.chi_squared.df);
        case NIMCP_DIST_F:
            return nimcp_stats_pdf_f(x, params->params.f.df1, params->params.f.df2);
        default:
            return NAN;
    }
}

//=============================================================================
// Probability Distributions - CDF
//=============================================================================

float nimcp_stats_cdf_standard_normal(float x) {
    return (float)(0.5 * (1.0 + nimcp_stats_erf(x / sqrt(2.0))));
}

float nimcp_stats_cdf_normal(float x, float mu, float sigma) {
    if (sigma <= 0.0f) return NAN;
    return nimcp_stats_cdf_standard_normal((x - mu) / sigma);
}

float nimcp_stats_cdf_exponential(float x, float lambda) {
    if (lambda <= 0.0f) return NAN;
    if (x < 0.0f) return 0.0f;
    return 1.0f - expf(-lambda * x);
}

float nimcp_stats_cdf_gamma(float x, float shape, float scale) {
    if (shape <= 0.0f || scale <= 0.0f) return NAN;
    if (x <= 0.0f) return 0.0f;
    return (float)nimcp_stats_gammainc(shape, x / scale);
}

float nimcp_stats_cdf_beta(float x, float alpha, float beta) {
    if (alpha <= 0.0f || beta <= 0.0f) return NAN;
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    return (float)nimcp_stats_betainc(x, alpha, beta);
}

float nimcp_stats_cdf_poisson(uint32_t k, float lambda) {
    if (lambda <= 0.0f) return NAN;
    return (float)(1.0 - nimcp_stats_gammainc(k + 1, lambda));
}

float nimcp_stats_cdf_binomial(uint32_t k, uint32_t n, float p) {
    if (p < 0.0f || p > 1.0f) return NAN;
    if (k >= n) return 1.0f;
    return (float)(1.0 - nimcp_stats_betainc(p, k + 1, n - k));
}

float nimcp_stats_cdf_student_t(float x, float df) {
    if (df <= 0.0f) return NAN;
    double t = x;
    double v = df;
    double p = nimcp_stats_betainc(v / (v + t * t), v / 2.0, 0.5);
    return (float)(t >= 0 ? 1.0 - 0.5 * p : 0.5 * p);
}

float nimcp_stats_cdf_chi_squared(float x, float df) {
    if (df <= 0.0f) return NAN;
    if (x <= 0.0f) return 0.0f;
    return (float)nimcp_stats_gammainc(df / 2.0, x / 2.0);
}

float nimcp_stats_cdf_f(float x, float df1, float df2) {
    if (df1 <= 0.0f || df2 <= 0.0f) return NAN;
    if (x <= 0.0f) return 0.0f;
    return (float)nimcp_stats_betainc(df1 * x / (df1 * x + df2), df1 / 2.0, df2 / 2.0);
}

float nimcp_stats_cdf(float x, const nimcp_distribution_params_t* params) {
    if (!params) return NAN;
    switch (params->type) {
        case NIMCP_DIST_NORMAL:
            return nimcp_stats_cdf_normal(x, params->params.normal.mu, params->params.normal.sigma);
        case NIMCP_DIST_EXPONENTIAL:
            return nimcp_stats_cdf_exponential(x, params->params.exponential.lambda);
        case NIMCP_DIST_GAMMA:
            return nimcp_stats_cdf_gamma(x, params->params.gamma.shape, params->params.gamma.scale);
        case NIMCP_DIST_BETA:
            return nimcp_stats_cdf_beta(x, params->params.beta.alpha, params->params.beta.beta);
        case NIMCP_DIST_STUDENT_T:
            return nimcp_stats_cdf_student_t(x, params->params.student_t.df);
        case NIMCP_DIST_CHI_SQUARED:
            return nimcp_stats_cdf_chi_squared(x, params->params.chi_squared.df);
        case NIMCP_DIST_F:
            return nimcp_stats_cdf_f(x, params->params.f.df1, params->params.f.df2);
        default:
            return NAN;
    }
}

//=============================================================================
// Probability Distributions - Quantile (Inverse CDF)
//=============================================================================

float nimcp_stats_quantile_standard_normal(float p) {
    if (p <= 0.0f) return -INFINITY;
    if (p >= 1.0f) return INFINITY;
    if (p == 0.5f) return 0.0f;

    // Wichura's AS241 algorithm - highly accurate rational approximation
    // Coefficients for the central region
    static const double a[] = {
        3.3871328727963666080e0,
        1.3314166764078193025e2,
        1.9715909503065514427e3,
        1.3731693765509461125e4,
        4.5921953931549871457e4,
        6.7265770927008700853e4,
        3.3430575583588128105e4,
        2.5090809287301226727e3
    };
    static const double b[] = {
        1.0,
        4.2313330701600911252e1,
        6.8718700749205790830e2,
        5.3941960214247511077e3,
        2.1213794301586595867e4,
        3.9307895513773136323e4,
        2.8729085735721942674e4,
        5.2264952788528545610e3
    };
    // Coefficients for the tail regions
    static const double c[] = {
        1.42343711074968357734e0,
        4.63033784615654529590e0,
        5.76949722146069140550e0,
        3.64784832476320460504e0,
        1.27045825245236838258e0,
        2.41780725177450611770e-1,
        2.27238449892691845833e-2,
        7.74545014278341407640e-4
    };
    static const double d[] = {
        1.0,
        2.05319162663775882187e0,
        1.67638483018380384940e0,
        6.89767334985100004550e-1,
        1.48103976427480074590e-1,
        1.51986665636164571966e-2,
        5.47593808499534494600e-4,
        1.05075007164441684324e-9
    };
    // Coefficients for the extreme tail
    static const double e[] = {
        6.65790464350110377720e0,
        5.46378491116411436990e0,
        1.78482653991729133580e0,
        2.96560571828504891230e-1,
        2.65321895265761230930e-2,
        1.24266094738807843860e-3,
        2.71155556874348757815e-5,
        2.01033439929228813265e-7
    };
    static const double f[] = {
        1.0,
        5.99832206555887937690e-1,
        1.36929880922735805310e-1,
        1.48753612908506148525e-2,
        7.86869131145613259100e-4,
        1.84631831751005468180e-5,
        1.42151175831644588870e-7,
        2.04426310338993978564e-15
    };

    double q = p - 0.5;
    double r, result;

    if (fabs(q) <= 0.425) {
        // Central region
        r = 0.180625 - q * q;
        result = q * (((((((a[7]*r + a[6])*r + a[5])*r + a[4])*r + a[3])*r + a[2])*r + a[1])*r + a[0]) /
                     (((((((b[7]*r + b[6])*r + b[5])*r + b[4])*r + b[3])*r + b[2])*r + b[1])*r + b[0]);
    } else {
        // Tail regions
        r = (q < 0) ? p : 1.0 - p;
        r = sqrt(-log(r));

        if (r <= 5.0) {
            // Intermediate region
            r = r - 1.6;
            result = (((((((c[7]*r + c[6])*r + c[5])*r + c[4])*r + c[3])*r + c[2])*r + c[1])*r + c[0]) /
                     (((((((d[7]*r + d[6])*r + d[5])*r + d[4])*r + d[3])*r + d[2])*r + d[1])*r + d[0]);
        } else {
            // Extreme tail
            r = r - 5.0;
            result = (((((((e[7]*r + e[6])*r + e[5])*r + e[4])*r + e[3])*r + e[2])*r + e[1])*r + e[0]) /
                     (((((((f[7]*r + f[6])*r + f[5])*r + f[4])*r + f[3])*r + f[2])*r + f[1])*r + f[0]);
        }

        if (q < 0) result = -result;
    }

    return (float)result;
}

float nimcp_stats_quantile_normal(float p, float mu, float sigma) {
    if (sigma <= 0.0f) return NAN;
    return mu + sigma * nimcp_stats_quantile_standard_normal(p);
}

float nimcp_stats_quantile_student_t(float p, float df) {
    if (df <= 0.0f || p <= 0.0f || p >= 1.0f) return NAN;
    if (p == 0.5f) return 0.0f;

    // Use bisection method for robustness
    // Set initial bounds based on probability
    double lo, hi;
    if (p < 0.5) {
        lo = -100.0;
        hi = 0.0;
    } else {
        lo = 0.0;
        hi = 100.0;
    }

    // Refine bounds
    while (nimcp_stats_cdf_student_t((float)lo, df) > p) lo *= 2;
    while (nimcp_stats_cdf_student_t((float)hi, df) < p) hi *= 2;

    // Bisection search
    for (int i = 0; i < 60; i++) {
        double mid = (lo + hi) / 2.0;
        double cdf = nimcp_stats_cdf_student_t((float)mid, df);
        if (fabs(cdf - p) < 1e-10) {
            return (float)mid;
        }
        if (cdf < p) {
            lo = mid;
        } else {
            hi = mid;
        }
        if (hi - lo < 1e-12) break;
    }

    return (float)((lo + hi) / 2.0);
}

float nimcp_stats_quantile_chi_squared(float p, float df) {
    if (df <= 0.0f || p < 0.0f || p > 1.0f) return NAN;
    if (p == 0.0f) return 0.0f;
    if (p == 1.0f) return INFINITY;

    // Initial guess from Wilson-Hilferty approximation
    double v = df;
    double z = nimcp_stats_quantile_standard_normal(p);
    double h = 2.0 / (9.0 * v);
    double x = v * pow(1.0 - h + z * sqrt(h), 3);
    if (x < 0) x = 0.01;

    // Newton-Raphson refinement
    for (int i = 0; i < 20; i++) {
        double cdf = nimcp_stats_cdf_chi_squared((float)x, df);
        double pdf = nimcp_stats_pdf_chi_squared((float)x, df);
        if (pdf < 1e-300) break;
        double dx = (cdf - p) / pdf;
        x -= dx;
        if (x < 0) x = 0.001;
        if (fabs(dx) < 1e-10 * x) break;
    }
    return (float)x;
}

float nimcp_stats_quantile_f(float p, float df1, float df2) {
    if (df1 <= 0.0f || df2 <= 0.0f || p < 0.0f || p > 1.0f) return NAN;
    if (p == 0.0f) return 0.0f;
    if (p == 1.0f) return INFINITY;

    // Use relationship with beta distribution
    double x = nimcp_stats_betainc(p, df1 / 2.0, df2 / 2.0);
    // Newton-Raphson to invert
    for (int i = 0; i < 20; i++) {
        double cdf = nimcp_stats_cdf_f((float)x, df1, df2);
        double pdf = nimcp_stats_pdf_f((float)x, df1, df2);
        if (pdf < 1e-300) break;
        double dx = (cdf - p) / pdf;
        x -= dx;
        if (x < 0) x = 0.001;
        if (fabs(dx) < 1e-10 * fabs(x) + 1e-15) break;
    }
    return (float)x;
}

float nimcp_stats_quantile_dist(float p, const nimcp_distribution_params_t* params) {
    if (!params) return NAN;
    switch (params->type) {
        case NIMCP_DIST_NORMAL:
            return nimcp_stats_quantile_normal(p, params->params.normal.mu, params->params.normal.sigma);
        case NIMCP_DIST_STUDENT_T:
            return nimcp_stats_quantile_student_t(p, params->params.student_t.df);
        case NIMCP_DIST_CHI_SQUARED:
            return nimcp_stats_quantile_chi_squared(p, params->params.chi_squared.df);
        case NIMCP_DIST_F:
            return nimcp_stats_quantile_f(p, params->params.f.df1, params->params.f.df2);
        default:
            return NAN;
    }
}

//=============================================================================
// Distribution Sampling (uses stdlib rand for now, integrate with nimcp_rand)
//=============================================================================

/* P2-U15: Use rand_r() with thread-local seed instead of thread-unsafe rand().
 * Multiple threads calling rand() simultaneously causes data races on the
 * internal state. rand_r() uses a caller-provided seed for thread safety. */
static _Thread_local unsigned int tl_rand_seed = 0;
static _Thread_local bool tl_rand_seeded = false;

static float rand_uniform(void) {
    if (!tl_rand_seeded) {
        tl_rand_seed = (unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)&tl_rand_seed;
        tl_rand_seeded = true;
    }
    return (float)rand_r(&tl_rand_seed) / (float)RAND_MAX;
}

static float rand_normal(void) {
    // Box-Muller transform
    float u1 = rand_uniform();
    float u2 = rand_uniform();
    while (u1 == 0.0f) u1 = rand_uniform();
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)PI * u2);
}

float nimcp_stats_sample(const nimcp_distribution_params_t* params) {
    if (!params) return NAN;

    switch (params->type) {
        case NIMCP_DIST_UNIFORM:
            return params->params.uniform.a +
                   rand_uniform() * (params->params.uniform.b - params->params.uniform.a);
        case NIMCP_DIST_NORMAL:
            return params->params.normal.mu + params->params.normal.sigma * rand_normal();
        case NIMCP_DIST_EXPONENTIAL: {
            float u = rand_uniform();
            while (u == 0.0f) u = rand_uniform();
            return -logf(u) / params->params.exponential.lambda;
        }
        case NIMCP_DIST_GAMMA: {
            // Marsaglia and Tsang's method
            float shape = params->params.gamma.shape;
            float scale = params->params.gamma.scale;
            if (shape < 1.0f) {
                return nimcp_stats_sample(&(nimcp_distribution_params_t){
                    .type = NIMCP_DIST_GAMMA,
                    .params.gamma = {shape + 1.0f, scale}
                }) * powf(rand_uniform(), 1.0f / shape);
            }
            float d = shape - 1.0f / 3.0f;
            float c = 1.0f / sqrtf(9.0f * d);
            while (1) {
                float x, v;
                do {
                    x = rand_normal();
                    v = 1.0f + c * x;
                } while (v <= 0.0f);
                v = v * v * v;
                float u = rand_uniform();
                if (u < 1.0f - 0.0331f * (x * x) * (x * x)) return d * v * scale;
                if (logf(u) < 0.5f * x * x + d * (1.0f - v + logf(v))) return d * v * scale;
            }
        }
        case NIMCP_DIST_BETA: {
            // Use gamma variates
            nimcp_distribution_params_t g1 = {.type = NIMCP_DIST_GAMMA,
                .params.gamma = {params->params.beta.alpha, 1.0f}};
            nimcp_distribution_params_t g2 = {.type = NIMCP_DIST_GAMMA,
                .params.gamma = {params->params.beta.beta, 1.0f}};
            float x = nimcp_stats_sample(&g1);
            float y = nimcp_stats_sample(&g2);
            return x / (x + y);
        }
        case NIMCP_DIST_POISSON: {
            float lambda = params->params.poisson.lambda;
            float L = expf(-lambda);
            uint32_t k = 0;
            float p = 1.0f;
            do {
                k++;
                p *= rand_uniform();
            } while (p > L);
            return (float)(k - 1);
        }
        default:
            return NAN;
    }
}

nimcp_stats_result_t nimcp_stats_sample_array(
    const nimcp_distribution_params_t* params,
    float* out,
    uint32_t n
) {
    if (!params || !out) return NIMCP_STATS_ERROR_NULL;
    for (uint32_t i = 0; i < n; i++) {
        out[i] = nimcp_stats_sample(params);
    }
    return NIMCP_STATS_OK;
}

//=============================================================================
// Hypothesis Testing - One Sample
//=============================================================================

nimcp_stats_result_t nimcp_stats_ttest_one_sample(
    const float* data,
    uint32_t n,
    float mu0,
    nimcp_test_type_t type,
    float confidence,
    nimcp_test_result_t* result
) {
    if (!data || !result) return NIMCP_STATS_ERROR_NULL;
    if (n < 2) return NIMCP_STATS_ERROR_SIZE;
    if (confidence <= 0.0f || confidence >= 1.0f) return NIMCP_STATS_ERROR_PARAMS;

    float mean = nimcp_stats_mean(data, n);
    float se = nimcp_stats_std_error(data, n);
    float df = (float)(n - 1);

    result->statistic = (mean - mu0) / se;
    result->df = df;
    result->confidence_level = confidence;
    result->type = type;

    // P-value calculation
    float cdf = nimcp_stats_cdf_student_t(result->statistic, df);
    switch (type) {
        case NIMCP_TEST_TWO_SIDED:
            result->p_value = 2.0f * fminf(cdf, 1.0f - cdf);
            break;
        case NIMCP_TEST_LEFT_TAIL:
            result->p_value = cdf;
            break;
        case NIMCP_TEST_RIGHT_TAIL:
            result->p_value = 1.0f - cdf;
            break;
    }

    // Confidence interval
    float alpha = 1.0f - confidence;
    float t_crit = nimcp_stats_quantile_student_t(1.0f - alpha / 2.0f, df);
    result->ci_lower = mean - t_crit * se;
    result->ci_upper = mean + t_crit * se;

    // Effect size (Cohen's d)
    result->effect_size = (mean - mu0) / nimcp_stats_std_dev(data, n);

    // Decision
    result->reject_null = (result->p_value < alpha);

    return NIMCP_STATS_OK;
}

nimcp_stats_result_t nimcp_stats_ztest_one_sample(
    const float* data,
    uint32_t n,
    float mu0,
    float sigma,
    nimcp_test_type_t type,
    float confidence,
    nimcp_test_result_t* result
) {
    if (!data || !result) return NIMCP_STATS_ERROR_NULL;
    if (n == 0 || sigma <= 0.0f) return NIMCP_STATS_ERROR_PARAMS;

    float mean = nimcp_stats_mean(data, n);
    float se = sigma / sqrtf((float)n);

    result->statistic = (mean - mu0) / se;
    result->df = INFINITY;
    result->confidence_level = confidence;
    result->type = type;

    float cdf = nimcp_stats_cdf_standard_normal(result->statistic);
    switch (type) {
        case NIMCP_TEST_TWO_SIDED:
            result->p_value = 2.0f * fminf(cdf, 1.0f - cdf);
            break;
        case NIMCP_TEST_LEFT_TAIL:
            result->p_value = cdf;
            break;
        case NIMCP_TEST_RIGHT_TAIL:
            result->p_value = 1.0f - cdf;
            break;
    }

    float alpha = 1.0f - confidence;
    float z_crit = nimcp_stats_quantile_standard_normal(1.0f - alpha / 2.0f);
    result->ci_lower = mean - z_crit * se;
    result->ci_upper = mean + z_crit * se;
    result->effect_size = (mean - mu0) / sigma;
    result->reject_null = (result->p_value < alpha);

    return NIMCP_STATS_OK;
}

//=============================================================================
// Hypothesis Testing - Two Sample
//=============================================================================

nimcp_stats_result_t nimcp_stats_ttest_two_sample(
    const float* data1,
    uint32_t n1,
    const float* data2,
    uint32_t n2,
    bool equal_var,
    nimcp_test_type_t type,
    float confidence,
    nimcp_test_result_t* result
) {
    if (!data1 || !data2 || !result) return NIMCP_STATS_ERROR_NULL;
    if (n1 < 2 || n2 < 2) return NIMCP_STATS_ERROR_SIZE;

    float mean1 = nimcp_stats_mean(data1, n1);
    float mean2 = nimcp_stats_mean(data2, n2);
    float var1 = nimcp_stats_variance(data1, n1);
    float var2 = nimcp_stats_variance(data2, n2);

    float se, df;

    if (equal_var) {
        // Pooled variance t-test
        float sp2 = ((n1 - 1) * var1 + (n2 - 1) * var2) / (n1 + n2 - 2);
        se = sqrtf(sp2 * (1.0f / n1 + 1.0f / n2));
        df = (float)(n1 + n2 - 2);
    } else {
        // Welch's t-test
        se = sqrtf(var1 / n1 + var2 / n2);
        float num = powf(var1 / n1 + var2 / n2, 2);
        float den = powf(var1 / n1, 2) / (n1 - 1) + powf(var2 / n2, 2) / (n2 - 1);
        df = num / den;
    }

    result->statistic = (mean1 - mean2) / se;
    result->df = df;
    result->confidence_level = confidence;
    result->type = type;

    float cdf = nimcp_stats_cdf_student_t(result->statistic, df);
    switch (type) {
        case NIMCP_TEST_TWO_SIDED:
            result->p_value = 2.0f * fminf(cdf, 1.0f - cdf);
            break;
        case NIMCP_TEST_LEFT_TAIL:
            result->p_value = cdf;
            break;
        case NIMCP_TEST_RIGHT_TAIL:
            result->p_value = 1.0f - cdf;
            break;
    }

    float alpha = 1.0f - confidence;
    float t_crit = nimcp_stats_quantile_student_t(1.0f - alpha / 2.0f, df);
    result->ci_lower = (mean1 - mean2) - t_crit * se;
    result->ci_upper = (mean1 - mean2) + t_crit * se;

    // Cohen's d (pooled standard deviation)
    float sp = sqrtf(((n1 - 1) * var1 + (n2 - 1) * var2) / (n1 + n2 - 2));
    result->effect_size = (mean1 - mean2) / sp;
    result->reject_null = (result->p_value < alpha);

    return NIMCP_STATS_OK;
}

nimcp_stats_result_t nimcp_stats_ttest_paired(
    const float* data1,
    const float* data2,
    uint32_t n,
    nimcp_test_type_t type,
    float confidence,
    nimcp_test_result_t* result
) {
    if (!data1 || !data2 || !result) return NIMCP_STATS_ERROR_NULL;
    if (n < 2) return NIMCP_STATS_ERROR_SIZE;

    float* diff = (float*)nimcp_malloc(n * sizeof(float));
    if (!diff) return NIMCP_STATS_ERROR_MEMORY;

    for (uint32_t i = 0; i < n; i++) {
        diff[i] = data1[i] - data2[i];
    }

    nimcp_stats_result_t res = nimcp_stats_ttest_one_sample(diff, n, 0.0f, type, confidence, result);
    nimcp_free(diff);
    return res;
}

nimcp_stats_result_t nimcp_stats_chi_squared_gof(
    const float* observed,
    const float* expected,
    uint32_t n,
    nimcp_test_result_t* result
) {
    if (!observed || !expected || !result) return NIMCP_STATS_ERROR_NULL;
    if (n < 2) return NIMCP_STATS_ERROR_SIZE;

    // Compute chi-squared statistic
    float chi_sq = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        if (expected[i] <= 0.0f) {
            return NIMCP_STATS_ERROR_PARAMS;  // Expected must be positive
        }
        float diff = observed[i] - expected[i];
        chi_sq += (diff * diff) / expected[i];
    }

    // Degrees of freedom = n - 1
    float df = (float)(n - 1);

    // Compute p-value using chi-squared CDF
    // Using incomplete gamma function approximation
    // P(X > chi_sq) = 1 - gamma(df/2, chi_sq/2) / Gamma(df/2)
    // Approximate using Wilson-Hilferty transformation for large df
    float p_value;
    if (df > 0) {
        // Wilson-Hilferty approximation: transform to approximately normal
        float k = df;
        float z = powf(chi_sq / k, 1.0f / 3.0f) - (1.0f - 2.0f / (9.0f * k));
        z /= sqrtf(2.0f / (9.0f * k));

        // Standard normal CDF approximation
        float t = 1.0f / (1.0f + 0.2316419f * fabsf(z));
        float d = 0.3989423f * expf(-z * z / 2.0f);
        float p = d * t * (0.3193815f + t * (-0.3565638f + t * (1.781478f + t * (-1.821256f + t * 1.330274f))));
        if (z > 0) {
            p_value = p;
        } else {
            p_value = 1.0f - p;
        }
    } else {
        p_value = 1.0f;
    }

    result->statistic = chi_sq;
    result->p_value = p_value;
    result->df = df;
    result->reject_null = (p_value < 0.05f);
    result->ci_lower = 0.0f;  // Not applicable for chi-squared
    result->ci_upper = 0.0f;

    return NIMCP_STATS_OK;
}

//=============================================================================
// Correlation Analysis
//=============================================================================

float nimcp_stats_covariance(const float* x, const float* y, uint32_t n) {
    if (!x || !y || n < 2) return NAN;

    float mean_x = nimcp_stats_mean(x, n);
    float mean_y = nimcp_stats_mean(y, n);

    float cov = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        cov += (x[i] - mean_x) * (y[i] - mean_y);
    }

    return cov / (n - 1);
}

nimcp_stats_result_t nimcp_stats_correlation_pearson(
    const float* x,
    const float* y,
    uint32_t n,
    nimcp_correlation_result_t* result
) {
    if (!x || !y || !result) return NIMCP_STATS_ERROR_NULL;
    if (n < 3) return NIMCP_STATS_ERROR_SIZE;

    float mean_x = nimcp_stats_mean(x, n);
    float mean_y = nimcp_stats_mean(y, n);

    float sum_xy = 0.0f, sum_x2 = 0.0f, sum_y2 = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float dx = x[i] - mean_x;
        float dy = y[i] - mean_y;
        sum_xy += dx * dy;
        sum_x2 += dx * dx;
        sum_y2 += dy * dy;
    }

    if (sum_x2 == 0.0f || sum_y2 == 0.0f) {
        result->r = 0.0f;
        result->p_value = 1.0f;
        return NIMCP_STATS_OK;
    }

    result->r = sum_xy / sqrtf(sum_x2 * sum_y2);
    result->r_squared = result->r * result->r;
    result->n = n;
    result->df = (float)(n - 2);

    // t-statistic for significance
    result->t_statistic = result->r * sqrtf(result->df / (1.0f - result->r_squared + 1e-10f));
    result->p_value = 2.0f * (1.0f - nimcp_stats_cdf_student_t(fabsf(result->t_statistic), result->df));

    // Fisher z-transformation for CI
    float z = 0.5f * logf((1.0f + result->r) / (1.0f - result->r + 1e-10f));
    float se_z = 1.0f / sqrtf((float)(n - 3));
    float z_crit = 1.96f;  // 95% CI
    float z_lower = z - z_crit * se_z;
    float z_upper = z + z_crit * se_z;
    result->ci_lower = tanhf(z_lower);
    result->ci_upper = tanhf(z_upper);

    return NIMCP_STATS_OK;
}

/**
 * @brief Helper to compute ranks for Spearman correlation
 */
static void compute_ranks(const float* data, float* ranks, uint32_t n) {
    // Create index array
    uint32_t* indices = (uint32_t*)nimcp_malloc(n * sizeof(uint32_t));
    /* P2-U16: NULL check - if allocation fails, fill ranks with sequential values
     * as a fallback rather than dereferencing NULL. */
    if (!indices) {
        for (uint32_t i = 0; i < n; i++) {
            ranks[i] = (float)(i + 1);
        }
        return;
    }
    for (uint32_t i = 0; i < n; i++) {
        indices[i] = i;
    }

    // Sort indices by data values (insertion sort for simplicity)
    for (uint32_t i = 1; i < n; i++) {
        uint32_t key = indices[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && data[indices[j]] > data[key]) {
            indices[j + 1] = indices[j];
            j--;
        }
        indices[j + 1] = key;
    }

    // Assign ranks (handling ties by averaging)
    uint32_t i = 0;
    while (i < n) {
        uint32_t j = i;
        // Find all tied values
        while (j < n - 1 && data[indices[j]] == data[indices[j + 1]]) {
            j++;
        }
        // Average rank for tied values
        float avg_rank = (float)(i + j) / 2.0f + 1.0f;
        for (uint32_t k = i; k <= j; k++) {
            ranks[indices[k]] = avg_rank;
        }
        i = j + 1;
    }

    nimcp_free(indices);
}

nimcp_stats_result_t nimcp_stats_correlation_spearman(
    const float* x,
    const float* y,
    uint32_t n,
    nimcp_correlation_result_t* result
) {
    if (!x || !y || !result) return NIMCP_STATS_ERROR_NULL;
    if (n < 3) return NIMCP_STATS_ERROR_SIZE;

    // Allocate rank arrays
    float* rank_x = (float*)nimcp_malloc(n * sizeof(float));
    float* rank_y = (float*)nimcp_malloc(n * sizeof(float));
    if (!rank_x || !rank_y) {
        nimcp_free(rank_x);
        nimcp_free(rank_y);
        return NIMCP_STATS_ERROR_MEMORY;
    }

    // Compute ranks
    compute_ranks(x, rank_x, n);
    compute_ranks(y, rank_y, n);

    // Compute Pearson correlation on ranks (this is Spearman's rho)
    float mean_rx = 0.0f, mean_ry = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        mean_rx += rank_x[i];
        mean_ry += rank_y[i];
    }
    mean_rx /= n;
    mean_ry /= n;

    float sum_xy = 0.0f, sum_x2 = 0.0f, sum_y2 = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float dx = rank_x[i] - mean_rx;
        float dy = rank_y[i] - mean_ry;
        sum_xy += dx * dy;
        sum_x2 += dx * dx;
        sum_y2 += dy * dy;
    }

    nimcp_free(rank_x);
    nimcp_free(rank_y);

    if (sum_x2 == 0.0f || sum_y2 == 0.0f) {
        result->r = 0.0f;
        result->p_value = 1.0f;
        result->r_squared = 0.0f;
        result->n = n;
        result->df = (float)(n - 2);
        result->t_statistic = 0.0f;
        result->ci_lower = 0.0f;
        result->ci_upper = 0.0f;
        return NIMCP_STATS_OK;
    }

    result->r = sum_xy / sqrtf(sum_x2 * sum_y2);
    result->r_squared = result->r * result->r;
    result->n = n;
    result->df = (float)(n - 2);

    // t-statistic for significance (same as Pearson)
    result->t_statistic = result->r * sqrtf(result->df / (1.0f - result->r_squared + 1e-10f));
    result->p_value = 2.0f * (1.0f - nimcp_stats_cdf_student_t(fabsf(result->t_statistic), result->df));

    // Fisher z-transformation for CI
    float z = 0.5f * logf((1.0f + result->r) / (1.0f - result->r + 1e-10f));
    float se_z = 1.0f / sqrtf((float)(n - 3));
    float z_crit = 1.96f;  // 95% CI
    float z_lower = z - z_crit * se_z;
    float z_upper = z + z_crit * se_z;
    result->ci_lower = tanhf(z_lower);
    result->ci_upper = tanhf(z_upper);

    return NIMCP_STATS_OK;
}

nimcp_stats_result_t nimcp_stats_covariance_matrix(
    const float* data,
    uint32_t n_obs,
    uint32_t n_vars,
    float* cov_matrix
) {
    if (!data || !cov_matrix) return NIMCP_STATS_ERROR_NULL;
    if (n_obs < 2 || n_vars == 0) return NIMCP_STATS_ERROR_SIZE;

    // Compute means
    float* means = (float*)nimcp_malloc(n_vars * sizeof(float));
    if (!means) return NIMCP_STATS_ERROR_MEMORY;

    for (uint32_t j = 0; j < n_vars; j++) {
        means[j] = 0.0f;
        for (uint32_t i = 0; i < n_obs; i++) {
            means[j] += data[i * n_vars + j];
        }
        means[j] /= n_obs;
    }

    // Compute covariance matrix
    for (uint32_t j = 0; j < n_vars; j++) {
        for (uint32_t k = j; k < n_vars; k++) {
            float cov = 0.0f;
            for (uint32_t i = 0; i < n_obs; i++) {
                cov += (data[i * n_vars + j] - means[j]) * (data[i * n_vars + k] - means[k]);
            }
            cov /= (n_obs - 1);
            cov_matrix[j * n_vars + k] = cov;
            cov_matrix[k * n_vars + j] = cov;  // Symmetric
        }
    }

    nimcp_free(means);
    return NIMCP_STATS_OK;
}

//=============================================================================
// Regression Analysis
//=============================================================================

nimcp_stats_result_t nimcp_stats_regression_linear(
    const float* x,
    const float* y,
    uint32_t n,
    nimcp_regression_result_t* result
) {
    if (!x || !y || !result) return NIMCP_STATS_ERROR_NULL;
    if (n < 3) return NIMCP_STATS_ERROR_SIZE;

    memset(result, 0, sizeof(nimcp_regression_result_t));

    float mean_x = nimcp_stats_mean(x, n);
    float mean_y = nimcp_stats_mean(y, n);

    float ss_xx = 0.0f, ss_xy = 0.0f, ss_yy = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float dx = x[i] - mean_x;
        float dy = y[i] - mean_y;
        ss_xx += dx * dx;
        ss_xy += dx * dy;
        ss_yy += dy * dy;
    }

    if (ss_xx == 0.0f) return NIMCP_STATS_ERROR_SINGULAR;

    result->slope = ss_xy / ss_xx;
    result->intercept = mean_y - result->slope * mean_x;

    // Residual sum of squares
    float ss_res = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float y_pred = result->intercept + result->slope * x[i];
        float res = y[i] - y_pred;
        ss_res += res * res;
    }

    float ss_tot = ss_yy;
    result->r_squared = 1.0f - ss_res / (ss_tot + 1e-10f);
    result->adj_r_squared = 1.0f - (1.0f - result->r_squared) * (n - 1) / (n - 2);
    result->std_error = sqrtf(ss_res / (n - 2));

    // F-statistic
    float ss_reg = ss_tot - ss_res;
    result->f_statistic = (ss_reg / 1) / (ss_res / (n - 2));
    result->p_value = 1.0f - nimcp_stats_cdf_f(result->f_statistic, 1.0f, (float)(n - 2));

    result->n_coefficients = 2;

    // AIC and BIC (k = 2 parameters: intercept + slope)
    float k = 2.0f;
    float mse = ss_res / n;
    if (mse > 0.0f) {
        result->aic = n * logf(mse) + 2.0f * k;
        result->bic = n * logf(mse) + k * logf((float)n);
    }

    // Durbin-Watson statistic for autocorrelation
    float sum_diff_sq = 0.0f;
    float prev_res = (y[0] - (result->intercept + result->slope * x[0]));
    for (uint32_t i = 1; i < n; i++) {
        float y_pred = result->intercept + result->slope * x[i];
        float res = y[i] - y_pred;
        float diff = res - prev_res;
        sum_diff_sq += diff * diff;
        prev_res = res;
    }
    result->durbin_watson = (ss_res > 0.0f) ? sum_diff_sq / ss_res : 2.0f;

    return NIMCP_STATS_OK;
}

void nimcp_stats_regression_nimcp_free(nimcp_regression_result_t* result) {
    if (!result) return;
    if (result->coefficients) nimcp_free(result->coefficients);
    if (result->se_coefficients) nimcp_free(result->se_coefficients);
    if (result->t_statistics) nimcp_free(result->t_statistics);
    if (result->p_values) nimcp_free(result->p_values);
    memset(result, 0, sizeof(nimcp_regression_result_t));
}

//=============================================================================
// Information Theory
//=============================================================================

float nimcp_stats_entropy(const float* probabilities, uint32_t n) {
    if (!probabilities || n == 0) return NAN;

    float h = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        if (probabilities[i] > 0.0f) {
            h -= probabilities[i] * log2f(probabilities[i]);
        }
    }
    return h;
}

float nimcp_stats_entropy_nats(const float* probabilities, uint32_t n) {
    if (!probabilities || n == 0) return NAN;

    float h = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        if (probabilities[i] > 0.0f) {
            h -= probabilities[i] * logf(probabilities[i]);
        }
    }
    return h;
}

float nimcp_stats_joint_entropy(const float* joint_prob, uint32_t n1, uint32_t n2) {
    if (!joint_prob || n1 == 0 || n2 == 0) return NAN;

    float h = 0.0f;
    uint32_t total = n1 * n2;
    for (uint32_t i = 0; i < total; i++) {
        if (joint_prob[i] > 0.0f) {
            h -= joint_prob[i] * log2f(joint_prob[i]);
        }
    }
    return h;
}

float nimcp_stats_conditional_entropy(const float* joint_prob, uint32_t n_x, uint32_t n_y) {
    if (!joint_prob || n_x == 0 || n_y == 0) return NAN;

    // H(Y|X) = H(X,Y) - H(X)
    float h_xy = nimcp_stats_joint_entropy(joint_prob, n_x, n_y);

    // Compute marginal P(X)
    float* p_x = (float*)nimcp_calloc(n_x, sizeof(float));
    if (!p_x) return NAN;

    for (uint32_t i = 0; i < n_x; i++) {
        for (uint32_t j = 0; j < n_y; j++) {
            p_x[i] += joint_prob[i * n_y + j];
        }
    }

    float h_x = nimcp_stats_entropy(p_x, n_x);
    nimcp_free(p_x);

    return h_xy - h_x;
}

float nimcp_stats_kl_divergence(const float* p, const float* q, uint32_t n) {
    if (!p || !q || n == 0) return NAN;

    float kl = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        if (p[i] > 0.0f) {
            if (q[i] <= 0.0f) return INFINITY;
            kl += p[i] * log2f(p[i] / q[i]);
        }
    }
    return kl;
}

float nimcp_stats_js_divergence(const float* p, const float* q, uint32_t n) {
    if (!p || !q || n == 0) return NAN;

    float* m = (float*)nimcp_malloc(n * sizeof(float));
    if (!m) return NAN;

    for (uint32_t i = 0; i < n; i++) {
        m[i] = 0.5f * (p[i] + q[i]);
    }

    float js = 0.5f * nimcp_stats_kl_divergence(p, m, n) +
               0.5f * nimcp_stats_kl_divergence(q, m, n);

    nimcp_free(m);
    return js;
}

float nimcp_stats_cross_entropy(const float* p, const float* q, uint32_t n) {
    if (!p || !q || n == 0) return NAN;

    float ce = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        if (p[i] > 0.0f) {
            if (q[i] <= 0.0f) return INFINITY;
            ce -= p[i] * log2f(q[i]);
        }
    }
    return ce;
}

float nimcp_stats_mutual_information(const float* joint_prob, uint32_t n_x, uint32_t n_y) {
    if (!joint_prob || n_x == 0 || n_y == 0) return NAN;

    // Compute marginals
    float* p_x = (float*)nimcp_calloc(n_x, sizeof(float));
    float* p_y = (float*)nimcp_calloc(n_y, sizeof(float));
    if (!p_x || !p_y) {
        nimcp_free(p_x);
        nimcp_free(p_y);
        return NAN;
    }

    for (uint32_t i = 0; i < n_x; i++) {
        for (uint32_t j = 0; j < n_y; j++) {
            p_x[i] += joint_prob[i * n_y + j];
            p_y[j] += joint_prob[i * n_y + j];
        }
    }

    // Compute MI
    float mi = 0.0f;
    for (uint32_t i = 0; i < n_x; i++) {
        for (uint32_t j = 0; j < n_y; j++) {
            float pxy = joint_prob[i * n_y + j];
            if (pxy > 0.0f && p_x[i] > 0.0f && p_y[j] > 0.0f) {
                mi += pxy * log2f(pxy / (p_x[i] * p_y[j]));
            }
        }
    }

    nimcp_free(p_x);
    nimcp_free(p_y);
    return mi;
}

//=============================================================================
// Bayesian Inference
//=============================================================================

nimcp_stats_result_t nimcp_stats_bayesian_beta_binomial(
    float prior_alpha,
    float prior_beta,
    uint32_t successes,
    uint32_t trials,
    float credible_level,
    nimcp_bayesian_result_t* result
) {
    if (!result) return NIMCP_STATS_ERROR_NULL;
    if (prior_alpha <= 0.0f || prior_beta <= 0.0f) return NIMCP_STATS_ERROR_PARAMS;

    float post_alpha = prior_alpha + successes;
    float post_beta = prior_beta + (trials - successes);

    result->posterior_mean = post_alpha / (post_alpha + post_beta);
    result->posterior_variance = (post_alpha * post_beta) /
        (powf(post_alpha + post_beta, 2) * (post_alpha + post_beta + 1));
    result->posterior_mode = (post_alpha > 1 && post_beta > 1) ?
        (post_alpha - 1) / (post_alpha + post_beta - 2) : result->posterior_mean;
    result->credible_level = credible_level;

    // Credible interval using beta quantiles via bisection (more robust than Newton-Raphson)
    float alpha_lower = (1.0f - credible_level) / 2.0f;
    float alpha_upper = 1.0f - alpha_lower;

    // Find lower quantile using bisection
    float lo = 0.0f, hi = result->posterior_mean;
    for (int iter = 0; iter < 50; iter++) {
        float mid = (lo + hi) / 2.0f;
        float cdf = nimcp_stats_cdf_beta(mid, post_alpha, post_beta);
        if (cdf < alpha_lower) {
            lo = mid;
        } else {
            hi = mid;
        }
        if (hi - lo < 1e-6f) break;
    }
    result->credible_lower = (lo + hi) / 2.0f;

    // Find upper quantile using bisection
    lo = result->posterior_mean;
    hi = 1.0f;
    for (int iter = 0; iter < 50; iter++) {
        float mid = (lo + hi) / 2.0f;
        float cdf = nimcp_stats_cdf_beta(mid, post_alpha, post_beta);
        if (cdf < alpha_upper) {
            lo = mid;
        } else {
            hi = mid;
        }
        if (hi - lo < 1e-6f) break;
    }
    result->credible_upper = (lo + hi) / 2.0f;

    return NIMCP_STATS_OK;
}

nimcp_stats_result_t nimcp_stats_bayesian_normal(
    float prior_mean,
    float prior_variance,
    const float* data,
    uint32_t n,
    float known_variance,
    float credible_level,
    nimcp_bayesian_result_t* result
) {
    if (!result) return NIMCP_STATS_ERROR_NULL;
    if (!data && n > 0) return NIMCP_STATS_ERROR_NULL;
    if (prior_variance <= 0.0f || known_variance <= 0.0f) return NIMCP_STATS_ERROR_PARAMS;
    if (credible_level <= 0.0f || credible_level >= 1.0f) return NIMCP_STATS_ERROR_PARAMS;

    // Compute sample mean
    float sample_mean = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sample_mean += data[i];
    }
    sample_mean = (n > 0) ? sample_mean / n : 0.0f;

    // Posterior precision = prior precision + n / known_variance
    float prior_precision = 1.0f / prior_variance;
    float data_precision = (float)n / known_variance;
    float post_precision = prior_precision + data_precision;
    float post_variance = 1.0f / post_precision;

    // Posterior mean = weighted average of prior mean and sample mean
    float post_mean = (prior_precision * prior_mean + data_precision * sample_mean) / post_precision;

    result->posterior_mean = post_mean;
    result->posterior_variance = post_variance;
    result->posterior_mode = post_mean;  // Normal is symmetric
    result->credible_level = credible_level;

    // Credible interval using normal quantiles
    float alpha = (1.0f - credible_level) / 2.0f;
    float z = nimcp_stats_quantile_normal(1.0f - alpha, 0.0f, 1.0f);
    float post_std = sqrtf(post_variance);

    result->credible_lower = post_mean - z * post_std;
    result->credible_upper = post_mean + z * post_std;

    return NIMCP_STATS_OK;
}

nimcp_stats_result_t nimcp_stats_bayesian_gamma_poisson(
    float prior_shape,
    float prior_rate,
    uint32_t events,
    float exposure,
    float credible_level,
    nimcp_bayesian_result_t* result
) {
    if (!result) return NIMCP_STATS_ERROR_NULL;
    if (prior_shape <= 0.0f || prior_rate <= 0.0f) return NIMCP_STATS_ERROR_PARAMS;
    if (exposure < 0.0f) return NIMCP_STATS_ERROR_PARAMS;
    if (credible_level <= 0.0f || credible_level >= 1.0f) return NIMCP_STATS_ERROR_PARAMS;

    // Posterior parameters: Gamma(shape + events, rate + exposure)
    float post_shape = prior_shape + (float)events;
    float post_rate = prior_rate + exposure;

    // Gamma distribution moments
    result->posterior_mean = post_shape / post_rate;
    result->posterior_variance = post_shape / (post_rate * post_rate);
    result->posterior_mode = (post_shape >= 1.0f) ? (post_shape - 1.0f) / post_rate : 0.0f;
    result->credible_level = credible_level;

    // Approximate credible interval using gamma quantiles
    // Using chi-square relationship: 2*rate*X ~ chi^2(2*shape) where X ~ Gamma(shape, rate)
    float alpha = (1.0f - credible_level) / 2.0f;

    // Lower bound: chi^2(2*shape, alpha) / (2 * rate)
    // Upper bound: chi^2(2*shape, 1-alpha) / (2 * rate)
    // Approximate using normal approximation for large shape
    if (post_shape > 10.0f) {
        float z_low = nimcp_stats_quantile_normal(alpha, 0.0f, 1.0f);
        float z_high = nimcp_stats_quantile_normal(1.0f - alpha, 0.0f, 1.0f);
        float std = sqrtf(result->posterior_variance);
        result->credible_lower = fmaxf(0.0f, result->posterior_mean + z_low * std);
        result->credible_upper = result->posterior_mean + z_high * std;
    } else {
        // Simple approximation for small shape
        float cv = 1.0f / sqrtf(post_shape);  // Coefficient of variation
        result->credible_lower = result->posterior_mean * (1.0f - 2.0f * cv);
        result->credible_upper = result->posterior_mean * (1.0f + 2.0f * cv);
        if (result->credible_lower < 0.0f) result->credible_lower = 0.0f;
    }

    return NIMCP_STATS_OK;
}

float nimcp_stats_bayes_factor(float log_ml_model1, float log_ml_model2) {
    return expf(log_ml_model1 - log_ml_model2);
}

//=============================================================================
// Utility Functions
//=============================================================================

nimcp_stats_result_t nimcp_stats_standardize(const float* data, uint32_t n, float* out) {
    if (!data || !out) return NIMCP_STATS_ERROR_NULL;
    if (n < 2) return NIMCP_STATS_ERROR_SIZE;

    float mean = nimcp_stats_mean(data, n);
    float std = nimcp_stats_std_dev(data, n);

    if (std == 0.0f) {
        for (uint32_t i = 0; i < n; i++) out[i] = 0.0f;
    } else {
        for (uint32_t i = 0; i < n; i++) {
            out[i] = (data[i] - mean) / std;
        }
    }

    return NIMCP_STATS_OK;
}

nimcp_stats_result_t nimcp_stats_normalize_minmax(const float* data, uint32_t n, float* out) {
    if (!data || !out) return NIMCP_STATS_ERROR_NULL;
    if (n == 0) return NIMCP_STATS_ERROR_SIZE;

    float min_val = nimcp_stats_min(data, n);
    float max_val = nimcp_stats_max(data, n);
    float range = max_val - min_val;

    if (range == 0.0f) {
        for (uint32_t i = 0; i < n; i++) out[i] = 0.5f;
    } else {
        for (uint32_t i = 0; i < n; i++) {
            out[i] = (data[i] - min_val) / range;
        }
    }

    return NIMCP_STATS_OK;
}

nimcp_stats_result_t nimcp_stats_detect_outliers_iqr(
    const float* data,
    uint32_t n,
    float k,
    bool* outlier_mask,
    uint32_t* n_outliers
) {
    if (!data || !outlier_mask || !n_outliers) return NIMCP_STATS_ERROR_NULL;
    if (n < 4) return NIMCP_STATS_ERROR_SIZE;

    float q1 = nimcp_stats_quantile(data, n, 0.25f);
    float q3 = nimcp_stats_quantile(data, n, 0.75f);
    float iqr = q3 - q1;

    float lower = q1 - k * iqr;
    float upper = q3 + k * iqr;

    *n_outliers = 0;
    for (uint32_t i = 0; i < n; i++) {
        outlier_mask[i] = (data[i] < lower || data[i] > upper);
        if (outlier_mask[i]) (*n_outliers)++;
    }

    return NIMCP_STATS_OK;
}

nimcp_stats_result_t nimcp_stats_detect_outliers_zscore(
    const float* data,
    uint32_t n,
    float threshold,
    bool* outlier_mask,
    uint32_t* n_outliers
) {
    if (!data || !outlier_mask || !n_outliers) return NIMCP_STATS_ERROR_NULL;
    if (n < 2) return NIMCP_STATS_ERROR_SIZE;

    float mean = nimcp_stats_mean(data, n);
    float std = nimcp_stats_std_dev(data, n);

    *n_outliers = 0;
    if (std == 0.0f) {
        for (uint32_t i = 0; i < n; i++) outlier_mask[i] = false;
        return NIMCP_STATS_OK;
    }

    for (uint32_t i = 0; i < n; i++) {
        float z = fabsf((data[i] - mean) / std);
        outlier_mask[i] = (z > threshold);
        if (outlier_mask[i]) (*n_outliers)++;
    }

    return NIMCP_STATS_OK;
}

//=============================================================================
// Error Handling
//=============================================================================

const char* nimcp_stats_error_string(nimcp_stats_result_t result) {
    switch (result) {
        case NIMCP_STATS_OK: return "Success";
        case NIMCP_STATS_ERROR_NULL: return "NULL pointer argument";
        case NIMCP_STATS_ERROR_SIZE: return "Invalid size (n=0 or too small)";
        case NIMCP_STATS_ERROR_MEMORY: return "Memory allocation failed";
        case NIMCP_STATS_ERROR_PARAMS: return "Invalid distribution parameters";
        case NIMCP_STATS_ERROR_CONVERGE: return "Algorithm did not converge";
        case NIMCP_STATS_ERROR_SINGULAR: return "Singular matrix in computation";
        case NIMCP_STATS_ERROR_RANGE: return "Value out of valid range";
        case NIMCP_STATS_ERROR_NOT_INIT: return "Module not initialized";
        default: return "Unknown error";
    }
}

//=============================================================================
// Shannon Module Integration
//=============================================================================

float nimcp_stats_channel_capacity(float bandwidth, float snr) {
    if (bandwidth < 0.0f || snr < 0.0f) return NAN;  // Invalid params
    if (bandwidth == 0.0f) return 0.0f;  // Zero bandwidth = zero capacity
    // Shannon-Hartley theorem: C = B × log₂(1 + SNR)
    return bandwidth * log2f(1.0f + snr);
}

float nimcp_stats_snr_from_db(float snr_db) {
    // SNR_linear = 10^(SNR_dB / 10)
    return powf(10.0f, snr_db / 10.0f);
}

float nimcp_stats_snr_to_db(float snr) {
    if (snr <= 0.0f) return -INFINITY;
    // SNR_dB = 10 × log₁₀(SNR_linear)
    return 10.0f * log10f(snr);
}

float nimcp_stats_variation_of_information(
    const float* joint_prob,
    uint32_t n_x,
    uint32_t n_y
) {
    if (!joint_prob || n_x == 0 || n_y == 0) return NAN;

    // VI = H(X,Y) - I(X;Y) = H(X|Y) + H(Y|X)
    float h_xy = nimcp_stats_joint_entropy(joint_prob, n_x, n_y);
    float mi = nimcp_stats_mutual_information(joint_prob, n_x, n_y);

    return h_xy - mi;
}

// Helper: discretize continuous data into bins
static void discretize_to_bins(const float* data, uint32_t n, uint32_t n_bins, uint32_t* bins) {
    float min_val = data[0], max_val = data[0];
    for (uint32_t i = 1; i < n; i++) {
        if (data[i] < min_val) min_val = data[i];
        if (data[i] > max_val) max_val = data[i];
    }

    float range = max_val - min_val;
    if (range == 0.0f) {
        for (uint32_t i = 0; i < n; i++) bins[i] = 0;
        return;
    }

    for (uint32_t i = 0; i < n; i++) {
        uint32_t b = (uint32_t)((data[i] - min_val) / range * (n_bins - 1));
        if (b >= n_bins) b = n_bins - 1;
        bins[i] = b;
    }
}

float nimcp_stats_transfer_entropy(
    const float* x,
    const float* y,
    uint32_t n,
    uint32_t k,
    uint32_t n_bins
) {
    if (!x || !y || n == 0 || k == 0 || n_bins == 0) return NAN;
    if (n <= k + 1) return NAN;

    // Discretize time series
    uint32_t* x_bins = (uint32_t*)nimcp_malloc(n * sizeof(uint32_t));
    uint32_t* y_bins = (uint32_t*)nimcp_malloc(n * sizeof(uint32_t));
    if (!x_bins || !y_bins) {
        nimcp_free(x_bins);
        nimcp_free(y_bins);
        return NAN;
    }

    discretize_to_bins(x, n, n_bins, x_bins);
    discretize_to_bins(y, n, n_bins, y_bins);

    // Count joint occurrences for T(X→Y)
    // We need: P(y_t, y_past, x_past), P(y_past, x_past), P(y_t, y_past), P(y_past)
    // Simplified: use k=1 embedding for efficiency

    uint32_t n_samples = n - k;
    uint32_t joint_size = n_bins * n_bins * n_bins;  // y_t, y_past, x_past
    uint32_t* counts_yyx = (uint32_t*)nimcp_calloc(joint_size, sizeof(uint32_t));
    uint32_t* counts_yx = (uint32_t*)nimcp_calloc(n_bins * n_bins, sizeof(uint32_t));
    uint32_t* counts_yy = (uint32_t*)nimcp_calloc(n_bins * n_bins, sizeof(uint32_t));
    uint32_t* counts_y = (uint32_t*)nimcp_calloc(n_bins, sizeof(uint32_t));

    if (!counts_yyx || !counts_yx || !counts_yy || !counts_y) {
        nimcp_free(x_bins); nimcp_free(y_bins);
        nimcp_free(counts_yyx); nimcp_free(counts_yx); nimcp_free(counts_yy); nimcp_free(counts_y);
        return NAN;
    }

    // Build count tables
    for (uint32_t t = k; t < n; t++) {
        uint32_t y_t = y_bins[t];
        uint32_t y_past = y_bins[t - 1];  // k=1 simplification
        uint32_t x_past = x_bins[t - 1];

        counts_yyx[y_t * n_bins * n_bins + y_past * n_bins + x_past]++;
        counts_yx[y_past * n_bins + x_past]++;
        counts_yy[y_t * n_bins + y_past]++;
        counts_y[y_past]++;
    }

    // Compute transfer entropy
    // T(X→Y) = Σ P(y_t, y_past, x_past) * log2(P(y_t|y_past,x_past) / P(y_t|y_past))
    float te = 0.0f;
    float inv_n = 1.0f / n_samples;

    for (uint32_t y_t = 0; y_t < n_bins; y_t++) {
        for (uint32_t y_past = 0; y_past < n_bins; y_past++) {
            for (uint32_t x_past = 0; x_past < n_bins; x_past++) {
                uint32_t c_yyx = counts_yyx[y_t * n_bins * n_bins + y_past * n_bins + x_past];
                if (c_yyx == 0) continue;

                uint32_t c_yx = counts_yx[y_past * n_bins + x_past];
                uint32_t c_yy = counts_yy[y_t * n_bins + y_past];
                uint32_t c_y = counts_y[y_past];

                if (c_yx == 0 || c_yy == 0 || c_y == 0) continue;

                // P(y_t|y_past,x_past) = P(y_t,y_past,x_past) / P(y_past,x_past)
                // P(y_t|y_past) = P(y_t,y_past) / P(y_past)
                float p_yyx = c_yyx * inv_n;
                float p_cond_full = (float)c_yyx / c_yx;
                float p_cond_reduced = (float)c_yy / c_y;

                if (p_cond_reduced > 0.0f) {
                    te += p_yyx * log2f(p_cond_full / p_cond_reduced);
                }
            }
        }
    }

    nimcp_free(x_bins); nimcp_free(y_bins);
    nimcp_free(counts_yyx); nimcp_free(counts_yx); nimcp_free(counts_yy); nimcp_free(counts_y);

    return fmaxf(0.0f, te);  // TE should be non-negative
}

float nimcp_stats_effective_information(
    const float* tpm,
    uint32_t n_states
) {
    if (!tpm || n_states == 0) return NAN;

    // EI = Determinism + Degeneracy
    // Determinism: How much outputs constrain inputs (average conditional entropy)
    // Degeneracy: How different inputs lead to same outputs

    // Uniform input distribution
    float p_uniform = 1.0f / n_states;

    // Compute output distribution (marginal over rows assuming uniform input)
    float* p_output = (float*)nimcp_calloc(n_states, sizeof(float));
    if (!p_output) return NAN;

    for (uint32_t i = 0; i < n_states; i++) {
        for (uint32_t j = 0; j < n_states; j++) {
            p_output[j] += p_uniform * tpm[i * n_states + j];
        }
    }

    // H(output) - entropy of output distribution
    float h_output = nimcp_stats_entropy(p_output, n_states);

    // H(output|input) - average conditional entropy
    float h_cond = 0.0f;
    for (uint32_t i = 0; i < n_states; i++) {
        // Row i of TPM is P(output|input=i)
        float h_row = nimcp_stats_entropy(&tpm[i * n_states], n_states);
        h_cond += p_uniform * h_row;
    }

    nimcp_free(p_output);

    // EI = H(output) - H(output|input) = I(input; output)
    return h_output - h_cond;
}

float nimcp_stats_information_integration(
    const float* cov_matrix,
    uint32_t n_vars
) {
    if (!cov_matrix || n_vars < 2) return NAN;

    // Simplified Phi measure based on mutual information
    // Phi ≈ I(all vars) - Σ I(individual vars with rest)

    // For Gaussian variables, MI relates to determinant ratio
    // I(X;Y) = 0.5 * log(|Cov_X| * |Cov_Y| / |Cov_XY|)

    // Compute log-determinant of full covariance (simplified: sum of log variances for diagonal approx)
    float log_det_full = 0.0f;
    for (uint32_t i = 0; i < n_vars; i++) {
        float var_i = cov_matrix[i * n_vars + i];
        if (var_i <= 0.0f) return NAN;
        log_det_full += logf(var_i);
    }

    // Sum of entropies of individual variables
    float h_sum_individual = 0.0f;
    for (uint32_t i = 0; i < n_vars; i++) {
        float var_i = cov_matrix[i * n_vars + i];
        // Gaussian entropy: 0.5 * log(2πe * var)
        h_sum_individual += 0.5f * logf(2.0f * (float)PI * (float)M_E * var_i);
    }

    // Joint entropy (approximation using diagonal)
    float h_joint = 0.0f;
    for (uint32_t i = 0; i < n_vars; i++) {
        float var_i = cov_matrix[i * n_vars + i];
        h_joint += 0.5f * logf(2.0f * (float)PI * (float)M_E * var_i);
    }

    // Account for correlations (off-diagonal terms reduce joint entropy)
    float corr_sum = 0.0f;
    for (uint32_t i = 0; i < n_vars; i++) {
        for (uint32_t j = i + 1; j < n_vars; j++) {
            float cov_ij = cov_matrix[i * n_vars + j];
            float var_i = cov_matrix[i * n_vars + i];
            float var_j = cov_matrix[j * n_vars + j];
            float rho = cov_ij / sqrtf(var_i * var_j);
            if (fabsf(rho) < 1.0f) {
                corr_sum += -0.5f * logf(1.0f - rho * rho);
            }
        }
    }

    // Phi approximation: total correlation = sum of individual entropies - joint entropy
    float phi = h_sum_individual - h_joint + corr_sum;

    return fmaxf(0.0f, phi);
}

float nimcp_stats_information_bottleneck(
    const float* joint_xy,
    uint32_t n_x,
    uint32_t n_y,
    uint32_t n_t,
    float beta,
    float* q_t_given_x,
    uint32_t max_iter
) {
    if (!joint_xy || !q_t_given_x || n_x == 0 || n_y == 0 || n_t == 0) return NAN;
    if (beta <= 0.0f) return NAN;

    // Compute marginals
    float* p_x = (float*)nimcp_calloc(n_x, sizeof(float));
    float* p_y = (float*)nimcp_calloc(n_y, sizeof(float));
    float* p_y_given_x = (float*)nimcp_malloc(n_x * n_y * sizeof(float));
    float* q_y_given_t = (float*)nimcp_calloc(n_t * n_y, sizeof(float));
    float* p_t = (float*)nimcp_calloc(n_t, sizeof(float));

    if (!p_x || !p_y || !p_y_given_x || !q_y_given_t || !p_t) {
        nimcp_free(p_x); nimcp_free(p_y); nimcp_free(p_y_given_x); nimcp_free(q_y_given_t); nimcp_free(p_t);
        return NAN;
    }

    // Compute p(x) and p(y)
    for (uint32_t i = 0; i < n_x; i++) {
        for (uint32_t j = 0; j < n_y; j++) {
            p_x[i] += joint_xy[i * n_y + j];
            p_y[j] += joint_xy[i * n_y + j];
        }
    }

    // Compute p(y|x)
    for (uint32_t i = 0; i < n_x; i++) {
        if (p_x[i] > 0.0f) {
            for (uint32_t j = 0; j < n_y; j++) {
                p_y_given_x[i * n_y + j] = joint_xy[i * n_y + j] / p_x[i];
            }
        }
    }

    // Initialize q(t|x) randomly (uniform + noise)
    for (uint32_t i = 0; i < n_x; i++) {
        float sum = 0.0f;
        for (uint32_t t = 0; t < n_t; t++) {
            q_t_given_x[i * n_t + t] = 1.0f / n_t + 0.01f * ((float)(i + t) / (n_x + n_t) - 0.5f);
            if (q_t_given_x[i * n_t + t] < 0.001f) q_t_given_x[i * n_t + t] = 0.001f;
            sum += q_t_given_x[i * n_t + t];
        }
        for (uint32_t t = 0; t < n_t; t++) {
            q_t_given_x[i * n_t + t] /= sum;
        }
    }

    // Blahut-Arimoto iterations
    for (uint32_t iter = 0; iter < max_iter; iter++) {
        // Compute p(t) = Σ_x p(x) q(t|x)
        for (uint32_t t = 0; t < n_t; t++) {
            p_t[t] = 0.0f;
            for (uint32_t i = 0; i < n_x; i++) {
                p_t[t] += p_x[i] * q_t_given_x[i * n_t + t];
            }
        }

        // Compute q(y|t) = Σ_x p(x|t) p(y|x) = Σ_x [p(x)q(t|x)/p(t)] p(y|x)
        for (uint32_t t = 0; t < n_t; t++) {
            for (uint32_t j = 0; j < n_y; j++) {
                q_y_given_t[t * n_y + j] = 0.0f;
                if (p_t[t] > 1e-10f) {
                    for (uint32_t i = 0; i < n_x; i++) {
                        float p_x_given_t = p_x[i] * q_t_given_x[i * n_t + t] / p_t[t];
                        q_y_given_t[t * n_y + j] += p_x_given_t * p_y_given_x[i * n_y + j];
                    }
                }
            }
        }

        // Update q(t|x) using IB formula
        for (uint32_t i = 0; i < n_x; i++) {
            float z = 0.0f;
            for (uint32_t t = 0; t < n_t; t++) {
                // D_KL(p(y|x) || q(y|t))
                float kl = 0.0f;
                for (uint32_t j = 0; j < n_y; j++) {
                    if (p_y_given_x[i * n_y + j] > 1e-10f && q_y_given_t[t * n_y + j] > 1e-10f) {
                        kl += p_y_given_x[i * n_y + j] *
                              logf(p_y_given_x[i * n_y + j] / q_y_given_t[t * n_y + j]);
                    }
                }
                q_t_given_x[i * n_t + t] = p_t[t] * expf(-beta * kl);
                z += q_t_given_x[i * n_t + t];
            }
            // Normalize
            if (z > 1e-10f) {
                for (uint32_t t = 0; t < n_t; t++) {
                    q_t_given_x[i * n_t + t] /= z;
                }
            }
        }
    }

    // Compute final I(T;Y) / I(X;Y)
    float mi_xy = nimcp_stats_mutual_information(joint_xy, n_x, n_y);

    // Build p(t,y) joint
    float* joint_ty = (float*)nimcp_calloc(n_t * n_y, sizeof(float));
    if (joint_ty) {
        for (uint32_t t = 0; t < n_t; t++) {
            for (uint32_t j = 0; j < n_y; j++) {
                for (uint32_t i = 0; i < n_x; i++) {
                    joint_ty[t * n_y + j] += p_x[i] * q_t_given_x[i * n_t + t] * p_y_given_x[i * n_y + j];
                }
            }
        }
    }

    float mi_ty = (joint_ty) ? nimcp_stats_mutual_information(joint_ty, n_t, n_y) : 0.0f;

    nimcp_free(p_x); nimcp_free(p_y); nimcp_free(p_y_given_x); nimcp_free(q_y_given_t); nimcp_free(p_t); nimcp_free(joint_ty);

    return (mi_xy > 0.0f) ? mi_ty / mi_xy : 0.0f;
}

//=============================================================================
// Bootstrap Methods
//=============================================================================

// Simple LCG random for bootstrap sampling (thread-local for thread safety)
static __thread uint32_t bootstrap_rand_state = 0;
static __thread bool bootstrap_rand_initialized = false;

static uint32_t bootstrap_rand(void) {
    if (!bootstrap_rand_initialized) {
        bootstrap_rand_state = (uint32_t)time(NULL) ^ (uint32_t)((uintptr_t)pthread_self() & 0xFFFFFFFF);
        bootstrap_rand_initialized = true;
    }
    bootstrap_rand_state = bootstrap_rand_state * 1103515245 + 12345;
    return (bootstrap_rand_state >> 16) & 0x7FFF;
}

static void bootstrap_seed(uint32_t seed) {
    bootstrap_rand_state = seed;
    bootstrap_rand_initialized = true;
}

// Helper: compute Pearson correlation coefficient (inline to avoid circular dep)
static float compute_pearson_r(const float* x, const float* y, uint32_t n) {
    if (n < 2) return 0.0f;
    float mean_x = 0.0f, mean_y = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        mean_x += x[i];
        mean_y += y[i];
    }
    mean_x /= n;
    mean_y /= n;

    float cov = 0.0f, var_x = 0.0f, var_y = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float dx = x[i] - mean_x;
        float dy = y[i] - mean_y;
        cov += dx * dy;
        var_x += dx * dx;
        var_y += dy * dy;
    }

    float denom = sqrtf(var_x * var_y);
    return (denom > 1e-10f) ? cov / denom : 0.0f;
}

nimcp_stats_result_t nimcp_stats_bootstrap_mean(
    const float* data,
    uint32_t n,
    uint32_t n_replicates,
    float confidence,
    nimcp_bootstrap_result_t* result
) {
    if (!data || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_stats_bootstrap_mean: null pointer");
        return NIMCP_STATS_ERROR_NULL;
    }
    if (n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_bootstrap_mean: n=0");
        return NIMCP_STATS_ERROR_SIZE;
    }
    if (n_replicates == 0 || confidence <= 0.0f || confidence >= 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_bootstrap_mean: invalid params");
        return NIMCP_STATS_ERROR_PARAMS;
    }

    float* boot_means = (float*)nimcp_malloc(n_replicates * sizeof(float));
    float* boot_sample = (float*)nimcp_malloc(n * sizeof(float));
    if (!boot_means || !boot_sample) {
        nimcp_free(boot_means);
        nimcp_free(boot_sample);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stats_bootstrap_mean: allocation failed");
        return NIMCP_STATS_ERROR_MEMORY;
    }

    bootstrap_seed((uint32_t)(data[0] * 1000));

    for (uint32_t r = 0; r < n_replicates; r++) {
        for (uint32_t i = 0; i < n; i++) {
            boot_sample[i] = data[bootstrap_rand() % n];
        }
        boot_means[r] = nimcp_stats_mean(boot_sample, n);
    }

    // Sort for percentile CI
    for (uint32_t i = 0; i < n_replicates - 1; i++) {
        for (uint32_t j = i + 1; j < n_replicates; j++) {
            if (boot_means[j] < boot_means[i]) {
                float tmp = boot_means[i];
                boot_means[i] = boot_means[j];
                boot_means[j] = tmp;
            }
        }
    }

    result->estimate = nimcp_stats_mean(data, n);
    result->bias = nimcp_stats_mean(boot_means, n_replicates) - result->estimate;
    result->std_error = nimcp_stats_std_dev(boot_means, n_replicates);

    float alpha = (1.0f - confidence) / 2.0f;
    uint32_t lower_idx = (uint32_t)(alpha * n_replicates);
    uint32_t upper_idx = (uint32_t)((1.0f - alpha) * n_replicates);
    if (upper_idx >= n_replicates) upper_idx = n_replicates - 1;

    result->ci_lower_percentile = boot_means[lower_idx];
    result->ci_upper_percentile = boot_means[upper_idx];
    result->ci_lower_bca = boot_means[lower_idx];  // Simplified BCa
    result->ci_upper_bca = boot_means[upper_idx];
    result->confidence_level = confidence;
    result->n_replicates = n_replicates;

    nimcp_free(boot_means);
    nimcp_free(boot_sample);
    return NIMCP_STATS_OK;
}

nimcp_stats_result_t nimcp_stats_bootstrap_median(
    const float* data,
    uint32_t n,
    uint32_t n_replicates,
    float confidence,
    nimcp_bootstrap_result_t* result
) {
    if (!data || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_stats_bootstrap_median: null pointer");
        return NIMCP_STATS_ERROR_NULL;
    }
    if (n == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_bootstrap_median: n=0");
        return NIMCP_STATS_ERROR_SIZE;
    }
    if (n_replicates == 0 || confidence <= 0.0f || confidence >= 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_bootstrap_median: invalid params");
        return NIMCP_STATS_ERROR_PARAMS;
    }

    float* boot_medians = (float*)nimcp_malloc(n_replicates * sizeof(float));
    float* boot_sample = (float*)nimcp_malloc(n * sizeof(float));
    if (!boot_medians || !boot_sample) {
        nimcp_free(boot_medians);
        nimcp_free(boot_sample);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stats_bootstrap_median: allocation failed");
        return NIMCP_STATS_ERROR_MEMORY;
    }

    bootstrap_seed((uint32_t)(data[0] * 1000 + 1));

    for (uint32_t r = 0; r < n_replicates; r++) {
        for (uint32_t i = 0; i < n; i++) {
            boot_sample[i] = data[bootstrap_rand() % n];
        }
        boot_medians[r] = nimcp_stats_median(boot_sample, n);
    }

    for (uint32_t i = 0; i < n_replicates - 1; i++) {
        for (uint32_t j = i + 1; j < n_replicates; j++) {
            if (boot_medians[j] < boot_medians[i]) {
                float tmp = boot_medians[i];
                boot_medians[i] = boot_medians[j];
                boot_medians[j] = tmp;
            }
        }
    }

    result->estimate = nimcp_stats_median(data, n);
    result->bias = nimcp_stats_mean(boot_medians, n_replicates) - result->estimate;
    result->std_error = nimcp_stats_std_dev(boot_medians, n_replicates);

    float alpha = (1.0f - confidence) / 2.0f;
    uint32_t lower_idx = (uint32_t)(alpha * n_replicates);
    uint32_t upper_idx = (uint32_t)((1.0f - alpha) * n_replicates);
    if (upper_idx >= n_replicates) upper_idx = n_replicates - 1;

    result->ci_lower_percentile = boot_medians[lower_idx];
    result->ci_upper_percentile = boot_medians[upper_idx];
    result->ci_lower_bca = boot_medians[lower_idx];
    result->ci_upper_bca = boot_medians[upper_idx];
    result->confidence_level = confidence;
    result->n_replicates = n_replicates;

    nimcp_free(boot_medians);
    nimcp_free(boot_sample);
    return NIMCP_STATS_OK;
}

nimcp_stats_result_t nimcp_stats_bootstrap_correlation(
    const float* x,
    const float* y,
    uint32_t n,
    uint32_t n_replicates,
    float confidence,
    nimcp_bootstrap_result_t* result
) {
    if (!x || !y || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_stats_bootstrap_correlation: null pointer");
        return NIMCP_STATS_ERROR_NULL;
    }
    if (n < 3) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_bootstrap_correlation: n<3");
        return NIMCP_STATS_ERROR_SIZE;
    }
    if (n_replicates == 0 || confidence <= 0.0f || confidence >= 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_bootstrap_correlation: invalid params");
        return NIMCP_STATS_ERROR_PARAMS;
    }

    float* boot_corrs = (float*)nimcp_malloc(n_replicates * sizeof(float));
    float* boot_x = (float*)nimcp_malloc(n * sizeof(float));
    float* boot_y = (float*)nimcp_malloc(n * sizeof(float));
    if (!boot_corrs || !boot_x || !boot_y) {
        nimcp_free(boot_corrs);
        nimcp_free(boot_x);
        nimcp_free(boot_y);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stats_bootstrap_correlation: allocation failed");
        return NIMCP_STATS_ERROR_MEMORY;
    }

    bootstrap_seed((uint32_t)(x[0] * 1000 + y[0] * 100));

    for (uint32_t r = 0; r < n_replicates; r++) {
        for (uint32_t i = 0; i < n; i++) {
            uint32_t idx = bootstrap_rand() % n;
            boot_x[i] = x[idx];
            boot_y[i] = y[idx];
        }
        boot_corrs[r] = compute_pearson_r(boot_x, boot_y, n);
    }

    for (uint32_t i = 0; i < n_replicates - 1; i++) {
        for (uint32_t j = i + 1; j < n_replicates; j++) {
            if (boot_corrs[j] < boot_corrs[i]) {
                float tmp = boot_corrs[i];
                boot_corrs[i] = boot_corrs[j];
                boot_corrs[j] = tmp;
            }
        }
    }

    result->estimate = compute_pearson_r(x, y, n);
    result->bias = nimcp_stats_mean(boot_corrs, n_replicates) - result->estimate;
    result->std_error = nimcp_stats_std_dev(boot_corrs, n_replicates);

    float alpha = (1.0f - confidence) / 2.0f;
    uint32_t lower_idx = (uint32_t)(alpha * n_replicates);
    uint32_t upper_idx = (uint32_t)((1.0f - alpha) * n_replicates);
    if (upper_idx >= n_replicates) upper_idx = n_replicates - 1;

    result->ci_lower_percentile = boot_corrs[lower_idx];
    result->ci_upper_percentile = boot_corrs[upper_idx];
    result->ci_lower_bca = boot_corrs[lower_idx];
    result->ci_upper_bca = boot_corrs[upper_idx];
    result->confidence_level = confidence;
    result->n_replicates = n_replicates;

    nimcp_free(boot_corrs);
    nimcp_free(boot_x);
    nimcp_free(boot_y);
    return NIMCP_STATS_OK;
}

//=============================================================================
// ANOVA
//=============================================================================

nimcp_stats_result_t nimcp_stats_anova_one_way(
    const float* const* groups,
    const uint32_t* sizes,
    uint32_t n_groups,
    float alpha,
    nimcp_anova_result_t* result
) {
    if (!groups || !sizes || !result) return NIMCP_STATS_ERROR_NULL;
    if (n_groups < 2) return NIMCP_STATS_ERROR_PARAMS;
    if (alpha <= 0.0f || alpha >= 1.0f) return NIMCP_STATS_ERROR_PARAMS;

    // Compute group means and total mean
    uint32_t n_total = 0;
    float grand_sum = 0.0f;

    float* group_means = (float*)nimcp_malloc(n_groups * sizeof(float));
    if (!group_means) return NIMCP_STATS_ERROR_MEMORY;

    for (uint32_t g = 0; g < n_groups; g++) {
        if (sizes[g] == 0 || !groups[g]) {
            nimcp_free(group_means);
            return NIMCP_STATS_ERROR_NULL;
        }
        group_means[g] = nimcp_stats_mean(groups[g], sizes[g]);
        n_total += sizes[g];
        for (uint32_t i = 0; i < sizes[g]; i++) {
            grand_sum += groups[g][i];
        }
    }

    float grand_mean = grand_sum / n_total;

    // Between-group sum of squares (SSB)
    float ssb = 0.0f;
    for (uint32_t g = 0; g < n_groups; g++) {
        float diff = group_means[g] - grand_mean;
        ssb += sizes[g] * diff * diff;
    }

    // Within-group sum of squares (SSW)
    float ssw = 0.0f;
    for (uint32_t g = 0; g < n_groups; g++) {
        for (uint32_t i = 0; i < sizes[g]; i++) {
            float diff = groups[g][i] - group_means[g];
            ssw += diff * diff;
        }
    }

    nimcp_free(group_means);

    // Degrees of freedom
    uint32_t df_between = n_groups - 1;
    uint32_t df_within = n_total - n_groups;

    if (df_within == 0) return NIMCP_STATS_ERROR_PARAMS;

    // Mean squares
    float msb = ssb / df_between;
    float msw = ssw / df_within;

    // F statistic
    float f_stat = (msw > 0.0f) ? msb / msw : INFINITY;

    // P-value from F distribution (approximation)
    float p_value = 1.0f - nimcp_stats_cdf_f(f_stat, (float)df_between, (float)df_within);

    result->f_statistic = f_stat;
    result->p_value = p_value;
    result->df_between = df_between;
    result->df_within = df_within;
    result->ss_between = ssb;
    result->ss_within = ssw;
    result->ss_total = ssb + ssw;
    result->ms_between = msb;
    result->ms_within = msw;
    result->eta_squared = ssb / (ssb + ssw);
    result->omega_squared = (ssb - (df_between * msw)) / (ssb + ssw + msw);
    result->significant = (p_value < alpha);

    return NIMCP_STATS_OK;
}

//=============================================================================
// Polynomial Regression
//=============================================================================

nimcp_stats_result_t nimcp_stats_regression_polynomial(
    const float* x,
    const float* y,
    uint32_t n,
    uint32_t degree,
    nimcp_regression_result_t* result
) {
    if (!x || !y || !result) return NIMCP_STATS_ERROR_NULL;
    if (n < degree + 1) return NIMCP_STATS_ERROR_SIZE;
    if (degree == 0 || degree > 10) return NIMCP_STATS_ERROR_PARAMS;

    uint32_t p = degree + 1;  // Number of coefficients

    // Build design matrix X (Vandermonde)
    float* X = (float*)nimcp_calloc(n * p, sizeof(float));
    float* XtX = (float*)nimcp_calloc(p * p, sizeof(float));
    float* Xty = (float*)nimcp_calloc(p, sizeof(float));
    float* beta = (float*)nimcp_calloc(p, sizeof(float));

    if (!X || !XtX || !Xty || !beta) {
        nimcp_free(X); nimcp_free(XtX); nimcp_free(Xty); nimcp_free(beta);
        return NIMCP_STATS_ERROR_MEMORY;
    }

    // Fill design matrix: X[i,j] = x[i]^j
    for (uint32_t i = 0; i < n; i++) {
        float xpow = 1.0f;
        for (uint32_t j = 0; j < p; j++) {
            X[i * p + j] = xpow;
            xpow *= x[i];
        }
    }

    // Compute X'X
    for (uint32_t i = 0; i < p; i++) {
        for (uint32_t j = 0; j < p; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < n; k++) {
                sum += X[k * p + i] * X[k * p + j];
            }
            XtX[i * p + j] = sum;
        }
    }

    // Compute X'y
    for (uint32_t i = 0; i < p; i++) {
        float sum = 0.0f;
        for (uint32_t k = 0; k < n; k++) {
            sum += X[k * p + i] * y[k];
        }
        Xty[i] = sum;
    }

    // Solve using Gauss-Jordan elimination
    float* aug = (float*)nimcp_calloc(p * (p + 1), sizeof(float));
    if (!aug) {
        nimcp_free(X); nimcp_free(XtX); nimcp_free(Xty); nimcp_free(beta);
        return NIMCP_STATS_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < p; i++) {
        for (uint32_t j = 0; j < p; j++) {
            aug[i * (p + 1) + j] = XtX[i * p + j];
        }
        aug[i * (p + 1) + p] = Xty[i];
    }

    // Forward elimination with partial pivoting
    for (uint32_t col = 0; col < p; col++) {
        // Find pivot
        uint32_t max_row = col;
        float max_val = fabsf(aug[col * (p + 1) + col]);
        for (uint32_t row = col + 1; row < p; row++) {
            if (fabsf(aug[row * (p + 1) + col]) > max_val) {
                max_val = fabsf(aug[row * (p + 1) + col]);
                max_row = row;
            }
        }

        // Swap rows
        if (max_row != col) {
            for (uint32_t j = 0; j <= p; j++) {
                float tmp = aug[col * (p + 1) + j];
                aug[col * (p + 1) + j] = aug[max_row * (p + 1) + j];
                aug[max_row * (p + 1) + j] = tmp;
            }
        }

        float pivot = aug[col * (p + 1) + col];
        if (fabsf(pivot) < 1e-10f) {
            nimcp_free(X); nimcp_free(XtX); nimcp_free(Xty); nimcp_free(beta); nimcp_free(aug);
            return NIMCP_STATS_ERROR_SINGULAR;
        }

        // Scale pivot row
        for (uint32_t j = col; j <= p; j++) {
            aug[col * (p + 1) + j] /= pivot;
        }

        // Eliminate column
        for (uint32_t row = 0; row < p; row++) {
            if (row != col) {
                float factor = aug[row * (p + 1) + col];
                for (uint32_t j = col; j <= p; j++) {
                    aug[row * (p + 1) + j] -= factor * aug[col * (p + 1) + j];
                }
            }
        }
    }

    // Extract solution
    for (uint32_t i = 0; i < p; i++) {
        beta[i] = aug[i * (p + 1) + p];
    }

    // Compute R-squared and Durbin-Watson
    float y_mean = nimcp_stats_mean(y, n);
    float ss_tot = 0.0f, ss_res = 0.0f;
    float sum_diff_sq = 0.0f;
    float prev_res = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float y_pred = 0.0f;
        float xpow = 1.0f;
        for (uint32_t j = 0; j < p; j++) {
            y_pred += beta[j] * xpow;
            xpow *= x[i];
        }
        float res = y[i] - y_pred;
        ss_res += res * res;
        ss_tot += (y[i] - y_mean) * (y[i] - y_mean);

        // Durbin-Watson calculation
        if (i > 0) {
            float diff = res - prev_res;
            sum_diff_sq += diff * diff;
        }
        prev_res = res;
    }

    result->intercept = beta[0];
    result->slope = (degree >= 1) ? beta[1] : 0.0f;

    // Allocate and copy coefficients
    result->coefficients = (float*)nimcp_malloc(p * sizeof(float));
    if (result->coefficients) {
        memcpy(result->coefficients, beta, p * sizeof(float));
    }
    result->n_coefficients = p;
    result->r_squared = (ss_tot > 0.0f) ? 1.0f - ss_res / ss_tot : 0.0f;
    result->adj_r_squared = 1.0f - (1.0f - result->r_squared) * (n - 1) / (n - p);
    result->std_error = sqrtf(ss_res / (n - p));
    result->f_statistic = 0.0f;
    result->p_value = 0.0f;
    result->se_coefficients = NULL;
    result->t_statistics = NULL;
    result->p_values = NULL;
    float mse = ss_res / n;
    result->aic = (mse > 0.0f) ? n * logf(mse) + 2.0f * p : 0.0f;
    result->bic = (mse > 0.0f) ? n * logf(mse) + p * logf((float)n) : 0.0f;
    result->durbin_watson = (ss_res > 0.0f) ? sum_diff_sq / ss_res : 2.0f;

    nimcp_free(X); nimcp_free(XtX); nimcp_free(Xty); nimcp_free(beta); nimcp_free(aug);
    return NIMCP_STATS_OK;
}

//=============================================================================
// Partial Correlation
//=============================================================================

nimcp_stats_result_t nimcp_stats_correlation_partial(
    const float* x,
    const float* y,
    const float* z,
    uint32_t n,
    nimcp_correlation_result_t* result
) {
    if (!x || !y || !z || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_stats_correlation_partial: null pointer");
        return NIMCP_STATS_ERROR_NULL;
    }
    if (n < 4) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_correlation_partial: n<4");
        return NIMCP_STATS_ERROR_SIZE;
    }

    // Compute pairwise correlations using local helper
    float r_xy = compute_pearson_r(x, y, n);
    float r_xz = compute_pearson_r(x, z, n);
    float r_yz = compute_pearson_r(y, z, n);

    // Partial correlation formula: r_xy.z = (r_xy - r_xz * r_yz) / sqrt((1-r_xz^2)(1-r_yz^2))
    float denom_x = 1.0f - r_xz * r_xz;
    float denom_y = 1.0f - r_yz * r_yz;

    if (denom_x < 1e-10f || denom_y < 1e-10f) {
        result->r = 0.0f;
        result->r_squared = 0.0f;
        result->p_value = 1.0f;
        result->t_statistic = 0.0f;
        result->n = n;
        result->df = (float)(n - 3);
        result->ci_lower = 0.0f;
        result->ci_upper = 0.0f;
        return NIMCP_STATS_OK;
    }

    float r_partial = (r_xy - r_xz * r_yz) / sqrtf(denom_x * denom_y);

    // Clamp to valid range
    if (r_partial > 1.0f) r_partial = 1.0f;
    if (r_partial < -1.0f) r_partial = -1.0f;

    // t-statistic with n-3 degrees of freedom
    float df = (float)(n - 3);
    float t_stat = r_partial * sqrtf(df / (1.0f - r_partial * r_partial + 1e-10f));

    // Two-tailed p-value
    float p_value = 2.0f * (1.0f - nimcp_stats_cdf_student_t(fabsf(t_stat), df));

    result->r = r_partial;
    result->r_squared = r_partial * r_partial;
    result->t_statistic = t_stat;
    result->p_value = p_value;
    result->n = n;
    result->df = df;
    result->ci_lower = 0.0f;  // Would need Fisher z-transform
    result->ci_upper = 0.0f;

    return NIMCP_STATS_OK;
}

//=============================================================================
// Winsorization
//=============================================================================

nimcp_stats_result_t nimcp_stats_winsorize(
    const float* data,
    uint32_t n,
    float lower_pct,
    float upper_pct,
    float* out
) {
    if (!data || !out) return NIMCP_STATS_ERROR_NULL;
    if (n == 0) return NIMCP_STATS_ERROR_SIZE;
    if (lower_pct < 0.0f || upper_pct > 1.0f || lower_pct >= upper_pct) {
        return NIMCP_STATS_ERROR_PARAMS;
    }

    // Create sorted copy to find percentiles
    float* sorted = (float*)nimcp_malloc(n * sizeof(float));
    if (!sorted) return NIMCP_STATS_ERROR_MEMORY;

    memcpy(sorted, data, n * sizeof(float));
    for (uint32_t i = 0; i < n - 1; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            if (sorted[j] < sorted[i]) {
                float tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }

    // Find percentile values
    uint32_t lower_idx = (uint32_t)(lower_pct * (n - 1));
    uint32_t upper_idx = (uint32_t)(upper_pct * (n - 1));
    float lower_val = sorted[lower_idx];
    float upper_val = sorted[upper_idx];

    // Apply winsorization
    for (uint32_t i = 0; i < n; i++) {
        if (data[i] < lower_val) {
            out[i] = lower_val;
        } else if (data[i] > upper_val) {
            out[i] = upper_val;
        } else {
            out[i] = data[i];
        }
    }

    nimcp_free(sorted);
    return NIMCP_STATS_OK;
}

//=============================================================================
// Regression Result Cleanup
//=============================================================================

void nimcp_stats_regression_free(nimcp_regression_result_t* result) {
    if (!result) return;

    if (result->coefficients) {
        nimcp_free(result->coefficients);
        result->coefficients = NULL;
    }
    if (result->se_coefficients) {
        nimcp_free(result->se_coefficients);
        result->se_coefficients = NULL;
    }
    if (result->t_statistics) {
        nimcp_free(result->t_statistics);
        result->t_statistics = NULL;
    }
    if (result->p_values) {
        nimcp_free(result->p_values);
        result->p_values = NULL;
    }
    result->n_coefficients = 0;
}

//=============================================================================
// Ranking Functions
//=============================================================================

/**
 * @brief Compute statistical ranks with tie-handling
 * @param data Input data
 * @param n Number of elements
 * @param ranks Output ranks (must be pre-allocated with n elements)
 * @param handle_ties Method: 'a' = average, 'f' = first, 'm' = min, 'M' = max
 */
nimcp_stats_result_t nimcp_stats_rank(
    const float* data,
    uint32_t n,
    float* ranks,
    char handle_ties
) {
    if (!data || !ranks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_stats_rank: null pointer");
        return NIMCP_STATS_ERROR_NULL;
    }
    if (n == 0) {
        return NIMCP_STATS_OK;
    }

    // Create index array for sorting
    uint32_t* indices = (uint32_t*)nimcp_malloc(n * sizeof(uint32_t));
    if (!indices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stats_rank: allocation failed");
        return NIMCP_STATS_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < n; i++) {
        indices[i] = i;
    }

    // Sort indices by data values (simple insertion sort for stability)
    for (uint32_t i = 1; i < n; i++) {
        uint32_t key = indices[i];
        float key_val = data[key];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && data[indices[j]] > key_val) {
            indices[j + 1] = indices[j];
            j--;
        }
        indices[j + 1] = key;
    }

    // Assign ranks based on tie-handling method
    uint32_t i = 0;
    while (i < n) {
        // Find the end of the tie group
        uint32_t tie_start = i;
        uint32_t tie_end = i;
        float current_val = data[indices[i]];

        while (tie_end < n - 1 && data[indices[tie_end + 1]] == current_val) {
            tie_end++;
        }

        uint32_t tie_count = tie_end - tie_start + 1;

        if (tie_count == 1) {
            // No tie - rank is position + 1
            ranks[indices[i]] = (float)(i + 1);
        } else {
            // Handle ties based on method
            switch (handle_ties) {
                case 'a': // Average rank
                {
                    float avg_rank = 0.0f;
                    for (uint32_t j = tie_start; j <= tie_end; j++) {
                        avg_rank += (float)(j + 1);
                    }
                    avg_rank /= tie_count;
                    for (uint32_t j = tie_start; j <= tie_end; j++) {
                        ranks[indices[j]] = avg_rank;
                    }
                    break;
                }
                case 'f': // First (sequential in order of appearance)
                    for (uint32_t j = tie_start; j <= tie_end; j++) {
                        ranks[indices[j]] = (float)(j + 1);
                    }
                    break;
                case 'm': // Minimum rank
                {
                    float min_rank = (float)(tie_start + 1);
                    for (uint32_t j = tie_start; j <= tie_end; j++) {
                        ranks[indices[j]] = min_rank;
                    }
                    break;
                }
                case 'M': // Maximum rank
                {
                    float max_rank = (float)(tie_end + 1);
                    for (uint32_t j = tie_start; j <= tie_end; j++) {
                        ranks[indices[j]] = max_rank;
                    }
                    break;
                }
                default: // Default to average
                {
                    float avg_rank = 0.0f;
                    for (uint32_t j = tie_start; j <= tie_end; j++) {
                        avg_rank += (float)(j + 1);
                    }
                    avg_rank /= tie_count;
                    for (uint32_t j = tie_start; j <= tie_end; j++) {
                        ranks[indices[j]] = avg_rank;
                    }
                    break;
                }
            }
        }

        i = tie_end + 1;
    }

    nimcp_free(indices);
    return NIMCP_STATS_OK;
}

//=============================================================================
// Normality Tests
//=============================================================================

/**
 * @brief Shapiro-Wilk test for normality
 * @param data Input data
 * @param n Sample size (3 <= n <= 5000)
 * @param result Output test result
 *
 * Uses the Royston (1992) approximation for larger samples.
 */
nimcp_stats_result_t nimcp_stats_shapiro_wilk(
    const float* data,
    uint32_t n,
    nimcp_test_result_t* result
) {
    if (!data || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_stats_shapiro_wilk: null pointer");
        return NIMCP_STATS_ERROR_NULL;
    }
    if (n < 3) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_shapiro_wilk: n must be >= 3");
        return NIMCP_STATS_ERROR_SIZE;
    }

    // Sort the data
    float* sorted = (float*)nimcp_malloc(n * sizeof(float));
    if (!sorted) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stats_shapiro_wilk: allocation failed");
        return NIMCP_STATS_ERROR_MEMORY;
    }

    memcpy(sorted, data, n * sizeof(float));

    // Simple insertion sort
    for (uint32_t i = 1; i < n; i++) {
        float key = sorted[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    // Compute mean
    float mean = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        mean += data[i];
    }
    mean /= n;

    // Compute sum of squares
    float ss = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float diff = data[i] - mean;
        ss += diff * diff;
    }

    if (ss < 1e-10f) {
        // All values are identical - not normally distributed (degenerate)
        nimcp_free(sorted);
        result->statistic = 0.0f;
        result->p_value = 0.0f;
        result->df = (float)n;
        return NIMCP_STATS_OK;
    }

    // Compute Shapiro-Wilk a coefficients using Royston's approximation
    // For simplicity, use polynomial approximations for expected normal order statistics
    float* a = (float*)nimcp_malloc(n * sizeof(float));
    if (!a) {
        nimcp_free(sorted);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stats_shapiro_wilk: allocation failed");
        return NIMCP_STATS_ERROR_MEMORY;
    }

    // Compute expected normal order statistics using Blom's approximation
    // E(Z(i)) ≈ Φ^(-1)((i - 3/8) / (n + 1/4))
    float* m = (float*)nimcp_malloc(n * sizeof(float));
    if (!m) {
        nimcp_free(sorted);
        nimcp_free(a);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stats_shapiro_wilk: allocation failed");
        return NIMCP_STATS_ERROR_MEMORY;
    }

    float m_sum_sq = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        // Blom's approximation for expected normal order statistics
        float p = ((float)(i + 1) - 0.375f) / ((float)n + 0.25f);
        // Approximate inverse normal CDF using Hastings' approximation
        float t;
        if (p < 0.5f) {
            t = sqrtf(-2.0f * logf(p));
            m[i] = -(t - (2.515517f + 0.802853f * t + 0.010328f * t * t) /
                    (1.0f + 1.432788f * t + 0.189269f * t * t + 0.001308f * t * t * t));
        } else {
            t = sqrtf(-2.0f * logf(1.0f - p));
            m[i] = t - (2.515517f + 0.802853f * t + 0.010328f * t * t) /
                   (1.0f + 1.432788f * t + 0.189269f * t * t + 0.001308f * t * t * t);
        }
        m_sum_sq += m[i] * m[i];
    }

    // Normalize to get a coefficients: a = m / ||m||
    float m_norm = sqrtf(m_sum_sq);
    for (uint32_t i = 0; i < n; i++) {
        a[i] = m[i] / m_norm;
    }

    // Compute W statistic: W = (Σ a_i * x_(i))² / SS
    float numerator = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        numerator += a[i] * sorted[i];
    }
    numerator = numerator * numerator;

    float W = numerator / ss;

    // Clamp W to valid range
    if (W > 1.0f) W = 1.0f;
    if (W < 0.0f) W = 0.0f;

    // Compute p-value using Royston's approximation
    // Transform W to approximately normal using log transformation
    float p_value;

    if (n <= 11) {
        // For small samples, use polynomial approximation
        float gamma = 0.459f * (float)n - 2.273f;
        float w_star = -logf(1.0f - W);
        float mu = -0.0006714f * (float)n * (float)n * (float)n +
                    0.025054f * (float)n * (float)n -
                    0.39978f * (float)n + 0.5440f;
        float sigma = expf(-0.0020322f * (float)n * (float)n * (float)n +
                          0.062767f * (float)n * (float)n -
                          0.77857f * (float)n + 1.3822f);
        float z = (powf(w_star, gamma) - mu) / sigma;
        // Convert z to p-value using standard normal CDF approximation
        if (z < -6.0f) {
            p_value = 1.0f;
        } else if (z > 6.0f) {
            p_value = 0.0f;
        } else {
            float t_val = 1.0f / (1.0f + 0.2316419f * fabsf(z));
            float d = 0.3989423f * expf(-z * z / 2.0f);
            float p_norm = d * t_val * (0.3193815f + t_val * (-0.3565638f + t_val * (1.781478f +
                          t_val * (-1.821256f + t_val * 1.330274f))));
            p_value = (z > 0.0f) ? p_norm : 1.0f - p_norm;
        }
    } else {
        // For larger samples, use log-normal approximation
        float ln_W = logf(1.0f - W);
        float mu = 0.0038915f * logf((float)n) * logf((float)n) * logf((float)n) -
                   0.083751f * logf((float)n) * logf((float)n) -
                   0.31082f * logf((float)n) - 1.5861f;
        float sigma = expf(0.0030302f * logf((float)n) * logf((float)n) -
                          0.082676f * logf((float)n) - 0.4803f);
        float z = (ln_W - mu) / sigma;
        // Convert z to p-value
        if (z < -6.0f) {
            p_value = 1.0f;
        } else if (z > 6.0f) {
            p_value = 0.0f;
        } else {
            float t_val = 1.0f / (1.0f + 0.2316419f * fabsf(z));
            float d = 0.3989423f * expf(-z * z / 2.0f);
            float p_norm = d * t_val * (0.3193815f + t_val * (-0.3565638f + t_val * (1.781478f +
                          t_val * (-1.821256f + t_val * 1.330274f))));
            p_value = (z > 0.0f) ? p_norm : 1.0f - p_norm;
        }
    }

    // Clamp p-value to valid range
    if (p_value < 0.0f) p_value = 0.0f;
    if (p_value > 1.0f) p_value = 1.0f;

    result->statistic = W;
    result->p_value = p_value;
    result->df = (float)n;

    nimcp_free(sorted);
    nimcp_free(a);
    nimcp_free(m);

    return NIMCP_STATS_OK;
}

/**
 * @brief Kolmogorov-Smirnov test for normality
 * @param data Input data
 * @param n Sample size
 * @param result Output test result
 */
nimcp_stats_result_t nimcp_stats_ks_normality(
    const float* data,
    uint32_t n,
    nimcp_test_result_t* result
) {
    if (!data || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_stats_ks_normality: null pointer");
        return NIMCP_STATS_ERROR_NULL;
    }
    if (n < 3) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_ks_normality: n must be >= 3");
        return NIMCP_STATS_ERROR_SIZE;
    }

    // Sort the data
    float* sorted = (float*)nimcp_malloc(n * sizeof(float));
    if (!sorted) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stats_ks_normality: allocation failed");
        return NIMCP_STATS_ERROR_MEMORY;
    }
    memcpy(sorted, data, n * sizeof(float));

    // Simple insertion sort
    for (uint32_t i = 1; i < n; i++) {
        float key = sorted[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    // Compute sample mean and standard deviation
    float mean = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        mean += data[i];
    }
    mean /= n;

    float var = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float diff = data[i] - mean;
        var += diff * diff;
    }
    float std_dev = sqrtf(var / (n - 1));

    if (std_dev < 1e-10f) {
        nimcp_free(sorted);
        result->statistic = 1.0f;
        result->p_value = 0.0f;
        result->df = (float)n;
        return NIMCP_STATS_OK;
    }

    // Compute KS statistic D = max|F_n(x) - F(x)|
    float D = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        // Standardize to compare with standard normal
        float z = (sorted[i] - mean) / std_dev;

        // Compute standard normal CDF using approximation
        float F_z;
        if (z < -6.0f) {
            F_z = 0.0f;
        } else if (z > 6.0f) {
            F_z = 1.0f;
        } else {
            float t = 1.0f / (1.0f + 0.2316419f * fabsf(z));
            float d = 0.3989423f * expf(-z * z / 2.0f);
            float p_norm = d * t * (0.3193815f + t * (-0.3565638f + t * (1.781478f +
                          t * (-1.821256f + t * 1.330274f))));
            F_z = (z > 0.0f) ? (1.0f - p_norm) : p_norm;
        }

        // Empirical CDF at this point
        float F_n = (float)(i + 1) / n;
        float F_n_minus = (float)i / n;

        // D+ = max(F_n(x) - F(x))
        // D- = max(F(x) - F_{n-1}(x))
        float d_plus = fabsf(F_n - F_z);
        float d_minus = fabsf(F_z - F_n_minus);

        if (d_plus > D) D = d_plus;
        if (d_minus > D) D = d_minus;
    }

    // Compute p-value using asymptotic distribution
    // For the Lilliefors test (KS with estimated parameters), use modified critical values
    // P(D > d) ≈ 2 * sum_{k=1}^inf (-1)^(k-1) * exp(-2k²n²d²)
    float sqrt_n = sqrtf((float)n);
    float lambda = (sqrt_n + 0.12f + 0.11f / sqrt_n) * D;

    float p_value = 0.0f;
    float sign = 1.0f;
    for (int k = 1; k <= 100; k++) {
        float term = sign * expf(-2.0f * k * k * lambda * lambda);
        p_value += term;
        sign = -sign;
        if (fabsf(term) < 1e-10f) break;
    }
    p_value = 2.0f * p_value;

    // Clamp p-value
    if (p_value < 0.0f) p_value = 0.0f;
    if (p_value > 1.0f) p_value = 1.0f;

    result->statistic = D;
    result->p_value = p_value;
    result->df = (float)n;

    nimcp_free(sorted);
    return NIMCP_STATS_OK;
}

//=============================================================================
// Logistic Regression
//=============================================================================

/**
 * @brief Sigmoid function for logistic regression
 */
static float sigmoid(float x) {
    // Clip to avoid overflow
    if (x > 20.0f) return 1.0f;
    if (x < -20.0f) return 0.0f;
    return 1.0f / (1.0f + expf(-x));
}

/**
 * @brief Logistic regression using iteratively reweighted least squares (IRLS)
 * @param X Design matrix (n x p, stored row-major)
 * @param y Binary response (0 or 1)
 * @param n Number of observations
 * @param p Number of predictors (including intercept if desired)
 * @param coefficients Output coefficients (size p)
 * @param max_iter Maximum iterations for convergence
 */
nimcp_stats_result_t nimcp_stats_regression_logistic(
    const float* X,
    const uint8_t* y,
    uint32_t n,
    uint32_t p,
    float* coefficients,
    uint32_t max_iter
) {
    if (!X || !y || !coefficients) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_stats_regression_logistic: null pointer");
        return NIMCP_STATS_ERROR_NULL;
    }
    if (n < p || p == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_regression_logistic: invalid dimensions");
        return NIMCP_STATS_ERROR_SIZE;
    }

    // Initialize coefficients to zero
    for (uint32_t j = 0; j < p; j++) {
        coefficients[j] = 0.0f;
    }

    // Allocate working arrays
    float* prob = (float*)nimcp_malloc(n * sizeof(float));
    float* grad = (float*)nimcp_malloc(p * sizeof(float));
    float* hess = (float*)nimcp_malloc(p * p * sizeof(float));
    float* delta = (float*)nimcp_malloc(p * sizeof(float));

    if (!prob || !grad || !hess || !delta) {
        if (prob) nimcp_free(prob);
        if (grad) nimcp_free(grad);
        if (hess) nimcp_free(hess);
        if (delta) nimcp_free(delta);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stats_regression_logistic: allocation failed");
        return NIMCP_STATS_ERROR_MEMORY;
    }

    float tol = 1e-6f;

    for (uint32_t iter = 0; iter < max_iter; iter++) {
        // Compute probabilities p_i = sigmoid(X_i * beta)
        for (uint32_t i = 0; i < n; i++) {
            float linear = 0.0f;
            for (uint32_t j = 0; j < p; j++) {
                linear += X[i * p + j] * coefficients[j];
            }
            prob[i] = sigmoid(linear);
        }

        // Compute gradient: grad = X^T (y - p)
        for (uint32_t j = 0; j < p; j++) {
            grad[j] = 0.0f;
            for (uint32_t i = 0; i < n; i++) {
                grad[j] += X[i * p + j] * ((float)y[i] - prob[i]);
            }
        }

        // Compute Hessian: H = -X^T W X where W = diag(p_i * (1 - p_i))
        for (uint32_t j = 0; j < p; j++) {
            for (uint32_t k = 0; k < p; k++) {
                hess[j * p + k] = 0.0f;
                for (uint32_t i = 0; i < n; i++) {
                    float w = prob[i] * (1.0f - prob[i]);
                    // Avoid division by zero
                    if (w < 1e-10f) w = 1e-10f;
                    hess[j * p + k] -= X[i * p + j] * w * X[i * p + k];
                }
            }
        }

        // Solve H * delta = -grad using simple Gauss-Jordan for small p
        // For Newton step: beta_new = beta - H^(-1) * grad
        // Since H is negative definite, we solve -H * delta = grad

        // Copy -H to working matrix and grad to delta for in-place solve
        for (uint32_t j = 0; j < p; j++) {
            for (uint32_t k = 0; k < p; k++) {
                hess[j * p + k] = -hess[j * p + k];
            }
            delta[j] = grad[j];
        }

        // Gauss-Jordan elimination
        for (uint32_t j = 0; j < p; j++) {
            // Find pivot
            float max_val = fabsf(hess[j * p + j]);
            uint32_t max_row = j;
            for (uint32_t i = j + 1; i < p; i++) {
                if (fabsf(hess[i * p + j]) > max_val) {
                    max_val = fabsf(hess[i * p + j]);
                    max_row = i;
                }
            }

            // Swap rows if needed
            if (max_row != j) {
                for (uint32_t k = 0; k < p; k++) {
                    float tmp = hess[j * p + k];
                    hess[j * p + k] = hess[max_row * p + k];
                    hess[max_row * p + k] = tmp;
                }
                float tmp = delta[j];
                delta[j] = delta[max_row];
                delta[max_row] = tmp;
            }

            float pivot = hess[j * p + j];
            if (fabsf(pivot) < 1e-10f) {
                // Singular matrix - add regularization
                pivot = 1e-6f;
                hess[j * p + j] = pivot;
            }

            // Scale pivot row
            for (uint32_t k = j; k < p; k++) {
                hess[j * p + k] /= pivot;
            }
            delta[j] /= pivot;

            // Eliminate column
            for (uint32_t i = 0; i < p; i++) {
                if (i != j) {
                    float factor = hess[i * p + j];
                    for (uint32_t k = j; k < p; k++) {
                        hess[i * p + k] -= factor * hess[j * p + k];
                    }
                    delta[i] -= factor * delta[j];
                }
            }
        }

        // Update coefficients: beta = beta + delta
        float max_change = 0.0f;
        for (uint32_t j = 0; j < p; j++) {
            coefficients[j] += delta[j];
            if (fabsf(delta[j]) > max_change) {
                max_change = fabsf(delta[j]);
            }
        }

        // Check convergence
        if (max_change < tol) {
            break;
        }
    }

    nimcp_free(prob);
    nimcp_free(grad);
    nimcp_free(hess);
    nimcp_free(delta);

    return NIMCP_STATS_OK;
}

//=============================================================================
// Kendall Correlation
//=============================================================================

/**
 * @brief Kendall's tau correlation coefficient
 * @param x First variable
 * @param y Second variable
 * @param n Sample size
 * @param result Output correlation result
 */
nimcp_stats_result_t nimcp_stats_correlation_kendall(
    const float* x,
    const float* y,
    uint32_t n,
    nimcp_correlation_result_t* result
) {
    if (!x || !y || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_stats_correlation_kendall: null pointer");
        return NIMCP_STATS_ERROR_NULL;
    }
    if (n < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_correlation_kendall: n must be >= 2");
        return NIMCP_STATS_ERROR_SIZE;
    }

    // Count concordant and discordant pairs
    int32_t concordant = 0;
    int32_t discordant = 0;
    int32_t ties_x = 0;
    int32_t ties_y = 0;

    for (uint32_t i = 0; i < n - 1; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            float dx = x[j] - x[i];
            float dy = y[j] - y[i];

            if (fabsf(dx) < 1e-10f && fabsf(dy) < 1e-10f) {
                // Tied on both
                ties_x++;
                ties_y++;
            } else if (fabsf(dx) < 1e-10f) {
                // Tied on x only
                ties_x++;
            } else if (fabsf(dy) < 1e-10f) {
                // Tied on y only
                ties_y++;
            } else if ((dx > 0 && dy > 0) || (dx < 0 && dy < 0)) {
                concordant++;
            } else {
                discordant++;
            }
        }
    }

    // Total pairs
    int64_t n_pairs = (int64_t)n * (n - 1) / 2;

    // Kendall's tau-b (handles ties)
    float tau;
    float denom = sqrtf((float)(n_pairs - ties_x) * (float)(n_pairs - ties_y));
    if (denom < 1e-10f) {
        tau = 0.0f;
    } else {
        tau = (float)(concordant - discordant) / denom;
    }

    // Compute z-score for p-value (large sample approximation)
    float var_tau = (float)(2 * (2 * n + 5)) / (float)(9 * n * (n - 1));
    float z = tau / sqrtf(var_tau);

    // Convert z to p-value (two-tailed)
    float p_value;
    float abs_z = fabsf(z);
    if (abs_z > 6.0f) {
        p_value = 0.0f;
    } else {
        float t = 1.0f / (1.0f + 0.2316419f * abs_z);
        float d = 0.3989423f * expf(-abs_z * abs_z / 2.0f);
        float p_one_tail = d * t * (0.3193815f + t * (-0.3565638f + t * (1.781478f +
                          t * (-1.821256f + t * 1.330274f))));
        p_value = 2.0f * p_one_tail;  // Two-tailed
    }

    result->r = tau;
    result->r_squared = tau * tau;
    result->p_value = p_value;
    result->t_statistic = z;
    result->n = n;
    result->df = (float)(n - 2);
    result->ci_lower = tau - 1.96f * sqrtf(var_tau);
    result->ci_upper = tau + 1.96f * sqrtf(var_tau);

    return NIMCP_STATS_OK;
}

//=============================================================================
// Point-Biserial Correlation
//=============================================================================

/**
 * @brief Point-biserial correlation between continuous and binary variable
 * @param continuous Continuous variable
 * @param binary Binary variable (0 or 1)
 * @param n Sample size
 * @param result Output correlation result
 */
nimcp_stats_result_t nimcp_stats_correlation_point_biserial(
    const float* continuous,
    const uint8_t* binary,
    uint32_t n,
    nimcp_correlation_result_t* result
) {
    if (!continuous || !binary || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_stats_correlation_point_biserial: null pointer");
        return NIMCP_STATS_ERROR_NULL;
    }
    if (n < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_correlation_point_biserial: n must be >= 2");
        return NIMCP_STATS_ERROR_SIZE;
    }

    // Compute means for each group
    float sum_0 = 0.0f, sum_1 = 0.0f;
    uint32_t n_0 = 0, n_1 = 0;

    for (uint32_t i = 0; i < n; i++) {
        if (binary[i] == 0) {
            sum_0 += continuous[i];
            n_0++;
        } else {
            sum_1 += continuous[i];
            n_1++;
        }
    }

    if (n_0 == 0 || n_1 == 0) {
        // All values in one group - correlation is undefined
        result->r = 0.0f;
        result->r_squared = 0.0f;
        result->p_value = 1.0f;
        result->t_statistic = 0.0f;
        result->n = n;
        result->df = (float)(n - 2);
        result->ci_lower = 0.0f;
        result->ci_upper = 0.0f;
        return NIMCP_STATS_OK;
    }

    float mean_0 = sum_0 / n_0;
    float mean_1 = sum_1 / n_1;

    // Compute overall mean and standard deviation
    float mean_total = (sum_0 + sum_1) / n;
    float ss_total = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float diff = continuous[i] - mean_total;
        ss_total += diff * diff;
    }
    float sd = sqrtf(ss_total / (n - 1));

    if (sd < 1e-10f) {
        result->r = 0.0f;
        result->r_squared = 0.0f;
        result->p_value = 1.0f;
        result->t_statistic = 0.0f;
        result->n = n;
        result->df = (float)(n - 2);
        result->ci_lower = 0.0f;
        result->ci_upper = 0.0f;
        return NIMCP_STATS_OK;
    }

    // Point-biserial correlation formula:
    // r_pb = (M1 - M0) / sd * sqrt(n0 * n1 / (n * n))
    float r = (mean_1 - mean_0) / sd * sqrtf((float)n_0 * n_1 / ((float)n * n));

    // Compute t-statistic
    float t_stat = r * sqrtf((n - 2) / (1.0f - r * r));

    // Compute p-value from t-distribution (approximation using normal for large n)
    float abs_t = fabsf(t_stat);
    float p_value;
    if (abs_t > 10.0f) {
        p_value = 0.0f;
    } else {
        // Use normal approximation for simplicity
        float z = abs_t;
        float t = 1.0f / (1.0f + 0.2316419f * z);
        float d = 0.3989423f * expf(-z * z / 2.0f);
        float p_one_tail = d * t * (0.3193815f + t * (-0.3565638f + t * (1.781478f +
                          t * (-1.821256f + t * 1.330274f))));
        p_value = 2.0f * p_one_tail;
    }

    result->r = r;
    result->r_squared = r * r;
    result->p_value = p_value;
    result->t_statistic = t_stat;
    result->n = n;
    result->df = (float)(n - 2);

    // Confidence interval using Fisher's z transformation
    float z_r = 0.5f * logf((1.0f + r) / (1.0f - r + 1e-10f));
    float se_z = 1.0f / sqrtf((float)(n - 3));
    float z_lower = z_r - 1.96f * se_z;
    float z_upper = z_r + 1.96f * se_z;
    result->ci_lower = (expf(2.0f * z_lower) - 1.0f) / (expf(2.0f * z_lower) + 1.0f);
    result->ci_upper = (expf(2.0f * z_upper) - 1.0f) / (expf(2.0f * z_upper) + 1.0f);

    return NIMCP_STATS_OK;
}

//=============================================================================
// Multiple Regression
//=============================================================================

/**
 * @brief Multiple linear regression using OLS
 * @param X Design matrix (n x p, row-major, should include column of 1s for intercept)
 * @param y Response variable
 * @param n Number of observations
 * @param p Number of predictors (including intercept column if present)
 * @param result Output regression result (coefficients array will be allocated)
 */
nimcp_stats_result_t nimcp_stats_regression_multiple(
    const float* X,
    const float* y,
    uint32_t n,
    uint32_t p,
    nimcp_regression_result_t* result
) {
    if (!X || !y || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_stats_regression_multiple: null pointer");
        return NIMCP_STATS_ERROR_NULL;
    }
    if (n <= p || p == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_stats_regression_multiple: n must be > p");
        return NIMCP_STATS_ERROR_SIZE;
    }

    // Allocate result arrays
    result->coefficients = (float*)nimcp_malloc(p * sizeof(float));
    result->se_coefficients = (float*)nimcp_malloc(p * sizeof(float));
    result->t_statistics = (float*)nimcp_malloc(p * sizeof(float));
    result->p_values = (float*)nimcp_malloc(p * sizeof(float));

    if (!result->coefficients || !result->se_coefficients ||
        !result->t_statistics || !result->p_values) {
        if (result->coefficients) nimcp_free(result->coefficients);
        if (result->se_coefficients) nimcp_free(result->se_coefficients);
        if (result->t_statistics) nimcp_free(result->t_statistics);
        if (result->p_values) nimcp_free(result->p_values);
        result->coefficients = NULL;
        result->se_coefficients = NULL;
        result->t_statistics = NULL;
        result->p_values = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stats_regression_multiple: allocation failed");
        return NIMCP_STATS_ERROR_MEMORY;
    }
    result->n_coefficients = p;

    // Compute X^T X (p x p matrix)
    float* XtX = (float*)nimcp_malloc(p * p * sizeof(float));
    float* Xty = (float*)nimcp_malloc(p * sizeof(float));

    if (!XtX || !Xty) {
        if (XtX) nimcp_free(XtX);
        if (Xty) nimcp_free(Xty);
        nimcp_stats_regression_free(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stats_regression_multiple: allocation failed");
        return NIMCP_STATS_ERROR_MEMORY;
    }

    // Compute X^T X
    for (uint32_t i = 0; i < p; i++) {
        for (uint32_t j = 0; j < p; j++) {
            float sum = 0.0f;
            for (uint32_t k = 0; k < n; k++) {
                sum += X[k * p + i] * X[k * p + j];
            }
            XtX[i * p + j] = sum;
        }
    }

    // Compute X^T y
    for (uint32_t i = 0; i < p; i++) {
        float sum = 0.0f;
        for (uint32_t k = 0; k < n; k++) {
            sum += X[k * p + i] * y[k];
        }
        Xty[i] = sum;
    }

    // Solve (X^T X) β = X^T y using Gauss-Jordan elimination
    // Augment XtX with Xty
    float* aug = (float*)nimcp_malloc(p * (p + 1) * sizeof(float));
    if (!aug) {
        nimcp_free(XtX);
        nimcp_free(Xty);
        nimcp_stats_regression_free(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_stats_regression_multiple: allocation failed");
        return NIMCP_STATS_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < p; i++) {
        for (uint32_t j = 0; j < p; j++) {
            aug[i * (p + 1) + j] = XtX[i * p + j];
        }
        aug[i * (p + 1) + p] = Xty[i];
    }

    // Gauss-Jordan with pivoting
    for (uint32_t col = 0; col < p; col++) {
        // Find pivot
        float max_val = fabsf(aug[col * (p + 1) + col]);
        uint32_t max_row = col;
        for (uint32_t row = col + 1; row < p; row++) {
            if (fabsf(aug[row * (p + 1) + col]) > max_val) {
                max_val = fabsf(aug[row * (p + 1) + col]);
                max_row = row;
            }
        }

        // Swap rows
        if (max_row != col) {
            for (uint32_t k = 0; k <= p; k++) {
                float tmp = aug[col * (p + 1) + k];
                aug[col * (p + 1) + k] = aug[max_row * (p + 1) + k];
                aug[max_row * (p + 1) + k] = tmp;
            }
        }

        float pivot = aug[col * (p + 1) + col];
        if (fabsf(pivot) < 1e-10f) {
            // Singular matrix - add regularization
            pivot = 1e-6f;
            aug[col * (p + 1) + col] = pivot;
        }

        // Scale pivot row
        for (uint32_t k = col; k <= p; k++) {
            aug[col * (p + 1) + k] /= pivot;
        }

        // Eliminate column
        for (uint32_t row = 0; row < p; row++) {
            if (row != col) {
                float factor = aug[row * (p + 1) + col];
                for (uint32_t k = col; k <= p; k++) {
                    aug[row * (p + 1) + k] -= factor * aug[col * (p + 1) + k];
                }
            }
        }
    }

    // Extract coefficients
    for (uint32_t i = 0; i < p; i++) {
        result->coefficients[i] = aug[i * (p + 1) + p];
    }

    // Set intercept and slope for simple regression compatibility
    result->intercept = result->coefficients[0];
    result->slope = (p > 1) ? result->coefficients[1] : 0.0f;

    // Compute residuals and statistics
    float y_mean = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        y_mean += y[i];
    }
    y_mean /= n;

    float ss_res = 0.0f;  // Residual sum of squares
    float ss_tot = 0.0f;  // Total sum of squares

    for (uint32_t i = 0; i < n; i++) {
        // Compute prediction
        float y_pred = 0.0f;
        for (uint32_t j = 0; j < p; j++) {
            y_pred += X[i * p + j] * result->coefficients[j];
        }

        float residual = y[i] - y_pred;
        ss_res += residual * residual;
        ss_tot += (y[i] - y_mean) * (y[i] - y_mean);
    }

    // R-squared
    result->r_squared = (ss_tot > 1e-10f) ? (1.0f - ss_res / ss_tot) : 0.0f;

    // Adjusted R-squared
    float df_res = (float)(n - p);
    float df_tot = (float)(n - 1);
    result->adj_r_squared = 1.0f - (1.0f - result->r_squared) * df_tot / df_res;

    // Standard error of estimate
    float mse = ss_res / df_res;
    result->std_error = sqrtf(mse);

    // F-statistic
    float ss_reg = ss_tot - ss_res;
    float df_reg = (float)(p - 1);
    if (df_reg > 0 && mse > 1e-10f) {
        result->f_statistic = (ss_reg / df_reg) / mse;
    } else {
        result->f_statistic = 0.0f;
    }

    // Compute (X^T X)^(-1) for standard errors
    // We need to invert XtX - do another Gauss-Jordan pass
    float* inv = (float*)nimcp_malloc(p * p * sizeof(float));
    if (inv) {
        // Initialize augmented matrix [XtX | I]
        float* aug2 = (float*)nimcp_malloc(p * 2 * p * sizeof(float));
        if (aug2) {
            for (uint32_t i = 0; i < p; i++) {
                for (uint32_t j = 0; j < p; j++) {
                    aug2[i * 2 * p + j] = XtX[i * p + j];
                    aug2[i * 2 * p + p + j] = (i == j) ? 1.0f : 0.0f;
                }
            }

            // Gauss-Jordan
            for (uint32_t col = 0; col < p; col++) {
                float max_val = fabsf(aug2[col * 2 * p + col]);
                uint32_t max_row = col;
                for (uint32_t row = col + 1; row < p; row++) {
                    if (fabsf(aug2[row * 2 * p + col]) > max_val) {
                        max_val = fabsf(aug2[row * 2 * p + col]);
                        max_row = row;
                    }
                }

                if (max_row != col) {
                    for (uint32_t k = 0; k < 2 * p; k++) {
                        float tmp = aug2[col * 2 * p + k];
                        aug2[col * 2 * p + k] = aug2[max_row * 2 * p + k];
                        aug2[max_row * 2 * p + k] = tmp;
                    }
                }

                float pivot = aug2[col * 2 * p + col];
                if (fabsf(pivot) < 1e-10f) pivot = 1e-6f;

                for (uint32_t k = 0; k < 2 * p; k++) {
                    aug2[col * 2 * p + k] /= pivot;
                }

                for (uint32_t row = 0; row < p; row++) {
                    if (row != col) {
                        float factor = aug2[row * 2 * p + col];
                        for (uint32_t k = 0; k < 2 * p; k++) {
                            aug2[row * 2 * p + k] -= factor * aug2[col * 2 * p + k];
                        }
                    }
                }
            }

            // Extract inverse
            for (uint32_t i = 0; i < p; i++) {
                for (uint32_t j = 0; j < p; j++) {
                    inv[i * p + j] = aug2[i * 2 * p + p + j];
                }
            }
            nimcp_free(aug2);

            // Compute standard errors: se_j = sqrt(MSE * (X^T X)^(-1)_jj)
            for (uint32_t j = 0; j < p; j++) {
                result->se_coefficients[j] = sqrtf(mse * inv[j * p + j]);
                if (result->se_coefficients[j] > 1e-10f) {
                    result->t_statistics[j] = result->coefficients[j] / result->se_coefficients[j];
                    // P-value from t-distribution (normal approximation)
                    float abs_t = fabsf(result->t_statistics[j]);
                    if (abs_t > 10.0f) {
                        result->p_values[j] = 0.0f;
                    } else {
                        float t = 1.0f / (1.0f + 0.2316419f * abs_t);
                        float d = 0.3989423f * expf(-abs_t * abs_t / 2.0f);
                        result->p_values[j] = 2.0f * d * t * (0.3193815f + t * (-0.3565638f +
                                             t * (1.781478f + t * (-1.821256f + t * 1.330274f))));
                    }
                } else {
                    result->t_statistics[j] = 0.0f;
                    result->p_values[j] = 1.0f;
                }
            }
        }
        nimcp_free(inv);
    }

    // P-value for F-test (using chi-squared approximation)
    if (result->f_statistic > 0.0f && df_reg > 0) {
        // Very rough approximation
        float x = result->f_statistic * df_reg / df_res;
        result->p_value = expf(-0.5f * x);
    } else {
        result->p_value = 1.0f;
    }

    // AIC and BIC
    float log_lik = -(float)n / 2.0f * (logf(NIMCP_TWO_PI_F * mse) + 1.0f);
    result->aic = -2.0f * log_lik + 2.0f * p;
    result->bic = -2.0f * log_lik + logf((float)n) * p;

    // Durbin-Watson statistic (for autocorrelation)
    float dw_num = 0.0f;
    float dw_denom = 0.0f;
    float prev_resid = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float y_pred = 0.0f;
        for (uint32_t j = 0; j < p; j++) {
            y_pred += X[i * p + j] * result->coefficients[j];
        }
        float resid = y[i] - y_pred;
        dw_denom += resid * resid;
        if (i > 0) {
            float diff = resid - prev_resid;
            dw_num += diff * diff;
        }
        prev_resid = resid;
    }
    result->durbin_watson = (dw_denom > 1e-10f) ? (dw_num / dw_denom) : 2.0f;

    nimcp_free(XtX);
    nimcp_free(Xty);
    nimcp_free(aug);

    return NIMCP_STATS_OK;
}

//=============================================================================
// Information Theory Functions
//=============================================================================

/**
 * @brief Differential entropy of continuous data using histogram estimation
 * @param data Continuous data
 * @param n Number of samples
 * @param n_bins Number of histogram bins
 * @return Differential entropy estimate in nats
 */
float nimcp_stats_differential_entropy(
    const float* data,
    uint32_t n,
    uint32_t n_bins
) {
    if (!data || n < 2) {
        return 0.0f;
    }

    // Use Sturges' rule for default bin count: k = ceil(log2(n) + 1)
    if (n_bins == 0) {
        n_bins = (uint32_t)ceilf(log2f((float)n) + 1.0f);
        if (n_bins < 5) n_bins = 5;
        if (n_bins > 100) n_bins = 100;
    }

    // Find min and max
    float min_val = data[0];
    float max_val = data[0];
    for (uint32_t i = 1; i < n; i++) {
        if (data[i] < min_val) min_val = data[i];
        if (data[i] > max_val) max_val = data[i];
    }

    float range = max_val - min_val;
    if (range < 1e-10f) {
        return -INFINITY;  // All values are identical
    }

    float bin_width = range / n_bins;

    // Compute histogram
    uint32_t* counts = (uint32_t*)nimcp_calloc(n_bins, sizeof(uint32_t));
    if (!counts) return 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t bin = (uint32_t)((data[i] - min_val) / bin_width);
        if (bin >= n_bins) bin = n_bins - 1;
        counts[bin]++;
    }

    // Compute entropy (in bits using log2 for consistency with nimcp_stats_entropy)
    // For differential entropy: h(X) ≈ -Σ p_i * log2(p_i / Δx)
    //                               = -Σ p_i * log2(p_i) + log2(Δx)
    float entropy = 0.0f;
    for (uint32_t i = 0; i < n_bins; i++) {
        if (counts[i] > 0) {
            float p = (float)counts[i] / n;
            entropy -= p * log2f(p);
        }
    }
    entropy += log2f(bin_width);

    nimcp_free(counts);
    return entropy;
}

/**
 * @brief Normalized mutual information
 * @param joint_prob Joint probability table (n_x x n_y, row-major)
 * @param n_x Number of outcomes for X
 * @param n_y Number of outcomes for Y
 * @return Normalized MI (0-1 scale)
 */
float nimcp_stats_normalized_mi(
    const float* joint_prob,
    uint32_t n_x,
    uint32_t n_y
) {
    if (!joint_prob || n_x == 0 || n_y == 0) {
        return 0.0f;
    }

    // Compute marginals
    float* p_x = (float*)nimcp_calloc(n_x, sizeof(float));
    float* p_y = (float*)nimcp_calloc(n_y, sizeof(float));
    if (!p_x || !p_y) {
        if (p_x) nimcp_free(p_x);
        if (p_y) nimcp_free(p_y);
        return 0.0f;
    }

    for (uint32_t i = 0; i < n_x; i++) {
        for (uint32_t j = 0; j < n_y; j++) {
            p_x[i] += joint_prob[i * n_y + j];
            p_y[j] += joint_prob[i * n_y + j];
        }
    }

    // Compute entropies
    float h_x = 0.0f;
    for (uint32_t i = 0; i < n_x; i++) {
        if (p_x[i] > 1e-10f) {
            h_x -= p_x[i] * log2f(p_x[i]);
        }
    }

    float h_y = 0.0f;
    for (uint32_t j = 0; j < n_y; j++) {
        if (p_y[j] > 1e-10f) {
            h_y -= p_y[j] * log2f(p_y[j]);
        }
    }

    // Compute joint entropy
    float h_xy = 0.0f;
    for (uint32_t i = 0; i < n_x; i++) {
        for (uint32_t j = 0; j < n_y; j++) {
            float p_ij = joint_prob[i * n_y + j];
            if (p_ij > 1e-10f) {
                h_xy -= p_ij * log2f(p_ij);
            }
        }
    }

    nimcp_free(p_x);
    nimcp_free(p_y);

    // Mutual information: I(X;Y) = H(X) + H(Y) - H(X,Y)
    float mi = h_x + h_y - h_xy;
    if (mi < 0.0f) mi = 0.0f;  // Numerical correction

    // Normalized MI: NMI = 2 * I(X;Y) / (H(X) + H(Y))
    float denom = h_x + h_y;
    if (denom < 1e-10f) {
        return 0.0f;
    }
    return 2.0f * mi / denom;
}

/**
 * @brief Compute all information-theoretic measures for a joint distribution
 * @param joint_prob Joint probability table (n_x x n_y, row-major)
 * @param n_x Number of outcomes for X
 * @param n_y Number of outcomes for Y
 * @param result Output information result
 */
nimcp_stats_result_t nimcp_stats_info_measures(
    const float* joint_prob,
    uint32_t n_x,
    uint32_t n_y,
    nimcp_info_result_t* result
) {
    if (!joint_prob || !result) {
        if (result) memset(result, 0, sizeof(nimcp_info_result_t));
        return NIMCP_STATS_ERROR_NULL;
    }
    if (n_x == 0 || n_y == 0) {
        memset(result, 0, sizeof(nimcp_info_result_t));
        return NIMCP_STATS_ERROR_SIZE;
    }

    // Compute marginals
    float* p_x = (float*)nimcp_calloc(n_x, sizeof(float));
    float* p_y = (float*)nimcp_calloc(n_y, sizeof(float));
    if (!p_x || !p_y) {
        if (p_x) nimcp_free(p_x);
        if (p_y) nimcp_free(p_y);
        memset(result, 0, sizeof(nimcp_info_result_t));
        return NIMCP_STATS_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < n_x; i++) {
        for (uint32_t j = 0; j < n_y; j++) {
            p_x[i] += joint_prob[i * n_y + j];
            p_y[j] += joint_prob[i * n_y + j];
        }
    }

    // Compute H(X)
    float h_x = 0.0f;
    for (uint32_t i = 0; i < n_x; i++) {
        if (p_x[i] > 1e-10f) {
            h_x -= p_x[i] * log2f(p_x[i]);
        }
    }
    result->entropy = h_x;

    // Compute H(Y)
    float h_y = 0.0f;
    for (uint32_t j = 0; j < n_y; j++) {
        if (p_y[j] > 1e-10f) {
            h_y -= p_y[j] * log2f(p_y[j]);
        }
    }

    // Compute H(X,Y)
    float h_xy = 0.0f;
    for (uint32_t i = 0; i < n_x; i++) {
        for (uint32_t j = 0; j < n_y; j++) {
            float p_ij = joint_prob[i * n_y + j];
            if (p_ij > 1e-10f) {
                h_xy -= p_ij * log2f(p_ij);
            }
        }
    }
    result->joint_entropy = h_xy;

    // Conditional entropy: H(X|Y) = H(X,Y) - H(Y)
    result->conditional_entropy = h_xy - h_y;
    if (result->conditional_entropy < 0.0f) {
        result->conditional_entropy = 0.0f;  // Numerical correction
    }

    // Mutual information: I(X;Y) = H(X) + H(Y) - H(X,Y)
    result->mutual_information = h_x + h_y - h_xy;
    if (result->mutual_information < 0.0f) {
        result->mutual_information = 0.0f;  // Numerical correction
    }

    // Normalized MI: NMI = 2 * I(X;Y) / (H(X) + H(Y))
    float denom = h_x + h_y;
    result->normalized_mi = (denom > 1e-10f) ? (2.0f * result->mutual_information / denom) : 0.0f;

    // Variation of Information: VI = H(X|Y) + H(Y|X) = 2*H(X,Y) - H(X) - H(Y)
    result->variation_of_info = 2.0f * h_xy - h_x - h_y;
    if (result->variation_of_info < 0.0f) {
        result->variation_of_info = 0.0f;  // Numerical correction
    }

    nimcp_free(p_x);
    nimcp_free(p_y);

    return NIMCP_STATS_OK;
}
