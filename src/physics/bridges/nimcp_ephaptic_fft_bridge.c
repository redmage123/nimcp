#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_ephaptic_fft_bridge.c - Ephaptic FFT Bridge Implementation
//=============================================================================
/**
 * @file nimcp_ephaptic_fft_bridge.c
 * @brief Implementation of FFT-based spectral analysis for Ephaptic LFP
 *
 * WHAT: Replaces hardcoded band power with proper FFT-based spectral analysis
 * WHY:  Enable accurate measurement of oscillatory power in neural signals
 * HOW:  Circular buffer for LFP time series, FFT with windowing, band integration
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "physics/bridges/nimcp_ephaptic_fft_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(ephaptic_fft_bridge)

#define LOG_MODULE "EPHAPTIC_FFT_BRIDGE"


//=============================================================================
// Internal Constants
//=============================================================================

#define PI NIMCP_PI_F
#define LN_2 0.693147180559945f

/** Small value for numerical stability */
#define EPSILON 1e-10f

//=============================================================================
// Internal Structure
//=============================================================================

struct ephaptic_fft_bridge_struct {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /** Configuration */
    ephaptic_fft_config_t config;

    /** FFT plan (pre-allocated for efficiency) */
    fft_plan_t* fft_plan;

    /** Circular buffer for LFP samples */
    float* lfp_buffer;

    /** Write index in circular buffer */
    uint32_t buffer_write_idx;

    /** Number of samples in buffer */
    uint32_t buffer_count;

    /** Working array for windowed signal */
    float* windowed_signal;

    /** FFT output spectrum */
    fft_complex_t* spectrum;

    /** Power spectrum */
    float* power_spectrum;

    /** Number of frequency bins */
    uint32_t num_bins;

    /** Last computed band power (for averaging) */
    ephaptic_fft_result_t last_result;

    /** Averaged band power (exponential moving average) */
    ephaptic_fft_result_t averaged_result;

    /** Has averaged result been initialized */
    bool averaging_initialized;

    /** Last timestamp */
    float last_timestamp;

    /** Initialization flag */
    bool initialized;
};

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Compute power in a frequency band from power spectrum
 */
static float compute_band_power_internal(
    const float* power,
    uint32_t num_bins,
    float sampling_rate,
    uint32_t fft_size,
    float freq_low,
    float freq_high
) {
    /* Convert frequencies to bin indices */
    float freq_resolution = sampling_rate / (float)fft_size;
    uint32_t bin_low = (uint32_t)(freq_low / freq_resolution);
    uint32_t bin_high = (uint32_t)(freq_high / freq_resolution);

    /* Clamp to valid range */
    if (bin_low >= num_bins) bin_low = num_bins - 1;
    if (bin_high >= num_bins) bin_high = num_bins - 1;

    /* Sum power in band */
    float sum = 0.0f;
    for (uint32_t i = bin_low; i <= bin_high; i++) {
        sum += power[i];
    }

    return sum;
}

/**
 * @brief Find dominant frequency (frequency with maximum power)
 */
static float find_dominant_frequency(
    const float* power,
    uint32_t num_bins,
    float sampling_rate,
    uint32_t fft_size,
    float* peak_power
) {
    uint32_t max_bin = 1;  /* Skip DC */
    float max_power = power[1];

    for (uint32_t i = 2; i < num_bins; i++) {
        if (power[i] > max_power) {
            max_power = power[i];
            max_bin = i;
        }
    }

    if (peak_power) {
        *peak_power = max_power;
    }

    float freq_resolution = sampling_rate / (float)fft_size;
    return (float)max_bin * freq_resolution;
}

/**
 * @brief Find median frequency (frequency below which 50% of power lies)
 */
static float find_median_frequency(
    const float* power,
    uint32_t num_bins,
    float sampling_rate,
    uint32_t fft_size,
    float total_power
) {
    float cumulative = 0.0f;
    float target = total_power * 0.5f;

    for (uint32_t i = 1; i < num_bins; i++) {
        cumulative += power[i];
        if (cumulative >= target) {
            float freq_resolution = sampling_rate / (float)fft_size;
            return (float)i * freq_resolution;
        }
    }

    return sampling_rate / 2.0f;  /* Nyquist */
}

/**
 * @brief Find spectral edge frequency (frequency below which X% of power lies)
 */
static float find_spectral_edge(
    const float* power,
    uint32_t num_bins,
    float sampling_rate,
    uint32_t fft_size,
    float total_power,
    float percentage  /* 0.95 for 95% */
) {
    float cumulative = 0.0f;
    float target = total_power * percentage;

    for (uint32_t i = 1; i < num_bins; i++) {
        cumulative += power[i];
        if (cumulative >= target) {
            float freq_resolution = sampling_rate / (float)fft_size;
            return (float)i * freq_resolution;
        }
    }

    return sampling_rate / 2.0f;
}

/**
 * @brief Compute spectral entropy (measure of spectral flatness)
 *
 * Entropy = -sum(p * log(p)) where p = power/total_power
 * High entropy = flat spectrum (noise-like)
 * Low entropy = peaked spectrum (rhythmic)
 */
static float compute_spectral_entropy(
    const float* power,
    uint32_t num_bins,
    float total_power
) {
    if (total_power < EPSILON) {
        return 0.0f;
    }

    float entropy = 0.0f;

    for (uint32_t i = 1; i < num_bins; i++) {
        float p = power[i] / total_power;
        if (p > EPSILON) {
            entropy -= p * logf(p);
        }
    }

    /* Normalize to [0, 1] by dividing by log(num_bins) */
    float max_entropy = logf((float)(num_bins - 1));
    if (max_entropy > EPSILON) {
        entropy /= max_entropy;
    }

    return entropy;
}

/**
 * @brief Update exponential moving average
 */
static void update_ema(float* ema, float new_value, float alpha) {
    *ema = alpha * new_value + (1.0f - alpha) * (*ema);
}

//=============================================================================
// Configuration API
//=============================================================================

int ephaptic_fft_default_config(ephaptic_fft_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_fft_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->fft_size = EPHAPTIC_FFT_DEFAULT_SIZE;
    config->sampling_rate = EPHAPTIC_FFT_DEFAULT_SAMPLE_RATE;
    config->window = EPHAPTIC_FFT_DEFAULT_WINDOW;
    config->enable_averaging = true;
    config->averaging_tau = 1.0f;  /* 1 second time constant */
    config->band_boundaries = NULL;  /* Use defaults */
    config->enable_coherence = false;

    return 0;
}

//=============================================================================
// Lifecycle API
//=============================================================================

ephaptic_fft_bridge_t* ephaptic_fft_bridge_create(
    const ephaptic_fft_config_t* config
) {
    ephaptic_fft_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    NIMCP_API_CHECK_ALLOC(bridge, "Failed to allocate ephaptic FFT bridge");
    if (bridge_base_init(&bridge->base, 0, "ephaptic_fft") != 0) { nimcp_free(bridge); return NULL; }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        ephaptic_fft_default_config(&bridge->config);
    }

    /* Validate FFT size */
    if (!fft_is_power_of_2(bridge->config.fft_size)) {
        bridge->config.fft_size = fft_next_power_of_2(bridge->config.fft_size);
    }
    if (bridge->config.fft_size < EPHAPTIC_FFT_MIN_SIZE) {
        bridge->config.fft_size = EPHAPTIC_FFT_MIN_SIZE;
    }
    if (bridge->config.fft_size > EPHAPTIC_FFT_MAX_SIZE) {
        bridge->config.fft_size = EPHAPTIC_FFT_MAX_SIZE;
    }

    /* Number of frequency bins */
    bridge->num_bins = bridge->config.fft_size / 2 + 1;

    /* Create FFT plan */
    bridge->fft_plan = fft_plan_create(bridge->config.fft_size, FFT_REAL);
    if (!bridge->fft_plan) {
        LOG_ERROR("Failed to create FFT plan for ephaptic bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create FFT plan");
        nimcp_free(bridge);
        return NULL;
    }

    /* Set window function */
    fft_plan_set_window(bridge->fft_plan, bridge->config.window);

    /* Allocate LFP buffer */
    bridge->lfp_buffer = nimcp_calloc(bridge->config.fft_size, sizeof(float));
    if (!bridge->lfp_buffer) {
        LOG_ERROR("Failed to allocate LFP buffer for FFT bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate LFP buffer");
        fft_plan_destroy(bridge->fft_plan);
        nimcp_free(bridge);
        return NULL;
    }

    /* Allocate windowed signal buffer */
    bridge->windowed_signal = nimcp_calloc(bridge->config.fft_size, sizeof(float));
    if (!bridge->windowed_signal) {
        LOG_ERROR("Failed to allocate windowed signal buffer for FFT bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate windowed signal buffer");
        nimcp_free(bridge->lfp_buffer);
        fft_plan_destroy(bridge->fft_plan);
        nimcp_free(bridge);
        return NULL;
    }

    /* Allocate spectrum */
    bridge->spectrum = nimcp_calloc(bridge->num_bins, sizeof(fft_complex_t));
    if (!bridge->spectrum) {
        LOG_ERROR("Failed to allocate FFT spectrum buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate FFT spectrum buffer");
        nimcp_free(bridge->windowed_signal);
        nimcp_free(bridge->lfp_buffer);
        fft_plan_destroy(bridge->fft_plan);
        nimcp_free(bridge);
        return NULL;
    }

    /* Allocate power spectrum */
    bridge->power_spectrum = nimcp_calloc(bridge->num_bins, sizeof(float));
    if (!bridge->power_spectrum) {
        LOG_ERROR("Failed to allocate power spectrum buffer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate power spectrum buffer");
        nimcp_free(bridge->spectrum);
        nimcp_free(bridge->windowed_signal);
        nimcp_free(bridge->lfp_buffer);
        fft_plan_destroy(bridge->fft_plan);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_fft_bridge_create: bridge->power_spectrum is NULL");
        return NULL;
    }

    bridge->buffer_write_idx = 0;
    bridge->buffer_count = 0;
    bridge->averaging_initialized = false;
    bridge->last_timestamp = 0.0f;
    bridge->initialized = true;

    return bridge;
}

void ephaptic_fft_bridge_destroy(ephaptic_fft_bridge_t* bridge) {
    if (!bridge) {
        return;
        NIMCP_LOGGING_DEBUG("Destroying %s bridge", "ephaptic_fft");
    }

    if (bridge->power_spectrum) {
        nimcp_free(bridge->power_spectrum);
    }
    if (bridge->spectrum) {
        nimcp_free(bridge->spectrum);
    }
    if (bridge->windowed_signal) {
        nimcp_free(bridge->windowed_signal);
    }
    if (bridge->lfp_buffer) {
        nimcp_free(bridge->lfp_buffer);
    }
    if (bridge->fft_plan) {
        fft_plan_destroy(bridge->fft_plan);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int ephaptic_fft_bridge_reset(ephaptic_fft_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_fft_bridge_reset: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    memset(bridge->lfp_buffer, 0, bridge->config.fft_size * sizeof(float));
    bridge->buffer_write_idx = 0;
    bridge->buffer_count = 0;
    bridge->averaging_initialized = false;
    memset(&bridge->last_result, 0, sizeof(bridge->last_result));
    memset(&bridge->averaged_result, 0, sizeof(bridge->averaged_result));

    return 0;
}

//=============================================================================
// Sample Collection API
//=============================================================================

int ephaptic_fft_add_sample(
    ephaptic_fft_bridge_t* bridge,
    float lfp_value,
    float timestamp
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_fft_add_sample: required parameter is NULL (bridge, bridge->initialized)");
        return -1;
    }

    /* Add to circular buffer */
    bridge->lfp_buffer[bridge->buffer_write_idx] = lfp_value;
    bridge->buffer_write_idx = (bridge->buffer_write_idx + 1) % bridge->config.fft_size;

    if (bridge->buffer_count < bridge->config.fft_size) {
        bridge->buffer_count++;
    }

    bridge->last_timestamp = timestamp;

    return 0;
}

int ephaptic_fft_add_lfp_result(
    ephaptic_fft_bridge_t* bridge,
    const nimcp_lfp_result_t* lfp,
    float timestamp
) {
    if (!bridge || !lfp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_fft_add_lfp_result: required parameter is NULL (bridge, lfp)");
        return -1;
    }

    return ephaptic_fft_add_sample(bridge, lfp->amplitude, timestamp);
}

bool ephaptic_fft_buffer_ready(const ephaptic_fft_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_fft_buffer_ready: required parameter is NULL (bridge, bridge->initialized)");
        return false;
    }
    return bridge->buffer_count >= bridge->config.fft_size;
}

float ephaptic_fft_buffer_level(const ephaptic_fft_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        return 0.0f;
    }
    return (float)bridge->buffer_count / (float)bridge->config.fft_size;
}

//=============================================================================
// Spectral Analysis API
//=============================================================================

int ephaptic_fft_compute_band_power(
    ephaptic_fft_bridge_t* bridge,
    ephaptic_fft_result_t* result
) {
    if (!bridge || !bridge->initialized || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_fft_compute_band_power: required parameter is NULL (bridge, bridge->initialized, result)");
        return -1;
    }

    if (!ephaptic_fft_buffer_ready(bridge)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ephaptic_fft_compute_band_power: ephaptic_fft_buffer_ready is NULL");
        return -1;  /* Not enough samples */
    }

    memset(result, 0, sizeof(*result));

    /* Copy buffer to windowed array in correct order */
    /* Since it's a circular buffer, we need to reorder */
    uint32_t fft_size = bridge->config.fft_size;
    uint32_t start_idx = bridge->buffer_write_idx;  /* Oldest sample */

    for (uint32_t i = 0; i < fft_size; i++) {
        uint32_t buf_idx = (start_idx + i) % fft_size;
        bridge->windowed_signal[i] = bridge->lfp_buffer[buf_idx];
    }

    /* Execute FFT (windowing applied by plan) */
    if (!fft_execute_real(bridge->fft_plan, bridge->windowed_signal, bridge->spectrum)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ephaptic_fft_compute_band_power: fft_execute_real is NULL");
        return -1;
    }

    /* Compute power spectrum */
    if (!fft_power_spectrum(bridge->spectrum, bridge->power_spectrum, bridge->num_bins)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ephaptic_fft_compute_band_power: fft_power_spectrum is NULL");
        return -1;
    }

    /* Compute total power */
    result->total_power = 0.0f;
    for (uint32_t i = 1; i < bridge->num_bins; i++) {  /* Skip DC */
        result->total_power += bridge->power_spectrum[i];
    }

    /* Get band boundaries */
    float delta_lo = EPHAPTIC_BAND_DELTA_LOW;
    float delta_hi = EPHAPTIC_BAND_DELTA_HIGH;
    float theta_lo = EPHAPTIC_BAND_THETA_LOW;
    float theta_hi = EPHAPTIC_BAND_THETA_HIGH;
    float alpha_lo = EPHAPTIC_BAND_ALPHA_LOW;
    float alpha_hi = EPHAPTIC_BAND_ALPHA_HIGH;
    float beta_lo = EPHAPTIC_BAND_BETA_LOW;
    float beta_hi = EPHAPTIC_BAND_BETA_HIGH;
    float gamma_lo = EPHAPTIC_BAND_GAMMA_LOW;
    float gamma_hi = EPHAPTIC_BAND_GAMMA_HIGH;

    /* Compute band powers */
    result->band_power[0] = compute_band_power_internal(
        bridge->power_spectrum, bridge->num_bins,
        bridge->config.sampling_rate, fft_size,
        delta_lo, delta_hi
    );
    result->band_power[1] = compute_band_power_internal(
        bridge->power_spectrum, bridge->num_bins,
        bridge->config.sampling_rate, fft_size,
        theta_lo, theta_hi
    );
    result->band_power[2] = compute_band_power_internal(
        bridge->power_spectrum, bridge->num_bins,
        bridge->config.sampling_rate, fft_size,
        alpha_lo, alpha_hi
    );
    result->band_power[3] = compute_band_power_internal(
        bridge->power_spectrum, bridge->num_bins,
        bridge->config.sampling_rate, fft_size,
        beta_lo, beta_hi
    );
    result->band_power[4] = compute_band_power_internal(
        bridge->power_spectrum, bridge->num_bins,
        bridge->config.sampling_rate, fft_size,
        gamma_lo, gamma_hi
    );

    /* Compute relative powers */
    if (result->total_power > EPSILON) {
        for (int i = 0; i < EPHAPTIC_FFT_NUM_BANDS; i++) {
            result->band_power_relative[i] = result->band_power[i] / result->total_power;
        }
    }

    /* Find dominant frequency */
    result->dominant_frequency = find_dominant_frequency(
        bridge->power_spectrum, bridge->num_bins,
        bridge->config.sampling_rate, fft_size,
        &result->peak_power
    );

    /* Compute median and spectral edge frequencies */
    result->median_frequency = find_median_frequency(
        bridge->power_spectrum, bridge->num_bins,
        bridge->config.sampling_rate, fft_size,
        result->total_power
    );

    result->spectral_edge_95 = find_spectral_edge(
        bridge->power_spectrum, bridge->num_bins,
        bridge->config.sampling_rate, fft_size,
        result->total_power, 0.95f
    );

    /* Compute spectral entropy */
    result->spectral_entropy = compute_spectral_entropy(
        bridge->power_spectrum, bridge->num_bins,
        result->total_power
    );

    /* Compute band ratios */
    if (result->band_power[2] > EPSILON) {  /* Alpha */
        result->theta_alpha_ratio = result->band_power[1] / result->band_power[2];
        result->delta_alpha_ratio = result->band_power[0] / result->band_power[2];
    }
    if (result->band_power[3] > EPSILON) {  /* Beta */
        result->theta_beta_ratio = result->band_power[1] / result->band_power[3];
    }

    result->timestamp_ms = bridge->last_timestamp;
    result->buffer_full = true;

    /* Store for averaging */
    bridge->last_result = *result;

    /* Update exponential moving average */
    if (bridge->config.enable_averaging) {
        /* Compute alpha for EMA based on time constant */
        float dt = 1.0f / bridge->config.sampling_rate * fft_size;  /* Time for one FFT */
        float alpha = 1.0f - expf(-dt / bridge->config.averaging_tau);

        if (!bridge->averaging_initialized) {
            bridge->averaged_result = *result;
            bridge->averaging_initialized = true;
        } else {
            for (int i = 0; i < EPHAPTIC_FFT_NUM_BANDS; i++) {
                update_ema(&bridge->averaged_result.band_power[i],
                           result->band_power[i], alpha);
                update_ema(&bridge->averaged_result.band_power_relative[i],
                           result->band_power_relative[i], alpha);
            }
            update_ema(&bridge->averaged_result.total_power, result->total_power, alpha);
            update_ema(&bridge->averaged_result.dominant_frequency,
                       result->dominant_frequency, alpha);
            update_ema(&bridge->averaged_result.spectral_entropy,
                       result->spectral_entropy, alpha);
        }
    }

    return 0;
}

int ephaptic_fft_get_averaged_power(
    const ephaptic_fft_bridge_t* bridge,
    ephaptic_fft_result_t* result
) {
    if (!bridge || !bridge->initialized || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_fft_get_averaged_power: required parameter is NULL (bridge, bridge->initialized, result)");
        return -1;
    }

    if (!bridge->config.enable_averaging || !bridge->averaging_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_fft_get_averaged_power: required parameter is NULL (bridge->config, bridge->averaging_initialized)");
        return -1;
    }

    *result = bridge->averaged_result;
    return 0;
}

int ephaptic_fft_get_power_spectrum(
    const ephaptic_fft_bridge_t* bridge,
    float* power,
    uint32_t size
) {
    if (!bridge || !bridge->initialized || !power) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_fft_get_power_spectrum: required parameter is NULL (bridge, bridge->initialized, power)");
        return -1;
    }

    uint32_t copy_size = (size < bridge->num_bins) ? size : bridge->num_bins;
    memcpy(power, bridge->power_spectrum, copy_size * sizeof(float));

    return (int)copy_size;
}

int ephaptic_fft_get_frequencies(
    const ephaptic_fft_bridge_t* bridge,
    float* freq,
    uint32_t size
) {
    if (!bridge || !bridge->initialized || !freq) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_fft_get_frequencies: required parameter is NULL (bridge, bridge->initialized, freq)");
        return -1;
    }

    uint32_t copy_size = (size < bridge->num_bins) ? size : bridge->num_bins;
    float freq_resolution = bridge->config.sampling_rate / (float)bridge->config.fft_size;

    for (uint32_t i = 0; i < copy_size; i++) {
        freq[i] = (float)i * freq_resolution;
    }

    return (int)copy_size;
}

//=============================================================================
// Integration with Ephaptic System
//=============================================================================

int ephaptic_fft_compute_lfp(
    ephaptic_fft_bridge_t* bridge,
    nimcp_ephaptic_system_t* system,
    const float position[3],
    nimcp_lfp_result_t* result
) {
    if (!bridge || !bridge->initialized || !system || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_fft_compute_lfp: required parameter is NULL (bridge, bridge->initialized, system, result)");
        return -1;
    }

    /* First compute LFP using the standard ephaptic function */
    nimcp_error_t err = nimcp_ephaptic_compute_lfp(system, position, result);
    if (err != NIMCP_OK) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ephaptic_fft_compute_lfp: validation failed");
        return -1;
    }

    /* Add sample to FFT buffer */
    ephaptic_fft_add_sample(bridge, result->amplitude, system->time);

    /* If buffer is full, compute FFT-based band power */
    if (ephaptic_fft_buffer_ready(bridge)) {
        ephaptic_fft_result_t fft_result;
        if (ephaptic_fft_compute_band_power(bridge, &fft_result) == 0) {
            /* Replace hardcoded band power with FFT-derived values */
            result->band_power[0] = fft_result.band_power[0];
            result->band_power[1] = fft_result.band_power[1];
            result->band_power[2] = fft_result.band_power[2];
            result->band_power[3] = fft_result.band_power[3];
            result->band_power[4] = fft_result.band_power[4];
            result->dominant_frequency = fft_result.dominant_frequency;
        }
    }

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

int ephaptic_fft_get_config(
    const ephaptic_fft_bridge_t* bridge,
    ephaptic_fft_config_t* config
) {
    if (!bridge || !bridge->initialized || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ephaptic_fft_get_config: required parameter is NULL (bridge, bridge->initialized, config)");
        return -1;
    }

    *config = bridge->config;
    return 0;
}

uint32_t ephaptic_fft_get_num_bins(const ephaptic_fft_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        return 0;
    }
    return bridge->num_bins;
}

float ephaptic_fft_get_frequency_resolution(const ephaptic_fft_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        return 0.0f;
    }
    return bridge->config.sampling_rate / (float)bridge->config.fft_size;
}

float ephaptic_fft_get_nyquist(const ephaptic_fft_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        return 0.0f;
    }
    return bridge->config.sampling_rate / 2.0f;
}
