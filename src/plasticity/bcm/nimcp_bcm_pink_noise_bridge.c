/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * bcm_pink_noise_create has zero callers in production code. Wrapper
 * around pink_noise_create that is unused. Either wire a consumer or
 * delete before the next major version. Do not extend.
 */

#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_bcm_pink_noise_bridge.c - Pink Noise Bridge for BCM Plasticity
//=============================================================================
/**
 * WHAT: Implementation of 1/f pink noise modulation for BCM learning
 * WHY:  Provides biologically-realistic stochastic dynamics
 * HOW:  Generate pink noise samples and apply to BCM parameters
 */

#include "plasticity/bcm/nimcp_bcm_pink_noise_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(bcm_pink_noise_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(bcm_pink_noise_bridge)

static float compute_activity_scale(
    const bcm_pink_noise_bridge_t* bridge,
    float activity
) {
    /**
     * WHAT: Scale noise amplitude based on activity level
     * WHY:  Low activity needs more exploration (higher noise)
     * HOW:  Boost when activity < 0.3, suppress when > 0.7
     */
    if (!bridge->config.activity_dependent) {
        return 1.0f;
    }

    if (activity < 0.3f) {
        // Low activity: boost noise
        float boost = 1.0f + bridge->config.low_activity_boost * (0.3f - activity) / 0.3f;
        return boost;
    } else if (activity > 0.7f) {
        // High activity: suppress noise
        float suppress = 1.0f - bridge->config.high_activity_suppression * (activity - 0.7f) / 0.3f;
        return fmaxf(0.1f, suppress);
    }

    return 1.0f;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

bcm_pink_noise_bridge_t* bcm_pink_noise_create(
    const bcm_pink_noise_config_t* config
) {
    bcm_pink_noise_bridge_t* bridge = nimcp_calloc(1, sizeof(bcm_pink_noise_bridge_t));
    NIMCP_API_CHECK_ALLOC(bridge, "BCM pink noise bridge allocation failed");

    // Apply configuration
    if (config) {
        memcpy(&bridge->config, config, sizeof(bcm_pink_noise_config_t));
    } else {
        bridge->config = bcm_pink_noise_default_config();
    }

    // Create pink noise generator
    pink_noise_config_t noise_config;
    pink_noise_default_config(&noise_config);
    noise_config.alpha = bridge->config.noise_alpha;
    noise_config.amplitude = bridge->config.noise_amplitude;
    noise_config.seed = bridge->config.seed;

    bridge->noise_gen = pink_noise_create(&noise_config);
    if (!bridge->noise_gen) {
        nimcp_free(bridge);
        LOG_ERROR("Pink noise generator creation failed for BCM");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Pink noise generator creation failed for BCM");
        return NULL;
    }
    bridge->noise_connected = true;

    // Initialize state
    bridge->is_enabled = bridge->config.enabled;
    bridge->noisy_threshold = 0.5f;  // Default BCM threshold
    bridge->noisy_lr = BCM_PINK_NOISE_DEFAULT_LR;
    bridge->noisy_threshold_tau = 100.0f;
    bridge->noisy_activity_tau = 50.0f;

    NIMCP_LOGGING_INFO("Created BCM pink noise bridge (alpha=%.2f, amp=%.3f)",
                       bridge->config.noise_alpha, bridge->config.noise_amplitude);

    return bridge;
}

void bcm_pink_noise_destroy(bcm_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_pink_noise_destroy: bridge is NULL");
        return;
    }

    if (bridge->noise_gen) {
        pink_noise_destroy(bridge->noise_gen);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_DEBUG("Destroyed BCM pink noise bridge");
}

//=============================================================================
// Connection Functions
//=============================================================================

int bcm_pink_noise_connect_bcm(
    bcm_pink_noise_bridge_t* bridge,
    bcm_params_t* params
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "BCM pink noise bridge is NULL");

    bridge->bcm_params = params;
    bridge->bcm_connected = (params != NULL);

    if (params) {
        // Initialize noisy values from base params
        bridge->noisy_lr = params->learning_rate;
        bridge->noisy_threshold_tau = params->threshold_time_constant;
        bridge->noisy_activity_tau = params->activity_time_constant;
        NIMCP_LOGGING_DEBUG("Connected BCM pink noise to BCM params");
    }

    return 0;
}

int bcm_pink_noise_disconnect_bcm(bcm_pink_noise_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "BCM pink noise bridge is NULL");

    bridge->bcm_params = NULL;
    bridge->bcm_connected = false;
    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int bcm_pink_noise_update(bcm_pink_noise_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "BCM pink noise bridge is NULL");

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "bcm_pink_noise_update");
    BRIDGE_LGSS_GATE(bridge, "bcm_pink_noise_update");
    if (!bridge->is_enabled) return 0;
    NIMCP_API_CHECK_NULL(bridge->noise_gen, -1, "Pink noise generator is NULL");

    // Generate noise samples for each parameter
    if (bridge->config.noise_targets & BCM_NOISE_TARGET_THRESHOLD) {
        bridge->threshold_noise = pink_noise_sample(bridge->noise_gen);
        bridge->threshold_noise *= bridge->config.threshold_noise_scale;
    }

    if (bridge->config.noise_targets & BCM_NOISE_TARGET_LR) {
        bridge->lr_noise = pink_noise_sample(bridge->noise_gen);
        bridge->lr_noise *= bridge->config.lr_noise_scale;
    }

    if (bridge->config.noise_targets & BCM_NOISE_TARGET_TIME_CONSTANTS) {
        bridge->tau_noise = pink_noise_sample(bridge->noise_gen);
        bridge->tau_noise *= bridge->config.tau_noise_scale;
    }

    // Compute activity-dependent scaling
    float activity_scale = compute_activity_scale(bridge, bridge->current_activity);

    // Apply noise to parameters
    float base_threshold = 0.5f;
    float base_lr = BCM_PINK_NOISE_DEFAULT_LR;
    float base_threshold_tau = 100.0f;
    float base_activity_tau = 50.0f;

    if (bridge->bcm_params) {
        base_lr = bridge->bcm_params->learning_rate;
        base_threshold_tau = bridge->bcm_params->threshold_time_constant;
        base_activity_tau = bridge->bcm_params->activity_time_constant;
    }

    // Apply noise based on mode
    float scaled_noise = bridge->config.noise_amplitude * activity_scale;

    if (bridge->config.noise_mode == BCM_NOISE_ADDITIVE) {
        bridge->noisy_threshold = base_threshold +
            scaled_noise * bridge->threshold_noise;
        bridge->noisy_lr = base_lr +
            scaled_noise * bridge->lr_noise;
        bridge->noisy_threshold_tau = base_threshold_tau +
            scaled_noise * bridge->tau_noise * 10.0f;
        bridge->noisy_activity_tau = base_activity_tau +
            scaled_noise * bridge->tau_noise * 5.0f;
    } else {
        // Multiplicative mode (default)
        bridge->noisy_threshold = base_threshold *
            (1.0f + scaled_noise * bridge->threshold_noise);
        bridge->noisy_lr = base_lr *
            (1.0f + scaled_noise * bridge->lr_noise);
        bridge->noisy_threshold_tau = base_threshold_tau *
            (1.0f + scaled_noise * bridge->tau_noise);
        bridge->noisy_activity_tau = base_activity_tau *
            (1.0f + scaled_noise * bridge->tau_noise);
    }

    // Clamp to bounds
    bridge->noisy_threshold = nimcp_clampf(bridge->noisy_threshold,
        bridge->config.threshold_min, bridge->config.threshold_max);
    bridge->noisy_lr = nimcp_clampf(bridge->noisy_lr,
        bridge->config.lr_min, bridge->config.lr_max);
    bridge->noisy_threshold_tau = nimcp_clampf(bridge->noisy_threshold_tau,
        bridge->config.tau_min, bridge->config.tau_max);
    bridge->noisy_activity_tau = nimcp_clampf(bridge->noisy_activity_tau,
        bridge->config.tau_min, bridge->config.tau_max);

    // Update statistics
    bridge->samples_generated++;
    bridge->parameters_modulated++;

    float noise_amp = fabsf(bridge->threshold_noise) * scaled_noise;
    bridge->avg_noise_amplitude = (bridge->avg_noise_amplitude * 0.99f) + (noise_amp * 0.01f);
    if (noise_amp > bridge->max_noise_amplitude) {
        bridge->max_noise_amplitude = noise_amp;
    }

    float threshold_shift = bridge->noisy_threshold - base_threshold;
    bridge->avg_threshold_shift = (bridge->avg_threshold_shift * 0.99f) +
                                   (threshold_shift * 0.01f);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int bcm_pink_noise_update_with_activity(
    bcm_pink_noise_bridge_t* bridge,
    float activity
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "BCM pink noise bridge is NULL");

    // Update activity tracking
    bridge->current_activity = nimcp_clampf(activity, 0.0f, 1.0f);
    bridge->activity_ema = (bridge->activity_ema * NIMCP_EMA_WEIGHT_MEDIUM) +
                           (bridge->current_activity * NIMCP_EMA_WEIGHT_MEDIUM_NEW);

    return bcm_pink_noise_update(bridge);
}

int bcm_pink_noise_apply_to_synapse(
    bcm_pink_noise_bridge_t* bridge,
    bcm_synapse_t* synapse
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "BCM pink noise bridge is NULL");
    NIMCP_API_CHECK_NULL(synapse, -1, "BCM synapse is NULL");
    if (!bridge->is_enabled) return 0;

    // Apply noisy threshold to synapse
    if (bridge->config.noise_targets & BCM_NOISE_TARGET_THRESHOLD) {
        synapse->threshold = bridge->noisy_threshold;
    }

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

float bcm_pink_noise_get_noisy_threshold(const bcm_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_pink_noise_get_noisy_threshold: bridge is NULL");
        return 0.5f;
    }
    return bridge->noisy_threshold;
}

float bcm_pink_noise_get_noisy_lr(const bcm_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_pink_noise_get_noisy_lr: bridge is NULL");
        return BCM_PINK_NOISE_DEFAULT_LR;
    }
    return bridge->noisy_lr;
}

float bcm_pink_noise_get_threshold_noise(const bcm_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_pink_noise_get_threshold_noise: bridge is NULL");
        return 0.0f;
    }
    return bridge->threshold_noise;
}

int bcm_pink_noise_get_stats(
    const bcm_pink_noise_bridge_t* bridge,
    bcm_pink_noise_stats_t* stats
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "BCM pink noise bridge is NULL");
    NIMCP_API_CHECK_NULL(stats, -1, "Stats output pointer is NULL");

    memset(stats, 0, sizeof(bcm_pink_noise_stats_t));
    stats->samples_generated = bridge->samples_generated;
    stats->parameters_modulated = bridge->parameters_modulated;
    stats->avg_noise_amplitude = bridge->avg_noise_amplitude;
    stats->max_noise_amplitude = bridge->max_noise_amplitude;
    stats->current_noisy_threshold = bridge->noisy_threshold;
    stats->current_noisy_lr = bridge->noisy_lr;
    stats->avg_threshold_shift = bridge->avg_threshold_shift;

    return 0;
}

//=============================================================================
// Control Functions
//=============================================================================

int bcm_pink_noise_set_enabled(
    bcm_pink_noise_bridge_t* bridge,
    bool enabled
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "BCM pink noise bridge is NULL");
    bridge->is_enabled = enabled;
    bridge->config.enabled = enabled;
    NIMCP_LOGGING_DEBUG("BCM pink noise bridge %s",
                        enabled ? "enabled" : "disabled");
    return 0;
}

int bcm_pink_noise_reset(bcm_pink_noise_bridge_t* bridge) {
    NIMCP_API_CHECK_NULL(bridge, -1, "BCM pink noise bridge is NULL");

    // Reset noise samples
    bridge->threshold_noise = 0.0f;
    bridge->lr_noise = 0.0f;
    bridge->tau_noise = 0.0f;

    // Reset noisy params to defaults
    bridge->noisy_threshold = 0.5f;
    bridge->noisy_lr = BCM_PINK_NOISE_DEFAULT_LR;
    bridge->noisy_threshold_tau = 100.0f;
    bridge->noisy_activity_tau = 50.0f;

    // Reset activity tracking
    bridge->current_activity = 0.0f;
    bridge->activity_ema = 0.0f;

    // Reset statistics
    bridge->samples_generated = 0;
    bridge->parameters_modulated = 0;
    bridge->avg_noise_amplitude = 0.0f;
    bridge->max_noise_amplitude = 0.0f;
    bridge->avg_threshold_shift = 0.0f;

    // Reset noise generator
    if (bridge->noise_gen) {
        pink_noise_reset(bridge->noise_gen);
    }

    NIMCP_LOGGING_DEBUG("BCM pink noise bridge reset");
    return 0;
}

int bcm_pink_noise_set_amplitude(
    bcm_pink_noise_bridge_t* bridge,
    float amplitude
) {
    NIMCP_API_CHECK_NULL(bridge, -1, "BCM pink noise bridge is NULL");
    NIMCP_API_CHECK(amplitude >= 0.0f && amplitude <= 1.0f, -1, "Amplitude must be between 0.0 and 1.0");

    bridge->config.noise_amplitude = amplitude;
    NIMCP_LOGGING_DEBUG("BCM pink noise amplitude set to %.3f", amplitude);
    return 0;
}
