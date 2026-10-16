/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * homeostatic_pink_noise_create has zero callers in production code.
 * Wrapper around pink_noise_create that is unused. Either wire a
 * consumer or delete before the next major version. Do not extend.
 */

#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_homeostatic_pink_noise_bridge.c - Pink Noise for Homeostatic Plasticity
//=============================================================================

#include "plasticity/homeostatic/nimcp_homeostatic_pink_noise_bridge.h"
#include "api/nimcp_api_exception.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "security/nimcp_bbb_helpers.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(homeostatic_pink_noise_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(homeo_pink_noise_bridge)

//=============================================================================
// Lifecycle Functions
//=============================================================================

homeo_pink_noise_bridge_t* homeo_pink_noise_create(
    const homeo_pink_noise_config_t* config
) {
    homeo_pink_noise_bridge_t* bridge = nimcp_calloc(1, sizeof(homeo_pink_noise_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "homeo_pink_noise_create: bridge allocation failed");
        return NULL;
    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(homeo_pink_noise_config_t));
    } else {
        bridge->config = homeo_pink_noise_default_config();
    }

    pink_noise_config_t noise_config;
    pink_noise_default_config(&noise_config);
    noise_config.alpha = bridge->config.noise_alpha;
    noise_config.amplitude = bridge->config.noise_amplitude;
    noise_config.seed = bridge->config.seed;

    bridge->noise_gen = pink_noise_create(&noise_config);
    if (!bridge->noise_gen) {
        nimcp_free(bridge);
        LOG_ERROR("Pink noise generator creation failed for homeostatic");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "homeo_pink_noise_create: noise_gen allocation failed");
        return NULL;
    }
    bridge->noise_connected = true;

    bridge->is_enabled = bridge->config.enabled;
    bridge->noisy_target_rate = HOMEO_PINK_NOISE_DEFAULT_TARGET;
    bridge->noisy_scaling_tau = 3600000.0f;  // 1 hour default
    bridge->noisy_scaling_exp = 1.0f;
    bridge->noisy_rate_avg_tau = 1000.0f;    // 1 second

    NIMCP_LOGGING_INFO("Created homeostatic pink noise bridge");
    return bridge;
}

void homeo_pink_noise_destroy(homeo_pink_noise_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->noise_gen) {
        pink_noise_destroy(bridge->noise_gen);
    }
    nimcp_free(bridge);
}

//=============================================================================
// Connection Functions
//=============================================================================

int homeo_pink_noise_connect_scaling(
    homeo_pink_noise_bridge_t* bridge,
    synaptic_scaling_params_t* params
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeo_pink_noise_connect_scaling: bridge is NULL");
        return -1;
    }
    bridge->scaling_params = params;
    bridge->params_connected = (params != NULL);

    if (params) {
        bridge->noisy_target_rate = params->target_rate;
        bridge->noisy_scaling_tau = params->scaling_time_constant;
        bridge->noisy_scaling_exp = params->scaling_exponent;
        bridge->noisy_rate_avg_tau = params->rate_averaging_tau;
    }
    return 0;
}

int homeo_pink_noise_disconnect(homeo_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeo_pink_noise_disconnect: bridge is NULL");
        return -1;
    }
    bridge->scaling_params = NULL;
    bridge->params_connected = false;
    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int homeo_pink_noise_update(homeo_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeo_pink_noise_update: bridge is NULL");
        return -1;
    }
    if (!bridge->is_enabled) return 0;
    if (!bridge->noise_gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "homeo_pink_noise_update: noise_gen is NULL");
        return -1;
    }

    // Generate noise samples
    if (bridge->config.noise_targets & HOMEO_NOISE_TARGET_RATE) {
        bridge->target_rate_noise = pink_noise_sample(bridge->noise_gen);
        bridge->target_rate_noise *= bridge->config.target_rate_noise_scale;
    }

    if (bridge->config.noise_targets & HOMEO_NOISE_SCALING_TAU) {
        bridge->scaling_tau_noise = pink_noise_sample(bridge->noise_gen);
        bridge->scaling_tau_noise *= bridge->config.scaling_tau_noise_scale;
    }

    if (bridge->config.noise_targets & HOMEO_NOISE_SCALING_EXP) {
        bridge->scaling_exp_noise = pink_noise_sample(bridge->noise_gen);
        bridge->scaling_exp_noise *= bridge->config.scaling_exp_noise_scale;
    }

    if (bridge->config.noise_targets & HOMEO_NOISE_RATE_AVG_TAU) {
        bridge->rate_avg_tau_noise = pink_noise_sample(bridge->noise_gen);
        bridge->rate_avg_tau_noise *= bridge->config.rate_avg_tau_noise_scale;
    }

    // Get base values
    float base_target_rate = HOMEO_PINK_NOISE_DEFAULT_TARGET;
    float base_scaling_tau = 3600000.0f;
    float base_scaling_exp = 1.0f;
    float base_rate_avg_tau = 1000.0f;

    if (bridge->scaling_params) {
        base_target_rate = bridge->scaling_params->target_rate;
        base_scaling_tau = bridge->scaling_params->scaling_time_constant;
        base_scaling_exp = bridge->scaling_params->scaling_exponent;
        base_rate_avg_tau = bridge->scaling_params->rate_averaging_tau;
    }

    float amp = bridge->config.noise_amplitude;

    // Apply multiplicative noise
    bridge->noisy_target_rate = base_target_rate *
        (1.0f + amp * bridge->target_rate_noise);
    bridge->noisy_scaling_tau = base_scaling_tau *
        (1.0f + amp * bridge->scaling_tau_noise);
    bridge->noisy_scaling_exp = base_scaling_exp *
        (1.0f + amp * bridge->scaling_exp_noise);
    bridge->noisy_rate_avg_tau = base_rate_avg_tau *
        (1.0f + amp * bridge->rate_avg_tau_noise);

    // Clamp to bounds
    bridge->noisy_target_rate = nimcp_clampf(bridge->noisy_target_rate,
        bridge->config.target_rate_min, bridge->config.target_rate_max);
    bridge->noisy_scaling_tau = nimcp_clampf(bridge->noisy_scaling_tau,
        bridge->config.scaling_tau_min, bridge->config.scaling_tau_max);
    bridge->noisy_scaling_exp = nimcp_clampf(bridge->noisy_scaling_exp,
        bridge->config.scaling_exp_min, bridge->config.scaling_exp_max);

    // Update statistics
    bridge->samples_generated++;
    bridge->parameters_modulated++;

    float noise_amp = fabsf(bridge->target_rate_noise) * amp;
    bridge->avg_noise_amplitude = bridge->avg_noise_amplitude * 0.99f + noise_amp * 0.01f;
    if (!isfinite(bridge->avg_noise_amplitude)) bridge->avg_noise_amplitude = noise_amp;
    if (noise_amp > bridge->max_noise_amplitude) {
        bridge->max_noise_amplitude = noise_amp;
    }

    float rate_shift = bridge->noisy_target_rate - base_target_rate;
    bridge->avg_target_rate_shift = bridge->avg_target_rate_shift * 0.99f + rate_shift * 0.01f;
    if (!isfinite(bridge->avg_target_rate_shift)) bridge->avg_target_rate_shift = rate_shift;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int homeo_pink_noise_apply_to_state(
    homeo_pink_noise_bridge_t* bridge,
    synaptic_scaling_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeo_pink_noise_apply_to_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeo_pink_noise_apply_to_state: state is NULL");
        return -1;
    }
    if (!bridge->is_enabled) return 0;
    // State is updated via scaling_params, not directly
    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

float homeo_pink_noise_get_noisy_target_rate(const homeo_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeo_pink_noise_get_noisy_target_rate: bridge is NULL");
        return HOMEO_PINK_NOISE_DEFAULT_TARGET;
    }
    return bridge->noisy_target_rate;
}

float homeo_pink_noise_get_noisy_scaling_exp(const homeo_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeo_pink_noise_get_noisy_scaling_exp: bridge is NULL");
        return 1.0f;
    }
    return bridge->noisy_scaling_exp;
}

int homeo_pink_noise_get_stats(
    const homeo_pink_noise_bridge_t* bridge,
    homeo_pink_noise_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeo_pink_noise_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeo_pink_noise_get_stats: stats is NULL");
        return -1;
    }

    memset(stats, 0, sizeof(homeo_pink_noise_stats_t));
    stats->samples_generated = bridge->samples_generated;
    stats->parameters_modulated = bridge->parameters_modulated;
    stats->avg_noise_amplitude = bridge->avg_noise_amplitude;
    stats->max_noise_amplitude = bridge->max_noise_amplitude;
    stats->current_noisy_target_rate = bridge->noisy_target_rate;
    stats->current_noisy_scaling_exp = bridge->noisy_scaling_exp;
    stats->avg_target_rate_shift = bridge->avg_target_rate_shift;

    return 0;
}

//=============================================================================
// Control Functions
//=============================================================================

int homeo_pink_noise_set_enabled(homeo_pink_noise_bridge_t* bridge, bool enabled) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeo_pink_noise_set_enabled: bridge is NULL");
        return -1;
    }
    bridge->is_enabled = enabled;
    bridge->config.enabled = enabled;
    return 0;
}

int homeo_pink_noise_reset(homeo_pink_noise_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeo_pink_noise_reset: bridge is NULL");
        return -1;
    }

    bridge->target_rate_noise = 0.0f;
    bridge->scaling_tau_noise = 0.0f;
    bridge->scaling_exp_noise = 0.0f;
    bridge->rate_avg_tau_noise = 0.0f;

    bridge->noisy_target_rate = HOMEO_PINK_NOISE_DEFAULT_TARGET;
    bridge->noisy_scaling_tau = 3600000.0f;
    bridge->noisy_scaling_exp = 1.0f;
    bridge->noisy_rate_avg_tau = 1000.0f;

    bridge->samples_generated = 0;
    bridge->parameters_modulated = 0;
    bridge->avg_noise_amplitude = 0.0f;
    bridge->max_noise_amplitude = 0.0f;
    bridge->avg_target_rate_shift = 0.0f;

    if (bridge->noise_gen) {
        pink_noise_reset(bridge->noise_gen);
    }

    return 0;
}

int homeo_pink_noise_set_amplitude(homeo_pink_noise_bridge_t* bridge, float amplitude) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "homeo_pink_noise_set_amplitude: bridge is NULL");
        return -1;
    }
    if (amplitude < 0.0f || amplitude > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "homeo_pink_noise_set_amplitude: amplitude out of range [0.0, 1.0]");
        return -1;
    }
    bridge->config.noise_amplitude = amplitude;
    return 0;
}
