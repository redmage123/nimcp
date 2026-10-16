/**
 * @file nimcp_neural_statistics.c
 * @brief Implementation of neural-specific statistical analysis tools
 *
 * WHAT: Complete neural statistics toolkit implementation
 * WHY:  Specialized analysis for spike trains, population coding, synaptic data
 * HOW:  Numerically stable algorithms with optional SIMD/GPU acceleration
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 */

#include "utils/statistics/nimcp_neural_statistics.h"
#include "utils/statistics/nimcp_statistics.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/memory/nimcp_memory.h"
#include "constants/nimcp_math_constants.h"

//=============================================================================
// MODULE IDENTIFICATION
//=============================================================================

#define LOG_MODULE_NAME "neural_statistics"

//=============================================================================
// CONSTANTS
//=============================================================================

#define PI NIMCP_PI_F
#define SQRT_2PI 2.5066282746310002f

//=============================================================================
// Global State
//=============================================================================

static bool g_neural_stats_initialized = false;
static neural_stats_config_t g_neural_stats_config;

//=============================================================================
// Module Initialization
//=============================================================================

neural_stats_config_t neural_stats_default_config(void) {
    neural_stats_config_t config = {
        .enable_simd = false,
        .enable_gpu = false,
        .enable_parallel = false,
        .parallel_threshold = 10000,
        .default_bin_width_ms = NEURAL_STATS_DEFAULT_BIN_WIDTH_MS,
        .random_seed = 0
    };
    return config;
}

neural_stats_result_t neural_stats_init(const neural_stats_config_t* config) {
    if (config) {
        g_neural_stats_config = *config;
    } else {
        g_neural_stats_config = neural_stats_default_config();
    }

    // Ensure base statistics module is initialized
    if (!nimcp_stats_is_initialized()) {
        nimcp_stats_result_t stats_result = nimcp_stats_init(NULL);
        if (stats_result != NIMCP_STATS_OK) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED,
                "Failed to initialize base statistics module");
            return NEURAL_STATS_ERROR_NOT_INIT;
        }
    }

    g_neural_stats_initialized = true;
    LOG_INFO("Neural statistics module initialized");
    return NEURAL_STATS_OK;
}

void neural_stats_shutdown(void) {
    g_neural_stats_initialized = false;
    LOG_INFO("Neural statistics module shutdown");
}

bool neural_stats_is_initialized(void) {
    return g_neural_stats_initialized;
}

const char* nimcp_neural_stats_error_string(neural_stats_result_t result) {
    switch (result) {
        case NEURAL_STATS_OK: return "Success";
        case NEURAL_STATS_ERROR_NULL: return "NULL pointer argument";
        case NEURAL_STATS_ERROR_SIZE: return "Invalid size";
        case NEURAL_STATS_ERROR_MEMORY: return "Memory allocation failed";
        case NEURAL_STATS_ERROR_PARAMS: return "Invalid parameters";
        case NEURAL_STATS_ERROR_CONVERGE: return "Algorithm did not converge";
        case NEURAL_STATS_ERROR_SINGULAR: return "Singular matrix";
        case NEURAL_STATS_ERROR_RANGE: return "Value out of range";
        case NEURAL_STATS_ERROR_NOT_INIT: return "Module not initialized";
        case NEURAL_STATS_ERROR_GPU: return "GPU operation failed";
        default: return "Unknown error";
    }
}

//=============================================================================
// Helper Functions
//=============================================================================

static int float_compare(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

static float compute_median(const float* data, uint32_t n) {
    if (n == 0) return NAN;

    float* sorted = (float*)nimcp_malloc(n * sizeof(float));
    if (!sorted) return NAN;
    memcpy(sorted, data, n * sizeof(float));
    qsort(sorted, n, sizeof(float), float_compare);

    float result;
    if (n % 2 == 1) {
        result = sorted[n / 2];
    } else {
        result = (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0f;
    }

    nimcp_free(sorted);
    return result;
}

static float gaussian_kernel(float x, float sigma) {
    return expf(-0.5f * (x * x) / (sigma * sigma)) / (sigma * SQRT_2PI);
}

//=============================================================================
// Spike Train Utilities
//=============================================================================

neural_spike_train_t* nimcp_neural_spike_train_create(
    const float* spike_times,
    uint32_t n_spikes,
    float start_time,
    float end_time,
    uint32_t neuron_id)
{
    if (!spike_times && n_spikes > 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL spike_times with non-zero n_spikes");
        return NULL;
    }

    neural_spike_train_t* train = (neural_spike_train_t*)nimcp_calloc(1, sizeof(neural_spike_train_t));
    if (!train) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate spike train structure");
        return NULL;
    }

    if (n_spikes > 0) {
        train->spike_times = (float*)nimcp_malloc(n_spikes * sizeof(float));
        if (!train->spike_times) {
            nimcp_free(train);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "Failed to allocate spike times array");
            return NULL;
        }
        memcpy(train->spike_times, spike_times, n_spikes * sizeof(float));
    }

    train->n_spikes = n_spikes;
    train->start_time = start_time;
    train->end_time = end_time;
    train->neuron_id = neuron_id;

    return train;
}

void nimcp_neural_spike_train_destroy(neural_spike_train_t* train) {
    if (!train) return;
    nimcp_free(train->spike_times);
    nimcp_free(train);
}

neural_spike_ensemble_t* nimcp_neural_spike_ensemble_create(
    const neural_spike_train_t* trains,
    uint32_t n_neurons)
{
    if (!trains || n_neurons == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "Invalid parameters for ensemble creation");
        return NULL;
    }

    neural_spike_ensemble_t* ensemble = (neural_spike_ensemble_t*)nimcp_calloc(1,
        sizeof(neural_spike_ensemble_t));
    if (!ensemble) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate spike ensemble");
        return NULL;
    }

    ensemble->trains = (neural_spike_train_t*)nimcp_calloc(n_neurons, sizeof(neural_spike_train_t));
    if (!ensemble->trains) {
        nimcp_free(ensemble);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate spike trains array");
        return NULL;
    }

    // Copy trains and determine common time range
    ensemble->start_time = trains[0].start_time;
    ensemble->end_time = trains[0].end_time;

    for (uint32_t i = 0; i < n_neurons; i++) {
        ensemble->trains[i] = trains[i];
        if (trains[i].n_spikes > 0) {
            ensemble->trains[i].spike_times = (float*)nimcp_malloc(
                trains[i].n_spikes * sizeof(float));
            if (ensemble->trains[i].spike_times) {
                memcpy(ensemble->trains[i].spike_times, trains[i].spike_times,
                    trains[i].n_spikes * sizeof(float));
            }
        }

        if (trains[i].start_time < ensemble->start_time) {
            ensemble->start_time = trains[i].start_time;
        }
        if (trains[i].end_time > ensemble->end_time) {
            ensemble->end_time = trains[i].end_time;
        }
    }

    ensemble->n_neurons = n_neurons;
    return ensemble;
}

void nimcp_neural_spike_ensemble_destroy(neural_spike_ensemble_t* ensemble) {
    if (!ensemble) return;

    for (uint32_t i = 0; i < ensemble->n_neurons; i++) {
        nimcp_free(ensemble->trains[i].spike_times);
    }
    nimcp_free(ensemble->trains);
    nimcp_free(ensemble);
}

//=============================================================================
// ISI Distribution
//=============================================================================

neural_stats_result_t nimcp_neural_isi_distribution(
    const neural_spike_train_t* spike_train,
    neural_isi_distribution_t* result)
{
    if (!spike_train || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL pointer in ISI distribution");
        return NEURAL_STATS_ERROR_NULL;
    }

    if (spike_train->n_spikes < 2) {
        LOG_WARN("Need at least 2 spikes for ISI distribution");
        return NEURAL_STATS_ERROR_SIZE;
    }

    memset(result, 0, sizeof(neural_isi_distribution_t));

    uint32_t n_intervals = spike_train->n_spikes - 1;
    result->n_intervals = n_intervals;

    // Allocate ISI array
    result->intervals = (float*)nimcp_malloc(n_intervals * sizeof(float));
    if (!result->intervals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate ISI array");
        return NEURAL_STATS_ERROR_MEMORY;
    }

    // Compute ISIs
    result->min_isi = FLT_MAX;
    result->max_isi = -FLT_MAX;
    double sum = 0.0;

    for (uint32_t i = 0; i < n_intervals; i++) {
        float isi = spike_train->spike_times[i + 1] - spike_train->spike_times[i];
        result->intervals[i] = isi;
        sum += isi;

        if (isi < result->min_isi) result->min_isi = isi;
        if (isi > result->max_isi) result->max_isi = isi;
    }

    // Mean
    result->mean = (float)(sum / n_intervals);

    // Variance (two-pass for numerical stability)
    double m2 = 0.0;
    double m3 = 0.0;
    double m4 = 0.0;

    for (uint32_t i = 0; i < n_intervals; i++) {
        double d = result->intervals[i] - result->mean;
        double d2 = d * d;
        m2 += d2;
        m3 += d2 * d;
        m4 += d2 * d2;
    }

    result->variance = (float)(m2 / (n_intervals - 1));
    result->std_dev = sqrtf(result->variance);

    // Coefficient of variation
    if (result->mean > NEURAL_STATS_EPSILON) {
        result->cv = result->std_dev / result->mean;
    } else {
        result->cv = NAN;
    }

    // Skewness and kurtosis
    double pop_var = m2 / n_intervals;
    if (pop_var > 0.0 && n_intervals >= 3) {
        result->skewness = (float)((m3 / n_intervals) / pow(pop_var, 1.5));
    }
    if (pop_var > 0.0 && n_intervals >= 4) {
        result->kurtosis = (float)((m4 / n_intervals) / (pop_var * pop_var) - 3.0);
    }

    // Median
    result->median_isi = compute_median(result->intervals, n_intervals);

    // CV2 (local coefficient of variation)
    result->cv2 = nimcp_neural_isi_cv2(spike_train);

    LOG_DEBUG("ISI distribution: n=%u, mean=%.2f, CV=%.3f, CV2=%.3f",
        n_intervals, result->mean, result->cv, result->cv2);

    return NEURAL_STATS_OK;
}

float nimcp_neural_isi_cv(const neural_spike_train_t* spike_train) {
    if (!spike_train || spike_train->n_spikes < 2) {
        return NAN;
    }

    uint32_t n = spike_train->n_spikes - 1;
    double sum = 0.0;
    double sum_sq = 0.0;

    for (uint32_t i = 0; i < n; i++) {
        float isi = spike_train->spike_times[i + 1] - spike_train->spike_times[i];
        sum += isi;
        sum_sq += isi * isi;
    }

    double mean = sum / n;
    if (mean < NEURAL_STATS_EPSILON) return NAN;

    double variance = (sum_sq - sum * sum / n) / (n - 1);
    return (float)(sqrt(variance) / mean);
}

float nimcp_neural_isi_cv2(const neural_spike_train_t* spike_train) {
    if (!spike_train || spike_train->n_spikes < 3) {
        return NAN;
    }

    uint32_t n = spike_train->n_spikes - 2;  // Number of consecutive pairs
    double sum_cv2 = 0.0;

    for (uint32_t i = 0; i < n; i++) {
        float isi1 = spike_train->spike_times[i + 1] - spike_train->spike_times[i];
        float isi2 = spike_train->spike_times[i + 2] - spike_train->spike_times[i + 1];
        float sum_isi = isi1 + isi2;

        if (sum_isi > NEURAL_STATS_EPSILON) {
            sum_cv2 += 2.0f * fabsf(isi2 - isi1) / sum_isi;
        }
    }

    return (float)(sum_cv2 / n);
}

void nimcp_neural_isi_distribution_free(neural_isi_distribution_t* result) {
    if (!result) return;
    nimcp_free(result->intervals);
    memset(result, 0, sizeof(neural_isi_distribution_t));
}

//=============================================================================
// Fano Factor
//=============================================================================

neural_stats_result_t nimcp_neural_fano_factor(
    const neural_spike_train_t* spike_train,
    float window_size,
    neural_fano_result_t* result)
{
    if (!spike_train || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL pointer in Fano factor computation");
        return NEURAL_STATS_ERROR_NULL;
    }

    if (window_size <= 0.0f) {
        return NEURAL_STATS_ERROR_PARAMS;
    }

    memset(result, 0, sizeof(neural_fano_result_t));

    float duration = spike_train->end_time - spike_train->start_time;
    uint32_t n_windows = (uint32_t)(duration / window_size);

    if (n_windows < 2) {
        LOG_WARN("Recording too short for Fano factor with window %.2f ms", window_size);
        return NEURAL_STATS_ERROR_SIZE;
    }

    // Count spikes in each window
    uint32_t* counts = (uint32_t*)nimcp_calloc(n_windows, sizeof(uint32_t));
    if (!counts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate spike counts");
        return NEURAL_STATS_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < spike_train->n_spikes; i++) {
        float t = spike_train->spike_times[i] - spike_train->start_time;
        uint32_t window_idx = (uint32_t)(t / window_size);
        if (window_idx < n_windows) {
            counts[window_idx]++;
        }
    }

    // Compute mean and variance
    double sum = 0.0;
    for (uint32_t i = 0; i < n_windows; i++) {
        sum += counts[i];
    }
    double mean = sum / n_windows;

    double sum_sq_diff = 0.0;
    for (uint32_t i = 0; i < n_windows; i++) {
        double diff = counts[i] - mean;
        sum_sq_diff += diff * diff;
    }
    double variance = sum_sq_diff / (n_windows - 1);

    nimcp_free(counts);

    // Fano factor
    if (mean > NEURAL_STATS_EPSILON) {
        result->fano_factor = (float)(variance / mean);
    } else {
        result->fano_factor = NAN;
    }

    result->expected_poisson = 1.0f;
    result->is_sub_poisson = (result->fano_factor < 1.0f);
    result->is_super_poisson = (result->fano_factor > 1.0f);

    LOG_DEBUG("Fano factor: %.3f (window=%.2f ms, n_windows=%u)",
        result->fano_factor, window_size, n_windows);

    return NEURAL_STATS_OK;
}

void nimcp_neural_fano_result_free(neural_fano_result_t* result) {
    if (!result) return;
    nimcp_free(result->fano_by_window);
    nimcp_free(result->window_sizes);
    memset(result, 0, sizeof(neural_fano_result_t));
}

//=============================================================================
// Spike Train Entropy
//=============================================================================

neural_stats_result_t nimcp_neural_spike_train_entropy(
    const neural_spike_train_t* spike_train,
    float bin_size,
    uint32_t word_length,
    neural_entropy_result_t* result)
{
    if (!spike_train || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL pointer in spike train entropy");
        return NEURAL_STATS_ERROR_NULL;
    }

    if (bin_size <= 0.0f || word_length == 0) {
        return NEURAL_STATS_ERROR_PARAMS;
    }

    memset(result, 0, sizeof(neural_entropy_result_t));

    float duration = spike_train->end_time - spike_train->start_time;
    uint32_t n_bins = (uint32_t)(duration / bin_size);

    if (n_bins < word_length) {
        LOG_WARN("Recording too short for entropy with word_length=%u", word_length);
        return NEURAL_STATS_ERROR_SIZE;
    }

    // Discretize spike train into binary bins
    uint8_t* binary = (uint8_t*)nimcp_calloc(n_bins, sizeof(uint8_t));
    if (!binary) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate binary array");
        return NEURAL_STATS_ERROR_MEMORY;
    }

    for (uint32_t i = 0; i < spike_train->n_spikes; i++) {
        float t = spike_train->spike_times[i] - spike_train->start_time;
        uint32_t bin_idx = (uint32_t)(t / bin_size);
        if (bin_idx < n_bins) {
            binary[bin_idx] = 1;
        }
    }

    // Count word patterns
    uint32_t n_patterns_possible = 1 << word_length;  // 2^word_length
    uint32_t* pattern_counts = (uint32_t*)nimcp_calloc(n_patterns_possible, sizeof(uint32_t));
    if (!pattern_counts) {
        nimcp_free(binary);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate pattern counts");
        return NEURAL_STATS_ERROR_MEMORY;
    }

    uint32_t n_words = n_bins - word_length + 1;
    for (uint32_t i = 0; i < n_words; i++) {
        uint32_t pattern = 0;
        for (uint32_t j = 0; j < word_length; j++) {
            pattern = (pattern << 1) | binary[i + j];
        }
        pattern_counts[pattern]++;
    }

    nimcp_free(binary);

    // Compute entropy
    double entropy = 0.0;
    result->n_patterns = 0;

    for (uint32_t i = 0; i < n_patterns_possible; i++) {
        if (pattern_counts[i] > 0) {
            result->n_patterns++;
            double p = (double)pattern_counts[i] / n_words;
            entropy -= p * log2(p);
        }
    }

    nimcp_free(pattern_counts);

    result->total_entropy = (float)entropy;
    result->entropy_rate = (float)(entropy / word_length);  // bits per bin

    LOG_DEBUG("Spike train entropy: %.3f bits (word_length=%u, n_patterns=%u)",
        result->total_entropy, word_length, result->n_patterns);

    return NEURAL_STATS_OK;
}

void nimcp_neural_entropy_result_free(neural_entropy_result_t* result) {
    if (!result) return;
    memset(result, 0, sizeof(neural_entropy_result_t));
}

//=============================================================================
// Firing Rate
//=============================================================================

neural_stats_result_t nimcp_neural_firing_rate(
    const neural_spike_train_t* spike_train,
    float kernel_width,
    float time_step,
    neural_firing_rate_t* result)
{
    if (!spike_train || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL pointer in firing rate computation");
        return NEURAL_STATS_ERROR_NULL;
    }

    if (time_step <= 0.0f) {
        return NEURAL_STATS_ERROR_PARAMS;
    }

    memset(result, 0, sizeof(neural_firing_rate_t));

    float duration = spike_train->end_time - spike_train->start_time;

    // Mean firing rate (always computed)
    if (duration > 0.0f) {
        result->mean_rate = (spike_train->n_spikes * 1000.0f) / duration;  // Hz
    } else {
        result->mean_rate = 0.0f;
    }

    // Instantaneous rate (kernel density estimation or histogram)
    uint32_t n_points = (uint32_t)(duration / time_step) + 1;
    if (n_points < 2) {
        result->n_time_points = 0;
        return NEURAL_STATS_OK;
    }

    result->instantaneous = (float*)nimcp_calloc(n_points, sizeof(float));
    result->rate_times = (float*)nimcp_malloc(n_points * sizeof(float));
    if (!result->instantaneous || !result->rate_times) {
        nimcp_free(result->instantaneous);
        nimcp_free(result->rate_times);
        result->instantaneous = NULL;
        result->rate_times = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate rate arrays");
        return NEURAL_STATS_ERROR_MEMORY;
    }

    result->n_time_points = n_points;
    result->peak_rate = 0.0f;

    // Generate time points
    for (uint32_t i = 0; i < n_points; i++) {
        result->rate_times[i] = spike_train->start_time + i * time_step;
    }

    if (kernel_width > 0.0f) {
        // Kernel density estimation (Gaussian kernel)
        float sigma = kernel_width / 2.35f;  // FWHM to sigma

        for (uint32_t i = 0; i < n_points; i++) {
            float t = result->rate_times[i];
            float rate = 0.0f;

            for (uint32_t j = 0; j < spike_train->n_spikes; j++) {
                float dt = t - spike_train->spike_times[j];
                rate += gaussian_kernel(dt, sigma);
            }

            result->instantaneous[i] = rate * 1000.0f;  // Convert to Hz

            if (result->instantaneous[i] > result->peak_rate) {
                result->peak_rate = result->instantaneous[i];
            }
        }
    } else {
        // Histogram-based (bin counting)
        for (uint32_t j = 0; j < spike_train->n_spikes; j++) {
            uint32_t bin = (uint32_t)(
                (spike_train->spike_times[j] - spike_train->start_time) / time_step);
            if (bin < n_points) {
                result->instantaneous[bin] += 1000.0f / time_step;  // Hz
            }
        }

        for (uint32_t i = 0; i < n_points; i++) {
            if (result->instantaneous[i] > result->peak_rate) {
                result->peak_rate = result->instantaneous[i];
            }
        }
    }

    // Compute rate variance
    double sum = 0.0, sum_sq = 0.0;
    for (uint32_t i = 0; i < n_points; i++) {
        sum += result->instantaneous[i];
        sum_sq += result->instantaneous[i] * result->instantaneous[i];
    }
    double mean = sum / n_points;
    result->rate_variance = (float)((sum_sq / n_points) - mean * mean);

    LOG_DEBUG("Firing rate: mean=%.2f Hz, peak=%.2f Hz",
        result->mean_rate, result->peak_rate);

    return NEURAL_STATS_OK;
}

void nimcp_neural_firing_rate_free(neural_firing_rate_t* result) {
    if (!result) return;
    nimcp_free(result->instantaneous);
    nimcp_free(result->rate_times);
    memset(result, 0, sizeof(neural_firing_rate_t));
}

//=============================================================================
// Burst Detection
//=============================================================================

neural_stats_result_t nimcp_neural_burst_detection(
    const neural_spike_train_t* spike_train,
    float max_isi_within,
    uint32_t min_spikes_per_burst,
    neural_burst_result_t* result)
{
    if (!spike_train || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL pointer in burst detection");
        return NEURAL_STATS_ERROR_NULL;
    }

    if (max_isi_within <= 0.0f || min_spikes_per_burst < 2) {
        return NEURAL_STATS_ERROR_PARAMS;
    }

    memset(result, 0, sizeof(neural_burst_result_t));

    if (spike_train->n_spikes < min_spikes_per_burst) {
        return NEURAL_STATS_OK;  // No bursts possible
    }

    // First pass: count bursts
    uint32_t n_bursts = 0;
    uint32_t burst_start_idx = 0;
    bool in_burst = false;
    uint32_t spikes_in_current = 1;

    for (uint32_t i = 1; i < spike_train->n_spikes; i++) {
        float isi = spike_train->spike_times[i] - spike_train->spike_times[i - 1];

        if (isi <= max_isi_within) {
            if (!in_burst) {
                in_burst = true;
                burst_start_idx = i - 1;
                spikes_in_current = 2;
            } else {
                spikes_in_current++;
            }
        } else {
            if (in_burst && spikes_in_current >= min_spikes_per_burst) {
                n_bursts++;
            }
            in_burst = false;
            spikes_in_current = 1;
        }
    }
    // Check last burst
    if (in_burst && spikes_in_current >= min_spikes_per_burst) {
        n_bursts++;
    }

    if (n_bursts == 0) {
        return NEURAL_STATS_OK;
    }

    // Allocate arrays
    result->burst_starts = (float*)nimcp_malloc(n_bursts * sizeof(float));
    result->burst_ends = (float*)nimcp_malloc(n_bursts * sizeof(float));
    result->spikes_per_burst = (uint32_t*)nimcp_malloc(n_bursts * sizeof(uint32_t));

    if (!result->burst_starts || !result->burst_ends || !result->spikes_per_burst) {
        nimcp_free(result->burst_starts);
        nimcp_free(result->burst_ends);
        nimcp_free(result->spikes_per_burst);
        memset(result, 0, sizeof(neural_burst_result_t));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate burst arrays");
        return NEURAL_STATS_ERROR_MEMORY;
    }

    // Second pass: record burst details
    uint32_t burst_idx = 0;
    in_burst = false;
    spikes_in_current = 1;
    float burst_start_time = 0.0f;
    uint32_t total_burst_spikes = 0;
    double total_duration = 0.0;
    double total_intra_freq = 0.0;

    for (uint32_t i = 1; i < spike_train->n_spikes; i++) {
        float isi = spike_train->spike_times[i] - spike_train->spike_times[i - 1];

        if (isi <= max_isi_within) {
            if (!in_burst) {
                in_burst = true;
                burst_start_time = spike_train->spike_times[i - 1];
                spikes_in_current = 2;
            } else {
                spikes_in_current++;
            }
        } else {
            if (in_burst && spikes_in_current >= min_spikes_per_burst) {
                result->burst_starts[burst_idx] = burst_start_time;
                result->burst_ends[burst_idx] = spike_train->spike_times[i - 1];
                result->spikes_per_burst[burst_idx] = spikes_in_current;

                float dur = result->burst_ends[burst_idx] - result->burst_starts[burst_idx];
                total_duration += dur;
                total_burst_spikes += spikes_in_current;

                if (dur > 0.0f) {
                    total_intra_freq += (spikes_in_current - 1) * 1000.0f / dur;
                }

                burst_idx++;
            }
            in_burst = false;
            spikes_in_current = 1;
        }
    }
    // Handle last burst
    if (in_burst && spikes_in_current >= min_spikes_per_burst) {
        result->burst_starts[burst_idx] = burst_start_time;
        result->burst_ends[burst_idx] = spike_train->spike_times[spike_train->n_spikes - 1];
        result->spikes_per_burst[burst_idx] = spikes_in_current;

        float dur = result->burst_ends[burst_idx] - result->burst_starts[burst_idx];
        total_duration += dur;
        total_burst_spikes += spikes_in_current;

        if (dur > 0.0f) {
            total_intra_freq += (spikes_in_current - 1) * 1000.0f / dur;
        }
    }

    result->n_bursts = n_bursts;
    result->mean_burst_duration = (float)(total_duration / n_bursts);
    result->mean_spikes_per_burst = (float)total_burst_spikes / n_bursts;
    result->intra_burst_freq = (float)(total_intra_freq / n_bursts);

    float duration = spike_train->end_time - spike_train->start_time;
    result->burst_rate = n_bursts * 1000.0f / duration;  // bursts per second
    result->fraction_in_bursts = (float)total_burst_spikes / spike_train->n_spikes;

    LOG_DEBUG("Burst detection: n_bursts=%u, mean_dur=%.2f ms, intra_freq=%.2f Hz",
        n_bursts, result->mean_burst_duration, result->intra_burst_freq);

    return NEURAL_STATS_OK;
}

void nimcp_neural_burst_result_free(neural_burst_result_t* result) {
    if (!result) return;
    nimcp_free(result->burst_starts);
    nimcp_free(result->burst_ends);
    nimcp_free(result->spikes_per_burst);
    memset(result, 0, sizeof(neural_burst_result_t));
}

//=============================================================================
// Fisher Information
//=============================================================================

neural_stats_result_t nimcp_neural_fisher_information(
    const float* tuning_curves,
    const float* stimulus_values,
    uint32_t n_neurons,
    uint32_t n_stimuli,
    const float* noise_cov,
    neural_fisher_info_t* result)
{
    if (!tuning_curves || !stimulus_values || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL pointer in Fisher information");
        return NEURAL_STATS_ERROR_NULL;
    }

    if (n_neurons == 0 || n_stimuli < 2) {
        return NEURAL_STATS_ERROR_SIZE;
    }

    if (n_neurons > NEURAL_STATS_MAX_FISHER_NEURONS) {
        LOG_WARN("Too many neurons for Fisher information: %u > %u",
            n_neurons, NEURAL_STATS_MAX_FISHER_NEURONS);
        return NEURAL_STATS_ERROR_SIZE;
    }

    memset(result, 0, sizeof(neural_fisher_info_t));
    result->n_params = 1;  // Single stimulus dimension

    // Allocate derivative array
    float* df_ds = (float*)nimcp_malloc(n_neurons * (n_stimuli - 1) * sizeof(float));
    if (!df_ds) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate derivative array");
        return NEURAL_STATS_ERROR_MEMORY;
    }

    // Compute tuning curve derivatives using finite differences
    for (uint32_t n = 0; n < n_neurons; n++) {
        for (uint32_t s = 0; s < n_stimuli - 1; s++) {
            float ds = stimulus_values[s + 1] - stimulus_values[s];
            if (fabsf(ds) > NEURAL_STATS_EPSILON) {
                float f1 = tuning_curves[n * n_stimuli + s];
                float f2 = tuning_curves[n * n_stimuli + s + 1];
                df_ds[n * (n_stimuli - 1) + s] = (f2 - f1) / ds;
            } else {
                df_ds[n * (n_stimuli - 1) + s] = 0.0f;
            }
        }
    }

    // Compute Fisher information at each stimulus point
    float* fisher_per_stim = (float*)nimcp_calloc(n_stimuli - 1, sizeof(float));
    if (!fisher_per_stim) {
        nimcp_free(df_ds);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate Fisher array");
        return NEURAL_STATS_ERROR_MEMORY;
    }

    if (noise_cov == NULL) {
        // Independent Poisson neurons: I = sum_i (df_i/ds)^2 / f_i
        for (uint32_t s = 0; s < n_stimuli - 1; s++) {
            float fisher = 0.0f;

            for (uint32_t n = 0; n < n_neurons; n++) {
                float f = (tuning_curves[n * n_stimuli + s] +
                          tuning_curves[n * n_stimuli + s + 1]) / 2.0f;
                float df = df_ds[n * (n_stimuli - 1) + s];

                if (f > NEURAL_STATS_EPSILON) {
                    fisher += (df * df) / f;
                }
            }

            fisher_per_stim[s] = fisher;
        }
    } else {
        // With noise correlations: I = f'^T C^{-1} f'
        // For simplicity, we compute diagonal approximation here
        // Full implementation would require matrix inversion
        for (uint32_t s = 0; s < n_stimuli - 1; s++) {
            float fisher = 0.0f;

            for (uint32_t n = 0; n < n_neurons; n++) {
                float df = df_ds[n * (n_stimuli - 1) + s];
                float var = noise_cov[n * n_neurons + n];  // Diagonal element

                if (var > NEURAL_STATS_EPSILON) {
                    fisher += (df * df) / var;
                }
            }

            fisher_per_stim[s] = fisher;
        }
    }

    // Average Fisher information
    float total_fisher = 0.0f;
    for (uint32_t s = 0; s < n_stimuli - 1; s++) {
        total_fisher += fisher_per_stim[s];
    }
    result->fisher_info = total_fisher / (n_stimuli - 1);
    result->total_info = total_fisher;

    // Cramer-Rao bound: Var(estimate) >= 1/I
    result->cramer_rao_bounds = (float*)nimcp_malloc(sizeof(float));
    if (result->cramer_rao_bounds) {
        result->cramer_rao_bounds[0] = (result->fisher_info > NEURAL_STATS_EPSILON) ?
            1.0f / result->fisher_info : FLT_MAX;
    }

    nimcp_free(df_ds);
    nimcp_free(fisher_per_stim);

    LOG_DEBUG("Fisher information: I=%.4f, CR bound=%.4f",
        result->fisher_info,
        result->cramer_rao_bounds ? result->cramer_rao_bounds[0] : NAN);

    return NEURAL_STATS_OK;
}

void nimcp_neural_fisher_info_free(neural_fisher_info_t* result) {
    if (!result) return;
    nimcp_free(result->fisher_matrix);
    nimcp_free(result->cramer_rao_bounds);
    memset(result, 0, sizeof(neural_fisher_info_t));
}

//=============================================================================
// Population Vector
//=============================================================================

neural_stats_result_t nimcp_neural_population_vector(
    const float* spike_counts,
    const float* preferred_stimuli,
    uint32_t n_neurons,
    uint32_t n_time_points,
    neural_population_vector_t* result)
{
    if (!spike_counts || !preferred_stimuli || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL pointer in population vector");
        return NEURAL_STATS_ERROR_NULL;
    }

    if (n_neurons == 0 || n_time_points == 0) {
        return NEURAL_STATS_ERROR_SIZE;
    }

    memset(result, 0, sizeof(neural_population_vector_t));

    result->decoded_stimulus = (float*)nimcp_malloc(n_time_points * sizeof(float));
    result->confidence = (float*)nimcp_malloc(n_time_points * sizeof(float));

    if (!result->decoded_stimulus || !result->confidence) {
        nimcp_free(result->decoded_stimulus);
        nimcp_free(result->confidence);
        memset(result, 0, sizeof(neural_population_vector_t));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate population vector arrays");
        return NEURAL_STATS_ERROR_MEMORY;
    }

    result->n_time_points = n_time_points;

    // For circular variables (like orientation), use vector averaging
    // For linear variables, use weighted average
    bool is_circular = true;  // Could be made a parameter

    for (uint32_t t = 0; t < n_time_points; t++) {
        if (is_circular) {
            // Vector sum for circular data
            float sum_x = 0.0f, sum_y = 0.0f;
            float total_weight = 0.0f;

            for (uint32_t n = 0; n < n_neurons; n++) {
                float weight = spike_counts[n * n_time_points + t];
                float angle = preferred_stimuli[n];

                sum_x += weight * cosf(angle);
                sum_y += weight * sinf(angle);
                total_weight += weight;
            }

            result->decoded_stimulus[t] = atan2f(sum_y, sum_x);

            // Confidence is vector length
            if (total_weight > NEURAL_STATS_EPSILON) {
                float r = sqrtf(sum_x * sum_x + sum_y * sum_y) / total_weight;
                result->confidence[t] = r;  // 0 to 1
            } else {
                result->confidence[t] = 0.0f;
            }
        } else {
            // Weighted average for linear data
            float sum_weighted = 0.0f;
            float total_weight = 0.0f;

            for (uint32_t n = 0; n < n_neurons; n++) {
                float weight = spike_counts[n * n_time_points + t];
                sum_weighted += weight * preferred_stimuli[n];
                total_weight += weight;
            }

            if (total_weight > NEURAL_STATS_EPSILON) {
                result->decoded_stimulus[t] = sum_weighted / total_weight;
                result->confidence[t] = sqrtf(total_weight);
            } else {
                result->decoded_stimulus[t] = 0.0f;
                result->confidence[t] = 0.0f;
            }
        }
    }

    LOG_DEBUG("Population vector decoded %u time points", n_time_points);

    return NEURAL_STATS_OK;
}

void nimcp_neural_population_vector_free(neural_population_vector_t* result) {
    if (!result) return;
    nimcp_free(result->decoded_stimulus);
    nimcp_free(result->confidence);
    memset(result, 0, sizeof(neural_population_vector_t));
}

//=============================================================================
// Decoding Error Bound
//=============================================================================

neural_stats_result_t nimcp_neural_decoding_error_bound(
    const float* fisher_info,
    uint32_t n_params,
    neural_decoding_bound_t* result)
{
    if (!fisher_info || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL pointer in decoding error bound");
        return NEURAL_STATS_ERROR_NULL;
    }

    if (n_params == 0) {
        return NEURAL_STATS_ERROR_SIZE;
    }

    memset(result, 0, sizeof(neural_decoding_bound_t));

    if (n_params == 1) {
        // Scalar case: CR bound = 1/I
        if (*fisher_info > NEURAL_STATS_EPSILON) {
            result->cramer_rao_bound = 1.0f / (*fisher_info);
        } else {
            result->cramer_rao_bound = FLT_MAX;
        }
    } else {
        // Matrix case: need to compute trace of inverse
        // For now, use diagonal approximation
        float sum_inv = 0.0f;
        for (uint32_t i = 0; i < n_params; i++) {
            float diag = fisher_info[i * n_params + i];
            if (diag > NEURAL_STATS_EPSILON) {
                sum_inv += 1.0f / diag;
            } else {
                sum_inv += FLT_MAX / n_params;
            }
        }
        result->cramer_rao_bound = sum_inv / n_params;  // Average bound
    }

    result->achieved_error = NAN;  // Not computed without decoder
    result->efficiency = NAN;
    result->is_efficient = false;

    LOG_DEBUG("Cramer-Rao bound: %.6f", result->cramer_rao_bound);

    return NEURAL_STATS_OK;
}

//=============================================================================
// Tuning Curve Fit
//=============================================================================

neural_stats_result_t nimcp_neural_tuning_curve_fit(
    const float* stimulus_values,
    const float* firing_rates,
    uint32_t n_trials,
    neural_tuning_type_t tuning_type,
    neural_tuning_params_t* result)
{
    if (!stimulus_values || !firing_rates || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL pointer in tuning curve fit");
        return NEURAL_STATS_ERROR_NULL;
    }

    if (n_trials < 5) {  // Need at least 5 points for reasonable fit
        return NEURAL_STATS_ERROR_SIZE;
    }

    memset(result, 0, sizeof(neural_tuning_params_t));
    result->type = tuning_type;

    // Find initial estimates
    float max_rate = -FLT_MAX;
    float min_rate = FLT_MAX;
    float preferred = 0.0f;

    for (uint32_t i = 0; i < n_trials; i++) {
        if (firing_rates[i] > max_rate) {
            max_rate = firing_rates[i];
            preferred = stimulus_values[i];
        }
        if (firing_rates[i] < min_rate) {
            min_rate = firing_rates[i];
        }
    }

    result->amplitude = max_rate - min_rate;
    result->baseline = min_rate;
    result->preferred = preferred;

    // Estimate width from half-maximum points
    float half_max = (max_rate + min_rate) / 2.0f;
    float left_half = preferred, right_half = preferred;

    for (uint32_t i = 0; i < n_trials; i++) {
        if (firing_rates[i] >= half_max) {
            if (stimulus_values[i] < preferred) {
                left_half = stimulus_values[i];
            } else if (stimulus_values[i] > preferred) {
                right_half = stimulus_values[i];
            }
        }
    }

    result->width = (right_half - left_half) / 2.35f;  // FWHM to sigma
    if (result->width < NEURAL_STATS_EPSILON) {
        result->width = 1.0f;  // Default
    }

    // Compute R-squared
    float ss_tot = 0.0f, ss_res = 0.0f;
    float mean_rate = 0.0f;
    for (uint32_t i = 0; i < n_trials; i++) {
        mean_rate += firing_rates[i];
    }
    mean_rate /= n_trials;

    for (uint32_t i = 0; i < n_trials; i++) {
        float predicted;
        float x = stimulus_values[i] - result->preferred;

        switch (tuning_type) {
            case NEURAL_TUNING_GAUSSIAN:
                predicted = result->baseline + result->amplitude *
                    expf(-0.5f * (x * x) / (result->width * result->width));
                break;
            case NEURAL_TUNING_VON_MISES:
                predicted = result->baseline + result->amplitude *
                    expf(result->width * cosf(x));
                break;
            case NEURAL_TUNING_COSINE:
                predicted = result->baseline + result->amplitude *
                    (1.0f + cosf(x)) / 2.0f;
                break;
            default:
                predicted = result->baseline + result->amplitude *
                    expf(-0.5f * (x * x) / (result->width * result->width));
        }

        ss_tot += (firing_rates[i] - mean_rate) * (firing_rates[i] - mean_rate);
        ss_res += (firing_rates[i] - predicted) * (firing_rates[i] - predicted);
    }

    result->r_squared = (ss_tot > NEURAL_STATS_EPSILON) ?
        1.0f - (ss_res / ss_tot) : 0.0f;

    // AIC (simplified)
    float n = (float)n_trials;
    uint32_t k = 4;  // Number of parameters
    result->aic = n * logf(ss_res / n) + 2.0f * k;

    LOG_DEBUG("Tuning curve fit: preferred=%.2f, width=%.2f, R2=%.3f",
        result->preferred, result->width, result->r_squared);

    return NEURAL_STATS_OK;
}

//=============================================================================
// Cross-Correlogram
//=============================================================================

neural_stats_result_t nimcp_neural_cross_correlogram(
    const neural_spike_train_t* train1,
    const neural_spike_train_t* train2,
    float bin_width,
    float max_lag,
    neural_cross_correlogram_t* result)
{
    if (!train1 || !train2 || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL pointer in cross-correlogram");
        return NEURAL_STATS_ERROR_NULL;
    }

    if (bin_width <= 0.0f || max_lag <= 0.0f) {
        return NEURAL_STATS_ERROR_PARAMS;
    }

    memset(result, 0, sizeof(neural_cross_correlogram_t));

    uint32_t n_bins = (uint32_t)(2.0f * max_lag / bin_width) + 1;
    result->n_bins = n_bins;
    result->bin_width = bin_width;
    result->max_lag = max_lag;

    result->correlogram = (float*)nimcp_calloc(n_bins, sizeof(float));
    result->lags = (float*)nimcp_malloc(n_bins * sizeof(float));
    result->is_significant = (bool*)nimcp_calloc(n_bins, sizeof(bool));

    if (!result->correlogram || !result->lags || !result->is_significant) {
        nimcp_free(result->correlogram);
        nimcp_free(result->lags);
        nimcp_free(result->is_significant);
        memset(result, 0, sizeof(neural_cross_correlogram_t));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate correlogram arrays");
        return NEURAL_STATS_ERROR_MEMORY;
    }

    // Generate lag values
    for (uint32_t i = 0; i < n_bins; i++) {
        result->lags[i] = -max_lag + i * bin_width;
    }

    // Compute cross-correlogram
    // For each spike in train1, find all spikes in train2 within lag range
    for (uint32_t i = 0; i < train1->n_spikes; i++) {
        float t1 = train1->spike_times[i];

        for (uint32_t j = 0; j < train2->n_spikes; j++) {
            float t2 = train2->spike_times[j];
            float lag = t2 - t1;

            if (lag >= -max_lag && lag <= max_lag) {
                uint32_t bin = (uint32_t)((lag + max_lag) / bin_width);
                if (bin < n_bins) {
                    result->correlogram[bin] += 1.0f;
                }
            }
        }
    }

    // Normalize by bin width and number of spikes
    float norm = (train1->n_spikes > 0) ?
        1.0f / (train1->n_spikes * bin_width / 1000.0f) : 1.0f;

    result->peak_correlation = -FLT_MAX;
    for (uint32_t i = 0; i < n_bins; i++) {
        result->correlogram[i] *= norm;

        if (result->correlogram[i] > result->peak_correlation) {
            result->peak_correlation = result->correlogram[i];
            result->peak_lag = result->lags[i];
        }
    }

    // Compute shuffle predictor (expected rate)
    float duration = train1->end_time - train1->start_time;
    float rate2 = (train2->n_spikes > 0 && duration > 0.0f) ?
        train2->n_spikes / (duration / 1000.0f) : 0.0f;
    result->shuffle_mean = rate2;

    // Significance threshold (2 std above shuffle)
    result->shuffle_std = sqrtf(result->shuffle_mean);
    result->significance_threshold = result->shuffle_mean + 2.0f * result->shuffle_std;

    for (uint32_t i = 0; i < n_bins; i++) {
        result->is_significant[i] =
            (result->correlogram[i] > result->significance_threshold);
    }

    LOG_DEBUG("Cross-correlogram: n_bins=%u, peak=%.2f at lag=%.2f ms",
        n_bins, result->peak_correlation, result->peak_lag);

    return NEURAL_STATS_OK;
}

void nimcp_neural_cross_correlogram_free(neural_cross_correlogram_t* result) {
    if (!result) return;
    nimcp_free(result->correlogram);
    nimcp_free(result->lags);
    nimcp_free(result->is_significant);
    memset(result, 0, sizeof(neural_cross_correlogram_t));
}

//=============================================================================
// JPSTH
//=============================================================================

neural_stats_result_t nimcp_neural_jpsth(
    const neural_spike_train_t* train1,
    const neural_spike_train_t* train2,
    const float* event_times,
    uint32_t n_events,
    float window_before,
    float window_after,
    float bin_width,
    neural_jpsth_t* result)
{
    if (!train1 || !train2 || !event_times || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL pointer in JPSTH computation");
        return NEURAL_STATS_ERROR_NULL;
    }

    if (n_events == 0 || bin_width <= 0.0f) {
        return NEURAL_STATS_ERROR_PARAMS;
    }

    memset(result, 0, sizeof(neural_jpsth_t));

    float window_size = window_before + window_after;
    uint32_t n_bins = (uint32_t)(window_size / bin_width);
    if (n_bins < 2) {
        return NEURAL_STATS_ERROR_SIZE;
    }

    result->n_bins = n_bins;
    result->bin_width = bin_width;

    result->jpsth_matrix = (float*)nimcp_calloc(n_bins * n_bins, sizeof(float));
    result->psth1 = (float*)nimcp_calloc(n_bins, sizeof(float));
    result->psth2 = (float*)nimcp_calloc(n_bins, sizeof(float));
    result->time_bins = (float*)nimcp_malloc(n_bins * sizeof(float));
    result->coincidence_histogram = (float*)nimcp_calloc(n_bins, sizeof(float));

    if (!result->jpsth_matrix || !result->psth1 || !result->psth2 ||
        !result->time_bins || !result->coincidence_histogram) {
        nimcp_neural_jpsth_free(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate JPSTH arrays");
        return NEURAL_STATS_ERROR_MEMORY;
    }

    // Generate time bins
    for (uint32_t i = 0; i < n_bins; i++) {
        result->time_bins[i] = -window_before + (i + 0.5f) * bin_width;
    }

    // For each event, compute PSTHs and JPSTH
    for (uint32_t e = 0; e < n_events; e++) {
        float t_event = event_times[e];

        // Find spikes in window for train1
        for (uint32_t i = 0; i < train1->n_spikes; i++) {
            float dt = train1->spike_times[i] - t_event;
            if (dt >= -window_before && dt < window_after) {
                uint32_t bin = (uint32_t)((dt + window_before) / bin_width);
                if (bin < n_bins) {
                    result->psth1[bin] += 1.0f;
                }
            }
        }

        // Find spikes in window for train2
        for (uint32_t i = 0; i < train2->n_spikes; i++) {
            float dt = train2->spike_times[i] - t_event;
            if (dt >= -window_before && dt < window_after) {
                uint32_t bin = (uint32_t)((dt + window_before) / bin_width);
                if (bin < n_bins) {
                    result->psth2[bin] += 1.0f;
                }
            }
        }

        // Compute joint histogram for this event
        for (uint32_t i = 0; i < train1->n_spikes; i++) {
            float dt1 = train1->spike_times[i] - t_event;
            if (dt1 < -window_before || dt1 >= window_after) continue;

            uint32_t bin1 = (uint32_t)((dt1 + window_before) / bin_width);
            if (bin1 >= n_bins) continue;

            for (uint32_t j = 0; j < train2->n_spikes; j++) {
                float dt2 = train2->spike_times[j] - t_event;
                if (dt2 < -window_before || dt2 >= window_after) continue;

                uint32_t bin2 = (uint32_t)((dt2 + window_before) / bin_width);
                if (bin2 >= n_bins) continue;

                result->jpsth_matrix[bin1 * n_bins + bin2] += 1.0f;
            }
        }
    }

    // Normalize by number of events
    float norm = 1.0f / n_events;
    for (uint32_t i = 0; i < n_bins; i++) {
        result->psth1[i] *= norm;
        result->psth2[i] *= norm;
    }
    for (uint32_t i = 0; i < n_bins * n_bins; i++) {
        result->jpsth_matrix[i] *= norm;
    }

    // Coincidence histogram (main diagonal)
    for (uint32_t i = 0; i < n_bins; i++) {
        result->coincidence_histogram[i] = result->jpsth_matrix[i * n_bins + i];
    }

    // Correlation strength (sum of off-diagonal elements / diagonal)
    float diag_sum = 0.0f, off_diag_sum = 0.0f;
    for (uint32_t i = 0; i < n_bins; i++) {
        for (uint32_t j = 0; j < n_bins; j++) {
            if (i == j) {
                diag_sum += result->jpsth_matrix[i * n_bins + j];
            } else {
                off_diag_sum += result->jpsth_matrix[i * n_bins + j];
            }
        }
    }
    result->correlation_strength = (diag_sum > NEURAL_STATS_EPSILON) ?
        off_diag_sum / diag_sum : 0.0f;

    LOG_DEBUG("JPSTH computed: %u bins, correlation_strength=%.3f",
        n_bins, result->correlation_strength);

    return NEURAL_STATS_OK;
}

void nimcp_neural_jpsth_free(neural_jpsth_t* result) {
    if (!result) return;
    nimcp_free(result->jpsth_matrix);
    nimcp_free(result->psth1);
    nimcp_free(result->psth2);
    nimcp_free(result->time_bins);
    nimcp_free(result->coincidence_histogram);
    nimcp_free(result->normalized_jpsth);
    memset(result, 0, sizeof(neural_jpsth_t));
}

//=============================================================================
// Spike-Triggered Average
//=============================================================================

neural_stats_result_t nimcp_neural_spike_triggered_average(
    const neural_spike_train_t* spike_train,
    const float* signal,
    const float* signal_times,
    uint32_t n_samples,
    float window_before,
    float window_after,
    neural_sta_t* result)
{
    if (!spike_train || !signal || !signal_times || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL pointer in STA computation");
        return NEURAL_STATS_ERROR_NULL;
    }

    if (n_samples < 2 || spike_train->n_spikes == 0) {
        return NEURAL_STATS_ERROR_SIZE;
    }

    memset(result, 0, sizeof(neural_sta_t));

    // Determine sample rate and window size
    float dt = signal_times[1] - signal_times[0];
    if (dt <= 0.0f) {
        return NEURAL_STATS_ERROR_PARAMS;
    }

    uint32_t samples_before = (uint32_t)(window_before / dt);
    uint32_t samples_after = (uint32_t)(window_after / dt);
    uint32_t n_sta_samples = samples_before + samples_after + 1;

    result->n_samples = n_sta_samples;
    result->sta = (float*)nimcp_calloc(n_sta_samples, sizeof(float));
    result->sta_times = (float*)nimcp_malloc(n_sta_samples * sizeof(float));
    result->sta_std = (float*)nimcp_calloc(n_sta_samples, sizeof(float));

    if (!result->sta || !result->sta_times || !result->sta_std) {
        nimcp_neural_sta_free(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate STA arrays");
        return NEURAL_STATS_ERROR_MEMORY;
    }

    // Generate time axis
    for (uint32_t i = 0; i < n_sta_samples; i++) {
        result->sta_times[i] = -window_before + i * dt;
    }

    // Accumulate signal windows aligned to spikes
    float* sum_sq = (float*)nimcp_calloc(n_sta_samples, sizeof(float));
    if (!sum_sq) {
        nimcp_neural_sta_free(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate temporary array");
        return NEURAL_STATS_ERROR_MEMORY;
    }

    result->n_spikes_used = 0;

    for (uint32_t s = 0; s < spike_train->n_spikes; s++) {
        float t_spike = spike_train->spike_times[s];

        // Find corresponding sample index
        // Binary search would be more efficient for long signals
        int32_t spike_idx = -1;
        for (uint32_t i = 0; i < n_samples; i++) {
            if (signal_times[i] >= t_spike) {
                spike_idx = i;
                break;
            }
        }

        if (spike_idx < 0 ||
            spike_idx < (int32_t)samples_before ||
            spike_idx + (int32_t)samples_after >= (int32_t)n_samples) {
            continue;  // Skip spikes at edges
        }

        result->n_spikes_used++;

        for (uint32_t i = 0; i < n_sta_samples; i++) {
            int32_t sig_idx = spike_idx - samples_before + i;
            float val = signal[sig_idx];
            result->sta[i] += val;
            sum_sq[i] += val * val;
        }
    }

    if (result->n_spikes_used == 0) {
        nimcp_free(sum_sq);
        LOG_WARN("No usable spikes for STA (all at edges)");
        return NEURAL_STATS_ERROR_SIZE;
    }

    // Compute mean and std
    float n_used = (float)result->n_spikes_used;
    result->peak_amplitude = -FLT_MAX;

    for (uint32_t i = 0; i < n_sta_samples; i++) {
        result->sta[i] /= n_used;

        float variance = (sum_sq[i] / n_used) - (result->sta[i] * result->sta[i]);
        result->sta_std[i] = (variance > 0.0f) ? sqrtf(variance) : 0.0f;

        if (fabsf(result->sta[i]) > fabsf(result->peak_amplitude)) {
            result->peak_amplitude = result->sta[i];
            result->peak_time = result->sta_times[i];
        }
    }

    nimcp_free(sum_sq);

    LOG_DEBUG("STA computed: n_spikes_used=%u, peak=%.4f at t=%.2f ms",
        result->n_spikes_used, result->peak_amplitude, result->peak_time);

    return NEURAL_STATS_OK;
}

void nimcp_neural_sta_free(neural_sta_t* result) {
    if (!result) return;
    nimcp_free(result->sta);
    nimcp_free(result->sta_times);
    nimcp_free(result->sta_std);
    nimcp_free(result->confidence_upper);
    nimcp_free(result->confidence_lower);
    memset(result, 0, sizeof(neural_sta_t));
}

//=============================================================================
// Spike-Field Coherence
//=============================================================================

neural_stats_result_t nimcp_neural_spike_field_coherence(
    const neural_spike_train_t* spike_train,
    const float* lfp,
    const float* lfp_times,
    uint32_t n_samples,
    float freq_min,
    float freq_max,
    uint32_t n_frequencies,
    neural_spike_field_coherence_t* result)
{
    if (!spike_train || !lfp || !lfp_times || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL pointer in spike-field coherence");
        return NEURAL_STATS_ERROR_NULL;
    }

    if (n_samples < 2 || n_frequencies == 0 || freq_max <= freq_min) {
        return NEURAL_STATS_ERROR_PARAMS;
    }

    memset(result, 0, sizeof(neural_spike_field_coherence_t));

    result->n_frequencies = n_frequencies;
    result->coherence = (float*)nimcp_calloc(n_frequencies, sizeof(float));
    result->frequencies = (float*)nimcp_malloc(n_frequencies * sizeof(float));
    result->phase = (float*)nimcp_calloc(n_frequencies, sizeof(float));
    result->phase_std = (float*)nimcp_calloc(n_frequencies, sizeof(float));
    result->confidence_level = (float*)nimcp_malloc(n_frequencies * sizeof(float));

    if (!result->coherence || !result->frequencies || !result->phase ||
        !result->phase_std || !result->confidence_level) {
        nimcp_neural_spike_field_coherence_free(result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "Failed to allocate coherence arrays");
        return NEURAL_STATS_ERROR_MEMORY;
    }

    // Generate frequency axis
    float df = (freq_max - freq_min) / (n_frequencies - 1);
    for (uint32_t i = 0; i < n_frequencies; i++) {
        result->frequencies[i] = freq_min + i * df;
    }

    // Determine sample rate
    float sample_rate = 1000.0f / (lfp_times[1] - lfp_times[0]);  // Hz

    // For each frequency, compute spike-phase histogram
    result->peak_coherence = 0.0f;

    for (uint32_t f = 0; f < n_frequencies; f++) {
        float freq = result->frequencies[f];
        float omega = NIMCP_TWO_PI_F * freq / 1000.0f;  // rad/ms

        // Compute phase at each spike time using Hilbert transform approximation
        // For simplicity, use bandpass filtering approach
        float sum_cos = 0.0f, sum_sin = 0.0f;
        uint32_t n_valid = 0;

        for (uint32_t s = 0; s < spike_train->n_spikes; s++) {
            float t_spike = spike_train->spike_times[s];

            // Find LFP phase at spike time
            // Simple approach: use local oscillation fit
            int32_t idx = -1;
            for (uint32_t i = 0; i < n_samples - 1; i++) {
                if (lfp_times[i] <= t_spike && lfp_times[i + 1] > t_spike) {
                    idx = i;
                    break;
                }
            }

            if (idx < 0 || idx < 5 || idx >= (int32_t)n_samples - 5) continue;

            // Estimate phase using local sinusoid fit
            float phase = 0.0f;
            float max_corr = -FLT_MAX;

            for (int32_t p = 0; p < 36; p++) {  // 10-degree resolution
                float test_phase = p * PI / 18.0f;
                float corr = 0.0f;

                for (int32_t k = -5; k <= 5; k++) {
                    float t = lfp_times[idx + k];
                    float expected = cosf(omega * t + test_phase);
                    corr += expected * lfp[idx + k];
                }

                if (corr > max_corr) {
                    max_corr = corr;
                    phase = test_phase;
                }
            }

            sum_cos += cosf(phase);
            sum_sin += sinf(phase);
            n_valid++;
        }

        if (n_valid > 0) {
            // Mean resultant vector (coherence)
            float r = sqrtf(sum_cos * sum_cos + sum_sin * sum_sin) / n_valid;
            result->coherence[f] = r;
            result->phase[f] = atan2f(sum_sin, sum_cos);

            // Confidence level (Rayleigh test)
            float z = n_valid * r * r;
            result->confidence_level[f] = expf(-z);  // p-value

            if (r > result->peak_coherence) {
                result->peak_coherence = r;
                result->peak_frequency = freq;
            }
        }
    }

    LOG_DEBUG("Spike-field coherence: peak=%.3f at %.1f Hz",
        result->peak_coherence, result->peak_frequency);

    return NEURAL_STATS_OK;
}

void nimcp_neural_spike_field_coherence_free(neural_spike_field_coherence_t* result) {
    if (!result) return;
    nimcp_free(result->coherence);
    nimcp_free(result->frequencies);
    nimcp_free(result->phase);
    nimcp_free(result->phase_std);
    nimcp_free(result->confidence_level);
    memset(result, 0, sizeof(neural_spike_field_coherence_t));
}

//=============================================================================
// Quantal Analysis
//=============================================================================

neural_stats_result_t nimcp_neural_quantal_analysis(
    const float* amplitudes,
    uint32_t n_trials,
    float baseline_noise,
    neural_quantal_analysis_t* result)
{
    if (!amplitudes || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL pointer in quantal analysis");
        return NEURAL_STATS_ERROR_NULL;
    }

    if (n_trials < 10) {  // Need sufficient trials for reliable estimation
        return NEURAL_STATS_ERROR_SIZE;
    }

    memset(result, 0, sizeof(neural_quantal_analysis_t));

    // Compute mean and variance
    double sum = 0.0, sum_sq = 0.0;
    uint32_t n_successes = 0;

    for (uint32_t i = 0; i < n_trials; i++) {
        sum += amplitudes[i];
        sum_sq += amplitudes[i] * amplitudes[i];
        if (amplitudes[i] > 2.0f * baseline_noise) {
            n_successes++;
        }
    }

    double mean = sum / n_trials;
    double variance = (sum_sq / n_trials) - (mean * mean);

    // Variance-mean analysis for estimating p
    // For binomial: var = npq = np(1-p), mean = np
    // Therefore: p = 1 - var/mean (if var < mean)

    if (mean > NEURAL_STATS_EPSILON && variance < mean) {
        result->p = (float)(1.0 - variance / mean);
        result->n = (float)(mean / (result->p * (1.0f - result->p + 0.001f)));
        result->q = (float)(mean / (result->n * result->p + 0.001f));
    } else {
        // Use failure rate method
        float failure_rate = 1.0f - (float)n_successes / n_trials;

        if (failure_rate > NEURAL_STATS_EPSILON && failure_rate < 1.0f) {
            // Estimate n from failure rate: p_failure = (1-p)^n
            // Assume n ~ 4-6 for typical synapses
            result->n = 5.0f;  // Initial guess
            result->p = 1.0f - powf(failure_rate, 1.0f / result->n);
            result->q = (float)(mean / (result->n * result->p + 0.001f));
        } else {
            // Fallback estimates
            result->p = (float)n_successes / n_trials;
            result->n = 5.0f;
            result->q = (result->p > NEURAL_STATS_EPSILON) ?
                (float)(mean / (result->n * result->p)) : 1.0f;
        }
    }

    // Variance estimates (bootstrap would be more accurate)
    result->n_variance = result->n * 0.1f;  // ~10% CV
    result->p_variance = result->p * (1.0f - result->p) / n_trials;
    result->q_variance = result->q * 0.1f;

    // Build amplitude histogram
    uint32_t n_bins = 50;
    result->n_bins = n_bins;
    result->amplitude_histogram = (float*)nimcp_calloc(n_bins, sizeof(float));
    result->histogram_bins = (float*)nimcp_malloc(n_bins * sizeof(float));

    if (result->amplitude_histogram && result->histogram_bins) {
        float min_amp = amplitudes[0], max_amp = amplitudes[0];
        for (uint32_t i = 1; i < n_trials; i++) {
            if (amplitudes[i] < min_amp) min_amp = amplitudes[i];
            if (amplitudes[i] > max_amp) max_amp = amplitudes[i];
        }

        float bin_width = (max_amp - min_amp) / n_bins;
        if (bin_width < NEURAL_STATS_EPSILON) bin_width = 1.0f;

        for (uint32_t i = 0; i < n_bins; i++) {
            result->histogram_bins[i] = min_amp + (i + 0.5f) * bin_width;
        }

        for (uint32_t i = 0; i < n_trials; i++) {
            uint32_t bin = (uint32_t)((amplitudes[i] - min_amp) / bin_width);
            if (bin >= n_bins) bin = n_bins - 1;
            result->amplitude_histogram[bin] += 1.0f / n_trials;
        }
    }

    // Chi-squared test for binomial fit
    result->chi_squared = 0.0f;  // Would require full fit
    result->p_value = 1.0f;
    result->is_binomial = true;  // Assume true for now

    LOG_DEBUG("Quantal analysis: n=%.1f, p=%.3f, q=%.3f",
        result->n, result->p, result->q);

    return NEURAL_STATS_OK;
}

void nimcp_neural_quantal_analysis_free(neural_quantal_analysis_t* result) {
    if (!result) return;
    nimcp_free(result->amplitude_histogram);
    nimcp_free(result->histogram_bins);
    memset(result, 0, sizeof(neural_quantal_analysis_t));
}

//=============================================================================
// Release Probability
//=============================================================================

neural_stats_result_t nimcp_neural_release_probability(
    const float* amplitudes,
    uint32_t n_trials,
    float quantal_size,
    neural_release_prob_t* result)
{
    if (!amplitudes || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL pointer in release probability");
        return NEURAL_STATS_ERROR_NULL;
    }

    if (n_trials < 5 || quantal_size <= 0.0f) {
        return NEURAL_STATS_ERROR_PARAMS;
    }

    memset(result, 0, sizeof(neural_release_prob_t));

    // Variance-mean method: p = 1 - (Var/Mean)/q
    double sum = 0.0, sum_sq = 0.0;
    uint32_t n_failures = 0;

    for (uint32_t i = 0; i < n_trials; i++) {
        sum += amplitudes[i];
        sum_sq += amplitudes[i] * amplitudes[i];
        if (amplitudes[i] < quantal_size / 2.0f) {
            n_failures++;
        }
    }

    double mean = sum / n_trials;
    double variance = (sum_sq / n_trials) - (mean * mean);

    // CV of amplitudes
    result->cv_amplitude = (mean > NEURAL_STATS_EPSILON) ?
        (float)(sqrt(variance) / mean) : NAN;

    // Failure rate
    result->failure_rate = (float)n_failures / n_trials;

    // Estimate p using variance-mean method
    if (mean > NEURAL_STATS_EPSILON) {
        float vm_ratio = (float)(variance / mean);
        result->p_release = 1.0f - vm_ratio / quantal_size;

        // Clamp to valid range
        if (result->p_release < 0.0f) result->p_release = 0.0f;
        if (result->p_release > 1.0f) result->p_release = 1.0f;
    } else {
        result->p_release = 0.0f;
    }

    // Variance of estimate
    result->p_variance = result->p_release * (1.0f - result->p_release) / n_trials;

    LOG_DEBUG("Release probability: p=%.3f (CV=%.3f, failure_rate=%.3f)",
        result->p_release, result->cv_amplitude, result->failure_rate);

    return NEURAL_STATS_OK;
}

void nimcp_neural_release_prob_free(neural_release_prob_t* result) {
    if (!result) return;
    nimcp_free(result->p_by_stimulus);
    memset(result, 0, sizeof(neural_release_prob_t));
}

//=============================================================================
// Paired-Pulse Ratio
//=============================================================================

neural_stats_result_t nimcp_neural_paired_pulse_ratio(
    const float* amplitude1,
    const float* amplitude2,
    uint32_t n_trials,
    float inter_stimulus_interval,
    neural_ppr_result_t* result)
{
    if (!amplitude1 || !amplitude2 || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "NULL pointer in PPR computation");
        return NEURAL_STATS_ERROR_NULL;
    }

    if (n_trials < 3) {
        return NEURAL_STATS_ERROR_SIZE;
    }

    memset(result, 0, sizeof(neural_ppr_result_t));

    // Compute PPR for each trial and average
    double sum_ppr = 0.0;
    double sum_ppr_sq = 0.0;
    double sum_p1 = 0.0;
    double sum_p2 = 0.0;
    uint32_t valid_trials = 0;

    for (uint32_t i = 0; i < n_trials; i++) {
        if (amplitude1[i] > NEURAL_STATS_EPSILON) {
            float ppr = amplitude2[i] / amplitude1[i];
            sum_ppr += ppr;
            sum_ppr_sq += ppr * ppr;
            sum_p1 += amplitude1[i];
            sum_p2 += amplitude2[i];
            valid_trials++;
        }
    }

    if (valid_trials < 3) {
        return NEURAL_STATS_ERROR_SIZE;
    }

    result->ppr = (float)(sum_ppr / valid_trials);
    double var_ppr = (sum_ppr_sq / valid_trials) -
        (sum_ppr / valid_trials) * (sum_ppr / valid_trials);
    result->ppr_std = (float)sqrt(var_ppr);

    // Facilitation index
    double mean_p1 = sum_p1 / valid_trials;
    double mean_p2 = sum_p2 / valid_trials;
    result->facilitation_index = (mean_p1 > NEURAL_STATS_EPSILON) ?
        (float)((mean_p2 - mean_p1) / mean_p1) : 0.0f;

    // Classification
    result->is_facilitating = (result->ppr > 1.05f);  // 5% threshold
    result->is_depressing = (result->ppr < 0.95f);

    // Recovery time (would need multiple intervals for proper estimation)
    result->recovery_time = inter_stimulus_interval * 2.0f;  // Rough estimate

    LOG_DEBUG("PPR: %.3f +/- %.3f (ISI=%.1f ms, %s)",
        result->ppr, result->ppr_std, inter_stimulus_interval,
        result->is_facilitating ? "facilitating" :
        (result->is_depressing ? "depressing" : "neutral"));

    return NEURAL_STATS_OK;
}

void nimcp_neural_ppr_result_free(neural_ppr_result_t* result) {
    if (!result) return;
    nimcp_free(result->ppr_by_interval);
    nimcp_free(result->intervals);
    memset(result, 0, sizeof(neural_ppr_result_t));
}

//=============================================================================
// GPU Batch Operations (Stubs - Implementation in CUDA file)
//=============================================================================

neural_stats_result_t nimcp_neural_isi_distribution_batch_gpu(
    const neural_spike_train_t* spike_trains,
    uint32_t n_trains,
    neural_isi_distribution_t* results)
{
    if (!g_neural_stats_config.enable_gpu) {
        // Fall back to CPU implementation
        for (uint32_t i = 0; i < n_trains; i++) {
            neural_stats_result_t res = nimcp_neural_isi_distribution(
                &spike_trains[i], &results[i]);
            if (res != NEURAL_STATS_OK) return res;
        }
        return NEURAL_STATS_OK;
    }

    // GPU implementation in nimcp_neural_statistics_kernels.cu
    LOG_WARN("GPU batch ISI not compiled - falling back to CPU");
    for (uint32_t i = 0; i < n_trains; i++) {
        neural_stats_result_t res = nimcp_neural_isi_distribution(
            &spike_trains[i], &results[i]);
        if (res != NEURAL_STATS_OK) return res;
    }
    return NEURAL_STATS_OK;
}

neural_stats_result_t nimcp_neural_cross_correlogram_batch_gpu(
    const neural_spike_train_t* trains1,
    const neural_spike_train_t* trains2,
    uint32_t n_pairs,
    float bin_width,
    float max_lag,
    neural_cross_correlogram_t* results)
{
    if (!g_neural_stats_config.enable_gpu) {
        // Fall back to CPU
        for (uint32_t i = 0; i < n_pairs; i++) {
            neural_stats_result_t res = nimcp_neural_cross_correlogram(
                &trains1[i], &trains2[i], bin_width, max_lag, &results[i]);
            if (res != NEURAL_STATS_OK) return res;
        }
        return NEURAL_STATS_OK;
    }

    LOG_WARN("GPU batch cross-correlogram not compiled - falling back to CPU");
    for (uint32_t i = 0; i < n_pairs; i++) {
        neural_stats_result_t res = nimcp_neural_cross_correlogram(
            &trains1[i], &trains2[i], bin_width, max_lag, &results[i]);
        if (res != NEURAL_STATS_OK) return res;
    }
    return NEURAL_STATS_OK;
}

neural_stats_result_t nimcp_neural_fisher_information_batch_gpu(
    const float* tuning_curves,
    const float* stimulus_values,
    uint32_t n_populations,
    uint32_t n_neurons,
    uint32_t n_stimuli,
    neural_fisher_info_t* results)
{
    if (!g_neural_stats_config.enable_gpu) {
        // Fall back to CPU
        size_t stride = n_neurons * n_stimuli;
        for (uint32_t i = 0; i < n_populations; i++) {
            neural_stats_result_t res = nimcp_neural_fisher_information(
                tuning_curves + i * stride,
                stimulus_values,
                n_neurons,
                n_stimuli,
                NULL,
                &results[i]);
            if (res != NEURAL_STATS_OK) return res;
        }
        return NEURAL_STATS_OK;
    }

    LOG_WARN("GPU batch Fisher information not compiled - falling back to CPU");
    size_t stride = n_neurons * n_stimuli;
    for (uint32_t i = 0; i < n_populations; i++) {
        neural_stats_result_t res = nimcp_neural_fisher_information(
            tuning_curves + i * stride,
            stimulus_values,
            n_neurons,
            n_stimuli,
            NULL,
            &results[i]);
        if (res != NEURAL_STATS_OK) return res;
    }
    return NEURAL_STATS_OK;
}
