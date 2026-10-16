/*
 * DEPRECATED — STATUE (audit 2026-04-30)
 *
 * metabolic_pink_noise_create has zero callers in production code.
 * Wrapper around pink_noise_create that is unused. Either wire a
 * consumer or delete before the next major version. Do not extend.
 */

#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_metabolic_pink_noise_bridge.c - Metabolic Pink Noise Implementation
//=============================================================================

#include "plasticity/metabolic/nimcp_metabolic_pink_noise_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "plasticity/metabolic/nimcp_metabolic_plasticity.h"
#include "plasticity/noise/nimcp_pink_noise.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "security/nimcp_bbb_helpers.h"

#define LOG_MODULE "metabolic_pink_noise"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(metabolic_pink_noise_bridge)

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(metabolic_pink_noise_bridge)

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Update noise amplitude based on energy state
 *
 * WHAT: Adapt noise amplitude to current metabolic energy level
 * WHY:  Model increased metabolic variability under energy stress
 * HOW:  Scale amplitude based on energy state classification
 */
static void update_adaptive_amplitude(metabolic_pink_noise_bridge_t* bridge) {
    // Guard: NULL bridge
    if (!bridge) {
        return;
    }

    // Guard: No metabolic system connected
    if (!bridge->metabolic) {
        bridge->state.effective_amplitude = bridge->config.healthy_amplitude_scale;
        return;
    }

    // Guard: Adaptive amplitude disabled
    if (!bridge->config.enable_adaptive_amplitude) {
        bridge->state.effective_amplitude = bridge->config.healthy_amplitude_scale;
        return;
    }

    // Get current energy state
    energy_state_t energy_state = metabolic_plasticity_get_energy_state(bridge->metabolic);

    // Scale amplitude based on energy state
    switch (energy_state) {
        case ENERGY_STATE_HEALTHY:
            bridge->state.effective_amplitude = bridge->config.healthy_amplitude_scale;
            break;

        case ENERGY_STATE_DEPLETED:
            bridge->state.effective_amplitude = bridge->config.depleted_amplitude_scale;
            break;

        case ENERGY_STATE_CRITICAL:
        case ENERGY_STATE_EMERGENCY:
            bridge->state.effective_amplitude = bridge->config.critical_amplitude_scale;
            break;

        default:
            bridge->state.effective_amplitude = bridge->config.healthy_amplitude_scale;
            break;
    }
}

/**
 * @brief Update spectral exponent based on energy state
 *
 * WHAT: Adapt alpha (spectral exponent) to metabolic state
 * WHY:  Model loss of homeostatic 1/f dynamics under stress
 * HOW:  Shift alpha based on energy state
 */
static void update_adaptive_alpha(metabolic_pink_noise_bridge_t* bridge) {
    // Guard: NULL bridge
    if (!bridge) {
        return;
    }

    // Guard: No metabolic system connected
    if (!bridge->metabolic) {
        bridge->state.effective_alpha = bridge->config.normal_alpha;
        return;
    }

    // Guard: Adaptive alpha disabled
    if (!bridge->config.enable_adaptive_alpha) {
        bridge->state.effective_alpha = bridge->config.normal_alpha;
        return;
    }

    // Get current energy state and ATP level
    energy_state_t energy_state = metabolic_plasticity_get_energy_state(bridge->metabolic);
    float atp_level = metabolic_plasticity_get_atp_level(bridge->metabolic);

    // Determine alpha based on energy state
    float alpha = bridge->config.normal_alpha;

    if (energy_state == ENERGY_STATE_CRITICAL || energy_state == ENERGY_STATE_EMERGENCY) {
        // Critical: shift toward white noise (loss of 1/f structure)
        alpha += bridge->config.critical_alpha_shift;
    } else if (energy_state == ENERGY_STATE_HEALTHY && atp_level > 80.0f) {
        // Healthy and recovering: shift toward red noise (more stable)
        alpha += bridge->config.recovery_alpha_shift;
    }

    bridge->state.effective_alpha = alpha;
}

/**
 * @brief Generate noise sample for target
 *
 * WHAT: Generate and scale noise sample for specific modulation target
 * WHY:  Each target uses same generator but different scaling
 * HOW:  Get sample, scale by amplitude and target strength
 */
static float generate_noise_sample(
    metabolic_pink_noise_bridge_t* bridge,
    metabolic_noise_target_t target
) {
    // Guard: NULL bridge
    if (!bridge) {
        return 0.0f;
    }

    // Guard: No noise generator
    if (!bridge->noise_generator) {
        return 0.0f;
    }

    // Generate base pink noise sample
    float sample = 0.0f;
    if (!pink_noise_generate_sample(bridge->noise_generator, &sample)) {
        NIMCP_LOGGING_WARN("Failed to generate pink noise sample");
        return 0.0f;
    }

    // Scale by effective amplitude
    sample *= bridge->state.effective_amplitude;

    // Apply target-specific strength
    switch (target) {
        case METABOLIC_NOISE_RECOVERY_RATE:
            sample *= bridge->config.recovery_rate_strength;
            break;

        case METABOLIC_NOISE_LTP_THRESHOLD:
        case METABOLIC_NOISE_LTD_THRESHOLD:
            sample *= bridge->config.threshold_jitter_strength;
            break;

        case METABOLIC_NOISE_LTP_COST:
        case METABOLIC_NOISE_LTD_COST:
            sample *= bridge->config.cost_variability_strength;
            break;

        case METABOLIC_NOISE_GLUCOSE_DELIVERY:
            sample *= bridge->config.delivery_noise_strength;
            break;

        default:
            break;
    }

    return sample;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int metabolic_pink_noise_default_config(metabolic_pink_noise_config_t* config) {
    // Guard: NULL config
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_pink_noise_default_config: config is NULL");
        return -1;
    }

    // Set default noise strengths
    config->recovery_rate_strength = METABOLIC_PINK_RECOVERY_NOISE_STRENGTH;
    config->threshold_jitter_strength = METABOLIC_PINK_THRESHOLD_JITTER;
    config->cost_variability_strength = METABOLIC_PINK_COST_VARIABILITY;
    config->delivery_noise_strength = METABOLIC_PINK_DELIVERY_NOISE;

    // Set energy-dependent amplitude scaling
    config->depleted_amplitude_scale = METABOLIC_PINK_DEPLETED_AMP_SCALE;
    config->critical_amplitude_scale = METABOLIC_PINK_CRITICAL_AMP_SCALE;
    config->healthy_amplitude_scale = METABOLIC_PINK_HEALTHY_AMP_SCALE;

    // Set alpha modulation
    config->normal_alpha = METABOLIC_PINK_NORMAL_ALPHA;
    config->critical_alpha_shift = METABOLIC_PINK_CRITICAL_ALPHA_SHIFT;
    config->recovery_alpha_shift = METABOLIC_PINK_RECOVERY_ALPHA_SHIFT;

    // Enable all features by default
    config->enable_recovery_noise = true;
    config->enable_threshold_jitter = true;
    config->enable_cost_variability = true;
    config->enable_delivery_noise = true;
    config->enable_adaptive_amplitude = true;
    config->enable_adaptive_alpha = true;

    return 0;
}

metabolic_pink_noise_bridge_t* metabolic_pink_noise_create(
    const metabolic_pink_noise_config_t* config,
    metabolic_plasticity_t* metabolic
) {
    // Allocate bridge structure
    metabolic_pink_noise_bridge_t* bridge = nimcp_malloc(sizeof(metabolic_pink_noise_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    // Zero initialize
    memset(bridge, 0, sizeof(metabolic_pink_noise_bridge_t));

    // Set configuration (use defaults if NULL)
    if (config) {
        bridge->config = *config;
    } else {
        metabolic_pink_noise_default_config(&bridge->config);
    }

    // Store metabolic system pointer (optional)
    bridge->metabolic = metabolic;

    // Create pink noise generator
    pink_noise_config_t noise_config = pink_noise_default_config();
    noise_config.alpha = bridge->config.normal_alpha;
    noise_config.amplitude = bridge->config.healthy_amplitude_scale;

    bridge->noise_generator = pink_noise_create(&noise_config);
    if (!bridge->noise_generator) {
        NIMCP_LOGGING_ERROR("Failed to create pink noise generator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metabolic_pink_noise_create: failed to create pink noise generator");
        nimcp_free(bridge);
        return NULL;
    }

    // Initialize state
    bridge->state.effective_amplitude = bridge->config.healthy_amplitude_scale;
    bridge->state.effective_alpha = bridge->config.normal_alpha;

    // Initialize bridge base (allocates mutex, sets module name)
    if (bridge_base_init(&bridge->base, 0, "metabolic_pink_noise") != 0) {
        pink_noise_destroy(bridge->noise_generator);
        nimcp_free(bridge);
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created metabolic pink noise bridge");
    return bridge;
}

void metabolic_pink_noise_destroy(metabolic_pink_noise_bridge_t* bridge) {
    // Guard: NULL bridge (safe to call with NULL)
    if (!bridge) {
        return;
    }

    // Destroy pink noise generator
    if (bridge->noise_generator) {
        pink_noise_destroy(bridge->noise_generator);
    }

    // Cleanup bridge base (disconnects bio-async, destroys+frees mutex)
    bridge_base_cleanup(&bridge->base);

    // Free bridge structure
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed metabolic pink noise bridge");
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

int metabolic_pink_noise_connect_metabolic(
    metabolic_pink_noise_bridge_t* bridge,
    metabolic_plasticity_t* metabolic
) {
    // Guard: NULL bridge
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_pink_noise_connect_metabolic: bridge is NULL");
        return -1;
    }

    // Guard: NULL metabolic
    if (!metabolic) {
        NIMCP_LOGGING_ERROR("NULL metabolic pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_pink_noise_connect_metabolic: metabolic is NULL");
        return -1;
    }

    // Thread-safe update
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metabolic = metabolic;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Connected to metabolic plasticity system");
    return 0;
}

int metabolic_pink_noise_disconnect(metabolic_pink_noise_bridge_t* bridge) {
    // Guard: NULL bridge
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_pink_noise_disconnect: bridge is NULL");
        return -1;
    }

    // Thread-safe update
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metabolic = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Disconnected from metabolic plasticity system");
    return 0;
}

bool metabolic_pink_noise_is_connected(const metabolic_pink_noise_bridge_t* bridge) {
    // Guard: NULL bridge
    if (!bridge) {
        return false;
    }

    return (bridge->metabolic != NULL);
}

/* ============================================================================
 * Update API Implementation
 * ============================================================================ */

int metabolic_pink_noise_update(
    metabolic_pink_noise_bridge_t* bridge,
    uint64_t delta_ms
) {
    // Guard: NULL bridge
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_pink_noise_update: bridge is NULL");
        return -1;
    }

    // Thread-safe update
    nimcp_mutex_lock(bridge->base.mutex);

    // Update adaptive amplitude based on energy state
    update_adaptive_amplitude(bridge);

    // Update adaptive alpha based on energy state
    update_adaptive_alpha(bridge);

    // Generate noise samples for each target
    if (bridge->config.enable_recovery_noise) {
        bridge->state.recovery_rate_noise = generate_noise_sample(bridge, METABOLIC_NOISE_RECOVERY_RATE);
    }

    if (bridge->config.enable_threshold_jitter) {
        bridge->state.ltp_threshold_noise = generate_noise_sample(bridge, METABOLIC_NOISE_LTP_THRESHOLD);
        bridge->state.ltd_threshold_noise = generate_noise_sample(bridge, METABOLIC_NOISE_LTD_THRESHOLD);
    }

    if (bridge->config.enable_cost_variability) {
        bridge->state.ltp_cost_noise = generate_noise_sample(bridge, METABOLIC_NOISE_LTP_COST);
        bridge->state.ltd_cost_noise = generate_noise_sample(bridge, METABOLIC_NOISE_LTD_COST);
    }

    if (bridge->config.enable_delivery_noise) {
        bridge->state.delivery_noise = generate_noise_sample(bridge, METABOLIC_NOISE_GLUCOSE_DELIVERY);
    }

    // Update statistics
    bridge->update_count++;
    bridge->avg_recovery_noise += (bridge->state.recovery_rate_noise - bridge->avg_recovery_noise) / bridge->update_count;
    bridge->avg_threshold_jitter += (fabsf(bridge->state.ltp_threshold_noise) - bridge->avg_threshold_jitter) / bridge->update_count;
    bridge->avg_cost_variation += (fabsf(bridge->state.ltp_cost_noise) - bridge->avg_cost_variation) / bridge->update_count;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

float metabolic_pink_noise_apply_recovery(
    const metabolic_pink_noise_bridge_t* bridge,
    float base_rate
) {
    // Guard: NULL bridge
    if (!bridge) {
        return base_rate;
    }

    // Guard: Feature disabled
    if (!bridge->config.enable_recovery_noise) {
        return base_rate;
    }

    // Apply multiplicative modulation: rate *= (1 + noise)
    return base_rate * (1.0f + bridge->state.recovery_rate_noise);
}

float metabolic_pink_noise_apply_threshold(
    const metabolic_pink_noise_bridge_t* bridge,
    float base_threshold,
    metabolic_noise_target_t target
) {
    // Guard: NULL bridge
    if (!bridge) {
        return base_threshold;
    }

    // Guard: Feature disabled
    if (!bridge->config.enable_threshold_jitter) {
        return base_threshold;
    }

    // Get appropriate noise value
    float noise = 0.0f;
    if (target == METABOLIC_NOISE_LTP_THRESHOLD) {
        noise = bridge->state.ltp_threshold_noise;
    } else if (target == METABOLIC_NOISE_LTD_THRESHOLD) {
        noise = bridge->state.ltd_threshold_noise;
    }

    // Apply additive jitter: threshold += noise * base_threshold
    return base_threshold + (noise * base_threshold);
}

float metabolic_pink_noise_apply_cost(
    const metabolic_pink_noise_bridge_t* bridge,
    float base_cost,
    metabolic_noise_target_t target
) {
    // Guard: NULL bridge
    if (!bridge) {
        return base_cost;
    }

    // Guard: Feature disabled
    if (!bridge->config.enable_cost_variability) {
        return base_cost;
    }

    // Get appropriate noise value
    float noise = 0.0f;
    if (target == METABOLIC_NOISE_LTP_COST) {
        noise = bridge->state.ltp_cost_noise;
    } else if (target == METABOLIC_NOISE_LTD_COST) {
        noise = bridge->state.ltd_cost_noise;
    }

    // Apply multiplicative modulation: cost *= (1 + noise)
    return base_cost * (1.0f + noise);
}

float metabolic_pink_noise_apply_delivery(
    const metabolic_pink_noise_bridge_t* bridge,
    float base_delivery
) {
    // Guard: NULL bridge
    if (!bridge) {
        return base_delivery;
    }

    // Guard: Feature disabled
    if (!bridge->config.enable_delivery_noise) {
        return base_delivery;
    }

    // Apply additive modulation: delivery += noise * base_delivery
    return base_delivery + (bridge->state.delivery_noise * base_delivery);
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int metabolic_pink_noise_get_state(
    const metabolic_pink_noise_bridge_t* bridge,
    metabolic_pink_noise_state_t* state
) {
    // Guard: NULL bridge
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_pink_noise_get_state: bridge is NULL");
        return -1;
    }

    // Guard: NULL state
    if (!state) {
        NIMCP_LOGGING_ERROR("NULL state pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_pink_noise_get_state: state is NULL");
        return -1;
    }

    // Thread-safe copy
    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float metabolic_pink_noise_get_amplitude(const metabolic_pink_noise_bridge_t* bridge) {
    // Guard: NULL bridge
    if (!bridge) {
        return 0.0f;
    }

    return bridge->state.effective_amplitude;
}

float metabolic_pink_noise_get_alpha(const metabolic_pink_noise_bridge_t* bridge) {
    // Guard: NULL bridge
    if (!bridge) {
        return 1.0f;
    }

    return bridge->state.effective_alpha;
}

/* ============================================================================
 * Statistics API Implementation
 * ============================================================================ */

int metabolic_pink_noise_get_stats(
    const metabolic_pink_noise_bridge_t* bridge,
    metabolic_pink_noise_stats_t* stats
) {
    // Guard: NULL bridge
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_pink_noise_get_stats: bridge is NULL");
        return -1;
    }

    // Guard: NULL stats
    if (!stats) {
        NIMCP_LOGGING_ERROR("NULL stats pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_pink_noise_get_stats: stats is NULL");
        return -1;
    }

    // Thread-safe copy
    nimcp_mutex_lock(bridge->base.mutex);

    stats->total_updates = bridge->update_count;
    stats->avg_recovery_noise = bridge->avg_recovery_noise;
    stats->avg_threshold_jitter = bridge->avg_threshold_jitter;
    stats->avg_cost_variation = bridge->avg_cost_variation;
    stats->current_amplitude = bridge->state.effective_amplitude;
    stats->current_alpha = bridge->state.effective_alpha;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int metabolic_pink_noise_reset_stats(metabolic_pink_noise_bridge_t* bridge) {
    // Guard: NULL bridge
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_pink_noise_reset_stats: bridge is NULL");
        return -1;
    }

    // Thread-safe reset
    nimcp_mutex_lock(bridge->base.mutex);

    bridge->update_count = 0;
    bridge->avg_recovery_noise = 0.0f;
    bridge->avg_threshold_jitter = 0.0f;
    bridge->avg_cost_variation = 0.0f;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Reset statistics");
    return 0;
}

/* ============================================================================
 * Control API Implementation
 * ============================================================================ */

int metabolic_pink_noise_enable_target(
    metabolic_pink_noise_bridge_t* bridge,
    metabolic_noise_target_t target
) {
    // Guard: NULL bridge
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_pink_noise_enable_target: bridge is NULL");
        return -1;
    }

    // Thread-safe update
    nimcp_mutex_lock(bridge->base.mutex);

    switch (target) {
        case METABOLIC_NOISE_RECOVERY_RATE:
            bridge->config.enable_recovery_noise = true;
            break;

        case METABOLIC_NOISE_LTP_THRESHOLD:
        case METABOLIC_NOISE_LTD_THRESHOLD:
            bridge->config.enable_threshold_jitter = true;
            break;

        case METABOLIC_NOISE_LTP_COST:
        case METABOLIC_NOISE_LTD_COST:
            bridge->config.enable_cost_variability = true;
            break;

        case METABOLIC_NOISE_GLUCOSE_DELIVERY:
            bridge->config.enable_delivery_noise = true;
            break;

        default:
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_LOGGING_ERROR("Invalid noise target");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metabolic_pink_noise_enable_target: invalid noise target");
            return -1;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int metabolic_pink_noise_disable_target(
    metabolic_pink_noise_bridge_t* bridge,
    metabolic_noise_target_t target
) {
    // Guard: NULL bridge
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_pink_noise_disable_target: bridge is NULL");
        return -1;
    }

    // Thread-safe update
    nimcp_mutex_lock(bridge->base.mutex);

    switch (target) {
        case METABOLIC_NOISE_RECOVERY_RATE:
            bridge->config.enable_recovery_noise = false;
            break;

        case METABOLIC_NOISE_LTP_THRESHOLD:
        case METABOLIC_NOISE_LTD_THRESHOLD:
            bridge->config.enable_threshold_jitter = false;
            break;

        case METABOLIC_NOISE_LTP_COST:
        case METABOLIC_NOISE_LTD_COST:
            bridge->config.enable_cost_variability = false;
            break;

        case METABOLIC_NOISE_GLUCOSE_DELIVERY:
            bridge->config.enable_delivery_noise = false;
            break;

        default:
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_LOGGING_ERROR("Invalid noise target");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metabolic_pink_noise_disable_target: invalid noise target");
            return -1;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int metabolic_pink_noise_set_strength(
    metabolic_pink_noise_bridge_t* bridge,
    metabolic_noise_target_t target,
    float strength
) {
    // Guard: NULL bridge
    if (!bridge) {
        NIMCP_LOGGING_ERROR("NULL bridge pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_pink_noise_set_strength: bridge is NULL");
        return -1;
    }

    // Guard: Invalid strength range
    if (strength < 0.0f || strength > 1.0f) {
        NIMCP_LOGGING_ERROR("Strength must be in range [0, 1]");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metabolic_pink_noise_set_strength: strength out of range [0, 1]");
        return -1;
    }

    // Thread-safe update
    nimcp_mutex_lock(bridge->base.mutex);

    switch (target) {
        case METABOLIC_NOISE_RECOVERY_RATE:
            bridge->config.recovery_rate_strength = strength;
            break;

        case METABOLIC_NOISE_LTP_THRESHOLD:
        case METABOLIC_NOISE_LTD_THRESHOLD:
            bridge->config.threshold_jitter_strength = strength;
            break;

        case METABOLIC_NOISE_LTP_COST:
        case METABOLIC_NOISE_LTD_COST:
            bridge->config.cost_variability_strength = strength;
            break;

        case METABOLIC_NOISE_GLUCOSE_DELIVERY:
            bridge->config.delivery_noise_strength = strength;
            break;

        default:
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_LOGGING_ERROR("Invalid noise target");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metabolic_pink_noise_set_strength: invalid noise target");
            return -1;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Helper Functions Implementation
 * ============================================================================ */

const char* metabolic_pink_noise_target_name(metabolic_noise_target_t target) {
    switch (target) {
        case METABOLIC_NOISE_RECOVERY_RATE:
            return "RECOVERY_RATE";
        case METABOLIC_NOISE_LTP_THRESHOLD:
            return "LTP_THRESHOLD";
        case METABOLIC_NOISE_LTD_THRESHOLD:
            return "LTD_THRESHOLD";
        case METABOLIC_NOISE_LTP_COST:
            return "LTP_COST";
        case METABOLIC_NOISE_LTD_COST:
            return "LTD_COST";
        case METABOLIC_NOISE_GLUCOSE_DELIVERY:
            return "GLUCOSE_DELIVERY";
        default:
            return "UNKNOWN";
    }
}
