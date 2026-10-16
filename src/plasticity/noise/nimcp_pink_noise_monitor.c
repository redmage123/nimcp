#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_pink_noise_monitor.c - Real-Time Spectral Monitoring
//=============================================================================

#include "plasticity/noise/nimcp_pink_noise_monitor.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pink_noise_monitor)

//=============================================================================
// Configuration
//=============================================================================

pink_monitor_config_t pink_monitor_default_config(void) {
    pink_monitor_config_t config = {0};
    config.target_alpha = 1.0f;
    config.tolerance = 0.15f;
    config.correction_gain = 0.05f;
    config.window_size = PINK_MONITOR_WINDOW_SIZE;
    config.update_rate = 256.0f;
    config.enable_auto_correction = true;
    config.enable_alerts = false;
    config.alert_callback = NULL;
    config.alert_user_data = NULL;
    return config;
}

//=============================================================================
// Internal: Spectral Analysis
//=============================================================================

static void compute_power_spectrum(
    const float* samples,
    float* power,
    uint32_t n
) {
    /**
     * WHAT: Compute power spectrum using DFT (simplified)
     * WHY:  Estimate 1/f^α exponent
     * HOW:  Direct DFT for small windows
     */
    float pi = NIMCP_PI_F;

    for (uint32_t k = 1; k < n / 2; k++) {
        float real = 0.0f, imag = 0.0f;

        for (uint32_t t = 0; t < n; t++) {
            float angle = 2.0f * pi * (float)k * (float)t / (float)n;
            real += samples[t] * cosf(angle);
            imag -= samples[t] * sinf(angle);
        }

        power[k] = (real * real + imag * imag) / (float)(n * n);
    }
}

static float estimate_alpha(
    const float* power,
    uint32_t n,
    float* r_squared
) {
    /**
     * WHAT: Estimate spectral exponent via log-log regression
     * WHY:  Determine if noise matches 1/f^α
     */
    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_xx = 0.0f;
    uint32_t count = 0;

    for (uint32_t k = 2; k < n / 2; k++) {
        if (power[k] > 1e-12f) {
            float log_f = logf((float)k);
            float log_p = logf(power[k]);
            sum_x += log_f;
            sum_y += log_p;
            sum_xy += log_f * log_p;
            sum_xx += log_f * log_f;
            count++;
        }
    }

    if (count < 10) {
        *r_squared = 0.0f;
        return 1.0f;
    }

    float n_f = (float)count;
    float denom = n_f * sum_xx - sum_x * sum_x;
    if (fabsf(denom) < 1e-10f) {
        *r_squared = 0.0f;
        return 1.0f;
    }

    float slope = (n_f * sum_xy - sum_x * sum_y) / denom;
    float alpha = -slope;  // α = -slope in log-log

    // Compute R²
    float mean_y = sum_y / n_f;
    float intercept = (sum_y - slope * sum_x) / n_f;
    float ss_res = 0.0f, ss_tot = 0.0f;

    for (uint32_t k = 2; k < n / 2; k++) {
        if (power[k] > 1e-12f) {
            float log_f = logf((float)k);
            float log_p = logf(power[k]);
            float pred = slope * log_f + intercept;
            ss_res += (log_p - pred) * (log_p - pred);
            ss_tot += (log_p - mean_y) * (log_p - mean_y);
        }
    }

    *r_squared = (ss_tot > 0.0f) ? (1.0f - ss_res / ss_tot) : 0.0f;
    return fmaxf(0.0f, fminf(3.0f, alpha));
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

pink_noise_monitor_t* pink_monitor_create(const pink_monitor_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }

    pink_noise_monitor_t* monitor = nimcp_calloc(1, sizeof(pink_noise_monitor_t));
    if (!monitor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "monitor is NULL");

        return NULL;

    }

    memcpy(&monitor->config, config, sizeof(pink_monitor_config_t));

    monitor->sample_buffer = nimcp_calloc(config->window_size, sizeof(float));
    monitor->power_spectrum = nimcp_calloc(config->window_size, sizeof(float));
    monitor->frequency_bins = nimcp_calloc(config->window_size / 2, sizeof(float));

    if (!monitor->sample_buffer || !monitor->power_spectrum || !monitor->frequency_bins) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "pink_monitor_create: failed to allocate buffers");
        pink_monitor_destroy(monitor);
        return NULL;
    }

    monitor->current_alpha = config->target_alpha;
    monitor->amplitude_correction = 1.0f;

    NIMCP_LOGGING_INFO("Created pink noise monitor (target α=%.2f)", config->target_alpha);
    return monitor;
}

void pink_monitor_destroy(pink_noise_monitor_t* monitor) {
    if (!monitor) return;
    if (monitor->sample_buffer) nimcp_free(monitor->sample_buffer);
    if (monitor->power_spectrum) nimcp_free(monitor->power_spectrum);
    if (monitor->frequency_bins) nimcp_free(monitor->frequency_bins);
    nimcp_free(monitor);
}

//=============================================================================
// Connection
//=============================================================================

int pink_monitor_connect(
    pink_noise_monitor_t* monitor,
    pink_noise_generator_t generator
) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_monitor_connect: monitor is NULL");
        return -1;
    }
    monitor->generator = generator;
    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int pink_monitor_update(pink_noise_monitor_t* monitor, float sample) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_monitor_update: monitor is NULL");
        return -1;
    }

    // Store sample in buffer
    monitor->sample_buffer[monitor->buffer_index] = sample;
    monitor->buffer_index = (monitor->buffer_index + 1) % monitor->config.window_size;
    monitor->total_samples++;

    if (!monitor->buffer_full && monitor->buffer_index == 0) {
        monitor->buffer_full = true;
    }

    // Check if we should update spectrum
    if (monitor->total_samples - monitor->last_update < (uint64_t)monitor->config.update_rate) {
        return 0;
    }

    if (!monitor->buffer_full) {
        return 0;
    }

    monitor->last_update = monitor->total_samples;

    // Compute power spectrum
    compute_power_spectrum(monitor->sample_buffer, monitor->power_spectrum,
                          monitor->config.window_size);

    // Estimate alpha
    float r2;
    float new_alpha = estimate_alpha(monitor->power_spectrum,
                                     monitor->config.window_size, &r2);
    monitor->current_alpha = new_alpha;
    monitor->spectral_fit_r2 = r2;

    // Update history
    monitor->alpha_history[monitor->history_index] = new_alpha;
    monitor->history_index = (monitor->history_index + 1) % PINK_MONITOR_HISTORY_SIZE;

    // Compute variance
    float sum = 0.0f, sum_sq = 0.0f;
    for (uint32_t i = 0; i < PINK_MONITOR_HISTORY_SIZE; i++) {
        sum += monitor->alpha_history[i];
        sum_sq += monitor->alpha_history[i] * monitor->alpha_history[i];
    }
    float mean = sum / PINK_MONITOR_HISTORY_SIZE;
    monitor->alpha_variance = sum_sq / PINK_MONITOR_HISTORY_SIZE - mean * mean;

    // Check deviation
    float deviation = fabsf(new_alpha - monitor->config.target_alpha);
    if (deviation > monitor->config.tolerance) {
        monitor->drift_count++;

        // Apply correction if enabled
        if (monitor->config.enable_auto_correction) {
            monitor->alpha_correction = monitor->config.correction_gain *
                                       (monitor->config.target_alpha - new_alpha);
            monitor->correction_active = true;
            monitor->correction_count++;
        }

        // Fire alert if enabled
        if (monitor->config.enable_alerts && monitor->config.alert_callback) {
            monitor->config.alert_callback(
                monitor->config.alert_user_data,
                new_alpha,
                monitor->config.target_alpha,
                "Alpha deviation detected"
            );
        }
    } else {
        // Within tolerance - reduce correction
        monitor->alpha_correction *= 0.9f;
        if (fabsf(monitor->alpha_correction) < 0.01f) {
            monitor->correction_active = false;
            monitor->alpha_correction = 0.0f;
        }
    }

    return 1;  // Spectrum was updated
}

//=============================================================================
// Query Functions
//=============================================================================

float pink_monitor_get_alpha(const pink_noise_monitor_t* monitor) {
    return monitor ? monitor->current_alpha : 1.0f;
}

float pink_monitor_get_amplitude_correction(const pink_noise_monitor_t* monitor) {
    return monitor ? monitor->amplitude_correction : 1.0f;
}

float pink_monitor_get_alpha_correction(const pink_noise_monitor_t* monitor) {
    return monitor ? monitor->alpha_correction : 0.0f;
}

int pink_monitor_get_quality(
    const pink_noise_monitor_t* monitor,
    pink_monitor_quality_t* quality
) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_monitor_get_quality: monitor is NULL");
        return -1;
    }
    if (!quality) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_monitor_get_quality: quality is NULL");
        return -1;
    }

    memset(quality, 0, sizeof(pink_monitor_quality_t));
    quality->current_alpha = monitor->current_alpha;
    quality->target_alpha = monitor->config.target_alpha;
    quality->alpha_deviation = fabsf(quality->current_alpha - quality->target_alpha);
    quality->alpha_variance = monitor->alpha_variance;
    quality->spectral_fit_r2 = monitor->spectral_fit_r2;
    quality->in_tolerance = (quality->alpha_deviation <= monitor->config.tolerance);
    quality->drift_events = monitor->drift_count;
    quality->corrections_applied = monitor->correction_count;

    return 0;
}

int pink_monitor_recalculate(pink_noise_monitor_t* monitor) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_monitor_recalculate: monitor is NULL");
        return -1;
    }
    if (!monitor->buffer_full) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "pink_monitor_recalculate: buffer not yet full");
        return -1;
    }

    compute_power_spectrum(monitor->sample_buffer, monitor->power_spectrum,
                          monitor->config.window_size);

    float r2;
    monitor->current_alpha = estimate_alpha(monitor->power_spectrum,
                                            monitor->config.window_size, &r2);
    monitor->spectral_fit_r2 = r2;

    return 0;
}

int pink_monitor_reset(pink_noise_monitor_t* monitor) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_monitor_reset: monitor is NULL");
        return -1;
    }

    memset(monitor->sample_buffer, 0, monitor->config.window_size * sizeof(float));
    memset(monitor->power_spectrum, 0, monitor->config.window_size * sizeof(float));
    memset(monitor->alpha_history, 0, sizeof(monitor->alpha_history));

    monitor->buffer_index = 0;
    monitor->buffer_full = false;
    monitor->history_index = 0;
    monitor->current_alpha = monitor->config.target_alpha;
    monitor->amplitude_correction = 1.0f;
    monitor->alpha_correction = 0.0f;
    monitor->correction_active = false;
    monitor->drift_count = 0;
    monitor->correction_count = 0;
    monitor->total_samples = 0;
    monitor->last_update = 0;

    return 0;
}

int pink_monitor_set_callback(
    pink_noise_monitor_t* monitor,
    pink_monitor_alert_fn callback,
    void* user_data
) {
    if (!monitor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "pink_monitor_set_callback: monitor is NULL");
        return -1;
    }
    monitor->config.alert_callback = callback;
    monitor->config.alert_user_data = user_data;
    monitor->config.enable_alerts = (callback != NULL);
    return 0;
}
