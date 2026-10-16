#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_pink_noise_multiscale.c - Multi-Scale Hierarchical Pink Noise
//=============================================================================
/**
 * WHAT: Implementation of multi-scale hierarchical pink noise
 * WHY:  Cortical hierarchy requires multiple temporal scales
 * HOW:  Independent generators with inter-scale coupling
 */

#include "plasticity/noise/nimcp_pink_noise_multiscale.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_frequency_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(pink_noise_multiscale)

//=============================================================================
// Default Configuration
//=============================================================================

pink_noise_multiscale_config_t pink_noise_multiscale_default_config(void) {
    /**
     * WHAT: Create biologically-motivated default configuration
     * WHY:  Match cortical temporal hierarchy
     * HOW:  4 scales from 50ms to 5000ms
     */
    pink_noise_multiscale_config_t config = {0};

    config.num_scales = PINK_NOISE_DEFAULT_SCALES;
    config.global_amplitude = 1.0f;
    config.sample_rate = NIMCP_OSC_SAMPLE_RATE_HZ;
    config.seed = 0;
    config.enable_coupling = true;

    // Scale 0: Fast (sensory, Layer 2/3)
    config.scales[0].timescale_ms = 50.0f;
    config.scales[0].alpha = 1.0f;
    config.scales[0].amplitude = 0.05f;
    config.scales[0].coupling_up = 0.0f;    // No faster scale
    config.scales[0].coupling_down = 0.2f;  // Influenced by medium scale

    // Scale 1: Medium (integration, Layer 4)
    config.scales[1].timescale_ms = 200.0f;
    config.scales[1].alpha = 1.0f;
    config.scales[1].amplitude = 0.08f;
    config.scales[1].coupling_up = 0.15f;   // Influenced by fast scale
    config.scales[1].coupling_down = 0.25f; // Influenced by slow scale

    // Scale 2: Slow (working memory, Layer 5)
    config.scales[2].timescale_ms = 1000.0f;
    config.scales[2].alpha = 1.1f;          // Slightly redder
    config.scales[2].amplitude = 0.1f;
    config.scales[2].coupling_up = 0.1f;
    config.scales[2].coupling_down = 0.3f;

    // Scale 3: Ultra-slow (context, Layer 6)
    config.scales[3].timescale_ms = 5000.0f;
    config.scales[3].alpha = 1.2f;          // More red for slow drift
    config.scales[3].amplitude = 0.12f;
    config.scales[3].coupling_up = 0.05f;
    config.scales[3].coupling_down = 0.0f;  // No slower scale

    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

pink_noise_multiscale_t* pink_noise_multiscale_create(
    const pink_noise_multiscale_config_t* config
) {
    /**
     * WHAT: Create multi-scale generator with coupled scales
     * WHY:  Initialize complete temporal hierarchy
     * HOW:  Create independent generators, setup coupling
     */
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;

    }
    if (config->num_scales == 0 || config->num_scales > PINK_NOISE_MAX_SCALES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_noise_multiscale_create: invalid num_scales");
        NIMCP_LOGGING_ERROR("Invalid number of scales: %u", config->num_scales);
        return NULL;
    }

    pink_noise_multiscale_t* ms = nimcp_calloc(1, sizeof(pink_noise_multiscale_t));
    if (!ms) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ms is NULL");

        return NULL;

    }

    memcpy(&ms->config, config, sizeof(pink_noise_multiscale_config_t));

    // Create generator for each scale
    for (uint32_t i = 0; i < config->num_scales; i++) {
        pink_noise_config_t gen_config = pink_noise_default_config();
        gen_config.alpha = config->scales[i].alpha;
        gen_config.amplitude = config->scales[i].amplitude;
        gen_config.sample_rate = config->sample_rate;
        gen_config.seed = config->seed + i;  // Different seed per scale

        // Adjust frequency range based on timescale
        float max_freq = 1000.0f / config->scales[i].timescale_ms;
        gen_config.max_frequency = fminf(max_freq, config->sample_rate / 2.0f);
        gen_config.min_frequency = max_freq / 100.0f;

        ms->generators[i] = pink_noise_create(&gen_config);
        if (!ms->generators[i]) {
            NIMCP_LOGGING_ERROR("Failed to create generator for scale %u", i);
            pink_noise_multiscale_destroy(ms);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pink_noise_multiscale_create: ms->generators is NULL");
            return NULL;
        }
    }

    NIMCP_LOGGING_INFO("Created multi-scale pink noise with %u scales", config->num_scales);
    return ms;
}

void pink_noise_multiscale_destroy(pink_noise_multiscale_t* ms) {
    /**
     * WHAT: Free all resources
     * WHY:  Prevent memory leaks
     * HOW:  Destroy each generator, free state
     */
    if (!ms) return;

    for (uint32_t i = 0; i < ms->config.num_scales; i++) {
        if (ms->generators[i]) {
            pink_noise_destroy(ms->generators[i]);
        }
    }

    nimcp_free(ms);
}

//=============================================================================
// Generation Functions
//=============================================================================

int pink_noise_multiscale_step(pink_noise_multiscale_t* ms) {
    /**
     * WHAT: Generate one sample at all scales, apply coupling
     * WHY:  Advance the hierarchical noise system
     * HOW:  Generate raw → apply inter-scale coupling
     */
    if (!ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_multiscale_step: ms is NULL");
        return -1;
    }

    // Step 1: Generate raw noise at each scale
    for (uint32_t i = 0; i < ms->config.num_scales; i++) {
        float sample;
        if (!pink_noise_generate_sample(ms->generators[i], &sample)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pink_noise_multiscale_step: pink_noise_generate_sample is NULL");
            return -1;
        }
        ms->current_values[i] = sample;
        ms->update_counters[i]++;
    }

    // Step 2: Apply inter-scale coupling if enabled
    if (ms->config.enable_coupling) {
        for (uint32_t i = 0; i < ms->config.num_scales; i++) {
            float coupled = ms->current_values[i];

            // Bottom-up influence (from faster scale)
            if (i > 0) {
                coupled += ms->config.scales[i].coupling_up * ms->current_values[i - 1];
            }

            // Top-down influence (from slower scale)
            if (i < ms->config.num_scales - 1) {
                coupled += ms->config.scales[i].coupling_down * ms->current_values[i + 1];
            }

            ms->coupled_values[i] = coupled * ms->config.global_amplitude;
        }
    } else {
        // No coupling - direct copy
        for (uint32_t i = 0; i < ms->config.num_scales; i++) {
            ms->coupled_values[i] = ms->current_values[i] * ms->config.global_amplitude;
        }
    }

    ms->sample_count++;
    return 0;
}

float pink_noise_multiscale_get_scale(
    const pink_noise_multiscale_t* ms,
    uint32_t scale_index
) {
    /**
     * WHAT: Get noise value at specific scale
     * WHY:  Different modules use different timescales
     */
    if (!ms || scale_index >= ms->config.num_scales) return 0.0f;
    return ms->coupled_values[scale_index];
}

float pink_noise_multiscale_get_combined(
    const pink_noise_multiscale_t* ms,
    const float* weights
) {
    /**
     * WHAT: Get weighted sum across all scales
     * WHY:  Single value incorporating all timescales
     * HOW:  Sum with weights (equal if NULL)
     */
    if (!ms) return 0.0f;

    float sum = 0.0f;
    float weight_sum = 0.0f;

    for (uint32_t i = 0; i < ms->config.num_scales; i++) {
        float w = weights ? weights[i] : 1.0f;
        sum += w * ms->coupled_values[i];
        weight_sum += w;
    }

    return (weight_sum > 0.0f) ? (sum / weight_sum) : 0.0f;
}

int pink_noise_multiscale_generate_batch(
    pink_noise_multiscale_t* ms,
    float** outputs,
    uint32_t num_samples
) {
    /**
     * WHAT: Generate batch of samples at all scales
     * WHY:  Efficient batch generation
     * HOW:  Loop calling step, store results
     */
    if (!ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_multiscale_generate_batch: ms is NULL");
        return -1;
    }
    if (!outputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_multiscale_generate_batch: outputs is NULL");
        return -1;
    }
    if (num_samples == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_noise_multiscale_generate_batch: num_samples is 0");
        return -1;
    }

    for (uint32_t s = 0; s < num_samples; s++) {
        int result = pink_noise_multiscale_step(ms);
        if (result != 0) return result;

        for (uint32_t i = 0; i < ms->config.num_scales; i++) {
            if (outputs[i]) {
                outputs[i][s] = ms->coupled_values[i];
            }
        }
    }

    return 0;
}

//=============================================================================
// Coupling Control
//=============================================================================

int pink_noise_multiscale_set_coupling(
    pink_noise_multiscale_t* ms,
    uint32_t scale_index,
    float coupling_up,
    float coupling_down
) {
    /**
     * WHAT: Adjust inter-scale coupling
     * WHY:  Dynamic modulation of hierarchical influence
     */
    if (!ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_multiscale_set_coupling: ms is NULL");
        return -1;
    }
    if (scale_index >= ms->config.num_scales) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_noise_multiscale_set_coupling: invalid scale_index");
        return -1;
    }
    if (coupling_up < 0.0f || coupling_up > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_noise_multiscale_set_coupling: coupling_up out of range");
        return -1;
    }
    if (coupling_down < 0.0f || coupling_down > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_noise_multiscale_set_coupling: coupling_down out of range");
        return -1;
    }

    ms->config.scales[scale_index].coupling_up = coupling_up;
    ms->config.scales[scale_index].coupling_down = coupling_down;
    return 0;
}

int pink_noise_multiscale_set_amplitude(
    pink_noise_multiscale_t* ms,
    uint32_t scale_index,
    float amplitude
) {
    /**
     * WHAT: Set amplitude at specific scale
     * WHY:  Dynamic modulation based on task demands
     */
    if (!ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_multiscale_set_amplitude: ms is NULL");
        return -1;
    }
    if (scale_index >= ms->config.num_scales) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_noise_multiscale_set_amplitude: invalid scale_index");
        return -1;
    }
    if (amplitude < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAMETER, "pink_noise_multiscale_set_amplitude: negative amplitude");
        return -1;
    }

    ms->config.scales[scale_index].amplitude = amplitude;
    return 0;
}

//=============================================================================
// Statistics and Reset
//=============================================================================

int pink_noise_multiscale_get_stats(
    const pink_noise_multiscale_t* ms,
    pink_noise_multiscale_stats_t* stats
) {
    /**
     * WHAT: Compute statistics for multi-scale noise
     * WHY:  Validate hierarchical noise quality
     */
    if (!ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_multiscale_get_stats: ms is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_multiscale_get_stats: stats is NULL");
        return -1;
    }

    memset(stats, 0, sizeof(pink_noise_multiscale_stats_t));

    float total_var = 0.0f;
    for (uint32_t i = 0; i < ms->config.num_scales; i++) {
        stats->scale_amplitudes[i] = ms->config.scales[i].amplitude;
        stats->scale_alphas[i] = ms->config.scales[i].alpha;
        total_var += ms->config.scales[i].amplitude * ms->config.scales[i].amplitude;

        // Cross-correlation with next scale (simplified)
        if (i < ms->config.num_scales - 1) {
            stats->cross_correlations[i] = ms->config.scales[i].coupling_down;
        }
    }

    stats->total_variance = total_var;
    stats->total_samples = ms->sample_count;

    return 0;
}

int pink_noise_multiscale_reset(
    pink_noise_multiscale_t* ms,
    uint32_t new_seed
) {
    /**
     * WHAT: Reset all scales to initial state
     * WHY:  Start fresh for new trial
     */
    if (!ms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pink_noise_multiscale_reset: ms is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < ms->config.num_scales; i++) {
        uint32_t seed = (new_seed == 0) ? ms->config.seed + i : new_seed + i;
        if (!pink_noise_reset(ms->generators[i], seed)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pink_noise_multiscale_reset: pink_noise_reset is NULL");
            return -1;
        }
        ms->current_values[i] = 0.0f;
        ms->coupled_values[i] = 0.0f;
        ms->update_counters[i] = 0;
    }

    ms->sample_count = 0;
    return 0;
}
