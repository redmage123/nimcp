#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_pink_noise_criticality.c - Self-Organized Criticality Integration
//=============================================================================
/**
 * WHAT: Pink noise criticality analysis and avalanche dynamics
 * WHY:  Brain operates near criticality for optimal computation
 * HOW:  Monitor 1/f spectrum, detect avalanches, provide feedback
 */

#include "plasticity/noise/nimcp_pink_noise_criticality.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/thread/nimcp_thread_rand.h"
#include "constants/nimcp_frequency_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pink_noise_criticality)

//=============================================================================
// Default Configuration
//=============================================================================

criticality_config_t criticality_default_config(void) {
    /**
     * WHAT: Create default configuration for criticality analysis
     * WHY:  Sensible defaults based on neuroscience literature
     */
    criticality_config_t config = {0};

    config.threshold_high = 2.0f;           // 2 standard deviations
    config.threshold_low = 0.5f;            // 0.5 standard deviations
    config.target_alpha = 1.0f;             // True pink noise
    config.target_tau = 1.5f;               // Critical avalanche exponent
    config.criticality_tolerance = 0.1f;    // κ threshold
    config.min_avalanche_duration = 3;      // At least 3 samples
    config.sample_rate = NIMCP_OSC_SAMPLE_RATE_HZ;
    config.enable_feedback = true;
    config.feedback_gain = 0.1f;

    return config;
}

//=============================================================================
// Internal Functions
//=============================================================================

static void update_running_stats(criticality_analyzer_t* ca, float sample) {
    /**
     * WHAT: Update running mean and std using Welford's algorithm
     * WHY:  Efficient online statistics
     */
    ca->total_samples++;
    float delta = sample - ca->running_mean;
    ca->running_mean += delta / (float)ca->total_samples;

    if (ca->total_samples > 1) {
        float delta2 = sample - ca->running_mean;
        float variance = ca->running_std * ca->running_std;
        variance = ((ca->total_samples - 1) * variance + delta * delta2) / ca->total_samples;
        ca->running_std = sqrtf(variance);
    }
}

static void fit_power_law(
    const float* sizes,
    uint32_t count,
    float* exponent,
    float* r_squared
) {
    /**
     * WHAT: Fit power law P(S) ∝ S^(-τ) using log-log regression
     * WHY:  Estimate avalanche size exponent
     */
    if (count < 5) {
        *exponent = 1.5f;
        *r_squared = 0.0f;
        return;
    }

    // Log-log regression
    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_xx = 0.0f;
    uint32_t valid = 0;

    for (uint32_t i = 0; i < count; i++) {
        if (sizes[i] > 0.01f) {
            float log_s = logf(sizes[i]);
            float log_rank = logf((float)(i + 1));
            sum_x += log_s;
            sum_y += log_rank;
            sum_xy += log_s * log_rank;
            sum_xx += log_s * log_s;
            valid++;
        }
    }

    if (valid < 5) {
        *exponent = 1.5f;
        *r_squared = 0.0f;
        return;
    }

    float n = (float)valid;
    float denom = n * sum_xx - sum_x * sum_x;
    if (fabsf(denom) < 1e-10f) {
        *exponent = 1.5f;
        *r_squared = 0.0f;
        return;
    }

    float slope = (n * sum_xy - sum_x * sum_y) / denom;
    *exponent = -slope;  // τ is negative of slope

    // Compute R²
    float mean_y = sum_y / n;
    float ss_tot = 0.0f, ss_res = 0.0f;
    float intercept = (sum_y - slope * sum_x) / n;

    uint32_t idx = 0;
    for (uint32_t i = 0; i < count && idx < valid; i++) {
        if (sizes[i] > 0.01f) {
            float log_s = logf(sizes[i]);
            float log_rank = logf((float)(i + 1));
            float predicted = slope * log_s + intercept;
            ss_res += (log_rank - predicted) * (log_rank - predicted);
            ss_tot += (log_rank - mean_y) * (log_rank - mean_y);
            idx++;
        }
    }

    *r_squared = (ss_tot > 0.0f) ? (1.0f - ss_res / ss_tot) : 0.0f;
}

static void compute_criticality_index(criticality_analyzer_t* ca) {
    /**
     * WHAT: Compute distance from critical point
     * WHY:  Single metric for criticality assessment
     */
    float alpha_dev = ca->measured_alpha - ca->config.target_alpha;
    float tau_dev = ca->measured_tau - ca->config.target_tau;

    ca->criticality_index = alpha_dev * alpha_dev + tau_dev * tau_dev;

    // Classify regime
    if (ca->total_samples < 100) {
        ca->regime = CRITICALITY_UNKNOWN;
    } else if (ca->criticality_index < ca->config.criticality_tolerance) {
        ca->regime = CRITICALITY_CRITICAL;
    } else if (ca->measured_alpha > ca->config.target_alpha) {
        ca->regime = CRITICALITY_SUBCRITICAL;
    } else {
        ca->regime = CRITICALITY_SUPERCRITICAL;
    }
}

static void update_feedback(criticality_analyzer_t* ca) {
    /**
     * WHAT: Compute homeostatic feedback corrections
     * WHY:  Drive system back toward criticality
     */
    if (!ca->config.enable_feedback) {
        ca->amplitude_correction = 1.0f;
        ca->alpha_correction = 0.0f;
        return;
    }

    float gain = ca->config.feedback_gain;

    // Amplitude correction
    if (ca->regime == CRITICALITY_SUBCRITICAL) {
        ca->amplitude_correction = 1.0f + gain;  // Increase noise
    } else if (ca->regime == CRITICALITY_SUPERCRITICAL) {
        ca->amplitude_correction = 1.0f - gain;  // Decrease noise
    } else {
        ca->amplitude_correction = 1.0f;
    }

    // Alpha correction
    ca->alpha_correction = gain * (ca->config.target_alpha - ca->measured_alpha);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

criticality_analyzer_t* criticality_create(const criticality_config_t* config) {
    /**
     * WHAT: Create criticality analyzer
     * WHY:  Initialize monitoring and avalanche detection
     */
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }

    criticality_analyzer_t* ca = nimcp_calloc(1, sizeof(criticality_analyzer_t));
    if (!ca) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ca is NULL");

        return NULL;

    }

    memcpy(&ca->config, config, sizeof(criticality_config_t));

    ca->running_std = 1.0f;
    ca->measured_alpha = 1.0f;
    ca->measured_tau = 1.5f;
    ca->amplitude_correction = 1.0f;
    ca->regime = CRITICALITY_UNKNOWN;

    NIMCP_LOGGING_INFO("Created criticality analyzer");
    return ca;
}

void criticality_destroy(criticality_analyzer_t* ca) {
    /**
     * WHAT: Free analyzer resources
     */
    if (!ca) return;
    nimcp_free(ca);
}

//=============================================================================
// Analysis Functions
//=============================================================================

int criticality_update(criticality_analyzer_t* ca, float sample) {
    /**
     * WHAT: Process new sample, detect avalanches, update metrics
     * WHY:  Continuous criticality monitoring
     */
    if (!ca) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "criticality_update: ca is NULL");
        return -1;
    }

    // Update history
    ca->history[ca->history_index] = sample;
    ca->history_index = (ca->history_index + 1) % CRITICALITY_HISTORY_SIZE;
    if (ca->history_count < CRITICALITY_HISTORY_SIZE) {
        ca->history_count++;
    }

    // Update running statistics
    update_running_stats(ca, sample);

    // Avalanche detection
    float normalized = (sample - ca->running_mean) / fmaxf(ca->running_std, 0.001f);

    if (!ca->in_avalanche) {
        // Check for avalanche start
        if (fabsf(normalized) > ca->config.threshold_high) {
            ca->in_avalanche = true;
            ca->current_avalanche.start_time = ca->total_samples;
            ca->current_avalanche.duration = 1;
            ca->current_avalanche.size = fabsf(sample);
            ca->current_avalanche.peak_amplitude = fabsf(sample);
        }
    } else {
        // In avalanche - check for end
        if (fabsf(normalized) < ca->config.threshold_low) {
            // Avalanche ended
            ca->in_avalanche = false;

            if (ca->current_avalanche.duration >= ca->config.min_avalanche_duration) {
                // Store valid avalanche
                if (ca->num_avalanches < CRITICALITY_MAX_AVALANCHES) {
                    ca->avalanches[ca->num_avalanches++] = ca->current_avalanche;
                } else {
                    // Shift array
                    memmove(&ca->avalanches[0], &ca->avalanches[1],
                            (CRITICALITY_MAX_AVALANCHES - 1) * sizeof(avalanche_event_t));
                    ca->avalanches[CRITICALITY_MAX_AVALANCHES - 1] = ca->current_avalanche;
                }
            }
        } else {
            // Continue avalanche
            ca->current_avalanche.duration++;
            ca->current_avalanche.size += fabsf(sample);
            if (fabsf(sample) > ca->current_avalanche.peak_amplitude) {
                ca->current_avalanche.peak_amplitude = fabsf(sample);
            }
        }
    }

    // Periodically update power-law fit
    if (ca->total_samples % 100 == 0 && ca->num_avalanches >= 10) {
        float sizes[CRITICALITY_MAX_AVALANCHES];
        for (uint32_t i = 0; i < ca->num_avalanches; i++) {
            sizes[i] = ca->avalanches[i].size;
        }

        float r2;
        fit_power_law(sizes, ca->num_avalanches, &ca->measured_tau, &r2);
    }

    // Periodically update spectral exponent (using variance ratio method)
    if (ca->total_samples % 256 == 0 && ca->history_count >= 256) {
        // Simplified: estimate alpha from autocorrelation decay
        float acf_1 = 0.0f, acf_4 = 0.0f, variance = 0.0f;
        float mean = 0.0f;

        for (uint32_t i = 0; i < 256; i++) {
            mean += ca->history[i];
        }
        mean /= 256.0f;

        for (uint32_t i = 0; i < 256; i++) {
            float centered = ca->history[i] - mean;
            variance += centered * centered;
            if (i < 255) {
                acf_1 += centered * (ca->history[i + 1] - mean);
            }
            if (i < 252) {
                acf_4 += centered * (ca->history[i + 4] - mean);
            }
        }

        variance /= 256.0f;
        acf_1 /= 255.0f;
        acf_4 /= 252.0f;

        if (variance > 0.001f && acf_1 > 0.001f && acf_4 > 0.001f) {
            // Alpha ~ 2 * (1 - log(acf_1/acf_4) / log(4))
            float ratio = acf_1 / acf_4;
            if (ratio > 0.0f) {
                ca->measured_alpha = 2.0f * (1.0f - logf(ratio) / logf(4.0f));
                ca->measured_alpha = fmaxf(0.5f, fminf(2.0f, ca->measured_alpha));
            }
        }
    }

    // Update criticality index and feedback
    compute_criticality_index(ca);
    update_feedback(ca);

    return 0;
}

int criticality_connect_generator(
    criticality_analyzer_t* ca,
    pink_noise_generator_t generator
) {
    /**
     * WHAT: Connect to pink noise generator
     */
    if (!ca) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "criticality_connect_generator: ca is NULL");
        return -1;
    }
    ca->noise_generator = generator;
    return 0;
}

criticality_regime_t criticality_get_regime(const criticality_analyzer_t* ca) {
    if (!ca) return CRITICALITY_UNKNOWN;
    return ca->regime;
}

float criticality_get_index(const criticality_analyzer_t* ca) {
    if (!ca) return 1.0f;
    return ca->criticality_index;
}

bool criticality_in_avalanche(const criticality_analyzer_t* ca) {
    if (!ca) {
        return false;  /* Boolean query returning false for NULL is normal */
    }
    return ca->in_avalanche;
}

float criticality_get_amplitude_correction(const criticality_analyzer_t* ca) {
    if (!ca) return 1.0f;
    return ca->amplitude_correction;
}

float criticality_get_alpha_correction(const criticality_analyzer_t* ca) {
    if (!ca) return 0.0f;
    return ca->alpha_correction;
}

//=============================================================================
// Avalanche Functions
//=============================================================================

int criticality_generate_avalanche(
    criticality_analyzer_t* ca,
    float* output,
    uint32_t max_samples,
    uint32_t* num_generated
) {
    /**
     * WHAT: Generate synthetic avalanche with power-law statistics
     * WHY:  Simulate neural avalanche dynamics
     */
    if (!ca) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "criticality_generate_avalanche: ca is NULL");
        return -1;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "criticality_generate_avalanche: output is NULL");
        return -1;
    }
    if (!num_generated) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "criticality_generate_avalanche: num_generated is NULL");
        return -1;
    }
    if (max_samples == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "criticality_generate_avalanche: max_samples is 0");
        return -1;
    }

    // Generate power-law distributed size
    float u = (float)nimcp_tl_rand() / (float)RAND_MAX;
    float size = powf(1.0f - u, -1.0f / ca->measured_tau);
    size = fminf(size, 100.0f);  // Cap size

    // Duration ~ size^(1/2) for critical avalanches
    uint32_t duration = (uint32_t)(sqrtf(size) * 2.0f + 1.0f);
    duration = (duration < max_samples) ? duration : max_samples;
    duration = (duration >= 1) ? duration : 1;

    // Generate avalanche shape (parabolic profile)
    float peak = size / (float)duration * 2.0f;
    for (uint32_t i = 0; i < duration; i++) {
        float t = (float)i / (float)(duration - 1);
        output[i] = peak * 4.0f * t * (1.0f - t);  // Parabola
    }

    *num_generated = duration;
    return 0;
}

int criticality_get_avalanches(
    const criticality_analyzer_t* ca,
    avalanche_event_t* avalanches,
    uint32_t max_count,
    uint32_t* count
) {
    if (!ca) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "criticality_get_avalanches: ca is NULL");
        return -1;
    }
    if (!avalanches) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "criticality_get_avalanches: avalanches is NULL");
        return -1;
    }
    if (!count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "criticality_get_avalanches: count is NULL");
        return -1;
    }

    *count = (ca->num_avalanches < max_count) ? ca->num_avalanches : max_count;
    memcpy(avalanches, ca->avalanches, *count * sizeof(avalanche_event_t));

    return 0;
}

//=============================================================================
// Statistics and Reset
//=============================================================================

int criticality_get_stats(
    const criticality_analyzer_t* ca,
    criticality_stats_t* stats
) {
    if (!ca) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "criticality_get_stats: ca is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "criticality_get_stats: stats is NULL");
        return -1;
    }

    memset(stats, 0, sizeof(criticality_stats_t));

    stats->spectral_exponent = ca->measured_alpha;
    stats->avalanche_size_exponent = ca->measured_tau;
    stats->criticality_index = ca->criticality_index;
    stats->regime = ca->regime;
    stats->num_avalanches = ca->num_avalanches;
    stats->total_samples = ca->total_samples;

    // Compute average avalanche properties
    if (ca->num_avalanches > 0) {
        float sum_size = 0.0f, sum_dur = 0.0f;
        for (uint32_t i = 0; i < ca->num_avalanches; i++) {
            sum_size += ca->avalanches[i].size;
            sum_dur += (float)ca->avalanches[i].duration;
        }
        stats->avg_avalanche_size = sum_size / (float)ca->num_avalanches;
        stats->avg_avalanche_duration = sum_dur / (float)ca->num_avalanches;
    }

    return 0;
}

int criticality_reset(criticality_analyzer_t* ca) {
    if (!ca) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "criticality_reset: ca is NULL");
        return -1;
    }

    memset(ca->history, 0, sizeof(ca->history));
    ca->history_index = 0;
    ca->history_count = 0;
    ca->running_mean = 0.0f;
    ca->running_std = 1.0f;
    ca->measured_alpha = 1.0f;
    ca->measured_tau = 1.5f;
    ca->criticality_index = 0.0f;
    ca->regime = CRITICALITY_UNKNOWN;
    ca->num_avalanches = 0;
    ca->in_avalanche = false;
    ca->amplitude_correction = 1.0f;
    ca->alpha_correction = 0.0f;
    ca->total_samples = 0;

    return 0;
}

const char* criticality_regime_name(criticality_regime_t regime) {
    switch (regime) {
        case CRITICALITY_SUBCRITICAL: return "subcritical";
        case CRITICALITY_CRITICAL: return "critical";
        case CRITICALITY_SUPERCRITICAL: return "supercritical";
        default: return "unknown";
    }
}
